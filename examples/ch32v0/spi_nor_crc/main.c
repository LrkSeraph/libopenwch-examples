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
 * The hardware SPI1 bus is remapped to PC5 (SCK), PC6 (MOSI) and PC7 (MISO).
 * A GPIO acts as chip select; PC4 is used by default and can be changed by
 * editing FLASH_CS_PORT/FLASH_CS_PIN below.
 *
 * The firmware probes the flash with JEDEC RDID first.  If no flash answers it
 * prints an explicit error and stops; otherwise it reports the ID, manufacturer,
 * capacity, SFDP presence and status register.  It then reads FLASH_PAGE_COUNT
 * 4 KiB pages from address zero and prints the CRC-32 of each page on USART1.
 * The CRC uses GCC's CRC32 builtins:
 *
 *     poly   = 0x04c11db7 (normal form)
 *     init   = 0xffffffff
 *     refin  = true
 *     refout = true
 *     xorout = 0xffffffff
 *
 * This is the usual CRC-32/ISO-HDLC ("Ethernet") used for flash images;
 * the standard check value for "123456789" is 0xcbf43926.  When GCC has
 * __builtin_rev_crc32_data8 it is used directly; older compilers get a small
 * table-free reflected software fallback with the same result.
 *
 * Wiring:
 *   PC4 = CS   (software, active low)
 *   PC5 = SCK
 *   PC6 = MOSI
 *   PC7 = MISO
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

#define UART_BAUD 115200u

#define FLASH_CS_PORT GPIOC
#define FLASH_CS_PIN GPIO4

#define FLASH_PAGE_SIZE 4096u
#define FLASH_CHUNK_SIZE 128u
#define FLASH_BASE_ADDR 0u

#ifndef FLASH_PAGE_COUNT
#define FLASH_PAGE_COUNT 16u
#endif

#define SPI_FLASH_CMD_READ 0x03u
#define SPI_FLASH_CMD_RDSR1 0x05u
#define SPI_FLASH_CMD_SFDP 0x5au
#define SPI_FLASH_CMD_RDID 0x9fu

#define CRC32_POLYNOMIAL 0x04c11db7u
#define CRC32_REFLECTED_POLYNOMIAL 0xedb88320u
#define CRC32_INIT 0xffffffffu
#define CRC32_XOROUT 0xffffffffu

#if __has_builtin(__builtin_rev_crc32_data8)
#define CRC32_UPDATE(crc, data)                                                \
	__builtin_rev_crc32_data8((crc), (data), CRC32_POLYNOMIAL)
#else
static uint32_t crc32_soft_update(uint32_t crc, uint8_t data) {
	unsigned i;

	crc ^= data;
	for (i = 0u; i < 8u; i++) {
		uint32_t mask = (crc & 1u) ? 0xffffffffu : 0u;

		crc = (crc >> 1) ^ (CRC32_REFLECTED_POLYNOMIAL & mask);
	}

	return crc;
}
#define CRC32_UPDATE(crc, data) crc32_soft_update((crc), (data))
#endif

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
	rcc_periph_clock_enable(RCC_AFIO);
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_SPI1);

	/* SPI1 remap: PC5 = SCK, PC6 = MOSI, PC7 = MISO. */
	gpio_primary_remap(GPIO_REMAP_SPI1);

	/* PC4 is a GPIO chip select, not the SPI NSS alternate function. */
	gpio_set_mode(FLASH_CS_PORT, GPIO_MODE_OUT_PP, FLASH_CS_PIN);
	spi_flash_cs_high();

	/* SCK and MOSI are outputs; MISO is an input. */
	gpio_set_mode(GPIOC, GPIO_MODE_AF_PP, GPIO5 | GPIO6);
	gpio_set_mode(GPIOC, GPIO_MODE_IN_FLOATING, GPIO7);

	/* Mode 0, 8-bit, MSB first, 48 MHz / 8 = 6 MHz SCK. */
	spi_init_master(SPI1, SPI_BAUDRATE_PRESCALER_8, SPI_CPOL_LOW,
			SPI_CPHA_FIRST, SPI_DFF_8BIT, SPI_BIT_ORDER_MSB_FIRST);
	spi_enable_software_slave_management(SPI1);
	spi_set_nss_high(SPI1);
	spi_enable(SPI1);
}

static bool spi_flash_read_id(uint8_t *manufacturer,
			      uint8_t *memory_type,
			      uint8_t *capacity) {
	spi_flash_cs_low();
	(void)spi_flash_xfer(SPI_FLASH_CMD_RDID);
	*manufacturer = spi_flash_xfer(0xffu);
	*memory_type = spi_flash_xfer(0xffu);
	*capacity = spi_flash_xfer(0xffu);
	spi_flash_cs_high();

	if (*manufacturer == 0xffu && *memory_type == 0xffu &&
	    *capacity == 0xffu) {
		return false;
	}

	if (*manufacturer == 0x00u && *memory_type == 0x00u &&
	    *capacity == 0x00u) {
		return false;
	}

	return true;
}

static uint8_t spi_flash_read_status1(void) {
	uint8_t status;

	spi_flash_cs_low();
	(void)spi_flash_xfer(SPI_FLASH_CMD_RDSR1);
	status = spi_flash_xfer(0xffu);
	spi_flash_cs_high();

	return status;
}

static bool spi_flash_sfdp_present(void) {
	uint8_t signature[4];
	unsigned i;

	spi_flash_cs_low();
	(void)spi_flash_xfer(SPI_FLASH_CMD_SFDP);
	(void)spi_flash_xfer(0x00u);
	(void)spi_flash_xfer(0x00u);
	(void)spi_flash_xfer(0x00u);
	(void)spi_flash_xfer(0xffu); /* one dummy byte */

	for (i = 0u; i < sizeof(signature); i++) {
		signature[i] = spi_flash_xfer(0xffu);
	}

	spi_flash_cs_high();

	return signature[0] == 'S' && signature[1] == 'F' &&
	       signature[2] == 'D' && signature[3] == 'P';
}

static const char *spi_flash_manufacturer_name(uint8_t manufacturer) {
	switch (manufacturer) {
	case 0x01u:
		return "Spansion/Cypress";
	case 0x0bu:
		return "XTX";
	case 0x1cu:
		return "EON";
	case 0x1fu:
		return "Atmel/Adesto";
	case 0x20u:
		return "Micron/ST";
	case 0x5eu:
		return "Zbit";
	case 0x62u:
		return "SANYO";
	case 0x68u:
		return "Boya";
	case 0x7fu:
	case 0x9du:
		return "ISSI";
	case 0x85u:
		return "Puya";
	case 0x89u:
		return "BergMicro";
	case 0x8cu:
		return "ESMT";
	case 0xa1u:
		return "Fudan";
	case 0xc2u:
		return "Macronix";
	case 0xefu:
		return "Winbond";
	default:
		return "unknown";
	}
}

static void console_put_size(uint32_t bytes) {
	console_putu(bytes);
	console_puts(" bytes");

	if (bytes >= 1024u) {
		console_puts(" (");
		console_putu(bytes / 1024u);
		console_puts(" KiB");

		if (bytes >= 1024u * 1024u) {
			console_puts(", ");
			console_putu(bytes / (1024u * 1024u));
			console_puts(" MiB");
		}

		console_puts(")");
	}
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
		crc = CRC32_UPDATE(crc, *data++);
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

static uint32_t spi_flash_capacity_from_code(uint8_t code) {
	if (code >= 32u) {
		return 0u;
	}

	return 1u << code;
}

int main(void) {
	uint8_t manufacturer = 0u;
	uint8_t memory_type = 0u;
	uint8_t capacity_code = 0u;
	uint32_t capacity_bytes;
	uint8_t status;
	uint32_t page;

	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_PLL_HSI_48MHZ]);

	console_init();
	spi_flash_init();

	console_puts("\r\nlibopenwch SPI NOR CRC32 test\r\n");

	if (!spi_flash_read_id(&manufacturer, &memory_type, &capacity_code)) {
		console_puts("SPI NOR flash NOT detected\r\n");
		console_puts("  RDID = 0x");
		console_puthex(manufacturer, 2u);
		console_puthex(memory_type, 2u);
		console_puthex(capacity_code, 2u);
		console_puts("\r\n");
		console_puts(
		    "  check CS (PA4), SCK (PA5), MISO (PA6), MOSI (PA7), "
		    "VCC and GND\r\n");

		for (;;) {
			;
		}
	}

	capacity_bytes = spi_flash_capacity_from_code(capacity_code);
	status = spi_flash_read_status1();

	console_puts("SPI NOR flash detected:\r\n");
	console_puts("  RDID = 0x");
	console_puthex(manufacturer, 2u);
	console_puthex(memory_type, 2u);
	console_puthex(capacity_code, 2u);
	console_puts("\r\n");

	console_puts("  manufacturer = ");
	console_puts(spi_flash_manufacturer_name(manufacturer));
	console_puts(" (0x");
	console_puthex(manufacturer, 2u);
	console_puts(")\r\n");

	console_puts("  memory type = 0x");
	console_puthex(memory_type, 2u);
	console_puts("\r\n");

	console_puts("  capacity = ");
	if (capacity_bytes != 0u) {
		console_put_size(capacity_bytes);
	} else {
		console_puts("unknown (capacity code 0x");
		console_puthex(capacity_code, 2u);
		console_puts(")");
	}
	console_puts("\r\n");

	console_puts("  SFDP = ");
	console_puts(spi_flash_sfdp_present() ? "present" : "not present");
	console_puts("\r\n");

	console_puts("  SR1 = 0x");
	console_puthex(status, 2u);
	console_puts(" (WIP=");
	console_putu(status & 0x01u);
	console_puts(", WEL=");
	console_putu((status >> 1) & 0x01u);
	console_puts(")\r\n");

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
