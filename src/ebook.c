#include <ch32fun.h>
#include <stdio.h>

#include "eink.h"
#include "lut-opt.h"
#include "peripherals.h"
#include "ports.h"
#include "renderer.h"

// For UC8179 IC
// https://www.buydisplay.com/download/ic/UC8179.pdf

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
	load_lut_short(REG_C, 0x00, 1);
	load_lut_short(REG_WW, 0x54, 1);
	load_lut_short(REG_KW, 0x50, 1);
	load_lut_short(REG_WK, 0x40, 1);
	load_lut_short(REG_KK, 0x00, 1);

	render_text(text);
	refresh();

	uint32_t end = funSysTick32();
	printf("Screen redraw took %ld/%d ticks\n", end - start,
		   DELAY_SEC_COUNT(1));

	Delay_Ms(5000);

	start = funSysTick32();
	load_lut_short(REG_C, 0x00, 0);
	load_lut_short(REG_WW, 0x0A, 0);
	load_lut_short(REG_KW, 0x1A, 0);
	load_lut_short(REG_WK, 0x5A, 0);
	load_lut_short(REG_KK, 0x42, 0);
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
