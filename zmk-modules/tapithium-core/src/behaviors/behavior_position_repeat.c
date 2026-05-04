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

#define PR_HOLD_LIST_SIZE CONFIG_ZMK_BEHAVIOR_POSITION_REPEAT_MAX_BINDINGS_HELD
#define PR_POS_BUFFER_SIZE                                                     \
  CONFIG_ZMK_BEHAVIOR_POSITION_REPEAT_POSITION_BUFFER_SIZE

struct pr_phandles_filter_item {
  const struct device *dev;
  const char *behavior_dev;
};

struct pr_phandles_filter {
  int count;
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
  struct pr_phandles_filter *handles;
  struct pr_bindings_filter *bindings;
};

struct behavior_position_repeat_config {
  bool use_whitelist;
  struct pr_filter whitelist;
  struct pr_filter transparent;
};

struct behavior_position_repeat_engine_data {};

static struct behavior_position_repeat_engine_data pr_data = {

};

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

  return ZMK_EV_EVENT_BUBBLE;
}

static int position_repeat_init(const struct device *dev) {

  

  LOG_DBG("PR Initialized");
  return 0;
};

static int
on_position_repeat_binding_pressed(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event) {

  const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
  const struct behavior_position_repeat_config *cfg = dev->config;

  LOG_DBG("PR Binding pressed: param1=%d, param2=%d, layer=%d, position=%d",
          binding->param1, binding->param2, event.layer, event.position);

  return ZMK_BEHAVIOR_OPAQUE;
}

static int
on_position_repeat_binding_released(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {

  LOG_DBG("PR Binding released: param1=%d, param2=%d, layer=%d, position=%d",
          binding->param1, binding->param2, event.layer, event.position);

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
  static struct behavior_position_repeat_config position_repeat_config_##n = { \
      .use_whitelist = true,                                                   \
      .whitelist =                                                             \
          {                                                                    \
              .handles = &position_repeat_config_phandles_whitelist_##n,       \
              .bindings = &position_repeat_config_bindings_whitelist_##n,      \
          },                                                                   \
      .transparent =                                                           \
          {                                                                    \
              .handles = &position_repeat_config_phandles_transparent_##n,     \
              .bindings = &position_repeat_config_bindings_transparent_##n,    \
          },                                                                   \
  };                                                                           \
                                                                               \
  BEHAVIOR_DT_INST_DEFINE(                                                     \
      n, /* Instance Number (Automatically populated by macro) */              \
      position_repeat_init,        /* Initialization Function */               \
      NULL,                        /* Power Management Device Pointer */       \
      NULL,                        /* Behavior Data Pointer */                 \
      &position_repeat_config_##n, /* Behavior Configuration Pointer */        \
      POST_KERNEL,                 /* Initialization Level */                  \
      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, /* Device Priority */               \
      &position_repeat_driver_api);        /* API struct */

DT_INST_FOREACH_STATUS_OKAY(POSITION_REPEAT_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
