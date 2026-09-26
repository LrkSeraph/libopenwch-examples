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
## CH58x UART counter.
##
## Prints chip id and sysclk, then sends a counter once per second on
## PA8 (TX) at 115200 8N1.
##

PROJECT= ch582_uart_counter
DEVICE?= ch582m

TEMPLATE_DIR?= $(abspath ../../..)

CFILES= main.c

include $(TEMPLATE_DIR)/rules/rules.mk
