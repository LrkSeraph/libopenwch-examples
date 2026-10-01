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
## Pin mapping (`SSD1315_I2C_PINS`):
##   0 / default   PC2 = SCL, PC1 = SDA
##   1 / partial   PD1 = SCL, PD0 = SDA
##   2 / full      PC5 = SCL, PC6 = SDA (default)
##

PROJECT		= ssd1315_i2c
DEVICE		?= ch32v003f4p6
TEMPLATE_DIR	?= $(abspath ../../..)
CFILES		= main.c

SSD1315_I2C_PINS ?= 2

ifeq ($(SSD1315_I2C_PINS),0)
SSD1315_I2C_MAP_INDEX := 0
else ifeq ($(SSD1315_I2C_PINS),default)
SSD1315_I2C_MAP_INDEX := 0
else ifeq ($(SSD1315_I2C_PINS),1)
SSD1315_I2C_MAP_INDEX := 1
else ifeq ($(SSD1315_I2C_PINS),partial)
SSD1315_I2C_MAP_INDEX := 1
else ifeq ($(SSD1315_I2C_PINS),2)
SSD1315_I2C_MAP_INDEX := 2
else ifeq ($(SSD1315_I2C_PINS),full)
SSD1315_I2C_MAP_INDEX := 2
else ifeq ($(SSD1315_I2C_PINS),remap)
SSD1315_I2C_MAP_INDEX := 2
else
$(error SSD1315_I2C_PINS must be 0/default, 1/partial or 2/full/remap)
endif

DEFS += -DSSD1315_I2C_MAP=$(SSD1315_I2C_MAP_INDEX)

include $(TEMPLATE_DIR)/rules/rules.mk

## Rebuild only when the pin-selection value changes.  DEFS is not tracked by
## make, so a small stamp records the selected mapping and is a prerequisite
## of the objects.
PINS_STAMP = $(BUILD_DIR)/.ssd1315_i2c_pins

$(PINS_STAMP): FORCE
	@mkdir -p $(dir $@)
	@printf '%s\n' '$(SSD1315_I2C_MAP_INDEX)' > $@.new
	@cmp -s $@.new $@ || mv $@.new $@
	@rm -f $@.new

$(OBJS): $(PINS_STAMP)

.PHONY: FORCE
FORCE:
