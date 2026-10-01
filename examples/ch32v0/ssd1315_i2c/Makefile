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
## SSD1315 128x64 OLED on I2C1 (address 0x78).
##
## Pin mapping:
##   SSD1315_I2C_PINS=remap    PC5 = SCL, PC6 = SDA (default)
##   SSD1315_I2C_PINS=default  PC2 = SCL, PC1 = SDA
##

PROJECT= ssd1315_i2c
DEVICE?= ch32v003f4p6
TEMPLATE_DIR?= $(abspath ../../..)
CFILES= main.c

SSD1315_I2C_PINS ?= remap

ifeq ($(SSD1315_I2C_PINS),remap)
DEFS += -DSSD1315_I2C_REMAP=1
else ifeq ($(SSD1315_I2C_PINS),default)
DEFS += -DSSD1315_I2C_REMAP=0
else
$(error SSD1315_I2C_PINS must be remap or default)
endif

include $(TEMPLATE_DIR)/rules/rules.mk
