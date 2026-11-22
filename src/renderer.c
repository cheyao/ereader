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

static Line lines[HEIGHT / LINE_HEIGHT + 1];
uint32_t render_advance;

#define max(a, b) (((a) > (b)) ? (a) : (b))

void prepare_text(const char* text) {
	int line = 0;
	int line_advance = 0;

	lines[0].index = 0;
	lines[0].advance = 0;
	lines[0].remaining = 0;

	// TODO: SPACE in font table
	int i = 0;
	for (; text[i] != 0; ++i) {
		char c = text[i];
		if (c < '!' || c > '~') {
			c = '?';
		}

		if (text[i] == '\r') {
			// \r means end of section
			break;
		}

		const uintptr_t glyph_offset = offset_table[c - '!'];
		const uint8_t glyph_advance =
				text[i] == ' ' ? SPACE_ADVANCE : font_table[glyph_offset + 3];

		if (text[i] == '\n' || (line_advance + glyph_advance) > WIDTH) {
			if (text[i] != '\n' && text[i] != ' ') {
				do {
					// Retreat to last spaace to break
					lines[line].remaining -= 1;
					i -= 1;
				} while (text[i] != ' ' && text[i] != '\n');
			}

			++line;

			if (line >= (HEIGHT / LINE_HEIGHT + 1)) {
				break;
			}

			lines[line].index = (text[i] == '\n' || text[i] == ' ') ? i + 1 : i;
			lines[line].advance = 0;
			lines[line].remaining = 0;

			line_advance = 0;

			if (text[i] == '\n' || text[i] == ' ') {
				continue;
			}
		}

		lines[line].remaining += 1;
		line_advance += text[i] == ' ' ? SPACE_ADVANCE : glyph_advance;
	}

	if (line >= HEIGHT / LINE_HEIGHT + 1) {
		render_advance = lines[HEIGHT / LINE_HEIGHT].index;
	} else if (text[i] == '\r')
		render_advance = i + 1;
	else {
		render_advance = i;
	}
}

void render_text(const char* text) {
	prepare_text(text);

	send(0x10);
	set_dc(1);
	render_text_partial(text, 0);
	set_dc(0);

	prepare_text(text);

	send(0x13);
	set_dc(1);
	render_text_partial(text, 1);
	set_dc(0);
}

void render_text_partial(const char* text, const uint8_t buffer) {
	for (int x = 0; x < WIDTH; ++x) {
		for (int y = 0; y < HEIGHT / 8; ++y) {
			const int line_num = y / (LINE_HEIGHT / 8);
			Line* line = &lines[line_num];

			if (line->remaining == 0) {
				send(0x00);
				continue;
			}

			while (line->remaining != 0 &&
				   (text[line->index] == ' ' || text[line->index] == '\n')) {
				line->index += 1;
				line->advance += SPACE_ADVANCE;
				line->remaining -= 1;
			}

			if (line->remaining == 0) {
				send(0x00);
				continue;
			}

			char c = text[line->index];
			if (c < '!' || c > '~') {
				c = '?';
			}

			// Glyph proprieties
			const uintptr_t glyph_offset = offset_table[c - '!'];
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
