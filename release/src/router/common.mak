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

export BUILD := $(shell (gcc -dumpmachine))
export HOSTCC := gcc

ifeq ($(RTCONFIG_BCMARM),y)
export PLATFORM := arm-uclibc
export CROSS_COMPILE := arm-brcm-linux-uclibcgnueabi-
export CROSS_COMPILER := $(CROSS_COMPILE)
export CONFIGURE := ./configure --host=arm-linux --build=$(BUILD)
export HOSTCONFIG := linux-armv4
export BCMEX := _arm
export EXTRA_FLAG := -lgcc_s
export ARCH := arm
export HOST := arm-linux
export TOOLS := $(SRCBASE)/toolchains/hndtools-arm-linux-2.6.36-uclibc-4.5.3
export RTVER := 0.9.32.1
else
export PLATFORM := mipsel-uclibc
export CROSS_COMPILE := mipsel-uclibc-
export CROSS_COMPILER := $(CROSS_COMPILE)
export CONFIGURE := ./configure --host=mipsel-linux --build=$(BUILD)
export HOSTCONFIG := linux-mipsel
export ARCH := mips
export HOST := mipsel-linux
export TOOLS := $(SRCBASE)/../../tools/brcm/hndtools-mipsel-linux
export RTVER := 0.9.30.1
endif

export PLT := $(ARCH)
export TOOLCHAIN := $(shell cd $(dir $(shell which $(CROSS_COMPILE)gcc))/.. && pwd -P)

export CC := $(CROSS_COMPILE)gcc
export CXX := $(CROSS_COMPILE)g++
export AR := $(CROSS_COMPILE)ar
export AS := $(CROSS_COMPILE)as
export LD := $(CROSS_COMPILE)ld
export NM := $(CROSS_COMPILE)nm
export OBJCOPY := $(CROSS_COMPILE)objcopy
export RANLIB := $(CROSS_COMPILE)ranlib
ifeq ($(RTCONFIG_BCMARM),y)
export STRIP := $(CROSS_COMPILE)strip
else
export STRIP := $(CROSS_COMPILE)strip -R .note -R .comment
endif
export SIZE := $(CROSS_COMPILE)size

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


include $(SRCBASE)/target.mak

export LIBDIR := $(TOOLCHAIN)/lib
export USRLIBDIR := $(TOOLCHAIN)/usr/lib

export PLATFORMDIR := $(TOP)/$(PLATFORM)
export INSTALLDIR := $(PLATFORMDIR)/install
export TARGETDIR := $(PLATFORMDIR)/target
export STAGEDIR := $(PLATFORMDIR)/stage

ifeq ($(EXTRACFLAGS),)
ifeq ($(RTCONFIG_BCMARM),y)
export EXTRACFLAGS := -DBCMWPA2 -DBCMARM -fno-delete-null-pointer-checks -marm
else
export EXTRACFLAGS := -DBCMWPA2 -fno-delete-null-pointer-checks -mips32r2 -mtune=mips32r2
endif
endif
export EXTRACFLAGS += -DLINUX_KERNEL_VERSION=$(LINUX_KERNEL_VERSION)

CPTMP = @[ -d $(TOP)/dbgshare ] && cp $@ $(TOP)/dbgshare/ || true


ifeq ($(CONFIG_RALINK),y)

# Ralink SoC
ifeq ($(CONFIG_LINUX30),y)
# linux-3.x, e.g. RT-N65U.
export KERNELCC := $(CC)
export KERNELLD := $(LD)
else
# linux-2.6.21.x, e.g. RT-N56U.
export KERNELCC := /opt/buildroot-gcc342/bin/mipsel-linux-uclibc-gcc
export KERNELLD := /opt/buildroot-gcc342/bin/mipsel-linux-uclibc-ld
endif

else # CONFIG_RALINK != y

# Broadcom SoC
ifeq ($(CONFIG_LINUX26),y)
export KERNELCC := $(CC)
else
export KERNELCC := $(CC)-3.4.6
endif

export KERNELLD := $(LD)
endif

#	ifneq ($(STATIC),1)
#	SIZECHECK = @$(SRCBASE)/btools/sizehistory.pl $@ $(TOMATO_PROFILE_L)_$(notdir $@)
#	else
SIZECHECK = @$(SIZE) $@
#	endif
