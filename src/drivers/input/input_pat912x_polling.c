/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 joguSD
 *
 * Trackball polling driver for Pocket Reform.
 * The Pocket Reform lacks a motion interrupt pin, so we poll at 5ms (200Hz)
 * matching the original firmware.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(trackball, CONFIG_INPUT_LOG_LEVEL);

#define PAT912X_MOTION_STATUS  0x02
#define PAT912X_DELTA_X_LO    0x03
#define PAT912X_DELTA_XY_HI   0x12
#define PAT912X_RES_X         0x0d
#define PAT912X_RES_Y         0x0e
#define PAT912X_WRITE_PROTECT 0x09
#define PAT912X_CONFIGURATION 0x06
#define MOTION_STATUS_MOTION   BIT(7)
#define WRITE_PROTECT_DISABLE  0x5a
#define CONFIGURATION_RESET    0x97
#define CONFIGURATION_CLEAR    0x17

#define POLL_INTERVAL_MS 5

#define TRACKBALL_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(mnt_trackball)

static const struct device *trackball_dev = DEVICE_DT_GET(TRACKBALL_NODE);

struct trackball_data {
    struct k_timer poll_timer;
    struct k_work poll_work;
};

struct trackball_config {
    struct i2c_dt_spec i2c;
    bool invert_x;
    bool invert_y;
    uint8_t res_x;
    uint8_t res_y;
};

static void trackball_poll(struct k_work *work)
{
    const struct trackball_config *cfg = trackball_dev->config;
    uint8_t status;
    int ret;

    ret = i2c_reg_read_byte_dt(&cfg->i2c, PAT912X_MOTION_STATUS, &status);
    if (ret < 0 || !(status & MOTION_STATUS_MOTION)) {
        return;
    }

    uint8_t xy[2];

    ret = i2c_burst_read_dt(&cfg->i2c, PAT912X_DELTA_X_LO, xy, 2);
    if (ret < 0) {
        return;
    }

    uint8_t hi;

    ret = i2c_reg_read_byte_dt(&cfg->i2c, PAT912X_DELTA_XY_HI, &hi);
    if (ret < 0) {
        return;
    }

    int16_t x = sign_extend(((hi >> 4) << 8) | xy[0], 11);
    int16_t y = sign_extend((hi & 0x0f) << 8 | xy[1], 11);

    if (cfg->invert_x) {
        x = -x;
    }
    if (cfg->invert_y) {
        y = -y;
    }

    if (x) {
        input_report_rel(trackball_dev, INPUT_REL_X, x, !y, K_FOREVER);
    }
    if (y) {
        input_report_rel(trackball_dev, INPUT_REL_Y, y, true, K_FOREVER);
    }
}

static void trackball_timer_handler(struct k_timer *timer)
{
    struct trackball_data *data = CONTAINER_OF(timer, struct trackball_data, poll_timer);

    k_work_submit(&data->poll_work);
}

static int trackball_init(const struct device *dev)
{
    struct trackball_data *data = dev->data;
    const struct trackball_config *cfg = dev->config;
    int ret;

    if (!i2c_is_ready_dt(&cfg->i2c)) {
        LOG_ERR("I2C bus not ready");
        return -ENODEV;
    }

    /* Reset */
    ret = i2c_reg_write_byte_dt(&cfg->i2c, PAT912X_CONFIGURATION, CONFIGURATION_RESET);
    if (ret < 0) {
        return ret;
    }
    k_msleep(2);
    ret = i2c_reg_write_byte_dt(&cfg->i2c, PAT912X_CONFIGURATION, CONFIGURATION_CLEAR);
    if (ret < 0) {
        return ret;
    }

    /* Set CPI resolution */
    ret = i2c_reg_write_byte_dt(&cfg->i2c, PAT912X_WRITE_PROTECT, WRITE_PROTECT_DISABLE);
    ret |= i2c_reg_write_byte_dt(&cfg->i2c, PAT912X_RES_X, cfg->res_x);
    ret |= i2c_reg_write_byte_dt(&cfg->i2c, PAT912X_RES_Y, cfg->res_y);
    ret |= i2c_reg_write_byte_dt(&cfg->i2c, PAT912X_WRITE_PROTECT, 0x00);
    if (ret < 0) {
        return ret;
    }

    k_work_init(&data->poll_work, trackball_poll);
    k_timer_init(&data->poll_timer, trackball_timer_handler, NULL);
    k_timer_start(&data->poll_timer, K_MSEC(POLL_INTERVAL_MS), K_MSEC(POLL_INTERVAL_MS));

    LOG_INF("MNT trackball initialized (polling at %dms)", POLL_INTERVAL_MS);
    return 0;
}

static struct trackball_data trackball_data_0;
static const struct trackball_config trackball_config_0 = {
    .i2c = I2C_DT_SPEC_GET(TRACKBALL_NODE),
    .invert_x = DT_PROP(TRACKBALL_NODE, invert_x),
    .invert_y = DT_PROP(TRACKBALL_NODE, invert_y),
    .res_x = DT_PROP(TRACKBALL_NODE, res_x_cpi) / 5,
    .res_y = DT_PROP(TRACKBALL_NODE, res_y_cpi) / 5,
};
DEVICE_DT_DEFINE(TRACKBALL_NODE, trackball_init, NULL, &trackball_data_0,
                 &trackball_config_0, POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY, NULL);
