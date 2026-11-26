#include "eink.h"

#include <ch32fun.h>
#include <stdint.h>
#include <stdio.h>

#include "lut-opt.h"
#include "peripherals.h"
#include "ports.h"

const static uint8_t Eink_Init_Code[] = {
		// Commands to initialize eink
		// Panel Settings: Reg LUT, BW mode, Scan up, Shift right, Booster on
		0x00, 1, 0x37,
		// Booster soft start / drive strength (Good Display reference values)
		// Stronger booster keeps VDH/VDL from sagging on dense pages
		0x06, 4, 0x17, 0x17, 0x28, 0x17,
		// PLL Clock Frequency
		0x30, 1, 0x06,
		// VCOM and DATA Interval Setting
		// LUT with OLD/NEW
		0x50, 2, 0x21, 0x03,
		// Resolution
		0x61, 4, (HEIGHT / 256), (HEIGHT % 256), (WIDTH / 256), (WIDTH % 256)};

void init_eink(void) {
	// Reset the screen
	funDigitalWrite(RESET, 0);
	Delay_Ms(1);
	funDigitalWrite(RESET, 1);
	Delay_Ms(4);

	send_sequence(Eink_Init_Code, sizeof(Eink_Init_Code));
	wait_busy();

	// Power ON booster
	const uint32_t start = funSysTick32();
	power_on();
	const uint32_t end = funSysTick32();
	printf("Booster power on took %ld\n", end - start);

	setup_lut(DEFAULT_LUT_SETTINGS);

	load_lut(REG_C, INIT_LUT_C);
	load_lut(REG_KW, INIT_LUT_KW);

	// Clear screen
	send(0x10);
	set_dc(1);
	// OLD
	for (int i = 0; i < 800 * 480 / 8; ++i) {
		send(0x00);
	}
	set_dc(0);

	send(0x13);
	set_dc(1);
	for (int i = 0; i < 800 * 480 / 8; ++i) {
		send(0xFF);
	}
	set_dc(0);

	refresh();

	// Disable border
	send(0x50);
	set_dc(1);
	send(0xA1);
	send(0x03);
	set_dc(0);
}

void refresh(void) {
	const uint32_t start = funSysTick32();
	send(0x12);
	wait_busy();
	const uint32_t end = funSysTick32();
	printf("Refreshed: %ld\n", end - start);
}

void refresh_no_wait(void) {
	send(0x12);
}

void setup_lut(const uint8_t LUT_SETTINGS[]) {
	// Frame rate
	send(0x30);
	set_dc(1);
	send(LUT_SETTINGS[0] >> 4);
	set_dc(0);

	// End voltage
	send(0x52);
	set_dc(1);
	send((LUT_SETTINGS[0] & 0b1000) | (LUT_SETTINGS[1] >> 6));
	set_dc(0);

	// Power settings
	send(0x01);
	set_dc(1);
	send(0b00111); // Internal DC/DC
	send((LUT_SETTINGS[0] & 0x07) | (0b1 << 4));
	send(LUT_SETTINGS[1] & 0x3F);
	send(LUT_SETTINGS[2] & 0x3F);
	send(LUT_SETTINGS[3] & 0x3F);
	set_dc(0);

	send(0x2A);
	set_dc(1);
	send(LUT_SETTINGS[2] & (0x3 << 6));
	send(LUT_SETTINGS[4]);
	set_dc(0);

	send(0x2B);
	set_dc(1);
	send(0b01);
	send(LUT_SETTINGS[3] & (0x3 << 6));
	send(0); // No KWE[7:0]
	set_dc(0);

	send(0x82);
	set_dc(1);
	send(LUT_SETTINGS[5]); // -1.90 VCOM
	set_dc(0);

	// Clear luts
	for (int i = 0; i < 5; ++i) {
		send(0x20 + i);
		set_dc(1);

		int num = 42;

		if (i != 1) {
			num += 18;
		}

		for (int j = 0; j < num; ++j) {
			send(0);
		}
		set_dc(0);
	}

	// Border LUT is LUTKW (BDV[1:0] = 0b10)

	wait_busy();
}

// One LUT group: 4 phase levels (2 bits each, phase 0 in bits 7:6),
// 4 phase frame counts and a repeat count. Remaining groups are zeroed.
static void load_lut_group(const uint8_t bank, const uint8_t levels,
						   const uint8_t frames[5]) {
	send(bank + 0x20);
	set_dc(1);
	send(levels);
	for (int j = 0; j < 5; ++j) {
		send(frames[j]);
	}

	for (int j = 0; j < 36; ++j) {
		send(0);
	}

	if (bank != REG_WW) {
		for (int j = 0; j < 18; ++j) {
			send(0);
		}
	}
	set_dc(0);
}

// Level bytes: 00 = hold, 01 = drive black, 10 = drive white
void load_draw_luts(void) {
	load_lut_group(REG_C, 0x00, LUT_WB_DELAYS);
	load_lut_group(REG_WW, 0x55, LUT_WB_DELAYS); // Black: all 4 phases
	load_lut_group(REG_KW, 0x50, LUT_WB_DELAYS); // Dark gray: phases 0-1
	load_lut_group(REG_WK, 0x40, LUT_WB_DELAYS); // Light gray: phase 0
	load_lut_group(REG_KK, 0x00, LUT_WB_DELAYS); // White: hold
}

void load_clear_luts(void) {
#if BALANCED_CLEAR
	// Phase 0: short black kick, phase 1: white for the black drive the pixel
	// got when drawn, plus the kick, plus a small white margin. Net drive per
	// flip is FRAMES_WHITE_MARGIN toward white.
#define CLEAR_WHITE(drive) ((drive) + FRAMES_SHAKE + FRAMES_WHITE_MARGIN)
	const uint8_t black[5] = {FRAMES_SHAKE, CLEAR_WHITE(DRIVE_BLACK), 0, 0, 1};
	const uint8_t dgray[5] = {FRAMES_SHAKE, CLEAR_WHITE(DRIVE_DGRAY), 0, 0, 1};
	const uint8_t lgray[5] = {FRAMES_SHAKE, CLEAR_WHITE(DRIVE_LGRAY), 0, 0, 1};
	const uint8_t white[5] = {FRAMES_SHAKE, CLEAR_WHITE(0), 0, 0, 1};
#undef CLEAR_WHITE

	// VCOM LUT spans the longest pixel LUT
	load_lut_group(REG_C, 0x00, black);
	load_lut_group(REG_WW, 0x60, black);
	load_lut_group(REG_KW, 0x60, dgray);
	load_lut_group(REG_WK, 0x60, lgray);
	load_lut_group(REG_KK, 0x60, white);
#else
	load_lut_group(REG_C, 0x00, LUT_BW_DELAYS);
	load_lut_group(REG_WW, 0x0A, LUT_BW_DELAYS);
	load_lut_group(REG_KW, 0x1A, LUT_BW_DELAYS);
	load_lut_group(REG_WK, 0x5A, LUT_BW_DELAYS);
	load_lut_group(REG_KK, 0x42, LUT_BW_DELAYS);
#endif
}

void load_lut(const uint8_t bank, const uint8_t data[]) {
	send(bank + 0x20);
	set_dc(1);
	for (int j = 0; j < 42; ++j) {
		send(data == 0 ? 0 : data[j]);
	}

	if (bank != REG_WW) {
		for (int j = 0; j < 18; ++j) {
			send(0);
		}
	}
	set_dc(0);
}

void power_on(void) {
	send(0x04);
	wait_busy();
}

void power_off(void) {
	send(0x02);
	wait_busy();
}

int8_t get_temperature(void) {
	// Enable (optional)
	send(0x41);
	set_dc(1);
	send(0x0);
	set_dc(0);

	// Now really read the value
	send(0x40);
	set_dc(1);

	(void)SPI2->DATAR;

	SPI2->CTLR1 &= ~CTLR1_SPE_Set;
	SPI2->CTLR1 &= ~SPI_CTLR1_BIDIOE;
	SPI2->CTLR1 |= CTLR1_SPE_Set;

	uint8_t buf[2];

	__disable_irq();
	for (int i = 0; i < 2 - 1; ++i) {
		while (!(SPI2->STATR & SPI_STATR_RXNE))
			;
		buf[i] = SPI2->DATAR;
	}
	// Clear last value
	SPI2->CTLR1 &= ~CTLR1_SPE_Set;
	while (!(SPI2->STATR & SPI_STATR_RXNE))
		;
	buf[1] = SPI2->DATAR;
	__enable_irq();

	SPI2->CTLR1 |= SPI_CTLR1_BIDIOE;
	SPI2->CTLR1 |= CTLR1_SPE_Set;

	set_dc(0);

	return buf[0];
}
