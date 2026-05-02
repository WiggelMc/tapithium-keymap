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
  zmk_keymap_layer_index_t layer;
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

static zmk_keymap_layer_index_t tp_layer_index_from_int(const int layer_int) {
  // TODO: Fix this when actual type of int is known
  if (layer_int < ZMK_KEYMAP_LAYERS_LEN) {
    return (zmk_keymap_layer_index_t)layer_int;
  } else {
    return ZMK_KEYMAP_LAYER_ID_INVAL;
  }
}

static zmk_mod_flags_t tp_to_mod_flag(const zmk_key_t keycode) {
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

static zmk_mod_flags_t tp_extract_mods(const zmk_key_t keycode) {
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

static int
tp_reraise_position_event(const struct zmk_position_state_changed *ev) {
  struct zmk_position_state_changed_event dupe_ev =
      copy_raised_zmk_position_state_changed(ev);
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

//
// Engine Handlers
//

static int tp_handle_on(const enum tp_mode mode,
                        const struct behavior_tapithium_mods_config *config) {
  // TODO
  const struct behavior_tapithium_mods_config *old_cfg = tp_data.config;
  const enum tp_stage old_stage = tp_data.stage;
  tp_data.config = config;
  tp_data.mode = mode;
  tp_data.stage = TP_STAGE_MODS_SELECT;

  if (config != old_cfg) {
    // Turn off old cfg layers
  }

  if (old_stage == TP_STAGE_IDLE) {
    // Clean up scheduled keys
  }

  // Turn on config layers

  return ZMK_BEHAVIOR_OPAQUE;
}

static int tp_handle_cancel() {
  // TODO
  tp_data.stage = TP_STAGE_IDLE;

  // Turn off config layers
  // Clean up scheduled keys

  return ZMK_BEHAVIOR_OPAQUE;
}

static int tp_handle_reset() {
  // TODO
  tp_data.stage = TP_STAGE_IDLE;

  // Turn off config layers
  // Clean up scheduled keys
  // Clean up and Disable active keys

  return ZMK_BEHAVIOR_OPAQUE;
}

static int tp_handle_mpress() {
  // Do nothing
  return ZMK_BEHAVIOR_OPAQUE;
}

static int tp_handle_none(const zmk_keymap_layer_index_t mod_layer_index) {
  // TODO

  if (tp_data.stage == TP_STAGE_MODS_SELECT) {
    tp_data.stage = TP_STAGE_MODS_ON;
    // Turn off all other config layers
  }

  return ZMK_BEHAVIOR_OPAQUE;
}

static int tp_handle_next(const zmk_keymap_layer_index_t mod_layer_index,
                          const struct zmk_behavior_binding_event event) {
  // TODO
  const enum tp_stage old_stage = tp_data.stage;

  if (old_stage != TP_STAGE_IDLE) {
    // Release Current Position

    switch (old_stage) {
    case TP_STAGE_MODS_SELECT:
      // Turn off current layer
      break;
    case TP_STAGE_MODS_ON:
      tp_data.stage = TP_STAGE_IDLE;
      // Turn off all config layers
      // TODO: Apply all scheduled keys
      break;
    }

    // Press Current Position
  }

  // Test Mod Injection
  // const int raise_status =
  //     tp_raise_position_event_from_behaviour(event, false);
  // if (raise_status < 0) {
  //   return raise_status;
  // }
  // zmk_keymap_layer_deactivate(event.layer);
  // return tp_raise_position_event_from_behaviour(event, true);
  //
  return ZMK_BEHAVIOR_OPAQUE;
}

static int tp_handle_mod(const zmk_key_t keycode,
                         const zmk_keymap_layer_index_t mod_layer_index) {
  const zmk_mod_flags_t mods = tp_extract_mods(keycode);
  // TODO
  const enum tp_stage old_stage = tp_data.stage;

  if (tp_data.stage == TP_STAGE_MODS_SELECT) {
    tp_data.stage = TP_STAGE_MODS_ON;
    // Turn off all other config layers
  }

  if (old_stage != TP_STAGE_IDLE) {
    // Add Mod to scheduled of current mode
  }

  return ZMK_BEHAVIOR_OPAQUE;
}

static int tp_handle_lay(const zmk_keymap_layer_index_t layer_index,
                         const zmk_keymap_layer_index_t mod_layer_index) {
  // TODO
  const enum tp_stage old_stage = tp_data.stage;

  if (tp_data.stage == TP_STAGE_MODS_SELECT) {
    tp_data.stage = TP_STAGE_MODS_ON;
    // Turn off all other config layers
  }

  if (old_stage != TP_STAGE_IDLE) {
    // Set Lay to scheduled of current mode
  }

  return ZMK_BEHAVIOR_OPAQUE;
}

static int tp_handle_release_sticky() {
  // TODO

  tp_data.is_sticky_pressed = false;
  // Clean up and Disable active sticky keys

  return ZMK_EV_EVENT_BUBBLE;
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
  tp_data.source = ev->source;
#endif

  if (tp_data.is_sticky_pressed) {
    const bool is_sticky_key = ev->position == tp_data.sticky_position;

    if ((!is_press && is_sticky_key) || (is_press && !is_sticky_key)) {
      return tp_handle_release_sticky();
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
  const zmk_keymap_layer_index_t mod_layer_index =
      tp_layer_index_from_int(event.layer);

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
    return tp_handle_none(mod_layer_index);
  case TP_NEXT_CMD:
    return tp_handle_next(mod_layer_index, event);
  case TP_MOD_CMD:
    return tp_handle_mod(param, mod_layer_index);
  case TP_LAY_CMD:
    if (param < ZMK_KEYMAP_LAYERS_LEN) {
      return tp_handle_lay((zmk_keymap_layer_index_t)param, mod_layer_index);
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
