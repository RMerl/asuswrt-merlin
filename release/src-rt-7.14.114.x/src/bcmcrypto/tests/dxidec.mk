#
# Assorted cryptographic algorithms
#
# Copyright (C) 2012 Broadcom Corporation
#
# $Id: $
#

SRCBASE	:= ../..
VPATH=.:$(SRCBASE)/bcmcrypto:$(SRCBASE)/shared

CC	:= gcc
CFLAGS	+= -I. -I$(SRCBASE)/include -Wall
CFLAGS	+= -g -DDEBUG -ffunction-sections
CFLAGS	+= -Wall -Werror

PROGS	:= dxidec

all: $(PROGS)

clean:
	rm -f *.o *.obj $(PROGS) *.exe

CRYPTO_OBJS = wep.o rc4.o aes.o rijndael-alg-fst.o tkmic.o tkhash.o
UTILS_OBJS = bcmutils.o
OBJS = dxidec.o $(CRYPTO_OBJS) $(UTILS_OBJS)
dxidec:	$(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
