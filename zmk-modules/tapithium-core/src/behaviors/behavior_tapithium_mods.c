/*
 * Copyright (c) 2026 WiggelMc
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_tapithium_mods

// Dependencies
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/behavior.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

// Instance-specific Data struct (Optional)
struct behavior_tapithium_mods_data {
    bool data_param1;
    bool data_param2;
    bool data_param3;
};

// Instance-specific Config struct (Optional)
struct behavior_tapithium_mods_config {
    bool config_param1;
    bool config_param2;
    bool config_param3;
};

// Initialization Function (Optional)
static int tapithium_mods_init(const struct device *dev) {
    return 0;
};

static int on_tapithium_mods_binding_pressed(struct zmk_behavior_binding *binding,
                                                 struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_tapithium_mods_binding_released(struct zmk_behavior_binding *binding,
                                                  struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

// API struct
static const struct behavior_driver_api tapithium_mods_driver_api = {
    .binding_pressed = on_tapithium_mods_binding_pressed,
    .binding_released = on_tapithium_mods_binding_released,
};

BEHAVIOR_DT_INST_DEFINE(0,                                                // Instance Number (0)
                        tapithium_mods_init,                          // Initialization Function
                        NULL,                                             // Power Management Device Pointer
                        &tapithium_mods_data,                         // Behavior Data Pointer
                        &tapithium_mods_config,                       // Behavior Configuration Pointer
                        POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT  // Initialization Level, Device Priority
                        &tapithium_mods_driver_api);                  // API struct

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
