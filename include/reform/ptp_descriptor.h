/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD
*/
#pragma once

#include <reform/ptp_report.h>
#include <zephyr/usb/class/hid.h>

#define HID_PHYSICAL_MIN8(a)                                                   \
  HID_ITEM(HID_ITEM_TAG_PHYSICAL_MIN, HID_ITEM_TYPE_GLOBAL, 1), a
#define HID_PHYSICAL_MAX16(a, b)                                               \
  HID_ITEM(HID_ITEM_TAG_PHYSICAL_MAX, HID_ITEM_TYPE_GLOBAL, 2), a, b
#define HID_PHYSICAL_MAX32(a, b, c, d)                                         \
  HID_ITEM(HID_ITEM_TAG_PHYSICAL_MAX, HID_ITEM_TYPE_GLOBAL, 3), a, b, c, d
#define HID_UNIT_EXPONENT(a)                                                   \
  HID_ITEM(HID_ITEM_TAG_UNIT_EXPONENT, HID_ITEM_TYPE_GLOBAL, 1), a
#define HID_UNIT8(a) HID_ITEM(HID_ITEM_TAG_UNIT, HID_ITEM_TYPE_GLOBAL, 1), a
#define HID_UNIT16(a, b)                                                       \
  HID_ITEM(HID_ITEM_TAG_UNIT, HID_ITEM_TYPE_GLOBAL, 2), a, b
#define HID_USAGE_PAGE16(a, b)                                                 \
  HID_ITEM(HID_ITEM_TAG_USAGE_PAGE, HID_ITEM_TYPE_GLOBAL, 2), a, b
#define HID_REPORT_COUNT16(a, b)                                               \
  HID_ITEM(HID_ITEM_TAG_REPORT_COUNT, HID_ITEM_TYPE_GLOBAL, 2), a, b

#define HID_IO_DATA_VAR_ABS 0x02
#define HID_IO_CONST_VAR_ABS 0x03
#define HID_IO_DATA_VAR_REL 0x06

#define HID_UNIT_CM_SILINEAR 0x11
#define HID_UNIT_EXP_DECI 0x0f
#define HID_UNIT_EXP_NONE 0x00
#define HID_UNIT_EXP_HUNDRED_MICRO 0x0c
#define HID_UNIT_SECONDS_LO 0x01
#define HID_UNIT_SECONDS_HI 0x10
#define HID_UNIT_NONE 0x00

#define HID_USAGE_PAGE_DIGITIZER 0x0d
#define HID_USAGE_PAGE_VENDOR_LO 0x00
#define HID_USAGE_PAGE_VENDOR_HI 0xff

#define PTP_USAGE_TOUCH_PAD 0x05
#define PTP_USAGE_CONFIGURATION 0x0e
#define PTP_USAGE_FINGER 0x22
#define PTP_USAGE_TIP_SWITCH 0x42
#define PTP_USAGE_CONFIDENCE 0x47
#define PTP_USAGE_CONTACT_ID 0x51
#define PTP_USAGE_INPUT_MODE 0x52
#define PTP_USAGE_CONTACT_COUNT 0x54
#define PTP_USAGE_CONTACT_COUNT_MAX 0x55
#define PTP_USAGE_SCAN_TIME 0x56
#define PTP_USAGE_SURFACE_SWITCH 0x57
#define PTP_USAGE_BUTTON_SWITCH 0x58
#define PTP_USAGE_PAD_TYPE 0x59
#define PTP_USAGE_CERT_BLOB 0xc5

#define PTP_REPORT_ID_TOUCHPAD 1
#define PTP_REPORT_ID_MAX_COUNT 3
#define PTP_REPORT_ID_PTPHQA 4
#define PTP_REPORT_ID_INPUT_MODE 5
#define PTP_REPORT_ID_FUNCTION_SWITCH 6

#define PTP_PTPHQA_SIZE 256

#define PTP_PAD_TYPE_CLICKPAD 0
#define PTP_PAD_TYPE_PRESSUREPAD 1
#define PTP_PAD_TYPE_DISCRETE 2

#define PTP_LO(v) ((v) & 0xff)
#define PTP_HI(v) (((v) >> 8) & 0xff)

#define PTP_DESC_FINGER(logical_x, logical_y, phys_x, phys_y)                  \
  HID_USAGE(PTP_USAGE_FINGER), HID_COLLECTION(HID_COLLECTION_LOGICAL),         \
      HID_LOGICAL_MIN8(0), HID_LOGICAL_MAX8(1), HID_REPORT_SIZE(1),            \
      HID_REPORT_COUNT(2), HID_USAGE(PTP_USAGE_CONFIDENCE),                    \
      HID_USAGE(PTP_USAGE_TIP_SWITCH), HID_INPUT(HID_IO_DATA_VAR_ABS),         \
      HID_REPORT_COUNT(2), HID_INPUT(HID_IO_CONST_VAR_ABS),                    \
      HID_LOGICAL_MAX8(15), HID_REPORT_SIZE(4), HID_REPORT_COUNT(1),           \
      HID_USAGE(PTP_USAGE_CONTACT_ID), HID_INPUT(HID_IO_DATA_VAR_ABS),         \
      HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),                                   \
      HID_UNIT_EXPONENT(HID_UNIT_EXP_DECI), HID_UNIT8(HID_UNIT_CM_SILINEAR),   \
      HID_REPORT_SIZE(16), HID_LOGICAL_MIN8(0), HID_PHYSICAL_MIN8(0),          \
      HID_LOGICAL_MAX16(PTP_LO(logical_x), PTP_HI(logical_x)),                 \
      HID_PHYSICAL_MAX16(PTP_LO(phys_x), PTP_HI(phys_x)),                      \
      HID_USAGE(HID_USAGE_GEN_DESKTOP_X), HID_INPUT(HID_IO_DATA_VAR_ABS),      \
      HID_LOGICAL_MAX16(PTP_LO(logical_y), PTP_HI(logical_y)),                 \
      HID_PHYSICAL_MAX16(PTP_LO(phys_y), PTP_HI(phys_y)),                      \
      HID_USAGE(HID_USAGE_GEN_DESKTOP_Y), HID_INPUT(HID_IO_DATA_VAR_ABS),      \
      HID_USAGE_PAGE(HID_USAGE_PAGE_DIGITIZER), HID_END_COLLECTION

#define PTP_REPORT_DESCRIPTOR(logical_x, logical_y, phys_x, phys_y)            \
  HID_USAGE_PAGE(HID_USAGE_PAGE_DIGITIZER), HID_USAGE(PTP_USAGE_TOUCH_PAD),    \
      HID_COLLECTION(HID_COLLECTION_APPLICATION),                              \
      HID_REPORT_ID(PTP_REPORT_ID_TOUCHPAD),                                   \
                                                                               \
      PTP_DESC_FINGER(logical_x, logical_y, phys_x, phys_y),                   \
      PTP_DESC_FINGER(logical_x, logical_y, phys_x, phys_y),                   \
      PTP_DESC_FINGER(logical_x, logical_y, phys_x, phys_y),                   \
      PTP_DESC_FINGER(logical_x, logical_y, phys_x, phys_y),                   \
      PTP_DESC_FINGER(logical_x, logical_y, phys_x, phys_y),                   \
                                                                               \
      HID_LOGICAL_MIN8(0), HID_LOGICAL_MAX8(127), HID_REPORT_SIZE(8),          \
      HID_REPORT_COUNT(1), HID_USAGE(PTP_USAGE_CONTACT_COUNT),                 \
      HID_INPUT(HID_IO_DATA_VAR_ABS),                                          \
                                                                               \
      HID_USAGE_PAGE(HID_USAGE_GEN_BUTTON), HID_LOGICAL_MAX8(1),               \
      HID_REPORT_SIZE(1), HID_REPORT_COUNT(3), HID_USAGE(1), HID_USAGE(2),     \
      HID_USAGE(3), HID_INPUT(HID_IO_DATA_VAR_ABS), HID_REPORT_COUNT(5),       \
      HID_INPUT(HID_IO_CONST_VAR_ABS),                                         \
                                                                               \
      HID_USAGE_PAGE(HID_USAGE_PAGE_DIGITIZER),                                \
      HID_UNIT_EXPONENT(HID_UNIT_EXP_HUNDRED_MICRO),                           \
      HID_UNIT16(HID_UNIT_SECONDS_LO, HID_UNIT_SECONDS_HI),                    \
      HID_LOGICAL_MAX32(0xff, 0xff, 0x00, 0x00),                               \
      HID_PHYSICAL_MAX32(0xff, 0xff, 0x00, 0x00), HID_REPORT_SIZE(16),         \
      HID_REPORT_COUNT(1), HID_USAGE(PTP_USAGE_SCAN_TIME),                     \
      HID_INPUT(HID_IO_DATA_VAR_ABS), HID_UNIT_EXPONENT(HID_UNIT_EXP_NONE),    \
      HID_UNIT8(HID_UNIT_NONE),                                                \
                                                                               \
      HID_REPORT_ID(PTP_REPORT_ID_MAX_COUNT), HID_LOGICAL_MIN8(0),             \
      HID_LOGICAL_MAX8(15), HID_REPORT_SIZE(4), HID_REPORT_COUNT(2),           \
      HID_USAGE(PTP_USAGE_CONTACT_COUNT_MAX), HID_USAGE(PTP_USAGE_PAD_TYPE),   \
      HID_FEATURE(HID_IO_DATA_VAR_ABS),                                        \
                                                                               \
      HID_USAGE_PAGE16(HID_USAGE_PAGE_VENDOR_LO, HID_USAGE_PAGE_VENDOR_HI),    \
      HID_REPORT_ID(PTP_REPORT_ID_PTPHQA), HID_USAGE(PTP_USAGE_CERT_BLOB),     \
      HID_LOGICAL_MIN8(0), HID_LOGICAL_MAX16(0xff, 0x00), HID_REPORT_SIZE(8),  \
      HID_REPORT_COUNT16(PTP_LO(PTP_PTPHQA_SIZE), PTP_HI(PTP_PTPHQA_SIZE)),    \
      HID_FEATURE(HID_IO_DATA_VAR_ABS), HID_END_COLLECTION,                    \
                                                                               \
      HID_USAGE_PAGE(HID_USAGE_PAGE_DIGITIZER),                                \
      HID_USAGE(PTP_USAGE_CONFIGURATION),                                      \
      HID_COLLECTION(HID_COLLECTION_APPLICATION),                              \
      HID_REPORT_ID(PTP_REPORT_ID_INPUT_MODE), HID_USAGE(PTP_USAGE_FINGER),    \
      HID_COLLECTION(HID_COLLECTION_LOGICAL), HID_LOGICAL_MIN8(0),             \
      HID_LOGICAL_MAX8(10), HID_REPORT_SIZE(8), HID_REPORT_COUNT(1),           \
      HID_USAGE(PTP_USAGE_INPUT_MODE), HID_FEATURE(HID_IO_DATA_VAR_ABS),       \
      HID_END_COLLECTION, HID_USAGE(PTP_USAGE_FINGER),                         \
      HID_COLLECTION(HID_COLLECTION_PHYSICAL),                                 \
      HID_REPORT_ID(PTP_REPORT_ID_FUNCTION_SWITCH), HID_LOGICAL_MIN8(0),       \
      HID_LOGICAL_MAX8(1), HID_REPORT_SIZE(1), HID_REPORT_COUNT(2),            \
      HID_USAGE(PTP_USAGE_SURFACE_SWITCH), HID_USAGE(PTP_USAGE_BUTTON_SWITCH), \
      HID_FEATURE(HID_IO_DATA_VAR_ABS), HID_REPORT_COUNT(6),                   \
      HID_FEATURE(HID_IO_CONST_VAR_ABS), HID_END_COLLECTION,                   \
      HID_END_COLLECTION
