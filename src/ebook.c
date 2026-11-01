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

	load_lut(REG_C, LUT_C);
	load_lut(REG_KW, LUT_KW);
	load_lut(REG_WK, LUT_WK);

	send(0x10);
	set_dc(1);
	for (int i = 0; i < 800 * 480 / 8; ++i) {
		send(0x0F);
	}
	set_dc(0);

	send(0x13);
	set_dc(1);
	for (int i = 0; i < 800 * 480 / 8; ++i) {
		send(0xF0);
	}
	set_dc(0);

	refresh();

	power_off();

	while (1)
		;
}
