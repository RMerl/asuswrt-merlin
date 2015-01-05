include ../common.mak

include $(SRCBASE)/.config

all:

install: all
	install -d $(INSTALLDIR)/usr/sbin
ifeq ($(RTAC3200), y)
	install lld2d.rtac3200 $(INSTALLDIR)/usr/sbin/lld2d
	install icon.rtac3200.ico $(INSTALLDIR)/usr/sbin/icon.ico
	install icon.large.rtac3200.ico $(INSTALLDIR)/usr/sbin/icon.large.ico
else ifeq ($(RTAC56U), y)
	install lld2d.rtac56u $(INSTALLDIR)/usr/sbin/lld2d
	install icon.rtac56u.ico $(INSTALLDIR)/usr/sbin/icon.ico
	install icon.large.rtac56u.ico $(INSTALLDIR)/usr/sbin/icon.large.ico
else ifeq ($(RTAC87U), y)
	install lld2d.rtac87u $(INSTALLDIR)/usr/sbin/lld2d
	install icon.rtac87u.ico $(INSTALLDIR)/usr/sbin/icon.ico
	install icon.large.rtac87u.ico $(INSTALLDIR)/usr/sbin/icon.large.ico
else ifeq ($(RTAC68U), y)
	install lld2d.rtac68u $(INSTALLDIR)/usr/sbin/lld2d
	install lld2d.rtac68p $(INSTALLDIR)/usr/sbin/lld2d.rtac68p
	install lld2d.rtac68r $(INSTALLDIR)/usr/sbin/lld2d.rtac68r
	install lld2d.rtac68w $(INSTALLDIR)/usr/sbin/lld2d.rtac68w
	install lld2d.rtac68rw $(INSTALLDIR)/usr/sbin/lld2d.rtac68rw
	install icon.rtac68u.ico $(INSTALLDIR)/usr/sbin/icon.ico
	install icon.large.rtac68u.ico $(INSTALLDIR)/usr/sbin/icon.large.ico
else
	install lld2d $(INSTALLDIR)/usr/sbin
	install icon.ico $(INSTALLDIR)/usr/sbin
	install icon.large.ico $(INSTALLDIR)/usr/sbin
endif
	install lld2d.conf $(INSTALLDIR)/usr/sbin

clean:
