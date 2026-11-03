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

	// Setup SPI
	RCC->APB2PRSTR = RCC_SPI1RST;
	RCC->APB2PRSTR = 0;
	RCC->APB2PCENR |= RCC_APB2Periph_SPI1;

	SPI1->CTLR2 = CTLR2_SSOE_Set;
	SPI1->CTLR1 = SPI_DataSize_8b | SPI_FirstBit_MSB | SPI_CPHA_1Edge |
				  SPI_CPOL_Low | SPI_Direction_1Line_Tx | SPI_Mode_Master |
				  SPI_BaudRatePrescaler_8 | CTLR1_SPE_Set;
	// Max SPI_BaudRatePrescaler_4 aka 36MHz overclock
	// SPI_BaudRatePrescaler_8 is within datasheet limits: 18MHz

	// Button
	funPinMode(BUTTON, GPIO_CFGLR_IN_FLOAT);

	// Interrupt for button
	AFIO->EXTICR[0] = 1 << 8;

	// Falling edge
	EXTI->RTENR &= ~(1 << 2);
	EXTI->FTENR |= 1 << 2;

	NVIC_EnableIRQ(EXTI2_IRQn);

	EXTI->INTENR |= 1 << 2;

	wait_busy();
}

void set_dc(const uint8_t dc) {
	while (!(SPI1->STATR & SPI_STATR_TXE))
		;
	while (SPI1->STATR & SPI_STATR_BSY)
		;
	funDigitalWrite(DC, dc);
}

void wait_busy(void) {
	while (!(SPI1->STATR & SPI_STATR_TXE))
		;
	while (SPI1->STATR & SPI_STATR_BSY)
		;
	// Delay_Ms(1);
	while (!funDigitalRead(BUSY))
		;
}

void send(const uint8_t data) {
	while (!(SPI1->STATR & SPI_STATR_TXE))
		;
	SPI1->DATAR = data;
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

