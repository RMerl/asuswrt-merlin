#
# Addresses of things unless overridden
#

ifeq ($(strip ${CFG_UNCACHED}),1)
  CFG_TEXT_START ?= 0xBFC00000
  CFG_DATA_START ?= 0xA1F00000
  CFG_ROM_START  ?= 0xBFC00000
else
  CFG_TEXT_START ?= 0x9FC00000
  CFG_DATA_START ?= 0x81F00000
  CFG_ROM_START  ?= 0xBFC00000
endif

#
# BOOTRAM mode (runs from ROM vector assuming ROM is writable)
# implies no relocation.
#

ifeq ($(strip ${CFG_BOOTRAM}),1)
  CFG_RELOC = 0
endif


#
# Basic compiler options and preprocessor flags
#

CFLAGS += -gdwarf-2 -c -ffreestanding 
CFLAGS += -Os -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes 

#
# Tools locations
#
TOOLPREFIX ?= sb1-elf-
GCC        ?= $(TOOLS)$(TOOLPREFIX)gcc
GCPP       ?= $(TOOLS)$(TOOLPREFIX)cpp
GLD        ?= $(TOOLS)$(TOOLPREFIX)ld
GAR        ?= $(TOOLS)$(TOOLPREFIX)ar
OBJDUMP    ?= $(TOOLS)$(TOOLPREFIX)objdump
OBJCOPY    ?= $(TOOLS)$(TOOLPREFIX)objcopy
RANLIB     ?= $(TOOLS)$(TOOLPREFIX)ranlib

#
# Check for 64-bit mode
#

ifeq ($(strip ${CFG_MLONG64}),1)
  CFLAGS += -mlong64 -D__long64
endif

#
# Determine parameters for the linker script, which is generated
# using the C preprocessor.
#
# Supported combinations:
#
#  CFG_RAMAPP   CFG_UNCACHED   CFG_RELOC   Description
#    Yes        YesOrNo        MustBeNo    CFE as a separate "application"
#    No         YesOrNo        Yes         CFE relocates to RAM as firmware
#    No         YesOrNo        No          CFE runs from flash as firmware
#

LDSCRIPT = ./cfe.lds
LDFLAGS += -g --script $(LDSCRIPT)
LDSCRIPT_TEMPLATE = ${ARCH_SRC}/cfe_ldscript.template

ifeq ($(strip ${CFG_UNCACHED}),1)
  GENLDFLAGS += -DCFG_RUNFROMKSEG0=0
else
  GENLDFLAGS += -DCFG_RUNFROMKSEG0=1
endif

ifeq ($(strip ${CFG_RAMAPP}),1)
   GENLDFLAGS += -DCFG_RAMAPP=1
   GENLDFLAGS += -DCFG_RUNFROMKSEG0=1
else 
 ifeq ($(strip ${CFG_RELOC}),0)
    ifeq ($(strip ${CFG_BOOTRAM}),1)
      GENLDFLAGS += -DCFG_BOOTRAM=1
    else
      GENLDFLAGS += -DCFG_BOOTRAM=0
    endif
  else
    CFLAGS += -membedded-pic -mlong-calls 
    GENLDFLAGS += -DCFG_EMBEDDED_PIC=1
    LDFLAGS +=  --embedded-relocs
  endif
endif

#
# Add GENLDFLAGS to CFLAGS (we need this stuff in the C code as well)
#

CFLAGS += ${GENLDFLAGS}

#
# Determine target endianness
#

ifeq ($(strip ${CFG_LITTLE}),1)
  ENDIAN = -EL
else
  ENDIAN = -EB
  CFLAGS += -EB
  LDFLAGS += -EB
endif

#
# Add the text/data/ROM addresses to the GENLDFLAGS so they
# will make it into the linker script.
#

GENLDFLAGS += -DCFE_ROM_START=${CFG_ROM_START} 
GENLDFLAGS += -DCFE_TEXT_START=${CFG_TEXT_START} 
GENLDFLAGS += -DCFE_DATA_START=${CFG_DATA_START} 
GENLDFLAGS += -DCFG_XIP=${CFG_XIP}
