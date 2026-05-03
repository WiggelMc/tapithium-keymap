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

#define PR_HOLD_QUEUE_SIZE CONFIG_ZMK_BEHAVIOR_POSITION_REPEAT_MAX_BINDINGS_HELD

struct behavior_position_repeat_config {};

struct behavior_position_repeat_engine_data {};

static struct behavior_position_repeat_engine_data pr_data = {

};
static int pr_fields[PR_HOLD_QUEUE_SIZE] = {};
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

#define POSITION_REPEAT_INST(n)                                                \
  static struct behavior_position_repeat_config position_repeat_config_##n =   \
      {};                                                                      \
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
