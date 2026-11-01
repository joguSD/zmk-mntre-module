/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD
*/
#include <reform/ptp_report.h>

#include <zephyr/device.h>

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>

#define PTP_INHIBIT_LAYER 1

static int ptp_layer_inhibit_changed(const zmk_event_t *eh) {
  const struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);
  struct ptp_device *dev = ptp_device_get(DEVICE_DT_GET(DT_CHOSEN(mnt_ptp)));

  if (ev == NULL || ev->layer != PTP_INHIBIT_LAYER || dev == NULL) {
    return ZMK_EV_EVENT_BUBBLE;
  }

  // This is a hack to block PTP reporting for the hyper layer
  // We need this so rel events go to the RGB handlers
  ptp_device_inhibit(dev, ev->state);
  return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(ptp_layer_inhibit, ptp_layer_inhibit_changed);
ZMK_SUBSCRIPTION(ptp_layer_inhibit, zmk_layer_state_changed);
