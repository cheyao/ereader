#include "page.h"

#include <ch32fun.h>
#include <stdint.h>
#include <stdio.h>

#include "peripherals.h"
#include "ports.h"

char page[MAX_PAGE_SIZE];
uint8_t sd_block_addr;

static inline size_t last_page_addr(void) {
	return FLASH_BASE_ADDR + (uint32_t)ESIG_FLASH_KB * 1024u - FAST_PAGE_SIZE;
}

void erase_last_page(void) {
	FLASH->KEYR = FLASH_KEY1;
	FLASH->KEYR = FLASH_KEY2;
	FLASH->MODEKEYR = FLASH_KEY1;
	FLASH->MODEKEYR = FLASH_KEY2;

	while (FLASH->STATR & FLASH_STATR_BSY)
		;

	FLASH->CTLR |= FLASH_CTLR_PAGE_ER;
	FLASH->ADDR = last_page_addr();
	FLASH->CTLR |= FLASH_CTLR_STRT;
	while (FLASH->STATR & FLASH_STATR_BSY)
		;
	FLASH->STATR = FLASH_STATR_EOP;
	FLASH->CTLR &= ~FLASH_CTLR_PAGE_ER;

	if (read_last_page() != 0) {
		printf("Failed to erase flash\n");
	}

	// Lock flash again
	FLASH->CTLR |= FLASH_CTLR_LOCK | FLASH_CTLR_FAST_LOCK;
}

void write_last_page(uint32_t value) {
	volatile uint32_t* addr = (uint32_t*)last_page_addr();
	volatile uint32_t* end = addr + FAST_PAGE_SIZE / 4;
	while (addr < end && *addr != 0xE339E339 && *addr != 0xFFFFFFFF) {
		++addr;
	}

	// OOB
	if (addr == end) {
		erase_last_page();
		addr = (uint32_t*)last_page_addr();
	}

	FLASH->KEYR = FLASH_KEY1;
	FLASH->KEYR = FLASH_KEY2;

	FLASH->CTLR |= FLASH_CTLR_PG;

	*(volatile uint16_t*)addr = (uint16_t)value;
	while (FLASH->STATR & FLASH_STATR_BSY)
		;
	FLASH->STATR = FLASH_STATR_EOP;
	*(volatile uint16_t*)((uint32_t)addr + 2) = (uint16_t)(value >> 16);
	while (FLASH->STATR & FLASH_STATR_BSY)
		;
	FLASH->STATR = FLASH_STATR_EOP;

	FLASH->CTLR &= ~FLASH_CTLR_PG;

	if (read_last_page() != value) {
		printf("Failed to program value into flash!\n");
	}

	// Lock flash again
	FLASH->CTLR |= FLASH_CTLR_LOCK;
}

uint32_t read_last_page(void) {
	volatile uint32_t* addr = (uint32_t*)last_page_addr();
	volatile uint32_t* end = addr + FAST_PAGE_SIZE / 4;
	while (addr < end && *addr != 0xE339E339 && *addr != 0xFFFFFFFF) {
		++addr;
	}

	if (addr == (uint32_t*)last_page_addr()) {
		return 0;
	}

	return *(--addr);
}

void sd_init(void) {
	// Wake up clocks
	sd_cs(1);
	funPinMode(SPI1_CLK, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP);
	funPinMode(SPI1_MOSI, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP);
	funDigitalWrite(SPI1_MOSI, 1);
	for (int i = 0; i < 80; ++i) {
		funDigitalWrite(SPI1_CLK, 1);
		Delay_Us(1);
		funDigitalWrite(SPI1_CLK, 0);
		Delay_Us(1);
	}
	funPinMode(SPI1_CLK, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP_AF);
	funPinMode(SPI1_MOSI, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP_AF);

	// CMD0 sw reset
	const uint8_t cmd0[] = {0x40, 0, 0, 0, 0, 0x95};
	sd_cs(0);
	sd_send_seq(cmd0, sizeof(cmd0));
	uint8_t r1 = sd_get_r1();
	sd_cs(1);
	sd_xfer(0xFF);

	// CMD8 get voltage
	const uint8_t cmd8[] = {0x48, 0, 0, 0x01, 0xAA, 0x87};
	sd_cs(0);
	sd_send_seq(cmd8, sizeof(cmd8));
	r1 = sd_get_r1();
	uint8_t r7[4];
	for (int i = 0; i < 4; ++i) {
		r7[i] = sd_xfer(0xFF);
	}
	sd_cs(1);
	sd_xfer(0xFF);

	if (r7[2] != 0x01 || r7[3] != 0xAA) {
		printf("SD Card Initialization failed: 0x1AA mismatch\n");
	}

	// ACMD41 Init card
	const uint8_t cmd55[] = {0x77, 0, 0, 0, 0, 0x65};
	const uint8_t acmd41[] = {0x69, 0x40, 0, 0, 0, 0x77};
	uint32_t start = funSysTick32();
	do {
		sd_cs(0);
		sd_send_seq(cmd55, sizeof(cmd55));
		r1 = sd_get_r1();
		sd_xfer(0xFF);

		sd_send_seq(acmd41, sizeof(acmd41));
		r1 = sd_get_r1();
		sd_cs(1);
		sd_xfer(0xFF);
	} while (r1 == 0x01 && funSysTick32() - start < Ticks_from_Ms(2000));

	if (r1 != 0x00) {
		printf("SD Card Initialization failed: %x\n", r1);
	}

	// CMD58 read OCR
	const uint8_t cmd58[] = {0x7A, 0, 0, 0, 0, 0xFD};
	sd_cs(0);
	sd_send_seq(cmd58, sizeof(cmd58));
	r1 = sd_get_r1();
	uint8_t ocr[4];
	for (int i = 0; i < 4; ++i) {
		ocr[i] = sd_xfer(0xFF);
	}
	sd_cs(1);
	sd_xfer(0xFF);

	// - CCS = 1 (SDHC/SDXC): the CMD17 argument is the block number (0, 1, 2).
	// - CCS = 0 (SDSC): the argument is a byte address (block × 512).
	sd_block_addr = ocr[0] & 0x40;
	if (sd_block_addr == 0) {
		const uint8_t cmd16[] = {0x50, 0, 0, 0x02, 0, 0xFF};
		sd_cs(0);
		sd_send_seq(cmd16, sizeof(cmd16));
		r1 = sd_get_r1();
		sd_cs(1);
		sd_xfer(0xFF);
	}

	// Increase clk speed
	while (SPI1->STATR & SPI_STATR_BSY)
		;
	SPI1->CTLR1 &= ~CTLR1_SPE_Set;
	SPI1->CTLR1 = (SPI1->CTLR1 & ~SPI_BaudRatePrescaler_256) |
				  SPI_BaudRatePrescaler_8;
	SPI1->CTLR1 |= CTLR1_SPE_Set;
}

void fetch_page(void) {
	// TODO: MAX
	uint32_t addr = read_last_page();
}
