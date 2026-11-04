#include "renderer.h"

#include <stdint.h>

#include "glyphs.h"
#include "peripherals.h"
#include "ports.h"

typedef struct Line {
	// Offset into text of our current line
	uint16_t index;
	uint16_t advance;

	uint8_t remaining;
} Line;

// TODO: = 33.333
static Line lines[HEIGHT / LINE_HEIGHT + 1];

void render_text(const char* text) {
	lines[0].index = 0;
	lines[0].advance = 0;

	lines[0].remaining = 0;
	for (int i = 0; text[i] != 0; ++i) {
		lines[0].remaining += 1;
	}

	send(0x10);
	set_dc(1);
	render_text_partial(text, 0);
	set_dc(0);

	lines[0].index = 0;
	lines[0].advance = 0;
	lines[0].remaining = 0;
	for (int i = 0; text[i] != 0; ++i) {
		lines[0].remaining += 1;
	}

	send(0x13);
	set_dc(1);
	render_text_partial(text, 1);
	set_dc(0);
}

#include <stdio.h>

void render_text_partial(const char* text, const uint8_t buffer) {
	for (int x = 0; x < WIDTH; ++x) {
		for (int y = 0; y < HEIGHT / 8; ++y) {
			// FIXME: When line height isn't a mul of 8
			const int line_num = y / (LINE_HEIGHT / 8);
			Line* line = &lines[line_num];

			if (line->remaining == 0) {
				send(0x00);
				continue;
			}

            if (text[line->index] == ' ') {
				line->index += 1;
				line->advance += SPACE_ADVANCE;
				line->remaining -= 1;
            }

			// Glyph proprieties
			const uintptr_t glyph_offset =
					offset_table[text[line->index] - '!'];
			const uint8_t w = font_table[glyph_offset],
						  h = font_table[glyph_offset + 1];
			const uint8_t xoff = font_table[glyph_offset + 2];
			const uint8_t glyph_advance = font_table[glyph_offset + 3];

			const uint8_t height = (h / 8 + !!(h % 8));

			const uint8_t local_x = x - line->advance - xoff;
			const uint8_t local_y = y - line_num * (LINE_HEIGHT / 8);
			if (line->advance + xoff <= x &&
				x < line->advance + glyph_advance && local_x < w &&
				local_y < height) {

				send(font_table[glyph_offset + HEADER_LENGTH +
								(local_x + buffer * w) * height + local_y]);
			} else {
				send(0x00);
			}

			if (line->advance + glyph_advance == x + 1 &&
				line->remaining != 0 && local_y == (LINE_HEIGHT / 8 - 1)) {
				line->index += 1;
				line->advance += glyph_advance;
				line->remaining -= 1;
			}
		}
	}
}
