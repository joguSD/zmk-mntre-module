/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD
*/
#define DT_DRV_COMPAT zmk_behavior_usb_guard

#include <drivers/behavior.h>
#include <stdbool.h>
#include <zephyr/device.h>

#include <zmk/behavior.h>

#if IS_ENABLED(CONFIG_ZMK_USB)
#include <zmk/usb.h>
#endif

#include <reform/display.h>
#include <reform/matrix.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct usb_guard_config {
  struct zmk_behavior_binding binding;
  const char *message;
};

static struct character_matrix character_matrix;
static const char *pending_message;

static void message_render_callback(uint8_t *buffer) {
  matrix_clear(&character_matrix);
  matrix_write_P(&character_matrix, pending_message);
  matrix_render(&character_matrix, buffer, -1);
}

static bool usb_host_present(void) {
#if IS_ENABLED(CONFIG_ZMK_USB)
  return zmk_usb_get_conn_state() == ZMK_USB_CONN_HID;
#else
  return true;
#endif
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
  const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
  const struct usb_guard_config *cfg = dev->config;

  if (!usb_host_present()) {
    if (cfg->message != NULL) {
      pending_message = cfg->message;
      display_request_render(message_render_callback);
    }
    return ZMK_BEHAVIOR_OPAQUE;
  }

  return zmk_behavior_invoke_binding(&cfg->binding, event, true);
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
  return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api usb_guard_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_EVENT_SOURCE,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

#define USB_GUARD_INST(n)                                                      \
  static const struct usb_guard_config usb_guard_config_##n = {                \
      .binding = {.behavior_dev =                                              \
                      DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(n, bindings, 0))}, \
      .message = DT_INST_PROP_OR(n, message, NULL),                            \
  };                                                                           \
  BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, &usb_guard_config_##n,          \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,    \
                          &usb_guard_driver_api);

DT_INST_FOREACH_STATUS_OKAY(USB_GUARD_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
