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
 * SPI NOR flash CRC32 test for a CH32V003.
 *
 * The hardware SPI1 bus is on the default pins PA5 (SCK), PA6 (MISO) and
 * PA7 (MOSI).  A GPIO acts as chip select; PA4 is used by default and can be
 * changed by editing FLASH_CS_PORT/FLASH_CS_PIN below.
 *
 * The firmware reads FLASH_PAGE_COUNT 4 KiB pages from address zero and prints
 * the CRC-32 of each page on USART1.  The CRC uses GCC's CRC32 builtins:
 *
 *     poly   = 0x04c11db7 (normal form)
 *     init   = 0xffffffff
 *     refin  = true
 *     refout = true
 *     xorout = 0xffffffff
 *
 * This is the usual CRC-32/ISO-HDLC ("Ethernet") used for flash images;
 * the standard check value for "123456789" is 0xcbf43926.
 *
 * Wiring:
 *   PA4 = CS   (software, active low)
 *   PA5 = SCK
 *   PA6 = MISO
 *   PA7 = MOSI
 *   PD5 = USART1 TX, 115200 8N1
 *
 * Override the number of pages with -DFLASH_PAGE_COUNT=...
 */

#include <libopenwch/ch32v0/gpio.h>
#include <libopenwch/ch32v0/rcc.h>
#include <libopenwch/ch32v0/spi.h>
#include <libopenwch/ch32v0/usart.h>

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if !__has_builtin(__builtin_rev_crc32_data8)
#error "This example needs GCC's CRC32 builtins"
#endif

#define UART_BAUD 115200u

#define FLASH_CS_PORT GPIOA
#define FLASH_CS_PIN GPIO4

#define FLASH_PAGE_SIZE 4096u
#define FLASH_CHUNK_SIZE 128u
#define FLASH_BASE_ADDR 0u

#ifndef FLASH_PAGE_COUNT
#define FLASH_PAGE_COUNT 16u
#endif

#define SPI_FLASH_CMD_READ 0x03u
#define SPI_FLASH_CMD_RDID 0x9fu

#define CRC32_POLYNOMIAL 0x04c11db7u
#define CRC32_INIT 0xffffffffu
#define CRC32_XOROUT 0xffffffffu

static uint8_t flash_buffer[FLASH_CHUNK_SIZE];

static void console_init(void) {
	rcc_periph_clock_enable(RCC_USART1);
	rcc_periph_clock_enable(RCC_AFIO);
	rcc_periph_clock_enable(RCC_GPIOD);

	/* USART1 partial remap 1: TX = PD5. */
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

static void spi_flash_cs_low(void) {
	gpio_clear(FLASH_CS_PORT, FLASH_CS_PIN);
}

static void spi_flash_cs_high(void) {
	gpio_set(FLASH_CS_PORT, FLASH_CS_PIN);
}

static uint8_t spi_flash_xfer(uint8_t value) {
	return (uint8_t)spi_xfer(SPI1, value);
}

static void spi_flash_init(void) {
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_SPI1);

	/* PA4 is a GPIO chip select, not the SPI NSS alternate function. */
	gpio_set_mode(FLASH_CS_PORT, GPIO_MODE_OUT_PP, FLASH_CS_PIN);
	spi_flash_cs_high();

	/* SCK and MOSI are outputs; MISO is an input. */
	gpio_set_mode(GPIOA, GPIO_MODE_AF_PP, GPIO5 | GPIO7);
	gpio_set_mode(GPIOA, GPIO_MODE_IN_FLOATING, GPIO6);

	/* Mode 0, 8-bit, MSB first, 48 MHz / 8 = 6 MHz SCK. */
	spi_init_master(SPI1, SPI_BAUDRATE_PRESCALER_8, SPI_CPOL_LOW,
			SPI_CPHA_FIRST, SPI_DFF_8BIT, SPI_BIT_ORDER_MSB_FIRST);
	spi_enable_software_slave_management(SPI1);
	spi_set_nss_high(SPI1);
	spi_enable(SPI1);
}

static uint32_t spi_flash_read_id(void) {
	uint8_t manufacturer;
	uint8_t memory_type;
	uint8_t capacity;

	spi_flash_cs_low();
	(void)spi_flash_xfer(SPI_FLASH_CMD_RDID);
	manufacturer = spi_flash_xfer(0xffu);
	memory_type = spi_flash_xfer(0xffu);
	capacity = spi_flash_xfer(0xffu);
	spi_flash_cs_high();

	return ((uint32_t)manufacturer << 16) | ((uint32_t)memory_type << 8) |
	       (uint32_t)capacity;
}

static void spi_flash_read(uint32_t address, uint8_t *out, uint32_t length) {
	uint32_t i;

	spi_flash_cs_low();
	(void)spi_flash_xfer(SPI_FLASH_CMD_READ);
	(void)spi_flash_xfer((uint8_t)(address >> 16));
	(void)spi_flash_xfer((uint8_t)(address >> 8));
	(void)spi_flash_xfer((uint8_t)address);

	for (i = 0; i < length; i++) {
		out[i] = spi_flash_xfer(0xffu);
	}

	spi_flash_cs_high();
}

static uint32_t
crc32_update(uint32_t crc, const uint8_t *data, uint32_t length) {
	while (length-- != 0u) {
		crc = __builtin_rev_crc32_data8(crc, *data++, CRC32_POLYNOMIAL);
	}

	return crc;
}

static uint32_t crc32_page(uint32_t address) {
	uint32_t crc = CRC32_INIT;
	uint32_t offset;

	for (offset = 0u; offset < FLASH_PAGE_SIZE;
	     offset += FLASH_CHUNK_SIZE) {
		uint32_t count = FLASH_PAGE_SIZE - offset;

		if (count > FLASH_CHUNK_SIZE) {
			count = FLASH_CHUNK_SIZE;
		}

		spi_flash_read(address + offset, flash_buffer, count);
		crc = crc32_update(crc, flash_buffer, count);
	}

	return crc ^ CRC32_XOROUT;
}

int main(void) {
	uint32_t page;

	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_PLL_HSI_48MHZ]);

	console_init();
	spi_flash_init();

	console_puts("\r\nlibopenwch SPI NOR CRC32 test\r\n");
	console_puts("jedec id = 0x");
	console_puthex(spi_flash_read_id(), 6u);
	console_puts("\r\n");

	for (page = 0u; page < FLASH_PAGE_COUNT; page++) {
		uint32_t address = FLASH_BASE_ADDR + page * FLASH_PAGE_SIZE;
		uint32_t crc = crc32_page(address);

		console_puts("page ");
		console_putu(page);
		console_puts(" @0x");
		console_puthex(address, 6u);
		console_puts(" crc32 = 0x");
		console_puthex(crc, 8u);
		console_puts("\r\n");
	}

	console_puts("done\r\n");

	for (;;) {
		;
	}

	/* Not reached. */
	return 0;
}
