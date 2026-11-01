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
		0x00, 1, 0x3F,
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
}

void refresh(void) {
	const uint32_t start = funSysTick32();
	send(0x12);
	wait_busy();
	const uint32_t end = funSysTick32();
	printf("Refreshed: %ld\n", end - start);
}

void setup_lut(const uint8_t TR[]) {
	// Frame rate
	send(0x30);
	set_dc(1);
	send(TR[0] >> 4);
	set_dc(0);

	// End voltage
	send(0x52);
	set_dc(1);
	send((TR[0] & 0b1000) | (TR[1] >> 6));
	set_dc(0);

	// Power settings
	send(0x01);
	set_dc(1);
	send(0b00111); // Internal DC/DC
	send((TR[0] & 0x07) | (0b1 << 4));
	send(TR[1] & 0x3F);
	send(TR[2] & 0x3F);
	send(TR[3] & 0x3F);
	set_dc(0);

	send(0x2A);
	set_dc(1);
	send(TR[2] & (0x3 << 6));
	send(TR[4]);
	set_dc(0);

	send(0x2B);
	set_dc(1);
	send(0b01);
	send(TR[3] & (0x3 << 6));
	send(0); // No KWE[7:0]
	set_dc(0);

	send(0x82);
	set_dc(1);
	send(TR[5]); // -1.90 VCOM
	set_dc(0);

	// Clear luts
	for (int i = 0; i < 5; ++i) {
		send(0x20 + i);
		set_dc(1);

		int num = 42;

		if (i != 1) {
			num += 18;
		}

		for (int j = 0; j < 42; ++j) {
			send(0);
		}
		set_dc(0);
	}

	// Border LUT is LUTKW (BDV[1:0] = 0b10)

	wait_busy();
}

void load_lut(const uint8_t bank, const uint8_t data[]) {
	send(bank);
	set_dc(1);
	for (int j = 0; j < 42; ++j) {
		send(data == 0 ? 0 : data[j]);
	}

	if (bank != 1) {
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

	(void)SPI1->DATAR;

	SPI1->CTLR1 &= ~CTLR1_SPE_Set;
	SPI1->CTLR1 &= ~SPI_CTLR1_BIDIOE;
	SPI1->CTLR1 |= CTLR1_SPE_Set;

	uint8_t buf[2];

	__disable_irq();
	for (int i = 0; i < 2 - 1; ++i) {
		while (!(SPI1->STATR & SPI_STATR_RXNE))
			;
		buf[i] = SPI1->DATAR;
	}
	// Clear last value
	SPI1->CTLR1 &= ~CTLR1_SPE_Set;
	while (!(SPI1->STATR & SPI_STATR_RXNE))
		;
	buf[1] = SPI1->DATAR;
	__enable_irq();

	SPI1->CTLR1 |= SPI_CTLR1_BIDIOE;
	SPI1->CTLR1 |= CTLR1_SPE_Set;

	set_dc(0);

	return buf[0];
}
