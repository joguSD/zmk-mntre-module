/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD

  IQS9150 trackpad input driver for Zephyr.
*/

#define DT_DRV_COMPAT azoteq_iqs9150

#include <dt-bindings/input/iqs9150-gesture-codes.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(iqs9150, CONFIG_INPUT_LOG_LEVEL);

#define IQS9150_ADDR_DATA 0x1010
#define IQS9150_ADDR_SYS_CTRL 0x11BC

#define IQS9150_BIT_ACK_RESET 7
#define IQS9150_BIT_TP_RE_ATI 5

/* Data offsets from 0x1010 */
#define IQS9150_OFF_SYS_STATUS 0
#define IQS9150_OFF_REL_X 4
#define IQS9150_OFF_REL_Y 6
#define IQS9150_OFF_GESTURE_X 8
#define IQS9150_OFF_GESTURE_Y 10
#define IQS9150_OFF_1F_GESTURE 12
#define IQS9150_OFF_2F_GESTURE 14
#define IQS9150_DATA_LEN 16

#define IQS9150_SYS_SNAP BIT(3)

/* Gesture bits */
#define IQS9150_1F_TAP_MASK (BIT(0) | BIT(1) | BIT(2))
#define IQS9150_1F_PALM BIT(5)
#define IQS9150_2F_TAP_MASK (BIT(0) | BIT(1) | BIT(2))
#define IQS9150_2F_VERTICAL_SCROLL BIT(6)
#define IQS9150_2F_HORIZONTAL_SCROLL BIT(7)

/* Single-finger swipe bits (high byte of 0x101C) */
#define IQS9150_1F_SWIPE_X_POS BIT(0)
#define IQS9150_1F_SWIPE_X_NEG BIT(1)
#define IQS9150_1F_SWIPE_Y_POS BIT(2)
#define IQS9150_1F_SWIPE_Y_NEG BIT(3)

/* Two-finger swipe bits (high byte of 0x101E) */
#define IQS9150_2F_SWIPE_X_POS BIT(0)
#define IQS9150_2F_SWIPE_X_NEG BIT(1)
#define IQS9150_2F_SWIPE_Y_POS BIT(2)
#define IQS9150_2F_SWIPE_Y_NEG BIT(3)
#define IQS9150_2F_ZOOM_IN BIT(4)
#define IQS9150_2F_ZOOM_OUT BIT(5)

/* Trackpad settings byte 0 bits */
#define IQS9150_TP_FLIP_X BIT(0)
#define IQS9150_TP_FLIP_Y BIT(1)
#define IQS9150_TP_SWAP_XY BIT(2)
#define IQS9150_TP_IIR_FILTER BIT(3)
#define IQS9150_TP_JITTER BIT(5)

/* Gesture enable bits */
#define IQS9150_GE_SINGLE_TAP BIT(0)
#define IQS9150_GE_PRESS_HOLD BIT(3)
#define IQS9150_GE_SWIPES 0xFF00
#define IQS9150_GE_2F_TAP BIT(0)
#define IQS9150_GE_2F_SCROLL (BIT(6) | BIT(7))

struct iqs9150_data {
  const struct device *dev;
  struct k_work work;
  struct gpio_callback rdy_cb;
  enum {
    IQS9150_RESET,
    IQS9150_INIT,
    IQS9150_ATI,
    IQS9150_RUNNING,
  } state;
};

struct iqs9150_config {
  struct i2c_dt_spec i2c;
  struct gpio_dt_spec rdy_gpio;

  /* Trackpad geometry */
  uint8_t total_rx;
  uint8_t total_tx;
  uint16_t x_resolution;
  uint16_t y_resolution;
  uint8_t x_trim;
  uint8_t y_trim;
  bool invert_x;
  bool invert_y;
  bool swap_xy;
  uint8_t max_fingers;

  /* Electrode mapping */
  uint8_t rx_tx_mapping[46];
  uint8_t rx_tx_mapping_len;

  /* ATI */
  uint8_t alp_ati_compensation[26];
  uint8_t alp_ati_compensation_len;
  uint8_t ati_multipliers[28];
  uint8_t ati_multipliers_len;
  uint16_t ati_target;
  uint16_t alp_ati_target;
  uint16_t alp_base_target;
  uint16_t tp_neg_delta_reati;
  uint16_t tp_pos_delta_reati;
  uint8_t tp_ref_drift_limit;
  uint8_t alp_lta_drift_limit;

  /* Sensitivity */
  uint8_t touch_set_threshold;
  uint8_t touch_clear_threshold;
  uint8_t alp_threshold;
  uint8_t alp_autoprox_threshold;
  uint8_t alp_set_debounce;
  uint8_t alp_clear_debounce;

  /* Timing */
  uint16_t active_sample_ms;
  uint16_t idle_touch_sample_ms;
  uint16_t idle_sample_ms;
  uint16_t lp1_sample_ms;
  uint16_t lp2_sample_ms;
  uint16_t stationary_touch_timeout_s;
  uint16_t idle_touch_timeout_s;
  uint16_t idle_timeout_s;
  uint16_t lp1_timeout_s;
  uint16_t active_timeout_ms;
  uint8_t reati_retry_time_s;
  uint8_t ref_update_time_s;
  uint16_t i2c_timeout_ms;

  /* ALP betas */
  uint8_t alp_count_beta_lp1;
  uint8_t alp_lta_beta_lp1;
  uint8_t alp_count_beta_lp2;
  uint8_t alp_lta_beta_lp2;

  /* Trackpad filter settings */
  uint16_t dynamic_filter_bottom_speed;
  uint16_t dynamic_filter_top_speed;
  uint8_t dynamic_filter_bottom_beta;
  uint8_t static_filter_beta;
  uint8_t stationary_touch_threshold;
  uint8_t finger_split_factor;
  uint8_t jitter_filter_delta;
  uint8_t finger_confidence_threshold;

  /* Gestures */
  bool tap_enable;
  bool double_tap_enable;
  bool triple_tap_enable;
  bool press_hold_enable;
  bool two_finger_tap_enable;
  bool scroll_enable;
  bool swipe_enable;
  bool zoom_enable;
  bool palm_reject_enable;
  uint16_t tap_time_ms;
  uint16_t tap_air_time_ms;
  uint16_t tap_distance;
  uint16_t scroll_initial_distance;
  uint16_t scroll_consecutive_distance;
  uint16_t hold_time_ms;
  uint16_t swipe_time_ms;
  uint16_t swipe_x_distance;
  uint16_t swipe_y_distance;
  uint8_t swipe_angle;
  uint8_t scroll_angle;
  uint16_t swipe_x_consecutive_distance;
  uint16_t swipe_y_consecutive_distance;
  uint16_t zoom_initial_distance;
  uint16_t zoom_consecutive_distance;
  uint16_t palm_gesture_threshold;

  /* System registers */
  uint8_t config_settings[2];
  uint8_t config_settings_len;
  uint8_t other_settings[2];
  uint8_t other_settings_len;

  /* Hardware blobs */
  uint8_t alp_setup[4];
  uint8_t alp_setup_len;
  uint8_t alp_tx_enable[6];
  uint8_t alp_tx_enable_len;
  uint8_t hardware_settings[10];
  uint8_t hardware_settings_len;
};

/* I2C helpers for 16-bit register addresses */
static int iqs9150_read(const struct i2c_dt_spec *i2c, uint16_t reg,
                        uint8_t *buf, uint8_t len) {
  uint8_t addr[2] = {reg & 0xFF, reg >> 8};
  return i2c_write_read_dt(i2c, addr, sizeof(addr), buf, len);
}

static int iqs9150_write(const struct i2c_dt_spec *i2c, uint16_t reg,
                         const uint8_t *data, uint8_t len) {
  uint8_t buf[32];
  if (len + 2 > sizeof(buf)) {
    return -ENOMEM;
  }
  buf[0] = reg & 0xFF;
  buf[1] = reg >> 8;
  memcpy(&buf[2], data, len);
  return i2c_write_dt(i2c, buf, len + 2);
}

/* Write a blob in 30-byte chunks (IQS9150 I2C max per transaction) */
static int iqs9150_write_chunked(const struct i2c_dt_spec *i2c, uint16_t reg,
                                 const uint8_t *data, uint8_t len) {
  while (len > 0) {
    uint8_t chunk = (len > 30) ? 30 : len;
    int ret = iqs9150_write(i2c, reg, data, chunk);
    if (ret < 0)
      return ret;
    reg += chunk;
    data += chunk;
    len -= chunk;
  }
  return 0;
}

static int iqs9150_write_config(const struct device *dev) {
  const struct iqs9150_config *cfg = dev->config;
  int ret;

  /* 0x115C: ALP ATI Compensation */
  ret = iqs9150_write(&cfg->i2c, 0x115C, cfg->alp_ati_compensation,
                      cfg->alp_ati_compensation_len);
  if (ret < 0)
    return ret;

  /* 0x117A: ATI Multipliers/Dividers */
  ret = iqs9150_write(&cfg->i2c, 0x117A, cfg->ati_multipliers,
                      cfg->ati_multipliers_len);
  if (ret < 0)
    return ret;

  /* 0x1196: ATI Settings (12 bytes) */
  uint8_t ati_settings[] = {
      cfg->ati_target & 0xFF,         cfg->ati_target >> 8,
      cfg->alp_ati_target & 0xFF,     cfg->alp_ati_target >> 8,
      cfg->alp_base_target & 0xFF,    cfg->alp_base_target >> 8,
      cfg->tp_neg_delta_reati & 0xFF, cfg->tp_neg_delta_reati >> 8,
      cfg->tp_pos_delta_reati & 0xFF, cfg->tp_pos_delta_reati >> 8,
      cfg->tp_ref_drift_limit,        cfg->alp_lta_drift_limit,
  };
  ret = iqs9150_write(&cfg->i2c, 0x1196, ati_settings, sizeof(ati_settings));
  if (ret < 0)
    return ret;

  /* 0x11A2: Sampling Periods and Timing (26 bytes) */
  uint8_t timing[] = {
      cfg->active_sample_ms & 0xFF,
      cfg->active_sample_ms >> 8,
      cfg->idle_touch_sample_ms & 0xFF,
      cfg->idle_touch_sample_ms >> 8,
      cfg->idle_sample_ms & 0xFF,
      cfg->idle_sample_ms >> 8,
      cfg->lp1_sample_ms & 0xFF,
      cfg->lp1_sample_ms >> 8,
      cfg->lp2_sample_ms & 0xFF,
      cfg->lp2_sample_ms >> 8,
      cfg->stationary_touch_timeout_s & 0xFF,
      cfg->stationary_touch_timeout_s >> 8,
      cfg->idle_touch_timeout_s & 0xFF,
      cfg->idle_touch_timeout_s >> 8,
      cfg->idle_timeout_s & 0xFF,
      cfg->idle_timeout_s >> 8,
      cfg->lp1_timeout_s & 0xFF,
      cfg->lp1_timeout_s >> 8,
      cfg->active_timeout_ms & 0xFF,
      cfg->active_timeout_ms >> 8,
      cfg->reati_retry_time_s,
      cfg->ref_update_time_s,
      cfg->i2c_timeout_ms & 0xFF,
      cfg->i2c_timeout_ms >> 8,
      20, /* snap_timeout_s -- not yet supported, using eval kit default */
      0x00,
  };
  ret = iqs9150_write(&cfg->i2c, 0x11A2, timing, sizeof(timing));
  if (ret < 0)
    return ret;

  /* 0x11BC: System Control + Config + Other (6 bytes) */
  uint8_t sys[6] = {0x00, 0x00};
  memcpy(&sys[2], cfg->config_settings, 2);
  memcpy(&sys[4], cfg->other_settings, 2);
  ret = iqs9150_write(&cfg->i2c, 0x11BC, sys, sizeof(sys));
  if (ret < 0)
    return ret;

  /* 0x11C2: ALP Setup + Tx Enable */
  ret = iqs9150_write(&cfg->i2c, 0x11C2, cfg->alp_setup, cfg->alp_setup_len);
  if (ret < 0)
    return ret;
  ret = iqs9150_write(&cfg->i2c, 0x11C6, cfg->alp_tx_enable,
                      cfg->alp_tx_enable_len);
  if (ret < 0)
    return ret;

  /* 0x11CC: Thresholds and Debounce (8 bytes) */
  uint8_t thresh[] = {
      cfg->touch_set_threshold, cfg->touch_clear_threshold,
      cfg->alp_threshold,       cfg->alp_autoprox_threshold,
      cfg->alp_set_debounce,    cfg->alp_clear_debounce,
      50, 50, /* snap set/clear thresholds -- not yet supported, using eval kit defaults */
  };
  ret = iqs9150_write(&cfg->i2c, 0x11CC, thresh, sizeof(thresh));
  if (ret < 0)
    return ret;

  /* 0x11D4: ALP Count/LTA Betas (4 bytes) */
  uint8_t betas[] = {
      cfg->alp_count_beta_lp1,
      cfg->alp_lta_beta_lp1,
      cfg->alp_count_beta_lp2,
      cfg->alp_lta_beta_lp2,
  };
  ret = iqs9150_write(&cfg->i2c, 0x11D4, betas, sizeof(betas));
  if (ret < 0)
    return ret;

  /* 0x11D8: Hardware Settings */
  ret = iqs9150_write(&cfg->i2c, 0x11D8, cfg->hardware_settings,
                      cfg->hardware_settings_len);
  if (ret < 0)
    return ret;

  /* 0x11E2: Trackpad Settings (20 bytes) */
  uint8_t tp_settings_0 = IQS9150_TP_IIR_FILTER | IQS9150_TP_JITTER;
  if (cfg->invert_x)
    tp_settings_0 |= IQS9150_TP_FLIP_X;
  if (cfg->invert_y)
    tp_settings_0 |= IQS9150_TP_FLIP_Y;
  if (cfg->swap_xy)
    tp_settings_0 |= IQS9150_TP_SWAP_XY;

  uint8_t tp[] = {
      tp_settings_0,
      (cfg->total_rx & 0xFF),
      (cfg->total_tx & 0xFF),
      cfg->max_fingers,
      cfg->x_resolution & 0xFF,
      cfg->x_resolution >> 8,
      cfg->y_resolution & 0xFF,
      cfg->y_resolution >> 8,
      cfg->dynamic_filter_bottom_speed & 0xFF,
      cfg->dynamic_filter_bottom_speed >> 8,
      cfg->dynamic_filter_top_speed & 0xFF,
      cfg->dynamic_filter_top_speed >> 8,
      cfg->dynamic_filter_bottom_beta,
      cfg->static_filter_beta,
      cfg->stationary_touch_threshold,
      cfg->finger_split_factor,
      cfg->x_trim,
      cfg->y_trim,
      cfg->jitter_filter_delta,
      cfg->finger_confidence_threshold,
  };
  ret = iqs9150_write(&cfg->i2c, 0x11E2, tp, sizeof(tp));
  if (ret < 0)
    return ret;

  /* 0x11F6: Gesture Settings (34 bytes, split 30+4) */
  uint8_t gesture_1f = 0;
  uint8_t gesture_1f_hi = 0;
  uint8_t gesture_2f = 0;
  uint8_t gesture_2f_hi = 0;

  if (cfg->tap_enable) {
    gesture_1f |= IQS9150_GE_SINGLE_TAP;
  }
  if (cfg->double_tap_enable || cfg->triple_tap_enable) {
    gesture_1f |= BIT(1);
    gesture_2f |= BIT(1);
  }
  if (cfg->triple_tap_enable) {
    gesture_1f |= BIT(2);
    gesture_2f |= BIT(2);
  }
  if (cfg->press_hold_enable) {
    gesture_1f |= IQS9150_GE_PRESS_HOLD;
  }
  if (cfg->two_finger_tap_enable) {
    gesture_2f |= IQS9150_GE_2F_TAP;
  }
  if (cfg->palm_reject_enable) {
    gesture_1f |= BIT(4);
  }
  if (cfg->scroll_enable) {
    gesture_2f |= IQS9150_GE_2F_SCROLL;
  }
  if (cfg->swipe_enable) {
    gesture_1f_hi = 0x0F; /* 1F swipes: X+, X-, Y+, Y- */
    gesture_2f_hi = 0x0F; /* 2F swipes: X+, X-, Y+, Y- */
  }
  if (cfg->zoom_enable) {
    gesture_2f_hi |= 0x30; /* 2F zoom in + zoom out */
  }

  uint8_t gestures[30] = {
      gesture_1f,
      gesture_1f_hi,
      gesture_2f,
      gesture_2f_hi,
      cfg->tap_time_ms & 0xFF,
      cfg->tap_time_ms >> 8,
      cfg->tap_air_time_ms & 0xFF,
      cfg->tap_air_time_ms >> 8,
      cfg->tap_distance & 0xFF,
      cfg->tap_distance >> 8,
      cfg->hold_time_ms & 0xFF,
      cfg->hold_time_ms >> 8,
      cfg->swipe_time_ms & 0xFF,
      cfg->swipe_time_ms >> 8,
      cfg->swipe_x_distance & 0xFF,
      cfg->swipe_x_distance >> 8,
      cfg->swipe_y_distance & 0xFF,
      cfg->swipe_y_distance >> 8,
      cfg->swipe_x_consecutive_distance & 0xFF,
      cfg->swipe_x_consecutive_distance >> 8,
      cfg->swipe_y_consecutive_distance & 0xFF,
      cfg->swipe_y_consecutive_distance >> 8,
      cfg->swipe_angle,
      cfg->scroll_angle,
      cfg->zoom_initial_distance & 0xFF,
      cfg->zoom_initial_distance >> 8,
      cfg->zoom_consecutive_distance & 0xFF,
      cfg->zoom_consecutive_distance >> 8,
      cfg->scroll_initial_distance & 0xFF,
      cfg->scroll_initial_distance >> 8,
  };
  ret = iqs9150_write(&cfg->i2c, 0x11F6, gestures, 30);
  if (ret < 0)
    return ret;

  uint8_t gestures_tail[] = {
      cfg->scroll_consecutive_distance & 0xFF,
      cfg->scroll_consecutive_distance >> 8,
      cfg->palm_gesture_threshold & 0xFF,
      cfg->palm_gesture_threshold >> 8,
  };
  ret = iqs9150_write(&cfg->i2c, 0x11F6 + 30, gestures_tail,
                      sizeof(gestures_tail));
  if (ret < 0)
    return ret;

  /* 0x1218: RxTx Mapping */
  ret = iqs9150_write_chunked(&cfg->i2c, 0x1218, cfg->rx_tx_mapping,
                              cfg->rx_tx_mapping_len);
  if (ret < 0)
    return ret;

  /* Not yet supported (chip boots with zeros, no write needed):
   * 0x1246: TP Channel Disables 0+1 (88 bytes) -- per-channel disable
   * 0x129E: TP Snap Enable 0 (44 bytes) -- per-channel snap dome enable
   * 0x12CA: TP Snap Enable 1 (44 bytes) -- per-channel snap dome enable
   * 0x12F6: Individual Touch Threshold Adjustments (10 bytes)
   * 0x14F0: Virtual Buttons 0 (34 bytes)
   * 0x1512: Virtual Buttons 1 (48 bytes)
   * 0x1542: Virtual Buttons 2 (48 bytes)
   * 0x1572: Virtual Sliders 0 (52 bytes)
   * 0x15A6: Virtual Sliders 1 (30 bytes)
   * 0x15C4: Virtual Wheels (40 bytes)
   */

  return 0;
}

static int iqs9150_ack_reset(const struct i2c_dt_spec *i2c) {
  uint8_t buf[2] = {BIT(IQS9150_BIT_ACK_RESET), 0x00};
  return iqs9150_write(i2c, IQS9150_ADDR_SYS_CTRL, buf, 2);
}

static int iqs9150_trigger_ati(const struct i2c_dt_spec *i2c) {
  uint8_t buf[2] = {BIT(IQS9150_BIT_TP_RE_ATI), 0x00};
  return iqs9150_write(i2c, IQS9150_ADDR_SYS_CTRL, buf, 2);
}

static void iqs9150_work_handler(struct k_work *work) {
  struct iqs9150_data *data = CONTAINER_OF(work, struct iqs9150_data, work);
  const struct iqs9150_config *cfg = data->dev->config;
  int ret;

  switch (data->state) {
  case IQS9150_RESET:
    LOG_INF("ACK reset");
    ret = iqs9150_ack_reset(&cfg->i2c);
    if (ret < 0) {
      LOG_ERR("ACK reset failed: %d", ret);
      return;
    }
    data->state = IQS9150_INIT;
    break;

  case IQS9150_INIT:
    LOG_INF("Writing config");
    ret = iqs9150_write_config(data->dev);
    if (ret < 0) {
      LOG_ERR("Config write failed: %d", ret);
      return;
    }
    data->state = IQS9150_ATI;
    break;

  case IQS9150_ATI:
    LOG_INF("Triggering ATI");
    ret = iqs9150_trigger_ati(&cfg->i2c);
    if (ret < 0) {
      LOG_ERR("ATI trigger failed: %d", ret);
      return;
    }
    data->state = IQS9150_RUNNING;
    LOG_INF("IQS9150 running");
    break;

  case IQS9150_RUNNING: {
    uint8_t buf[IQS9150_DATA_LEN];
    ret = iqs9150_read(&cfg->i2c, IQS9150_ADDR_DATA, buf, sizeof(buf));
    if (ret < 0) {
      LOG_ERR("Data read failed: %d", ret);
      return;
    }

    int16_t rel_x =
        (int16_t)(buf[IQS9150_OFF_REL_X] | (buf[IQS9150_OFF_REL_X + 1] << 8));
    int16_t rel_y =
        (int16_t)(buf[IQS9150_OFF_REL_Y] | (buf[IQS9150_OFF_REL_Y + 1] << 8));
    int16_t gest_x = (int16_t)(buf[IQS9150_OFF_GESTURE_X] |
                               (buf[IQS9150_OFF_GESTURE_X + 1] << 8));
    int16_t gest_y = (int16_t)(buf[IQS9150_OFF_GESTURE_Y] |
                               (buf[IQS9150_OFF_GESTURE_Y + 1] << 8));
    uint8_t gestures_1f = buf[IQS9150_OFF_1F_GESTURE];
    uint8_t gestures_1f_hi = buf[IQS9150_OFF_1F_GESTURE + 1];
    uint8_t gestures_2f = buf[IQS9150_OFF_2F_GESTURE];
    uint8_t gestures_2f_hi = buf[IQS9150_OFF_2F_GESTURE + 1];


    bool scrolling = gestures_2f & (IQS9150_2F_VERTICAL_SCROLL |
                                    IQS9150_2F_HORIZONTAL_SCROLL);

    LOG_DBG("rel:%d,%d gest:%d,%d 1f:%02x/%02x 2f:%02x/%02x", rel_x, rel_y,
            gest_x, gest_y, gestures_1f, gestures_1f_hi, gestures_2f,
            gestures_2f_hi);

    if (gestures_1f & IQS9150_1F_PALM) {
      LOG_DBG("palm detected, suppressing");
      break;
    }

    if (scrolling) {
      if (gest_x) {
        input_report_rel(data->dev, INPUT_REL_HWHEEL, gest_x, !gest_y,
                         K_FOREVER);
      }
      if (gest_y) {
        input_report_rel(data->dev, INPUT_REL_WHEEL, -gest_y, true, K_FOREVER);
      }
    } else if (gestures_1f & IQS9150_1F_TAP_MASK) {
      input_report_key(data->dev, INPUT_BTN_0, 1, true, K_FOREVER);
      input_report_key(data->dev, INPUT_BTN_0, 0, true, K_FOREVER);
    } else if (gestures_2f & IQS9150_2F_TAP_MASK) {
      input_report_key(data->dev, INPUT_BTN_1, 1, true, K_FOREVER);
      input_report_key(data->dev, INPUT_BTN_1, 0, true, K_FOREVER);
    } else if (rel_x || rel_y) {
      if (rel_x) {
        input_report_rel(data->dev, INPUT_REL_X, rel_x, !rel_y, K_FOREVER);
      }
      if (rel_y) {
        input_report_rel(data->dev, INPUT_REL_Y, rel_y, true, K_FOREVER);
      }
    }

    /* Swipe and zoom gestures (reported independently of above) */
    static const struct {
      uint8_t bit;
      uint16_t code;
    } swipe_1f_map[] =
        {
            {IQS9150_1F_SWIPE_X_POS, IQS9150_GESTURE_1F_SWIPE_X_POS},
            {IQS9150_1F_SWIPE_X_NEG, IQS9150_GESTURE_1F_SWIPE_X_NEG},
            {IQS9150_1F_SWIPE_Y_POS, IQS9150_GESTURE_1F_SWIPE_Y_POS},
            {IQS9150_1F_SWIPE_Y_NEG, IQS9150_GESTURE_1F_SWIPE_Y_NEG},
        },
      swipe_2f_map[] = {
          {IQS9150_2F_SWIPE_X_POS, IQS9150_GESTURE_2F_SWIPE_X_POS},
          {IQS9150_2F_SWIPE_X_NEG, IQS9150_GESTURE_2F_SWIPE_X_NEG},
          {IQS9150_2F_SWIPE_Y_POS, IQS9150_GESTURE_2F_SWIPE_Y_POS},
          {IQS9150_2F_SWIPE_Y_NEG, IQS9150_GESTURE_2F_SWIPE_Y_NEG},
      };

    for (int i = 0; i < ARRAY_SIZE(swipe_1f_map); i++) {
      if (gestures_1f_hi & swipe_1f_map[i].bit) {
        LOG_DBG("1f swipe: code=%02x", swipe_1f_map[i].code);
        input_report(data->dev, INPUT_EV_DEVICE, swipe_1f_map[i].code, 1, true,
                     K_FOREVER);
      }
    }
    for (int i = 0; i < ARRAY_SIZE(swipe_2f_map); i++) {
      if (gestures_2f_hi & swipe_2f_map[i].bit) {
        LOG_DBG("2f swipe: code=%02x", swipe_2f_map[i].code);
        input_report(data->dev, INPUT_EV_DEVICE, swipe_2f_map[i].code, 1, true,
                     K_FOREVER);
      }
    }

    /* Zoom: magnitude reported in Gesture X (positive = in, negative = out) */
    if ((gestures_2f_hi & (IQS9150_2F_ZOOM_IN | IQS9150_2F_ZOOM_OUT)) &&
        gest_x) {
      LOG_DBG("zoom: %s mag=%d", gest_x > 0 ? "in" : "out",
              gest_x > 0 ? gest_x : -gest_x);
      input_report(data->dev, INPUT_EV_DEVICE,
                   gest_x > 0 ? IQS9150_GESTURE_2F_ZOOM_IN
                              : IQS9150_GESTURE_2F_ZOOM_OUT,
                   gest_x > 0 ? gest_x : -gest_x, true, K_FOREVER);
    }
    break;
  }
  }
}

static void iqs9150_rdy_callback(const struct device *port,
                                 struct gpio_callback *cb,
                                 gpio_port_pins_t pins) {
  struct iqs9150_data *data = CONTAINER_OF(cb, struct iqs9150_data, rdy_cb);
  k_work_submit(&data->work);
}

static int iqs9150_init(const struct device *dev) {
  struct iqs9150_data *data = dev->data;
  const struct iqs9150_config *cfg = dev->config;

  data->dev = dev;
  data->state = IQS9150_RESET;

  if (!i2c_is_ready_dt(&cfg->i2c)) {
    LOG_ERR("I2C bus not ready");
    return -ENODEV;
  }

  k_work_init(&data->work, iqs9150_work_handler);

  if (!gpio_is_ready_dt(&cfg->rdy_gpio)) {
    LOG_ERR("RDY GPIO not ready");
    return -ENODEV;
  }

  int ret = gpio_pin_configure_dt(&cfg->rdy_gpio, GPIO_INPUT);
  if (ret < 0) {
    LOG_ERR("RDY pin configure failed: %d", ret);
    return ret;
  }

  gpio_init_callback(&data->rdy_cb, iqs9150_rdy_callback,
                     BIT(cfg->rdy_gpio.pin));

  ret = gpio_add_callback(cfg->rdy_gpio.port, &data->rdy_cb);
  if (ret < 0) {
    LOG_ERR("RDY callback add failed: %d", ret);
    return ret;
  }

  ret =
      gpio_pin_interrupt_configure_dt(&cfg->rdy_gpio, GPIO_INT_EDGE_TO_ACTIVE);
  if (ret < 0) {
    LOG_ERR("RDY interrupt configure failed: %d", ret);
    return ret;
  }

  LOG_INF("IQS9150 driver initialized, waiting for RDY");
  return 0;
}

#define IQS9150_INST(n)                                                        \
  BUILD_ASSERT(DT_INST_PROP_LEN(n, rx_tx_mapping) <= 46,                       \
               "rx-tx-mapping exceeds 46 bytes");                              \
  BUILD_ASSERT(DT_INST_PROP_LEN(n, alp_ati_compensation) <= 26,                \
               "alp-ati-compensation exceeds 26 bytes");                       \
  BUILD_ASSERT(DT_INST_PROP_LEN(n, ati_multipliers) <= 28,                     \
               "ati-multipliers exceeds 28 bytes");                            \
  BUILD_ASSERT(DT_INST_PROP_LEN(n, config_settings) <= 2,                      \
               "config-settings exceeds 2 bytes");                             \
  BUILD_ASSERT(DT_INST_PROP_LEN(n, other_settings) <= 2,                       \
               "other-settings exceeds 2 bytes");                              \
  BUILD_ASSERT(DT_INST_PROP_LEN(n, alp_setup) <= 4,                            \
               "alp-setup exceeds 4 bytes");                                   \
  BUILD_ASSERT(DT_INST_PROP_LEN(n, alp_tx_enable) <= 6,                        \
               "alp-tx-enable exceeds 6 bytes");                               \
  BUILD_ASSERT(DT_INST_PROP_LEN(n, hardware_settings) <= 10,                   \
               "hardware-settings exceeds 10 bytes");                          \
  static struct iqs9150_data iqs9150_data_##n;                                 \
  static const struct iqs9150_config iqs9150_config_##n = {                    \
      .i2c = I2C_DT_SPEC_INST_GET(n),                                          \
      .rdy_gpio = GPIO_DT_SPEC_INST_GET(n, rdy_gpios),                         \
      .total_rx = DT_INST_PROP(n, total_rx),                                   \
      .total_tx = DT_INST_PROP(n, total_tx),                                   \
      .x_resolution = DT_INST_PROP(n, x_resolution),                           \
      .y_resolution = DT_INST_PROP(n, y_resolution),                           \
      .x_trim = DT_INST_PROP(n, x_trim),                                       \
      .y_trim = DT_INST_PROP(n, y_trim),                                       \
      .invert_x = DT_INST_PROP(n, invert_x),                                   \
      .invert_y = DT_INST_PROP(n, invert_y),                                   \
      .swap_xy = DT_INST_PROP(n, swap_xy),                                     \
      .max_fingers = DT_INST_PROP(n, max_fingers),                             \
      .rx_tx_mapping = DT_INST_PROP(n, rx_tx_mapping),                         \
      .rx_tx_mapping_len = DT_INST_PROP_LEN(n, rx_tx_mapping),                 \
      .alp_ati_compensation = DT_INST_PROP(n, alp_ati_compensation),           \
      .alp_ati_compensation_len = DT_INST_PROP_LEN(n, alp_ati_compensation),   \
      .ati_multipliers = DT_INST_PROP(n, ati_multipliers),                     \
      .ati_multipliers_len = DT_INST_PROP_LEN(n, ati_multipliers),             \
      .ati_target = DT_INST_PROP(n, ati_target),                               \
      .alp_ati_target = DT_INST_PROP(n, alp_ati_target),                       \
      .alp_base_target = DT_INST_PROP(n, alp_base_target),                     \
      .tp_neg_delta_reati = DT_INST_PROP(n, tp_neg_delta_reati),               \
      .tp_pos_delta_reati = DT_INST_PROP(n, tp_pos_delta_reati),               \
      .tp_ref_drift_limit = DT_INST_PROP(n, tp_ref_drift_limit),               \
      .alp_lta_drift_limit = DT_INST_PROP(n, alp_lta_drift_limit),             \
      .touch_set_threshold = DT_INST_PROP(n, touch_set_threshold),             \
      .touch_clear_threshold = DT_INST_PROP(n, touch_clear_threshold),         \
      .alp_threshold = DT_INST_PROP(n, alp_threshold),                         \
      .alp_autoprox_threshold = DT_INST_PROP(n, alp_autoprox_threshold),       \
      .alp_set_debounce = DT_INST_PROP(n, alp_set_debounce),                   \
      .alp_clear_debounce = DT_INST_PROP(n, alp_clear_debounce),               \
      .active_sample_ms = DT_INST_PROP(n, active_sample_ms),                   \
      .idle_touch_sample_ms = DT_INST_PROP(n, idle_touch_sample_ms),           \
      .idle_sample_ms = DT_INST_PROP(n, idle_sample_ms),                       \
      .lp1_sample_ms = DT_INST_PROP(n, lp1_sample_ms),                         \
      .lp2_sample_ms = DT_INST_PROP(n, lp2_sample_ms),                         \
      .stationary_touch_timeout_s =                                            \
          DT_INST_PROP(n, stationary_touch_timeout_s),                         \
      .idle_touch_timeout_s = DT_INST_PROP(n, idle_touch_timeout_s),           \
      .idle_timeout_s = DT_INST_PROP(n, idle_timeout_s),                       \
      .lp1_timeout_s = DT_INST_PROP(n, lp1_timeout_s),                         \
      .active_timeout_ms = DT_INST_PROP(n, active_timeout_ms),                 \
      .reati_retry_time_s = DT_INST_PROP(n, reati_retry_time_s),               \
      .ref_update_time_s = DT_INST_PROP(n, ref_update_time_s),                 \
      .i2c_timeout_ms = DT_INST_PROP(n, i2c_timeout_ms),                       \
      .alp_count_beta_lp1 = DT_INST_PROP(n, alp_count_beta_lp1),               \
      .alp_lta_beta_lp1 = DT_INST_PROP(n, alp_lta_beta_lp1),                   \
      .alp_count_beta_lp2 = DT_INST_PROP(n, alp_count_beta_lp2),               \
      .alp_lta_beta_lp2 = DT_INST_PROP(n, alp_lta_beta_lp2),                   \
      .dynamic_filter_bottom_speed =                                           \
          DT_INST_PROP(n, dynamic_filter_bottom_speed),                        \
      .dynamic_filter_top_speed = DT_INST_PROP(n, dynamic_filter_top_speed),   \
      .dynamic_filter_bottom_beta =                                            \
          DT_INST_PROP(n, dynamic_filter_bottom_beta),                         \
      .static_filter_beta = DT_INST_PROP(n, static_filter_beta),               \
      .stationary_touch_threshold =                                            \
          DT_INST_PROP(n, stationary_touch_threshold),                         \
      .finger_split_factor = DT_INST_PROP(n, finger_split_factor),             \
      .jitter_filter_delta = DT_INST_PROP(n, jitter_filter_delta),             \
      .finger_confidence_threshold =                                           \
          DT_INST_PROP(n, finger_confidence_threshold),                        \
      .tap_enable = DT_INST_PROP(n, tap_enable),                               \
      .double_tap_enable = DT_INST_PROP(n, double_tap_enable),                 \
      .triple_tap_enable = DT_INST_PROP(n, triple_tap_enable),                 \
      .press_hold_enable = DT_INST_PROP(n, press_hold_enable),                 \
      .two_finger_tap_enable = DT_INST_PROP(n, two_finger_tap_enable),         \
      .scroll_enable = DT_INST_PROP(n, scroll_enable),                         \
      .swipe_enable = DT_INST_PROP(n, swipe_enable),                           \
      .zoom_enable = DT_INST_PROP(n, zoom_enable),                             \
      .palm_reject_enable = DT_INST_PROP(n, palm_reject_enable),               \
      .tap_time_ms = DT_INST_PROP(n, tap_time_ms),                             \
      .tap_air_time_ms = DT_INST_PROP(n, tap_air_time_ms),                     \
      .tap_distance = DT_INST_PROP(n, tap_distance),                           \
      .scroll_initial_distance = DT_INST_PROP(n, scroll_initial_distance),     \
      .scroll_consecutive_distance =                                           \
          DT_INST_PROP(n, scroll_consecutive_distance),                        \
      .hold_time_ms = DT_INST_PROP(n, hold_time_ms),                           \
      .swipe_time_ms = DT_INST_PROP(n, swipe_time_ms),                         \
      .swipe_x_distance = DT_INST_PROP(n, swipe_x_distance),                   \
      .swipe_y_distance = DT_INST_PROP(n, swipe_y_distance),                   \
      .swipe_angle = DT_INST_PROP(n, swipe_angle),                             \
      .scroll_angle = DT_INST_PROP(n, scroll_angle),                           \
      .swipe_x_consecutive_distance =                                          \
          DT_INST_PROP(n, swipe_x_consecutive_distance),                       \
      .swipe_y_consecutive_distance =                                          \
          DT_INST_PROP(n, swipe_y_consecutive_distance),                       \
      .zoom_initial_distance = DT_INST_PROP(n, zoom_initial_distance),         \
      .zoom_consecutive_distance = DT_INST_PROP(n, zoom_consecutive_distance), \
      .palm_gesture_threshold = DT_INST_PROP(n, palm_gesture_threshold),       \
      .config_settings = DT_INST_PROP(n, config_settings),                     \
      .config_settings_len = DT_INST_PROP_LEN(n, config_settings),             \
      .other_settings = DT_INST_PROP(n, other_settings),                       \
      .other_settings_len = DT_INST_PROP_LEN(n, other_settings),               \
      .alp_setup = DT_INST_PROP(n, alp_setup),                                 \
      .alp_setup_len = DT_INST_PROP_LEN(n, alp_setup),                         \
      .alp_tx_enable = DT_INST_PROP(n, alp_tx_enable),                         \
      .alp_tx_enable_len = DT_INST_PROP_LEN(n, alp_tx_enable),                 \
      .hardware_settings = DT_INST_PROP(n, hardware_settings),                 \
      .hardware_settings_len = DT_INST_PROP_LEN(n, hardware_settings),         \
  };                                                                           \
  DEVICE_DT_INST_DEFINE(n, iqs9150_init, NULL, &iqs9150_data_##n,              \
                        &iqs9150_config_##n, POST_KERNEL,                      \
                        CONFIG_INPUT_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(IQS9150_INST)
