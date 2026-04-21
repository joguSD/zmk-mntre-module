/*
 * Copyright (c) 2025 George Norton
 *
 * SPDX-License-Identifier: MIT
 *
 * Based on https://github.com/george-norton/zmk-driver-rp2040-sleep
 * Ideally we'd import their module but it uses CONFIG_INPUT_INIT_PRIORITY in
 * the definition below, but the input subsystem is only enabled in the next
 * builds for the trackpad.
 */

#define DT_DRV_COMPAT raspberrypi_rp2040_sleep

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/device.h>

#define PPB_BASE    0xe0000000
#define SCR         0xed10

#define CLOCKS_BASE 0x40008000
#define SLEEP_EN0   0xa8
#define SLEEP_EN1   0xac
#define WAKE_EN0  0xa0
#define WAKE_EN1  0xa4

LOG_MODULE_REGISTER(rp2040_sleep, CONFIG_ZMK_LOG_LEVEL);

struct rp2040_sleep_config {
    uint32_t sleep_en0;
    uint32_t sleep_en1;
    uint32_t wake_en0;
    uint32_t wake_en1;
    bool deep_sleep;
};

static int rp2040_sleep_init(const struct device *dev) {
    const struct rp2040_sleep_config *config = dev->config;

    LOG_INF("SCR: %08x, SLEEP_EN0: %08x, SLEEP_EN1: %08x, WAKE_EN0: %08x, WAKE_EN1: %08x",
        *((uint32_t*)(PPB_BASE + SCR)),
        *((uint32_t*)(CLOCKS_BASE + SLEEP_EN0)), *((uint32_t*)(CLOCKS_BASE + SLEEP_EN1)),
        *((uint32_t*)(CLOCKS_BASE + WAKE_EN0)), *((uint32_t*)(CLOCKS_BASE + WAKE_EN1)));

    if (config->deep_sleep) {
        uint32_t scr = *((uint32_t*)(PPB_BASE + SCR));
        *((uint32_t*)(PPB_BASE+SCR)) = (scr | 0x4); // Deep sleep
    }

    *((uint32_t*)(CLOCKS_BASE + SLEEP_EN0)) = config->sleep_en0;
    *((uint32_t*)(CLOCKS_BASE + SLEEP_EN1)) = config->sleep_en1;
    *((uint32_t*)(CLOCKS_BASE + WAKE_EN0)) = config->wake_en0;
    *((uint32_t*)(CLOCKS_BASE + WAKE_EN1)) = config->wake_en1;

    LOG_INF("SCR: %08x, SLEEP_EN0: %08x, SLEEP_EN1: %08x, WAKE_EN0: %08x, WAKE_EN1: %08x",
        *((uint32_t*)(PPB_BASE + SCR)),
        *((uint32_t*)(CLOCKS_BASE + SLEEP_EN0)), *((uint32_t*)(CLOCKS_BASE + SLEEP_EN1)),
        *((uint32_t*)(CLOCKS_BASE + WAKE_EN0)), *((uint32_t*)(CLOCKS_BASE + WAKE_EN1)));

    return 0;
}

#define RP2040_SLEEP_DEFINE(n)                                                          \
    static const struct rp2040_sleep_config config##n = {                               \
        .sleep_en0 = DT_PROP(DT_DRV_INST(n), sleep_en0),                                \
        .sleep_en1 = DT_PROP(DT_DRV_INST(n), sleep_en1),                                \
        .wake_en0 = DT_PROP(DT_DRV_INST(n), wake_en0),                                  \
        .wake_en1 = DT_PROP(DT_DRV_INST(n), wake_en1),                                  \
        .deep_sleep = DT_PROP(DT_DRV_INST(n), deep_sleep),                              \
    };                                                                                  \
    DEVICE_DT_INST_DEFINE(n, rp2040_sleep_init, NULL, NULL, &config##n, POST_KERNEL,    \
        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, NULL);

DT_INST_FOREACH_STATUS_OKAY(RP2040_SLEEP_DEFINE)
