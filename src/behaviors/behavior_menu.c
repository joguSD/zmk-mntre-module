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

#include <dt-bindings/zmk/hid_usage.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/keys.h>
#include <zmk/matrix.h>
#include <zmk/sensors.h>

#include <reform/display.h>
#include <reform/matrix.h>
#include <zmk-behavior-menu/menu.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

// TODO Metadata?

// Global state tracking across all menus
const struct device *active_menu_dev;
static struct character_matrix character_matrix;

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
    struct behavior_menu_data *data = active_menu_dev->data;
    active_menu_dev = NULL;
    data->item_index = 0;
    data->highlight_index = 0;
    buffer_logo();
    buffer_show();
    return ZMK_BEHAVIOR_OPAQUE;
  }

  LOG_INF("Activating Menu: %s", dev->name);
  active_menu_dev = dev;
  const struct behavior_menu_config *config = active_menu_dev->config;
  const struct behavior_menu_data *data = active_menu_dev->data;
  menu_render(config, data);

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
      .key = ZMK_HID_USAGE_ID(DT_PROP(child, key)),                            \
      .behavior = ZMK_KEYMAP_EXTRACT_BINDING(0, child),                        \
      .virtual_key_position = ZMK_VIRTUAL_KEY_POSITION_MENU(__COUNTER__),      \
  }

#define MENU_INST(n)                                                           \
  static struct behavior_menu_data menu_data_##n = {                           \
      .item_index = 0,                                                         \
      .highlight_index = 0,                                                    \
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

void menu_cycle_up() {
  struct behavior_menu_data *data = active_menu_dev->data;
  if (data->highlight_index > 0) {
    data->highlight_index--;
  } else {
    if (data->item_index > 0) {
      data->item_index--;
    }
  }

  LOG_DBG("Menu Up i %d, h %d", data->item_index, data->highlight_index);
}

void menu_cycle_down() {
  struct behavior_menu_data *data = active_menu_dev->data;
  const struct behavior_menu_config *config = active_menu_dev->config;
  if (data->highlight_index < CMATRIX_ROWS - 1) {
    data->highlight_index++;
  } else {
    if (data->item_index < config->menu_len - CMATRIX_ROWS) {
      data->item_index++;
    }
  }

  LOG_DBG("Menu Down i %d, h %d", data->item_index, data->highlight_index);
}

static int menu_keycode_state_changed_listener(const zmk_event_t *eh) {
  struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
  if (ev == NULL) {
    return ZMK_EV_EVENT_BUBBLE;
  }

  // If menu is not active, do nothing and bubble
  if (!active_menu_dev) {
    return ZMK_EV_EVENT_BUBBLE;
  }

  const struct behavior_menu_config *config = active_menu_dev->config;
  struct behavior_menu_data *data = active_menu_dev->data;

  if (ev->keycode == HID_USAGE_KEY_KEYBOARD_UPARROW && !ev->state) {
    menu_cycle_up();
    menu_render(config, data);
    return ZMK_EV_EVENT_HANDLED;
  }

  if (ev->keycode == HID_USAGE_KEY_KEYBOARD_DOWNARROW && !ev->state) {
    menu_cycle_down();
    menu_render(config, data);
    return ZMK_EV_EVENT_HANDLED;
  }

  if (ev->keycode == HID_USAGE_KEY_KEYBOARD_RETURN_ENTER) {
    uint8_t selected_item = data->item_index + data->highlight_index;
    const struct menu_item *item = &config->menu_items[selected_item];

    LOG_DBG("Select: %s, %d", item->text, ev->state);

    struct zmk_behavior_binding_event event = {
        .position = item->virtual_key_position,
        .timestamp = ev->timestamp,
    };
    zmk_behavior_invoke_binding(&item->behavior, event, ev->state);
    return ZMK_EV_EVENT_HANDLED;
  }

  for (size_t i = 0; i < config->menu_len; i++) {
    const struct menu_item *item = &config->menu_items[i];

    if (item->key == ev->keycode) {
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

// bridge behavior and our custom display logic
void menu_render(const struct behavior_menu_config *menu_config,
                 const struct behavior_menu_data *menu_data) {
  uint8_t item_index = menu_data->item_index;
  matrix_clear(&character_matrix);
  for (int i = 0; i < CMATRIX_ROWS; i++) {
    matrix_write_P(&character_matrix,
                   menu_config->menu_items[item_index++].text);
    if (item_index >= menu_config->menu_len) {
      item_index = 0;
    }
  }

  matrix_render(&character_matrix, buffer_get(), menu_data->highlight_index);
  buffer_show();
}

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
