include ../common.mak

ifeq ($(or $(RTCONFIG_BCMARM),$(RTCONFIG_QCA_ARM)),y)
CONFIGURE=configure-router-arm
ARCH=ARM
ifeq ($(RTCONFIG_QCA_ARM),y)
LDFLAGS="-L/$(LIBDIR) -L$(LIBDIR)/gcc/arm-openwrt-linux-uclibcgnueabi/4.6.3 -lc -lm -ldl -lgcc_s"
else
LDFLAGS=
endif
else
CONFIGURE=configure-router-mips
ARCH=MIPS
ifeq ($(RTCONFIG_QCA),y)
LDFLAGS="-L/$(LIBDIR) -L$(LIBDIR)/gcc/mips-openwrt-linux-uclibc/4.6.3 -lc -lm -ldl -lgcc_s"
else
LDFLAGS=
endif
endif

all: pjproject-1.12/stamp-h1-$(ARCH)
	$(MAKE) -C pjproject-1.12
	$(MAKE) -C udt os=LINUX arch=$(ARCH)
	$(MAKE) -C natnl linux-lib router=yes
#	$(MAKE) -C natnl/msvc_prjs/asusnatnl_test

clean:
	-$(MAKE) -C pjproject-1.12 distclean
	-$(MAKE) -C udt clean
	-$(MAKE) -C natnl clean
	@rm -f pjproject-1.12/stamp-h1-*

pjproject-1.12/stamp-h1-%:
	cd pjproject-1.12 && AR="$(AR) -r -v" \
	./$(CONFIGURE) $(LDFLAGS)
	@touch $@
