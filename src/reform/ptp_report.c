/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD
*/
#include <reform/ptp_report.h>

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

size_t ptp_pack_report(const struct ptp_state *state, uint16_t scan_time,
                       uint8_t *out, size_t out_len) {
  if (out_len < PTP_REPORT_SIZE) {
    return 0;
  }

  uint8_t count = MIN(state->contact_count, PTP_MAX_CONTACTS);

  for (uint8_t i = 0; i < PTP_MAX_CONTACTS; i++) {
    uint8_t *slot = &out[i * PTP_CONTACT_SIZE];

    if (i < count) {
      const struct ptp_contact *c = &state->contacts[i];

      slot[0] = (c->confidence ? BIT(0) : 0) | (c->tip_switch ? BIT(1) : 0) |
                (uint8_t)((c->contact_id & 0x0f) << 4);
      sys_put_le16(c->x, &slot[1]);
      sys_put_le16(c->y, &slot[3]);
    } else {
      memset(slot, 0, PTP_CONTACT_SIZE);
    }
  }

  out[PTP_MAX_CONTACTS * PTP_CONTACT_SIZE] = count;
  out[PTP_MAX_CONTACTS * PTP_CONTACT_SIZE + 1] = state->button ? BIT(0) : 0;
  sys_put_le16(scan_time, &out[PTP_MAX_CONTACTS * PTP_CONTACT_SIZE + 2]);

  return PTP_REPORT_SIZE;
}

uint16_t ptp_scan_time_now(void) {
  return (uint16_t)(k_ticks_to_us_floor64(k_uptime_ticks()) /
                    PTP_SCAN_TIME_UNIT_US);
}

void ptp_device_init(struct ptp_device *dev) {
  sys_slist_init(&dev->sinks);
  dev->contacts_down = false;
  dev->inhibited = false;
  dev->active_ids = 0;
}

static void ptp_append_releases(struct ptp_device *dev, struct ptp_state *out) {
  uint16_t now_ids = 0;

  for (uint8_t i = 0; i < out->contact_count; i++) {
    now_ids |= BIT(out->contacts[i].contact_id & 0x0f);
  }

  uint16_t pending = dev->active_ids & ~now_ids;

  for (uint8_t id = 0; id < 16 && pending != 0; id++) {
    if (!(pending & BIT(id))) {
      continue;
    }
    if (out->contact_count >= PTP_MAX_CONTACTS) {
      break;
    }

    struct ptp_contact *c = &out->contacts[out->contact_count];

    c->x = 0;
    c->y = 0;
    c->contact_id = id;
    c->tip_switch = false;
    c->confidence = true;
    out->contact_count++;
    pending &= ~BIT(id);
  }

  dev->active_ids = now_ids | pending;
}

void ptp_device_inhibit(struct ptp_device *dev, bool inhibit) {
  uint8_t all_up[PTP_REPORT_SIZE];
  struct ptp_state empty = {0};
  struct ptp_sink *sink;

  if (dev->inhibited == inhibit) {
    return;
  }

  dev->inhibited = inhibit;

  if (!inhibit) {
    return;
  }

  ptp_pack_report(&empty, ptp_scan_time_now(), all_up, sizeof(all_up));

  SYS_SLIST_FOR_EACH_CONTAINER(&dev->sinks, sink, node) {
    if (sink->write != NULL && sink->mode == PTP_INPUT_MODE_PTP) {
      sink->write(sink, all_up, PTP_REPORT_SIZE);
    }
  }
}

static void ptp_sink_defaults(struct ptp_sink *sink) {
  sink->mode = PTP_INPUT_MODE_MOUSE;
  sink->mode_requested = PTP_INPUT_MODE_MOUSE;
  sink->function_switch = PTP_SWITCH_ALL;
  sink->mode_switching = false;
  sink->mode_flushed = false;
}

void ptp_sink_register(struct ptp_device *dev, struct ptp_sink *sink) {
  ptp_sink_defaults(sink);
  sink->dev = dev;
  sys_slist_append(&dev->sinks, &sink->node);
}

void ptp_device_reset_sinks(struct ptp_device *dev) {
  struct ptp_sink *sink;

  dev->active_ids = 0;
  dev->contacts_down = false;

  SYS_SLIST_FOR_EACH_CONTAINER(&dev->sinks, sink, node) {
    ptp_sink_defaults(sink);
    if (sink->reset != NULL) {
      sink->reset(sink);
    }
  }
}

void ptp_sink_mode_set(struct ptp_sink *sink, uint8_t mode) {
  if (mode != PTP_INPUT_MODE_MOUSE && mode != PTP_INPUT_MODE_PTP) {
    mode = PTP_INPUT_MODE_MOUSE;
  }

  sink->mode_requested = mode;

  if (mode == sink->mode) {
    sink->mode_switching = false;
    return;
  }

  if (sink->dev == NULL || !sink->dev->contacts_down) {
    sink->mode = mode;
    sink->mode_switching = false;
    return;
  }

  sink->mode_switching = true;
  sink->mode_flushed = false;
}

uint8_t ptp_sink_mode_get(const struct ptp_sink *sink) { return sink->mode; }

static void ptp_sink_emit(struct ptp_sink *sink, const struct ptp_state *state,
                          const uint8_t *report, const uint8_t *all_up) {
  if (sink->write == NULL) {
    return;
  }

  if (sink->mode_switching) {
    if (!sink->mode_flushed) {
      if (sink->mode == PTP_INPUT_MODE_PTP) {
        sink->write(sink, all_up, PTP_REPORT_SIZE);
      }
      sink->mode_flushed = true;
    }
    if (state->contact_count == 0 && !state->button) {
      sink->mode = sink->mode_requested;
      sink->mode_switching = false;
    }
    return;
  }

  if (sink->mode != PTP_INPUT_MODE_PTP) {
    return;
  }
  if (!(sink->function_switch & PTP_SWITCH_SURFACE)) {
    return;
  }

  sink->write(sink, report, PTP_REPORT_SIZE);
}

bool ptp_emit(struct ptp_device *dev, const struct ptp_state *state) {
  uint8_t report[PTP_REPORT_SIZE];
  uint8_t all_up[PTP_REPORT_SIZE];
  struct ptp_state empty = {0};
  bool handled = false;
  struct ptp_sink *sink;

  if (dev->inhibited || sys_slist_is_empty(&dev->sinks)) {
    return false;
  }

  dev->contacts_down = state->contact_count > 0 || state->button;

  struct ptp_state framed = *state;

  ptp_append_releases(dev, &framed);

  uint16_t scan_time = ptp_scan_time_now();

  ptp_pack_report(&framed, scan_time, report, sizeof(report));
  ptp_pack_report(&empty, scan_time, all_up, sizeof(all_up));

  SYS_SLIST_FOR_EACH_CONTAINER(&dev->sinks, sink, node) {
    if (sink->mode == PTP_INPUT_MODE_PTP || sink->mode_switching) {
      handled = true;
    }
    ptp_sink_emit(sink, state, report, all_up);
  }

  return handled;
}
