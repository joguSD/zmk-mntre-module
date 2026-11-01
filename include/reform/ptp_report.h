/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD
*/
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/sys/slist.h>

#define PTP_MIN_CONTACTS 3
#define PTP_MAX_CONTACTS 5
#define PTP_CONTACT_SIZE 5
#define PTP_REPORT_SIZE (PTP_MAX_CONTACTS * PTP_CONTACT_SIZE + 4)

#define PTP_INPUT_MODE_MOUSE 0
#define PTP_INPUT_MODE_PTP 3

#define PTP_SWITCH_SURFACE 0x01
#define PTP_SWITCH_BUTTON 0x02
#define PTP_SWITCH_ALL (PTP_SWITCH_SURFACE | PTP_SWITCH_BUTTON)

#define PTP_SCAN_TIME_UNIT_US 100

struct ptp_contact {
  uint16_t x;
  uint16_t y;
  uint8_t contact_id;
  bool tip_switch;
  bool confidence;
};

struct ptp_state {
  struct ptp_contact contacts[PTP_MAX_CONTACTS];
  uint8_t contact_count;
  bool button;
};

struct ptp_sink {
  sys_snode_t node;
  struct ptp_device *dev;
  void (*write)(struct ptp_sink *sink, const uint8_t *data, uint16_t len);
  void (*reset)(struct ptp_sink *sink);
  uint8_t mode;
  uint8_t mode_requested;
  uint8_t function_switch;
  bool mode_switching;
  bool mode_flushed;
};

struct ptp_device {
  sys_slist_t sinks;
  uint16_t active_ids;
  bool contacts_down;
  bool inhibited;
};

struct ptp_api {
  struct ptp_device *(*device)(const struct device *dev);
};

static inline struct ptp_device *ptp_device_get(const struct device *dev) {
  const struct ptp_api *api = dev->api;

  if (api == NULL || api->device == NULL) {
    return NULL;
  }
  return api->device(dev);
}

size_t ptp_pack_report(const struct ptp_state *state, uint16_t scan_time,
                       uint8_t *out, size_t out_len);
uint16_t ptp_scan_time_now(void);

void ptp_device_init(struct ptp_device *dev);
void ptp_sink_register(struct ptp_device *dev, struct ptp_sink *sink);
bool ptp_emit(struct ptp_device *dev, const struct ptp_state *state);

void ptp_device_inhibit(struct ptp_device *dev, bool inhibit);
void ptp_device_reset_sinks(struct ptp_device *dev);

void ptp_sink_mode_set(struct ptp_sink *sink, uint8_t mode);
uint8_t ptp_sink_mode_get(const struct ptp_sink *sink);
