# $Id: Makefile,v 1.3 2011/03/02 16:04:23 nanard Exp $
CFLAGS?=-Wall -g -DDEBUG
CC = gcc

#LIBS = -liptc
LIBS = -lip4tc

ARCH := $(shell uname -m | grep -q "x86_64" && echo 64)
ifdef IPTABLESPATH
CFLAGS := $(CFLAGS) -I$(IPTABLESPATH)/include/
LDFLAGS := $(LDFLAFGS) -L$(IPTABLESPATH)/libiptc/
# get iptables version and set IPTABLES_143 macro if needed
IPTABLESVERSION := $(shell grep "\#define VERSION" $(IPTABLESPATH)/config.h | tr -d \" |cut -d" " -f3 )
IPTABLESVERSION1 := $(shell echo $(IPTABLESVERSION) | cut -d. -f1 )
IPTABLESVERSION2 := $(shell echo $(IPTABLESVERSION) | cut -d. -f2 )
IPTABLESVERSION3 := $(shell echo $(IPTABLESVERSION) | cut -d. -f3 )
# test if iptables version >= 1.4.3
TEST := $(shell [ \( \( $(IPTABLESVERSION1) -ge 1 \) -a \( $(IPTABLESVERSION2) -ge 4 \) \) -a \( $(IPTABLESVERSION3) -ge 3 \) ] && echo 1 )
ifeq ($(TEST), 1)
CFLAGS := $(CFLAGS) -DIPTABLES_143
# the following sucks, but works
LIBS = $(IPTABLESPATH)/libiptc/.libs/libip4tc.o
#LIBS = $(IPTABLESPATH)/libiptc/.libs/libiptc.a
else
LIBS = $(IPTABLESPATH)/libiptc/libiptc.a
endif
else
# check for system-wide iptables files. Test if iptables version >= 1.4.3
TEST := $(shell test -f /usr/include/iptables/internal.h && grep -q "\#define IPTABLES_VERSION" /usr/include/iptables/internal.h && echo 1)
ifeq ($(TEST), 1)
CFLAGS := $(CFLAGS) -DIPTABLES_143
LIBS = -liptc
TEST_LIB := $(shell test -f /usr/lib$(ARCH)/libiptc.a && echo 1)
ifeq ($(TEST_LIB), 1)
LIBS = -liptc /usr/lib$(ARCH)/libiptc.a
endif
endif 
endif

all:	iptcrdr.o testiptcrdr

clean:
	$(RM) *.o testiptcrdr

testiptcrdr:	testiptcrdr.o iptcrdr.o upnpglobalvars.o $(LIBS)

iptcrdr.o:	iptcrdr.c iptcrdr.h

upnpglobalvars.o:	../upnpglobalvars.c ../upnpglobalvars.h
	$(CC) -c -o $@ $<


