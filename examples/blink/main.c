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
 * Blink — the smallest useful libopenwch program.
 *
 * It brings the part up on the internal oscillator, runs at 48 MHz, and
 * blinks an active-low LED with a visible delay.  There is no SysTick setup
 * here on purpose: the delay loop is a plain busy-wait, so the example has no
 * dependencies beyond the RCC and GPIO drivers.
 *
 * Build:   make
 * Flash:   make flash          (needs minichlink on PATH)
 * Size:    make size
 */

#include <libopenwch/ch32v0/gpio.h>
#include <libopenwch/ch32v0/rcc.h>
#include <libopenwch/qingke/assert.h>

/*
 * Board wiring.  This board has the LED on PC2, between PC2 and the 3.3 V
 * rail, so driving PC2 low lights it.  Change these two lines for your own
 * board.
 */
#define LED_PORT GPIOC
#define LED_PIN GPIO2

/* Delay length, in loop iterations.  Roughly 250 ms at 48 MHz. */
#define DELAY_LOOPS 600000u

/** Busy-wait for a while.  `volatile` keeps the loop from being removed. */
static void delay(volatile uint32_t loops) {
	while (loops--) {
		__asm__ volatile("nop");
	}
}

int main(void) {
	/*
	 * Run at 48 MHz from the internal 24 MHz RC oscillator and its PLL:
	 * one call names the whole tree, and it leaves the resulting
	 * frequencies in the rcc_*_frequency variables.
	 */
	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_PLL_HSI_48MHZ]);

	/* Power the port before configuring it. */
	rcc_periph_clock_enable(RCC_GPIOC);

	/*
	 * The LED is active low, so drive the output latch high before the pin
	 * becomes an output.  This keeps the LED off during startup.
	 */
	gpio_set(LED_PORT, LED_PIN);

	/*
	 * Push-pull output.  The nibble is WCH's opaque pin configuration
	 * value; see include/libopenwch/ch32v0/common/gpio_common_v1.h for why
	 * it is not split into separate mode and configuration fields.
	 */
	gpio_set_mode(LED_PORT, GPIO_MODE_OUT_PP, LED_PIN);

	for (;;) {
		/* Active low: this toggles the LED between off and on. */
		gpio_toggle(LED_PORT, LED_PIN);
		delay(DELAY_LOOPS);
	}

	/* Not reached. */
	return 0;
}
