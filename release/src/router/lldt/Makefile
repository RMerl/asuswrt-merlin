VPATH = src
CFLAGS += -O3 -pipe -mips32 -mtune=mips32 -Wall -Wno-unused -s -fpack-struct -Wpadded  -D__LITTLE_ENDIAN
LD= $(CC)
OS_LAYER = osl-linux.c

all: lld2d lld2test

include src/common.mk

CFLAGS += -I$(TOP)/shared

depend:
	$(CC) $(CFLAGS) -M $(DCFILES) >.depend

clean:
	rm -f *.o lld2d lld2test

install: all
	@ echo " [lld2d] Installing..."
	install -D lld2d $(INSTALLDIR)/bin/lld2d
	install -D lld2d.conf $(INSTALLDIR)/rom/etc/lld2d.conf
ifeq ($(RTN66U), y)
	install -D icon.RT-N66U.ico $(INSTALLDIR)/rom/etc/icon.ico
	install -D icon.large.RT-N66U.ico $(INSTALLDIR)/rom/etc/icon.large.ico
else ifeq ($(RTN56U), y)
	install -D icon.RT-N56U.ico $(INSTALLDIR)/rom/etc/icon.ico
        install -D icon.large.RT-N56U.ico $(INSTALLDIR)/rom/etc/icon.large.ico
else
	install -D icon.ico $(INSTALLDIR)/rom/etc/icon.ico
	install -D icon.large.ico $(INSTALLDIR)/rom/etc/icon.large.ico
endif
	$(STRIP) $(INSTALLDIR)/bin/lld2d
	chmod 0500 $(INSTALLDIR)/bin/lld2d

#-include .depend
