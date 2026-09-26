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
 * USART counter for a CH32V003 board.
 *
 * Brings the system up on the 48 MHz HSI PLL, prints basic hardware
 * information (chip identifier and system clock), then sends an incrementing
 * counter once per second on USART1.
 *
 * Wiring: PD5 = TX, 115200 8N1, partial remap 1 (the WCH EVT board pin).
 */

#include <libopenwch/ch32v0/dbgmcu.h>
#include <libopenwch/ch32v0/gpio.h>
#include <libopenwch/ch32v0/rcc.h>
#include <libopenwch/ch32v0/usart.h>
#include <libopenwch/qingke/systick.h>

#define BAUD 115200u

/*
 * Which TX pin to use.
 *   0 - PD5, no remap          <- evaluation board default
 *   1 - PD0, partial remap 1
 *   2 - PD6, partial remap 2
 *   3 - PC0, full remap
 */
#define UART_PIN_MAPPING 0

static void console_init(void) {
	rcc_periph_clock_enable(RCC_USART1);
	rcc_periph_clock_enable(RCC_AFIO);

	usart_set_baudrate(USART1, BAUD);
	usart_set_databits(USART1, 8);
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_mode(USART1, USART_MODE_TX);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);

#if UART_PIN_MAPPING == 0
	rcc_periph_clock_enable(RCC_GPIOD);
	gpio_set_mode(GPIOD, GPIO_MODE_AF_PP, GPIO5);
#elif UART_PIN_MAPPING == 1
	gpio_usart1_remap(GPIO_REMAP_USART1_PARTIAL1);
	rcc_periph_clock_enable(RCC_GPIOD);
	gpio_set_mode(GPIOD, GPIO_MODE_AF_PP, GPIO0);
#elif UART_PIN_MAPPING == 2
	gpio_usart1_remap(GPIO_REMAP_USART1_PARTIAL2);
	rcc_periph_clock_enable(RCC_GPIOD);
	gpio_set_mode(GPIOD, GPIO_MODE_AF_PP, GPIO6);
#elif UART_PIN_MAPPING == 3
	gpio_usart1_remap(GPIO_REMAP_USART1_FULL);
	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_mode(GPIOC, GPIO_MODE_AF_PP, GPIO0);
#else
#error "invalid UART_PIN_MAPPING"
#endif

	usart_enable(USART1);
}

static void console_puts(const char *s) {
	while (*s != '\0') {
		usart_send_blocking(USART1, (uint8_t)*s++);
	}
}

static void console_putu(uint32_t value) {
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

static void console_puthex(uint32_t value, unsigned digits) {
	static const char hex[] = "0123456789abcdef";
	unsigned shift = digits * 4u;

	while (shift != 0u) {
		unsigned nibble;

		shift -= 4u;
		nibble = (value >> shift) & 0xfu;
		usart_send_blocking(USART1, (uint8_t)hex[nibble]);
	}
}

static void print_hardware_info(void) {
	uint32_t id = ((uint32_t)dbgmcu_get_revision_id() << 16) |
		      (uint32_t)dbgmcu_get_device_id();

	console_puts("chip id = 0x");
	console_puthex(id, 8u);
	console_puts("\r\nsysclk = ");
	console_putu(rcc_sysclk_frequency);
	console_puts(" Hz\r\n");
}

int main(void) {
	uint32_t count = 0;

	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_PLL_HSI_48MHZ]);

	console_init();
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
