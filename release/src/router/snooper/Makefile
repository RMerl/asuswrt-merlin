#
# Ethernet Switch IGMP Snooper
# Copyright (C) 2014 ASUSTeK Inc.
# All Rights Reserved.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

#CROSS = mipsel-uclibc-
#CROSS = mipsel-linux-
#CROSS = mipsel-linux-uclibc-
#CROSS = mips-openwrt-linux-
#CROSS = arm-uclibc-
#CC = $(CROSS)gcc

CFLAGS = -Os -g -Wall -W -Wno-unused-parameter $(EXTRACFLAGS)
LDFLAGS = $(EXTRALDFLAGS)
LIBS += -Wl,--as-needed -lrt -Wl,--no-as-needed

CFLAGS += -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections

#CFLAGS += -DDEBUG
#CFLAGS += -DDEBUG_IGMP
#CFLAGS += -DDEBUG_IGMP_MORE
#CFLAGS += -DDEBUG_TIMER
#CFLAGS += -DDEBUG_CACHE
#CFLAGS += -DDEBUG_SWITCH

SWITCH ?= simul

all: snooper

snooper: snooper.o igmp.o timer.o cache.o utils.o $(SWITCH).o
	@echo " [snooper] LD $@"
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

test: $(SWITCH).c
	@echo " [snooper] LD $@"
	@$(CC) -DTEST $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	@rm -f *.o *~ snooper test

%.o: %.c *.h Makefile
	@echo " [snooper] CC $@"
	@$(CC) $(CFLAGS) -c -o $@ -fPIC $<
