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
 * CH32V003 timer self-test.
 *
 * CH32V003 has TIM1 (advanced-control, APB2) and TIM2 (general-purpose,
 * APB1).  Both are configured as 1 Hz time bases; the QingKe SysTick counter
 * measures one full update period for each timer and prints PASS/WARN on
 * USART1.
 *
 * Wiring: PD5 = USART1 TX, 115200 8N1, partial remap 1.
 */

#include <stdbool.h>
#include <stdint.h>

#include <libopenwch/ch32v0/gpio.h>
#include <libopenwch/ch32v0/rcc.h>
#include <libopenwch/ch32v0/tim.h>
#include <libopenwch/ch32v0/usart.h>
#include <libopenwch/qingke/systick.h>

#define UART_BAUD 115200u

#define TIMER_PRESCALER 4799u /* 48 MHz / 4800 = 10 kHz */
#define EXPECTED_PERIOD_US 1000000u
#define PERIOD_TOLERANCE_US 5000u

static void uart_init(void) {
	rcc_periph_clock_enable(RCC_USART1);
	rcc_periph_clock_enable(RCC_AFIO);
	rcc_periph_clock_enable(RCC_GPIOD);

	/* USART1 default pins: TX=PD5, RX=PD6. */
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

static void reference_clock_init(void) {
	qingke_systick_set_frequency(rcc_sysclk_frequency);
	systick_set_clock_source(1); /* run from HCLK */
	systick_clear_interrupt();
	systick_enable_counter();
}

/**
 * APB timers run at PCLK when the APB prescaler is 1, and at twice PCLK
 * otherwise.
 */
static uint32_t timer_clock_hz(uint32_t pclk, uint32_t ahb) {
	uint32_t clock = pclk;

	if (pclk != ahb) {
		clock *= 2u;
	}

	return clock;
}

static bool timer_period_test(uint32_t tim,
			      uint32_t rcc_clock,
			      const char *name,
			      uint32_t clock_hz) {
	uint32_t period = (clock_hz / (TIMER_PRESCALER + 1u)) - 1u;
	uint64_t start;
	uint64_t end;
	uint64_t elapsed_us;
	bool pass;

	rcc_periph_clock_enable(rcc_clock);

	timer_disable(tim);
	timer_set_prescaler(tim, (uint16_t)TIMER_PRESCALER);
	timer_set_period(tim, (uint16_t)period);
	timer_set_mode(tim, TIM_MODE_EDGE_ALIGNED | TIM_MODE_UP |
				TIM_MODE_UPDATE_OVERFLOW);
	timer_set_counter(tim, 0);
	timer_generate_event(tim, TIM_EVENT_UPDATE);
	timer_clear_flag(tim, TIM_FLAG_UPDATE);
	timer_enable(tim);

	uart_puts(name);
	uart_puts(" clock = ");
	uart_putu(clock_hz);
	uart_puts(" Hz, psc = ");
	uart_putu(TIMER_PRESCALER);
	uart_puts(", arr = ");
	uart_putu(period);
	uart_puts("\r\n");

	/* Synchronise to an update edge, then time the next full period. */
	while (timer_get_flag(tim, TIM_FLAG_UPDATE) == 0u) {
		;
	}

	timer_clear_flag(tim, TIM_FLAG_UPDATE);
	start = systick_get_counter();

	while (timer_get_flag(tim, TIM_FLAG_UPDATE) == 0u) {
		;
	}

	end = systick_get_counter();
	timer_clear_flag(tim, TIM_FLAG_UPDATE);
	timer_disable(tim);

	elapsed_us =
	    ((end - start) * 1000000ull) / (uint64_t)rcc_sysclk_frequency;

	pass = (elapsed_us + PERIOD_TOLERANCE_US >= EXPECTED_PERIOD_US) &&
	       (elapsed_us <= EXPECTED_PERIOD_US + PERIOD_TOLERANCE_US);

	uart_puts(name);
	uart_puts(": period = ");
	uart_putu((uint32_t)elapsed_us);
	uart_puts(" us, cnt = ");
	uart_putu(timer_get_counter(tim));
	uart_puts(pass ? ", PASS\r\n" : ", WARN (expected 1000000 us)\r\n");

	return pass;
}

int main(void) {
	uint32_t tim1_clock;
	uint32_t tim2_clock;

	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_PLL_HSI_48MHZ]);

	uart_init();
	reference_clock_init();

	tim1_clock = timer_clock_hz(rcc_apb2_frequency, rcc_ahb_frequency);
	tim2_clock = timer_clock_hz(rcc_apb1_frequency, rcc_ahb_frequency);

	uart_puts("\r\nlibopenwch timer self-test\r\n");

	for (;;) {
		bool tim1_ok;
		bool tim2_ok;

		tim1_ok = timer_period_test(TIM1, RCC_TIM1, "TIM1", tim1_clock);
		tim2_ok = timer_period_test(TIM2, RCC_TIM2, "TIM2", tim2_clock);

		uart_puts(tim1_ok && tim2_ok ? "all timers: PASS\r\n"
					     : "all timers: WARN\r\n");

		qingke_delay_ms(500u);
	}

	/* Not reached. */
	return 0;
}
