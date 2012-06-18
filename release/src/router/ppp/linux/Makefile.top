# PPP top-level Makefile for Linux.


BINDIR = $(DESTDIR)/usr/sbin
MANDIR = $(DESTDIR)/usr/man
ETCDIR = $(DESTDIR)/etc/ppp

# uid 0 = root
INSTALL= install

all:
	cd chat; $(MAKE) $(MFLAGS) all
	cd pppd/plugins; $(MAKE) $(MFLAGS) all
	cd pppd; $(MAKE) $(MFLAGS) all
	cd pppstats; $(MAKE) $(MFLAGS) all
	cd pppdump; $(MAKE) $(MFLAGS) all

install: $(BINDIR) $(MANDIR)/man8 install-progs install-etcppp

install-progs:
	cd chat; $(MAKE) BINDIR=$(BINDIR) MANDIR=$(MANDIR) $(MFLAGS) install
	cd pppd/plugins; $(MAKE) BINDIR=$(BINDIR) MANDIR=$(MANDIR) $(MFLAGS) install
	cd pppd; $(MAKE) BINDIR=$(BINDIR) MANDIR=$(MANDIR) $(MFLAGS) install
	cd pppstats; $(MAKE) BINDIR=$(BINDIR) MANDIR=$(MANDIR) $(MFLAGS) install
	cd pppdump; $(MAKE) BINDIR=$(BINDIR) MANDIR=$(MANDIR) $(MFLAGS) install

install-etcppp: $(ETCDIR) $(ETCDIR)/options $(ETCDIR)/pap-secrets \
	$(ETCDIR)/chap-secrets

$(ETCDIR)/options:
	$(INSTALL) -c -m 644 etc.ppp/options $@
$(ETCDIR)/pap-secrets:
	$(INSTALL) -c -m 600 etc.ppp/pap-secrets $@
$(ETCDIR)/chap-secrets:
	$(INSTALL) -c -m 600 etc.ppp/chap-secrets $@

$(BINDIR):
	$(INSTALL) -d -m 755 $@
$(MANDIR)/man8:
	$(INSTALL) -d -m 755 $@
$(ETCDIR):
	$(INSTALL) -d -m 755 $@

clean:
	rm -f `find . -name '*.[oas]' -print`
	rm -f `find . -name 'core' -print`
	rm -f `find . -name '*~' -print`
	cd chat; $(MAKE) clean
	cd pppd/plugins; $(MAKE) clean
	cd pppd; $(MAKE) clean
	cd pppstats; $(MAKE) clean
	cd pppdump; $(MAKE) clean

dist-clean:	clean
	rm -f Makefile `find . -name Makefile -print`

#kernel:
#	cd linux; ./kinstall.sh
