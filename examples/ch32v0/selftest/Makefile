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
## CH32V003 self-test group.
##
## Keeps the individual self-tests in their own folders while presenting the
## group as one entry to the examples build and CI loops.
##

TARGETS = dma_selftest timer_selftest

all:
	@for t in $(TARGETS); do \
	$(MAKE) -C $$t || exit $$?; \
	done

clean:
	@for t in $(TARGETS); do \
	$(MAKE) -C $$t clean || exit $$?; \
	done

.PHONY: all clean
