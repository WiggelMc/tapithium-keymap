/*
 * Copyright (c) 2026 WiggelMc
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_tapithium_mods

// Dependencies
#include <drivers/behavior.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/modifiers_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <zmk/keys.h>
#include <zmk/matrix.h>

#include <dt-bindings/zmk/tapithium_mods.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

typedef uint8_t zmk_tp_stage_t;
#define TP_STAGE_IDLE 0
#define TP_STAGE_MODS_SELECT 1
#define TP_STAGE_MODS_ON 2

typedef uint8_t zmk_tp_mode_t;
#define TP_MODE_ENABLE 0
#define TP_MODE_STICKY 1

struct behavior_tapithium_mods_data {
  bool is_active;
};

struct behavior_tapithium_mods_config {
  zmk_keymap_layers_state_t mod_layers;
  int32_t cancel_after_idle_ms;
};

struct behavior_tapithium_mods_engine_data {
  zmk_tp_stage_t stage;
  zmk_tp_mode_t mode;
  struct behavior_tapithium_mods_config *config;
  zmk_keymap_layer_index_t pending_layer;
  zmk_mod_flags_t pending_mods;
};

static struct behavior_tapithium_mods_engine_data tapithium_mods_engine_data = {
    .stage = TP_STAGE_IDLE,
    .mode = TP_MODE_ENABLE,
    .config = NULL,
    .pending_layer = 0,
    .pending_mods = 0,
};

static int tapithium_mods_init(const struct device *dev) { return 0; };

static int
on_tapithium_mods_binding_pressed(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event) {
  return ZMK_BEHAVIOR_OPAQUE;
}

static int
on_tapithium_mods_binding_released(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event) {
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
  struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
  if (ev == NULL) {
    return ZMK_EV_EVENT_BUBBLE;
  }

  return ZMK_EV_EVENT_BUBBLE;
}

static int
tapithium_mods_position_state_changed_listener(const zmk_event_t *eh);

ZMK_LISTENER(behavior_tapithium_mods_position_state_changed,
             tapithium_mods_position_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_tapithium_mods_position_state_changed,
                 zmk_position_state_changed);

static int
tapithium_mods_position_state_changed_listener(const zmk_event_t *eh) {
  struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
  if (ev == NULL) {
    return ZMK_EV_EVENT_BUBBLE;
  }

  return ZMK_EV_EVENT_BUBBLE;
}

static int tapithium_mods_layer_state_changed_listener(const zmk_event_t *eh);

ZMK_LISTENER(behavior_tapithium_mods_layer_state_changed,
             tapithium_mods_layer_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_tapithium_mods_layer_state_changed,
                 zmk_layer_state_changed);

static int tapithium_mods_layer_state_changed_listener(const zmk_event_t *eh) {
  struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);
  if (ev == NULL) {
    return ZMK_EV_EVENT_BUBBLE;
  }

  return ZMK_EV_EVENT_BUBBLE;
}

static int
tapithium_mods_modifiers_state_changed_listener(const zmk_event_t *eh);

ZMK_LISTENER(behavior_tapithium_mods_modifiers_state_changed,
             tapithium_mods_modifiers_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_tapithium_mods_modifiers_state_changed,
                 zmk_modifiers_state_changed);

static int
tapithium_mods_modifiers_state_changed_listener(const zmk_event_t *eh) {
  struct zmk_modifiers_state_changed *ev = as_zmk_modifiers_state_changed(eh);
  if (ev == NULL) {
    return ZMK_EV_EVENT_BUBBLE;
  }

  return ZMK_EV_EVENT_BUBBLE;
}

#define TP_LAYER_BIT(node_id, prop, idx)                                       \
  (1U << DT_PROP_BY_IDX(node_id, prop, idx)) |

#define TP_BUILD_LAYER_MASK(n, prop)                                           \
  (DT_INST_FOREACH_PROP_ELEM(n, prop, TP_LAYER_BIT) 0U)

#define TAPITHIUM_MODS_INST(n)                                                 \
  static struct behavior_tapithium_mods_data tapithium_mods_data_##n = {};     \
                                                                               \
  static struct behavior_tapithium_mods_config tapithium_mods_config_##n = {   \
      .mod_layers = TP_BUILD_LAYER_MASK(n, mod_layers),                        \
      .cancel_after_idle_ms = DT_INST_PROP(n, cancel_after_idle_ms),           \
  };                                                                           \
                                                                               \
  BEHAVIOR_DT_INST_DEFINE(                                                     \
      n, /* Instance Number (Automatically populated by macro) */              \
      tapithium_mods_init,        /* Initialization Function */                \
      NULL,                       /* Power Management Device Pointer */        \
      &tapithium_mods_data_##n,   /* Behavior Data Pointer */                  \
      &tapithium_mods_config_##n, /* Behavior Configuration Pointer */         \
      POST_KERNEL,                /* Initialization Level */                   \
      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, /* Device Priority */               \
      &tapithium_mods_driver_api);         /* API struct */

DT_INST_FOREACH_STATUS_OKAY(TAPITHIUM_MODS_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
