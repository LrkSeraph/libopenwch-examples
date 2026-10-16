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
 * Integrated CH32V003 self-test cases.
 *
 * Every case is deliberately non-destructive except for the two watchdog
 * cases in selftest_main.c.  Cases use the public libopenwch API where it
 * exists and fall back to the documented MMIO register definitions only to
 * verify register fields or to touch functionality the API does not expose.
 */

#include <stdbool.h>
#include <stdint.h>

#include <libopenwch/ch32v0/adc.h>
#include <libopenwch/ch32v0/dma.h>
#include <libopenwch/ch32v0/esig.h>
#include <libopenwch/ch32v0/exten.h>
#include <libopenwch/ch32v0/gpio.h>
#include <libopenwch/ch32v0/i2c.h>
#include <libopenwch/ch32v0/rcc.h>
#include <libopenwch/ch32v0/spi.h>
#include <libopenwch/ch32v0/tim.h>
#include <libopenwch/ch32v0/usart.h>
#include <libopenwch/qingke/sync.h>
#include <libopenwch/qingke/systick.h>

#include "selftest.h"
#include "selftest_console.h"

#pragma GCC optimize("Os")

/* --- Small helpers ------------------------------------------------------- */

SELFTEST_OPTIMIZE static bool wait_mmio16(uint32_t address,
					  uint16_t mask,
					  uint16_t expected,
					  uint32_t timeout) {
	while (timeout-- != 0u) {
		if ((MMIO16(address) & mask) == expected) {
			return true;
		}
	}
	return false;
}

/* --- RCC ---------------------------------------------------------------- */

SELFTEST_OPTIMIZE static selftest_result_t test_rcc(void) {
	uint32_t ctlr = RCC_CTLR;
	uint32_t cfgr = RCC_CFGR0;
	uint32_t sw;
	uint32_t sws;

	if ((ctlr & RCC_CTLR_HSION) == 0u || (ctlr & RCC_CTLR_HSIRDY) == 0u) {
		return SELFTEST_RESULT_FAIL;
	}
	if ((ctlr & RCC_CTLR_PLLON) == 0u || (ctlr & RCC_CTLR_PLLRDY) == 0u) {
		return SELFTEST_RESULT_FAIL;
	}

	sw = (cfgr & RCC_CFGR0_SW_MASK) >> RCC_CFGR0_SW_SHIFT;
	sws = (cfgr & RCC_CFGR0_SWS_MASK) >> RCC_CFGR0_SWS_SHIFT;
	if (sw != RCC_CFGR0_SW_PLL || sws != RCC_CFGR0_SW_PLL) {
		return SELFTEST_RESULT_FAIL;
	}

	if (((cfgr & RCC_CFGR0_HPRE_MASK) >> RCC_CFGR0_HPRE_SHIFT) !=
	    RCC_CFGR0_HPRE_DIV1) {
		return SELFTEST_RESULT_FAIL;
	}
	if (((cfgr & RCC_CFGR0_PPRE1_MASK) >> RCC_CFGR0_PPRE1_SHIFT) !=
	    RCC_CFGR0_PPRE_DIV1) {
		return SELFTEST_RESULT_FAIL;
	}
	if (((cfgr & RCC_CFGR0_PPRE2_MASK) >> RCC_CFGR0_PPRE2_SHIFT) !=
	    RCC_CFGR0_PPRE_DIV1) {
		return SELFTEST_RESULT_FAIL;
	}
	if (((cfgr & RCC_CFGR0_ADCPRE_MASK) >> RCC_CFGR0_ADCPRE_SHIFT) !=
	    RCC_CFGR0_ADCPRE_DIV2) {
		return SELFTEST_RESULT_FAIL;
	}

	if (rcc_sysclk_frequency != 48000000u ||
	    rcc_ahb_frequency != 48000000u || rcc_apb1_frequency != 48000000u ||
	    rcc_apb2_frequency != 48000000u || rcc_adc_frequency != 24000000u) {
		return SELFTEST_RESULT_FAIL;
	}
	if (rcc_get_sysclk_frequency() != 48000000u) {
		return SELFTEST_RESULT_FAIL;
	}

	/* Recompute from the register fields and ensure the table is honest. */
	rcc_measure_clocks();
	if (rcc_sysclk_frequency != 48000000u ||
	    rcc_ahb_frequency != 48000000u || rcc_apb1_frequency != 48000000u ||
	    rcc_apb2_frequency != 48000000u || rcc_adc_frequency != 24000000u) {
		return SELFTEST_RESULT_FAIL;
	}

	return SELFTEST_RESULT_PASS;
}

/* --- SysTick ------------------------------------------------------------ */

SELFTEST_OPTIMIZE static selftest_result_t test_systick(void) {
	uint32_t start;
	uint32_t end;
	uint32_t elapsed;
	uint32_t ctlr;

	if (qingke_systick_get_frequency() != 48000000u) {
		return SELFTEST_RESULT_FAIL;
	}
	ctlr = SYSTICK->ctlr;
	if ((ctlr & SYSTICK_CTLR_EN) == 0u) {
		return SELFTEST_RESULT_FAIL;
	}
	if ((ctlr & SYSTICK_CTLR_CLKSRC) == 0u) {
		return SELFTEST_RESULT_FAIL;
	}

	start = systick_get_value();
	qingke_delay_us(1000u);
	end = systick_get_value();
	elapsed = end - start;

	/* 48 MHz HCLK gives 48000 ticks per millisecond. */
	if (elapsed < 43000u || elapsed > 53000u) {
		return SELFTEST_RESULT_FAIL;
	}

	/* Exercise the counter gate and make sure later cases still have it. */
	systick_disable_counter();
	if ((SYSTICK->ctlr & SYSTICK_CTLR_EN) != 0u) {
		return SELFTEST_RESULT_FAIL;
	}
	systick_enable_counter();
	if ((SYSTICK->ctlr & SYSTICK_CTLR_EN) == 0u) {
		return SELFTEST_RESULT_FAIL;
	}

	return SELFTEST_RESULT_PASS;
}

/* --- GPIO --------------------------------------------------------------- */

SELFTEST_OPTIMIZE static selftest_result_t test_gpio(void) {
	const uint32_t port = GPIOC;
	const uint16_t pin = GPIO0;
	uint32_t saved_cfglr;
	uint16_t saved_out;

	rcc_periph_clock_enable(RCC_GPIOC);

	saved_cfglr = GPIO_CFGLR(port);
	saved_out = (uint16_t)GPIO_OUTDR(port);

	gpio_set_mode(port, GPIO_MODE_OUT_PP, pin);
	if ((GPIO_CFGLR(port) & 0x0000000fu) !=
	    (((uint32_t)GPIO_MODE_OUT_PP & 0x0fu) | 1u)) {
		return SELFTEST_RESULT_FAIL;
	}

	gpio_clear(port, pin);
	if ((GPIO_OUTDR(port) & pin) != 0u) {
		return SELFTEST_RESULT_FAIL;
	}
	gpio_set(port, pin);
	if ((GPIO_OUTDR(port) & pin) == 0u) {
		return SELFTEST_RESULT_FAIL;
	}
	gpio_toggle(port, pin);
	if ((GPIO_OUTDR(port) & pin) != 0u) {
		return SELFTEST_RESULT_FAIL;
	}
	gpio_toggle(port, pin);
	if ((GPIO_OUTDR(port) & pin) == 0u) {
		return SELFTEST_RESULT_FAIL;
	}

	/* Exercise the whole-port accessor while preserving other pins. */
	gpio_port_write(port, (uint16_t)((saved_out & ~pin) | pin));
	if ((GPIO_OUTDR(port) & pin) == 0u) {
		return SELFTEST_RESULT_FAIL;
	}
	gpio_port_write(port, (uint16_t)(saved_out & ~pin));
	if ((GPIO_OUTDR(port) & pin) != 0u) {
		return SELFTEST_RESULT_FAIL;
	}

	gpio_set_mode(port, GPIO_MODE_IN_FLOATING, pin);
	if ((GPIO_CFGLR(port) & 0x0000000fu) !=
	    (uint32_t)GPIO_MODE_IN_FLOATING) {
		return SELFTEST_RESULT_FAIL;
	}

	GPIO_CFGLR(port) = saved_cfglr;
	GPIO_OUTDR(port) = saved_out;
	return SELFTEST_RESULT_PASS;
}

/* --- USART1 ------------------------------------------------------------- */

SELFTEST_OPTIMIZE static uint32_t usart_config_checks(void) {
	usart_set_baudrate(USART1, 9600u);
	if (USART_BRR(USART1) != 5000u) {
		return 1u;
	}
	usart_set_baudrate(USART1, 115200u);
	if (USART_BRR(USART1) != 417u) {
		return 2u;
	}

	usart_set_databits(USART1, 9u);
	if ((USART_CTLR1(USART1) & USART_CTLR1_M) == 0u) {
		return 3u;
	}
	usart_set_databits(USART1, 8u);
	if ((USART_CTLR1(USART1) & USART_CTLR1_M) != 0u) {
		return 4u;
	}

	usart_set_parity(USART1, USART_PARITY_ODD);
	if ((USART_CTLR1(USART1) & (USART_CTLR1_PCE | USART_CTLR1_PS)) !=
	    (USART_CTLR1_PCE | USART_CTLR1_PS)) {
		return 5u;
	}
	usart_set_parity(USART1, USART_PARITY_EVEN);
	if ((USART_CTLR1(USART1) & USART_CTLR1_PCE) == 0u ||
	    (USART_CTLR1(USART1) & USART_CTLR1_PS) != 0u) {
		return 6u;
	}
	usart_set_parity(USART1, USART_PARITY_NONE);
	if ((USART_CTLR1(USART1) & (USART_CTLR1_PCE | USART_CTLR1_PS)) != 0u) {
		return 7u;
	}

	usart_set_stopbits(USART1, USART_STOPBITS_2);
	if ((USART_CTLR2(USART1) & USART_CTLR2_STOP_MASK) !=
	    (2u << USART_CTLR2_STOP_SHIFT)) {
		return 8u;
	}
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	if ((USART_CTLR2(USART1) & USART_CTLR2_STOP_MASK) != 0u) {
		return 9u;
	}

	usart_set_clock(USART1, USART_CLOCK_ENABLE | USART_CLOCK_CPOL_HIGH |
				    USART_CLOCK_CPHA_2EDGE);
	if ((USART_CTLR2(USART1) &
	     (USART_CTLR2_CLKEN | USART_CTLR2_CPOL | USART_CTLR2_CPHA)) !=
	    (USART_CTLR2_CLKEN | USART_CTLR2_CPOL | USART_CTLR2_CPHA)) {
		return 10u;
	}
	usart_set_clock(USART1, USART_CLOCK_DISABLE);

	usart_set_flow_control(USART1, USART_FLOWCONTROL_RTS_CTS);
	if ((USART_CTLR3(USART1) & (USART_CTLR3_RTSE | USART_CTLR3_CTSE)) !=
	    (USART_CTLR3_RTSE | USART_CTLR3_CTSE)) {
		return 11u;
	}
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);

	usart_set_mode(USART1, USART_MODE_TX_RX);
	if ((USART_CTLR1(USART1) & (USART_CTLR1_TE | USART_CTLR1_RE)) !=
	    (USART_CTLR1_TE | USART_CTLR1_RE)) {
		return 12u;
	}
	usart_set_mode(USART1, USART_MODE_TX);

	return 0u;
}

SELFTEST_OPTIMIZE static selftest_result_t test_usart(void) {
	uint16_t saved_ctlr1 = USART_CTLR1(USART1);
	uint16_t saved_ctlr2 = USART_CTLR2(USART1);
	uint16_t saved_ctlr3 = USART_CTLR3(USART1);
	uint16_t saved_brr = USART_BRR(USART1);
	uint32_t step;

	usart_disable(USART1);
	/*
	 * usart_disable() only clears UE, but the reference manual locks some
	 * CTLR2 fields once TE is set (CPOL/CPHA/LBCL).  Clear both directions
	 * before reconfiguring the block.
	 */
	USART_CTLR1(USART1) =
	    (uint16_t)(saved_ctlr1 &
		       ~(USART_CTLR1_UE | USART_CTLR1_TE | USART_CTLR1_RE));
	step = usart_config_checks();

	/* Always restore the console, even when a register check failed. */
	USART_CTLR1(USART1) = saved_ctlr1;
	USART_CTLR2(USART1) = saved_ctlr2;
	USART_CTLR3(USART1) = saved_ctlr3;
	USART_BRR(USART1) = saved_brr;
	usart_enable(USART1);

	if (step != 0u) {
		console_puts("usart fail step=");
		console_putu(step);
		console_crlf();
		return SELFTEST_RESULT_FAIL;
	}

	return SELFTEST_RESULT_PASS;
}

/* --- DMA ---------------------------------------------------------------- */

#define DMA_BLOCK_WORDS 64u
#define DMA_TIMEOUT_LOOPS 2000000u

union dma_block {
	uint32_t words[DMA_BLOCK_WORDS];
	uint16_t halves[DMA_BLOCK_WORDS * 2u];
	uint8_t bytes[DMA_BLOCK_WORDS * 4u];
};

static volatile union dma_block dma_src;
static volatile union dma_block dma_dst;

SELFTEST_OPTIMIZE static bool dma_mem2mem_copy(uint8_t channel,
					       uint32_t source,
					       uint32_t destination,
					       uint16_t count,
					       uint32_t size,
					       uint32_t *elapsed_ticks) {
	uint32_t start;
	uint32_t end;
	uint32_t timeout;

	dma_channel_reset(DMA1, channel);
	dma_set_peripheral_address(DMA1, channel, source);
	dma_set_memory_address(DMA1, channel, destination);
	dma_set_number_of_data(DMA1, channel, count);
	dma_enable_peripheral_increment_mode(DMA1, channel);
	dma_enable_memory_increment_mode(DMA1, channel);
	dma_set_peripheral_size(DMA1, channel, size);
	dma_set_memory_size(DMA1, channel, size);
	dma_set_priority(DMA1, channel, DMA_PRIORITY_VERY_HIGH);
	dma_enable_mem2mem_mode(DMA1, channel);
	dma_clear_interrupt_pending_bit(DMA1, channel);

	start = systick_get_value();
	dma_channel_enable(DMA1, channel);

	for (timeout = 0; timeout < DMA_TIMEOUT_LOOPS; timeout++) {
		if (dma_get_flag(DMA1, channel, DMA_TCIF) != 0u) {
			break;
		}
	}

	end = systick_get_value();
	dma_channel_disable(DMA1, channel);

	if (elapsed_ticks != NULL) {
		*elapsed_ticks = end - start;
	}

	if (timeout == DMA_TIMEOUT_LOOPS) {
		return false;
	}

	if (dma_get_number_of_data(DMA1, channel) != 0u) {
		return false;
	}

	return true;
}

SELFTEST_OPTIMIZE static void dma_fill_8(uint32_t round) {
	uint32_t i;

	for (i = 0; i < sizeof(dma_src.bytes); i++) {
		dma_src.bytes[i] =
		    (uint8_t)(0xa5u ^ (uint8_t)i ^ (uint8_t)round);
		dma_dst.bytes[i] = 0u;
	}
}

SELFTEST_OPTIMIZE static void dma_fill_16(uint32_t round) {
	uint32_t i;

	for (i = 0; i < (DMA_BLOCK_WORDS * 2u); i++) {
		dma_src.halves[i] =
		    (uint16_t)(0x5a5au ^ (uint16_t)i ^ (uint16_t)round);
		dma_dst.halves[i] = 0u;
	}
}

SELFTEST_OPTIMIZE static void dma_fill_32(uint32_t round) {
	uint32_t i;

	for (i = 0; i < DMA_BLOCK_WORDS; i++) {
		dma_src.words[i] = 0x5a5a0000u ^ i ^ (round * 0x01010101u);
		dma_dst.words[i] = 0u;
	}
}

SELFTEST_OPTIMIZE static bool dma_verify_8(uint32_t count) {
	uint32_t i;

	for (i = 0; i < count; i++) {
		if (dma_dst.bytes[i] != dma_src.bytes[i]) {
			return false;
		}
	}
	return true;
}

SELFTEST_OPTIMIZE static bool dma_verify_16(uint32_t count) {
	uint32_t i;

	for (i = 0; i < count; i++) {
		if (dma_dst.halves[i] != dma_src.halves[i]) {
			return false;
		}
	}
	return true;
}

SELFTEST_OPTIMIZE static bool dma_verify_32(uint32_t count) {
	uint32_t i;

	for (i = 0; i < count; i++) {
		if (dma_dst.words[i] != dma_src.words[i]) {
			return false;
		}
	}
	return true;
}

SELFTEST_OPTIMIZE static selftest_result_t
dma_run_one(uint8_t channel, uint32_t size, uint16_t count, uint32_t round) {
	uint32_t ticks = 0u;
	uint32_t source;
	uint32_t destination;
	uint32_t status;

	if (size == DMA_SIZE_8BIT) {
		dma_fill_8(round);
		source = (uint32_t)dma_src.bytes;
		destination = (uint32_t)dma_dst.bytes;
	} else if (size == DMA_SIZE_16BIT) {
		dma_fill_16(round);
		source = (uint32_t)dma_src.halves;
		destination = (uint32_t)dma_dst.halves;
	} else {
		dma_fill_32(round);
		source = (uint32_t)dma_src.words;
		destination = (uint32_t)dma_dst.words;
	}

	if (!dma_mem2mem_copy(channel, source, destination, count, size,
			      &ticks)) {
		return SELFTEST_RESULT_FAIL;
	}

	status = dma_get_interrupt_status(DMA1, channel);
	if ((status & DMA_TCIF) == 0u) {
		return SELFTEST_RESULT_FAIL;
	}
	if ((status & DMA_HTIF) == 0u) {
		return SELFTEST_RESULT_FAIL;
	}

	if (size == DMA_SIZE_8BIT) {
		if (!dma_verify_8(count)) {
			return SELFTEST_RESULT_FAIL;
		}
	} else if (size == DMA_SIZE_16BIT) {
		if (!dma_verify_16(count)) {
			return SELFTEST_RESULT_FAIL;
		}
	} else {
		if (!dma_verify_32(count)) {
			return SELFTEST_RESULT_FAIL;
		}
	}

	return SELFTEST_RESULT_PASS;
}

SELFTEST_OPTIMIZE static selftest_result_t test_dma(void) {
	uint8_t channel;
	uint32_t cfgr;

	rcc_periph_clock_enable(RCC_DMA1);

	/* Exercise the register-only controls on a channel before using it. */
	dma_channel_reset(DMA1, DMA_CHANNEL7);
	dma_set_read_from_peripheral(DMA1, DMA_CHANNEL7);
	cfgr = DMA_CFGR(DMA1, DMA_CHANNEL7);
	if ((cfgr & DMA_CFGR_DIR) != 0u) {
		return SELFTEST_RESULT_FAIL;
	}
	dma_set_read_from_memory(DMA1, DMA_CHANNEL7);
	cfgr = DMA_CFGR(DMA1, DMA_CHANNEL7);
	if ((cfgr & DMA_CFGR_DIR) == 0u) {
		return SELFTEST_RESULT_FAIL;
	}
	dma_set_priority(DMA1, DMA_CHANNEL7, DMA_PRIORITY_MEDIUM);
	cfgr = DMA_CFGR(DMA1, DMA_CHANNEL7);
	if (((cfgr & DMA_CFGR_PL_MASK) >> DMA_CFGR_PL_SHIFT) !=
	    DMA_PRIORITY_MEDIUM) {
		return SELFTEST_RESULT_FAIL;
	}
	dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL7);
	dma_enable_half_transfer_interrupt(DMA1, DMA_CHANNEL7);
	dma_enable_transfer_error_interrupt(DMA1, DMA_CHANNEL7);
	cfgr = DMA_CFGR(DMA1, DMA_CHANNEL7);
	if ((cfgr & (DMA_CFGR_TCIE | DMA_CFGR_HTIE | DMA_CFGR_TEIE)) !=
	    (DMA_CFGR_TCIE | DMA_CFGR_HTIE | DMA_CFGR_TEIE)) {
		return SELFTEST_RESULT_FAIL;
	}
	dma_enable_circular_mode(DMA1, DMA_CHANNEL7);
	if ((DMA_CFGR(DMA1, DMA_CHANNEL7) & DMA_CFGR_CIRC) == 0u) {
		return SELFTEST_RESULT_FAIL;
	}
	dma_channel_reset(DMA1, DMA_CHANNEL7);

	/* Real MEM2MEM transfers on several channels and all three sizes. */
	if (dma_run_one(DMA_CHANNEL1, DMA_SIZE_8BIT, 256u, 1u) !=
	    SELFTEST_RESULT_PASS) {
		return SELFTEST_RESULT_FAIL;
	}
	if (dma_run_one(DMA_CHANNEL2, DMA_SIZE_16BIT, 128u, 2u) !=
	    SELFTEST_RESULT_PASS) {
		return SELFTEST_RESULT_FAIL;
	}
	if (dma_run_one(DMA_CHANNEL3, DMA_SIZE_32BIT, 64u, 3u) !=
	    SELFTEST_RESULT_PASS) {
		return SELFTEST_RESULT_FAIL;
	}
	for (channel = DMA_CHANNEL4; channel <= DMA_CHANNEL7; channel++) {
		if (dma_run_one(channel, DMA_SIZE_8BIT, 32u, channel) !=
		    SELFTEST_RESULT_PASS) {
			return SELFTEST_RESULT_FAIL;
		}
	}

	return SELFTEST_RESULT_PASS;
}

/* --- TIM1 / TIM2 -------------------------------------------------------- */

#define TIMER_PSC 4799u /* 48 MHz / 4800 = 10 kHz */
#define TIMER_PERIOD_US 1000000u
#define TIMER_TOLERANCE_US 5000u
#define TIMER_WAIT_US 2000000u

SELFTEST_OPTIMIZE static uint32_t timer_clock_hz(uint32_t pclk, uint32_t ahb) {
	uint32_t clock = pclk;

	if (pclk != ahb) {
		clock *= 2u;
	}
	return clock;
}

SELFTEST_OPTIMIZE static bool timer_register_checks(uint32_t tim) {
	uint16_t value;

	timer_disable(tim);

	timer_set_prescaler(tim, 1234u);
	if (TIM_PSC(tim) != 1234u) {
		return false;
	}
	timer_set_period(tim, 5678u);
	if (TIM_ATRLR(tim) != 5678u) {
		return false;
	}
	timer_set_counter(tim, 0x1234u);
	if (TIM_CNT(tim) != 0x1234u) {
		return false;
	}

	timer_set_mode(tim, TIM_MODE_EDGE_ALIGNED | TIM_MODE_UP |
				TIM_MODE_UPDATE_OVERFLOW);
	if ((TIM_CTLR1(tim) & TIM_CTLR1_URS) == 0u) {
		return false;
	}

	timer_set_clock_division(tim, TIM_CKD_DIV2);
	if ((TIM_CTLR1(tim) & TIM_CTLR1_CKD_MASK) != TIM_CTLR1_CKD_DIV2) {
		return false;
	}
	timer_set_clock_division(tim, TIM_CKD_DIV1);

	timer_enable_preload(tim);
	if ((TIM_CTLR1(tim) & TIM_CTLR1_ARPE) == 0u) {
		return false;
	}
	timer_disable_preload(tim);

	timer_enable_irq(tim, TIM_IRQ_UPDATE);
	if ((TIM_DMAINTENR(tim) & TIM_DMAINTENR_UIE) == 0u) {
		return false;
	}
	timer_disable_irq(tim, TIM_IRQ_UPDATE);

	/* URS=0 lets UG set UIF; URS=1 restricts update requests to counter
	 * overflow and is what timer_period_test() wants later. */
	timer_set_mode(tim, TIM_MODE_EDGE_ALIGNED | TIM_MODE_UP);
	timer_clear_flag(tim, TIM_FLAG_UPDATE);
	timer_generate_event(tim, TIM_EVENT_UPDATE);
	if (wait_mmio16((uint32_t)&TIM_INTFR(tim), TIM_INTFR_UIF, TIM_INTFR_UIF,
			100000u) == false) {
		return false;
	}
	timer_clear_flag(tim, TIM_FLAG_UPDATE);
	if ((TIM_INTFR(tim) & TIM_INTFR_UIF) != 0u) {
		return false;
	}

	/* Output compare registers and flags, without routing a pin. */
	timer_set_oc_mode(tim, TIM_OC1, TIM_OC_MODE_PWM1);
	value = TIM_CHCTLR1(tim);
	if ((value & TIM_CHCTLR1_OC1M_MASK) !=
	    ((uint16_t)TIM_OC_MODE_PWM1 << TIM_CHCTLR1_OC1M_SHIFT)) {
		return false;
	}
	timer_set_oc_value(tim, TIM_OC1, 0x1234u);
	if ((TIM_CH1CVR(tim) & 0xffffu) != 0x1234u) {
		return false;
	}
	timer_set_oc_polarity(tim, TIM_OC1, TIM_OC_POLARITY_ACTIVE_LOW);
	if ((TIM_CCER(tim) & TIM_CCER_CC1P) == 0u) {
		return false;
	}
	timer_set_oc_polarity(tim, TIM_OC1, TIM_OC_POLARITY_ACTIVE_HIGH);

	timer_enable_oc_preload(tim, TIM_OC1);
	if ((TIM_CHCTLR1(tim) & TIM_CHCTLR1_OC1PE) == 0u) {
		return false;
	}
	timer_disable_oc_preload(tim, TIM_OC1);

	timer_enable_oc_output(tim, TIM_OC1);
	if ((TIM_CCER(tim) & TIM_CCER_CC1E) == 0u) {
		return false;
	}
	timer_disable_oc_output(tim, TIM_OC1);

	/* Input capture filter/prescaler/polarity fields. */
	timer_set_input_filter(tim, TIM_IC1, 0x0fu);
	if ((TIM_CHCTLR1(tim) & 0x00f0u) != 0x00f0u) {
		return false;
	}
	timer_set_ic_prescaler(tim, TIM_IC1, 0x3u);
	if ((TIM_CHCTLR1(tim) & 0x000cu) != 0x000cu) {
		return false;
	}
	timer_set_input_polarity(tim, TIM_IC1, TIM_IC_POLARITY_FALLING);
	if ((TIM_CCER(tim) & TIM_CCER_CC1P) == 0u) {
		return false;
	}
	timer_set_input_polarity(tim, TIM_IC1, TIM_IC_POLARITY_RISING);
	timer_set_input_filter(tim, TIM_IC1, 0u);
	timer_set_ic_prescaler(tim, TIM_IC1, 0u);

	if (tim == TIM1) {
		timer_set_deadtime(tim, 0x12u);
		if ((TIM_BDTR(tim) & TIM_BDTR_DTG_MASK) != 0x12u) {
			return false;
		}
		timer_enable_break_main_output(tim);
		if ((TIM_BDTR(tim) & TIM_BDTR_MOE) == 0u) {
			return false;
		}
		timer_set_oc_idle_state(tim, TIM_OC1, TIM_OC_IDLE_SET);
		if ((TIM_CTLR2(tim) & TIM_CTLR2_OIS1) == 0u) {
			return false;
		}
		timer_set_oc_idle_state(tim, TIM_OC1, TIM_OC_IDLE_RESET);
	}

	timer_disable(tim);
	return true;
}

SELFTEST_OPTIMIZE static bool timer_wait_update(uint32_t tim,
						uint32_t timeout_us) {
	uint32_t start = systick_get_value();
	uint32_t timeout_ticks =
	    timeout_us * (qingke_systick_get_frequency() / 1000000u);

	while ((systick_get_value() - start) < timeout_ticks) {
		if ((TIM_INTFR(tim) & TIM_INTFR_UIF) != 0u) {
			return true;
		}
	}
	return false;
}

SELFTEST_OPTIMIZE static selftest_result_t
timer_period_test(uint32_t tim, uint32_t rcc_clock, uint32_t clock_hz) {
	uint32_t period;
	uint32_t start;
	uint32_t end;
	uint32_t elapsed_ticks;
	uint32_t ticks_per_us = qingke_systick_get_frequency() / 1000000u;

	rcc_periph_clock_enable(rcc_clock);

	period = (clock_hz / (TIMER_PSC + 1u)) - 1u;

	timer_disable(tim);
	timer_set_prescaler(tim, (uint16_t)TIMER_PSC);
	timer_set_period(tim, (uint16_t)period);
	timer_set_mode(tim, TIM_MODE_EDGE_ALIGNED | TIM_MODE_UP |
				TIM_MODE_UPDATE_OVERFLOW);
	timer_set_counter(tim, 0u);
	timer_clear_flag(tim, TIM_FLAG_UPDATE);
	timer_generate_event(tim, TIM_EVENT_UPDATE);
	timer_clear_flag(tim, TIM_FLAG_UPDATE);
	timer_enable(tim);

	/* Wait for the first update, then time the following full period. */
	if (timer_wait_update(tim, TIMER_WAIT_US) == false) {
		timer_disable(tim);
		return SELFTEST_RESULT_FAIL;
	}
	timer_clear_flag(tim, TIM_FLAG_UPDATE);
	start = systick_get_value();

	if (timer_wait_update(tim, TIMER_WAIT_US) == false) {
		timer_disable(tim);
		return SELFTEST_RESULT_FAIL;
	}
	end = systick_get_value();
	timer_clear_flag(tim, TIM_FLAG_UPDATE);
	timer_disable(tim);

	elapsed_ticks = end - start;

	if (elapsed_ticks + TIMER_TOLERANCE_US * ticks_per_us <
		TIMER_PERIOD_US * ticks_per_us ||
	    elapsed_ticks > TIMER_PERIOD_US * ticks_per_us +
				TIMER_TOLERANCE_US * ticks_per_us) {
		return SELFTEST_RESULT_FAIL;
	}

	return SELFTEST_RESULT_PASS;
}

SELFTEST_OPTIMIZE static selftest_result_t test_timers(void) {
	uint32_t tim1_clock =
	    timer_clock_hz(rcc_apb2_frequency, rcc_ahb_frequency);
	uint32_t tim2_clock =
	    timer_clock_hz(rcc_apb1_frequency, rcc_ahb_frequency);

	/* Register checks touch the blocks directly, so their clocks must be on
	 * before the period tests run. */
	rcc_periph_clock_enable(RCC_TIM1);
	rcc_periph_clock_enable(RCC_TIM2);

	if (!timer_register_checks(TIM1)) {
		console_puts("tim1 reg fail\r\n");
		return SELFTEST_RESULT_FAIL;
	}
	if (!timer_register_checks(TIM2)) {
		console_puts("tim2 reg fail\r\n");
		return SELFTEST_RESULT_FAIL;
	}

	if (timer_period_test(TIM1, RCC_TIM1, tim1_clock) !=
	    SELFTEST_RESULT_PASS) {
		console_puts("tim1 period fail\r\n");
		return SELFTEST_RESULT_FAIL;
	}
	if (timer_period_test(TIM2, RCC_TIM2, tim2_clock) !=
	    SELFTEST_RESULT_PASS) {
		console_puts("tim2 period fail\r\n");
		return SELFTEST_RESULT_FAIL;
	}

	return SELFTEST_RESULT_PASS;
}

/* --- ADC1 --------------------------------------------------------------- */

#define ADC_WAIT_LOOPS 2000000u

SELFTEST_OPTIMIZE static bool adc_wait_flag(uint32_t flag) {
	uint32_t timeout;

	for (timeout = 0; timeout < ADC_WAIT_LOOPS; timeout++) {
		if (adc_get_flag(ADC1, flag) != 0u) {
			return true;
		}
	}
	return false;
}

SELFTEST_OPTIMIZE static bool adc_convert_regular(uint8_t channel,
						  uint16_t *value) {
	adc_set_channel(ADC1, channel, 1u);
	adc_clear_flag(ADC1, ADC_FLAG_EOC);
	adc_start_conversion_regular(ADC1);

	if (!adc_wait_flag(ADC_FLAG_EOC)) {
		return false;
	}

	*value = adc_read_regular(ADC1);
	adc_clear_flag(ADC1, ADC_FLAG_EOC);
	return true;
}

#define ADC_FAIL(code)                                                         \
	do {                                                                   \
		console_puts("adc fail=");                                     \
		console_putu((uint32_t)(code));                                \
		console_crlf();                                                \
		return SELFTEST_RESULT_FAIL;                                   \
	} while (0)

SELFTEST_OPTIMIZE static selftest_result_t test_adc(void) {
	uint16_t vcal_regular;
	uint16_t vcal_injected;

	rcc_periph_clock_enable(RCC_ADC1);

	adc_disable(ADC1);
	adc_set_right_aligned(ADC1);
	adc_set_single_conversion_mode(ADC1);
	adc_set_sample_time_on_all_channels(ADC1, ADC_SAMPLETIME_241CYCLES);

	if ((ADC_SAMPTR1(ADC1) & ADC_SAMPTR1_SMP_MASK) !=
	    ADC_SAMPTR1_SMP_MASK) {
		ADC_FAIL(1);
	}
	if ((ADC_SAMPTR2(ADC1) & ADC_SAMPTR2_SMP_MASK) !=
	    ADC_SAMPTR2_SMP_MASK) {
		ADC_FAIL(2);
	}

	/*
	 * CH32V003RM section 9.3.3 has no TSVREFE bit: the two internal
	 * channels are always connected and are selected by channel number.
	 */
	adc_set_calibration_voltage(ADC1, ADC_CALVOL_50PERCENT);
	if ((ADC_CTLR1(ADC1) & ADC_CTLR1_CALVOLSELECT_MASK) !=
	    ADC_CALVOL_50PERCENT) {
		ADC_FAIL(4);
	}

	adc_enable(ADC1);
	qingke_delay_us(10u);
	adc_reset_calibration(ADC1);
	adc_start_calibration(ADC1);
	if (!adc_is_calibration_complete(ADC1)) {
		ADC_FAIL(5);
	}

	if (!adc_convert_regular(9u, &vcal_regular)) {
		ADC_FAIL(6);
	}
	if (vcal_regular < 400u || vcal_regular > 620u) {
		console_puts("adc vcal=");
		console_putu(vcal_regular);
		console_crlf();
		ADC_FAIL(7);
	}

	/*
	 * Inject channel 9 at rank 1 and read IDATAR1.  CH32V003RM section
	 * 9.3.12 note: with JL=0 the injected sequencer converts JSQ4, not
	 * JSQ1.  Channel 9 therefore belongs in JSQ4[4:0] (bits 15..19).
	 */
	ADC_ISQR(ADC1) = (uint32_t)((9u & ADC_ISQR_JSQ_MASK) << 15);
	adc_clear_flag(ADC1, ADC_FLAG_JEOC);
	adc_start_conversion_injected(ADC1);
	if (!adc_wait_flag(ADC_FLAG_JEOC)) {
		ADC_FAIL(8);
	}
	vcal_injected = adc_read_injected(ADC1, 1u);
	adc_clear_flag(ADC1, ADC_FLAG_JEOC);
	if (vcal_injected < 400u || vcal_injected > 620u) {
		console_puts("adc vcal_i=");
		console_putu(vcal_injected);
		console_crlf();
		ADC_FAIL(9);
	}

	/* Register-only ADC paths: offset, trigger delay and interrupts. */
	adc_set_injected_offset(ADC1, 1u, 123u);
	if ((ADC_IOFR1(ADC1) & ADC_IOFR_JOFFSET_MASK) != 123u) {
		ADC_FAIL(10);
	}
	adc_enable_irq(ADC1, ADC_IRQ_EOC);
	if ((ADC_CTLR1(ADC1) & ADC_CTLR1_EOCIE) == 0u) {
		ADC_FAIL(11);
	}
	adc_disable_irq(ADC1, ADC_IRQ_EOC);
	adc_enable_dma(ADC1);
	if ((ADC_CTLR2(ADC1) & ADC_CTLR2_DMA) == 0u) {
		ADC_FAIL(12);
	}
	adc_disable_dma(ADC1);

	adc_disable(ADC1);
	rcc_periph_clock_disable(RCC_ADC1);

	return SELFTEST_RESULT_PASS;
}

/* --- SPI1 registers ----------------------------------------------------- */

SELFTEST_OPTIMIZE static selftest_result_t test_spi_regs(void) {
	uint16_t saved_ctlr1 = SPI_CTLR1(SPI1);
	uint16_t saved_ctlr2 = SPI_CTLR2(SPI1);
	uint16_t saved_crc = SPI_CRCR(SPI1);
	uint16_t mask;
	uint16_t value;

	rcc_periph_clock_enable(RCC_SPI1);

	mask = SPI_CTLR1_MSTR | SPI_CTLR1_SSM | SPI_CTLR1_SSI | SPI_CTLR1_CPOL |
	       SPI_CTLR1_CPHA | SPI_CTLR1_LSBFIRST | SPI_CTLR1_BR_MASK;
	value = SPI_CTLR1_MSTR | SPI_CTLR1_SSM | SPI_CTLR1_SSI |
		SPI_CTLR1_CPOL | SPI_CTLR1_CPHA | SPI_CTLR1_LSBFIRST |
		(SPI_BAUDRATE_PRESCALER_128 << SPI_CTLR1_BR_SHIFT);
	SPI_CTLR1(SPI1) = value;
	if ((SPI_CTLR1(SPI1) & mask) != value) {
		return SELFTEST_RESULT_FAIL;
	}

	mask = SPI_CTLR2_SSOE | SPI_CTLR2_RXDMAEN | SPI_CTLR2_TXDMAEN |
	       SPI_CTLR2_TXEIE;
	SPI_CTLR2(SPI1) = mask;
	if ((SPI_CTLR2(SPI1) & mask) != mask) {
		return SELFTEST_RESULT_FAIL;
	}

	SPI_CRCR(SPI1) = 0x1021u;
	if (SPI_CRCR(SPI1) != 0x1021u) {
		return SELFTEST_RESULT_FAIL;
	}

	SPI_CTLR1(SPI1) |= SPI_CTLR1_SPE;
	if ((SPI_CTLR1(SPI1) & SPI_CTLR1_SPE) == 0u) {
		return SELFTEST_RESULT_FAIL;
	}
	if (wait_mmio16((uint32_t)&SPI_STATR(SPI1), SPI_STATR_TXE,
			SPI_STATR_TXE, 100000u) == false) {
		return SELFTEST_RESULT_FAIL;
	}
	SPI_CTLR1(SPI1) &= (uint16_t)~SPI_CTLR1_SPE;

	SPI_CTLR1(SPI1) = saved_ctlr1;
	SPI_CTLR2(SPI1) = saved_ctlr2;
	SPI_CRCR(SPI1) = saved_crc;
	return SELFTEST_RESULT_PASS;
}

/* --- I2C1 registers ----------------------------------------------------- */

SELFTEST_OPTIMIZE static selftest_result_t test_i2c_regs(void) {
	uint16_t saved_ctlr1 = I2C_CTLR1(I2C1);
	uint16_t saved_ctlr2 = I2C_CTLR2(I2C1);
	uint16_t saved_ckcfgr = I2C_CKCFGR(I2C1);
	uint16_t saved_oaddr1 = I2C_OADDR1(I2C1);
	uint16_t saved_datar = I2C_DATAR(I2C1);
	uint16_t mask;

	rcc_periph_clock_enable(RCC_I2C1);

	I2C_CTLR2(I2C1) = 48u;
	if ((I2C_CTLR2(I2C1) & I2C_CTLR2_FREQ_MASK) != 48u) {
		return SELFTEST_RESULT_FAIL;
	}
	I2C_CKCFGR(I2C1) = 240u; /* 48 MHz / (2 * 100 kHz), standard mode */
	if ((I2C_CKCFGR(I2C1) & I2C_CKCFGR_CCR_MASK) != 240u) {
		return SELFTEST_RESULT_FAIL;
	}

	I2C_OADDR1(I2C1) = (uint16_t)(0x42u << 1);
	if ((I2C_OADDR1(I2C1) & I2C_OADDR1_ADD1_7) != (uint16_t)(0x42u << 1)) {
		return SELFTEST_RESULT_FAIL;
	}

	mask = I2C_CTLR1_PE | I2C_CTLR1_ENGC | I2C_CTLR1_ACK | I2C_CTLR1_ENPEC |
	       I2C_CTLR1_PEC;
	I2C_CTLR1(I2C1) = mask;
	if ((I2C_CTLR1(I2C1) & mask) != mask) {
		return SELFTEST_RESULT_FAIL;
	}

	I2C_DATAR(I2C1) = 0x5au;
	if ((I2C_DATAR(I2C1) & I2C_DATAR_MASK) != 0x5au) {
		return SELFTEST_RESULT_FAIL;
	}

	I2C_CTLR2(I2C1) |= I2C_CTLR2_ITEVTEN | I2C_CTLR2_ITBUFEN;
	if ((I2C_CTLR2(I2C1) & (I2C_CTLR2_ITEVTEN | I2C_CTLR2_ITBUFEN)) !=
	    (I2C_CTLR2_ITEVTEN | I2C_CTLR2_ITBUFEN)) {
		return SELFTEST_RESULT_FAIL;
	}

	I2C_CTLR1(I2C1) = saved_ctlr1;
	I2C_CTLR2(I2C1) = saved_ctlr2;
	I2C_CKCFGR(I2C1) = saved_ckcfgr;
	I2C_OADDR1(I2C1) = saved_oaddr1;
	I2C_DATAR(I2C1) = saved_datar;
	return SELFTEST_RESULT_PASS;
}

/* --- ESIG / EXTEN ------------------------------------------------------- */

SELFTEST_OPTIMIZE static selftest_result_t test_esig_exten(void) {
	uint8_t id[12];
	uint32_t saved_ctrl;
	uint32_t uid0;
	uint32_t uid1;
	uint32_t uid2;
	uint16_t flash_kb;

	flash_kb = esig_get_flash_size_kb();
	if (flash_kb == 0u || flash_kb == 0xffffu) {
		return SELFTEST_RESULT_FAIL;
	}

	uid0 = esig_get_unique_id_word(0u);
	uid1 = esig_get_unique_id_word(1u);
	uid2 = esig_get_unique_id_word(2u);
	if (uid0 == 0u && uid1 == 0u && uid2 == 0u) {
		return SELFTEST_RESULT_FAIL;
	}
	esig_get_unique_id(id);

	saved_ctrl = EXTEN_CTR(EXTEN);
	exten_clear_lockup_reset_flag(EXTEN);
	if (exten_get_lockup_reset_flag(EXTEN) != 0u) {
		EXTEN_CTR(EXTEN) = saved_ctrl;
		return SELFTEST_RESULT_FAIL;
	}
	exten_enable_lockup_reset(EXTEN);
	if ((EXTEN_CTR(EXTEN) & EXTEN_CTR_LOCKUP_EN) == 0u) {
		EXTEN_CTR(EXTEN) = saved_ctrl;
		return SELFTEST_RESULT_FAIL;
	}
	exten_disable_lockup_reset(EXTEN);
	exten_set_ldo_mode(EXTEN, EXTEN_LDO_MODE_NORMAL);
	if (exten_get_ldo_mode(EXTEN) != EXTEN_LDO_MODE_NORMAL) {
		EXTEN_CTR(EXTEN) = saved_ctrl;
		return SELFTEST_RESULT_FAIL;
	}
	EXTEN_CTR(EXTEN) = saved_ctrl;

	return SELFTEST_RESULT_PASS;
}

/* --- Registry ----------------------------------------------------------- */

const struct selftest_case selftest_cases[] = {
    {"rcc clock tree", test_rcc},
    {"systick reference", test_systick},
    {"gpio output/input", test_gpio},
    {"usart1 registers/tx", test_usart},
    {"dma1 mem2mem", test_dma},
    {"tim1/tim2", test_timers},
    {"adc1 internal channels", test_adc},
    {"spi1 registers", test_spi_regs},
    {"i2c1 registers", test_i2c_regs},
    {"esig/exten factory", test_esig_exten},
};

const unsigned int selftest_case_count =
    sizeof(selftest_cases) / sizeof(selftest_cases[0]);
