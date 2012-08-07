#
# Makefile for compressed self-booting CFE on Broadcom BCM947XX boards
#
# Copyright (C) 2011, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: Makefile 300761 2011-12-05 23:22:14Z $
#

CROSS_COMPILE	:= $(TOOLPREFIX)
CC		:= $(GCC)
LD		:= $(GLD)
LOADADDR	:= $(CFG_TEXT_START)

# The self-decompresor uses the minimal OSL, get rid of _CFE_ and vxworks
CFLAGS		:= $(subst -D_CFE_,,$(CFLAGS)) -fdata-sections
CFLAGS		+= -D_MINOSL_ -DBCMBUSTYPE=SI_BUS -Uvxworks -U__vxworks

ifeq ($(strip $(CFG_MINIMAL_SIZE)), 1)
ifeq ($(strip $(CFG_GMAC)), 0)
CFLAGS		+= -DBCMCHIPTYPE=0
else
CFLAGS		+= -DBCMCHIPTYPE=1
endif
endif

# Link at 3 MB offset in RAM
TEXT_START	:= 0x80001000

OBJCOPY		:= $(CROSS_COMPILE)objcopy -O binary -R .reginfo -R .note -R .comment -R .mdebug -S
OBJCOPYSREC	:= $(CROSS_COMPILE)objcopy -O srec -R .reginfo -R .note -R .comment -R .mdebug -S

SYSTEM		:= ../cfe

vpath %.c $(SRCBASE)/shared
vpath %.S $(SRCBASE)/shared
vpath %.lds.in $(SRCBASE)/shared $(SRCBASE)/tools/misc/lzma_src/C

CFLAGS		+= -DLOADADDR=$(LOADADDR)
CFLAGS		+= -ffunction-sections $(call check_gcc, -fvtable-gc, )

OBJECTS		:= boot.o sisdram.o siutils.o load.o \
		 hndmips.o hndchipc.o hndpmu.o \
		 min_osl.o bcmutils.o bcmstdlib.o sromstubs.o nvramstubs.o

ifeq ($(strip $(CFG_MINIMAL_SIZE)), 1)
ifeq ($(strip $(CFG_GMAC)), 0)
OBJECTS		+= sbsdram.o sbutils.o
else
OBJECTS		+= aisdram.o aiutils.o
endif
else
OBJECTS		+= aisdram.o aiutils.o sbsdram.o sbutils.o
endif

ifeq ($(strip $(CFG_NFLASH)), 1)
OBJECTS		+= nflash.o
endif

# Default to lzma_4k
COMPRESS	:= lzma_4k e

ifneq ($(findstring lzma_4k,$(COMPRESS)),)
CFLAGS		+= -DUSE_LZMA -DBCMLZMA
else
ifneq ($(findstring gzip,$(COMPRESS)),)
CFLAGS		+= -DUSE_GZIP
else
ifneq ($(findstring bzip2,$(COMPRESS)),)
CFLAGS		+= -DUSE_BZIP2
else
COMPRESS	:= cat
endif
endif
endif

all: cfez.bin
	../../../../../../tools/nvserial -i cfez.bin -o cfe_rt-ac66u.bin -s 00 rt-ac66u_nvram.txt
	mv cfe_rt-ac66u.bin ../../../..

# Don't build dependencies, this may die if $(CC) isn't gcc
dep:

cfez.bin: cfez
	$(OBJCOPY) $< $@
	$(OBJCOPYSREC) $< cfez.srec

	@if [ "$(CFEZ_MAXSIZE)" != "" ]; then \
		if [ "`wc -c < cfez.bin`" -gt "$(CFEZ_MAXSIZE)" ]; then \
			echo "*** ERROR *** : cfez.bin bootrom image size: `wc -c < cfez.bin` exceeds limit: $(CFEZ_MAXSIZE)"; \
		fi; \
	fi; \

# Link the loader and the kernel binary together
cfez: cfez.lds $(OBJECTS) piggy.o
	$(LD) -static -Map cfez.map --gc-sections -no-warn-mismatch -T cfez.lds -o $@ $(OBJECTS) piggy.o

cfez.lds: hndrte.lds.in Makefile
	sed -e s/TEXT_START/$(TEXT_START)/ \
	    -e s/TARGET_ARCH/mips/ < $< > $@

# Create a linkable version of the (possibly compressed) kernel binary
piggy.o: piggz piggy.lds

ifeq ($(CFG_LITTLE), 1)
	$(LD) -no-warn-mismatch -T piggy.lds -r -o $@ -b binary piggz -b elf32-tradlittlemips 
else
	$(LD) -no-warn-mismatch -T piggy.lds -r -o $@ -b binary piggz -b elf32-bigmips
endif

piggy.lds:
	@echo "SECTIONS { .data : { input_len = .; LONG(input_data_end - input_data) input_data = .; *(.data) input_data_end = .; }}" > $@

piggz: piggy
ifneq ($(findstring lzma_4k,$(COMPRESS)),)
	$(COMPRESS) $< $@
else
	$(COMPRESS) $< > $@
endif

piggy: $(SYSTEM)
	$(OBJCOPY) $< $@

mrproper: clean

clean:
	rm -f cfez cfez.bin cfez.srec piggz piggy *.lds *.o *.map

%.o : %.S
	$(GCC) $(CFLAGS) -o $@ $<
