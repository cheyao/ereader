#ifndef PORTS_H
#define PORTS_H

// Reset, dc, cs active low
#define BUSY PA0
#define RESET PA1
#define DC PA2
#define CS PB12
#define CLK PB13
#define MOSI PB15

#define BUTTON_LEFT PB0
#define BUTTON_RIGHT PB1

#define SPI1_CS PA3
#define SPI1_CLK PA5
#define SPI1_MISO PA6
#define SPI1_MOSI PA7

#define REG_C 0x00
#define REG_WW 0x01
#define REG_KW 0x02
#define REG_WK 0x03
#define REG_KK 0x04

// Decrease this to emulate a smaller screen
#define HEIGHT 800
#define WIDTH 480

#endif // PORTS_H
