#include "ch32fun.h"
#include "lut-opt.h"

#include <stdio.h>

// Reset, dc, cs active low
#define BUSY PA1
#define RESET PA2
#define DC PA3
#define CS PA4
#define CLK PA5
#define MOSI PA7

#define HEIGHT 152
#define WIDTH 152

const uint8_t Eink_Software_Reset[] = {0x00, 1, 0x10};
const uint8_t Eink_Init_Code[] = {
		// Commands to initialize eink
		// Panel Settings: LUT from OTP, BW mode, scan up, shift right, booster
		// on
		0x00,
		1,
		0x3F,
		// PLL Clock Frequency
		0x30,
		1,
		0x06,
};
const uint8_t Eink_Refresh[] = {
		// Display update control 2 - enable clock -> enable analog -> display
		// w/ mode 1 -> disable analog -> disable clock
		0x22, 1, 0xC7,
		// Execute master update
		0x20, 0};
const uint8_t Eink_Reset_Cursor[] = {// Set RAM X address
									 0x4E, 1, 0x00,
									 // Set RAM Y address
									 0x4F, 2, (HEIGHT - 1) % 256,
									 (HEIGHT - 1) / 256};

void wait_busy(void) {
	while (!(SPI1->STATR & SPI_STATR_TXE))
		;
	while (SPI1->STATR & SPI_STATR_BSY)
		;
	Delay_Ms(1);
	while (!funDigitalRead(BUSY))
		;
}

void send(const uint8_t data) {
	while (!(SPI1->STATR & SPI_STATR_TXE))
		;
	SPI1->DATAR = data;
}

void set_dc(const uint8_t dc) {
	while (!(SPI1->STATR & SPI_STATR_TXE))
		;
	while (SPI1->STATR & SPI_STATR_BSY)
		;
	funDigitalWrite(DC, dc);
}

void send_sequence(const uint8_t data[], const int length) {
	for (int i = 0; i < length; ++i) {
		send(data[i]);
		set_dc(1);
		for (int j = 0; j < data[i + 1]; ++j) {
			send(data[i + j + 2]);
		}
		set_dc(0);
		i += data[i + 1] + 1;
	}
}

void send_lut(const uint8_t TR[]) {
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
	send((TR[0] & 0x03) | (0b1 << 4));
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

	for (int i = 0; i < 5; ++i) {
		send(0x20 + i);
		set_dc(1);
		for (int j = 0; j < 42; ++j) {
			if (i == 1 || i == 4) {
                // Void useless LUTs
				send(0x00);
			} else {
				send(TR[6 + i * 42 + j]);
			}
		}

		if (i != 1) {
			for (int j = 0; j < 18; ++j) {
				send(0);
			}
		}
		set_dc(0);
	}

	// Border LUT is never used so ignore

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

int main() {
	SystemInit();
	funGpioInitAll();

	funPinMode(BUSY, GPIO_Speed_In | GPIO_CNF_IN_FLOATING);
	funPinMode(RESET, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP);
	funPinMode(DC, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP);
	funPinMode(CS, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP_AF);
	funPinMode(CLK, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP_AF);
	funPinMode(MOSI, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP_AF);

	funDigitalWrite(DC, 0);

	// Reset the chip
	funDigitalWrite(RESET, 0);
	Delay_Ms(1);
	funDigitalWrite(RESET, 1);
	Delay_Ms(4);

	// Setup SPI
	RCC->APB2PRSTR = RCC_SPI1RST;
	RCC->APB2PRSTR = 0;
	RCC->APB2PCENR |= RCC_APB2Periph_SPI1;

	SPI1->CTLR2 = CTLR2_SSOE_Set;
	SPI1->CTLR1 = SPI_DataSize_8b | SPI_FirstBit_MSB | SPI_CPHA_1Edge |
				  SPI_CPOL_Low | SPI_Direction_1Line_Tx | SPI_Mode_Master |
				  SPI_BaudRatePrescaler_32 | CTLR1_SPE_Set;
	// Max SPI_BaudRatePrescaler_4 aka 36MHz overclock
	// SPI_BaudRatePrescaler_8 is within datasheet limits: 18MHz

	wait_busy();

	send_sequence(Eink_Init_Code, sizeof(Eink_Init_Code));
	wait_busy();

	// Power ON
	send(0x04);
	wait_busy();

	printf("Temp: %d C\n", get_temperature());

	send_lut(TR_20);

	/*
	send(0x10);
	set_dc(1);
	for (int i = 0; i < 800 * 480 / 8; ++i) {
		send(0x0F);
	}
	set_dc(0);
	*/

	send(0x13);
	set_dc(1);
	for (int i = 0; i < 800 * 480 / 8; ++i) {
		send(0xF0);
	}
	set_dc(0);

	uint32_t start = funSysTick32();
	send(0x12);
	wait_busy();
	uint32_t end = funSysTick32();
	printf("Used: %d", end - start);

	while (1)
		;
}
