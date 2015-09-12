include ../common.mak

ifeq ($(RTCONFIG_BCMARM),y)
CONFIGURE=configure-router-arm
ARCH=ARM
else
CONFIGURE=configure-router-mips
ARCH=MIPS
endif

all: pjproject-1.12/stamp-h1-$(ARCH)
	$(MAKE) -C pjproject-1.12
	$(MAKE) -C miniupnpc-1.6 upnpc-static
	$(MAKE) -C udt os=LINUX arch=$(ARCH)
	$(MAKE) -C natnl linux-lib
#	$(MAKE) -C natnl/msvc_prjs/asusnatnl_test
#	$(MAKE) -C pjproject-1.12/third_party/bzip2

clean:
	-$(MAKE) -C pjproject-1.12 distclean
	-$(MAKE) -C miniupnpc-1.6 clean
	-$(MAKE) -C udt clean
	-$(MAKE) -C natnl clean
#	-$(MAKE) -C natnl/msvc_prjs/asusnatnl_test clean
#	-$(MAKE) -C pjproject-1.12/third_party/bzip2 distclean
	@rm -f pjproject-1.12/stamp-h1-*

pjproject-1.12/stamp-h1-%:
	cd pjproject-1.12 && AR="$(AR) -r -v" \
	./$(CONFIGURE)
	@touch $@
