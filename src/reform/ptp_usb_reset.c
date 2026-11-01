/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD
*/
#include <reform/ptp_report.h>

#include <zephyr/device.h>

#include <zmk/event_manager.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>

static int ptp_usb_reset_conn_changed(const zmk_event_t *eh) {
  const struct zmk_usb_conn_state_changed *ev =
      as_zmk_usb_conn_state_changed(eh);
  struct ptp_device *dev = ptp_device_get(DEVICE_DT_GET(DT_CHOSEN(mnt_ptp)));

  if (ev == NULL || dev == NULL || ev->conn_state == ZMK_USB_CONN_HID) {
    return ZMK_EV_EVENT_BUBBLE;
  }

  ptp_device_reset_sinks(dev);
  return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(ptp_usb_reset, ptp_usb_reset_conn_changed);
ZMK_SUBSCRIPTION(ptp_usb_reset, zmk_usb_conn_state_changed);
