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
MODULE_EXTRA_FLAGS += -DUFSD_DEBUG -g3
LDFLAGS   += -d
else
MODULE_EXTRA_FLAGS += -DNDEBUG -g0
LDFLAGS   += -s -O2 -d -x -X
endif

ifndef CONFIG_UFSD_FS
    CONFIG_UFSD_FS = m
endif

MODULE_OBJECTS += $(MODULE_SRC)/$(MODULE_LIBNAME).bin

#KERNEL_SOURCE_DIR:=/home/merlin/asuswrt.ac68/release/src-rt-6.x.4708/linux/linux-2.6
#KERNEL_BUILD_DIR:=/home/merlin/asuswrt.ac68/release/src-rt-6.x.4708/linux/linux-2.6
#KERNEL_INSTALL_DIR:=/lib/modules/3.2.0-23-generic/kernel/external/ufsd

#ARCH:=arm
#VERSION:=2.6
#PATCHLEVEL:=@KERNEL_PATCHLEVEL@
#MODULE_EXT:=ko

$(MODULE_NAME)-objs := $(MODULE_OBJECTS)
$(MODULE_NAME)-y := $(MODULE_OBJECTS)

obj-m := $(MODULE_NAME).o

ifeq (2.6,2.4)
O_TARGET := $(MODULE_NAME).o
obj-y := $(MODULE_OBJECTS)
endif

EXTRA_CFLAGS +=	 $(MODULE_EXTRA_FLAGS)

ifeq (2.6,2.4)
EXTRA_CFLAGS += -DNO_MM
endif


ifdef PACKAGE_TAG
EXTRA_CFLAGS += "-DPACKAGE_TAG=\"$(PACKAGE_TAG)\""
endif

ifdef EXT_MODULE_FLAGS
EXTRA_CFLAGS += $(EXT_MODULE_FLAGS)
endif


$(src)$(MODULE_SRC)/$(MODULE_LIBNAME).bin:
	@ln -s "ifslinux/objfre/libufsd_arm.bin" "$(src)$(MODULE_SRC)/$(MODULE_LIBNAME).bin"

clean:
	@$(MAKE) -C "$(SRCBASE)/linux/linux-2.6" SUBDIRS="." clean 2>&1 > /dev/null
	@/bin/rm -Rf "$(src)$(MODULE_SRC)/$(MODULE_LIBNAME).bin"

-include $(TOPDIR)/Rules.make


$(MODULE_NAME).ko:
ifeq (2.6,2.4)
	@if [ ! -e "$(SRCBASE)/linux/linux-2.6/.config" ] ; then $(MAKE) -C "$(SRCBASE)/linux/linux-2.6" SUBDIRS="." V=1 cloneconfig >/dev/null 2>&1 ; true ; fi
	@if [ ! -e "$(SRCBASE)/linux/linux-2.6/.config" ] ; then echo -e "\033[32mPlease run 'make menuconfig' or 'make defconfig' from kernel source dir to make config file\033[0m" ; exit -1 ; fi
endif
	$(MAKE) -C "$(SRCBASE)/linux/linux-2.6" SUBDIRS="." O="$(SRCBASE)/linux/linux-2.6" V=1 modules 2>&1

driver:$(MODULE_NAME).ko
	arm-brcm-linux-uclibcgnueabi-strip -g --strip-unneeded ufsd.ko

ifeq (1,1)
driver_install: $(MODULE_NAME).ko
ifeq ($(shell /usr/bin/id -u),0)
	@/sbin/modprobe -qr $(MODULE_NAME) 2>&1 > /dev/null ; /bin/mkdir -p /lib/modules/3.2.0-23-generic/kernel/external/ufsd 2>&1 ; /bin/cp $(MODULE_NAME).ko /lib/modules/3.2.0-23-generic/kernel/external/ufsd 2>&1 && /sbin/depmod -a 2>&1 ; /sbin/insmod /lib/modules/3.2.0-23-generic/kernel/external/ufsd/$(MODULE_NAME).ko 2>&1
else
	@echo -e "\033[33mEnter Root password for install driver\033[0m"
	@/bin/su -c '/sbin/modprobe -qr $(MODULE_NAME) 2>&1 > /dev/null ; /bin/mkdir -p /lib/modules/3.2.0-23-generic/kernel/external/ufsd 2>&1 ; /bin/cp $(MODULE_NAME).ko /lib/modules/3.2.0-23-generic/kernel/external/ufsd 2>&1 && /sbin/depmod -a 2>&1 ; /sbin/insmod /lib/modules/3.2.0-23-generic/kernel/external/ufsd/$(MODULE_NAME).ko 2>&1'
endif

driver_uninstall:
ifeq ($(shell /usr/bin/id -u),0)
	@/sbin/modprobe -qr $(MODULE_NAME) 2>&1 ; /bin/rm -Rf /lib/modules/3.2.0-23-generic/kernel/external/ufsd 2>&1 && /sbin/depmod -a 2>&1
else
	@echo -e "\033[33mEnter Root password for uninstall driver\033[0m"
	@/bin/su -c '/sbin/modprobe -qr $(MODULE_NAME) 2>&1 ; /bin/rm -Rf /lib/modules/3.2.0-23-generic/kernel/external/ufsd 2>&1 && /sbin/depmod -a 2>&1'
endif
endif
