#include <ch32fun.h>
#include <stdio.h>

#include "eink.h"
#include "glyphs.h"
#include "lut-opt.h"
#include "peripherals.h"
#include "ports.h"
#include "renderer.h"

// For UC8179 IC
// https://www.buydisplay.com/download/ic/UC8179.pdf

// Refresh over 4 frames
// -> Gray

static uint8_t glyph_pixel(const uint8_t* g, int w, int row, int col) {
	const int stride = (w + 3) / 4;
	return (g[row * stride + col / 4] >> (6 - 2 * (col % 4))) & 0x3;
}

static void draw_glyph(const uint8_t* g, int w, int h) {
	for (int x = 0; x < w; ++x) {
		for (int y = 0; y < h; ++y) {
			// White: WW
			const uint8_t color = glyph_pixel(g, w, x, y);

			// buffer_old[x][y / 8] = 0xFF;
			// buffer_new[x][y / 8] = 0;
			// buffer_current[x][y] = render;
		}
	}
}

void update(void) {
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
				send(((x % (WIDTH / 2)) > WIDTH / 4) ? 0xFF : 0x00);
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
				send(x > WIDTH / 2 ? 0xFF : 0x00);
			}
		}
	}
	set_dc(0);
}

const static char text[] = "Hello!";

int main() {
	SystemInit();
	funGpioInitAll();

	init_peripherals();
	init_eink();

	// The Eink screen is guarenteed to be blank rn

	Delay_Ms(2000);

	// Black
	uint32_t start = funSysTick32();
	load_lut_short(REG_C, 0x00);
	load_lut_short(REG_WW, 0x54);
	load_lut_short(REG_KW, 0x50);
	load_lut_short(REG_WK, 0x40);
	load_lut_short(REG_KK, 0x00);

	update();
	refresh();

	uint32_t end = funSysTick32();
	printf("Screen redraw took %ld/%d ticks\n", end - start,
		   DELAY_SEC_COUNT(1));

	Delay_Ms(5000);

	start = funSysTick32();
	load_lut_short(REG_WW, 0xA8);
	load_lut_short(REG_KW, 0xA0);
	load_lut_short(REG_WK, 0x80);
	load_lut_short(REG_KK, 0x00);
	refresh();
	end = funSysTick32();
	printf("Screen erasal took %ld/%d ticks\n", end - start,
		   DELAY_SEC_COUNT(1));

	Delay_Ms(10000);

	// End program
	power_off();

	while (1)
		;
}

__attribute__((interrupt)) void EXTI2_IRQHandler(void) {
	EXTI->INTFR = 1 << 2;
	printf("hi");
}

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
