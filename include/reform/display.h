#pragma once

#include <stdint.h>

typedef void (*display_render_callback_t)(uint8_t *buffer);

void display_request_render(display_render_callback_t callback);
