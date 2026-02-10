#pragma once

#include <stddef.h>
#include <stdint.h>

#include <zmk/behavior.h>

// Instance-specific Data struct
struct behavior_menu_data {
  uint8_t item_index;
  uint8_t highlight_index;
};

struct menu_item {
  const char *text;
  uint16_t key;
  struct zmk_behavior_binding behavior;
  int32_t virtual_key_position;
};

// Instance-specific Config struct (properties from device tree)
struct behavior_menu_config {
  size_t menu_len;
  struct menu_item *menu_items;
};

void menu_render(const struct behavior_menu_config *menu_config,
                 const struct behavior_menu_data *menu_data);
