/*
 * Copyright (c) 2026 WiggelMc
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_position_repeat

#include <drivers/behavior.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <zmk/keys.h>
#include <zmk/matrix.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

//
// Register Listener
//

static int
position_repeat_position_state_changed_listener(const zmk_event_t *eh);

ZMK_LISTENER(behavior_position_repeat,
             position_repeat_position_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_position_repeat, zmk_position_state_changed);

//
// Types
//

#define PR_POS_BUFFER_SIZE                                                     \
  CONFIG_ZMK_BEHAVIOR_POSITION_REPEAT_POSITION_BUFFER_SIZE
#define PR_HOLD_LIST_SIZE CONFIG_ZMK_BEHAVIOR_POSITION_REPEAT_MAX_BINDINGS_HELD

struct pr_position_buffer {
  size_t count;
  size_t last_index;
  uint32_t items[PR_POS_BUFFER_SIZE];
};

struct pr_hold_list_item {
  uint32_t position;
  struct zmk_behavior_binding binding;
};

struct pr_hold_list {
  size_t count;
  struct pr_hold_list_item items[PR_HOLD_LIST_SIZE];
};

struct pr_phandles_filter_item {
  const struct device *dev;
  const char *behavior_dev;
};

struct pr_phandles_filter {
  size_t count;
  struct pr_phandles_filter_item items[];
};

struct pr_bindings_filter_item {
  const struct device *dev;
  struct zmk_behavior_binding binding;
};

struct pr_bindings_filter {
  int count;
  struct pr_bindings_filter_item items[];
};

struct pr_filter {
  struct pr_phandles_filter *phandles;
  struct pr_bindings_filter *bindings;
};

struct pr_filter_settings {
  bool use_whitelist;
  struct pr_filter whitelist;
  struct pr_filter transparent;
};

struct behavior_position_repeat_data {
  struct pr_filter_settings *filter_settings;
};

struct behavior_position_repeat_engine_data {
  struct pr_position_buffer position_buffer;
  struct pr_hold_list hold_list;
};

static struct behavior_position_repeat_engine_data pr_data = {
    .position_buffer = {0},
    .hold_list = {0},
};

//
// Helpers
//

static void pr_pos_buffer_push(struct pr_position_buffer *buffer,
                               const uint32_t position) {

  const size_t next_index = (buffer->last_index + 1) % PR_POS_BUFFER_SIZE;
  const size_t new_count = buffer->count + 1;

  buffer->items[next_index] = position;
  buffer->last_index = next_index;

  if (new_count <= PR_POS_BUFFER_SIZE) {
    buffer->count = new_count;
  }
}

static void pr_pos_buffer_remove_trailing(struct pr_position_buffer *buffer,
                                          const uint32_t position) {

  while (buffer->count != 0) {
    const uint32_t last = buffer->items[buffer->last_index];

    if (last != position) {
      return;
    }

    const size_t new_count = buffer->count - 1;
    const size_t new_index =
        (buffer->last_index + PR_POS_BUFFER_SIZE - 1) % PR_POS_BUFFER_SIZE;

    buffer->count = new_count;
    buffer->last_index = new_index;
  }
}

static bool pr_pos_buffer_get_last(const struct pr_position_buffer *buffer,
                                   uint32_t *out_position) {

  if (buffer->count == 0) {
    return false;
  }

  *out_position = buffer->items[buffer->last_index];
  return true;
}

static bool pr_hold_list_push(struct pr_hold_list *list,
                              const uint32_t position,
                              const struct zmk_behavior_binding binding) {

  if (list->count >= PR_HOLD_LIST_SIZE) {
    return false;
  }

  const struct pr_hold_list_item item = {
      .position = position,
      .binding = binding,
  };

  list->items[list->count] = item;
  list->count++;

  return true;
}

static bool pr_hold_list_pop_from(struct pr_hold_list *list,
                                  const uint32_t position,
                                  struct zmk_behavior_binding *out_binding) {

  for (size_t i = 0; i < list->count; i++) {
    const struct pr_hold_list_item *item = &list->items[i];
    if (item->position == position) {
      *out_binding = item->binding;

      for (size_t i2 = i; i2 < list->count - 1; i2++) {
        list->items[i2] = list->items[i2 + 1];
      }
      list->count--;

      return true;
    }
  }
  return false;
}

static bool pr_match_filter(const struct zmk_behavior_binding *binding,
                            const struct pr_filter *filter) {

  if (binding == NULL || filter == NULL) {
    return false;
  }

  const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);

  const struct pr_phandles_filter *phandles_filter = filter->phandles;
  for (size_t i = 0; i < phandles_filter->count; i++) {
    const struct pr_phandles_filter_item *item = &phandles_filter->items[i];
    if (item->dev == dev) {
      return true;
    }
  }

  const struct pr_bindings_filter *bindings_filter = filter->bindings;
  for (size_t i = 0; i < bindings_filter->count; i++) {
    const struct pr_bindings_filter_item *item = &bindings_filter->items[i];
    if (item->dev == dev && item->binding.param1 == binding->param1 &&
        item->binding.param2 == binding->param2) {
      return true;
    }
  }

  return false;
}

static bool pr_get_binding(const uint32_t position,
                           const struct pr_filter *transparent_filter,
                           struct zmk_behavior_binding *out_binding) {

  for (zmk_keymap_layer_index_t index = ZMK_KEYMAP_LAYERS_LEN; index-- > 0;) {
    const zmk_keymap_layer_id_t id = zmk_keymap_layer_index_to_id(index);
    const bool is_layer_active = zmk_keymap_layer_active(id);

    if (is_layer_active) {
      const struct zmk_behavior_binding *binding =
          zmk_keymap_get_layer_binding_at_idx(id, position);

      if (binding == NULL) {
        return false;
      }

      const bool is_transparent = pr_match_filter(binding, transparent_filter);

      if (!is_transparent) {
        *out_binding = *binding;
        return true;
      }
    }
  }
  return false;
}

static void pre_init_filter(struct pr_filter *filter) {

  struct pr_phandles_filter *phandles_filter = filter->phandles;
  for (size_t i = 0; i < phandles_filter->count; i++) {
    struct pr_phandles_filter_item *item = &phandles_filter->items[i];
    const char *behavior_dev = item->behavior_dev;
    if (behavior_dev != NULL) {
      item->dev = zmk_behavior_get_binding(behavior_dev);
    }
  }

  struct pr_bindings_filter *bindings_filter = filter->bindings;
  for (size_t i = 0; i < bindings_filter->count; i++) {
    struct pr_bindings_filter_item *item = &bindings_filter->items[i];
    const char *behavior_dev = item->binding.behavior_dev;
    if (behavior_dev != NULL) {
      item->dev = zmk_behavior_get_binding(behavior_dev);
    }
  }
}

static bool pre_press_binding(const uint32_t position,
                              const struct zmk_behavior_binding binding,
                              struct zmk_behavior_binding_event event) {

  const bool can_press =
      pr_hold_list_push(&pr_data.hold_list, position, binding);

  if (!can_press) {
    return false;
  }

  zmk_behavior_invoke_binding(&binding, event, true);

  return true;
}

static bool pre_release_binding(const uint32_t position,
                                struct zmk_behavior_binding_event event) {

  struct zmk_behavior_binding binding;

  const bool has_binding =
      pr_hold_list_pop_from(&pr_data.hold_list, position, &binding);

  if (!has_binding) {
    return false;
  }

  zmk_behavior_invoke_binding(&binding, event, false);

  return true;
}

//
// Zmk Handlers
//

static int
position_repeat_position_state_changed_listener(const zmk_event_t *eh) {

  const struct zmk_position_state_changed *ev =
      as_zmk_position_state_changed(eh);
  if (ev == NULL) {
    return ZMK_EV_EVENT_BUBBLE;
  }

  LOG_DBG("PR Position State changed: position: %d, source: %d", ev->position,
          ev->source);

  if (ev->state) {
    pr_pos_buffer_push(&pr_data.position_buffer, ev->position);
  }

  return ZMK_EV_EVENT_BUBBLE;
}

static int position_repeat_init(const struct device *dev) {

  struct behavior_position_repeat_data *data = dev->data;
  struct pr_filter_settings *filter_settings = data->filter_settings;

  pre_init_filter(&filter_settings->whitelist);
  pre_init_filter(&filter_settings->transparent);

  LOG_DBG("PR Initialized");
  return 0;
};

static int
on_position_repeat_binding_pressed(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event) {

  const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
  const struct behavior_position_repeat_data *data = dev->data;
  const struct pr_filter_settings *filter_settings = data->filter_settings;

  LOG_DBG("PR Binding pressed: param1=%d, param2=%d, layer=%d, position=%d",
          binding->param1, binding->param2, event.layer, event.position);

  const uint32_t position = event.position;

  pr_pos_buffer_remove_trailing(&pr_data.position_buffer, position);

  uint32_t repeat_position;
  const bool has_repeat_position =
      pr_pos_buffer_get_last(&pr_data.position_buffer, &repeat_position);

  if (!has_repeat_position) {
    return ZMK_BEHAVIOR_OPAQUE;
  }

  struct zmk_behavior_binding repeat_binding;
  const bool has_repeat_binding = pr_get_binding(
      repeat_position, &filter_settings->transparent, &repeat_binding);

  if (!has_repeat_binding) {
    return ZMK_BEHAVIOR_OPAQUE;
  }

  if (zmk_behavior_get_binding(repeat_binding.behavior_dev) == dev) {
    return ZMK_BEHAVIOR_OPAQUE;
  }

  if (filter_settings->use_whitelist) {
    const bool is_whitelisted =
        pr_match_filter(&repeat_binding, &filter_settings->whitelist);
    if (!is_whitelisted) {
      return ZMK_BEHAVIOR_OPAQUE;
    }
  }

  pre_press_binding(position, repeat_binding, event);

  return ZMK_BEHAVIOR_OPAQUE;
}

static int
on_position_repeat_binding_released(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {

  LOG_DBG("PR Binding released: param1=%d, param2=%d, layer=%d, position=%d",
          binding->param1, binding->param2, event.layer, event.position);

  const uint32_t position = event.position;

  pre_release_binding(position, event);

  return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api position_repeat_driver_api = {
    .binding_pressed = on_position_repeat_binding_pressed,
    .binding_released = on_position_repeat_binding_released,
};

//
// Define Behavior
//

#define PR_EXTRACT_PHANDLE(idx, drv_inst, prop)                                \
  {                                                                            \
      .dev = NULL,                                                             \
      .behavior_dev = DEVICE_DT_NAME(DT_PHANDLE_BY_IDX(drv_inst, prop, idx)),  \
  }

#define PR_EXTRACT_PHANDLE_ARR(n, prop)                                        \
  {LISTIFY(DT_INST_PROP_LEN_OR(n, prop, 0), PR_EXTRACT_PHANDLE, (, ),          \
           DT_DRV_INST(n), prop)}

#define PR_EXTRACT_PHANDLES(n, prop)                                           \
  {                                                                            \
      .count = DT_INST_PROP_LEN_OR(n, prop, 0),                                \
      .items = PR_EXTRACT_PHANDLE_ARR(n, prop),                                \
  }

#define PR_EXTRACT_ZMK_BINDING(idx, drv_inst, prop)                            \
  {                                                                            \
      .behavior_dev = DEVICE_DT_NAME(DT_PHANDLE_BY_IDX(drv_inst, prop, idx)),  \
      .param1 =                                                                \
          COND_CODE_0(DT_PHA_HAS_CELL_AT_IDX(drv_inst, prop, idx, param1),     \
                      (0), (DT_PHA_BY_IDX(drv_inst, prop, idx, param1))),      \
      .param2 =                                                                \
          COND_CODE_0(DT_PHA_HAS_CELL_AT_IDX(drv_inst, prop, idx, param2),     \
                      (0), (DT_PHA_BY_IDX(drv_inst, prop, idx, param2))),      \
  }

#define PR_EXTRACT_BINDING(idx, drv_inst, prop)                                \
  {                                                                            \
      .dev = NULL,                                                             \
      .binding = PR_EXTRACT_ZMK_BINDING(idx, drv_inst, prop),                  \
  }

#define PR_EXTRACT_BINDING_ARR(n, prop)                                        \
  {LISTIFY(DT_INST_PROP_LEN_OR(n, prop, 0), PR_EXTRACT_BINDING, (, ),          \
           DT_DRV_INST(n), prop)}

#define PR_EXTRACT_BINDINGS(n, prop)                                           \
  {                                                                            \
      .count = DT_INST_PROP_LEN_OR(n, prop, 0),                                \
      .items = PR_EXTRACT_BINDING_ARR(n, prop),                                \
  }

#define POSITION_REPEAT_INST(n)                                                \
  static struct pr_phandles_filter                                             \
      position_repeat_config_phandles_whitelist_##n =                          \
          PR_EXTRACT_PHANDLES(n, whitelist_phandles);                          \
                                                                               \
  static struct pr_bindings_filter                                             \
      position_repeat_config_bindings_whitelist_##n =                          \
          PR_EXTRACT_BINDINGS(n, whitelist_bindings);                          \
                                                                               \
  static struct pr_phandles_filter                                             \
      position_repeat_config_phandles_transparent_##n =                        \
          PR_EXTRACT_PHANDLES(n, transparent_phandles);                        \
                                                                               \
  static struct pr_bindings_filter                                             \
      position_repeat_config_bindings_transparent_##n =                        \
          PR_EXTRACT_BINDINGS(n, transparent_bindings);                        \
                                                                               \
  static struct pr_filter_settings position_repeat_filter_settings_##n = {     \
      .use_whitelist = DT_INST_PROP_OR(n, use_whitelist, false),               \
      .whitelist =                                                             \
          {                                                                    \
              .phandles = &position_repeat_config_phandles_whitelist_##n,      \
              .bindings = &position_repeat_config_bindings_whitelist_##n,      \
          },                                                                   \
      .transparent =                                                           \
          {                                                                    \
              .phandles = &position_repeat_config_phandles_transparent_##n,    \
              .bindings = &position_repeat_config_bindings_transparent_##n,    \
          },                                                                   \
  };                                                                           \
                                                                               \
  static struct behavior_position_repeat_data position_repeat_data_##n = {     \
      .filter_settings = &position_repeat_filter_settings_##n,                 \
  };                                                                           \
                                                                               \
  BEHAVIOR_DT_INST_DEFINE(                                                     \
      n, /* Instance Number (Automatically populated by macro) */              \
      position_repeat_init,      /* Initialization Function */                 \
      NULL,                      /* Power Management Device Pointer */         \
      &position_repeat_data_##n, /* Behavior Data Pointer */                   \
      NULL,                      /* Behavior Configuration Pointer */          \
      POST_KERNEL,               /* Initialization Level */                    \
      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, /* Device Priority */               \
      &position_repeat_driver_api);        /* API struct */

DT_INST_FOREACH_STATUS_OKAY(POSITION_REPEAT_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
