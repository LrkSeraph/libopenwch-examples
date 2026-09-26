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
 * CH32V003 watchdog self-test.
 *
 * The test is a two-stage state machine that deliberately resets the part:
 *
 *   1. enable IWDG, do not reload it, wait for the IWDG reset;
 *   2. verify RCC_RSTSCKR_IWDGRSTF, then enable WWDG, do not refresh it,
 *      and wait for the WWDG reset;
 *   3. verify RCC_RSTSCKR_WWDGRSTF and print the final result.
 *
 * The state is kept in the .noinit section so it survives watchdog resets.
 * On a power-on, pin, software, or low-power reset the test restarts from
 * stage 0; RCC reset flags are cleared before each stage so the next reset
 * source is unambiguous.
 *
 * Wiring: PD5 = USART1 TX, 115200 8N1, partial remap 1.
 */

#include <stdbool.h>
#include <stdint.h>

#include <libopenwch/ch32v0/gpio.h>
#include <libopenwch/ch32v0/iwdg.h>
#include <libopenwch/ch32v0/rcc.h>
#include <libopenwch/ch32v0/usart.h>
#include <libopenwch/ch32v0/wwdg.h>

#define UART_BAUD 115200u

#define WD_MAGIC 0x57444754u /* "WDGT" */

#define IWDG_PRESCALER IWDG_PSCR_DIV128
#define IWDG_RELOAD 500u /* approximately 1.6 s at 40 kHz LSI */

#define WWDG_COUNTER 0x7fu
#define WWDG_WINDOW 0x40u
#define WWDG_PRESCALER WWDG_PRESCALER_8

struct wd_state {
	uint32_t magic;
	uint32_t magic_inv;
	uint32_t stage;
	uint32_t iwdg_passes;
	uint32_t wwdg_passes;
};

/* Survives system resets; the reset path only clears .bss, not .noinit. */
static volatile struct wd_state wd_state __attribute__((section(".noinit")));

static void uart_init(void) {
	rcc_periph_clock_enable(RCC_USART1);
	rcc_periph_clock_enable(RCC_AFIO);
	rcc_periph_clock_enable(RCC_GPIOD);

	/* USART1 partial remap 1: TX=PD5, RX=PD6. */
	gpio_usart1_remap(GPIO_REMAP_USART1_PARTIAL1);
	gpio_set_mode(GPIOD, GPIO_MODE_AF_PP, GPIO5);

	usart_set_baudrate(USART1, UART_BAUD);
	usart_set_databits(USART1, 8);
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_mode(USART1, USART_MODE_TX);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
	usart_enable(USART1);
}

static void uart_puts(const char *s) {
	while (*s != '\0') {
		usart_send_blocking(USART1, (uint8_t)*s++);
	}
}

static void uart_putu(uint32_t value) {
	char buf[10];
	unsigned i = 0;

	if (value == 0u) {
		usart_send_blocking(USART1, '0');
		return;
	}

	while (value != 0u && i < sizeof(buf)) {
		buf[i++] = (char)('0' + (value % 10u));
		value /= 10u;
	}

	while (i > 0u) {
		usart_send_blocking(USART1, (uint8_t)buf[--i]);
	}
}

static void uart_puthex(uint32_t value, unsigned digits) {
	static const char hex[] = "0123456789abcdef";
	unsigned shift = digits * 4u;

	while (shift != 0u) {
		shift -= 4u;
		usart_send_blocking(USART1,
				    (uint8_t)hex[(value >> shift) & 0xfu]);
	}
}

static bool wd_state_valid(void) {
	return wd_state.magic == WD_MAGIC && wd_state.magic_inv == ~WD_MAGIC;
}

static void wd_state_reset(void) {
	wd_state.magic = WD_MAGIC;
	wd_state.magic_inv = ~WD_MAGIC;
	wd_state.stage = 0u;
	wd_state.iwdg_passes = 0u;
	wd_state.wwdg_passes = 0u;
}

static void print_reset_flags(uint32_t flags) {
	uart_puts("reset flags = 0x");
	uart_puthex(flags, 8u);

	if ((flags & RCC_RSTSCKR_PINRSTF) != 0u) {
		uart_puts(" PIN");
	}
	if ((flags & RCC_RSTSCKR_PORRSTF) != 0u) {
		uart_puts(" POR");
	}
	if ((flags & RCC_RSTSCKR_SFTRSTF) != 0u) {
		uart_puts(" SOFTWARE");
	}
	if ((flags & RCC_RSTSCKR_IWDGRSTF) != 0u) {
		uart_puts(" IWDG");
	}
	if ((flags & RCC_RSTSCKR_WWDGRSTF) != 0u) {
		uart_puts(" WWDG");
	}
	if ((flags & RCC_RSTSCKR_LPWRRSTF) != 0u) {
		uart_puts(" LOW_POWER");
	}

	uart_puts("\r\n");
}

static void start_iwdg(void) {
	uint32_t timeout;

	uart_puts("IWDG: enable and never reload; reset expected\r\n");

	iwdg_write_access_enable(IWDG);
	iwdg_set_prescaler(IWDG, IWDG_PRESCALER);

	for (timeout = 0;
	     timeout < 1000000u && iwdg_get_flag(IWDG, IWDG_FLAG_PVU) != 0u;
	     timeout++) {
		;
	}

	iwdg_set_reload(IWDG, IWDG_RELOAD);

	for (timeout = 0;
	     timeout < 1000000u && iwdg_get_flag(IWDG, IWDG_FLAG_RVU) != 0u;
	     timeout++) {
		;
	}

	iwdg_reload_counter(IWDG);
	iwdg_enable(IWDG);

	/* IWDG enable starts LSI if it was off. */
	for (timeout = 0;
	     timeout < 1000000u && (RCC_RSTSCKR & RCC_RSTSCKR_LSIRDY) == 0u;
	     timeout++) {
		;
	}

	for (;;) {
		;
	}
}

static void start_wwdg(void) {
	uart_puts("WWDG: enable and never refresh; reset expected\r\n");

	rcc_periph_clock_enable(RCC_WWDG);

	wwdg_set_counter(WWDG, WWDG_COUNTER);
	wwdg_set_prescaler(WWDG, WWDG_PRESCALER);
	wwdg_set_window(WWDG, WWDG_WINDOW);
	wwdg_clear_flag(WWDG);
	wwdg_enable(WWDG, WWDG_COUNTER);

	for (;;) {
		;
	}
}

int main(void) {
	uint32_t flags;

	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_PLL_HSI_48MHZ]);

	uart_init();

	flags = rcc_get_reset_flags();

	if (!wd_state_valid() ||
	    (flags & (RCC_RSTSCKR_PINRSTF | RCC_RSTSCKR_PORRSTF |
		      RCC_RSTSCKR_SFTRSTF | RCC_RSTSCKR_LPWRRSTF)) != 0u) {
		wd_state_reset();
		rcc_clear_reset_flags();
		flags = 0u;
	}

	uart_puts("\r\nlibopenwch watchdog self-test\r\n");
	print_reset_flags(flags);
	uart_puts("stage = ");
	uart_putu(wd_state.stage);
	uart_puts("\r\n");

	if (wd_state.stage == 0u) {
		wd_state.stage = 1u;
		start_iwdg();
	}

	if (wd_state.stage == 1u) {
		if ((flags & RCC_RSTSCKR_IWDGRSTF) == 0u) {
			uart_puts("IWDG: FAIL (unexpected reset flags)\r\n");
			wd_state_reset();
			wd_state.stage = 1u;
			start_iwdg();
		}

		wd_state.iwdg_passes++;
		uart_puts("IWDG: PASS\r\n");
		rcc_clear_reset_flags();
		wd_state.stage = 2u;
		start_wwdg();
	}

	if (wd_state.stage == 2u) {
		if ((flags & RCC_RSTSCKR_WWDGRSTF) == 0u) {
			uart_puts("WWDG: FAIL (unexpected reset flags)\r\n");
			wd_state_reset();
			wd_state.stage = 1u;
			start_iwdg();
		}

		wd_state.wwdg_passes++;
		uart_puts("WWDG: PASS\r\n");
		rcc_clear_reset_flags();
		wd_state.stage = 3u;
		uart_puts("watchdog self-test: PASS\r\n");
		uart_puts("IWDG passes = ");
		uart_putu(wd_state.iwdg_passes);
		uart_puts(", WWDG passes = ");
		uart_putu(wd_state.wwdg_passes);
		uart_puts("\r\n");
	}

	for (;;) {
		;
	}

	/* Not reached. */
	return 0;
}
