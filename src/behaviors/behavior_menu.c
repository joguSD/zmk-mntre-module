/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD
*/
#define DT_DRV_COMPAT zmk_behavior_menu

// Dependencies
#include <drivers/behavior.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/keys.h>
#include <zmk/matrix.h>
#include <zmk/sensors.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

/**
 * Backport zmk_key_param and decoder from PR #1742.
 */
#ifndef ZMK_KEY_PARAM_DECODE
struct zmk_key_param {
  zmk_mod_flags_t modifiers;
  uint8_t page;
  uint16_t id;
};

#define ZMK_KEY_PARAM_DECODE(param)                                            \
  (struct zmk_key_param) {                                                     \
    .modifiers = SELECT_MODS(param), .page = ZMK_HID_USAGE_PAGE(param),        \
    .id = ZMK_HID_USAGE_ID(param),                                             \
  }
#endif // ZMK_KEY_PARAM_DECODE

// TODO Metadata?

// Instance-specific Data struct
struct behavior_menu_data {
  int item_index;
  int scroll_offset;
};

struct menu_item {
  const char *text;
  struct zmk_key_param key;
  struct zmk_behavior_binding behavior;
  int32_t virtual_key_position;
};

// Instance-specific Config struct (properties from device tree)
struct behavior_menu_config {
  size_t menu_len;
  struct menu_item *menu_items;
};

// Global state tracking across all menus
const struct device *active_menu_dev;

// Initialization Function (Optional)
static int menu_init(const struct device *dev) { return 0; };

static int on_menu_binding_pressed(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event) {
  LOG_DBG("Pressing menu binding");

  const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);

  // Closes the active menu upon duplicate menu binding press
  if (active_menu_dev == dev) {
    LOG_INF("Deactivating Menu: %s", dev->name);
    // TODO force release events for held down menu items?
    active_menu_dev = NULL;
    return ZMK_BEHAVIOR_OPAQUE;
  }

  LOG_INF("Activating Menu: %s", dev->name);
  active_menu_dev = dev;

  return ZMK_BEHAVIOR_OPAQUE;
}

static int on_menu_binding_released(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {
  return ZMK_BEHAVIOR_OPAQUE;
}

// API struct
static const struct behavior_driver_api menu_driver_api = {
    .binding_pressed = on_menu_binding_pressed,
    .binding_released = on_menu_binding_released,
};

#define ZMK_VIRTUAL_KEY_POSITION_MENU(index)                                   \
  (ZMK_KEYMAP_LEN + ZMK_KEYMAP_SENSORS_LEN + (index))

#define PROP_MENU_ITEMS(child)                                                 \
  {                                                                            \
      .text = DT_PROP(child, text),                                            \
      .key = ZMK_KEY_PARAM_DECODE(DT_PROP(child, key)),                        \
      .behavior = ZMK_KEYMAP_EXTRACT_BINDING(0, child),                        \
      .virtual_key_position = ZMK_VIRTUAL_KEY_POSITION_MENU(__COUNTER__),      \
  }

#define MENU_INST(n)                                                           \
  static struct behavior_menu_data menu_data_##n = {                           \
      .item_index = 0,                                                         \
      .scroll_offset = 0,                                                      \
  };                                                                           \
                                                                               \
  static struct menu_item menu_items##n[] = {                                  \
      DT_FOREACH_CHILD_SEP(DT_DRV_INST(n), PROP_MENU_ITEMS, (, ))};            \
  static const struct behavior_menu_config menu_config_##n = {                 \
      .menu_len = ARRAY_SIZE(menu_items##n),                                   \
      .menu_items = menu_items##n,                                             \
  };                                                                           \
                                                                               \
  BEHAVIOR_DT_INST_DEFINE(                                                     \
      n,              /* Instance Number (Automatically populated by macro) */ \
      menu_init,      /* Initialization Function */                            \
      NULL,           /* Power Management Device Pointer */                    \
      &menu_data_##n, /* Behavior Data Pointer */                              \
      &menu_config_##n, /* Behavior Configuration Pointer */                   \
      POST_KERNEL,      /* Initialization Level */                             \
      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, /* Device Priority */               \
      &menu_driver_api);                   /* API struct */

DT_INST_FOREACH_STATUS_OKAY(MENU_INST)

static int menu_keycode_state_changed_listener(const zmk_event_t *eh);

ZMK_LISTENER(behavior_menu_listener, menu_keycode_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_menu_listener, zmk_keycode_state_changed);

static int menu_keycode_state_changed_listener(const zmk_event_t *eh) {
  struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
  if (ev == NULL) {
    return ZMK_EV_EVENT_BUBBLE;
  }

  const struct zmk_key_param key = {
      .modifiers = ev->implicit_modifiers,
      .page = ev->usage_page,
      .id = ev->keycode,
  };

  // If menu is not active, do nothing and bubble
  if (!active_menu_dev) {
    return ZMK_EV_EVENT_BUBBLE;
  }

  const struct behavior_menu_config *config = active_menu_dev->config;
  for (size_t i = 0; i < config->menu_len; i++) {
    const struct menu_item *item = &config->menu_items[i];

    if (item->key.page == key.page && item->key.id == key.id) {
      LOG_DBG("Text: %s, %d", item->text, ev->state);
      struct zmk_behavior_binding_event event = {
          .position = item->virtual_key_position,
          .timestamp = ev->timestamp,
      };
      zmk_behavior_invoke_binding(&item->behavior, event, ev->state);
    }
  }

  // Menu is active, process events by updating menu
  return ZMK_EV_EVENT_HANDLED;
}

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
