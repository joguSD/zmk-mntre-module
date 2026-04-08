/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD
*/

#define DT_DRV_COMPAT zmk_input_processor_inc_dec

#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <drivers/input_processor.h>

#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/keymap.h>
#include <zmk/virtual_key_position.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct ip_inc_dec_config {
    uint8_t index;
    uint8_t type;
    uint8_t tap_ms;
    size_t codes_len;
    const uint16_t *codes;
    const struct zmk_behavior_binding *bindings;
};

static int ip_inc_dec_handle_event(const struct device *dev, struct input_event *event,
                                   uint32_t param1, uint32_t param2,
                                   struct zmk_input_processor_state *state)
{
    const struct ip_inc_dec_config *cfg = dev->config;

    if (event->type != cfg->type) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    for (size_t i = 0; i < cfg->codes_len; i++) {
        if (cfg->codes[i] != event->code) {
            continue;
        }

        if (event->value == 0) {
            return ZMK_INPUT_PROC_STOP;
        }

        const struct zmk_behavior_binding *binding =
            event->value > 0 ? &cfg->bindings[0] : &cfg->bindings[1];

        struct zmk_behavior_binding_event behavior_event = {
            .position = ZMK_VIRTUAL_KEY_POSITION_BEHAVIOR_INPUT_PROCESSOR(
                state->input_device_index, cfg->index),
            .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
            .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
        };

        int taps = event->value > 0 ? event->value : -event->value;
        // Cap taps to prevent high-magnitude inputs from flooding the behavior queue
        if (taps > 5) {
            taps = 5;
        }

        for (int t = 0; t < taps; t++) {
            zmk_behavior_queue_add(&behavior_event, *binding, true, cfg->tap_ms);
            zmk_behavior_queue_add(&behavior_event, *binding, false, 0);
        }

        return ZMK_INPUT_PROC_STOP;
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api ip_inc_dec_driver_api = {
    .handle_event = ip_inc_dec_handle_event,
};

#define IP_INC_DEC_INST(n)                                                                         \
    static const uint16_t ip_inc_dec_codes_##n[] = DT_INST_PROP(n, codes);                         \
    static const struct zmk_behavior_binding ip_inc_dec_bindings_##n[] = {                          \
        LISTIFY(DT_INST_PROP_LEN(n, bindings), ZMK_KEYMAP_EXTRACT_BINDING, (, ),                   \
                DT_DRV_INST(n))};                                                                  \
    BUILD_ASSERT(ARRAY_SIZE(ip_inc_dec_bindings_##n) == 2,                                         \
                 "bindings must have exactly 2 entries (inc, dec)");                                \
    static const struct ip_inc_dec_config ip_inc_dec_config_##n = {                                \
        .index = n,                                                                                \
        .type = DT_INST_PROP_OR(n, type, INPUT_EV_REL),                                            \
        .tap_ms = DT_INST_PROP(n, tap_ms),                                                         \
        .codes_len = DT_INST_PROP_LEN(n, codes),                                                   \
        .codes = ip_inc_dec_codes_##n,                                                             \
        .bindings = ip_inc_dec_bindings_##n,                                                       \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, &ip_inc_dec_config_##n, POST_KERNEL,                \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &ip_inc_dec_driver_api);

DT_INST_FOREACH_STATUS_OKAY(IP_INC_DEC_INST)
