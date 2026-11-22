#include "page.h"

#include <ch32fun.h>
#include <stdint.h>
#include <stdio.h>

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
