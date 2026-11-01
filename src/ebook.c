#include <ch32fun.h>
#include <stdio.h>

#include "eink.h"
#include "glyphs.h"
#include "lut-opt.h"
#include "peripherals.h"
#include "ports.h"

// For UC8179 IC
// https://www.buydisplay.com/download/ic/UC8179.pdf

// Refresh over 4 frames
// -> Gray

static uint8_t glyph_pixel(const uint8_t* g, int w, int row, int col) {
	const int stride = (w + 3) / 4;
	return (g[row * stride + col / 4] >> (6 - 2 * (col % 4))) & 0x3;
}

static void send_glyph(const uint8_t* g, int w, int h, uint8_t threshold) {
	send(0x13);
	set_dc(1);
	for (int i = 0; i < HEIGHT * WIDTH / 8; ++i) {
		const int x = i / (HEIGHT / 8);
		const int y = i % (HEIGHT / 8);
		uint8_t output = 0xFF;

		if (x < w) {
			for (int l = 0; l < 8; ++l) {
				const int row = y * 8 + l;
				output <<= 1;
				if (row >= h || glyph_pixel(g, w, row, x) < threshold) {
					output |= 1;
				}
			}
		}
		send(output);
	}
	set_dc(0);
}

int main() {
	SystemInit();
	funGpioInitAll();

	init_peripherals();
	init_eink();

	// The Eink screen is guarenteed to be blank rn

	// printf("Temp: %d C\n", get_temperature());

	Delay_Ms(2000);

	load_lut(REG_KW, 0);
	load_lut(REG_WW, 0);

	// Set OLD to full white
	send(0x10);
	set_dc(1);
	for (int i = 0; i < 800 * 480 / 8; ++i) {
		send(0xFF);
	}
	set_dc(0);

	// TODO: Condense three passes into one
    char letter = 'b';
	const uint8_t* glyph = &font_table[offset_table[letter - '!'] + 6];
	int w = font_table[offset_table[letter - '!']],
		h = font_table[offset_table[letter - '!'] + 1];

	// Black
	load_lut(REG_C, LUT_C_WK);
	load_lut(REG_WK, LUT_WK);

	send_glyph(glyph, w, h, 3);
	refresh();

	// Dark gray
	load_lut(REG_C, LUT_C_WDK);
	load_lut(REG_WW, LUT_WW_WDK);
	load_lut(REG_WK, LUT_WDK);

	send_glyph(glyph, w, h, 2);
	refresh();

	// Light gray
	send_glyph(glyph, w, h, 1);
	refresh();

	/*
	send(0x13);
	set_dc(1);
	for (int i = 0; i < HEIGHT * WIDTH / 8; ++i) {
		if (i >= (HEIGHT * WIDTH / 8) / 4 * 1) {
			send(0x00);
		} else {
			send(0xFF);
		}
	}
	set_dc(0);
	*/

	Delay_Ms(30000);

	// End program
	power_off();

	while (1)
		;
}
