include ../common.mak

ifeq ($(RTCONFIG_BCMARM),y)
SRCDIR=broadcom_arm
else
SRCDIR=broadcom_mipsel
endif


stamp-h1:
	touch stamp-h1

all: stamp-h1
	@echo " [cloudcheck] do nothing."

install:
	@echo " [cloudcheck] installing..."
	install -D $(SRCDIR)/booster $(INSTALLDIR)/bin/booster
	install -D $(SRCDIR)/booster_watchdog $(INSTALLDIR)/bin/booster_watchdog
	install -D $(SRCDIR)/libpolarssl.so.0 $(INSTALLDIR)/lib/libpolarssl.so.0
	cd $(INSTALLDIR)/lib && ln -sf libpolarssl.so.0 libpolarssl.so

clean:
	rm -f stamp-h1

