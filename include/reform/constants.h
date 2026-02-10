#pragma once
#include <zephyr/devicetree.h>

#define DISPLAY_NODE DT_CHOSEN(zephyr_display)

#define DISPLAY_WIDTH DT_PROP(DISPLAY_NODE, width)
#define DISPLAY_HEIGHT DT_PROP(DISPLAY_NODE, height)

#define BUFFER_SIZE DISPLAY_WIDTH *DISPLAY_HEIGHT / 8
