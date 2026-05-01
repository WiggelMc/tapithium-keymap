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

enum tp_stage {
  TP_STAGE_IDLE,
  TP_STAGE_MODS_SELECT,
  TP_STAGE_MODS_ON,
};

enum tp_mode {
  TP_MODE_ENABLE,
  TP_MODE_STICKY,
};

typedef int32_t tp_optional_keymap_layer_index_t;

struct behavior_tapithium_mods_config {
  zmk_keymap_layers_state_t mod_layers;
  int32_t cancel_after_idle_ms;
};

struct tp_action_props {
  tp_optional_keymap_layer_index_t layer;
  zmk_mod_flags_t mods;
};

struct tp_action_data {
  struct tp_action_props scheduled;
  struct tp_action_props active;
};

#define DEFAULT_TP_ACTION_PROPS                                                \
  {                                                                            \
      .layer = -1,                                                             \
      .mods = 0,                                                               \
  }

#define DEFAULT_TP_ACTION_DATA                                                 \
  {                                                                            \
      .scheduled = DEFAULT_TP_ACTION_PROPS,                                    \
      .active = DEFAULT_TP_ACTION_PROPS,                                       \
  }

struct behavior_tapithium_mods_engine_data {
  enum tp_stage stage;
  enum tp_mode mode;
  struct behavior_tapithium_mods_config *config;
  int64_t last_input_timestamp;
  struct tp_action_data enabled;
  struct tp_action_data sticky;
  bool is_sticky_pressed;
  uint32_t sticky_position;
};

static uint32_t test_data = 0;

static struct behavior_tapithium_mods_engine_data tapithium_mods_engine_data = {
    .stage = TP_STAGE_IDLE,
    .mode = TP_MODE_ENABLE,
    .config = NULL,
    .last_input_timestamp = 0,
    .enabled = DEFAULT_TP_ACTION_DATA,
    .sticky = DEFAULT_TP_ACTION_DATA,
    .is_sticky_pressed = false,
    .sticky_position = 0,
};

// TODO:
// TP Key Press => [Actions + Keypress] (Used for selection with TP_NEXT and for
//   exit with TP_NEXT and for disable of sticky before press)
// TP Key Release => [Keypress + Actions] (Used for disable
//   of sticky after release)

static int tapithium_mods_init(const struct device *dev) {
  LOG_DBG("TP Initialized");
  return 0;
};

static int
tp_raise_position_event_from_behaviour(struct zmk_behavior_binding_event event,
                                       bool pressed) {
  struct zmk_position_state_changed data = {
      .source = 0,
      .position = event.position,
      .state = pressed,
      .timestamp = k_uptime_get(),
  };
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
  data.source = event.source;
#endif

  struct zmk_position_state_changed_event ev = {
      .data = data, .header = {.event = &zmk_event_zmk_position_state_changed}};
  return ZMK_EVENT_RAISE(ev);
}

static int
on_tapithium_mods_binding_pressed(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event) {

  LOG_DBG("TP Binding pressed: param1=%d, param2=%d, layer=%d, position=%d",
          binding->param1, binding->param2, event.layer, event.position);

  const uint32_t command = binding->param1;
  const uint32_t param = binding->param2;

  switch (command) {
  case TP_ENABLE_CMD:
    // TODO (Enable with Enable Mode)
    test_data = 0;
    break;
  case TP_STICKY_CMD:
    // TODO (Enable with Sticky Mode)
    test_data = 3;
    break;
  case TP_CANCEL_CMD:
    // TODO (Cancel)
    test_data = 2;
    break;
  case TP_RESET_CMD:
    // TODO (Reset)
    test_data = 1;
    break;
  case TP_MPRESS_CMD:
    // Do nothing
    break;
  case TP_NONE_CMD:
    // TODO (Capture)
    break;
  case TP_NEXT_CMD:
    // TODO (Fall through, Exit)
    // Test Mod Injection
    const int raise_status =
        tp_raise_position_event_from_behaviour(event, false);
    if (raise_status < 0) {
      return raise_status;
    }
    zmk_keymap_layer_deactivate(event.layer);
    return tp_raise_position_event_from_behaviour(event, true);
    //
    break;
  case TP_MOD_CMD:
    // zmk_key_t (type is already the same)
    // TODO (Capture, add Mod)
    break;
  case TP_LAY_CMD:
    // zmk_keymap_layer_index_t (downcast?? u32 -> u8)
    // TODO (Capture, set Layer)
    break;
  }

  return ZMK_BEHAVIOR_OPAQUE;
}

static void tp_cancel_idle() {
  // TODO
}

static void tp_reset_idle() {
  // TODO
}

static void
tp_reset_idle_with_config(struct behavior_tapithium_mods_config *config) {
  // TODO
}

static int
on_tapithium_mods_binding_released(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event) {
  LOG_DBG("TP Binding released: param1=%d, param2=%d, layer=%d, position=%d",
          binding->param1, binding->param2, event.layer, event.position);

  const uint32_t command = binding->param1;

  switch (command) {
  case TP_ENABLE_CMD:
  case TP_STICKY_CMD:
    tp_reset_idle();
    break;
  case TP_CANCEL_CMD:
  case TP_RESET_CMD:
  case TP_MPRESS_CMD:
    // Do nothing
    break;
  case TP_NONE_CMD:
    tp_reset_idle();
    break;
  case TP_NEXT_CMD:
    // Do nothing
    break;
  case TP_MOD_CMD:
  case TP_LAY_CMD:
    tp_reset_idle();
    break;
  }

  return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api tapithium_mods_driver_api = {
    .binding_pressed = on_tapithium_mods_binding_pressed,
    .binding_released = on_tapithium_mods_binding_released,
};

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

static int
tapithium_mods_position_state_changed_listener(const zmk_event_t *eh);

ZMK_LISTENER(behavior_tapithium_mods_position_state_changed,
             tapithium_mods_position_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_tapithium_mods_position_state_changed,
                 zmk_position_state_changed);

static int tp_raise_keycode_event(zmk_key_t keycode, bool pressed) {
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
  return ZMK_EVENT_RAISE_AFTER(dupe_ev,
                               behavior_tapithium_mods_position_state_changed);
}

static int
tapithium_mods_position_state_changed_listener(const zmk_event_t *eh) {
  const struct zmk_position_state_changed *ev =
      as_zmk_position_state_changed(eh);
  if (ev == NULL) {
    return ZMK_EV_EVENT_BUBBLE;
  }

  LOG_DBG("TP Position State changed: position: %d, source: %d", ev->position,
          ev->source);

  switch (test_data) {
  case 1:
    if (ev->state) {
      test_data = 0;
      tp_raise_keycode_event(LSHIFT, true);
      return ZMK_EV_EVENT_BUBBLE;
      // inject press shift before press
    }
    break;
  case 2:
    if (!(ev->state)) {
      test_data = 20;
    }
    break;
  case 20:
    if (!(ev->state)) {
      test_data = 0;
      tp_reraise_position_event(ev);
      tp_raise_keycode_event(LSHIFT, false);
      return ZMK_EV_EVENT_HANDLED;
      // Inject Release shift after release
    }
    break;
  case 3:
    if (ev->state) {
      test_data = 0;
      zmk_keymap_layer_activate(1);
      // Inject moving to layer 1 before press
    }
    break;
  }

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

#define TP_LAYER_BIT(node_id, prop, idx)                                       \
  (1U << DT_PROP_BY_IDX(node_id, prop, idx)) |

#define TP_BUILD_LAYER_MASK(n, prop)                                           \
  (DT_INST_FOREACH_PROP_ELEM(n, prop, TP_LAYER_BIT) 0U)

#define TAPITHIUM_MODS_INST(n)                                                 \
  static struct behavior_tapithium_mods_config tapithium_mods_config_##n = {   \
      .mod_layers = TP_BUILD_LAYER_MASK(n, mod_layers),                        \
      .cancel_after_idle_ms = DT_INST_PROP(n, cancel_after_idle_ms),           \
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
