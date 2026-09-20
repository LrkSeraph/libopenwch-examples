##
## This file is part of the libopenwch template.
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
## USART echo for a CH32V003 board.
##
## Wiring: PD5 = TX, PD6 = RX at 115200 8N1.  Those are the "partial remap 1"
## pins, which is what the WCH evaluation board breaks out.
##

PROJECT		= uart_echo
DEVICE		?= ch32v003f4p6

TEMPLATE_DIR	?= $(abspath ../..)

CFILES		= main.c

include $(TEMPLATE_DIR)/rules/rules.mk
