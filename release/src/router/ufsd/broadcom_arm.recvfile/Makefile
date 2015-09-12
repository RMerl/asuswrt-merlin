include ../../common.mak

PACKAGE_TAG="lke_9.2.0_r255171_b9"

ifeq ($(DSTDIR),i)
DSTDIR=.
endif

ifeq ($(DEBUG),y)
DFLAG="--enable-ufsd-debug"
BINARY1=ufsd_debug.ko
BINARY2=jnl_debug.ko
else
DFLAG=
BINARY1=ufsd.ko
BINARY2=jnl.ko
endif

export KERNEL_SOURCE_PATH:=$(LINUXDIR)
export KERNEL_BUILD_PATH:=$(LINUXDIR)
export COMPILER_NAME:=arm-brcm-linux-uclibcgnueabi
export COMPILER_TARGET:=arm
export CC:=${COMPILER_NAME}-gcc
export COMPILER_PATH:=$(TOOLCHAIN)/bin

all: package/Makefile
	$(MAKE) -C package clean
	$(MAKE) -C package driver

package/Makefile:
	( cd package ; \
	./configure --host=${COMPILER_NAME} --with-ks-dir=${KERNEL_SOURCE_PATH} --with-kb-dir=${KERNEL_BUILD_PATH} PATH="${COMPILER_PATH}:${PATH}" CFLAGS="-I$(KERNEL_BUILD_PATH)/arch/arm/include -I$(KERNEL_BUILD_PATH)/include -mlittle-endian -I$(KERNEL_BUILD_PATH)/arch/arm/mach-brcm-hnd/include -I$(KERNEL_BUILD_PATH)/arch/arm/plat-brcm/include -fno-strict-aliasing -fno-common -fno-delete-null-pointer-checks -DBCMDRIVER -Dlinux -DHNDCTF -DCTFPOOL -DCTFMAP -DPKTC -DCTF_PPPOE -DCTF_PPTP -DCTF_L2TP -DCTF_IPV6 -DBCM47XX -marm -ffreestanding -fno-dwarf2-cfi-asm -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables -D__LINUX_ARM_ARCH__=7 -march=armv7-a -msoft-float -Uarm -fno-stack-protector -fomit-frame-pointer -fno-strict-overflow -fconserve-stack -DBCMFA -DCONFIG_LINUX_MTD=64 -ggdb -O1" CC=${CC} CROSS_COMPILE=${COMPILER_NAME}- CROSSCOMPILE=${COMPILER_NAME}- TARGET=${COMPILER_TARGET} --target=${COMPILER_TARGET} ARCH=arm EXT_MODULE_FLAGS="-DUFSD_DISABLE_UGM -DUFSD_CHECK_BDI -DUFSD_TRACE -DUFSD_ALLOW_MUL_WRITE_BEGIN" PACKAGE_TAG="${PACKAGE_TAG}" ${DFLAG} \
	)

clean:
	-[ ! -f package/Makefile ] || $(MAKE) -C package $@
	-rm -f config.log package/config.status package/config.log package/ifslinux/config.h package/*.ko package/Makefile

install:
	cp -f package/$(BINARY1) $(DSTDIR)
	cp -f package/$(BINARY2) $(DSTDIR)
	cp -f package/$(BINARY1) ./
	cp -f package/$(BINARY2) ./

.phony: all clean install

