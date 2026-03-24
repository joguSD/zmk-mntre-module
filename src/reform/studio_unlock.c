/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD

  Safety mechanism: holding physical position 0 (ESC) for 5 seconds
  unlocks ZMK Studio. This is independent of the keymap and can't be
  removed via Studio.
*/
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/studio/core.h>

LOG_MODULE_DECLARE(mnt, CONFIG_MNT_LOG_LEVEL);

#define UNLOCK_POSITION 0
#define UNLOCK_HOLD_MS 5000

static void unlock_timer_handler(struct k_work *work) {
  LOG_INF("Studio unlock triggered by long press");
  zmk_studio_core_unlock();
}

static K_WORK_DELAYABLE_DEFINE(unlock_work, unlock_timer_handler);

static int studio_unlock_listener(const zmk_event_t *eh) {
  struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
  if (ev == NULL || ev->position != UNLOCK_POSITION) {
    return ZMK_EV_EVENT_BUBBLE;
  }

  if (ev->state) {
    k_work_schedule(&unlock_work, K_MSEC(UNLOCK_HOLD_MS));
  } else {
    k_work_cancel_delayable(&unlock_work);
  }

  return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(reform_studio_unlock, studio_unlock_listener);
ZMK_SUBSCRIPTION(reform_studio_unlock, zmk_position_state_changed);
