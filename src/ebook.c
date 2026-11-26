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

// Both buttons pressed within this long of each other count as a chord
#define CHORD_WINDOW_MS 60
// Two chords within this long reset to the start of the book
#define DOUBLE_CHORD_MS 1500

static volatile uint8_t pressed;

int main() {
	SystemInit();
	funGpioInitAll();

	printf("Booting with offset %lu\n", read_last_page());

	init_peripherals();
	init_eink();
	if (sd_init() != 0) {
		printf("SD Init Failed\n");
		while (1)
			;
	}

	int ok = -1;
	for (int attempt = 0; attempt < 4 && ok != 0; ++attempt) {
		if (attempt) {
			Delay_Ms(1000);
		}
		// Header first: it sets where the book text starts on the card
		ok = book_init();
		if (ok == 0) {
			ok = fetch_page(read_last_page());
		}
	}

	if (ok != 0) {
		printf("Boot page read failed at offset %lu\n", read_last_page());
		while (1)
			;
	}

	printf("Rendering pages now\n");

	// Black
	uint32_t start = funSysTick32();
	load_draw_luts();

	prepare_text(page + 512);
	update_chapter_progress(read_last_page());
	render_prepared(page + 512);
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

	// Ticks of the last left+right chord, 0 when none is pending
	uint32_t last_chord = 0;

	while (1) {
		if (!pressed) {
			Delay_Ms(10);
			continue;
		}

		// Debounce, and give the other button a moment to join a chord
		Delay_Ms(CHORD_WINDOW_MS);

		if (!funDigitalRead(BUTTON_LEFT) && !funDigitalRead(BUTTON_RIGHT)) {
			const uint32_t now = funSysTick32();
			if (last_chord &&
				now - last_chord < DELAY_MSEC_COUNT(DOUBLE_CHORD_MS)) {
				last_chord = 0;
				goto_page(0);
			} else {
				last_chord = now | 1; // never 0
			}

			// Wait for both to be released so the release bounce
			// doesn't flip a page
			while (!funDigitalRead(BUTTON_LEFT) ||
				   !funDigitalRead(BUTTON_RIGHT))
				;
			Delay_Ms(20);
			pressed = 0;
			continue;
		}

		last_chord = 0;
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
