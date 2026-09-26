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
 * UART counter for a CH58x board.
 *
 * Brings the crystal up, runs at 60 MHz, prints the chip id and system clock,
 * then sends an incrementing counter once per second on UART1.
 *
 * Wiring: PA8 = TX, 115200 8N1.
 */

#include <libopenwch/ch5xx58x/clk.h>
#include <libopenwch/ch5xx58x/gpio.h>
#include <libopenwch/ch5xx58x/sys.h>
#include <libopenwch/ch5xx58x/uart.h>
#include <libopenwch/qingke/systick.h>

#define BAUD 115200u

static void console_putu(uint32_t value) {
	char buf[10];
	unsigned i = 0;

	if (value == 0u) {
		uart_send_blocking(UART1, '0');
		return;
	}

	while (value != 0u && i < sizeof(buf)) {
		buf[i++] = (char)('0' + (value % 10u));
		value /= 10u;
	}

	while (i > 0u) {
		uart_send_blocking(UART1, (uint8_t)buf[--i]);
	}
}

static void console_puthex(uint32_t value, unsigned digits) {
	static const char hex[] = "0123456789abcdef";
	unsigned shift = digits * 4u;

	while (shift != 0u) {
		unsigned nibble;

		shift -= 4u;
		nibble = (value >> shift) & 0xfu;
		uart_send_blocking(UART1, (uint8_t)hex[nibble]);
	}
}

static void console_puts(const char *s) {
	while (*s != '\0') {
		uart_send_blocking(UART1, (uint8_t)*s++);
	}
}

static void print_hardware_info(void) {
	console_puts("chip id = 0x");
	console_puthex((uint32_t)sys_get_chip_id(), 2u);
	console_puts("\r\nsysclk = ");
	console_putu(rcc_sysclk_frequency);
	console_puts(" Hz\r\n");
}

int main(void) {
	uint32_t count = 0;

	/* 60 MHz from the PLL.  This powers XT32M first. */
	clk_set_sys_clock(CLK_SOURCE_PLL_60MHZ);

	/* TX is PA8, push-pull; this example does not receive. */
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_PP_5MA, GPIO8);

	uart_set_baudrate(UART1, BAUD);
	uart_set_databits(UART1, UART_DATA_8BITS);
	uart_set_stopbits(UART1, UART_STOPBITS_1);
	uart_set_parity(UART1, UART_PARITY_NONE);
	uart_enable(UART1);

	print_hardware_info();

	qingke_systick_set_frequency(rcc_sysclk_frequency);
	systick_set_clock_source(1); /* run from HCLK */
	systick_clear_interrupt();
	systick_enable_counter();

	console_puts("uart counter started\r\n");

	for (;;) {
		qingke_delay_ms(1000u);
		count++;
		console_puts("count = ");
		console_putu(count);
		console_puts("\r\n");
	}

	/* Not reached. */
	return 0;
}
