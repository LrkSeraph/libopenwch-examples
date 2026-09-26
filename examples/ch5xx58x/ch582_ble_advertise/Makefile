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
## CH582M Bluetooth LE peripheral.
##
## LIBOPENWCH_BLE=1 links WCH's closed-source Bluetooth stack, which is what
## the ble_* layer calls into.  Without it the link fails with undefined
## references to BLE_LibInit and friends, which is the intended failure mode:
## the stack is not part of libopenwch_ch5xx58x.a.
##
##   make LIBOPENWCH_BLE=1
##
## The default is 0, so an application that does not use Bluetooth never pulls
## the stack in.
##

PROJECT		= ch582_ble_advertise

## Only the CH58x family has a Bluetooth stack.
DEVICE		?= ch582m

TEMPLATE_DIR	?= $(abspath ../../..)

CFILES		= main.c

## The stack needs memcpy(), and this example is built freestanding so that it
## works on toolchains without newlib (Debian's, for instance).
LIBOPENWCH_BLE		?= 1
LIBOPENWCH_NOSTDLIB	?= 1

include $(TEMPLATE_DIR)/rules/rules.mk
