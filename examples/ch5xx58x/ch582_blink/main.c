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

#include <libopenwch/ch5xx58x/gpio.h>
#include <libopenwch/ch5xx58x/rcc.h>
#include <libopenwch/qingke/systick.h>

/*
 * Board wiring.  Change these two lines for your own board.  The current
 * board has an active-low LED on PA7, so driving the pin low turns it on.
 */
#define LED_PORT GPIOA
#define LED_PIN GPIO7

int main(void) {
	/*
	 * 60 MHz from the PLL: the same table-driven style as CH32V0,
	 * and it leaves the frequency in rcc_sysclk_frequency.
	 */
	rcc_clock_setup_pll(&rcc_sysclk_configs[RCC_SYSCLK_PLL_60MHZ]);

	/* Drive the LED pin push-pull. */
	gpio_set_mode(LED_PORT, GPIO_MODE_OUTPUT_PP_5MA, LED_PIN);

	/* Active low: clear the pin to turn the LED on immediately. */
	gpio_clear(LED_PORT, LED_PIN);

	/* Use SysTick as the time base: HCLK clocks the counter directly. */
	qingke_systick_setup_hclk(rcc_sysclk_frequency);

	for (;;) {
		gpio_clear(LED_PORT, LED_PIN); /* on  */
		qingke_delay_ms(250);
		gpio_set(LED_PORT, LED_PIN); /* off */
		qingke_delay_ms(250);
	}

	return 0;
}
