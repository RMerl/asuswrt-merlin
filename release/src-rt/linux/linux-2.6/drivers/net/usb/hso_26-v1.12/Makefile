#
# Makefile for the Linux WWAN device drivers.
#
# NOTE: This make file can serve as both an external Makefile (launched
#       directly by the user), or as the sub-dir Makefile used by the kernel
# 	build system.

# If CONFIG_HSO isn't set, we'll assume the user has never configured
# their kernel to include this module and set up some defaults.
#
# NOTE: If you have previously added the HSO project to your kernel 
# 	and configured it for inclusion, these settings will be 
#	overridden by your kernel configuration.
ifndef CONFIG_HSO
EXTERNAL_BUILD=y
CONFIG_HSO=m
CONFIG_HSO_DEBUG=y
ifeq ($(CONFIG_USB_SUSPEND),y)
CONFIG_HSO_AUTOPM=y
endif
endif


ifeq ($(CONFIG_HSO_DEBUG),y)
	EXTRA_CFLAGS += -g -Wa,-adhlms=$@.lst
endif

ifdef KBUILD_EXTMOD
	EXTRA_CFLAGS += -I$(KBUILD_EXTMOD)
endif


list-m :=
list-$(CONFIG_HSO) += hso
obj-$(CONFIG_HSO) += hso.o

#
# Begin dual Makefile mode here.  First we provide support for when we
# are being invoked by the kernel build system
#
ifneq ($(KERNELRELEASE),)

EXTRA_CFLAGS += -DCONFIG_PM

ifeq ($(EXTERNAL_BUILD),y)
ifeq ($(CONFIG_HSO_DEBUG),y)
EXTRA_CFLAGS += -DCONFIG_HSO_DEBUG
endif
ifeq ($(CONFIG_HSO_AUTOPM),y)
EXTRA_CFLAGS += -DCONFIG_HSO_AUTOPM
endif
endif

else 
# Here we begin the portion that is executed if the user invoked this Makefile
# directly.

# KSRC should be set to the path to your sources
KVER := $(shell uname -r)
KSRC := /lib/modules/$(KVER)/build
INSTALLDIR:=/lib/modules/$(KVER)/kernel/drivers/net/wwan/

# KSRC_OUTPUT should be overridden if you are using a 2.6 kernel that
# has it's output sent elsewhere via KBUILD_OUTPUT= or O=
KSRC_OUTPUT := $(KSRC)

PWD=$(shell pwd)

VERFILE := $(KSRC_OUTPUT)/include/linux/version.h
KERNELRELEASE := $(shell \
	if [ -r $(VERFILE) ]; then \
		(cat $(VERFILE); echo UTS_RELEASE) | \
		$(CC) -I$(KSRC_OUTPUT) $(CFLAGS) -E - | \
		tail -n 1 | \
		xargs echo; \
        else \
		uname -r; \
	fi)

MODPATH := $(DESTDIR)/lib/modules/$(KERNELRELEASE)

all: modules

clean:
	$(RM) -f *.mod.* *.mod *.o *.ko .*.o.cmd .*.ko.cmd *.flags .lst 
	$(RM) -f *.lst Module*.symvers modules.order *~ 
	$(RM) -rf $(PWD)/tmp conninfo.ini
	$(RM) -rf .tmp_versions

distclean: clean
	rm -f tags TAGS

TMP=$(PWD)/tmp
MODVERDIR=$(TMP)/.tmp_versions

modules:
	mkdir -p $(MODVERDIR)
ifdef ($(KSRC_OUTPUT)/.tmp_versions)
	-cp $(KSRC_OUTPUT)/.tmp_versions/*.mod $(MODVERDIR)
endif
ifeq ($(KSRC),$(KSRC_OUTPUT)) # We're not outputting elsewhere
ifdef ($(KSRC)/.tmp_versions)
	-cp $(KSRC)/.tmp_versions/*.mod $(MODVERDIR)
endif
	$(MAKE) -C $(KSRC) M=$(PWD) MODVERDIR=$(MODVERDIR) modules
else # We've got a kernel with seperate output, copy the config, and use O=
	mkdir -p $(TMP)
	cp $(KSRC_OUTPUT)/.config $(TMP)
	$(MAKE) -C $(KSRC) M=$(PWD) MODVERDIR=$(MODVERDIR)) O=$(PWD)/tmp modules
endif

install: modules
	modprobe -r hso || true
	install -d $(INSTALLDIR)
	install -m 644 -c $(addsuffix .ko,$(list-m)) $(INSTALLDIR)
	/sbin/depmod -a ${KVER}

uninstall:
	rm -rf $(addprefix $(INSTALLDIR),$(addsuffix .ko,$(list-m)))
	/sbin/depmod -a ${KVER}

endif # End of internal build

.PHONY: TAGS tags

RCS_FIND_IGNORE := \( -name SCCS -o -name BitKeeper -o -name .svn -o -name CVS \) -prune -o
define all-sources
	( find . $(RCS_FIND_IGNORE) -name '*.[chS]' -print )
endef

TAGS:
	$(all-sources) | etags -
tags:
	$(RM) -f $@
	CTAGSF=`ctags --version | grep -i exuberant >/dev/null && echo "-I __initdata,__exitdata,EXPORT_SYMBOL,EXPORT_SYMBOL_NOVERS"`; \
	$(all-sources) | xargs ctags $$CTAGSF -a

