#include <ch32fun.h>
#include <stdio.h>

#include "eink.h"
#include "lut-opt.h"
#include "peripherals.h"
#include "ports.h"

// For UC8179 IC
// https://www.buydisplay.com/download/ic/UC8179.pdf

// Refresh over 4 frames
// -> Gray

int main() {
	SystemInit();
	funGpioInitAll();

	init_peripherals();
	init_eink();

	// The Eink screen is guarenteed to be blank rn

	printf("Temp: %d C\n", get_temperature());

	power_off();

	while (1)
		;
}
