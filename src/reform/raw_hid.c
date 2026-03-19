/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD
*/
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/class/hid.h>
#include <zephyr/usb/class/usb_hid.h>

#include <zmk/hid.h>

#include <reform/display.h>
#include <reform/matrix.h>

LOG_MODULE_DECLARE(mnt, CONFIG_MNT_LOG_LEVEL);

#define RAW_HID_REPORT_SIZE 520

#define HID_USAGE_PAGE16(page, page2)                                          \
  HID_ITEM(HID_ITEM_TAG_USAGE_PAGE, HID_ITEM_TYPE_GLOBAL, 2), page, page2

#define HID_REPORT_COUNT16(count)                                              \
  HID_ITEM(HID_ITEM_TAG_REPORT_COUNT, HID_ITEM_TYPE_GLOBAL, 2),               \
      (count & 0xFF), ((count >> 8) & 0xFF)

// Report ID 0x78 ('x') matches the original MNT Reform firmware protocol
#define REFORM_HID_REPORT_ID 0x78

static const uint8_t raw_hid_report_desc[] = {
    HID_USAGE_PAGE16(0x60, 0xFF), // Usage Page 0xFF60 (Vendor Defined)
    HID_USAGE(0x61),
    HID_COLLECTION(0x01),
    HID_REPORT_ID(REFORM_HID_REPORT_ID),
    HID_LOGICAL_MIN8(0x00),
    HID_LOGICAL_MAX16(0xFF, 0x00),
    HID_REPORT_SIZE(0x08),
    HID_REPORT_COUNT16(RAW_HID_REPORT_SIZE),
    HID_USAGE(0x01),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR |
              ZMK_HID_MAIN_VAL_ABS),
    HID_USAGE(0x02),
    HID_OUTPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR |
               ZMK_HID_MAIN_VAL_ABS | ZMK_HID_MAIN_VAL_NON_VOL),
    HID_END_COLLECTION,
};

static const struct device *raw_hid_dev;
static struct character_matrix hid_matrix;
static uint8_t hid_bitmap[BUFFER_SIZE];

static void hid_text_render(uint8_t *buffer) {
  matrix_render(&hid_matrix, buffer, -1);
}

static void hid_bitmap_render(uint8_t *buffer) {
  memcpy(buffer, hid_bitmap, BUFFER_SIZE);
}

static void handle_report(const uint8_t *data, uint16_t len) {
  // Skip report ID byte (0x78 / 'x')
  if (len < 1) return;
  data++;
  len--;

  if (len < 4) return;

  if (memcmp(data, "OLED", 4) == 0) {
    matrix_clear(&hid_matrix);
    for (int c = 4; c < len; c++) {
      matrix_write_char(&hid_matrix, data[c]);
    }
    display_request_render(hid_text_render);
  } else if (memcmp(data, "WCLR", 4) == 0) {
    display_request_render(NULL);
  } else if (memcmp(data, "WBIT", 4) == 0) {
    int copy_len = len - 4;
    if (copy_len > BUFFER_SIZE) copy_len = BUFFER_SIZE;
    memcpy(hid_bitmap, &data[4], copy_len);
    display_request_render(hid_bitmap_render);
  }
}

static void in_ready_cb(const struct device *dev) {}

static int get_report_cb(const struct device *dev,
                         struct usb_setup_packet *setup, int32_t *len,
                         uint8_t **data) {
  return -ENOTSUP;
}

static int set_report_cb(const struct device *dev,
                         struct usb_setup_packet *setup, int32_t *len,
                         uint8_t **data) {
  LOG_DBG("Received HID report, len=%d", *len);
  handle_report(*data, *len);
  return 0;
}

static const struct hid_ops ops = {
    .int_in_ready = in_ready_cb,
    .get_report = get_report_cb,
    .set_report = set_report_cb,
};

static int reform_raw_hid_init(void) {
  raw_hid_dev = device_get_binding("HID_1");
  if (raw_hid_dev == NULL) {
    LOG_ERR("Failed to find HID_1 device");
    return -EINVAL;
  }

  usb_hid_register_device(raw_hid_dev, raw_hid_report_desc,
                           sizeof(raw_hid_report_desc), &ops);
  usb_hid_init(raw_hid_dev);

  LOG_INF("Reform Raw HID Initialized!");
  return 0;
}

SYS_INIT(reform_raw_hid_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
