/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2021-2024 MNT Research GmbH (mntre.com)
  Copyright 2026 joguSD
*/
#include <stdio.h>
#include <string.h>

#include "font.c"
#include <reform/matrix.h>

void matrix_write_char_inner(struct character_matrix *matrix, uint8_t c) {
  if (matrix->cursor - &matrix->display[0][0] == sizeof(matrix->display)) {
    // We went off the end; scroll the display upwards by one line
    memmove(&matrix->display[0], &matrix->display[1],
            CMATRIX_COLS * (CMATRIX_ROWS - 1));
    matrix->cursor = &matrix->display[CMATRIX_ROWS - 1][0];
    memset(matrix->cursor, ' ', CMATRIX_COLS);
  }

  *matrix->cursor = c;
  ++matrix->cursor;
}

void matrix_write_char(struct character_matrix *matrix, uint8_t c) {
  if (c == '\n') {
    // Clear to end of line from the cursor and then move to the
    // start of the next line
    int cursor_col = (matrix->cursor - &matrix->display[0][0]) % CMATRIX_COLS;

    while (cursor_col++ < CMATRIX_COLS) {
      matrix_write_char_inner(matrix, ' ');
    }
    return;
  }

  matrix_write_char_inner(matrix, c);
}

void matrix_write_P(struct character_matrix *matrix, const char *data) {
  while (true) {
    uint8_t c = *data;
    if (c == 0) {
      return;
    }
    matrix_write_char(matrix, c);
    ++data;
  }
}

void matrix_clear(struct character_matrix *matrix) {
  memset(matrix->display, ' ', sizeof(matrix->display));
  matrix->cursor = &matrix->display[0][0];
}

void matrix_render(struct character_matrix *matrix, uint8_t *display_buffer,
                   int invert_row) {
  int i = 0;
  for (uint8_t row = 0; row < CMATRIX_ROWS; ++row) {
    for (uint8_t col = 0; col < CMATRIX_COLS; ++col) {
      const uint8_t *glyph =
          reform_font + (matrix->display[row][col] * FONT_WIDTH);
      for (uint8_t glyphCol = 0; glyphCol < FONT_WIDTH; ++glyphCol) {
        uint8_t colBits = *(glyph + glyphCol);
        if (invert_row == row)
          colBits = ~colBits;
        display_buffer[i++] = colBits;
      }
    }
    // Pad to ensure all columns for the row are generated
    // In the current configuration, we have two spare columns per row
    while ((i % DISPLAY_WIDTH) != 0) {
      display_buffer[i++] = 0x00;
    }
  }
}
