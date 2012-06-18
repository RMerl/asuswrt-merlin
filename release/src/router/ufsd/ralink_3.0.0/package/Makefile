ifdef src
    src := $(src)/
endif

#Module
#Module
MODULE_SRC := ifslinux
ifeq (0,1)
MODULE_NAME := ufsd_debug
else
MODULE_NAME := ufsd
endif

MODULE_LIBNAME := libufsd
MODULE_OBJECTS := $(MODULE_SRC)/ufsdvfs.o $(MODULE_SRC)/vfsdebug.o

MODULE_EXTRA_FLAGS := -include $(src)$(MODULE_SRC)/fs_conf.h -DUFSD_DEVICE=ufsd -DUFSD_USE_ASM_DIV64
ifeq (0,1)
MODULE_EXTRA_FLAGS += -DUFSD_DEBUG
else
MODULE_EXTRA_FLAGS += -DNDEBUG -g0
endif

ifndef CONFIG_UFSD_FS
    CONFIG_UFSD_FS = m
endif

MODULE_OBJECTS += $(MODULE_SRC)/$(MODULE_LIBNAME).bin

#KERNEL_SOURCE_DIR:=/media/ASUSWRT/asuswrt/release/src-ra/linux/linux-3.x
#KERNEL_BUILD_DIR:=/media/ASUSWRT/asuswrt/release/src-ra/linux/linux-3.x
#KERNEL_INSTALL_DIR:=/lib/modules/2.6.38-11-generic-pae/kernel/external/ufsd

#ARCH:=mips
#VERSION:=3.0
#PATCHLEVEL:=@KERNEL_PATCHLEVEL@
#MODULE_EXT:=ko

$(MODULE_NAME)-objs := $(MODULE_OBJECTS)
$(MODULE_NAME)-y := $(MODULE_OBJECTS)

obj-m := $(MODULE_NAME).o

ifeq (3.0,2.4)
O_TARGET := $(MODULE_NAME).o
obj-y := $(MODULE_OBJECTS)
endif

EXTRA_CFLAGS +=	 $(MODULE_EXTRA_FLAGS)

ifeq (3.0,2.4)
EXTRA_CFLAGS += -DNO_MM
endif


ifdef PACKAGE_TAG
EXTRA_CFLAGS += "-DPACKAGE_TAG=\"$(PACKAGE_TAG)\""
endif

ifdef EXT_MODULE_FLAGS
EXTRA_CFLAGS += $(EXT_MODULE_FLAGS)
endif

LDFLAGS   += -s -O2 -d -x -X

$(src)$(MODULE_SRC)/$(MODULE_LIBNAME).bin:
	@ln -s "/media/ASUSWRT/asuswrt/release/src/router/ufsd/ralink_3.0.0/package/ifslinux/objfre/libufsd_mips.bin" "$(src)$(MODULE_SRC)/$(MODULE_LIBNAME).bin"

clean:
	@$(MAKE) -C "/media/ASUSWRT/asuswrt/release/src-ra/linux/linux-3.x" SUBDIRS="/media/ASUSWRT/asuswrt/release/src/router/ufsd/ralink_3.0.0/package" clean 2>&1 > /dev/null
	@/bin/rm -Rf "$(src)$(MODULE_SRC)/$(MODULE_LIBNAME).bin"

-include $(TOPDIR)/Rules.make


$(MODULE_NAME).ko:
ifeq (3.0,2.4)
	@if [ ! -e "/media/ASUSWRT/asuswrt/release/src-ra/linux/linux-3.x/.config" ] ; then $(MAKE) -C "/media/ASUSWRT/asuswrt/release/src-ra/linux/linux-3.x" SUBDIRS="/media/ASUSWRT/asuswrt/release/src/router/ufsd/ralink_3.0.0/package" V=1 cloneconfig >/dev/null 2>&1 ; true ; fi
	@if [ ! -e "/media/ASUSWRT/asuswrt/release/src-ra/linux/linux-3.x/.config" ] ; then echo -e "\033[32mPlease run 'make menuconfig' or 'make defconfig' from kernel source dir to make config file\033[0m" ; exit -1 ; fi
endif
	$(MAKE) -C "/media/ASUSWRT/asuswrt/release/src-ra/linux/linux-3.x" SUBDIRS="/media/ASUSWRT/asuswrt/release/src/router/ufsd/ralink_3.0.0/package" O="/media/ASUSWRT/asuswrt/release/src-ra/linux/linux-3.x" V=1 modules 2>&1

driver:$(MODULE_NAME).ko
	mipsel-linux-uclibc-strip -g --strip-unneeded ufsd.ko

ifeq (1,1)
driver_install: $(MODULE_NAME).ko
ifeq ($(shell /usr/bin/id -u),0)
	@/sbin/modprobe -qr $(MODULE_NAME) 2>&1 > /dev/null ; /bin/mkdir -p /lib/modules/2.6.38-11-generic-pae/kernel/external/ufsd 2>&1 ; /bin/cp $(MODULE_NAME).ko /lib/modules/2.6.38-11-generic-pae/kernel/external/ufsd 2>&1 && /sbin/depmod -a 2>&1 ; /sbin/insmod /lib/modules/2.6.38-11-generic-pae/kernel/external/ufsd/$(MODULE_NAME).ko 2>&1
else
	@echo -e "\033[33mEnter Root password for install driver\033[0m"
	@/bin/su -c '/sbin/modprobe -qr $(MODULE_NAME) 2>&1 > /dev/null ; /bin/mkdir -p /lib/modules/2.6.38-11-generic-pae/kernel/external/ufsd 2>&1 ; /bin/cp $(MODULE_NAME).ko /lib/modules/2.6.38-11-generic-pae/kernel/external/ufsd 2>&1 && /sbin/depmod -a 2>&1 ; /sbin/insmod /lib/modules/2.6.38-11-generic-pae/kernel/external/ufsd/$(MODULE_NAME).ko 2>&1'
endif

driver_uninstall:
ifeq ($(shell /usr/bin/id -u),0)
	@/sbin/modprobe -qr $(MODULE_NAME) 2>&1 ; /bin/rm -Rf /lib/modules/2.6.38-11-generic-pae/kernel/external/ufsd 2>&1 && /sbin/depmod -a 2>&1
else
	@echo -e "\033[33mEnter Root password for uninstall driver\033[0m"
	@/bin/su -c '/sbin/modprobe -qr $(MODULE_NAME) 2>&1 ; /bin/rm -Rf /lib/modules/2.6.38-11-generic-pae/kernel/external/ufsd 2>&1 && /sbin/depmod -a 2>&1'
endif
endif
