#GCC     = mips64-sb1sim-gcc 
#GLD     = mips64-sb1sim-ld
#AR      = mips64-sb1sim-ar
#OBJDUMP = mips64-sb1sim-objdump
#OBJCOPY = mips64-sb1sim-objcopy
#RANLIB  = mips64-sb1sim-ranlib


CFG_MLONG64 ?= 0
# Note: Big endian is not likely to work without some
# major source changes in the drivers.
CFG_LITTLE ?= 1
CFG_RELOC ?= 1
CFG_UNCACHED ?= 0
CFG_VAPI ?= 0 
CFG_BOOTRAM ?= 0
CFG_BOARDNAME = "BCM95380_RR"
CFG_PCI = 0

# Use M-Systems Boot SDK (Binary partiion reader)
# Note: You first need to boot Linux on your target and run "DFORMAT"
#       to create the binary partition (which does not interfere with 
#       your filesystem).
CFG_MSYS = 1

#
# Set the path HND_TOP, below, to the 'src'
# directory checked out from the HND's CVS repository.
#
# Below this directory should be the "shared" and "et/sys"
# directories.  
#
# The CFE build procedure will pull in sources required
# for the Ethernet driver from this location.
#

HND_TOP=../../../..

TOP = ../../../cfe
ARCH = mips
CPU = bcmcore
BOARD = bcm94710

include ${TOP}/main/cfe.mk

#LDLIBS += -lgcc

ALL : cfe cfe.flash docprep
	echo done

localclean:
	rm -f docprep

include ${TOP}/main/cfe_link.mk
