all:

install: all
	install -d $(INSTALLDIR)/usr/sbin
ifeq ($(RTAC56U), y)
	install lld2d.rtac56u $(INSTALLDIR)/usr/sbin/lld2d
	install icon.rtac56u.ico $(INSTALLDIR)/usr/sbin/icon.ico
	install icon.large.rtac56u.ico $(INSTALLDIR)/usr/sbin/icon.large.ico
else ifeq ($(RTAC68U), y)
	install lld2d.rtac68u $(INSTALLDIR)/usr/sbin/lld2d
	install icon.rtac68u.ico $(INSTALLDIR)/usr/sbin/icon.ico
	install icon.large.rtac68u.ico $(INSTALLDIR)/usr/sbin/icon.large.ico
else
	install lld2d $(INSTALLDIR)/usr/sbin
	install icon.ico $(INSTALLDIR)/usr/sbin
	install icon.large.ico $(INSTALLDIR)/usr/sbin
endif
	install lld2d.conf $(INSTALLDIR)/usr/sbin

clean:
