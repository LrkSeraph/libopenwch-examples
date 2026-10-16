##
## This file is part of the libopenwch examples.
##
## Copyright (C) 2025 libopenwch contributors
##
## This library is free software: you can redistribute it and/or modify
## it under the terms of the GNU Lesser General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This library is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public License
## along with this library.  If not, see <http://www.gnu.org/licenses/>.
##

##
## CH32V003 integrated self-test.
##
## Runs RCC, SysTick, GPIO, USART, DMA, TIM1/TIM2, ADC, SPI, I2C and
## ESIG/EXTEN cases in one image, then verifies IWDG and WWDG reset
## behaviour.  Progress is printed on USART1 (PD5, 115200 8N1).
##

PROJECT= selftest

DEVICE?= ch32v003f4p6

TEMPLATE_DIR?= $(abspath ../../..)

CFILES= selftest_main.c selftest_console.c selftest_cases.c

## A self-test has no useful recovery path if an assert fires; prefer compact
## code and explicit PASS/FAIL reporting in this image.  The sources also
## carry SELFTEST_OPTIMIZE (gnu::optimize("Os")) so the size contract is visible
## in the code rather than only in this Makefile.
OPT= -Os
CFLAGS += -DNDEBUG

include $(TEMPLATE_DIR)/rules/rules.mk
