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
 * CH32V003 TIM2 period self-test.
 *
 * TIM2 is configured for a 1 Hz update event.  The main loop polls the update
 * flag, measures the interval with the QingKe SysTick counter, and prints the
 * measured period on USART1.  The measured value should stay close to
 * 1,000,000 us; a warning is printed if it does not.
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

/* The timer should produce one update per second. */
#define EXPECTED_PERIOD_US 1000000u
#define PERIOD_TOLERANCE_US 5000u

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

static void reference_clock_init(void) {
	qingke_systick_set_frequency(rcc_sysclk_frequency);
	systick_set_clock_source(1); /* run from HCLK */
	systick_clear_interrupt();
	systick_enable_counter();
}

/**
 * TIM2 is on APB1.  When APB1 is prescaled, the timer clock is twice PCLK1;
 * when it is not, the timer runs directly from PCLK1.
 */
static uint32_t timer_clock_hz(void) {
	uint32_t clock = rcc_apb1_frequency;

	if (rcc_apb1_frequency != rcc_ahb_frequency) {
		clock *= 2u;
	}

	return clock;
}

static void tim2_init(uint32_t clock_hz) {
	uint32_t prescaler = 4799u; /* 48 MHz / 4800 = 10 kHz */
	uint32_t period = (clock_hz / (prescaler + 1u)) - 1u;

	rcc_periph_clock_enable(RCC_TIM2);

	timer_disable(TIM2);
	timer_set_prescaler(TIM2, (uint16_t)prescaler);
	timer_set_period(TIM2, (uint16_t)period);
	timer_set_mode(TIM2, TIM_MODE_EDGE_ALIGNED | TIM_MODE_UP |
				 TIM_MODE_UPDATE_OVERFLOW);
	timer_set_counter(TIM2, 0);
	timer_generate_event(TIM2, TIM_EVENT_UPDATE);
	timer_clear_flag(TIM2, TIM_FLAG_UPDATE);
	timer_enable(TIM2);

	uart_puts("TIM2 clock = ");
	uart_putu(clock_hz);
	uart_puts(" Hz\r\npsc = ");
	uart_putu(prescaler);
	uart_puts(", arr = ");
	uart_putu(period);
	uart_puts(", expected = 1 Hz\r\n");
}

int main(void) {
	uint32_t timer_clock;
	uint32_t tick = 0;
	uint32_t failures = 0;

	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_PLL_HSI_48MHZ]);

	uart_init();
	reference_clock_init();

	timer_clock = timer_clock_hz();
	tim2_init(timer_clock);

	uart_puts("\r\nlibopenwch TIM2 self-test\r\n");

	timer_clear_flag(TIM2, TIM_FLAG_UPDATE);

	for (;;) {
		uint64_t start;
		uint64_t end;
		uint64_t elapsed_us;

		/* Synchronise to an update edge, then time the next full period. */
		while (timer_get_flag(TIM2, TIM_FLAG_UPDATE) == 0u) {
			;
		}

		timer_clear_flag(TIM2, TIM_FLAG_UPDATE);
		start = systick_get_counter();

		while (timer_get_flag(TIM2, TIM_FLAG_UPDATE) == 0u) {
			;
		}

		end = systick_get_counter();
		timer_clear_flag(TIM2, TIM_FLAG_UPDATE);

		elapsed_us = ((end - start) * 1000000ull) /
			     (uint64_t)rcc_sysclk_frequency;

		tick++;

		uart_puts("tick ");
		uart_putu(tick);
		uart_puts(": period = ");
		uart_putu((uint32_t)elapsed_us);
		uart_puts(" us, cnt = ");
		uart_putu(timer_get_counter(TIM2));

		if ((elapsed_us + PERIOD_TOLERANCE_US >= EXPECTED_PERIOD_US) &&
		    (elapsed_us <= EXPECTED_PERIOD_US + PERIOD_TOLERANCE_US)) {
			uart_puts(", PASS\r\n");
		} else {
			failures++;
			uart_puts(", WARN (expected 1000000 us; failures = ");
			uart_putu(failures);
			uart_puts(")\r\n");
		}
	}

	/* Not reached. */
	return 0;
}
