/*
 * Copyright (c) 2026 WiggelMc
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_tapithium_mods

#include <drivers/behavior.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <zmk/keys.h>
#include <zmk/matrix.h>

#include <dt-bindings/zmk/tapithium_mods.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

//
// Register Listener
//

static int
tapithium_mods_position_state_changed_listener(const zmk_event_t *eh);

ZMK_LISTENER(behavior_tapithium_mods,
             tapithium_mods_position_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_tapithium_mods, zmk_position_state_changed);

//
// Types
//

enum tp_stage {
  TP_STAGE_IDLE,
  TP_STAGE_MODS_SELECT,
  TP_STAGE_MODS_ON,
};

enum tp_mode {
  TP_MODE_ENABLE,
  TP_MODE_STICKY,
};

struct behavior_tapithium_mods_config {
  zmk_keymap_layers_state_t mod_layers;
};

struct tp_action_props {
  zmk_keymap_layer_id_t layer;
  bool has_layer;
  zmk_mod_flags_t mods;
};

struct tp_action_data {
  struct tp_action_props scheduled;
  struct tp_action_props active;
};

struct behavior_tapithium_mods_engine_data {
  enum tp_stage stage;
  enum tp_mode mode;
  struct behavior_tapithium_mods_config *config;
  struct tp_action_data enabled;
  struct tp_action_data sticky;
  bool is_sticky_pressed;
  uint32_t sticky_position;
  uint8_t last_source;
};

static struct behavior_tapithium_mods_engine_data tp_data = {
    .stage = TP_STAGE_IDLE,
    .mode = TP_MODE_ENABLE,
    .config = NULL,
    .enabled = {},
    .sticky = {},
    .is_sticky_pressed = false,
    .sticky_position = 0,
    .last_source = 0,
};

//
// Helpers
//

static void tp_clear_action_props(struct tp_action_props *props) {

  props->layer = 0;
  props->has_layer = false;
  props->mods = 0;
}

static inline zmk_keymap_layer_id_t tp_layer_id_from_int(const int layer_int) {

  if (layer_int >= 0 && layer_int < ZMK_KEYMAP_LAYERS_LEN) {
    return (zmk_keymap_layer_id_t)layer_int;
  } else {
    return ZMK_KEYMAP_LAYER_ID_INVAL;
  }
}

static inline zmk_keymap_layers_state_t
tp_layers_without(const zmk_keymap_layers_state_t layers,
                  const zmk_keymap_layer_id_t excluded_layer) {

  if (excluded_layer < ZMK_KEYMAP_LAYERS_LEN) {
    return layers & ~(((zmk_keymap_layers_state_t)1U) << excluded_layer);
  } else {
    return layers;
  }
}

static inline zmk_mod_flags_t tp_to_mod_flag(const zmk_key_t keycode) {

  switch (keycode) {
  case LCTRL:
    return MOD_LCTL;
  case LSHIFT:
    return MOD_LSFT;
  case LALT:
    return MOD_LALT;
  case LGUI:
    return MOD_LGUI;
  case RCTRL:
    return MOD_RCTL;
  case RSHIFT:
    return MOD_RSFT;
  case RALT:
    return MOD_RALT;
  case RGUI:
    return MOD_RGUI;
  default:
    return 0;
  }
}

static inline zmk_key_t tp_mod_index_to_keycode(const uint8_t mod_index) {

  if (mod_index >= 8) {
    return 0;
  }

  switch (1U << mod_index) {
  case MOD_LCTL:
    return LCTRL;
  case MOD_LSFT:
    return LSHIFT;
  case MOD_LALT:
    return LALT;
  case MOD_LGUI:
    return LGUI;
  case MOD_RCTL:
    return RCTRL;
  case MOD_RSFT:
    return RSHIFT;
  case MOD_RALT:
    return RALT;
  case MOD_RGUI:
    return RGUI;
  default:
    return 0;
  }
}

static inline zmk_mod_flags_t tp_extract_mods(const zmk_key_t keycode) {

  const zmk_mod_flags_t mods = SELECT_MODS(keycode);
  const zmk_mod_flags_t key_mod = tp_to_mod_flag(STRIP_MODS(keycode));
  return mods | key_mod;
}

static int tp_raise_keycode_event(const zmk_key_t keycode, const bool pressed) {

  struct zmk_keycode_state_changed data =
      zmk_keycode_state_changed_from_encoded(keycode, pressed, k_uptime_get());

  struct zmk_keycode_state_changed_event ev = {
      .data = data, .header = {.event = &zmk_event_zmk_keycode_state_changed}};
  return ZMK_EVENT_RAISE(ev);
}

static int tp_raise_keycode_events_from_mods(const zmk_mod_flags_t mods,
                                             const bool pressed) {

  zmk_mod_flags_t bits = mods;
  uint8_t mod_index = 0;

  while (bits != 0) {
    if (bits & ((zmk_mod_flags_t)1U)) {
      const zmk_key_t keycode = tp_mod_index_to_keycode(mod_index);
      if (keycode != 0) {
        tp_raise_keycode_event(keycode, pressed);
      }
    }

    bits >>= 1U;
    mod_index++;
  }
  return 0;
}

static int
tp_reraise_position_event(const struct zmk_position_state_changed *ev) {

  struct zmk_position_state_changed data = {
      .source = ev->source,
      .position = ev->position,
      .state = ev->state,
      .timestamp = k_uptime_get(),
  };

  struct zmk_position_state_changed_event dupe_ev = {
      .data = data, .header = {.event = &zmk_event_zmk_position_state_changed}};
  return ZMK_EVENT_RAISE_AFTER(dupe_ev, behavior_tapithium_mods);
}

static int tp_raise_position_event_from_behaviour(
    const struct zmk_behavior_binding_event event, const bool pressed) {

  struct zmk_position_state_changed data = {
      .source = tp_data.last_source,
      .position = event.position,
      .state = pressed,
      .timestamp = k_uptime_get(),
  };
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
  data.source = event.source;
#endif

  struct zmk_position_state_changed_event ev = {
      .data = data, .header = {.event = &zmk_event_zmk_position_state_changed}};
  return ZMK_EVENT_RAISE_AFTER(ev, behavior_tapithium_mods);
}

static bool tp_is_any_layer_active(const zmk_keymap_layers_state_t layers) {
  const zmk_keymap_layers_state_t state = zmk_keymap_layer_state();
  return (state & layers) != 0;
}

static int tp_set_layer_state(const zmk_keymap_layer_id_t layer,
                              const bool state) {

  if (layer < ZMK_KEYMAP_LAYERS_LEN) {
    if (state) {
      zmk_keymap_layer_activate(layer);
    } else {
      zmk_keymap_layer_deactivate(layer);
    }
  }

  return 0;
}

static int tp_set_all_layer_states(const zmk_keymap_layers_state_t layers,
                                   const bool state) {

  zmk_keymap_layers_state_t bits = layers;
  zmk_keymap_layer_id_t id = 0;

  while (bits != 0) {
    if (bits & ((zmk_keymap_layers_state_t)1U)) {
      if (state) {
        zmk_keymap_layer_activate(id);
      } else {
        zmk_keymap_layer_deactivate(id);
      }
    }

    bits >>= 1U;
    id++;
  }

  return 0;
}

static int tpe_clear_scheduled() {

  tp_clear_action_props(&tp_data.enabled.scheduled);
  tp_clear_action_props(&tp_data.sticky.scheduled);
  return 0;
}

static int
tpe_select_mod_layer(const zmk_keymap_layer_id_t mod_layer_id,
                     const struct behavior_tapithium_mods_config *config) {

  tp_data.stage = TP_STAGE_MODS_ON;

  if (config != NULL) {
    tp_set_all_layer_states(tp_layers_without(config->mod_layers, mod_layer_id),
                            false);

    if (!tp_is_any_layer_active(config->mod_layers)) {
      tp_data.stage = TP_STAGE_IDLE;
      tpe_clear_scheduled();
    }
  }

  return 0;
}

static int tpe_schedule_mods(const zmk_mod_flags_t mods,
                             const enum tp_mode mode) {

  switch (mode) {
  case TP_MODE_ENABLE:
    tp_data.enabled.scheduled.mods |= mods;
    break;
  case TP_MODE_STICKY:
    tp_data.sticky.scheduled.mods |= mods;
    break;
  }
  return 0;
}

static int tpe_schedule_layer(const zmk_keymap_layer_id_t layer,
                              const enum tp_mode mode) {

  if (layer < ZMK_KEYMAP_LAYERS_LEN) {
    switch (mode) {
    case TP_MODE_ENABLE: {
      struct tp_action_props *enabled_props = &tp_data.enabled.scheduled;
      enabled_props->layer = layer;
      enabled_props->has_layer = true;

      struct tp_action_props *sticky_props = &tp_data.sticky.scheduled;
      sticky_props->layer = 0;
      sticky_props->has_layer = false;
      break;
    }
    case TP_MODE_STICKY: {
      struct tp_action_props *sticky_props = &tp_data.sticky.scheduled;
      sticky_props->layer = layer;
      sticky_props->has_layer = true;
      break;
    }
    }
  }

  return 0;
}

static int tpe_release_mods(const zmk_mod_flags_t mods) {
  const zmk_mod_flags_t active_mods =
      tp_data.enabled.active.mods | tp_data.sticky.active.mods;
  const zmk_mod_flags_t release_mods = mods & active_mods;

  tp_data.enabled.active.mods &= ~release_mods;
  tp_data.sticky.active.mods &= ~release_mods;

  tp_raise_keycode_events_from_mods(release_mods, false);

  return 0;
}

static int tpe_press_mods(const zmk_mod_flags_t mods, const enum tp_mode mode) {
  const zmk_mod_flags_t active_mods =
      tp_data.enabled.active.mods | tp_data.sticky.active.mods;
  const zmk_mod_flags_t press_mods = mods & ~active_mods;

  switch (mode) {
  case TP_MODE_ENABLE:
    tp_data.enabled.active.mods |= press_mods;
    break;
  case TP_MODE_STICKY:
    tp_data.sticky.active.mods |= press_mods;
    break;
  default:
    return -1;
  }

  tp_raise_keycode_events_from_mods(press_mods, true);

  return 0;
}

static int tpe_apply_scheduled_mods() {

  const zmk_mod_flags_t enabled_active_mods = tp_data.enabled.active.mods;
  const zmk_mod_flags_t sticky_active_mods = tp_data.sticky.active.mods;
  const zmk_mod_flags_t active_mods = enabled_active_mods | sticky_active_mods;

  const zmk_mod_flags_t enabled_scheduled_mods = tp_data.enabled.scheduled.mods;
  const zmk_mod_flags_t sticky_scheduled_mods = tp_data.sticky.scheduled.mods;
  const zmk_mod_flags_t scheduled_mods =
      enabled_scheduled_mods | sticky_scheduled_mods;

  const zmk_mod_flags_t retrigger_mods = active_mods & scheduled_mods;
  const zmk_mod_flags_t trigger_enabled_mods =
      enabled_scheduled_mods | (retrigger_mods & enabled_active_mods);
  const zmk_mod_flags_t trigger_sticky_mods =
      sticky_scheduled_mods & ~trigger_enabled_mods;

  tpe_release_mods(retrigger_mods);
  tpe_press_mods(trigger_enabled_mods, TP_MODE_ENABLE);
  tpe_press_mods(trigger_sticky_mods, TP_MODE_STICKY);

  return 0;
}

static int tpe_apply_scheduled_layers() {

  // struct tp_action_props *enabled_active_props = &tp_data.enabled.active;
  // struct tp_action_props *sticky_active_props = &tp_data.sticky.active;

  // struct tp_action_props *enabled_scheduled_props =
  // &tp_data.enabled.scheduled; struct tp_action_props *sticky_scheduled_props
  // = &tp_data.sticky.scheduled;

  // if (sticky_scheduled_props->has_layer) {

  // } else if (enabled_scheduled_props->has_layer) {
  // }

  // TODO
  return 0;
}

static int tpe_apply_scheduled(const uint32_t position) {

  tp_data.is_sticky_pressed = true;
  tp_data.sticky_position = position;

  tpe_apply_scheduled_mods();
  tpe_apply_scheduled_layers();

  return 0;
}

static int tpe_deactivate_sticky() {

  tp_data.is_sticky_pressed = false;

  tpe_release_mods(tp_data.sticky.active.mods);
  // Release Layers

  // TODO
  return 0;
}

static int tpe_deactivate_enabled() {

  tpe_release_mods(tp_data.enabled.active.mods);
  // Release Layers

  // TODO
  return 0;
}

static int tpe_deactivate_all() {

  tpe_deactivate_enabled();
  tpe_deactivate_sticky();
  return 0;
}

//
// Engine Handlers
//

static int tp_handle_on(const enum tp_mode mode,
                        const struct behavior_tapithium_mods_config *config) {

  const struct behavior_tapithium_mods_config *old_cfg = tp_data.config;
  const enum tp_stage old_stage = tp_data.stage;

  if (config != NULL) {
    tp_data.config = config;
    tp_data.mode = mode;
    tp_data.stage = TP_STAGE_MODS_SELECT;

    if (config != old_cfg && old_cfg != NULL) {
      tp_set_all_layer_states(old_cfg->mod_layers, false);
    }

    if (old_stage == TP_STAGE_IDLE) {
      tpe_clear_scheduled();
    }

    tp_set_all_layer_states(config->mod_layers, true);
  }

  return ZMK_BEHAVIOR_OPAQUE;
}

static int tp_handle_cancel() {

  const struct behavior_tapithium_mods_config *cfg = tp_data.config;
  tp_data.stage = TP_STAGE_IDLE;

  if (cfg != NULL) {
    tp_set_all_layer_states(cfg->mod_layers, false);
  }
  tpe_clear_scheduled();

  return ZMK_BEHAVIOR_OPAQUE;
}

static int tp_handle_reset() {

  const struct behavior_tapithium_mods_config *cfg = tp_data.config;
  tp_data.stage = TP_STAGE_IDLE;

  if (cfg != NULL) {
    tp_set_all_layer_states(cfg->mod_layers, false);
  }
  tpe_clear_scheduled();
  tpe_deactivate_all();

  return ZMK_BEHAVIOR_OPAQUE;
}

static int tp_handle_mpress() { return ZMK_BEHAVIOR_OPAQUE; }

static int tp_handle_none(const zmk_keymap_layer_id_t mod_layer_id) {

  if (tp_data.stage == TP_STAGE_MODS_SELECT) {
    tpe_select_mod_layer(mod_layer_id, tp_data.config);
  }

  return ZMK_BEHAVIOR_OPAQUE;
}

static int tp_handle_next(const zmk_keymap_layer_id_t mod_layer_id,
                          const struct zmk_behavior_binding_event event) {

  const enum tp_stage old_stage = tp_data.stage;

  if (old_stage != TP_STAGE_IDLE) {
    tp_raise_position_event_from_behaviour(event, false);

    switch (old_stage) {
    case TP_STAGE_MODS_SELECT: {
      tp_set_layer_state(mod_layer_id, false);
      const struct behavior_tapithium_mods_config *cfg = tp_data.config;

      if (cfg != NULL && !tp_is_any_layer_active(cfg->mod_layers)) {
        tp_data.stage = TP_STAGE_IDLE;
        tpe_clear_scheduled();
      }
      break;
    }
    case TP_STAGE_MODS_ON: {
      tp_data.stage = TP_STAGE_IDLE;
      const struct behavior_tapithium_mods_config *cfg = tp_data.config;
      if (cfg != NULL) {
        tp_set_all_layer_states(cfg->mod_layers, false);
      }
      tpe_apply_scheduled(event.position);
    }

    break;
    }

    tp_raise_position_event_from_behaviour(event, true);
  }

  return ZMK_BEHAVIOR_OPAQUE;
}

static int tp_handle_mod(const zmk_key_t keycode,
                         const zmk_keymap_layer_id_t mod_layer_id) {

  const enum tp_stage old_stage = tp_data.stage;

  if (tp_data.stage == TP_STAGE_MODS_SELECT) {
    tpe_select_mod_layer(mod_layer_id, tp_data.config);
  }

  if (old_stage != TP_STAGE_IDLE) {
    const zmk_mod_flags_t mods = tp_extract_mods(keycode);
    tpe_schedule_mods(mods, tp_data.mode);
  }

  return ZMK_BEHAVIOR_OPAQUE;
}

static int tp_handle_lay(const zmk_keymap_layer_id_t layer_id,
                         const zmk_keymap_layer_id_t mod_layer_id) {

  const enum tp_stage old_stage = tp_data.stage;

  if (tp_data.stage == TP_STAGE_MODS_SELECT) {
    tpe_select_mod_layer(mod_layer_id, tp_data.config);
  }

  if (old_stage != TP_STAGE_IDLE) {
    tpe_schedule_layer(layer_id, tp_data.mode);
  }

  return ZMK_BEHAVIOR_OPAQUE;
}

static int tp_handle_sticky_intercept() {

  tp_data.is_sticky_pressed = false;
  tpe_deactivate_sticky();

  return ZMK_EV_EVENT_BUBBLE;
}

static int
tp_handle_sticky_release(const struct zmk_position_state_changed *ev) {

  tp_data.is_sticky_pressed = false;

  tp_reraise_position_event(ev);
  tpe_deactivate_sticky();

  return ZMK_EV_EVENT_HANDLED;
}

//
// Zmk Handlers
//

static int
tapithium_mods_position_state_changed_listener(const zmk_event_t *eh) {

  const struct zmk_position_state_changed *ev =
      as_zmk_position_state_changed(eh);
  if (ev == NULL) {
    return ZMK_EV_EVENT_BUBBLE;
  }

  LOG_DBG("TP Position State changed: position: %d, source: %d", ev->position,
          ev->source);

  const bool is_press = ev->state;

#if !IS_ENABLED(CONFIG_ZMK_SPLIT)
  tp_data.last_source = ev->source;
#endif

  if (tp_data.is_sticky_pressed) {
    const bool is_sticky_key = ev->position == tp_data.sticky_position;

    if (!is_press && is_sticky_key) {
      return tp_handle_sticky_release(ev);
    } else if (is_press && !is_sticky_key) {
      return tp_handle_sticky_intercept();
    }
  }

  return ZMK_EV_EVENT_BUBBLE;
}

static int tapithium_mods_init(const struct device *dev) {
  LOG_DBG("TP Initialized");
  return 0;
};

static int
on_tapithium_mods_binding_pressed(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event) {

  const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
  const struct behavior_tapithium_mods_config *cfg = dev->config;

  LOG_DBG("TP Binding pressed: param1=%d, param2=%d, layer=%d, position=%d",
          binding->param1, binding->param2, event.layer, event.position);

  const uint32_t command = binding->param1;
  const uint32_t param = binding->param2;
  const zmk_keymap_layer_id_t mod_layer_id = tp_layer_id_from_int(event.layer);

  switch (command) {
  case TP_ENABLE_CMD:
    return tp_handle_on(TP_MODE_ENABLE, cfg);
  case TP_STICKY_CMD:
    return tp_handle_on(TP_MODE_STICKY, cfg);
  case TP_CANCEL_CMD:
    return tp_handle_cancel();
  case TP_RESET_CMD:
    return tp_handle_reset();
  case TP_MPRESS_CMD:
    return tp_handle_mpress();
  case TP_NONE_CMD:
    return tp_handle_none(mod_layer_id);
  case TP_NEXT_CMD:
    return tp_handle_next(mod_layer_id, event);
  case TP_MOD_CMD:
    return tp_handle_mod((zmk_key_t)param, mod_layer_id);
  case TP_LAY_CMD:
    if (param < ZMK_KEYMAP_LAYERS_LEN) {
      return tp_handle_lay((zmk_keymap_layer_id_t)param, mod_layer_id);
    } else {
      return ZMK_BEHAVIOR_OPAQUE;
    }
  default:
    return ZMK_BEHAVIOR_OPAQUE;
  }
}

static int
on_tapithium_mods_binding_released(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event) {

  LOG_DBG("TP Binding released: param1=%d, param2=%d, layer=%d, position=%d",
          binding->param1, binding->param2, event.layer, event.position);

  return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api tapithium_mods_driver_api = {
    .binding_pressed = on_tapithium_mods_binding_pressed,
    .binding_released = on_tapithium_mods_binding_released,
};

//
// Logging
//

static int tapithium_mods_keycode_state_changed_listener(const zmk_event_t *eh);

ZMK_LISTENER(behavior_tapithium_mods_keycode_state_changed,
             tapithium_mods_keycode_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_tapithium_mods_keycode_state_changed,
                 zmk_keycode_state_changed);

static int
tapithium_mods_keycode_state_changed_listener(const zmk_event_t *eh) {

  const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
  if (ev == NULL) {
    return ZMK_EV_EVENT_BUBBLE;
  }

  LOG_DBG("TP Keycode State changed: keycode: %d, state: %d", ev->keycode,
          ev->state);

  return ZMK_EV_EVENT_BUBBLE;
}

static int tapithium_mods_layer_state_changed_listener(const zmk_event_t *eh);

ZMK_LISTENER(behavior_tapithium_mods_layer_state_changed,
             tapithium_mods_layer_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_tapithium_mods_layer_state_changed,
                 zmk_layer_state_changed);

static int tapithium_mods_layer_state_changed_listener(const zmk_event_t *eh) {

  const struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);
  if (ev == NULL) {
    return ZMK_EV_EVENT_BUBBLE;
  }

  LOG_DBG("TP Layer State changed");

  return ZMK_EV_EVENT_BUBBLE;
}

//
// Define Behavior
//

#define TP_LAYER_BIT(node_id, prop, idx)                                       \
  (1U << DT_PROP_BY_IDX(node_id, prop, idx)) |

#define TP_BUILD_LAYER_MASK(n, prop)                                           \
  (DT_INST_FOREACH_PROP_ELEM(n, prop, TP_LAYER_BIT) 0U)

#define TAPITHIUM_MODS_INST(n)                                                 \
  static struct behavior_tapithium_mods_config tapithium_mods_config_##n = {   \
      .mod_layers = TP_BUILD_LAYER_MASK(n, mod_layers),                        \
  };                                                                           \
                                                                               \
  BEHAVIOR_DT_INST_DEFINE(                                                     \
      n, /* Instance Number (Automatically populated by macro) */              \
      tapithium_mods_init,        /* Initialization Function */                \
      NULL,                       /* Power Management Device Pointer */        \
      NULL,                       /* Behavior Data Pointer */                  \
      &tapithium_mods_config_##n, /* Behavior Configuration Pointer */         \
      POST_KERNEL,                /* Initialization Level */                   \
      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, /* Device Priority */               \
      &tapithium_mods_driver_api);         /* API struct */

DT_INST_FOREACH_STATUS_OKAY(TAPITHIUM_MODS_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
