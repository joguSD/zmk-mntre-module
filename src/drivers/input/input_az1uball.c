/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD
*/

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(az1uball, CONFIG_INPUT_LOG_LEVEL);

#define POLL_INTERVAL_MS 5

#define AZ1UBALL_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(palette_az1uball)

static const struct device *az1uball_dev = DEVICE_DT_GET(AZ1UBALL_NODE);

struct az1uball_data {
  struct k_timer poll_timer;
  struct k_work poll_work;
  bool button_pressed;
};

struct az1uball_config {
  struct i2c_dt_spec i2c;
  bool invert_x;
  bool invert_y;
};

static void az1uball_poll(struct k_work *work) {
  struct az1uball_data *data = az1uball_dev->data;
  const struct az1uball_config *cfg = az1uball_dev->config;
  uint8_t buf[5];
  int ret;

  ret = i2c_read_dt(&cfg->i2c, buf, sizeof(buf));
  if (ret < 0) {
    return;
  }

  int16_t x = (int16_t)buf[1] - (int16_t)buf[0];
  int16_t y = (int16_t)buf[3] - (int16_t)buf[2];

  if (cfg->invert_x) {
    x = -x;
  }
  if (cfg->invert_y) {
    y = -y;
  }

  if (x) {
    input_report_rel(az1uball_dev, INPUT_REL_X, x, !y, K_FOREVER);
  }
  if (y) {
    input_report_rel(az1uball_dev, INPUT_REL_Y, y, true, K_FOREVER);
  }

  bool pressed = (buf[4] & 0x80) != 0;

  if (pressed != data->button_pressed) {
    data->button_pressed = pressed;
    input_report_key(az1uball_dev, INPUT_BTN_0, pressed ? 1 : 0, true,
                     K_FOREVER);
  }
}

static void az1uball_timer_handler(struct k_timer *timer) {
  struct az1uball_data *data =
      CONTAINER_OF(timer, struct az1uball_data, poll_timer);

  k_work_submit(&data->poll_work);
}

static int az1uball_init(const struct device *dev) {
  struct az1uball_data *data = dev->data;
  const struct az1uball_config *cfg = dev->config;

  if (!i2c_is_ready_dt(&cfg->i2c)) {
    LOG_ERR("I2C bus not ready");
    return -ENODEV;
  }

  k_work_init(&data->poll_work, az1uball_poll);
  k_timer_init(&data->poll_timer, az1uball_timer_handler, NULL);
  k_timer_start(&data->poll_timer, K_MSEC(POLL_INTERVAL_MS),
                K_MSEC(POLL_INTERVAL_MS));

  LOG_INF("AZ1UBALL initialized (polling at %dms)", POLL_INTERVAL_MS);
  return 0;
}

static struct az1uball_data az1uball_data_0;
static const struct az1uball_config az1uball_config_0 = {
    .i2c = I2C_DT_SPEC_GET(AZ1UBALL_NODE),
    .invert_x = DT_PROP(AZ1UBALL_NODE, invert_x),
    .invert_y = DT_PROP(AZ1UBALL_NODE, invert_y),
};
DEVICE_DT_DEFINE(AZ1UBALL_NODE, az1uball_init, NULL, &az1uball_data_0,
                 &az1uball_config_0, POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY,
                 NULL);
