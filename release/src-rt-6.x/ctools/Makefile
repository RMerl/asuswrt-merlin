CC = gcc
CFLAGS = -O3 -Wall
ifeq ($(TRX),NEW)
CFLAGS += -DTRX_NEW
endif

ifneq ($(wildcard ./prebuild/trx_asus),)
all: trx_asus check_link

check_link:
	@-[ -f lzma_4k ] || ln -sf ../../src/ctools/lzma_4k lzma_4k
	@-[ -f lzma_9k ] || ln -sf ../../src/ctools/lzma_9k lzma_9k
	@-[ -f objcopy ] || ln -sf ../../src/ctools/objcopy objcopy
	@-[ -f trx ] || ln -sf ../../src/ctools/trx trx

trx_asus:
	@-cp -f ./prebuild/trx_asus .
else
all: trx_asus

trx_asus: trx_asus.c
	$(CC) $(CFLAGS) -o $@ $^
endif

clean:
	@[ ! -f trx_asus.c ] || rm -f trx_asus
	rm -f *.o piggy target.squashfs vmlinuz-lzma

%.o: %.c
	$(CC) $(CFLAGS) -c $<

.PHONY: trx_asus
