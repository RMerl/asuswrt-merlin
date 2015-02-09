ifeq ($(SRCBASE),)
	# ..../src/router/
	# (directory of the last (this) makefile)
	export TOP := $(shell cd $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))) && pwd)

	# ..../src/
	export SRCBASE := $(shell (cd $(TOP)/.. && pwd))
	export SRCBASEDIR := $(shell (cd $(TOP)/.. && pwd | sed 's/.*release\///g'))
else
	export TOP := $(SRCBASE)/router
endif

-include $(TOP)/.config
include $(SRCBASE)/profile.mak
include $(SRCBASE)/target.mak
include $(SRCBASE)/platform.mak

ifeq ($(or $(PLATFORM),$(CROSS_COMPILE),$(CONFIGURE),$(ARCH),$(HOST)),)
$(error Define Platform-specific definitions in platform.mak)
endif

export BUILD := $(shell (gcc -dumpmachine))
export HOSTCC := gcc

export PLT := $(ARCH)
export TOOLCHAIN := $(shell cd $(dir $(shell which $(CROSS_COMPILE)gcc))/.. && pwd -P)

export CC := $(CROSS_COMPILE)gcc
export GCC := $(CROSS_COMPILE)gcc
export CXX := $(CROSS_COMPILE)g++
export AR := $(CROSS_COMPILE)ar
export AS := $(CROSS_COMPILE)as
export LD := $(CROSS_COMPILE)ld
export NM := $(CROSS_COMPILE)nm
export OBJCOPY := $(CROSS_COMPILE)objcopy
export OBJDUMP := $(CROSS_COMPILE)objdump
export RANLIB := $(CROSS_COMPILE)ranlib
export READELF ?= $(CROSS_COMPILE)readelf
export STRIPX := $(CROSS_COMPILE)strip -x
ifeq ($(RTCONFIG_BCMARM),y)
export STRIP := $(CROSS_COMPILE)strip
else
export STRIP := $(CROSS_COMPILE)strip -R .note -R .comment
endif
export SIZE := $(CROSS_COMPILE)size

# Use a pkg-config wrapper to avoid pulling in host libs during cross-compilation.
export PKG_CONFIG := $(SRCBASE)/../../tools/asuswrt-pkg-config

# Determine kernel version
SCMD=sed -e 's,[^=]*=[        ]*\([^  ]*\).*,\1,'
KVERSION:=	$(shell grep '^VERSION[ 	]*=' $(LINUXDIR)/Makefile|$(SCMD))
KPATCHLEVEL:=	$(shell grep '^PATCHLEVEL[ 	]*=' $(LINUXDIR)/Makefile|$(SCMD))
KSUBLEVEL:=	$(shell grep '^SUBLEVEL[ 	]*=' $(LINUXDIR)/Makefile|$(SCMD))
KEXTRAVERSION:=	$(shell grep '^EXTRAVERSION[ 	]*=' $(LINUXDIR)/Makefile|$(SCMD))
LINUX_KERNEL=$(KVERSION).$(KPATCHLEVEL).$(KSUBLEVEL)$(KEXTRAVERSION)
LINUX_KERNEL_VERSION=$(shell expr $(KVERSION) \* 65536 + $(KPATCHLEVEL) \* 256 + $(KSUBLEVEL))
ifeq ($(LINUX_KERNEL),)
$(error Empty LINUX_KERNEL variable)
endif
export LINUX_KERNEL


include $(SRCBASE)/target.mak

export LIBDIR := $(TOOLCHAIN)/lib
export USRLIBDIR := $(TOOLCHAIN)/usr/lib

export PLATFORMDIR := $(TOP)/$(PLATFORM)
export INSTALLDIR := $(PLATFORMDIR)/install
export TARGETDIR := $(PLATFORMDIR)/target
export STAGEDIR := $(PLATFORMDIR)/stage
export PKG_CONFIG_SYSROOT_DIR := $(STAGEDIR)
export PKG_CONFIG_PATH := $(STAGEDIR)/usr/lib/pkgconfig:$(STAGEDIR)/etc/lib/pkgconfig

export EXTRACFLAGS += -DLINUX_KERNEL_VERSION=$(LINUX_KERNEL_VERSION) $(if $(STAGING_DIR),--sysroot=$(STAGING_DIR))

CPTMP = @[ -d $(TOP)/dbgshare ] && cp $@ $(TOP)/dbgshare/ || true


ifeq ($(CONFIG_RALINK),y)
# Move to platform.mak
else ifeq ($(CONFIG_QCA),y)
# Move to platform.mak
else # CONFIG_RALINK != y && CONFIG_QCA != y
# Broadcom SoC
ifeq ($(CONFIG_LINUX26),y)
ifeq ($(RTCONFIG_BCMWL6),y)
ifneq ($(RTCONFIG_BCMARM),y)
# e.g. RT-AC66U
export KERNELCC := $(SRCBASE)/../../tools/brcm/K26/hndtools-mipsel-linux-uclibc-4.2.3/bin/mipsel-linux-uclibc-gcc
else # RTCONFIG_BCMARM = y
export KERNELCC := $(CC)
endif
else # RTCONFIG_BCMWL6 != y
export KERNELCC := $(CC)
endif
else # CONFIG_LINUX26 != y
export KERNELCC := $(CC)-3.4.6
endif

ifeq ($(RTCONFIG_BCMWL6),y)
ifneq ($(RTCONFIG_BCMARM),y)
# e.g. RT-AC66U
export KERNELLD := $(SRCBASE)/../../tools/K26/hndtools-mipsel-linux-uclibc-4.2.3/bin/mipsel-linux-uclibc-ld
else # RTCONFIG_BCMARM = y
export KERNELLD := $(LD)
endif
else # RTCONFIG_BCMWL6 != y
export KERNELLD := $(LD)
endif
endif

ifeq ($(KERNELCC),)
export KERNELCC := $(CC)
endif
ifeq ($(KERNELLD),)
export KERNELLD := $(LD)
endif

#	ifneq ($(STATIC),1)
#	SIZECHECK = @$(SRCBASE)/btools/sizehistory.pl $@ $(TOMATO_PROFILE_L)_$(notdir $@)
#	else
SIZECHECK = @$(SIZE) $@
#	endif
