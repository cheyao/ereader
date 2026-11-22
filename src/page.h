#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>

#define FLASH_BASE_ADDR 0x08000000u
#define FAST_PAGE_SIZE 256u
#define ESIG_FLASH_KB (*(volatile uint16_t*)0x1FFFF7E0) // flash size in KB

#define MAX_PAGE_SIZE 4096

extern char page[MAX_PAGE_SIZE];
extern uint8_t sd_block_addr;

void erase_last_page(void);
void write_last_page(uint32_t value);
uint32_t read_last_page(void);

void sd_init(void);
void fetch_page(void);

#endif // PAGE_H
