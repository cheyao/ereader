#ifndef PORTS_H
#define PORTS_H

// Reset, dc, cs active low
#define BUSY PA1
#define RESET PA2
#define DC PA3
#define CS PA4
#define CLK PA5
#define MOSI PA7

#define REG_C 0x20
#define REG_WW 0x21
#define REG_KW 0x22
#define REG_WK 0x23
#define REG_KK 0x24

// Decrease this to emulate a smaller screen
#define HEIGHT 800
#define WIDTH 480

#endif // PORTS_H
