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

int main() {
	SystemInit();
	funGpioInitAll();

	init_peripherals();
	init_eink();
	sd_init();

	fetch_page();

	// Black
	uint32_t start = funSysTick32();
	load_lut_short(REG_C, 0x00, 1);
	load_lut_short(REG_WW, 0x54, 1);
	load_lut_short(REG_KW, 0x50, 1);
	load_lut_short(REG_WK, 0x40, 1);
	load_lut_short(REG_KK, 0x00, 1);

	render_text(page + 512);
	refresh();

	uint32_t end = funSysTick32();
	printf("Screen redraw took %ld/%d ticks\n", end - start,
		   DELAY_SEC_COUNT(1));

	// Start button interrupts
	EXTI->INTFR = (1 << 1) | (1 << 0);
	NVIC_EnableIRQ(EXTI0_IRQn);
	NVIC_EnableIRQ(EXTI1_IRQn);

	while (1)
		;
}

__attribute__((interrupt)) void EXTI0_IRQHandler(void) {
	printf("\nFlip backwards\n");

	flip_page(1);

	EXTI->INTFR = 1 << 0;
}

__attribute__((interrupt)) void EXTI1_IRQHandler(void) {
	printf("\nFlip forwards\n");

	flip_page(0);

	EXTI->INTFR = 1 << 1;
}
