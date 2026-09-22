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
 * CH58x UART echo.
 *
 * Brings the crystal up, runs at 60 MHz and echoes bytes on UART1 with no
 * interrupts.  The baud divisor is derived from the live system clock, so the
 * rate stays correct whatever the PLL divider is set to -- which is the part
 * of this family that is easy to get wrong, because the divisor has to be
 * computed from a runtime frequency rather than a compile-time constant.
 *
 * Default pins: PA8 = TX, PA9 = RX.  Change UART_PIN_REMAP below to move them.
 */
#include <libopenwch/ch5xx58x/clk.h>
#include <libopenwch/ch5xx58x/gpio.h>
#include <libopenwch/ch5xx58x/uart.h>
#include <libopenwch/ch5xx58x/rwa.h>

#define BAUD 115200

/* Set to GPIO_REMAP_UART1 to move the port to the alternate pin pair. */
#define UART_PIN_REMAP 0

/**
 * Print an unsigned value in decimal without pulling in printf.
 */
static void console_putu(uint32_t value) {
	char buf[10];
	int i = 0;

	if (value == 0) {
		uart_send_blocking(UART1, '0');
		return;
	}

	while (value && i < (int)sizeof(buf)) {
		buf[i++] = (char)('0' + (value % 10u));
		value /= 10u;
	}

	while (i--) {
		uart_send_blocking(UART1, (uint8_t)buf[i]);
	}
}

static void console_puts(const char *s) {
	while (*s) {
		uart_send_blocking(UART1, (uint8_t)*s++);
	}
}

int main(void) {

	/* 60 MHz from the PLL.  This powers XT32M first. */
	clk_set_sys_clock(CLK_SOURCE_PLL_60MHZ);

	/*
	 * Pin function selection is two steps on this family: enable the remap
	 * bit (if any) and configure the pins.  There is no per-pin AF number.
	 */
	UART_PIN_REMAP ? gpio_pin_remap(UART_PIN_REMAP) : (void)0;

	/* TX is driven push-pull; RX is a floating input. */
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_PP_5MA, GPIO8);
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT_FLOAT, GPIO9);

	/* 8N1.  The baud rate divider comes from the published system clock. */
	uart_set_baudrate(UART1, BAUD);
	uart_set_databits(UART1, UART_DATA_8BITS);
	uart_set_stopbits(UART1, UART_STOPBITS_1);
	uart_set_parity(UART1, UART_PARITY_NONE);
	uart_enable(UART1);

	console_puts("\r\nlibopenwch ch582_uart_echo\r\nsysclk = ");
	console_putu(rcc_sysclk_frequency);
	console_puts(" Hz, baud = ");
	console_putu(BAUD);
	console_puts("\r\n");

	for (;;) {
		uint8_t c = uart_recv_blocking(UART1);

		uart_send_blocking(UART1, c);
		if (c == '\r') {
			uart_send_blocking(UART1, '\n');
		}
	}

	return 0;
}
