#ifndef EINK_H
#define EINK_H

#include <stdint.h>

void init_eink(void);
void refresh(void);

// LUT operations
void setup_lut(const uint8_t LUT_SETTINGS[]);
void load_lut_short(const uint8_t bank, const uint8_t data, bool wtb);
void load_lut(const uint8_t bank, const uint8_t data[]);

// Misc
void power_on(void);
void power_off(void);
int8_t get_temperature(void);

#endif // EINK_H
