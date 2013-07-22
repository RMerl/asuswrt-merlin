
#GCC     = mips64-sb1sim-gcc -DDBG -DBCMDBG
GCC     = mips64-sb1sim-gcc 
GLD     = mips64-sb1sim-ld
AR      = mips64-sb1sim-ar
OBJDUMP = mips64-sb1sim-objdump
OBJCOPY = mips64-sb1sim-objcopy
RANLIB  = mips64-sb1sim-ranlib

CFG_MLONG64 ?= 0

# Uncomment to boot from PCI/JTAG
#CFG_EPIBOOT=1

# Note: Big endian is not likely to work without some
# major source changes in the drivers.
# Note: this configuration is for loading into FLASH via
#       Epidiag

# Link & Run uncached (Quickturn ala Epidiag/JTAG)
ifeq ($(CFG_EPIBOOT),1)
CFG_LITTLE ?= 1
CFG_RELOC ?= 0
CFG_UNCACHED ?= 1
CFG_VAPI ?= 0 
CFG_BOOTRAM ?= 1
CFG_UNCACHED_RAMAPP ?= 1
CFG_BOARDNAME = "BU4704"
# For JTAG Boot, uncomment this line
CFLAGS += -DJTAG_RAM_BOOT
#CFG_MINIMAL_SIZE=1
CFG_MINIMAL_SIZE=0

else

# Note: this configuration is for loading into FLASH via
#       Epidiag via "s flashutl.tcl" and "flwritefile cfe.bin"
# Link and Run cached (Note: will not work from Epidiag)

CFG_LITTLE ?= 1
CFG_RELOC ?= 1
CFG_UNCACHED ?= 0
CFG_VAPI ?= 0 
CFG_BOOTRAM ?= 0
CFG_BOARDNAME = "BU4704"
CFG_PCI = 0
CFG_MSYS = 0
CFG_MINIMAL_SIZE=0
endif

# Define the CPU family
CFLAGS += -DMIPS33xx 
CFLAGS += -DBCM4704

# Define for building 4704
CFG_4704 = 1

# For DDR SDRAM
CFLAGS += -DDDRAM

# Disable Flow control on UART
CFLAGS += -DNS16550_NO_FLOW

CFG_PCI = 0
CFG_ZLIB=1
CFG_MSYS= 0


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
BOARD = bcm4x5x

include ${TOP}/main/cfe.mk

#LDLIBS += -lgcc

ALL : cfe cfe.flash
	echo done

include ${TOP}/main/cfe_link.mk
