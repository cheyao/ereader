#include <ch32fun.h>
#include <stdio.h>

#include "eink.h"
#include "lut-opt.h"
#include "peripherals.h"
#include "ports.h"

// Refresh over 4 frames
// -> Gray

int main() {
	SystemInit();
	funGpioInitAll();

	init_peripherals();
	init_eink();

	printf("Temp: %d C\n", get_temperature());

	while (1)
		;
}
