/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD
*/

#define DT_DRV_COMPAT zmk_input_processor_quadratic_scaler

#include <drivers/input_processor.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct quad_scaler_config {
  uint8_t type;
  size_t codes_len;
  uint16_t codes[];
};

static int quad_scale_val(struct input_event *event, uint32_t scale,
                          struct zmk_input_processor_state *state) {
  int16_t val = event->value;
  int16_t sign = (val > 0) - (val < 0);
  int16_t abs = val * sign;
  int16_t scaled = sign * (int16_t)(scale * abs * abs);

  if (state && state->remainder) {
    scaled += *state->remainder;
    if (scaled > 127) {
      *state->remainder = scaled - 127;
      scaled = 127;
    } else if (scaled < -127) {
      *state->remainder = scaled + 127;
      scaled = -127;
    } else {
      *state->remainder = 0;
    }
  }

  event->value = scaled;
  return ZMK_INPUT_PROC_CONTINUE;
}

static int quad_scaler_handle_event(const struct device *dev,
                                    struct input_event *event, uint32_t param1,
                                    uint32_t param2,
                                    struct zmk_input_processor_state *state) {
  const struct quad_scaler_config *cfg = dev->config;

  if (event->type != cfg->type) {
    return ZMK_INPUT_PROC_CONTINUE;
  }

  for (int i = 0; i < cfg->codes_len; i++) {
    if (cfg->codes[i] == event->code) {
      return quad_scale_val(event, param1, state);
    }
  }

  return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api quad_scaler_driver_api = {
    .handle_event = quad_scaler_handle_event,
};

#define QUAD_SCALER_INST(n)                                                    \
  static const struct quad_scaler_config quad_scaler_config_##n = {            \
      .type = DT_INST_PROP_OR(n, type, INPUT_EV_REL),                          \
      .codes_len = DT_INST_PROP_LEN(n, codes),                                 \
      .codes = DT_INST_PROP(n, codes),                                         \
  };                                                                           \
  DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, &quad_scaler_config_##n,          \
                        POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,      \
                        &quad_scaler_driver_api);

DT_INST_FOREACH_STATUS_OKAY(QUAD_SCALER_INST)
