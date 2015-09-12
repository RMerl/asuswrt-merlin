#
# Assorted cryptographic algorithms
#
# Copyright (C) 2012 Broadcom Corporation
#
# $Id $
#

SRCBASE	:= ../..
VPATH=.:$(SRCBASE)/bcmcrypto:$(SRCBASE)/shared

CC	:= gcc
CFLAGS	+= -I. -I$(SRCBASE)/include -Wall
CFLAGS	+= -g -DDEBUG -ffunction-sections
CFLAGS	+= -Wall -Werror

PROGS	:= gcm_test

all: $(PROGS)

clean:
	rm -f *.o *.obj $(PROGS) *.exe

CRYPTO_OBJS = aes.o rijndael-alg-fst.o sha256.o
UTILS_OBJS = bcmutils.o

OBJS = gcm_test.o gcm.o aesgcm.o $(CRYPTO_OBJS) $(UTILS_OBJS)

ifdef GCM_TABLE_SZ
ifneq ($(GCM_TABLE_SZ), 0)
CFLAGS += -DGCM_TABLE_SZ=$(GCM_TABLE_SZ)
OBJS += gcm_tables.o
endif
endif

$(PROGS):	$(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(OBJS): $(@:.o=.c)
