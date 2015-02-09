
#GCC     = mips64-sb1sim-gcc -DDBG -DBCMDBG
GCC     = mips64-sb1sim-gcc 
GLD     = mips64-sb1sim-ld
AR      = mips64-sb1sim-ar
OBJDUMP = mips64-sb1sim-objdump
OBJCOPY = mips64-sb1sim-objcopy
RANLIB  = mips64-sb1sim-ranlib

CFG_MLONG64 ?= 0

# Note: Big endian is not likely to work without some
# major source changes in the drivers.
# Note: this configuration is for loading into FLASH via
#       Epidiag via "s flashutl.tcl" and "flwritefile cfe.bin"
# Link and Run cached (Note: will not work from Epidiag)
CFG_LITTLE ?= 1
CFG_RELOC ?= 1
CFG_UNCACHED ?= 0
CFG_VAPI ?= 0 
CFG_BOOTRAM ?= 0
CFG_BOARDNAME = "BCM5365_FPGA"
CFG_PCI = 0
CFG_MSYS = 0
CFG_MINIMAL_SIZE=0

# This configuration will allow you to use Epidiag with "s mipsutls.tcl"
# and "loadcfe" which loads the cfe_boot.S stub and loads cfe ELF file.
# Link & Run uncached (FPGA, 5365FPGA ala Epidiag/JTAG)
# For JTAG Boot, uncomment these lines
#CFG_LITTLE ?= 1
#CFG_RELOC ?= 0
#CFG_UNCACHED ?= 1
#CFG_VAPI ?= 0 
#CFG_BOOTRAM ?= 1
#CFG_UNCACHED_RAMAPP ?= 1
#CFG_BOARDNAME = "BCM5365_FPGA"
#CFLAGS += -DJTAG_RAM_BOOT

# Define the CPU family
CFLAGS += -DMIPS33xx

# Define the chip (-DBCM4704, -DBCM5365)
CFLAGS += -DBCM5365

# Disable Flow control on UART
CFLAGS += -DNS16550_NO_FLOW

# SDRAM Type (comment out for DDR default)
CFLAGS += -DSDRAM_MODE

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
