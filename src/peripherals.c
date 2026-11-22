#include "peripherals.h"

#include <ch32fun.h>
#include <stdint.h>

#include "ports.h"

void init_peripherals(void) {
	funPinMode(BUSY, GPIO_Speed_In | GPIO_CNF_IN_FLOATING);
	funPinMode(RESET, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP);
	funPinMode(DC, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP);
	funPinMode(CS, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP_AF);
	funPinMode(CLK, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP_AF);
	funPinMode(MOSI, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP_AF);

	funDigitalWrite(DC, 0);

	funPinMode(SPI1_CS, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
	funPinMode(SPI1_CLK, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP_AF);
	funPinMode(SPI1_MISO, GPIO_Speed_In | GPIO_CNF_IN_PUPD);
	funPinMode(SPI1_MOSI, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP_AF);

	funDigitalWrite(SPI1_CS, 1);
	funDigitalWrite(SPI1_MISO, 1);

	// Setup SPI
	RCC->APB2PRSTR = RCC_SPI1RST;
	RCC->APB2PRSTR = 0;
	RCC->APB2PCENR |= RCC_APB2Periph_SPI1;

	RCC->APB1PRSTR = RCC_SPI2RST;
	RCC->APB1PRSTR = 0;
	RCC->APB1PCENR |= RCC_APB1Periph_SPI2;

	SPI1->CTLR2 = 0;
	SPI1->CTLR1 = SPI_DataSize_8b | SPI_FirstBit_MSB | SPI_CPHA_1Edge |
				  SPI_CPOL_Low | SPI_Direction_2Lines_FullDuplex |
				  SPI_Mode_Master | SPI_NSS_Soft | SPI_BaudRatePrescaler_256 |
				  CTLR1_SPE_Set | SPI_CTLR1_SSI;
	SPI2->CTLR2 = CTLR2_SSOE_Set;
	SPI2->CTLR1 = SPI_DataSize_8b | SPI_FirstBit_MSB | SPI_CPHA_1Edge |
				  SPI_CPOL_Low | SPI_Direction_1Line_Tx | SPI_Mode_Master |
				  SPI_NSS_Hard | SPI_BaudRatePrescaler_4 | CTLR1_SPE_Set;
	// Max SPI_BaudRatePrescaler_4 aka 36MHz overclock
	// SPI_BaudRatePrescaler_8 is within datasheet limits: 18MHz

	// Button
	funPinMode(BUTTON_LEFT, GPIO_Speed_In | GPIO_CNF_IN_PUPD);
	funPinMode(BUTTON_RIGHT, GPIO_Speed_In | GPIO_CNF_IN_PUPD);
	funDigitalWrite(BUTTON_LEFT, 1);
	funDigitalWrite(BUTTON_RIGHT, 1);

	// Interrupt for button
	AFIO->EXTICR[0] = (1 << 4) | (1 << 0);

	// Raising edge
	EXTI->RTENR = (1 << 1) | (1 << 0);
	EXTI->FTENR = 0;

	EXTI->INTENR = (1 << 1) | (1 << 0);

	wait_busy();
}

void set_dc(const uint8_t dc) {
	while (!(SPI2->STATR & SPI_STATR_TXE))
		;
	while (SPI2->STATR & SPI_STATR_BSY)
		;
	funDigitalWrite(DC, dc);
}

void wait_busy(void) {
	while (!(SPI2->STATR & SPI_STATR_TXE))
		;
	while (SPI2->STATR & SPI_STATR_BSY)
		;
	while (!funDigitalRead(BUSY))
		;
}

void send(const uint8_t data) {
	while (!(SPI2->STATR & SPI_STATR_TXE))
		;
	SPI2->DATAR = data;
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

void sd_cs(const uint8_t selected) {
	while (SPI1->STATR & SPI_STATR_BSY)
		;
	funDigitalWrite(SPI1_CS, selected);
}

uint8_t sd_xfer(const uint8_t data) {
	while (!(SPI1->STATR & SPI_STATR_TXE))
		;
	SPI1->DATAR = data;
	while (!(SPI1->STATR & SPI_STATR_RXNE))
		;
	return SPI1->DATAR;
}

void sd_send_seq(const uint8_t* data, const uint8_t size) {
	for (int i = 0; i < size; ++i) {
		sd_xfer(data[i]);
	}
}

uint8_t sd_get_r1(void) {
	uint8_t r1;
	for (int i = 0; i < 10 && ((r1 = sd_xfer(0xFF)) & 0x80); ++i)
		;
	return r1;
}
