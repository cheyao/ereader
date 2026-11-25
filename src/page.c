#include "page.h"

#include <ch32fun.h>
#include <stdint.h>
#include <stdio.h>

#include "eink.h"
#include "lut-opt.h"
#include "peripherals.h"
#include "ports.h"
#include "renderer.h"

char page[MAX_PAGE_SIZE + 512 + 1];
uint8_t sd_block_addr;

#define HISTORY_LEN 16
static uint32_t history[HISTORY_LEN];
static uint8_t history_len, history_pos;

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

int sd_init(void) {
	// SD Init must run below 144 MHz
	const uint32_t cfgr = RCC->CFGR0;
	RCC->CFGR0 = (cfgr & ~RCC_PPRE2) | RCC_PPRE2_DIV2;

	// Wake up clocks
	sd_cs(1);
	funPinMode(SPI1_CLK, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP);
	funPinMode(SPI1_MOSI, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP);
	funDigitalWrite(SPI1_MOSI, 1);
	for (int i = 0; i < 80; ++i) {
		funDigitalWrite(SPI1_CLK, 1);
		Delay_Us(1);
		funDigitalWrite(SPI1_CLK, 0);
		Delay_Us(1);
	}
	funPinMode(SPI1_CLK, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF);
	funPinMode(SPI1_MOSI, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF);

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
		RCC->CFGR0 = cfgr;
		return -1;
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
		RCC->CFGR0 = cfgr;
		return -1;
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
	RCC->CFGR0 = cfgr;
	SPI1->CTLR1 &= ~CTLR1_SPE_Set;
	SPI1->CTLR1 = (SPI1->CTLR1 & ~SPI_BaudRatePrescaler_256) |
				  SPI_BaudRatePrescaler_8;
	SPI1->CTLR1 |= CTLR1_SPE_Set;

	return 0;
}

int sd_read_block(uint32_t block, uint8_t* buf) {
	uint32_t addr = sd_block_addr ? block : block * 512;
	const uint8_t cmd17[] = {0x51,		addr >> 24, addr >> 16,
							 addr >> 8, addr,		0x01};

	sd_cs(0);
	sd_send_seq(cmd17, 6);
	if (sd_get_r1() != 0x00) {
		sd_cs(1);
		sd_xfer(0xFF);
		return -1;
	}

	// Wait for data token 0xFE (card may take up to ~100 ms)
	uint8_t t;
	uint32_t start = funSysTick32();
	while ((t = sd_xfer(0xFF)) == 0xFF &&
		   funSysTick32() - start < Ticks_from_Ms(150))
		;
	if (t != 0xFE) {
		sd_cs(1);
		sd_xfer(0xFF);
		return -2;
	} // 0x0X = error token

	for (int i = 0; i < 512; ++i) {
		buf[i] = sd_xfer(0xFF);
	}
	sd_xfer(0xFF);
	sd_xfer(0xFF); // CRC, ignored

	sd_cs(1);
	sd_xfer(0xFF);
	return 0;
}

int fetch_page(const uint32_t addr) {
	const uint32_t first = addr / 512;
	const uint32_t skip = addr % 512;

	for (int i = 0; i < MAX_PAGE_SIZE / 512; ++i) {
		uint8_t* dst = (uint8_t*)page + (i + 1) * 512 - skip;

		const int err = sd_read_block(first + i, dst);
		if (err == 0) {
			continue;
		}
		// -1: command rejected, -2: no data token within the timeout
		printf("Try 1: SD read failed at block %lu (err %d)\n", first + i, err);

		// A failed read can leave the card stuck mid-transfer, answering
		// every later command with garbage: reset it before retrying
		if (sd_init() == 0 && sd_read_block(first + i, dst) == 0) {
			continue;
		}

		printf("Try 2: SD read failed at block %lu\n", first + i);
		printf("Aborting\n");

		if (i == 0) {
			return -1;
		} else {
			page[512 + i * 512 - skip] = 0;
			return 0;
		}
	}

	page[512 + 6 * 512 - skip] = 0;
	return 0;
}

void flip_page(int backwards) {
	uint32_t start = funSysTick32();

	const uint32_t cur = read_last_page();
	uint32_t next;
	if (backwards) {
		next = history_len
					   ? history[(history_pos + HISTORY_LEN - 1) % HISTORY_LEN]
					   : (cur < 1536 ? 0 : cur - 1536);
	} else {
		if (render_advance == 0) {
			return;
		}

		next = cur + render_advance;
	}

	// TODO: Put this with the refresh to save time?
	if (fetch_page(next) != 0) {
		return;
	}
	if (backwards) {
		if (history_len) {
			// Dec pos by 1
			history_pos = (history_pos + HISTORY_LEN - 1) % HISTORY_LEN;
			--history_len;
		}
	} else {
		history[history_pos] = cur;
		history_pos = (history_pos + 1) % HISTORY_LEN;
		if (history_len < HISTORY_LEN) {
			++history_len;
		}
	}
	write_last_page(next);

	// Do this even when we do full refresh to balance bw
	load_clear_luts();
	refresh();

	static int flips_since_refresh = 0;
	if (flips_since_refresh >= 10) {
		// Clear screen
		load_lut(REG_C, INIT_LUT_C);
		load_lut(REG_KW, INIT_LUT_KW);

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
		printf("Completedly refreshed screen\n");

		flips_since_refresh = 0;
	} else {
		++flips_since_refresh;
	}

	load_draw_luts();
	render_text(page + 512);
	refresh();

	uint32_t end = funSysTick32();
	printf("Page advance took %ld/%d ticks\n", end - start, DELAY_SEC_COUNT(1));
}
