#pragma once
#include <stdbool.h>
#include <stdint.h>

#include <reform/constants.h>

#define FONT_WIDTH 6
#define FONT_HEIGHT 8
#define CMATRIX_ROWS (DISPLAY_HEIGHT / FONT_HEIGHT)
#define CMATRIX_COLS (DISPLAY_WIDTH / FONT_WIDTH)

struct character_matrix {
  uint8_t display[CMATRIX_ROWS][CMATRIX_COLS];
  uint8_t *cursor;
};

void matrix_write_char_inner(struct character_matrix *matrix, uint8_t c);
void matrix_write_char(struct character_matrix *matrix, uint8_t c);
void matrix_write_P(struct character_matrix *matrix, const char *data);
void matrix_clear(struct character_matrix *matrix);
void matrix_render(struct character_matrix *matrix, uint8_t *display_buffer,
                   int invert_row);
