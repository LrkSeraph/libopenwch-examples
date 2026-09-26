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
 * UART counter — a minimal transmit-only test firmware.
 *
 * It runs at 48 MHz and prints an increasing counter on USART1 at 115200 8N1,
 * one line about every 500 ms.  No input is read, no interrupt is used, and
 * nothing depends on printf or the C library.
 *
 * Wiring: PD5 = USART1 TX, the "partial remap 1" pin used by the
 * WCH CH32V003F4P6-EVT-R0 board.  Connect PD5 to the RX input of the
 * WCH-LinkE USB serial bridge, then:
 *
 *screen /dev/ttyACM0 115200
 *
 * or, for scripts:
 *
 *stty -F /dev/ttyACM0 115200 raw -echo && cat /dev/ttyACM0
 */

#include <libopenwch/ch32v0/gpio.h>
#include <libopenwch/ch32v0/rcc.h>
#include <libopenwch/ch32v0/usart.h>
#include <libopenwch/qingke/systick.h>

#define BAUD 115200

/*
 * Which pin mapping to use.
 *   0 - PA9 (TX) / PA10 (RX), no remap
 *   1 - PD5 (TX) / PD6 (RX), partial remap 1   <- evaluation board default
 *   2 - PD0 (TX) / PD1 (RX), partial remap 2
 *   3 - PD6 (TX) / PD5 (RX), full remap
 *
 * Only TX is configured; the counter firmware never receives.
 */
#define UART_PIN_MAPPING 1

static void console_init(void) {
	rcc_periph_clock_enable(RCC_USART1);
	rcc_periph_clock_enable(RCC_AFIO);

	usart_set_baudrate(USART1, BAUD);
	usart_set_databits(USART1, 8);
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_mode(USART1, USART_MODE_TX);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);

#if UART_PIN_MAPPING == 1
	gpio_primary_remap(GPIO_REMAP_USART1_PARTIAL1);
	rcc_periph_clock_enable(RCC_GPIOD);
	gpio_set_mode(GPIOD, GPIO_MODE_AF_PP, GPIO5); /* TX */
#elif UART_PIN_MAPPING == 2
	gpio_primary_remap(GPIO_REMAP_USART1_PARTIAL2);
	rcc_periph_clock_enable(RCC_GPIOD);
	gpio_set_mode(GPIOD, GPIO_MODE_AF_PP, GPIO0); /* TX */
#elif UART_PIN_MAPPING == 3
	gpio_primary_remap(GPIO_REMAP_USART1_FULL);
	rcc_periph_clock_enable(RCC_GPIOD);
	gpio_set_mode(GPIOD, GPIO_MODE_AF_PP, GPIO6); /* TX */
#else
	rcc_periph_clock_enable(RCC_GPIOA);
	gpio_set_mode(GPIOA, GPIO_MODE_AF_PP, GPIO9); /* TX */
#endif

	usart_enable(USART1);
}

static void console_puts(const char *s) {
	while (*s != '\0') {
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

	while (value != 0 && i < (int)sizeof(buf)) {
		buf[i++] = (char)('0' + (value % 10u));
		value /= 10u;
	}

	while (i-- > 0) {
		usart_send_blocking(USART1, (uint8_t)buf[i]);
	}
}

int main(void) {
	uint32_t count = 0;

	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_PLL_HSI_48MHZ]);

	/* Let the delay helper convert milliseconds to SysTick ticks. */
	qingke_systick_set_frequency(rcc_sysclk_frequency);
	systick_set_clock_source(1); /* HCLK, not the default HCLK/8 */

	console_init();

	console_puts("\r\nlibopenwch uart_counter\r\n");
	console_puts("sysclk = ");
	console_putu(rcc_sysclk_frequency);
	console_puts(" Hz, baud = ");
	console_putu(BAUD);
	console_puts("\r\n");

	for (;;) {
		console_putu(count++);
		console_puts("\r\n");
		qingke_delay_ms(500);
	}

	return 0;
}
