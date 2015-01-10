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
#CROSS = mipsel-linux-uclibc-
#CROSS = arm-uclibc-
#CC = $(CROSS)gcc

CFLAGS = -Os -g -Wall -W -Wno-unused-parameter $(EXTRACFLAGS)
LDFLAGS = $(EXTRALDFLAGS)

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
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

test: $(SWITCH).c
	$(CC) -DTEST $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o *~ snooper test

%.o: %.c *.h Makefile
	$(CC) $(CFLAGS) -c -o $@ -fPIC $<
