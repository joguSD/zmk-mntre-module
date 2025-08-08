/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2025 joguSD
*/
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include <zephyr/device.h>
#include <zephyr/display/cfb.h>
#include <stdio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define DISPLAY_THREAD_PRIORITY 5
#define DISPLAY_THREAD_STACK_SIZE 2045
#define DISPLAY_TICK_PERIOD_MS 100

static const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

void display_tick_cb(struct k_work *work) {
    cfb_framebuffer_clear(display, false);

    int32_t uptime = k_uptime_get();
    char uptime_str[16];
    sprintf(uptime_str, "%d", uptime);

    cfb_print(display, "MNT REFORM", 0, 0);
    cfb_print(display, uptime_str, 0, 16);

    cfb_framebuffer_finalize(display);
}

K_WORK_DEFINE(display_tick_work, display_tick_cb);

K_THREAD_STACK_DEFINE(display_work_stack_area, DISPLAY_THREAD_STACK_SIZE);

static struct k_work_q display_work_q;

void display_timer_cb() {
    k_work_submit_to_queue(&display_work_q, &display_tick_work);
}

K_TIMER_DEFINE(display_timer, display_timer_cb, NULL);

void initialize_display(struct k_work *work) {
    if (!device_is_ready(display)) {
        LOG_ERR("Failed to find display device");
        return;
    }

    if (display_set_pixel_format(display, PIXEL_FORMAT_MONO10) != 0) {
        if (display_set_pixel_format(display, PIXEL_FORMAT_MONO01) != 0) {
            LOG_ERR("Failed to set required pixel format");
            return;
        }
    }

    if (cfb_framebuffer_init(display)) {
        LOG_ERR("Framebuffer initialization failed!");
        return;
    }

    cfb_framebuffer_clear(display, true);

    display_blanking_off(display);

    k_timer_start(&display_timer, K_MSEC(DISPLAY_TICK_PERIOD_MS),
            K_MSEC(DISPLAY_TICK_PERIOD_MS));

    LOG_INF("Reform Display Initialized!");
}

K_WORK_DEFINE(init_work, initialize_display);

static int reform_display_init(void) {
    k_work_queue_start(&display_work_q, display_work_stack_area,
            K_THREAD_STACK_SIZEOF(display_work_stack_area),
            DISPLAY_THREAD_PRIORITY, NULL);

    k_work_submit_to_queue(&display_work_q, &init_work);

    LOG_INF("Reform Display Thread Initialized!");
    return 0;
}

SYS_INIT(reform_display_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
