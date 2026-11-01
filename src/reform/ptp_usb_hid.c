/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD
*/
#include <reform/ptp_descriptor.h>
#include <reform/ptp_report.h>

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/class/usb_hid.h>

LOG_MODULE_REGISTER(ptp_usb_hid, CONFIG_MNT_LOG_LEVEL);

#if !DT_HAS_CHOSEN(mnt_ptp)
#error "CONFIG_MNT_PTP_USB_HID needs chosen { mnt,ptp = <&trackpad>; }"
#endif

#define PTP_NODE DT_CHOSEN(mnt_ptp)

#define PTP_LOGICAL_X DT_PROP(PTP_NODE, x_resolution)
#define PTP_LOGICAL_Y DT_PROP(PTP_NODE, y_resolution)
#define PTP_PHYSICAL_X DT_PROP(PTP_NODE, x_physical_mm)
#define PTP_PHYSICAL_Y DT_PROP(PTP_NODE, y_physical_mm)
#define PTP_MAX_FINGERS DT_PROP(PTP_NODE, max_fingers)

#define PTP_CONTACT_COUNT_MAX MIN(PTP_MAX_FINGERS, PTP_MAX_CONTACTS)

#define PTP_HID_DEVICE_NAME                                                    \
  CONFIG_USB_HID_DEVICE_NAME "_" STRINGIFY(CONFIG_MNT_PTP_USB_HID_INSTANCE)

#define PTP_HID_REPORT_TYPE_MASK 0xff00
#define PTP_HID_REPORT_ID_MASK 0x00ff
#define PTP_HID_REPORT_TYPE_FEATURE 0x0300

BUILD_ASSERT(PTP_CONTACT_COUNT_MAX >= PTP_MIN_CONTACTS,
             "a precision touchpad must report at least PTP_MIN_CONTACTS "
             "concurrent contacts");
BUILD_ASSERT(CONFIG_HID_INTERRUPT_EP_MPS >= PTP_REPORT_SIZE + 1,
             "CONFIG_HID_INTERRUPT_EP_MPS must hold a full report; the USB "
             "driver silently truncates writes larger than the endpoint");
BUILD_ASSERT(CONFIG_USB_HID_DEVICE_COUNT > CONFIG_MNT_PTP_USB_HID_INSTANCE,
             "CONFIG_USB_HID_DEVICE_COUNT must exceed "
             "CONFIG_MNT_PTP_USB_HID_INSTANCE");

static const uint8_t ptp_desc[] = {PTP_REPORT_DESCRIPTOR(
    PTP_LOGICAL_X, PTP_LOGICAL_Y, PTP_PHYSICAL_X, PTP_PHYSICAL_Y)};

static const struct device *ptp_hid_dev;
static struct ptp_sink ptp_usb_hid_sink;
static K_SEM_DEFINE(ptp_tx_sem, 1, 1);

static void ptp_usb_hid_write(struct ptp_sink *sink, const uint8_t *data,
                              uint16_t len) {
  uint8_t buf[PTP_REPORT_SIZE + 1];

  if (ptp_hid_dev == NULL || len > PTP_REPORT_SIZE) {
    return;
  }

  buf[0] = PTP_REPORT_ID_TOUCHPAD;
  memcpy(&buf[1], data, len);

  if (k_sem_take(&ptp_tx_sem, K_NO_WAIT) != 0) {
    LOG_DBG("tx busy, dropping report");
    return;
  }

  if (hid_int_ep_write(ptp_hid_dev, buf, len + 1, NULL) != 0) {
    k_sem_give(&ptp_tx_sem);
  }
}

static void ptp_usb_hid_reset(struct ptp_sink *sink) {
  k_sem_reset(&ptp_tx_sem);
  k_sem_give(&ptp_tx_sem);
}

static void ptp_usb_hid_in_ready(const struct device *dev) {
  k_sem_give(&ptp_tx_sem);
}

static int ptp_usb_hid_get_report(const struct device *dev,
                                  struct usb_setup_packet *setup, int32_t *len,
                                  uint8_t **data) {
  static uint8_t buf[PTP_PTPHQA_SIZE + 1];
  uint8_t id = setup->wValue & PTP_HID_REPORT_ID_MASK;

  if ((setup->wValue & PTP_HID_REPORT_TYPE_MASK) !=
      PTP_HID_REPORT_TYPE_FEATURE) {
    return -ENOTSUP;
  }

  buf[0] = id;

  switch (id) {
  case PTP_REPORT_ID_MAX_COUNT:
    buf[1] = (uint8_t)(PTP_CONTACT_COUNT_MAX | (PTP_PAD_TYPE_DISCRETE << 4));
    *len = 2;
    break;
  case PTP_REPORT_ID_PTPHQA:
    memset(&buf[1], 0, PTP_PTPHQA_SIZE);
    *len = PTP_PTPHQA_SIZE + 1;
    break;
  case PTP_REPORT_ID_INPUT_MODE:
    buf[1] = ptp_usb_hid_sink.mode_requested;
    *len = 2;
    break;
  case PTP_REPORT_ID_FUNCTION_SWITCH:
    buf[1] = ptp_usb_hid_sink.function_switch;
    *len = 2;
    break;
  default:
    LOG_WRN("unhandled get_report id %u", id);
    return -ENOTSUP;
  }

  *data = buf;
  return 0;
}

static int ptp_usb_hid_set_report(const struct device *dev,
                                  struct usb_setup_packet *setup, int32_t *len,
                                  uint8_t **data) {
  uint8_t id = setup->wValue & PTP_HID_REPORT_ID_MASK;

  if ((setup->wValue & PTP_HID_REPORT_TYPE_MASK) !=
      PTP_HID_REPORT_TYPE_FEATURE) {
    return -ENOTSUP;
  }
  if (*len < 2) {
    return -EINVAL;
  }

  switch (id) {
  case PTP_REPORT_ID_INPUT_MODE:
    ptp_sink_mode_set(&ptp_usb_hid_sink, (*data)[1]);
    LOG_INF("input mode %u", (*data)[1]);
    break;
  case PTP_REPORT_ID_FUNCTION_SWITCH:
    ptp_usb_hid_sink.function_switch = (*data)[1] & PTP_SWITCH_ALL;
    LOG_INF("function switch %u", ptp_usb_hid_sink.function_switch);
    break;
  default:
    LOG_WRN("unhandled set_report id %u", id);
    return -ENOTSUP;
  }

  return 0;
}

static const struct hid_ops ptp_usb_hid_ops = {
    .get_report = ptp_usb_hid_get_report,
    .set_report = ptp_usb_hid_set_report,
    .int_in_ready = ptp_usb_hid_in_ready,
};

static int ptp_usb_hid_init(void) {
  const struct device *trackpad = DEVICE_DT_GET(PTP_NODE);

  if (!device_is_ready(trackpad)) {
    LOG_ERR("trackpad not ready");
    return -ENODEV;
  }

  ptp_hid_dev = device_get_binding(PTP_HID_DEVICE_NAME);
  if (ptp_hid_dev == NULL) {
    LOG_ERR("%s not found; raise CONFIG_USB_HID_DEVICE_COUNT",
            PTP_HID_DEVICE_NAME);
    return -ENODEV;
  }

  usb_hid_register_device(ptp_hid_dev, ptp_desc, sizeof(ptp_desc),
                          &ptp_usb_hid_ops);

  int err = usb_hid_init(ptp_hid_dev);
  if (err) {
    LOG_ERR("usb_hid_init failed: %d", err);
    ptp_hid_dev = NULL;
    return err;
  }

  struct ptp_device *ptp_dev = ptp_device_get(trackpad);

  if (ptp_dev == NULL) {
    LOG_ERR("trackpad does not expose a PTP API");
    ptp_hid_dev = NULL;
    return -ENOTSUP;
  }

  ptp_usb_hid_sink.write = ptp_usb_hid_write;
  ptp_usb_hid_sink.reset = ptp_usb_hid_reset;
  ptp_sink_register(ptp_dev, &ptp_usb_hid_sink);

  LOG_INF("PTP on %s, %u contacts, %ux%u over %ux%u mm", PTP_HID_DEVICE_NAME,
          PTP_CONTACT_COUNT_MAX, PTP_LOGICAL_X, PTP_LOGICAL_Y, PTP_PHYSICAL_X,
          PTP_PHYSICAL_Y);
  return 0;
}

SYS_INIT(ptp_usb_hid_init, APPLICATION, CONFIG_MNT_PTP_USB_HID_INIT_PRIORITY);
