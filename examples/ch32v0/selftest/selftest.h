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

#ifndef SELFTEST_H
#define SELFTEST_H

#include <stdbool.h>
#include <stdint.h>

/*
 * The self-test is deliberately size-constrained on CH32V003.  GCC lets an
 * individual function carry the size optimisation level that matters most for
 * this image, even if the surrounding build is configured differently.
 */
#if defined(__GNUC__)
#define SELFTEST_OPTIMIZE [[gnu::optimize("Os")]]
#else
#define SELFTEST_OPTIMIZE
#endif

/** Result of one self-test case. */
typedef enum {
	SELFTEST_RESULT_PASS = 0, /**< the case ran and passed */
	SELFTEST_RESULT_FAIL,	  /**< the case ran and failed */
	SELFTEST_RESULT_SKIP,	  /**< the case is not testable in this setup */
} selftest_result_t;

/** A function that runs one self-test case. */
typedef selftest_result_t (*selftest_fn_t)(void);

/** One registry entry. */
struct selftest_case {
	const char *name;
	selftest_fn_t run;
};

extern const struct selftest_case selftest_cases[];
extern const unsigned int selftest_case_count;

#endif /* SELFTEST_H */
