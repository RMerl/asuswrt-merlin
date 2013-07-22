#
# Makefile for Broadcom BCM947XX boards
#
# Copyright (C) 2011, Broadcom Corporation. All Rights Reserved.
# 
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# $Id: Makefile 341899 2012-06-29 04:06:38Z $
#

# Default mips
ifeq ($(ARCH),)
ARCH := mips
endif

ifeq ($(ARCH), arm)
  TEXT_START      ?= 0x00700000
  LOADADDR        := 0x00001000
  CROSS_COMPILE   ?= arm-uclibc-linux-2.6.36-
  VMLINUZ_LDS_IN  := cfez_arm.lds.in
  ELF32_LITTLE    := elf32-littlearm
  ELF32_BIG       := elf32-bigarm
  LDLIBS          := -L $(shell dirname `$(CROSS_COMPILE)gcc $(CFLAGS) -print-libgcc-file-name`) -lgcc
else
  # Link at 3 MB offset in RAM
  TEXT_START      ?= 0x80300000
  LOADADDR        := 0x80001000
  CROSS_COMPILE   ?= mipsel-linux-
  VMLINUZ_LDS_IN  := hndrte.lds.in
  ELF32_LITTLE    := elf32-tradlittlemips
  ELF32_BIG       ?= elf32-bigmips
  LDLIBS          :=
endif

OBJCOPY-arm     := $(CROSS_COMPILE)objcopy -O binary -R .note -R .note.gnu.build-id -R .comment -S
OBJCOPY-mips    := $(CROSS_COMPILE)objcopy -O binary -R .reginfo -R .note -R .comment -R .mdebug -S
OBJCOPY		:= $(OBJCOPY-$(ARCH))

OBJCOPYSREC	:= $(CROSS_COMPILE)objcopy -O srec -R .reginfo -R .note -R .comment -R .mdebug -S

vpath %.c $(SRCBASE)/shared
vpath %.S $(SRCBASE)/shared
vpath %.lds.in $(SRCBASE)/shared

# The self-decompresor is standalone, get rid of the linux flags
CFLAGS		:= $(subst -Dlinux,,$(CFLAGS))
CFLAGS		+= -Ulinux -D_MINOSL_ -ffreestanding
ifdef CONFIG_MCOUNT
CFLAGS		:= $(subst -pg,,$(CFLAGS))
endif
CFLAGS		+= -ffunction-sections $(call check_gcc, -fvtable-gc, )

SYSTEM		?= $(LINUXDIR)/vmlinux

# Note: siutils.o appeared seperately on purpose to solve post reference problem of GCC
OBJS-mips = boot.o sisdram.o sbsdram.o aisdram.o sflash.o hndmips.o sbutils.o aiutils.o siutils.o nicpci.o
# We should not compile sbutils.o but need add more compile directive in siutils.c
OBJS-arm = bootarm.o  aisdram_ca9.o siutils.o hndarm_ca9.o pcieinit_ca9.o sbutils.o aiutils.o nicpci.o
OBJECTS := $(OBJS-$(ARCH))
OBJECTS += load.o hndchipc.o hndpmu.o min_osl.o bcmutils.o \
	   bcmstdlib.o sromstubs.o nvramstubs.o

CFLAGS-mips	= -DLOADADDR=$(LOADADDR) -march=mips32 -Wa,-mips32 -Wa,--trap -G 0 -mno-abicalls -fno-pic -pipe -msoft-float
ASFLAGS-mips	= -D__ASSEMBLY__ -DLOADADDR=$(LOADADDR) -march=mips32 -Wa,-mips32 -Wa,--trap -G 0 -mno-abicalls -fno-pic -pipe
CFLAGS-arm	= -DLOADADDR=$(LOADADDR) -Ulinux -pipe -g -DSTANDALONE -O0 -D__KERNEL__ -msoft-float -marm -mabi=aapcs-linux \
		  -mno-thumb-interwork -fno-common -ffixed-r8 -msoft-float -march=armv6k -D__ARM_ARCH_7A__ -DBCMCHIPTYPE=3 \
		  -DBCMPMUCTL=0 -DCFG_CRU=1
ASFLAGS-arm	= -D_LANGUAGE_ASSEMBLY -DLOADADDR=$(LOADADDR) -Ulinux -pipe -g -DSTANDALONE -O0 -D__KERNEL__ -msoft-float \
		  -marm -mabi=aapcs-linux -mno-thumb-interwork -fno-common -ffixed-r8 -msoft-float -march=armv6k -D__ARM_ARCH_7A__ \
		  -DBCMCHIPTYPE=3 -DBCMPMUCTL=0 -DCFG_CRU=1
CFLAGS		+= $(CFLAGS-$(ARCH))
ASFLAGS		+= $(ASFLAGS-$(ARCH))

# Default to lzma_4k
COMPRESS	?= $(SRCBASE)/ctools/lzma_4k e
CFLAGS		+= -DUSE_LZMA -D_LZMA_NO_SYSTEM_SIZE_T -DBCMLZMA -I$(SRCBASE)/tools/misc/lzma_src/C

CFLAGS		+= -I$(SRCBASE)/include -DBCMDRIVER
ASFLAGS		+= -I$(SRCBASE)/include

all: zImage vmlinuz vmlinuz-gzip

# Don't build dependencies, this may die if $(CC) isn't gcc
dep:

zImage: vmlinux
	$(OBJCOPY) $< $@

# Link the loader and the kernel binary together
vmlinux: vmlinux.lds $(OBJECTS) piggy.o
	$(LD) -static --gc-sections -no-warn-mismatch -T vmlinux.lds -o $@ $(OBJECTS) $(LDLIBS) piggy.o

vmlinux.lds: $(VMLINUZ_LDS_IN) Makefile
	sed -e s/TEXT_START/$(TEXT_START)/ \
	     -e s/TARGET_ARCH/mips/ < $< > $@

# Create a linkable version of the (possibly compressed) kernel binary
piggy.o: piggz piggy.lds
	$(LD) -no-warn-mismatch -T piggy.lds -r -o $@ -b binary piggz -b $(ELF32_LITTLE)

piggy.lds:
	@echo "SECTIONS { .data : { input_len = .; LONG(input_data_end - input_data) input_data = .; *(.data) input_data_end = .; }}" > $@

# Create an lzma version named vmlinuz
vmlinuz: piggy
	$(COMPRESS) $< $@

# Create a gzip version named vmlinuz-gzip
vmlinuz-gzip: piggy
	gzip -c9 $< > $@

piggz: piggy
	$(COMPRESS) $< $@

piggy: $(SYSTEM)
	$(OBJCOPY) $< $@
	
mrproper: clean

clean:
	rm -f vmlinux vmlinuz zImage piggz piggy *.lds *.o
