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
 * CH582 breathing-light PWM example on PA7.
 *
 * PA7 is PWM5 in the PWMX alternate pin group.  Enabling
 * GPIO_REMAP_PWMX moves PWM4/PWM5/PWM7/PWM8/PWM9 from
 * PA12/PA13/PB4/PB6/PB7 to PA6/PA7/PB1/PB2/PB3.
 *
 * The LED is active low, so the channel is configured with
 * PWM_POLARITY_ACTIVE_LOW and the duty value directly represents the
 * low-time (brightness).
 *
 * Wiring:
 *   PA7 = PWM5 output / active-low LED
 */

#include <stdbool.h>
#include <stdint.h>

#include <libopenwch/ch5xx58x/gpio.h>
#include <libopenwch/ch5xx58x/pwm.h>
#include <libopenwch/ch5xx58x/pwr.h>
#include <libopenwch/ch5xx58x/rcc.h>
#include <libopenwch/qingke/systick.h>

#define PWM_PIN_PORT GPIOA
#define PWM_PIN GPIO7
#define PWM_CHANNEL PWM_CH5

#define PWM_PERIOD_CLOCKS 256u
#define PWM_FREQUENCY_HZ 1000u
#define PWM_MAX_BRIGHTNESS 255u

#define BREATH_STEPS 32u
#define BREATH_DELAY_MS 25u /* about 0.8 s up + 0.8 s down */
#define STARTUP_FULL_ON_MS 1000u

/*
 * Raised-cosine brightness curve, 0 at index 0 and 255 at index 32.
 * It is smooth at both ends, which makes the LED breathe instead of
 * looking like a linear ramp.
 */
static const uint8_t breath_table[BREATH_STEPS + 1u] = {
    0,	 1,   2,   5,	10,  15,  21,  29,  37,	 47,  57,
    67,	 79,  90,  103, 115, 127, 140, 152, 165, 176, 188,
    198, 208, 218, 226, 234, 240, 245, 250, 253, 254, 255};

static void pwm_init(void) {
	uint32_t target_clocks;
	uint32_t divider;

	pwr_periph_clock_enable(PWR_CLK_PWMX);

	/* PA7 = PWM5 alternate output. */
	gpio_pin_remap(GPIO_REMAP_PWMX);
	gpio_set_mode(PWM_PIN_PORT, GPIO_MODE_OUTPUT_PP_5MA, PWM_PIN);

	/*
 * PWMX cycle = (PWM_CLOCK_DIV + 1) * Tsys * PWM_PERIOD_CLOCKS.
 * Pick the divider that gets closest to PWM_FREQUENCY_HZ without
 * overflowing the 8-bit divider register.
 */
	target_clocks = PWM_FREQUENCY_HZ * PWM_PERIOD_CLOCKS;
	divider = (rcc_sysclk_frequency + target_clocks - 1u) / target_clocks;

	if (divider == 0u) {
		divider = 1u;
	}

	if (divider > (PWM_MAX_BRIGHTNESS + 1u)) {
		divider = PWM_MAX_BRIGHTNESS + 1u;
	}

	pwm_set_clock_divider(PWMX, divider - 1u);
	pwm_set_cycle(PWMX, PWM_CYCLE_256);

	/* Active-low LED: data value is the low-time, i.e. brightness. */
	pwm_set_polarity(PWMX, PWM_CHANNEL, PWM_POLARITY_ACTIVE_LOW);
	pwm_set_channel(PWMX, PWM_CHANNEL, 0u);
	pwm_enable_channel(PWMX, PWM_CHANNEL);
}

static void pwm_set_brightness(uint8_t brightness) {
	pwm_set_channel(PWMX, PWM_CHANNEL, brightness);
}

int main(void) {
	uint32_t phase = 0u;
	bool increasing = true;

	rcc_clock_setup_pll(&rcc_sysclk_configs[RCC_SYSCLK_PLL_60MHZ]);
	qingke_systick_setup_hclk(rcc_sysclk_frequency);

	pwm_init();

	/* Startup check: force the LED fully on for one second. */
	pwm_set_brightness(PWM_MAX_BRIGHTNESS);
	qingke_delay_ms(STARTUP_FULL_ON_MS);

	for (;;) {
		pwm_set_brightness(breath_table[phase]);
		qingke_delay_ms(BREATH_DELAY_MS);

		if (increasing) {
			if (phase >= BREATH_STEPS) {
				increasing = false;
				phase = BREATH_STEPS - 1u;
			} else {
				phase++;
			}
		} else {
			if (phase == 0u) {
				increasing = true;
				phase = 1u;
			} else {
				phase--;
			}
		}
	}

	/* Not reached. */
	return 0;
}
