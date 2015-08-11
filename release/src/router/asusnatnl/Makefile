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
	cd $(TOP)/asusnatnl/pjproject-1.12 && \
	./$(CONFIGURE) && make distclean && \
	cd $(TOP)/asusnatnl/miniupnpc-1.6 && make clean && \
	cd $(TOP)/asusnatnl/udt && make clean && \
	cd $(TOP)/asusnatnl/natnl && make clean
#	cd $(TOP)/asusnatnl/natnl/msvc_prjs/asusnatnl_test && make clean
#cd $(TOP)/asusnatnl/pjproject-1.12/third_party/bzip2 && make dist clean && \

pjproject-1.12/stamp-h1-%:
	cd pjproject-1.12 && AR="$(AR) -r -v" \
	./$(CONFIGURE)
	@touch $@
