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
 * CH32V003 DMA memory-to-memory self-test.
 *
 * DMA1 channel 1 copies a block from one SRAM buffer to another in MEM2MEM
 * mode.  Both 8-bit and 32-bit transfer sizes are exercised, the transfer
 * number is checked, and every byte/word in the destination is compared with
 * the source.  Progress and PASS/FAIL results are printed on USART1.
 *
 * Wiring: PD5 = USART1 TX, 115200 8N1, partial remap 1.
 */

#include <stdbool.h>
#include <stdint.h>

#include <libopenwch/ch32v0/dma.h>
#include <libopenwch/ch32v0/gpio.h>
#include <libopenwch/ch32v0/rcc.h>
#include <libopenwch/ch32v0/usart.h>
#include <libopenwch/qingke/systick.h>

#define UART_BAUD 115200u
#define DMA_CHANNEL DMA_CHANNEL1
#define DMA_TIMEOUT 1000000u

#define DMA8_COUNT 32u
#define DMA32_COUNT 32u

static volatile uint8_t dma8_src[DMA8_COUNT];
static volatile uint8_t dma8_dst[DMA8_COUNT];
static volatile uint32_t dma32_src[DMA32_COUNT];
static volatile uint32_t dma32_dst[DMA32_COUNT];

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

static void delay_init(void) {
	qingke_systick_set_frequency(rcc_sysclk_frequency);
	systick_set_clock_source(1); /* run from HCLK */
	systick_clear_interrupt();
	systick_enable_counter();
}

static void console_hex32(uint32_t value) {
	uart_puts("0x");
	uart_puthex(value, 8u);
}

/**
 * Run one MEM2MEM transfer.
 *
 * The CH32V003 DMA controller moves from PADDR to MADDR in MEM2MEM mode, so
 * PADDR is the source and MADDR is the destination here.  The peripheral-side
 * address is incremented (source) and the memory-side address is incremented
 * (destination).
 *
 * @return true when the transfer-complete flag appeared and CNTR reached zero.
 */
static bool dma_mem2mem_copy(uint32_t source,
			     uint32_t destination,
			     uint16_t count,
			     uint32_t size) {
	uint32_t timeout;

	rcc_periph_clock_enable(RCC_DMA1);

	dma_channel_reset(DMA1, DMA_CHANNEL);
	dma_set_peripheral_address(DMA1, DMA_CHANNEL, source);
	dma_set_memory_address(DMA1, DMA_CHANNEL, destination);
	dma_set_number_of_data(DMA1, DMA_CHANNEL, count);
	dma_enable_peripheral_increment_mode(DMA1, DMA_CHANNEL);
	dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL);
	dma_set_peripheral_size(DMA1, DMA_CHANNEL, size);
	dma_set_memory_size(DMA1, DMA_CHANNEL, size);
	dma_set_priority(DMA1, DMA_CHANNEL, DMA_PRIORITY_HIGH);
	dma_enable_mem2mem_mode(DMA1, DMA_CHANNEL);
	dma_clear_flag(DMA1, DMA_CHANNEL, DMA_TCIF);
	dma_channel_enable(DMA1, DMA_CHANNEL);

	for (timeout = 0; timeout < DMA_TIMEOUT; timeout++) {
		if (dma_get_flag(DMA1, DMA_CHANNEL, DMA_TCIF) != 0u) {
			break;
		}
	}

	dma_channel_disable(DMA1, DMA_CHANNEL);
	dma_clear_flag(DMA1, DMA_CHANNEL, DMA_TCIF);

	if (timeout == DMA_TIMEOUT) {
		return false;
	}

	return dma_get_number_of_data(DMA1, DMA_CHANNEL) == 0u;
}

static bool dma8_test(uint32_t round) {
	uint32_t i;
	bool ok;

	for (i = 0; i < DMA8_COUNT; i++) {
		dma8_src[i] = (uint8_t)(0xa5u ^ (uint8_t)i ^ (uint8_t)round);
		dma8_dst[i] = 0u;
	}

	ok = dma_mem2mem_copy((uint32_t)dma8_src, (uint32_t)dma8_dst,
			      DMA8_COUNT, DMA_SIZE_8BIT);

	uart_puts("DMA mem2mem 8-bit : ");
	if (!ok) {
		uart_puts("FAIL (timeout or count)\r\n");
		return false;
	}

	for (i = 0; i < DMA8_COUNT; i++) {
		if (dma8_dst[i] != dma8_src[i]) {
			uart_puts("FAIL at byte ");
			uart_putu(i);
			uart_puts(": ");
			console_hex32(dma8_dst[i]);
			uart_puts(" != ");
			console_hex32(dma8_src[i]);
			uart_puts("\r\n");
			return false;
		}
	}

	uart_puts("PASS (");
	uart_putu(DMA8_COUNT);
	uart_puts(" bytes)\r\n");
	return true;
}

static bool dma32_test(uint32_t round) {
	uint32_t i;
	bool ok;

	for (i = 0; i < DMA32_COUNT; i++) {
		dma32_src[i] = 0x5a5a0000u ^ i ^ (round * 0x01010101u);
		dma32_dst[i] = 0u;
	}

	ok = dma_mem2mem_copy((uint32_t)dma32_src, (uint32_t)dma32_dst,
			      DMA32_COUNT, DMA_SIZE_32BIT);

	uart_puts("DMA mem2mem 32-bit: ");
	if (!ok) {
		uart_puts("FAIL (timeout or count)\r\n");
		return false;
	}

	for (i = 0; i < DMA32_COUNT; i++) {
		if (dma32_dst[i] != dma32_src[i]) {
			uart_puts("FAIL at word ");
			uart_putu(i);
			uart_puts(": ");
			console_hex32(dma32_dst[i]);
			uart_puts(" != ");
			console_hex32(dma32_src[i]);
			uart_puts("\r\n");
			return false;
		}
	}

	uart_puts("PASS (");
	uart_putu(DMA32_COUNT);
	uart_puts(" words)\r\n");
	return true;
}

int main(void) {
	uint32_t round = 0;

	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_PLL_HSI_48MHZ]);

	uart_init();
	delay_init();

	uart_puts("\r\nlibopenwch DMA mem2mem self-test\r\n");
	uart_puts("DMA1 channel 1, 8-bit and 32-bit SRAM transfers\r\n");

	for (;;) {
		round++;

		uart_puts("round ");
		uart_putu(round);
		uart_puts("\r\n");

		(void)dma8_test(round);
		(void)dma32_test(round);

		qingke_delay_ms(1000u);
	}

	/* Not reached. */
	return 0;
}
