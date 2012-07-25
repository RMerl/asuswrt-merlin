all:

install: all
	install -d $(INSTALLDIR)/usr/sbin
ifeq ($(RTAC66U), y)
	install lld2d.rtac66u $(INSTALLDIR)/usr/sbin/lld2d
	install icon.rtn66u.ico $(INSTALLDIR)/usr/sbin/icon.ico
	install icon.large.rtn66u.ico $(INSTALLDIR)/usr/sbin/icon.large.ico
else ifeq ($(RTN66U), y)
	install lld2d.rtn66u $(INSTALLDIR)/usr/sbin/lld2d
	install icon.rtn66u.ico $(INSTALLDIR)/usr/sbin/icon.ico
	install icon.large.rtn66u.ico $(INSTALLDIR)/usr/sbin/icon.large.ico
else
	install lld2d $(INSTALLDIR)/usr/sbin
	install icon.ico $(INSTALLDIR)/usr/sbin
	install icon.large.ico $(INSTALLDIR)/usr/sbin
endif
	install lld2d.conf $(INSTALLDIR)/usr/sbin

clean:
