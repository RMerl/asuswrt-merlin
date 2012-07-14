
CFG_MLONG64  ?= 0
CFG_LITTLE   ?= 0        # Mindbender
CFG_RELOC    ?= 1
CFG_UNCACHED ?= 0      # Mindbender
CFG_NEWRELOC ?= 0
CFG_VAPI     ?= 0 
CFG_BOOTRAM  ?= 0
CFG_RAMAPP   ?= 0       # EPI
CFG_BOARDNAME = "BCM9635x"
CFG_PCI       = 0
CFG_ZLIB	  = 1		# enable zlib
#CFE_DEV_FLASH = 1		# enable flash  
CFE_VER_BCM963XX ?= 0

# Mindbender
CFG_IDE ?= 0
CFG_FAT ?= 0

TOP   = ../../../cfe
ARCH  = mips
CPU   = bcmcore
BOARD = bcm9635x

HOSTTOOLS = ../../../../hostTools

DEFINES += -DBROADCOM -DMIPS2_EXT_HACK -DCONFIG_CPU_MIPS32 -DDUMP_TRACE
CFLAGS += -DCFE_VER_RELEASE=${CFE_VER_BCM963XX}

include ${TOP}/main/cfe.mk

include ${ARCH_TOP}/board/bcm963xx/src/Makefile

SRCDIRS += ${ARCH_TOP}/board/bcm963xx/src
VPATH += ${ARCH_TOP}/board/bcm963xx/src
INCDIRS += -I${ARCH_TOP}/board/bcm963xx/include

ALL : cfe cfe.flash
	( if [ -d ${HOSTTOOLS} ] ; then \
	    ${MAKE} -C ${HOSTTOOLS} INCLUDEDIR=-I../cfe/cfe/arch/mips/board/bcm963xx/include ; \
	    ${HOSTTOOLS}/bcmImageBuilder -o bcm9635x_cfe -b 635x --cfefile cfe.bin ; \
	fi )
	echo done

mkramapp: cfe
	echo done

ramapp:
	${MAKE} clean
	${MAKE} CFG_RELOC?=0 CFG_RAMAPP?=1 mkramapp

include ${TOP}/main/cfe_link.mk
