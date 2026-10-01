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
 * SSD1315 128x64 OLED over hardware I2C1.
 *
 * Build-time wiring selection (`SSD1315_I2C_PINS`):
 *
 *   0 / default   PC2 = I2C1 SCL, PC1 = I2C1 SDA
 *   1 / partial   PD1 = I2C1 SCL, PD0 = I2C1 SDA
 *   2 / full      PC5 = I2C1 SCL (SCL2), PC6 = I2C1 SDA (SDA2)
 *                 (this is the default)
 *
 *   3V3 -> OLED VCC
 *   GND -> OLED GND
 *
 * The panel must have its I2C address select tied to the default 0x78
 * write address (0x3c 7-bit).  Override with
 *   make CFLAGS+=-DSSD1315_I2C_ADDRESS=0x7a
 * for the alternate module.
 *
 * The example initialises the panel, keeps a 128x64 monochrome framebuffer and
 * continuously draws a moving test pattern.  It uses only the public libopenwch
 * I2C and GPIO APIs; the SSD1315 command set is the classic SSD1306-compatible
 * one.
 */

#include <libopenwch/ch32v0/gpio.h>
#include <libopenwch/ch32v0/i2c.h>
#include <libopenwch/ch32v0/rcc.h>

#define SSD1315_I2C I2C1

/* 8-bit write address used by the common 128x64 SSD1315 modules. */
#ifndef SSD1315_I2C_ADDRESS
#define SSD1315_I2C_ADDRESS 0x78u
#endif

#ifndef SSD1315_I2C_MAP
#define SSD1315_I2C_MAP 2
#endif

#define SSD1315_WIDTH 128u
#define SSD1315_HEIGHT 64u

/* Each page is 8 horizontal pixels; a 128x64 panel has eight pages. */
#define SSD1315_PAGES (SSD1315_HEIGHT / 8u)
#define SSD1315_FB_SIZE (SSD1315_WIDTH * SSD1315_PAGES)

#define SSD1315_PAGE_SIZE 8u
#define I2C_TIMEOUT 200000u

/* SSD1315 command bytes. */
#define SSD1315_DISPLAY_OFF 0xaeu
#define SSD1315_DISPLAY_ON 0xafu
#define SSD1315_SET_CLOCK_DIV 0xd5u
#define SSD1315_SET_MULTIPLEX 0xa8u
#define SSD1315_SET_DISPLAY_OFFSET 0xd3u
#define SSD1315_SET_START_LINE 0x40u
#define SSD1315_CHARGE_PUMP 0x8du
#define SSD1315_MEMORY_MODE 0x20u
#define SSD1315_SEGMENT_REMAP 0xa1u
#define SSD1315_COM_SCAN_DEC 0xc8u
#define SSD1315_SET_COM_PINS 0xdau
#define SSD1315_SET_CONTRAST 0x81u
#define SSD1315_SET_PRECHARGE 0xd9u
#define SSD1315_SET_VCOMH 0xdbu
#define SSD1315_ENTIRE_DISPLAY_ON 0xa4u
#define SSD1315_NORMAL_DISPLAY 0xa6u
#define SSD1315_DEACTIVATE_SCROLL 0x2eu
#define SSD1315_INTERNAL_IREF 0xadu

/*
 * Set to 1 for modules that rely on the SSD1315 internal IREF.  The
 * application example in the datasheet uses an external IREF resistor, so the
 * default follows the reset value (external IREF).
 */
#ifndef SSD1315_USE_INTERNAL_IREF
#define SSD1315_USE_INTERNAL_IREF 0
#endif

#define SSD1315_SET_COLUMN_ADDR 0x21u
#define SSD1315_SET_PAGE_ADDR 0x22u

/* Control bytes in the I2C stream. */
#define SSD1315_CONTROL_COMMAND 0x00u
#define SSD1315_CONTROL_DATA 0x40u

static uint8_t framebuffer[SSD1315_FB_SIZE];

/* --- small I2C master write path ---------------------------------------- */

static int i2c_wait_flag(uint16_t flag, int set) {
	uint32_t timeout;

	for (timeout = 0; timeout < I2C_TIMEOUT; timeout++) {
		if ((i2c_get_flag(SSD1315_I2C, flag) != 0u) == set) {
			return 0;
		}
	}

	return -1;
}

static void i2c_abort(void) {
	i2c_send_stop(SSD1315_I2C);
	i2c_clear_flag(SSD1315_I2C, I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF |
					I2C_SR1_OVR | I2C_SR1_PECERR);
}

/*
 * One SSD1315 I2C write: start, slave address, control byte, payload, stop.
 * The caller supplies either commands (0x00) or display RAM data (0x40).
 */
static int ssd1315_write(uint8_t control, const uint8_t *data, size_t length) {
	size_t i;

	if (i2c_wait_flag(I2C_SR2_BUSY, 0) != 0) {
		return -1;
	}

	i2c_send_start(SSD1315_I2C);

	if (i2c_wait_flag(I2C_SR1_SB, 1) != 0) {
		i2c_abort();
		return -1;
	}

	i2c_send_7bit_address(SSD1315_I2C, (uint8_t)(SSD1315_I2C_ADDRESS >> 1),
			      0);

	if (i2c_wait_flag(I2C_SR1_ADDR, 1) != 0) {
		i2c_abort();
		return -1;
	}

	/* Reading STAR1 then STAR2 clears ADDR and releases the clock. */
	i2c_clear_flag(SSD1315_I2C, I2C_SR1_ADDR);

	if (i2c_wait_flag(I2C_SR1_TXE, 1) != 0) {
		i2c_abort();
		return -1;
	}

	i2c_send_data(SSD1315_I2C, control);

	for (i = 0; i < length; i++) {
		if (i2c_wait_flag(I2C_SR1_TXE, 1) != 0) {
			i2c_abort();
			return -1;
		}

		i2c_send_data(SSD1315_I2C, data[i]);
	}

	if (i2c_wait_flag(I2C_SR1_BTF, 1) != 0) {
		i2c_abort();
		return -1;
	}

	i2c_send_stop(SSD1315_I2C);

	return 0;
}

static int ssd1315_write_commands(const uint8_t *commands, size_t length) {
	return ssd1315_write(SSD1315_CONTROL_COMMAND, commands, length);
}

/* --- framebuffer --------------------------------------------------------- */

static void fb_clear(uint8_t value) {
	size_t i;

	for (i = 0; i < sizeof(framebuffer); i++) {
		framebuffer[i] = value;
	}
}

static void fb_pixel(uint16_t x, uint16_t y, int on) {
	uint16_t page;
	uint8_t mask;

	if (x >= SSD1315_WIDTH || y >= SSD1315_HEIGHT) {
		return;
	}

	page =
	    (uint16_t)(x + (uint16_t)(y / SSD1315_PAGE_SIZE) * SSD1315_WIDTH);
	mask = (uint8_t)(1u << (y & (SSD1315_PAGE_SIZE - 1u)));

	if (on) {
		framebuffer[page] |= mask;
	} else {
		framebuffer[page] &= (uint8_t)~mask;
	}
}

static void fb_hline(uint16_t x, uint16_t y, uint16_t width, int on) {
	uint16_t i;

	for (i = 0; i < width; i++) {
		fb_pixel((uint16_t)(x + i), y, on);
	}
}

static void fb_vline(uint16_t x, uint16_t y, uint16_t height, int on) {
	uint16_t i;

	for (i = 0; i < height; i++) {
		fb_pixel(x, (uint16_t)(y + i), on);
	}
}

static void
fb_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, int on) {
	fb_hline(x, y, width, on);
	fb_hline(x, (uint16_t)(y + height - 1u), width, on);
	fb_vline(x, y, height, on);
	fb_vline((uint16_t)(x + width - 1u), y, height, on);
}

static void
fb_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, int on) {
	uint16_t row;

	for (row = 0; row < height; row++) {
		fb_hline(x, (uint16_t)(y + row), width, on);
	}
}

/* --- panel --------------------------------------------------------------- */

static int ssd1315_init(void) {
	static const uint8_t init[] = {
	    SSD1315_DISPLAY_OFF,
	    SSD1315_SET_CLOCK_DIV,
	    0x80u,
	    SSD1315_SET_MULTIPLEX,
	    SSD1315_HEIGHT - 1u,
	    SSD1315_SET_DISPLAY_OFFSET,
	    0x00u,
	    SSD1315_SET_START_LINE | 0x00u,
	    SSD1315_MEMORY_MODE,
	    0x00u,
	    SSD1315_SEGMENT_REMAP,
	    SSD1315_COM_SCAN_DEC,
	    SSD1315_SET_COM_PINS,
	    0x12u,
	    SSD1315_SET_CONTRAST,
	    0xcfu,

	    /*
     * D9h: phase 2 = 15, phase 1 = 2.  The datasheet requires
     * even DCLK values; 0xF1 (the SSD1306-era value) has an odd
     * phase-1 field.
     */
	    SSD1315_SET_PRECHARGE,
	    0xf2u,

	    /* DBh: 0x20 -> ~0.77 x VCC, the datasheet reset value. */
	    SSD1315_SET_VCOMH,
	    0x20u,

#if SSD1315_USE_INTERNAL_IREF
	    /* ADh 0x30: internal IREF, 30 uA / 240 uA maximum ISEG. */
	    SSD1315_INTERNAL_IREF,
	    0x30u,
#endif

	    SSD1315_ENTIRE_DISPLAY_ON,
	    SSD1315_NORMAL_DISPLAY,
	    SSD1315_DEACTIVATE_SCROLL,

	    /*
     * 8Dh 14h enables the charge pump; AFh must follow it.  Keep
     * these adjacent, as required by the power-on sequence.
     */
	    SSD1315_CHARGE_PUMP,
	    0x14u,
	    SSD1315_DISPLAY_ON,
	};

	if (ssd1315_write_commands(init, sizeof(init)) != 0) {
		return -1;
	}

	return 0;
}

static int ssd1315_refresh(void) {
	static const uint8_t window[] = {
	    SSD1315_SET_COLUMN_ADDR, 0x00u, SSD1315_WIDTH - 1u,
	    SSD1315_SET_PAGE_ADDR,   0x00u, SSD1315_PAGES - 1u,
	};

	if (ssd1315_write_commands(window, sizeof(window)) != 0) {
		return -1;
	}

	return ssd1315_write(SSD1315_CONTROL_DATA, framebuffer,
			     sizeof(framebuffer));
}

/* --- demo pattern -------------------------------------------------------- */

static void draw_pattern(uint8_t phase) {
	uint16_t x;
	uint16_t box_x;

	fb_clear(0x00u);
	fb_rect(0u, 0u, SSD1315_WIDTH, SSD1315_HEIGHT, 1);

	/* A repeating diagonal that scrolls down one pixel per frame. */
	for (x = 1u; x + 1u < SSD1315_WIDTH; x++) {
		uint16_t y = (uint16_t)((x + phase) & (SSD1315_HEIGHT - 1u));

		fb_pixel(x, y, 1);
	}

	/* A rectangle bouncing left and right with the same phase. */
	box_x =
	    (uint16_t)(16u + ((phase * 3u) % (SSD1315_WIDTH - 32u - 16u + 1u)));
	fb_fill_rect(box_x, 24u, 16u, 16u, 1);
}

static void delay_loops(volatile uint32_t loops) {
	while (loops--) {
		__asm__ volatile("nop");
	}
}

int main(void) {
	uint8_t phase = 0u;

	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_PLL_HSI_48MHZ]);

	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_AFIO);
	rcc_periph_clock_enable(RCC_I2C1);

	/*
	 * I2C1 pin mapping selected by SSD1315_I2C_MAP:
	 *   0 = default: SCL = PC2, SDA = PC1
	 *   1 = partial: SCL = PD1, SDA = PD0
	 *   2 = full:    SCL = PC5, SDA = PC6
	 * Pins are open-drain alternate function; bus pull-ups are external.
	 */
#if SSD1315_I2C_MAP == 0
	gpio_set_mode(GPIOC, GPIO_MODE_AF_OD, GPIO1 | GPIO2);
#elif SSD1315_I2C_MAP == 1
	rcc_periph_clock_enable(RCC_GPIOD);
	gpio_i2c1_remap(GPIO_REMAP_I2C1_PARTIAL);
	gpio_set_mode(GPIOD, GPIO_MODE_AF_OD, GPIO0 | GPIO1);
#elif SSD1315_I2C_MAP == 2
	gpio_i2c1_remap(GPIO_REMAP_I2C1_FULL);
	gpio_set_mode(GPIOC, GPIO_MODE_AF_OD, GPIO5 | GPIO6);
#else
#error "SSD1315_I2C_MAP must be 0 (default), 1 (partial) or 2 (full)"
#endif

	i2c_init_master(SSD1315_I2C, rcc_apb1_frequency, I2C_SPEED_STANDARD,
			I2C_CCR_DUTY_2);
	i2c_enable(SSD1315_I2C);

	/* Let the panel finish its own power-on reset before commands. */
	delay_loops(120000u);

	if (ssd1315_init() != 0) {
		for (;;) {
			delay_loops(120000u);
		}
	}

	for (;;) {
		draw_pattern(phase);

		if (ssd1315_refresh() != 0) {
			for (;;) {
				delay_loops(120000u);
			}
		}

		phase++;
		delay_loops(80000u);
	}

	/* Not reached. */
	return 0;
}
