#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <stdint.h>

void init_peripherals(void);
void set_dc(const uint8_t dc);
void wait_busy(void);

void send(const uint8_t data);
void send_sequence(const uint8_t data[], const int length);

void sd_cs(const uint8_t selected);
uint8_t sd_xfer(const uint8_t data);
void sd_send_seq(const uint8_t* data, const uint8_t size);
uint8_t sd_get_r1(void);

#endif // PERIPHERALS_H
