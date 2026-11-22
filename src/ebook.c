#include <ch32fun.h>
#include <stdio.h>

#include "eink.h"
#include "lut-opt.h"
#include "page.h"
#include "peripherals.h"
#include "ports.h"
#include "renderer.h"

// For UC8179 IC
// https://www.buydisplay.com/download/ic/UC8179.pdf

// TODO: Normalize unicode
const static char page[] =
		"......................................................................"
		"......................................................................"
		"......................................................................"
		"......................................................................"
		"......................................................................"
		"......................................................................"
		"......................................................................"
		"......................";

int main() {
	SystemInit();
	funGpioInitAll();

	init_peripherals();
	// init_eink();

    printf("Last page: %d", read_last_page());

	while (1)
		;

	// The Eink screen is guarenteed to be blank rn

	Delay_Ms(1000);

	// Black
	uint32_t start = funSysTick32();
	load_lut_short(REG_C, 0x00, 1);
	load_lut_short(REG_WW, 0x54, 1);
	load_lut_short(REG_KW, 0x50, 1);
	load_lut_short(REG_WK, 0x40, 1);
	load_lut_short(REG_KK, 0x00, 1);

	render_text(page);
	refresh();

	uint32_t end = funSysTick32();
	printf("Screen redraw took %ld/%d ticks\n", end - start,
		   DELAY_SEC_COUNT(1));

	Delay_Ms(15000);

	/*
	start = funSysTick32();
	load_lut_short(REG_C, 0x00, 0);
	load_lut_short(REG_WW, 0x0A, 0);
	load_lut_short(REG_KW, 0x1A, 0);
	load_lut_short(REG_WK, 0x5A, 0);
	load_lut_short(REG_KK, 0x42, 0);
	refresh();

	load_lut_short(REG_C, 0x00, 1);
	load_lut_short(REG_WW, 0x54, 1);
	load_lut_short(REG_KW, 0x50, 1);
	load_lut_short(REG_WK, 0x40, 1);
	load_lut_short(REG_KK, 0x00, 1);
	render_text(text2);
	refresh();

	end = funSysTick32();
	printf("Page advance took %ld/%d ticks\n", end - start, DELAY_SEC_COUNT(1));
	*/

	Delay_Ms(10000);

	// End program
	power_off();

	while (1)
		;
}

__attribute__((interrupt)) void EXTI0_IRQHandler(void) {
	EXTI->INTFR = 1 << 0;

	printf("hi0");
	Delay_Ms(50);
}

__attribute__((interrupt)) void EXTI1_IRQHandler(void) {
	EXTI->INTFR = 1 << 1;

	printf("hi1");
	Delay_Ms(50);
}
