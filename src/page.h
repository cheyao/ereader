#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>

#define FLASH_BASE_ADDR 0x08000000u
#define FAST_PAGE_SIZE 256u
#define ESIG_FLASH_KB (*(volatile uint16_t*)0x1FFFF7E0) // flash size in KB

void erase_last_page(void);
void write_last_page(uint32_t value);
uint32_t read_last_page(void);

#endif // PAGE_H
