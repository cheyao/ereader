#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <stdint.h>

void init_peripherals(void);
void set_dc(const uint8_t dc);
void wait_busy(void);

void send(const uint8_t data);
void send_sequence(const uint8_t data[], const int length);

#endif // PERIPHERALS_H
