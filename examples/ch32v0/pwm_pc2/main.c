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
 * CH32V003 PWM example on PC2.
 *
 * PC2 can be routed to TIM2_CH2 with TIM2 partial remap 1.  This example
 * generates a 1 kHz PWM signal and sweeps the duty cycle from 0 % to 100 %
 * and back.  The current duty is also printed on USART1.
 *
 * Wiring:
 *   PC2 = TIM2_CH2 PWM output
 *   PD5 = USART1 TX, 115200 8N1, partial remap 1
 *
 * On boards where PC2 drives an active-low LED to 3.3 V, the visible
 * brightness is roughly inverse to the duty cycle because the LED lights when
 * PC2 is low.
 */

#include <stdint.h>

#include <libopenwch/ch32v0/gpio.h>
#include <libopenwch/ch32v0/rcc.h>
#include <libopenwch/ch32v0/tim.h>
#include <libopenwch/ch32v0/usart.h>
#include <libopenwch/qingke/systick.h>

#define UART_BAUD 115200u

#define PWM_TIMER TIM2
#define PWM_OC TIM_OC2
#define PWM_PERIOD 999u
#define PWM_MAX_DUTY 1000u
#define PWM_STEP 10u
#define PWM_DELAY_MS 20u

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

static void delay_init(void) {
	qingke_systick_set_frequency(rcc_sysclk_frequency);
	systick_set_clock_source(1); /* run from HCLK */
	systick_clear_interrupt();
	systick_enable_counter();
}

static void pwm_init(void) {
	rcc_periph_clock_enable(RCC_AFIO);
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_TIM2);

	/* PC2 = TIM2_CH2, available with TIM2 partial remap 1. */
	gpio_tim2_remap(GPIO_REMAP_TIM2_PARTIAL1);
	gpio_set_mode(GPIOC, GPIO_MODE_AF_PP, GPIO2);

	timer_disable(PWM_TIMER);
	timer_set_prescaler(PWM_TIMER, 47u);	 /* 48 MHz / 48 = 1 MHz */
	timer_set_period(PWM_TIMER, PWM_PERIOD); /* 1 MHz / 1000 = 1 kHz */
	timer_set_mode(PWM_TIMER, TIM_MODE_EDGE_ALIGNED | TIM_MODE_UP);
	timer_set_oc_mode(PWM_TIMER, PWM_OC, TIM_OC_MODE_PWM1);
	timer_set_oc_value(PWM_TIMER, PWM_OC, 0u);
	timer_enable_oc_preload(PWM_TIMER, PWM_OC);
	timer_enable_oc_output(PWM_TIMER, PWM_OC);
	timer_generate_event(PWM_TIMER, TIM_EVENT_UPDATE);
	timer_clear_flag(PWM_TIMER, TIM_FLAG_UPDATE);
	timer_enable(PWM_TIMER);
}

static void pwm_set_duty(uint16_t duty) {
	timer_set_oc_value(PWM_TIMER, PWM_OC, duty);
}

static void print_duty(uint16_t duty) {
	uart_puts("PWM PC2 TIM2_CH2: duty = ");
	uart_putu(duty);
	uart_puts("/");
	uart_putu(PWM_MAX_DUTY);
	uart_puts(" (");
	uart_putu(((uint32_t)duty * 100u) / PWM_MAX_DUTY);
	uart_puts("%)\r\n");
}

int main(void) {
	int32_t duty = 0;
	int32_t step = PWM_STEP;

	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_PLL_HSI_48MHZ]);

	uart_init();
	delay_init();
	pwm_init();

	uart_puts("\r\nlibopenwch PWM example on PC2\r\n");
	uart_puts("TIM2_CH2, 1 kHz, duty 0..100%\r\n");

	for (;;) {
		pwm_set_duty((uint16_t)duty);

		if ((duty % 100) == 0) {
			print_duty((uint16_t)duty);
			qingke_delay_ms(100u);
		} else {
			qingke_delay_ms(PWM_DELAY_MS);
		}

		duty += step;

		if (duty >= (int32_t)PWM_MAX_DUTY) {
			duty = (int32_t)PWM_MAX_DUTY;
			step = -PWM_STEP;
		} else if (duty <= 0) {
			duty = 0;
			step = PWM_STEP;
		}
	}

	/* Not reached. */
	return 0;
}
