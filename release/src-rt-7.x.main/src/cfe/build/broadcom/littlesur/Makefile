
CFG_MLONG64 ?= 1
CFG_LITTLE ?= 0
CFG_RELOC ?= 1
CFG_UNCACHED ?= 0
CFG_VAPI ?= 0
CFG_BOOTRAM ?= 0
CFG_BOARDNAME = "LITTLESUR"
CFG_PCI ?= 1

TOP = ../../../cfe
ARCH = mips
CPU = sb1250
BOARD = littlesur

include ${TOP}/main/cfe.mk

#
# Don't clean these object files if building bi-endian.
# To build a clean bi-endian flash image, use "gmake clean cfe-biendian.flash"
#

ifeq ($(strip ${CFG_BIENDIAN}),0)
CLEANOBJS += cfe.bin.le cfe.bin.be
endif

#
# Uncomment to make things happy under the functional simulator.
#
#CFLAGS += -D_FUNCSIM_ -D_FASTEMUL_

#
# For now, we do the BCM1400 magic at compile time, not at run-time
# So, define this symbol to tweak the MC code and other stuff
# to work properly.
#
CFLAGS += -D_SB1250_
#
# Main target for normal builds
#

ALL : cfe cfe.flash
	echo done

#
# Special targets for bi-endian builds
#

cfe.bin.le :
	${MAKE} clean all CFG_BIENDIAN=1 CFG_LITTLE=1 CFG_RELOC=1
	cp cfe.bin cfe.bin.le

cfe.bin.be :
	${MAKE} clean all CFG_BIENDIAN=1 CFG_LITTLE=0 CFG_RELOC=1
	cp cfe.bin cfe.bin.be

biend : cfe-biendian.flash cfe-biendian.srec
	echo done

cfe-biendian.flash : cfe.bin.le cfe.bin.be mkflashimage
	./mkflashimage -v -EX -B ${CFG_BOARDNAME} -V ${CFE_VER_MAJ}.${CFE_VER_MIN}.${CFE_VER_ECO} cfe.bin.be cfe.bin.le cfe-biendian.flash

cfe-biendian.srec : cfe-biendian.flash
	dd if=cfe-biendian.flash of=cfe-biendian.bin bs=64 skip=1
	$(OBJCOPY) --input-target=binary --output-target=srec cfe-biendian.bin cfe-biendian.srec


include ${TOP}/main/cfe_link.mk
