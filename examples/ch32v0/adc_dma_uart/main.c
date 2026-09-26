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
 * CH32V003 ADC + DMA + UART test.
 *
 * PA1 is ADC channel 1.  The ADC runs in continuous conversion mode and DMA1
 * channel 1 moves each conversion result into a RAM block.  When a block is
 * complete the foreground averages it and prints the raw mean and an
 * approximate millivolt value on USART1.
 *
 * Wiring: PA1 = analog input (0..3.3 V), PD5 = USART1 TX, 115200 8N1.
 * USART1 is on partial remap 1, as on the WCH EVT board.
 */

#include <libopenwch/ch32v0/adc.h>
#include <libopenwch/ch32v0/dma.h>
#include <libopenwch/ch32v0/gpio.h>
#include <libopenwch/ch32v0/rcc.h>
#include <libopenwch/ch32v0/usart.h>

#define UART_BAUD 115200u

/* PA1 is ADC channel 1 on the CH32V003.  Its ADC is 10-bit. */
#define ADC_CHANNEL_PA1 1u
#define ADC_SAMPLE_COUNT 128u
#define ADC_FULL_SCALE 1023u
#define ADC_VREF_MV 3300u

/* DMA1 channel 1 is the hard-wired ADC1 request channel. */
#define ADC_DMA_CHANNEL DMA_CHANNEL1

static volatile uint16_t adc_samples[ADC_SAMPLE_COUNT];

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

/** Average a block of right-aligned 12-bit ADC samples. */
static uint16_t adc_mean(const volatile uint16_t *samples, uint32_t count) {
	uint32_t sum = 0;
	uint32_t i;

	for (i = 0; i < count; i++) {
		sum += samples[i];
	}

	return (uint16_t)((sum + count / 2u) / count);
}

static void adc_dma_init(void) {
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_ADC1);
	rcc_periph_clock_enable(RCC_DMA1);

	/*
 * Keep ADCCLK at 6 MHz for a 48 MHz APB2 clock, matching WCH's EVT
 * examples.  The reset prescaler of /2 would be 24 MHz.
 */
	rcc_adc_set_prescaler(RCC_CFGR0_ADCPRE_DIV8);
	rcc_measure_clocks();

	/* PA1 as analog input. */
	gpio_set_mode(GPIOA, GPIO_MODE_AIN, GPIO1);

	adc_disable(ADC1);
	adc_set_right_aligned(ADC1);
	adc_set_continuous_conversion_mode(ADC1);
	adc_set_channel(ADC1, ADC_CHANNEL_PA1, 1);
	adc_set_sample_time(ADC1, ADC_CHANNEL_PA1, ADC_SAMPLETIME_241CYCLES);
	adc_set_external_trigger_regular(ADC1, ADC_EXTTRIG_REGULAR_NONE);
	adc_disable_external_trigger_regular(ADC1);
	adc_set_calibration_voltage(ADC1, ADC_CALVOL_50PERCENT);

	adc_enable(ADC1);
	adc_reset_calibration(ADC1);
	adc_start_calibration(ADC1);
	adc_enable_dma(ADC1);

	/*
 * Normal (non-circular) DMA: move one block, then stop.  The main loop
 * averages the stable block and re-arms the channel for the next one.
 */
	dma_channel_reset(DMA1, ADC_DMA_CHANNEL);
	dma_set_peripheral_address(DMA1, ADC_DMA_CHANNEL,
				   (uint32_t)&ADC_RDATAR(ADC1));
	dma_set_memory_address(DMA1, ADC_DMA_CHANNEL, (uint32_t)adc_samples);
	dma_set_number_of_data(DMA1, ADC_DMA_CHANNEL, ADC_SAMPLE_COUNT);
	dma_set_read_from_peripheral(DMA1, ADC_DMA_CHANNEL);
	dma_disable_peripheral_increment_mode(DMA1, ADC_DMA_CHANNEL);
	dma_enable_memory_increment_mode(DMA1, ADC_DMA_CHANNEL);
	dma_set_peripheral_size(DMA1, ADC_DMA_CHANNEL, DMA_SIZE_16BIT);
	dma_set_memory_size(DMA1, ADC_DMA_CHANNEL, DMA_SIZE_16BIT);
	dma_set_priority(DMA1, ADC_DMA_CHANNEL, DMA_PRIORITY_HIGH);

	dma_channel_enable(DMA1, ADC_DMA_CHANNEL);
	adc_start_conversion_regular(ADC1);
}

/** Re-arm DMA for another block; ADC continuous conversion keeps running. */
static void adc_dma_restart(void) {
	dma_channel_disable(DMA1, ADC_DMA_CHANNEL);
	dma_clear_interrupt_pending_bit(DMA1, ADC_DMA_CHANNEL);
	dma_set_memory_address(DMA1, ADC_DMA_CHANNEL, (uint32_t)adc_samples);
	dma_set_number_of_data(DMA1, ADC_DMA_CHANNEL, ADC_SAMPLE_COUNT);
	dma_channel_enable(DMA1, ADC_DMA_CHANNEL);
}

int main(void) {
	uint16_t mean;
	uint32_t millivolts;

	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_PLL_HSI_48MHZ]);

	uart_init();
	adc_dma_init();

	uart_puts("\r\nlibopenwch ADC DMA test: PA1\r\n");

	for (;;) {
		/* Wait until DMA has moved one complete block. */
		while (dma_get_flag(DMA1, ADC_DMA_CHANNEL, DMA_TCIF) == 0u) {
			;
		}

		/* Stop DMA before reading so the block cannot change under us. */
		dma_channel_disable(DMA1, ADC_DMA_CHANNEL);
		dma_clear_flag(DMA1, ADC_DMA_CHANNEL, DMA_TCIF);

		mean = adc_mean(adc_samples, ADC_SAMPLE_COUNT);
		millivolts =
		    ((uint32_t)mean * ADC_VREF_MV + ADC_FULL_SCALE / 2u) /
		    ADC_FULL_SCALE;

		uart_puts("PA1 mean raw = ");
		uart_putu(mean);
		uart_puts(" (");
		uart_putu(millivolts);
		uart_puts(" mV)\r\n");

		adc_dma_restart();
	}

	/* Not reached. */
	return 0;
}
