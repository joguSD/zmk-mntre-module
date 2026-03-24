/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2025 joguSD
*/
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

#include <reform/constants.h>
#include <reform/display.h>
#include <reform/matrix.h>

LOG_MODULE_DECLARE(mnt, CONFIG_MNT_LOG_LEVEL);

#define DISPLAY_THREAD_PRIORITY 5
#define DISPLAY_THREAD_STACK_SIZE 2048

static const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static struct display_buffer_descriptor desc = {
    .buf_size = BUFFER_SIZE,
    .width = DISPLAY_WIDTH,
    .height = DISPLAY_HEIGHT,
    .pitch = DISPLAY_WIDTH,
};
static uint8_t logo[BUFFER_SIZE];
static uint8_t display_buffer[BUFFER_SIZE];
static atomic_ptr_t render_callback;
static struct character_matrix logo_matrix;

K_SEM_DEFINE(render_sem, 0, 1);
K_THREAD_STACK_DEFINE(display_work_stack_area, DISPLAY_THREAD_STACK_SIZE);
static struct k_thread display_thread_data;

void buffer_clear() { memset(display_buffer, 0, BUFFER_SIZE); }

static void render_logo() {
  // MNT logo: 12x3 glyphs at font positions (5+y)*32+x, centered on rows 1-3
  matrix_clear(&logo_matrix);
  for (uint8_t y = 0; y < 3; y++) {
    for (uint8_t x = 0; x < 12; x++) {
      matrix_poke(&logo_matrix, x + 4, y + 1, (5 + y) * 32 + x);
    }
  }
  matrix_render(&logo_matrix, logo, -1);
}

void buffer_logo() { memcpy(display_buffer, logo, BUFFER_SIZE); }

int buffer_show() {
  const struct display_driver_api *api = display->api;
  return api->write(display, 0, 0, &desc, display_buffer);
}

static void render_logo_callback(uint8_t *buffer) { buffer_logo(); }

void display_request_render(display_render_callback_t requested_callback) {
  if (!requested_callback) {
    requested_callback = render_logo_callback;
  }

  atomic_ptr_set(&render_callback, (void *)requested_callback);
  k_sem_give(&render_sem);
}

static void reform_display_render_loop(void *p1, void *p2, void *p3) {
  if (!device_is_ready(display)) {
    LOG_ERR("Failed to find display device");
    return;
  }

  if (display_set_pixel_format(display, PIXEL_FORMAT_MONO01) != 0) {
    LOG_ERR("Failed to set required pixel format");
    return;
  }

  render_logo();
  buffer_clear();
  buffer_logo();
  buffer_show();
  display_blanking_off(display);

  LOG_INF("Reform Display Initialized!");

  while (true) {
    k_sem_take(&render_sem, K_FOREVER);
    display_render_callback_t callback =
        (display_render_callback_t)atomic_ptr_get(&render_callback);
    if (callback) {
      buffer_clear();
      callback(display_buffer);
      buffer_show();
    }
  }
}

static int reform_display_init(void) {
  k_thread_create(&display_thread_data, display_work_stack_area,
                  K_THREAD_STACK_SIZEOF(display_work_stack_area),
                  reform_display_render_loop, NULL, NULL, NULL,
                  DISPLAY_THREAD_PRIORITY, 0, K_NO_WAIT);

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
