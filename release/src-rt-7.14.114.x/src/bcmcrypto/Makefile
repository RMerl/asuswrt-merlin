#
# Assorted cryptographic algorithms
#
# Copyright (C) 2002 Broadcom Corporation
#
# $Id: Makefile 241182 2011-02-17 21:50:03Z $
#

SRCBASE	:= ..

CC	:= gcc
CFLAGS	+= -I. -I$(SRCBASE)/include -Wall
CFLAGS	+= -g -DDEBUG -ffunction-sections
#CFLAGS	+= -O2
CFLAGS	+= -Wall -Werror

#CC	:= cl
#CFLAGS	+= -I. -I../include -Ic:/tools/msdev/2600ddk/inc/crt
#LDFLAGS	+= -link -LIBPATH:c:/tools/msdev/2600ddk/lib/wxp/i386

PROGS	:= aes aeskeywrap des dh passhash prf rc4 sha1 tkhash tkhash_shared_sbox sha256



all: $(PROGS)

clean:
	rm -f *.o *.obj $(PROGS) *.exe

AESFLAGS := -DBCMAES_TEST -DBCMAES_TEST_STANDALONE -DBCMAES_GENTABLE
aes:	aes.c rijndael-alg-fst.c
	$(CC) $(CFLAGS) $(AESFLAGS) -o $@ $^ $(LDFLAGS)

aeskeywrap:	aeskeywrap.c aes.c rijndael-alg-fst.c
	$(CC) $(CFLAGS) -DBCMAESKEYWRAP_TEST -o $@ $^ $(LDFLAGS)


des:	des.c
	$(CC) $(CFLAGS) -DDES_TEST_STANDALONE -o $@ $^ $(LDFLAGS)

passhash:	passhash.c sha1.c prf.c
	$(CC) $(CFLAGS) -DBCMPASSHASH_TEST -o $@ $^ $(LDFLAGS)

prf:	prf.c hmac.c sha1.c md5.c
	$(CC) $(CFLAGS) -DBCMPRF_TEST -DBCMCCX -o $@ $^ $(LDFLAGS)

rc4:	rc4.c
	$(CC) $(CFLAGS) -DBCMRC4_TEST -o $@ $^ $(LDFLAGS)

sha1:	sha1.c
	$(CC) $(CFLAGS) -DBCMSHA1_TEST -o $@ $^ $(LDFLAGS)

sha256:	sha256.c
	$(CC) $(CFLAGS) -DBCMSHA256_TEST -o $@ $^ $(LDFLAGS)


tkhash:	tkhash.c
	$(CC) $(CFLAGS) -DBCMTKHASH_TEST -o $@ $^ $(LDFLAGS)

tkhash_shared_sbox:	tkhash.c rijndael-alg-fst.c
	$(CC) $(CFLAGS) -DBCMTKHASH_TEST -DSHARE_RIJNDAEL_SBOX -o $@ $^ $(LDFLAGS)

dh:	dh.c bn.c random.c
	$(CC) $(CFLAGS) -m32 -DBCMDH_TEST -o $@ $^ $(LDFLAGS)

install:
	install -d $(INSTALLDIR)/usr/sbin
	install $(PROGS) $(INSTALLDIR)/usr/sbin
	$(STRIP) $(foreach file,$(PROGS),$(INSTALLDIR)/usr/sbin/$(file))

test_all: $(PROGS)
	@for i in $(PROGS); do \
		echo ""; \
		echo "==> Testing $$i"; \
		$$i || exit 127; \
	done
