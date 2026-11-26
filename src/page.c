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
	// Identification must run at <= 400 kHz, also when re-initializing after
	// a failed read (SPI1 is then still at the fast data rate)
	while (SPI1->STATR & SPI_STATR_BSY)
		;
	SPI1->CTLR1 &= ~CTLR1_SPE_Set;

	const uint32_t cfgr = RCC->CFGR0;
	RCC->CFGR0 = (cfgr & ~RCC_PPRE2) | RCC_PPRE2_DIV2;

	SPI1->CTLR1 |= SPI_BaudRatePrescaler_256; // All baud rate bits set: /256
	SPI1->CTLR1 |= CTLR1_SPE_Set;

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
		// SysTick->CNT is 64-bit: subtract in 32 bits, or the timeout expires
		// instantly once the counter passes 2^32 (~4 min after boot)
	} while (r1 == 0x01 &&
			 (uint32_t)(funSysTick32() - start) < Ticks_from_Ms(2000));

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
	SPI1->CTLR1 &= ~CTLR1_SPE_Set;

	RCC->CFGR0 = cfgr;
	SPI1->CTLR1 = (SPI1->CTLR1 & ~SPI_BaudRatePrescaler_256) |
				  SPI_BaudRatePrescaler_16;

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

	// Wait for data token 0xFE. Spec says up to 100 ms, but cards doing
	// internal housekeeping can take longer
	uint8_t t;
	uint32_t start = funSysTick32();
	// SysTick->CNT is 64-bit: subtract in 32 bits (see sd_init)
	while ((t = sd_xfer(0xFF)) == 0xFF &&
		   (uint32_t)(funSysTick32() - start) < Ticks_from_Ms(500))
		;
	if (t != 0xFE) {
		// 0xFF = timeout, 0x0X = card error token, else garbled
		printf("SD token 0x%02x\n", t);

		// Drain whatever the card still wants to send (block + CRC) before
		// deselecting, so it isn't left stuck mid-transfer and ignoring the
		// reset that follows
		for (int i = 0; i < 1024; ++i) {
			sd_xfer(0xFF);
		}

		sd_cs(1);
		sd_xfer(0xFF);
		return -2;
	}

	for (int i = 0; i < 512; ++i) {
		buf[i] = sd_xfer(0xFF);
	}
	sd_xfer(0xFF);
	sd_xfer(0xFF); // CRC, ignored

	sd_cs(1);
	sd_xfer(0xFF);
	return 0;
}

// Book layout from the card header written by sdcard/flash.sh. Without a
// header the text starts at block 0 and there is no chapter progress bar.
#define MAX_CHAPTERS 512

static uint32_t book_block; // First card block of the book text
static uint32_t book_len;
static uint32_t chapters[MAX_CHAPTERS]; // Chapter start offsets, ascending
static uint32_t chapter_count;

static uint32_t le32(const uint8_t* p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
		   ((uint32_t)p[3] << 24);
}

int book_init(void) {
	// Borrow the page buffer, fetch_page() overwrites it afterwards
	uint8_t* const buf = (uint8_t*)page;
	if (sd_read_block(0, buf) != 0) {
		return -1;
	}

	book_block = 0;
	book_len = 0;
	chapter_count = 0;
	if (buf[0] != 'E' || buf[1] != 'B' || buf[2] != 'K' || buf[3] != '1') {
		printf("No book header\n");
		return 0;
	}

	book_block = le32(buf + 4);
	book_len = le32(buf + 8);
	const uint32_t count = le32(buf + 12);
	if (count > MAX_CHAPTERS) {
		printf("Too many chapters (%lu), no progress bar\n", count);
	} else {
		// Table starts at byte 16 and may run into the next header blocks
		uint32_t loaded_block = 0;
		for (uint32_t k = 0; k < count; ++k) {
			const uint32_t pos = 16 + 4 * k;
			if (pos / 512 != loaded_block) {
				loaded_block = pos / 512;
				if (sd_read_block(loaded_block, buf) != 0) {
					return -1;
				}
			}
			// Offsets are 4-byte aligned, so never straddle a block
			chapters[k] = le32(buf + pos % 512);
		}
		chapter_count = count;
	}

	printf("Book: %lu bytes from block %lu, %lu chapters\n", book_len,
		   book_block, chapter_count);
	return 0;
}

int fetch_page(const uint32_t addr) {
	const uint32_t first = book_block + addr / 512;
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

// Sets the progress bar for the page at offset, laid out by prepare_text()
// (needs render_advance). Chapter = from its start offset up to the next
// chapter's start, or the end of the book.
void update_chapter_progress(uint32_t offset) {
	if (chapter_count == 0) {
		set_progress(0, 0);
		return;
	}

	// Last chapter starting at or before offset (binary search)
	uint32_t lo = 0, hi = chapter_count;
	while (hi - lo > 1) {
		const uint32_t mid = (lo + hi) / 2;
		if (chapters[mid] <= offset) {
			lo = mid;
		} else {
			hi = mid;
		}
	}

	const uint32_t start = chapters[lo];
	const uint32_t end = lo + 1 < chapter_count ? chapters[lo + 1] : book_len;
	set_progress(offset + render_advance - start, end - start);
}

static void show_page(uint32_t next, uint32_t start);

static void push_history(uint32_t offset) {
	history[history_pos] = offset;
	history_pos = (history_pos + 1) % HISTORY_LEN;
	if (history_len < HISTORY_LEN) {
		++history_len;
	}
}

void goto_page(uint32_t addr) {
	uint32_t start = funSysTick32();

	const uint32_t cur = read_last_page();
	if (addr == cur) {
		return;
	}
	if (fetch_page(addr) != 0) {
		return;
	}
	// Remember where we were so flipping back undoes the jump
	push_history(cur);
	show_page(addr, start);
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
		push_history(cur);
	}
	show_page(next, start);
}

// Saves next as the current page and draws it (page already fetched)
static void show_page(uint32_t next, uint32_t start) {
	write_last_page(next);

	// Do this even when we do full refresh to balance bw
	load_clear_luts();

	// Lay out the new page while the panel clears (CPU only, no panel I/O)
	const uint32_t clear_start = funSysTick32();
	refresh_no_wait();
	prepare_text(page + 512);
	update_chapter_progress(next);
	wait_busy();
	const uint32_t clear_end = funSysTick32();
	printf("Refreshed: %ld\n", clear_end - clear_start);

	static int flips_since_refresh = 0;
	if (flips_since_refresh >= FLIPS_PER_FULL_CLEAR - 1) {
		// Clear screen
		load_lut(REG_C, FULL_CLEAR_LUT_C);
		load_lut(REG_KW, FULL_CLEAR_LUT_KW);

		// Drive the border with its own balanced LUT (BDV=11: LUTBD):
		// floating, it drifts with the screen's long uniform drive
		load_lut(REG_BD, BORDER_CLEAR_LUT);
		send(0x50);
		set_dc(1);
		send(0x31);
		send(0x03);
		set_dc(0);

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

		// Border back to floating
		send(0x50);
		set_dc(1);
		send(0xA1);
		send(0x03);
		set_dc(0);

		flips_since_refresh = 0;
	} else {
		++flips_since_refresh;
	}

	load_draw_luts();
	render_prepared(page + 512); // Laid out during the clear pass
	refresh();

	uint32_t end = funSysTick32();
	printf("Page advance took %ld/%d ticks\n", end - start, DELAY_SEC_COUNT(1));
}
