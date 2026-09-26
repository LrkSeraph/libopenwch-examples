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
## CH58x bring-up: start the 32 MHz crystal, run the PLL at 48 MHz and drive a
## pin.  This is the CH58x counterpart of the CH32V003 blink.
##
## NOTE: the CH58x peripheral drivers (including the RWA safe-access sequence
## that the clock and GPIO registers require) are the P3 milestone and are not
## implemented yet.  Until then this example exercises only the core layer:
## vector table, interrupt control and delay, which is already enough to prove
## that the family links and boots with the right memory layout.
##

PROJECT		= ch582_blink

## CH582/CH583/CH584/CH585 all resolve to the ch5xx58x family.
DEVICE		?= ch582m

TEMPLATE_DIR	?= $(abspath ../../..)

CFILES		= main.c

include $(TEMPLATE_DIR)/rules/rules.mk
