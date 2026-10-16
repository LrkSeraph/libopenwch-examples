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
 * CH32V003 integrated self-test.
 *
 * One firmware image runs every non-destructive module case, then uses the
 * independent and window watchdogs to prove that both can reset the part.
 * The watchdog phase machine is kept in .noinit so it survives those resets;
 * normal module results are stored there as well and printed in the final
 * summary.
 *
 * Wiring: PD5 = USART1 TX, 115200 8N1.
 */

#include <stdbool.h>
#include <stdint.h>

#include <libopenwch/ch32v0/gpio.h>
#include <libopenwch/ch32v0/iwdg.h>
#include <libopenwch/ch32v0/rcc.h>
#include <libopenwch/ch32v0/wwdg.h>
#include <libopenwch/qingke/sync.h>
#include <libopenwch/qingke/systick.h>

#include "selftest.h"
#include "selftest_console.h"

#pragma GCC optimize("Os")

#define WD_MAGIC 0x53454c46u /* "SELF" */
#define WD_STATE_PHASE_DONE 3u

struct selftest_counts {
	uint32_t pass;
	uint32_t fail;
	uint32_t skip;
};

struct watchdog_state {
	uint32_t magic;
	uint32_t magic_inv;
	uint32_t phase;
	uint32_t pass;
	uint32_t fail;
	uint32_t skip;
};

/* Reset does not clear .noinit, so this survives watchdog resets. */
static volatile struct watchdog_state wd_state
    __attribute__((section(".noinit")));

SELFTEST_OPTIMIZE static bool wd_state_valid(void) {
	return wd_state.magic == WD_MAGIC && wd_state.magic_inv == ~WD_MAGIC;
}

SELFTEST_OPTIMIZE static void wd_state_reset(void) {
	wd_state.magic = WD_MAGIC;
	wd_state.magic_inv = ~WD_MAGIC;
	wd_state.phase = 0u;
	wd_state.pass = 0u;
	wd_state.fail = 0u;
	wd_state.skip = 0u;
}

SELFTEST_OPTIMIZE static void reference_clock_init(void) {
	qingke_systick_set_frequency(rcc_sysclk_frequency);
	systick_set_clock_source(1u); /* HCLK */
	systick_clear_interrupt();
	systick_enable_counter();
}

SELFTEST_OPTIMIZE static struct selftest_counts run_normal_tests(void) {
	struct selftest_counts counts = {0u, 0u, 0u};
	unsigned int i;

	console_puts("\r\nnormal module cases\r\n");

	for (i = 0; i < selftest_case_count; i++) {
		selftest_result_t result;

		result = selftest_cases[i].run();

		switch (result) {
		case SELFTEST_RESULT_PASS:
			counts.pass++;
			console_puts("pass ");
			break;
		case SELFTEST_RESULT_SKIP:
			counts.skip++;
			console_puts("skip ");
			break;
		default:
			counts.fail++;
			console_puts("fail ");
			break;
		}

		console_puts(selftest_cases[i].name);
		console_crlf();
	}

	return counts;
}

SELFTEST_OPTIMIZE static bool wait_lsi_ready(void) {
	uint32_t timeout;

	rcc_osc_on(RCC_OSC_LSI);
	for (timeout = 0; timeout < 1000000u; timeout++) {
		if ((RCC_RSTSCKR & RCC_RSTSCKR_LSIRDY) != 0u) {
			return true;
		}
	}
	return false;
}

SELFTEST_OPTIMIZE static bool wait_iwdg_flag(uint16_t flag) {
	uint32_t timeout;

	for (timeout = 0; timeout < 1000000u; timeout++) {
		if ((iwdg_get_flag(IWDG, flag)) == 0u) {
			return true;
		}
	}
	return false;
}

SELFTEST_OPTIMIZE static bool arm_iwdg_reset(void) {
	console_puts("\r\nIWDG: enable without reload; reset expected\r\n");

	if (!wait_lsi_ready()) {
		console_puts("IWDG: FAIL (LSI did not become ready)\r\n");
		return false;
	}

	iwdg_write_access_enable(IWDG);
	iwdg_set_prescaler(IWDG, IWDG_PSCR_DIV128);
	if (!wait_iwdg_flag(IWDG_FLAG_PVU)) {
		console_puts("IWDG: FAIL (prescaler update timeout)\r\n");
		return false;
	}
	iwdg_set_reload(IWDG, 500u);
	if (!wait_iwdg_flag(IWDG_FLAG_RVU)) {
		console_puts("IWDG: FAIL (reload update timeout)\r\n");
		return false;
	}
	iwdg_reload_counter(IWDG);
	iwdg_enable(IWDG);

	for (;;) {
		qingke_nop();
	}

	/* Not reached. */
	return true;
}

SELFTEST_OPTIMIZE static void arm_wwdg_reset(void) {
	console_puts("\r\nWWDG: enable without refresh; reset expected\r\n");

	rcc_periph_clock_enable(RCC_WWDG);
	wwdg_set_counter(WWDG, 0x7fu);
	wwdg_set_prescaler(WWDG, WWDG_PRESCALER_8);
	wwdg_set_window(WWDG, 0x40u);
	wwdg_clear_flag(WWDG);
	wwdg_enable(WWDG, 0x7fu);

	for (;;) {
		qingke_nop();
	}
}

SELFTEST_OPTIMIZE static void print_final_summary(void) {
	console_puts("\r\nsummary pass=");
	console_putu(wd_state.pass);
	console_puts(" fail=");
	console_putu(wd_state.fail);
	console_puts(" skip=");
	console_putu(wd_state.skip);
	console_crlf();

	console_puts("integrated self-test: ");
	console_puts(wd_state.fail == 0u ? "PASS\r\n" : "FAIL\r\n");
}

SELFTEST_OPTIMIZE int main(void) {
	uint32_t reset_flags;

	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_PLL_HSI_48MHZ]);
	console_init();
	reference_clock_init();

	reset_flags = rcc_get_reset_flags();

	/*
 * A power-on, reset-pin, software or low-power reset starts a fresh
 * session.  The watchdog phases deliberately keep the magic and only
 * accept IWDG/WWDG reset flags.
 */
	if (!wd_state_valid() ||
	    (reset_flags & (RCC_RSTSCKR_PINRSTF | RCC_RSTSCKR_PORRSTF |
			    RCC_RSTSCKR_SFTRSTF | RCC_RSTSCKR_LPWRRSTF)) !=
		0u) {
		wd_state_reset();
		rcc_clear_reset_flags();
		reset_flags = 0u;
	}

	console_puts("\r\nlibopenwch CH32V003 integrated self-test\r\n");
	console_puts("reset flags = 0x");
	console_puthex(reset_flags, 8u);
	console_crlf();

	if (wd_state.phase == 0u) {
		struct selftest_counts counts = run_normal_tests();

		wd_state.pass = counts.pass;
		wd_state.fail = counts.fail;
		wd_state.skip = counts.skip;

		rcc_clear_reset_flags();
		wd_state.phase = 1u;
		if (!arm_iwdg_reset()) {
			wd_state.fail++;
			wd_state.phase = WD_STATE_PHASE_DONE;
		}
	}

	if (wd_state.phase == 1u) {
		if ((reset_flags & RCC_RSTSCKR_IWDGRSTF) != 0u) {
			wd_state.pass++;
			console_puts("IWDG: PASS\r\n");
			rcc_clear_reset_flags();
			wd_state.phase = 2u;
			arm_wwdg_reset();
		} else {
			wd_state.fail++;
			console_puts("IWDG: FAIL (unexpected reset flags)\r\n");
			wd_state.phase = WD_STATE_PHASE_DONE;
		}
	}

	if (wd_state.phase == 2u) {
		if ((reset_flags & RCC_RSTSCKR_WWDGRSTF) != 0u) {
			wd_state.pass++;
			console_puts("WWDG: PASS\r\n");
			rcc_clear_reset_flags();
			wd_state.phase = WD_STATE_PHASE_DONE;
		} else {
			wd_state.fail++;
			console_puts("WWDG: FAIL (unexpected reset flags)\r\n");
			wd_state.phase = WD_STATE_PHASE_DONE;
		}
	}

	if (wd_state.phase >= WD_STATE_PHASE_DONE) {
		print_final_summary();
	}

	for (;;) {
		qingke_nop();
	}

	/* Not reached. */
	return 0;
}
