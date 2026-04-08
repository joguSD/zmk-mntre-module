/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD
*/

#define DT_DRV_COMPAT zmk_input_processor_rgb_control

#include <drivers/input_processor.h>
#include <dt-bindings/zmk/rgb_control.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zmk/rgb_underglow.h>

struct rgb_control_config {
  uint8_t type;
  size_t codes_len;
  uint16_t codes[];
};

static int rgb_control_handle_event(const struct device *dev,
                                    struct input_event *event, uint32_t param1,
                                    uint32_t param2,
                                    struct zmk_input_processor_state *state) {
  const struct rgb_control_config *cfg = dev->config;

  if (event->type != cfg->type) {
    return ZMK_INPUT_PROC_CONTINUE;
  }

  for (int i = 0; i < cfg->codes_len; i++) {
    if (cfg->codes[i] == event->code) {
      int dir = event->value > 0 ? 1 : -1;

      switch (param1) {
      case RGB_CTRL_HUE:
        zmk_rgb_underglow_change_hue(dir);
        break;
      case RGB_CTRL_SAT:
        zmk_rgb_underglow_change_sat(dir);
        break;
      case RGB_CTRL_BRT:
        zmk_rgb_underglow_change_brt(dir);
        break;
      }

      return ZMK_INPUT_PROC_STOP;
    }
  }

  return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api rgb_control_driver_api = {
    .handle_event = rgb_control_handle_event,
};

#define RGB_CONTROL_INST(n)                                                    \
  static const struct rgb_control_config rgb_control_config_##n = {            \
      .type = DT_INST_PROP_OR(n, type, INPUT_EV_REL),                          \
      .codes_len = DT_INST_PROP_LEN(n, codes),                                 \
      .codes = DT_INST_PROP(n, codes),                                         \
  };                                                                           \
  DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, &rgb_control_config_##n,          \
                        POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,      \
                        &rgb_control_driver_api);

DT_INST_FOREACH_STATUS_OKAY(RGB_CONTROL_INST)
