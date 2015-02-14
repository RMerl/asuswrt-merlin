
#
# CFE's version number
#

include ${TOP}/main/cfe_version.mk

#
# Default values for certain parameters
#

CFG_MLONG64 ?= 0
CFG_LITTLE  ?= 0
CFG_RELOC ?= 0
CFG_UNCACHED ?= 0
CFG_NEWRELOC ?= 0
CFG_BOOTRAM ?= 0
CFG_VGACONSOLE ?= 0
CFG_PCI ?= 1
CFG_LDT_REV_017 ?= 0
CFG_ZLIB ?= 0
CFG_LZMA ?= 0
CFG_BIENDIAN ?= 0
CFG_DOWNLOAD ?= 0
CFG_RAMAPP ?= 0
CFG_USB ?= 0
CFG_XIP ?= 0
CFG_SIM ?= 0
CFG_SIM_CONSOLE ?= 0
CFG_LDR_SREC ?= 0
CFG_LDR_ELF ?= 0

#
# Paths to other parts of the firmware.  Everything's relative to ${TOP}
# so that you can actually do a build anywhere you want.
#

ARCH_TOP   = ${TOP}/arch/${ARCH}
ARCH_CMN   = ${TOP}/arch/common
ARCH_SRC   = ${ARCH_TOP}/common/src
ARCH_INC   = ${ARCH_TOP}/common/include
CPU_SRC    = ${ARCH_TOP}/cpu/${CPU}/src
CPU_INC    = ${ARCH_TOP}/cpu/${CPU}/include

#
# It's actually optional to have a 'board'
# directory.  If you don't specify BOARD,
# don't include the files.
#

ifneq ("$(strip ${BOARD})","")
BOARD_SRC  = ${ARCH_TOP}/board/${BOARD}/src
BOARD_CMN  = ${ARCH_CMN}/board/${BOARD}/src
BOARD_INC  = ${ARCH_TOP}/board/${BOARD}/include
endif

#
# Preprocessor defines for CFE's version number
#

VDEF = -DCFE_VER_MAJ=${CFE_VER_MAJ} -DCFE_VER_MIN=${CFE_VER_MIN} -DCFE_VER_ECO=${CFE_VER_ECO}

#
# Construct the list of paths that will eventually become the include
# paths and VPATH
#

SRCDIRS = ${ARCH_SRC} ${CPU_SRC} ${BOARD_SRC} ${BOARD_CMN} ${TOP}/main ${TOP}/vendor ${TOP}/include ${TOP}/net ${TOP}/dev ${TOP}/pci ${TOP}/ui ${TOP}/lib ${TOP}/common ${TOP}/verif 

CFE_INC = ${TOP}/include ${TOP}/pci ${TOP}/net 

ifeq ($(strip ${CFG_VGACONSOLE}),1)
SRCDIRS += ${TOP}/x86emu ${TOP}/pccons
CFE_INC += ${TOP}/x86emu ${TOP}/pccons
endif

ifeq ($(strip ${CFG_VAPI}),1)
SRCDIRS += ${TOP}/verif
CFE_INC += ${TOP}/verif
endif

ifeq ($(strip ${CFG_ZLIB}),1)
SRCDIRS += ${TOP}/zlib
CFE_INC += ${TOP}/zlib
endif

ifeq ($(strip ${CFG_SIM}),1)
CFLAGS += -DCFG_SIM=1
endif

ifeq ($(strip ${CFG_SIM_CONSOLE}),1)
CFLAGS += -DCFG_SIM_CONSOLE=1
endif

ifneq (${CFG_HEAP_SIZE},)
CFLAGS += -DCFG_HEAP_SIZE=${CFG_HEAP_SIZE}
endif

ifeq ($(strip ${CFG_LZMA}),1)
SRCDIRS += ${SRCBASE}/tools/misc/lzma_src/C
CFE_INC += ${SRCBASE}/tools/misc/lzma_src/C
endif

ifeq ($(strip ${CFG_LDR_SREC}),1)
CFLAGS += -DCFG_LDR_SREC=1
endif

ifeq ($(strip ${CFG_LDR_ELF}),1)
CFLAGS += -DCFG_LDR_ELF=1
endif

INCDIRS = $(patsubst %,-I%,$(subst :, ,$(ARCH_INC) $(CPU_INC) $(BOARD_INC) $(CFE_INC)))

VPATH = $(SRCDIRS)

#
# Bi-endian support: If we're building the little-endian
# version, use a different linker script so we can locate the
# ROM at a higher address.  You'd think we could do this with
# normal linker command line switches, but there appears to be no
# command-line way to override the 'AT' qualifier in the linker script.
#

CFG_TEXTAT1MB=0
ifeq ($(strip ${CFG_BIENDIAN}),1)
  ifeq ($(strip ${CFG_LITTLE}),1)
    CFG_TEXT_START = 0x9fd00000
    CFG_ROM_START  = 0xbfd00000
    CFG_TEXTAT1MB=1
  endif
endif


#
# Configure tools and basic tools flags.  This include sets up
# macros for calling the C compiler, basic flags,
# and linker scripts.
#

include ${ARCH_SRC}/tools.mk

#
# Add some common flags that are used on any architecture.
#

CFLAGS += -I. $(INCDIRS)
CFLAGS += -D_CFE_ ${VDEF} -DCFG_BOARDNAME=\"${CFG_BOARDNAME}\"

#
# Gross - allow more options to be supplied from command line
#

ifdef CFG_OPTIONS
OPTFLAGS = $(patsubst %,-D%,$(subst :, ,$(CFG_OPTIONS)))
CFLAGS += ${OPTFLAGS}
endif


#
# This is the makefile's main target.  Note that we actually
# do most of the work in 'ALL' not 'all', since we include
# other makefiles after this point.
#

all : build_date.c makereg pcidevs_data2.h ALL

#
# Macros that expand to the list of arch-independent files
#

DEVOBJS = dev_flash.o dev_newflash.o  dev_null.o dev_promice.o \
	  dev_ide_common.o dev_ns16550.o dev_ds17887clock.o
LIBOBJS = lib_malloc.o lib_printf.o lib_queue.o lib_string.o lib_string2.o \
          lib_arena.o lib_misc.o lib_setjmp.o lib_qsort.o lib_hssubr.o lib_physio.o \
          lib_scanf.o
NETOBJS = net_ether.o net_arp.o net_ip.o net_udp.o net_api.o net_dns.o \
	  net_dhcp.o net_tftp.o net_icmp.o net_tcp.o net_tcpbuf.o dev_tcpconsole.o net_http.o
CFEOBJS = env_subr.o cfe_attach.o cfe_iocb_dispatch.o cfe_devfuncs.o \
          nvram_subr.o cfe_console.o cfe_main.o cfe_mem.o cfe_timer.o \
          cfe_background.o cfe_error.o build_date.o \
	  cfe_rawfs.o cfe_zlibfs.o cfe_xreq.o cfe_fatfs.o cfe_httpfs.o cfe_filesys.o cfe_boot.o \
          cfe_autoboot.o cfe_ldr_elf.o cfe_ldr_raw.o cfe_ldr_srec.o cfe_loader.o url.o \
	  cfe_savedata.o cfe_memfs.o
UIOBJS  = ui_command.o ui_cmddisp.o ui_envcmds.o ui_devcmds.o \
	  ui_netcmds.o ui_tcpcmds.o ui_memcmds.o ui_loadcmds.o ui_pcicmds.o \
	  ui_examcmds.o ui_flash.o ui_misccmds.o \
          ui_test_disk.o ui_test_ether.o ui_test_flash.o ui_test_uart.o

#
# Add more object files if we're supporting PCI
#

ifeq ($(strip ${CFG_PCI}),1)
PCIOBJS  = pciconf.o pci_subr.o pci_devs.o
PCIOBJS += ldtinit.o
DEVOBJS += dev_sp1011.o dev_ht7520.o
DEVOBJS += dev_ide_pci.o dev_ns16550_pci.o
DEVOBJS += dev_tulip.o dev_dp83815.o
CFLAGS  += -DCFG_PCI=1
ifeq ($(strip ${CFG_LDT_REV_017}),1)
CFLAGS  += -DCFG_LDT_REV_017=1
endif
ifeq ($(strip ${CFG_DOWNLOAD}),1)
DEVOBJS += dev_bcm1250.o download.data
CFLAGS += -DCFG_DOWNLOAD=1
endif
endif

ifeq ($(strip ${CFG_LZMA}),1)
ALLOBJS += cfe_lzmafs.o LzmaDec.o
CFLAGS += -DCFG_LZMA=1
endif

#
# If doing bi-endian, add the compiler switch to change
# the way the vectors are generated.  These switches are
# only added to the big-endian portion of the ROM,
# which is located at the real boot vector.
#

ifeq ($(strip ${CFG_BIENDIAN}),1)
  ifeq ($(strip ${CFG_LITTLE}),0)
    CFLAGS += -DCFG_BIENDIAN=1
  endif
endif

#
# Use the (slightly more) portable C flash engine
#

ifeq ($(strip ${CFG_CFLASH}),1)
CFLAGS += -DCFG_CFLASH=1
ALLOBJS += dev_flashop_engine.o
endif

#
# Include the makefiles for the architecture-common, cpu-specific,
# and board-specific directories.  Each of these will supply
# some files to "ALLOBJS".  The BOARD directory is optional
# as some ports are so simple they don't need boad-specific stuff.
#

include ${ARCH_SRC}/Makefile
include ${CPU_SRC}/Makefile

ifneq ("$(strip ${BOARD})","")
include ${BOARD_SRC}/Makefile
endif

#
# Add the common object files here.
#

ALLOBJS += $(LIBOBJS) $(DEVOBJS) $(CFEOBJS) $(VENOBJS) $(UIOBJS) $(NETOBJS) $(PCIOBJS)

#
# VAPI continues to be a special case.
#

ifeq ($(strip ${CFG_VAPI}),1)
include ${TOP}/verif/Makefile
endif

#
# USB support
#

ifeq ($(strip ${CFG_USB}),1)
SRCDIRS += ${TOP}/usb
CFE_INC += ${TOP}/usb
include ${TOP}/usb/Makefile
endif

#
# If we're doing the VGA console thing, pull in the x86 emulator
# and the pcconsole subsystem
#

ifeq ($(strip ${CFG_VGACONSOLE}),1)
include ${TOP}/x86emu/Makefile
include ${TOP}/pccons/Makefile
endif

#
# If we're including ZLIB, then add its makefile.
#

ifeq ($(strip ${CFG_ZLIB}),1)
include ${TOP}/zlib/Makefile
CFLAGS += -DCFG_ZLIB=1 -DMY_ZCALLOC -DNO_MEMCPY
endif

#
# Vendor extensions come next - they live in their own directory.
#

include ${TOP}/vendor/Makefile

.PHONY : all 
.PHONY : ALL
.PHONY : build_date.c

#
# Build the local tools that we use to construct other source files
#

mkpcidb : ${TOP}/hosttools/mkpcidb.c
	gcc -o mkpcidb ${TOP}/hosttools/mkpcidb.c

memconfig : ${TOP}/hosttools/memconfig.c
	gcc -o memconfig -D_MCSTANDALONE_ -D_MCSTANDALONE_NOISY_ -I${TOP}/arch/mips/cpu/sb1250/include ${TOP}/hosttools/memconfig.c ${TOP}/arch/${ARCH}/cpu/${CPU}/src/sb1250_draminit.c

pcidevs_data2.h : mkpcidb ${TOP}/pci/pcidevs_data.h
	./mkpcidb > pcidevs_data2.h

mkflashimage : ${TOP}/hosttools/mkflashimage.c
	gcc -o mkflashimage -I${TOP}/include ${TOP}/hosttools/mkflashimage.c

pci_subr.o : ${TOP}/pci/pci_subr.c pcidevs_data2.h

build_date.c :
	echo "const char *builddate = \"`date`\";" > build_date.c
	echo "const char *builduser = \"`whoami`@`hostname`\";" >> build_date.c

#
# Make a define for the board name
#

CFLAGS += -D_$(patsubst "%",%,${CFG_BOARDNAME})_

LIBCFE = libcfe.a

%.o : %.c
	$(GCC) $(CFLAGS) -o $@ $<

%.o : %.S
	$(GCC) $(ASFLAGS) $(CFLAGS) -o $@ $<

#
# This rule constructs "libcfe.a" which contains most of the object
# files.
#

$(LIBCFE) : $(ALLOBJS)
	rm -f $(LIBCFE)
	$(GAR) cr $(LIBCFE) $(ALLOBJS)
	$(RANLIB) $(LIBCFE)
