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

static volatile uint8_t pressed;

int main() {
	SystemInit();
	funGpioInitAll();

	printf("Booting with offset %lu\n", read_last_page());

	init_peripherals();
	init_eink();
	// TODO: Print to eink screen?
	if (sd_init() != 0) {
		printf("SD Init Failed\n");
		while (1)
			;
	}

	int retry_times = 3;
	while (fetch_page(read_last_page()) != 0 && retry_times > 0) {
		--retry_times;
		Delay_Ms(1000);
	}

	if (retry_times == 0) {
		printf("Boot page read failed at offset %lu\n", read_last_page());
		while (1)
			;
	}

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
	NVIC_ClearPendingIRQ(EXTI0_IRQn);
	NVIC_ClearPendingIRQ(EXTI1_IRQn);
	NVIC_EnableIRQ(EXTI0_IRQn);
	NVIC_EnableIRQ(EXTI1_IRQn);

	while (1) {
		if (!pressed) {
			Delay_Ms(10);
			continue;
		}

		Delay_Ms(20);

		if (pressed & 0b01 && !funDigitalRead(BUTTON_LEFT)) {
			flip_page(1);
		} else if (pressed & 0b10 && !funDigitalRead(BUTTON_RIGHT)) {
			flip_page(0);
		}
		pressed = 0;
	}
}

__attribute__((interrupt)) void EXTI0_IRQHandler(void) {
	EXTI->INTFR = 1 << 0;
	pressed |= 1 << 0;
}

__attribute__((interrupt)) void EXTI1_IRQHandler(void) {
	EXTI->INTFR = 1 << 1;
	pressed |= 1 << 1;
}
