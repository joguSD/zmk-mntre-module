/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * IQS9150 gesture codes for INPUT_EV_DEVICE events.
 */

#pragma once

/* Single-finger gestures (register 0x101C) */
#define IQS9150_GESTURE_1F_SWIPE_X_POS 0x01
#define IQS9150_GESTURE_1F_SWIPE_X_NEG 0x02
#define IQS9150_GESTURE_1F_SWIPE_Y_POS 0x03
#define IQS9150_GESTURE_1F_SWIPE_Y_NEG 0x04

/* Two-finger gestures (register 0x101E) */
#define IQS9150_GESTURE_2F_SWIPE_X_POS 0x11
#define IQS9150_GESTURE_2F_SWIPE_X_NEG 0x12
#define IQS9150_GESTURE_2F_SWIPE_Y_POS 0x13
#define IQS9150_GESTURE_2F_SWIPE_Y_NEG 0x14
#define IQS9150_GESTURE_2F_ZOOM_IN 0x15
#define IQS9150_GESTURE_2F_ZOOM_OUT 0x16
