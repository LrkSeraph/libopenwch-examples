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
 * USART echo — a blocking, interrupt-free serial example.
 *
 * It runs at 48 MHz, prints a banner and then echoes every byte it receives.
 * You can watch it with minichlink's single-wire terminal:
 *
 *	make flash
 *	make monitor
 *
 * Wiring: PD5 = TX, PD6 = RX, 115200 8N1.  USART1's default pins are PA9/PA10;
 * the "partial remap 1" option below moves it to PD5/PD6, which is what the
 * WCH CH32V003F4P6-EVT-R0 board brings out.
 */

#include <libopenwch/ch32v0/gpio.h>
#include <libopenwch/ch32v0/rcc.h>
#include <libopenwch/ch32v0/usart.h>
#include <libopenwch/qingke/sync.h>

#define BAUD 115200

/*
 * Which pin mapping to use.
 *   0 - PA9 (TX) / PA10 (RX), no remap
 *   1 - PD5 (TX) / PD6 (RX), partial remap 1   <- evaluation board default
 *   2 - PD0 (TX) / PD1 (RX), partial remap 2
 *   3 - PD6 (TX) / PD5 (RX), full remap
 */
#define UART_PIN_MAPPING 1

static void console_init(void) {
	rcc_periph_clock_enable(RCC_USART1);
	rcc_periph_clock_enable(RCC_AFIO);

	/*
	 * 8 data bits, no parity, one stop bit, transmit and receive.  The
	 * baud rate needs the APB2 clock, which the RCC driver knows.
	 */
	usart_set_baudrate(USART1, BAUD);
	usart_set_databits(USART1, 8);
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_mode(USART1, USART_MODE_TX_RX);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);

#if UART_PIN_MAPPING == 1
	gpio_primary_remap(GPIO_REMAP_USART1_PARTIAL1);
	rcc_periph_clock_enable(RCC_GPIOD);
	gpio_set_mode(GPIOD, GPIO_MODE_AF_PP, GPIO5);	    /* TX */
	gpio_set_mode(GPIOD, GPIO_MODE_IN_FLOATING, GPIO6); /* RX */
#elif UART_PIN_MAPPING == 2
	gpio_primary_remap(GPIO_REMAP_USART1_PARTIAL2);
	rcc_periph_clock_enable(RCC_GPIOD);
	gpio_set_mode(GPIOD, GPIO_MODE_AF_PP, GPIO0);	    /* TX */
	gpio_set_mode(GPIOD, GPIO_MODE_IN_FLOATING, GPIO1); /* RX */
#elif UART_PIN_MAPPING == 3
	gpio_primary_remap(GPIO_REMAP_USART1_FULL);
	rcc_periph_clock_enable(RCC_GPIOD);
	gpio_set_mode(GPIOD, GPIO_MODE_AF_PP, GPIO6);	    /* TX */
	gpio_set_mode(GPIOD, GPIO_MODE_IN_FLOATING, GPIO5); /* RX */
#else
	rcc_periph_clock_enable(RCC_GPIOA);
	gpio_set_mode(GPIOA, GPIO_MODE_AF_PP, GPIO9);	     /* TX */
	gpio_set_mode(GPIOA, GPIO_MODE_IN_FLOATING, GPIO10); /* RX */
#endif

	usart_enable(USART1);
}

static void console_puts(const char *s) {
	while (*s) {
		usart_send_blocking(USART1, (uint8_t)*s++);
	}
}

/** Print an unsigned value in decimal without pulling in printf. */
static void console_putu(uint32_t value) {
	char buf[10];
	int i = 0;

	if (value == 0) {
		usart_send_blocking(USART1, '0');
		return;
	}

	while (value && i < (int)sizeof(buf)) {
		buf[i++] = (char)('0' + (value % 10u));
		value /= 10u;
	}

	while (i--) {
		usart_send_blocking(USART1, (uint8_t)buf[i]);
	}
}

int main(void) {
	uint32_t count = 0;

	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_PLL_HSI_48MHZ]);

	console_init();

	console_puts("\r\nlibopenwch uart_echo\r\n");
	console_puts("sysclk = ");
	console_putu(rcc_sysclk_frequency);
	console_puts(" Hz, baud = ");
	console_putu(BAUD);
	console_puts("\r\n");

	for (;;) {
		uint16_t c = usart_recv_blocking(USART1);

		/* Echo it back, and count lines so the echo is visible. */
		usart_send_blocking(USART1, c);
		if (c == '\r') {
			usart_send_blocking(USART1, '\n');
			console_puts("#");
			console_putu(++count);
			console_puts("\r\n");
		}
	}

	return 0;
}
