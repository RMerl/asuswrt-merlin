include ../common.mak

ifneq ($(wildcard $(SRCBASE)/wl/$(PLATFORM)/eapd),)
UTILS := wl nas eapd
else
UTILS := wl nas
endif

all: $(UTILS)

clean:
	rm -f *~ $(UTILS)

ifneq ($(wildcard $(SRCBASE)/wl/$(PLATFORM)/wl),)
wl:
	@cp -f $(SRCBASE)/wl/$(PLATFORM)/wl wl
else
wl: wl_exe.o
	$(CC) $(CCFLAGS) -DBCMWPA2 -o $@ $<
endif

ifneq ($(wildcard $(SRCBASE)/wl/$(PLATFORM)/nas),)
nas:
	@cp -f $(SRCBASE)/wl/$(PLATFORM)/nas nas
else
nas: nas_exe.o
	$(CC) $(CCFLAGS) -DBCMWPA2 -o $@ $< -L../shared  -L../nvram -lnvram -lshared
endif

ifneq ($(wildcard $(SRCBASE)/wl/$(PLATFORM)/eapd),)
eapd:
	@cp -f $(SRCBASE)/wl/$(PLATFORM)/eapd eapd
endif

install: all
	install -d $(INSTALLDIR)/usr/sbin

	install -m 0755 nas $(INSTALLDIR)/usr/sbin/nas
	$(STRIP) $(INSTALLDIR)/usr/sbin/nas
	# nas4not is now a symbolic link to nas
	ln -sf nas $(INSTALLDIR)/usr/sbin/nas4not
	
	install -m 0755 wl $(INSTALLDIR)/usr/sbin/wl
	$(STRIP) $(INSTALLDIR)/usr/sbin/wl
	
ifneq ($(wildcard $(SRCBASE)/wl/$(PLATFORM)/eapd),)
	install -d $(INSTALLDIR)/bin
	install -m 0755 eapd $(INSTALLDIR)/bin/eapd
	$(STRIP) $(INSTALLDIR)/bin/eapd
endif

