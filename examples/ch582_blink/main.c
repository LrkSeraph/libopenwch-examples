/*
 * This file is part of the libopenwch examples.
 *
 * Copyright (C) 2025 libopenwch contributors
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Blink for a CH58x board.
 *
 * Brings the 32 MHz crystal up, runs the system from the 480 MHz PLL divided
 * to 60 MHz, and toggles a pin.  This is the CH58x counterpart of the
 * CH32V003 blink and exercises the three things that make this family
 * different:
 *
 *   - the crystal is mandatory: there is no internal high-speed oscillator on
 *     the CH582/CH583, so clk_set_sys_clock() powers XT32M before it touches
 *     the divider;
 *   - almost every clock register is RWA and needs the 0x57/0xA8 safe-access
 *     window, which the RWA_* macros handle;
 *   - GPIO is bit-parallel, so a whole port is configured with plain masks.
 *
 * Build:   make
 * Flash:   make flash
 */

#include <libopenwch/ch5xx58x/clk.h>
#include <libopenwch/ch5xx58x/gpio.h>
#include <libopenwch/qingke/systick.h>

/*
 * Board wiring.  Change these two lines for your own board.  PB4 is broken
 * out on most CH582/CH583 modules; the LED is active low on the WCH boards.
 */
#define LED_PORT GPIOB
#define LED_PIN GPIO4

int main(void) {
	uint32_t sysclk;

	/* 60 MHz from the PLL.  This powers XT32M first. */
	clk_set_sys_clock(CLK_SOURCE_PLL_60MHZ);
	sysclk = clk_get_sys_clock();

	/* Drive the LED pin push-pull. */
	gpio_set_mode(LED_PORT, GPIO_MODE_OUTPUT_PP_5MA, LED_PIN);

	/* Use SysTick as the time base: 1 ms ticks. */
	qingke_systick_set_frequency(sysclk);
	systick_set_clock_source(1); /* run from the system clock */
	systick_clear_interrupt();
	systick_enable_counter();

	for (;;) {
		gpio_toggle(LED_PORT, LED_PIN);
		qingke_delay_ms(250);
	}

	return 0;
}
