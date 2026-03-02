#pragma once
#include <stdbool.h>
#include <stdint.h>

#include <reform/constants.h>

struct character_matrix {
  uint8_t display[DISPLAY_TEXT_ROWS][DISPLAY_TEXT_COLS];
  uint8_t *cursor;
};

void matrix_write_char_inner(struct character_matrix *matrix, uint8_t c);
void matrix_write_char(struct character_matrix *matrix, uint8_t c);
void matrix_write_P(struct character_matrix *matrix, const char *data);
void matrix_clear(struct character_matrix *matrix);
void matrix_render(struct character_matrix *matrix, uint8_t *display_buffer,
                   int invert_row);
