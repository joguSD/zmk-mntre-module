/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD

  Rate-based acceleration input processor.

  Tracks time between events per axis and applies a polynomial curve
  to scale the output based on input speed. Faster movement = larger output.

  Uses integer math only (no floating point).

  The curve:
    speed = 1000 / delta_ms  (ticks per second)
    output = value * (base + (speed^exponent / divisor))

  DTS params:
    param1 = divisor (higher = less acceleration)
*/

#define DT_DRV_COMPAT zmk_input_processor_rate_accel

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <drivers/input_processor.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define MAX_CODES 2

struct rate_accel_config {
    uint8_t type;
    size_t codes_len;
    uint16_t codes[];
};

struct rate_accel_data {
    int64_t last_time[MAX_CODES];
};

static int rate_accel_handle_event(const struct device *dev, struct input_event *event,
                                   uint32_t param1, uint32_t param2,
                                   struct zmk_input_processor_state *state)
{
    const struct rate_accel_config *cfg = dev->config;
    struct rate_accel_data *data = dev->data;

    if (event->type != cfg->type) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    uint32_t divisor = param1 > 0 ? param1 : 100;

    for (size_t i = 0; i < cfg->codes_len && i < MAX_CODES; i++) {
        if (cfg->codes[i] != event->code) {
            continue;
        }

        int64_t now = k_uptime_get();
        int64_t delta_ms = now - data->last_time[i];
        data->last_time[i] = now;

        if (delta_ms <= 0 || delta_ms > 500) {
            // First event or idle too long — no acceleration
            return ZMK_INPUT_PROC_CONTINUE;
        }

        // speed = ticks per second (higher = faster movement)
        int32_t speed = 1000 / (int32_t)delta_ms;

        // scale = 1 + speed^2 / divisor (integer approximation of polynomial curve)
        int32_t accel = (speed * speed) / (int32_t)divisor;
        int32_t scale = 1 + accel;

        int16_t val = event->value;
        int16_t sign = (val > 0) - (val < 0);
        int32_t scaled = (int32_t)val * scale;

        // Clamp to int16 range
        if (scaled > 127) {
            scaled = 127;
        } else if (scaled < -127) {
            scaled = -127;
        }

        event->value = (int16_t)scaled;
        return ZMK_INPUT_PROC_CONTINUE;
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api rate_accel_driver_api = {
    .handle_event = rate_accel_handle_event,
};

#define RATE_ACCEL_INST(n)                                                                         \
    static struct rate_accel_data rate_accel_data_##n = {0};                                        \
    static const struct rate_accel_config rate_accel_config_##n = {                                 \
        .type = DT_INST_PROP_OR(n, type, INPUT_EV_REL),                                            \
        .codes_len = DT_INST_PROP_LEN(n, codes),                                                   \
        .codes = DT_INST_PROP(n, codes),                                                           \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &rate_accel_data_##n, &rate_accel_config_##n,             \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                        \
                          &rate_accel_driver_api);

DT_INST_FOREACH_STATUS_OKAY(RATE_ACCEL_INST)
