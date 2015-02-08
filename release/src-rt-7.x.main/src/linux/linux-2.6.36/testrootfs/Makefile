
TOOLDIR=/tools/linux/local/hndtools-armeabi-linux-2009q3
PREFIX=arm-none-linux-gnueabi-

# TOOLDIR=/tools/linux/local/hndtools-arm-eabi-4.4.0
# PREFIX=arm-eabi-

export PATH:=$(TOOLDIR)/bin:$(PATH)
CC=$(PREFIX)gcc
LD=$(PREFIX)ld
# CFLAGS=-I/usr/include
CFLAGS+=-mcpu=cortex-a9

all:	init

init:	init.c
	$(CC) -o $@ $< --static

# init:	init.o
# 	$(LD) $(LDFLAGS) -o $@ $< --static -lc -lgcc

clean:
	rm -f init init.o
