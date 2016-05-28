CC = gcc
CFLAGS = -O3 -Wall
ifeq ($(TRX),NEW)
CFLAGS += -DTRX_NEW
endif

all: trx_asus

ifneq ($(wildcard ./prebuild/trx_asus),)
trx_asus:
	@-cp -f ./prebuild/trx_asus .
else
trx_asus: trx_asus.c
	$(CC) $(CFLAGS) -o $@ $^
endif

clean:
	@[ ! -f trx_asus.c ] || rm -f trx_asus
	rm -f *.o piggy target.squashfs vmlinuz-lzma

%.o: %.c
	$(CC) $(CFLAGS) -c $<

.PHONY: trx_asus
