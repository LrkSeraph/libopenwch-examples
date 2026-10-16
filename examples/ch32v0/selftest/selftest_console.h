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

#ifndef SELFTEST_CONSOLE_H
#define SELFTEST_CONSOLE_H

#include <stdint.h>

/** Initialise USART1 on PD5 (TX), 115200 8N1, and enable GPIO/USART clocks. */
void console_init(void);

/** Write one character. */
void console_putc(char c);
/** Write a NUL-terminated string. */
void console_puts(const char *s);
/** Write an unsigned decimal number. */
void console_putu(uint32_t value);
/** Write a hexadecimal number with exactly @p digits digits. */
void console_puthex(uint32_t value, unsigned int digits);
/** Write CRLF. */
void console_crlf(void);

#endif /* SELFTEST_CONSOLE_H */
