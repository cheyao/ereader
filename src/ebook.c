#include <ch32fun.h>
#include <stdio.h>

#include "eink.h"
#include "lut-opt.h"
#include "peripherals.h"
#include "ports.h"

// For UC8179 IC
// https://www.buydisplay.com/download/ic/UC8179.pdf

// Refresh over 4 frames
// -> Gray

int main() {
	SystemInit();
	funGpioInitAll();

	init_peripherals();
	init_eink();

	// The Eink screen is guarenteed to be blank rn

	// printf("Temp: %d C\n", get_temperature());

	Delay_Ms(2000);

	load_lut(REG_KW, 0);

	// Set OLD to full white
	send(0x10);
	set_dc(1);
	for (int i = 0; i < 800 * 480 / 8; ++i) {
		send(0xFF);
	}
	set_dc(0);

	// Black
	load_lut(REG_C, LUT_C_WK);
	load_lut(REG_WK, LUT_WK);

	send(0x13);
	set_dc(1);
	for (int i = 0; i < HEIGHT * WIDTH / 8; ++i) {
		if (i >= (HEIGHT * WIDTH / 8) / 4 * 3) {
			send(0x00);
		} else {
			send(0xFF);
		}
	}
	set_dc(0);
	refresh();

	// Dark gray
	load_lut(REG_C, LUT_C_WDK);
	load_lut(REG_WW, LUT_WW_WDK);
	load_lut(REG_WK, LUT_WDK);

	send(0x13);
	set_dc(1);
	for (int i = 0; i < HEIGHT * WIDTH / 8; ++i) {
		if (i >= (HEIGHT * WIDTH / 8) / 4 * 2) {
			send(0x00);
		} else {
			send(0xFF);
		}
	}
	set_dc(0);
	refresh();

	// Light gray
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
	refresh();

	Delay_Ms(30000);

	// End program
	power_off();

	while (1)
		;
}
