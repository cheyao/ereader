#include "renderer.h"

#include <stdint.h>

#include "ports.h"
#include "glyphs.h"
#include "peripherals.h"

uint8_t glyph_pixel(const uint8_t* g, const int w, const int row,
						   const int col) {
	const int stride = (w + 3) / 4;
	return (g[row * stride + col / 4] >> (6 - 2 * (col % 4))) & 0x3;
}

void render_text(const char* text) {
	char letter = 'e';
	uint32_t glyph = offset_table[letter - '!'];
	int w = font_table[offset_table[letter - '!']],
		h = font_table[offset_table[letter - '!'] + 1];

	send(0x10);
	set_dc(1);
	for (int x = 0; x < WIDTH; ++x) {
		for (int y = 0; y < HEIGHT / 8; ++y) {
			const uint8_t height = (h / 8 + !!(h % 8));
			if (x < w && y < height) {
				// LSB
				send(font_table[6 + x * height + y + glyph]);
			} else {
				send(0x00);
			}
		}
	}
	set_dc(0);

	send(0x13);
	set_dc(1);
	for (int x = 0; x < WIDTH; ++x) {
		for (int y = 0; y < HEIGHT / 8; ++y) {
			const uint8_t height = (h / 8 + !!(h % 8));
			if (x < w && y < height) {
				send(font_table[6 + (x + w) * height + y + glyph]);
			} else {
				send(0x00);
			}
		}
	}
	set_dc(0);
}
