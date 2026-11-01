#ifndef EINK_H
#define EINK_H

#include <stdint.h>

void init_eink(void);
void refresh(void);

// LUT operations
void send_lut(const uint8_t TR[]);
void load_lut(const uint8_t bank, const uint8_t data[]);

// Misc
int8_t get_temperature(void);

#endif // EINK_H
