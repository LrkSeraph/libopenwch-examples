##
## This file is part of the libopenwch examples.
##
## Copyright (C) 2025 libopenwch contributors
##
## Derived from libopencm3-template's rules.mk (LGPL-3-or-later), reworked for
## RISC-V and for libopenwch's genlink-based linker script generation.
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

# This file expects the following to be defined before inclusion:
#
### REQUIRED ###
# PROJECT   - basename of the output files, e.g. blink
# DEVICE    - the part number, e.g. ch32v003f4p6.  The linker script, the
#             -march/-mabi flags and the library choice are all derived from
#             it through ld/devices.data.
#
### OPTIONAL ###
# CFILES    - C sources, basenames only (default: main.c)
# AFILES    - assembly sources, basenames only
# CXXFILES  - C++ sources, basenames only
# INCLUDES  - extra -I paths
# BUILD_DIR - defaults to bin
# OPT       - defaults to -Os
# CSTD      - defaults to -std=c99
# CXXSTD    - no default
# LDSCRIPT  - full path; overrides the generated one
# LDFLAGS   - extra linker flags
# CFLAGS    - extra compiler flags
# DEFS      - extra -D flags
# LDLIBS    - extra libraries
# LIBOPENWCH_NOSTDLIB
#           - set to 1 to link with -nostdlib and the bundled mini-libc
#             instead of newlib.  Useful with a toolchain that has no newlib.
# LIBOPENWCH_BLE
#           - set to 1 to link WCH's closed-source Bluetooth stack, for the
#             ble_* layer.  CH58x family only.

BUILD_DIR	?= bin
OPT		?= -Os
CSTD		?= -std=c99
CFILES		?= main.c

# Be silent per default, but 'make V=1' will show all compiler calls.
ifneq ($(V),1)
Q	:= @
endif

# Pull in the toolchain discovery (PREFIX, CC, LD, OBJCOPY, ...).
include $(TEMPLATE_DIR)/rules/toolchain.mk

##
## Linker script and architecture flags come from the device database.
##
## genlink-config.mk sets:
##	ARCH_FLAGS  -march=... -mabi=...
##	CPPFLAGS    -D<FAMILY> ... -I$(OPENWCH_DIR)/include
##	LDLIBS      -lopenwch_<family>
##	LDFLAGS     -L$(OPENWCH_DIR)/lib
##	LDSCRIPT    generated.$(DEVICE).ld
##
## Where libopenwch is.  OPENWCH_DIR set by the caller -- on the command line,
## in the environment, or in a project's own Makefile -- always wins; this only
## guesses when nobody said.
##
## Three layouts are understood, in this order:
##
##   * the submodule, ./libopenwch inside this repository, which is what a
##     --recurse-submodules clone gives you and therefore the normal case;
##   * a sibling checkout, ../libopenwch, for someone who would rather keep one
##     shared copy of the library than one per repository;
##   * this repository sitting inside a libopenwch checkout, where the parent
##     directory is libopenwch.
##
## Each candidate is confirmed by looking for mk/genlink-config.mk, so an
## unrelated directory that happens to be next door is never mistaken for the
## library.  If none works the build stops here, naming the paths it tried,
## rather than failing later with a puzzling "no such file or directory".
##
OPENWCH_DIR	?= $(firstword $(foreach d, \
		   $(abspath $(TEMPLATE_DIR)/libopenwch) \
		   $(abspath $(TEMPLATE_DIR)/../libopenwch) \
		   $(abspath $(TEMPLATE_DIR)/..), \
		   $(if $(wildcard $(d)/mk/genlink-config.mk),$(d))))

ifeq ($(strip $(OPENWCH_DIR)),)
$(error Cannot find libopenwch. Pass OPENWCH_DIR=/path/to/libopenwch, or run \
    `git submodule update --init` to fetch the submodule. Looked in \
    $(abspath $(TEMPLATE_DIR)/libopenwch), \
    $(abspath $(TEMPLATE_DIR)/../libopenwch) and $(abspath $(TEMPLATE_DIR)/..).)
endif

include $(OPENWCH_DIR)/mk/genlink-config.mk

## An included makefile's first target would otherwise become the default goal
## (toolchain.mk defines `monitor` before this file defines `all`).
.DEFAULT_GOAL := all

##
## Build flags.
##
INCLUDES	+= $(patsubst %,-I%, . $(TEMPLATE_DIR))

OBJS		= $(CFILES:%.c=$(BUILD_DIR)/%.o)
OBJS		+= $(AFILES:%.S=$(BUILD_DIR)/%.o)
OBJS		+= $(CXXFILES:%.cxx=$(BUILD_DIR)/%.o)

GENERATED_BINS	= $(PROJECT).elf $(PROJECT).bin $(PROJECT).hex \
		  $(PROJECT).map $(PROJECT).list

TGT_CPPFLAGS	+= -MD -Wall -Wundef $(INCLUDES) $(DEFS)

TGT_CFLAGS	+= $(OPT) $(CSTD) -g3
TGT_CFLAGS	+= $(ARCH_FLAGS)
TGT_CFLAGS	+= -fno-common -mno-relax
TGT_CFLAGS	+= -ffunction-sections -fdata-sections
TGT_CFLAGS	+= -Wextra -Wshadow -Wno-unused-variable
TGT_CFLAGS	+= -Wimplicit-function-declaration
TGT_CFLAGS	+= -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes

TGT_CXXFLAGS	+= $(OPT) $(CXXSTD) -g3
TGT_CXXFLAGS	+= $(ARCH_FLAGS)
TGT_CXXFLAGS	+= -fno-common -mno-relax
TGT_CXXFLAGS	+= -ffunction-sections -fdata-sections
TGT_CXXFLAGS	+= -Wextra -Wshadow -Wredundant-decls

## -nostartfiles is essential: libopenwch supplies its own reset entry
## (_start / _reset_entry) and vector table, so the toolchain's crt0 must not
## be linked in.  Without it, picolibc's crt0.o collides on _start and pulls in
## __data_size/__stack symbols this linker script does not define.
TGT_LDFLAGS	+= -nostartfiles
TGT_LDFLAGS	+= -T$(LDSCRIPT)
TGT_LDFLAGS	+= $(ARCH_FLAGS)
TGT_LDFLAGS	+= -mno-relax
TGT_LDFLAGS	+= -Wl,--gc-sections
TGT_LDFLAGS	+= -Wl,-Map=$(PROJECT).map

##
## Bluetooth LE.
##
## LIBOPENWCH_BLE=1 links WCH's closed-source Bluetooth stack, which the ble_*
## layer in libopenwch_ch5xx58x.a calls into.  It must be scanned after that
## archive (which introduces the references) and before the libc below (which
## supplies the memcpy the stack needs), so it goes here rather than alongside
## the other libraries.
##
## The archive stays under WCH's Apache-2.0 licence; see NOTICE.  It is only
## meaningful for the CH58x family, so only link it there.
##
ifeq ($(LIBOPENWCH_BLE),1)
ifneq ($(genlink_family),ch5xx58x)
$(error LIBOPENWCH_BLE=1 is only supported for the CH58x family; DEVICE=$(DEVICE) is $(genlink_family))
endif
LDLIBS		+= $(OPENWCH_DIR)/lib/ble/wch/LIBCH58xBLE.a
endif

##
## libc.
##
## By default the toolchain's newlib is used.  A toolchain built without newlib
## -- for instance Debian's gcc-riscv64-unknown-elf, which ships none for the
## rv32e or rv32imac multilibs -- can be used by setting LIBOPENWCH_NOSTDLIB=1,
## which links the freestanding mini-libc that libopenwch builds for exactly
## this case.  The archive is per family, so it carries the right ISA.
##
ifeq ($(LIBOPENWCH_NOSTDLIB),1)
TGT_LDFLAGS	+= -nostdlib
LDLIBS		+= $(OPENWCH_DIR)/lib/libopenwch_mini_libc_$(genlink_family).a -lgcc
else
TGT_LDFLAGS	+= -Wl,--start-group
LDLIBS		+= -lc -lgcc -lnosys
TGT_LDFLAGS	+= -Wl,--end-group
endif

# Never let make try to check out a source file from RCS/SCCS.
%: %,v
%: RCS/%,v
%: RCS/%
%: s.%
%: SCCS/s.%

.SUFFIXES:
.SUFFIXES: .c .S .h .o .cxx .elf .bin .hex .list

all: $(PROJECT).elf $(PROJECT).bin $(PROJECT).hex

## The linker script is produced from ld/linker.ld.S by genlink-rules.mk,
## which is included at the bottom of this file.  Naming it as a prerequisite
## here is what makes make build it before the final link.
LDSCRIPT_DEPS	:= $(LDSCRIPT)

$(BUILD_DIR)/%.o: %.c
	@printf "  CC      $<\n"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(TGT_CFLAGS) $(CFLAGS) $(TGT_CPPFLAGS) $(CPPFLAGS) -o $@ -c $<

$(BUILD_DIR)/%.o: %.S
	@printf "  AS      $<\n"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(TGT_CFLAGS) $(CFLAGS) $(TGT_CPPFLAGS) $(CPPFLAGS) -o $@ -c $<

$(BUILD_DIR)/%.o: %.cxx
	@printf "  CXX     $<\n"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CXX) $(TGT_CXXFLAGS) $(CXXFLAGS) $(TGT_CPPFLAGS) $(CPPFLAGS) -o $@ -c $<

$(PROJECT).elf: $(OBJS) $(LDSCRIPT_DEPS) $(LIBDEPS)
	@printf "  LD      $@\n"
	$(Q)$(LD) $(OBJS) $(TGT_LDFLAGS) $(LDFLAGS) $(LDLIBS) -o $@
	@$(SIZE) $@

%.bin: %.elf
	@printf "  OBJCOPY $@\n"
	$(Q)$(OBJCOPY) -Obinary $< $@

%.hex: %.elf
	@printf "  OBJCOPY $@\n"
	$(Q)$(OBJCOPY) -Oihex $< $@

%.list: %.elf
	@printf "  OBJDUMP $@\n"
	$(Q)$(OBJDUMP) -d -S $< > $@

size: $(PROJECT).elf
	@$(SIZE) $(PROJECT).elf

##
## Flashing.
##
## WRITE_SECTION defaults to the internal flash; a family with an external
## image (CH32V20x) can set it to the external region instead.  It only applies
## to minichlink -- wchlink takes the region from the part it was told about.
##
## FLASH_PREFIX/FLASH_SUFFIX come from toolchain.mk, which picks the programmer
## and knows how each one wants its arguments.
##
WRITE_SECTION	?= flash

flash: $(PROJECT).bin
	@printf "  FLASH   $< ($(PROGRAMMER))\n"
	$(Q)$(FLASH_PREFIX) $< $(FLASH_SUFFIX)

clean:
	$(Q)rm -rf $(BUILD_DIR) $(GENERATED_BINS)
	## Remove every device's generated script, not just the currently selected
	## one, so that switching DEVICE does not leave stale scripts behind.
	$(Q)rm -f generated.*.ld

.PHONY: all clean flash size

##
## Build the library on demand.  $(LIBDEPS) is the archive for the part
## selected by DEVICE; when it is missing -- a fresh clone, or a family that
## has not been built yet -- this builds libopenwch once, so that a single
## `make` in an example is enough to get started.  On every later build the
## archive is already there and nothing happens; a library that has changed is
## picked up with `make -C $(OPENWCH_DIR)`.
##
$(LIBDEPS):
	@printf "  MAKE    libopenwch\n"
	$(Q)$(MAKE) -C $(OPENWCH_DIR)

-include $(OBJS:.o=.d)

# The rule that produces $(LDSCRIPT) from ld/linker.ld.S.
include $(OPENWCH_DIR)/mk/genlink-rules.mk
