VPATH = src
LD= $(CC)
OS_LAYER = osl-linux.c

all: lld2d lld2test

include src/common.mk

CFLAGS += -I$(TOP)/shared
CFLAGS += -I$(SRCBASE)/include
LDFLAGS += -L$(TOP)/shared -lshared
LDFLAGS += -L$(TOP)/nvram -lnvram

include $(SRCBASE)/.config

depend:
	$(CC) $(CFLAGS) -M $(DCFILES) >.depend

clean:
	rm -f *.o lld2d lld2test

install: all
	@ echo " [lld2d] Installing..."
	install -D lld2d $(INSTALLDIR)/usr/sbin/lld2d
	install -D lld2d.conf $(INSTALLDIR)/rom/etc/lld2d.conf
ifeq ($(RTN66U), y)
	install -D icon.RT-N66U.ico $(INSTALLDIR)/rom/etc/icon.ico
	install -D icon.large.RT-N66U.ico $(INSTALLDIR)/rom/etc/icon.large.ico
else ifeq ($(RTN56U)$(RTN65U), y)
	install -D icon.RT-N56U.ico $(INSTALLDIR)/rom/etc/icon.ico
	install -D icon.large.RT-N56U.ico $(INSTALLDIR)/rom/etc/icon.large.ico
else ifeq ($(DSLN55U), y)
	install -D icon.DSL-N55U.ico $(INSTALLDIR)/rom/etc/icon.ico
	install -D icon.large.DSL-N55U.ico $(INSTALLDIR)/rom/etc/icon.large.ico
else ifeq ($(RTN14U)$(RTAC52U)$(RTAC51U)$(RTN54U)$(RTAC1200HP)$(RTN56UB1)$(RTAC54U),y)
	install -D icon.RT-N14U.ico $(INSTALLDIR)/rom/etc/icon.ico
	install -D icon.large.RT-N14U.ico $(INSTALLDIR)/rom/etc/icon.large.ico
else
	install -D icon.ico $(INSTALLDIR)/rom/etc/icon.ico
	install -D icon.large.ico $(INSTALLDIR)/rom/etc/icon.large.ico
	[ ! -e icon.$(BUILD_NAME).ico ] || install -D icon.$(BUILD_NAME).ico $(INSTALLDIR)/rom/etc/icon.ico
	[ ! -e icon.large.$(BUILD_NAME).ico ] || install -D icon.large.$(BUILD_NAME).ico $(INSTALLDIR)/rom/etc/icon.large.ico
endif
	$(STRIP) $(INSTALLDIR)/usr/sbin/lld2d
	chmod 0500 $(INSTALLDIR)/usr/sbin/lld2d

#-include .depend
