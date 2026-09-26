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
 * SDI counter — debug output over the single-wire debug interface.
 *
 * WCH-LinkE can expose an "SDI virtual serial port" on the debug interface.
 * The target writes short packets to the debug-module data registers:
 *
 *DMDATA0 = 0xE00000F4   low byte: length (1..7), upper bytes: data[0..2]
 *DMDATA1 = 0xE00000F8   data[3..6]
 *
 * and the programmer forwards them to its USB CDC port.  No target UART pin
 * is used, so this works with the SWIO/GND/3.3V connection alone.
 *
 * The WCH-LinkE must have its SDI virtual serial port enabled (the
 * EnableSDIPrintf option in recent WCH-LinkUtility releases).  Then:
 *
 *stty -F /dev/ttyACM0 115200 raw -echo
 *cat /dev/ttyACM0
 *
 * The register addresses and packet shape are interface facts from WCH's
 * debug interface; the implementation below is original to this example.
 */

#include <libopenwch/ch32v0/rcc.h>
#include <libopenwch/qingke/common.h>
#include <libopenwch/qingke/systick.h>

/** Debug-module data registers used by the WCH-LinkE SDI console. */
#define SDI_DATA0 MMIO32(0xe00000f4u)
#define SDI_DATA1 MMIO32(0xe00000f8u)

/** Initialise the debug channel. */
static void sdi_init(void) {
	SDI_DATA0 = 0u;
}

/**
 * Send up to seven bytes in a single SDI packet.
 *
 * The low byte of DMDATA0 is the payload length; the remaining three bytes
 * are data[0..2], and DMDATA1 carries data[3..6].  A non-zero DMDATA0 means
 * the debugger has not consumed the previous packet yet, so wait for it to
 * drain before overwriting the register.  The wait is bounded: if the SDI
 * console is not enabled, the target should keep running rather than hang on
 * the first print.
 */
static void sdi_send(const char *data, size_t length) {
	while (length != 0u) {
		volatile uint32_t spin = 0u;
		size_t chunk = length > 7u ? 7u : length;
		uint32_t data0 = (uint32_t)chunk;
		uint32_t data1 = 0u;

		if (chunk > 3u) {
			data1 |= (uint32_t)(uint8_t)data[3];
		}
		if (chunk > 4u) {
			data1 |= (uint32_t)(uint8_t)data[4] << 8;
		}
		if (chunk > 5u) {
			data1 |= (uint32_t)(uint8_t)data[5] << 16;
		}
		if (chunk > 6u) {
			data1 |= (uint32_t)(uint8_t)data[6] << 24;
		}

		data0 |= (uint32_t)(uint8_t)data[0] << 8;

		if (chunk > 1u) {
			data0 |= (uint32_t)(uint8_t)data[1] << 16;
		}
		if (chunk > 2u) {
			data0 |= (uint32_t)(uint8_t)data[2] << 24;
		}

		while (SDI_DATA0 != 0u && spin < 1000000u) {
			spin++;
		}

		SDI_DATA1 = data1;
		SDI_DATA0 = data0;

		data += chunk;
		length -= chunk;
	}
}

static void sdi_puts(const char *text) {
	while (*text != '\0') {
		sdi_send(text, 1u);
		text++;
	}
}

/** Print an unsigned value in decimal without pulling in printf. */
static void sdi_putu(uint32_t value) {
	char buf[10];
	int i = 0;

	if (value == 0u) {
		sdi_puts("0");
		return;
	}

	while (value != 0u && i < (int)sizeof(buf)) {
		buf[i++] = (char)('0' + (value % 10u));
		value /= 10u;
	}

	while (i-- > 0) {
		sdi_send(&buf[i], 1u);
	}
}

int main(void) {
	uint32_t count = 0u;

	/* Delay uses SysTick; the SDI channel itself does not need the PLL. */
	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_PLL_HSI_48MHZ]);
	qingke_systick_set_frequency(rcc_sysclk_frequency);
	systick_set_clock_source(1); /* HCLK, not the default HCLK/8 */

	sdi_init();

	sdi_puts("\r\nlibopenwch sdi_counter\r\n");
	sdi_puts("sysclk = ");
	sdi_putu(rcc_sysclk_frequency);
	sdi_puts(" Hz\r\n");

	for (;;) {
		sdi_putu(count++);
		sdi_puts("\r\n");
		qingke_delay_ms(500);
	}

	return 0;
}
