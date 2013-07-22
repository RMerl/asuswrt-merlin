include ../../common.mak

ifeq ($(DSTDIR),i)
DSTDIR=.
endif

export KERNEL_SOURCE_PATH:=$(LINUXDIR)
export KERNEL_BUILD_PATH:=$(LINUXDIR)
export COMPILER_NAME:=arm-brcm-linux-uclibcgnueabi
export CC:=${COMPILER_NAME}-gcc
export COMPILER_PATH:=$(TOOLCHAIN)/bin

all: package/Makefile
	$(MAKE) -C package clean
	$(MAKE) -C package driver
	cp -f package/ufsd.ko ./

package/Makefile:
	( cd package ; \
	./configure --host=${COMPILER_NAME} --with-ks-dir=${KERNEL_SOURCE_PATH} --with-kb-dir=${KERNEL_BUILD_PATH} PATH="${COMPILER_PATH}:${PATH}" CFLAGS="-I$(KERNEL_BUILD_PATH)/arch/arm/include -I$(KERNEL_BUILD_PATH)/include -mlittle-endian -I$(KERNEL_BUILD_PATH)/arch/arm/mach-brcm-hnd/include -I$(KERNEL_BUILD_PATH)/arch/arm/plat-brcm/include -fno-strict-aliasing -fno-common -fno-delete-null-pointer-checks -DBCMDRIVER -Dlinux -DHNDCTF -DCTFPOOL -DCTFMAP -DPKTC -DCTF_PPPOE -DCTF_IPV6 -marm -ffreestanding -fno-dwarf2-cfi-asm -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables -D__LINUX_ARM_ARCH__=7 -march=armv7-a -msoft-float -Uarm -fno-stack-protector -fomit-frame-pointer -fno-strict-overflow -fconserve-stack " CC=${COMPILER_NAME}-gcc CROSSCOMPILE=${COMPILER_NAME}- TARGET=arm ARCH=arm CROSS_COMPILE=${COMPILER_NAME}- EXT_MODULE_FLAGS="-DUFSD_DISABLE_UGM -DUFSD_NTFS_LOGFILE -DUFSD_CHECK_BDI -DUFSD_SMART_DIRTY -DUFSD_DIRTY_OFF" PACKAGE_TAG="U86_r195788_b2" \
	)

clean:
	-[ ! -f package/Makefile ] || $(MAKE) -C package $@
	-rm -f config.log package/config.log package/ifslinux/config.h *.ko package/*.ko package/Makefile

install:
	echo "Override pre-built ufsd.ko"
	cp -f package/ufsd.ko $(DSTDIR)

.phony: all clean install
