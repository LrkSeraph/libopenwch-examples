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
## CH32V003 TIM2 period self-test.
##
## TIM2 generates a 1 Hz update event; the measured period is printed on
## USART1 (PD5, 115200 8N1).
##

PROJECT= timer_selftest_uart

DEVICE?= ch32v003f4p6

TEMPLATE_DIR?= $(abspath ../../..)

CFILES= main.c

include $(TEMPLATE_DIR)/rules/rules.mk
