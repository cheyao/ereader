#include "ch32fun.h"

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

const uint8_t Eink_Init_Code[] = {
		// Commands to initialize eink
		// Gate settings
		0x01, 3, (HEIGHT - 1) % 256, (HEIGHT - 1) / 256, 0x00,
		// Data entry mode - Y dec X inc
		0x11, 1, 0x01,
		// RAM X Window
		0x44, 2, 0x00, WIDTH / 8 - 1,
		// RAM Y Window
		0x45, 4, (HEIGHT - 1) % 256, (HEIGHT - 1) / 256, 0x00, 0x00,
		// Border waveform
		0x3C, 1, 0x05,
		// Read built-in temperature
		0x18, 1, 0x80,
		// Display update control 1 - Normal RAM, source from S8 to S167 (why)
		0x21, 2, 0x00, 0x80,
		// Display update control 2 - enable clock -> load temp -> load LUT ->
		// disable clock
		0x22, 1, 0xB1,
		// Execute master update
		0x20, 0};
const uint8_t Eink_Refresh[] = {
		// Display update control 2 - enable clock -> enable analog -> display
		// w/ mode 1 -> disable analog -> disable clock
		0x22, 1, 0xC7,
		// Execute master update
		0x20, 0};
const uint8_t Eink_Reset_Cursor[] = {
		// Set RAM X address
		0x4E, 1,
		0x00, 
        // Set RAM Y address
		0x4F, 2, (HEIGHT - 1) % 256, (HEIGHT - 1) / 256};

void wait_busy(void) {
	while (!(SPI1->STATR & SPI_STATR_TXE))
		;
	while (SPI1->STATR & SPI_STATR_BSY)
		;
	while (funDigitalRead(BUSY))
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
	Delay_Ms(10);
	funDigitalWrite(RESET, 1);
	Delay_Ms(10);

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

	// Restart
	send(0x12);
	wait_busy();

	send_sequence(Eink_Init_Code, sizeof(Eink_Init_Code));
	wait_busy();

	send_sequence(Eink_Reset_Cursor, sizeof(Eink_Reset_Cursor));

	send(0x24);
	set_dc(1);
	// 152 x 152
	for (int i = 0; i < 2888; ++i) {
		send(0xF9);
	}
	set_dc(0);

	send_sequence(Eink_Refresh, sizeof(Eink_Refresh));
	wait_busy();

	while (1)
		;
}
