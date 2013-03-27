ifeq ($(DSTDIR),)
DSTDIR=.
endif

KERNEL_BUILD_DIR=$(LINUXDIR)
KERNEL_SOURCE_DIR=$(LINUXDIR)
TOOLCHAIN_DIR=$(shell which $(CROSS_COMPILE)gcc)/../..
CFLAGS=-I$(KERNEL_SOURCE_DIR)/arch/mips/include -I$(KERNEL_SOURCE_DIR)/arch/mips/include/generated -I$(KERNEL_SOURCE_DIR)/include -D__KERNEL__ -DVMLINUX_LOAD_ADDRESS=0x80000000 -DDATAOFFSET=0 -fno-strict-aliasing -fno-common -fno-delete-null-pointer-checks -mno-check-zero-division -mabi=32 -mno-abicalls -fno-pic -pipe -msoft-float -march=mips32r2 -Wa,-mips32r2 -Wa,--trap -I$(KERNEL_SOURCE_DIR)/arch/mips/include/asm/rt2880 -I$(KERNEL_SOURCE_DIR)/arch/mips/include/asm/mach-generic -fno-stack-protector -fomit-frame-pointer -fno-strict-overflow -DMODULE -mlong-calls

all: package/Makefile
	ARCH=mips EXT_MODULE_FLAGS=" -DUFSD_DISABLE_UGM -DUFSD_DISABLE_UGM -DUFSD_CHECK_BDI" $(MAKE) -C package driver

package/Makefile:
	( cd package ; \
	  CFLAGS="$(CFLAGS) -O1" $(CONFIGURE) \
	  --host=mipsel-linux-uclibc \
	  --with-ks-dir=$(KERNEL_SOURCE_DIR) \
	  --with-kb-dir=$(KERNEL_BUILD_DIR) \
	)

clean:
	-[ ! -f package/Makefile ] || $(MAKE) -C package $@
	-rm -f package/*.ko package/Makefile

install:
	@echo "Override pre-built ufsd.ko"
	@cp -f package/ufsd.ko $(DSTDIR)

.phony: all clean install
