/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD
*/
#define DT_DRV_COMPAT zmk_behavior_reform_sysctrl

// Dependencies
#include <string.h>

#include <drivers/behavior.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>

#include <reform/sysctrl.h>
#include <dt-bindings/zmk/reform_sysctrl.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

// Initialization Function (Optional)
static int reform_sysctrl_init(const struct device *dev) { return 0; };

static int
on_reform_sysctrl_binding_pressed(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event) {
  switch (binding->param1) {
  case RSC_POWER_CMD:
    if (binding->param2) {
      reform_sysctrl_power_on();
    } else {
      reform_sysctrl_power_off();
    }
    break;
  case RSC_STATUS_CMD:
    reform_sysctrl_status();
    break;
  case RSC_CHARGE_CMD:
    reform_sysctrl_battery();
    break;
  case RSC_WAKE_CMD:
    reform_sysctrl_wake();
    break;
  }
  return ZMK_BEHAVIOR_OPAQUE;
}

static int
on_reform_sysctrl_binding_released(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event) {
  return ZMK_BEHAVIOR_OPAQUE;
}

// API struct
static const struct behavior_driver_api reform_sysctrl_driver_api = {
    .binding_pressed = on_reform_sysctrl_binding_pressed,
    .binding_released = on_reform_sysctrl_binding_released,
};

BEHAVIOR_DT_INST_DEFINE(
    0,                   // Instance Number (0)
    reform_sysctrl_init, // Initialization Function
    NULL,                // Power Management Device Pointer
    NULL,                // Behavior Data Pointer
    NULL,                // Behavior Configuration Pointer
    POST_KERNEL,
    CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, // Initialization Level, Device
                                         // Priority
    &reform_sysctrl_driver_api);         // API struct

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
