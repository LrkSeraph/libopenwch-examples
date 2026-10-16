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

#include <libopenwch/ch32v0/gpio.h>
#include <libopenwch/ch32v0/rcc.h>
#include <libopenwch/ch32v0/usart.h>

#include "selftest_console.h"

#pragma GCC optimize("Os")

#define CONSOLE_BAUD 115200u

void console_init(void) {
	rcc_periph_clock_enable(RCC_USART1);
	rcc_periph_clock_enable(RCC_AFIO);
	rcc_periph_clock_enable(RCC_GPIOD);

	/* USART1 default pins are TX=PD5, RX=PD6. */
	gpio_set_mode(GPIOD, GPIO_MODE_AF_PP, GPIO5);

	usart_set_baudrate(USART1, CONSOLE_BAUD);
	usart_set_databits(USART1, 8);
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_mode(USART1, USART_MODE_TX);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
	usart_enable(USART1);
}

void console_putc(char c) {
	usart_send_blocking(USART1, (uint8_t)c);
}

void console_puts(const char *s) {
	while (*s != '\0') {
		console_putc(*s++);
	}
}

void console_putu(uint32_t value) {
	char buf[10];
	unsigned int i = 0;

	if (value == 0u) {
		console_putc('0');
		return;
	}

	while (value != 0u && i < sizeof(buf)) {
		buf[i++] = (char)('0' + (value % 10u));
		value /= 10u;
	}

	while (i > 0u) {
		console_putc(buf[--i]);
	}
}

void console_puthex(uint32_t value, unsigned int digits) {
	static const char hex[] = "0123456789abcdef";
	unsigned int shift = digits * 4u;

	while (shift != 0u) {
		shift -= 4u;
		console_putc(hex[(value >> shift) & 0xfu]);
	}
}

void console_crlf(void) {
	console_putc('\r');
	console_putc('\n');
}
