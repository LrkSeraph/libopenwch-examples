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
## Blink for a CH32V003 board with an active-low LED on PC2.
##
## This is the smallest complete libopenwch application.  Copy this directory
## to start a new project.
##

PROJECT		= blink

## The part number drives -march/-mabi and the linker script.  Override on the
## command line to build for a sibling part, e.g.
##	make DEVICE=ch32v002f4p6
DEVICE		?= ch32v003f4p6

## Where the shared rules live, and where libopenwch is.
TEMPLATE_DIR	?= $(abspath ../../..)

## Sources.  Add more with CFILES += other.c
CFILES		= main.c

## Extra settings, e.g.
##	CFLAGS += -DMY_OPTION=1
##	INCLUDES += -I../shared
##	DEFS += -DBOARD_REV=2

include $(TEMPLATE_DIR)/rules/rules.mk
