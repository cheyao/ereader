#ifndef PORTS_H
#define PORTS_H

// Reset, dc, cs active low
#define BUSY PA1
#define RESET PA2
#define DC PA3
#define CS PA4
#define CLK PA5
#define MOSI PA7

#define BUTTON PB2

#define REG_C 0x00
#define REG_WW 0x01
#define REG_KW 0x02
#define REG_WK 0x03
#define REG_KK 0x04

// Decrease this to emulate a smaller screen
#define HEIGHT 800
#define WIDTH 480

#endif // PORTS_H
