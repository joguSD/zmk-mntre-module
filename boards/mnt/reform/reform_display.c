/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2025 joguSD
*/
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <stdio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

#define DISPLAY_THREAD_PRIORITY 5
#define DISPLAY_THREAD_STACK_SIZE 2045
#define DISPLAY_TICK_PERIOD_MS 100

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 32
#define BUFFER_SIZE DISPLAY_WIDTH * DISPLAY_HEIGHT / 8

static const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static struct display_buffer_descriptor desc = {
    .buf_size = BUFFER_SIZE,
    .width = DISPLAY_WIDTH,
    .height = DISPLAY_HEIGHT,
    .pitch = DISPLAY_WIDTH,
};
static uint8_t logo[BUFFER_SIZE] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,128,128,128,128,128,128,192, 64,128,  0,  0,  0,  0,  0,  0,  0,  0,  0,128,  0,  0,  0,  0,  0,  0,  0,  0,  0,128,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 14,249, 65, 65, 64, 96, 32, 32, 32, 32, 32, 32, 63,240,  0,  0,  0,  0,  0,  0,  0,  0,  1,  3,  6, 12,  0,  0,  0,  0,  0,192,127,  0,  0,  0,  8, 12,  6,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,128,112, 31,  8,  4,  4,  4,  4,  4,  6,  2,  2,  2,  2,253,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,129,193, 97, 61,  7,  1,  1,  1,  1,255,153,  1,  1,  1,  1,  1,129,129,192,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  2,  3,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  3,  2,  2,  6,  3,  0,  0,  0,  0,  0,  0,  0,  0,  2,  3,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};
static uint8_t display_buffer[BUFFER_SIZE];

void clear_buffer() {
    memset(display_buffer, 0, BUFFER_SIZE);
}

int show_buffer() {
    const struct display_driver_api *api = display->api;
    return api->write(display, 0, 0, &desc, display_buffer);
}

void display_tick_cb(struct k_work *work) {
    clear_buffer();
    memcpy(display_buffer, logo, BUFFER_SIZE);
    show_buffer();
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

    if (display_set_pixel_format(display, PIXEL_FORMAT_MONO01) != 0) {
        LOG_ERR("Failed to set required pixel format");
        return;
    }

    clear_buffer();
    memcpy(display_buffer, logo, BUFFER_SIZE);
    show_buffer();
    display_blanking_off(display);

    //k_timer_start(&display_timer, K_MSEC(DISPLAY_TICK_PERIOD_MS),
    //        K_MSEC(DISPLAY_TICK_PERIOD_MS));

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

int display_event_handler(const zmk_event_t *eh) {
    struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev == NULL) {
        return -ENOTSUP;
    }

    switch (ev->state) {
    case ZMK_ACTIVITY_ACTIVE:
        display_blanking_off(display);
        break;
    case ZMK_ACTIVITY_IDLE:
    case ZMK_ACTIVITY_SLEEP:
        display_blanking_on(display);
        break;
    default:
        LOG_WRN("Unhandled activity state: %d", ev->state);
        return -EINVAL;
    }
    return 0;
}

ZMK_LISTENER(reform_display, display_event_handler);
ZMK_SUBSCRIPTION(reform_display, zmk_activity_state_changed);
