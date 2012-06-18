# We have our own, simpler dependencies stuff in 'reconf'
AUTOMAKE_OPTIONS = no-dependencies foreign

@SET_MAKE@

export LIBDIR=$(libdir)/pptpd
INCLUDES = -I.
## Change this if you don't have gcc
## -pedantic removed for now (OpenBSD header files).
## -Werror removed for now (getopt stuff on OSF/1 throws a
##  warning with -Wmissing-prototypes).
## -Wmissing-prototypes removed (eg, Linux 2.2.6 headers
##  aren't up to it).
CFLAGS = -O2 -fno-builtin -Wall -DSBINDIR='"$(sbindir)"'
#CFLAGS = -O2 -fno-builtin -Wall -ansi -DSBINDIR='"$(sbindir)"'
#CFLAGS = -O2 -fno-builtin -Wall -ansi -pedantic -Wmissing-prototypes -Werror -DSBINDIR='"$(sbindir)"'

man_MANS = pptpctrl.8 pptpd.8 pptpd.conf.5

EXTRA_DIST = \
html README* ChangeLog NEWS TODO $(man_MANS) \
samples/chap-secrets samples/options.pptpd samples/pptpd.conf \
Makefile.uClinux config.embed.h version reconf \
tools/pptp-portslave tools/vpnstats tools/vpnstats.pl tools/vpnuser \
pptpd.init pptpd.spec makepackage \
plugins/Makefile plugins/*.h plugins/*.c \
debian/README.debian debian/changelog debian/conffiles debian/config \
debian/control debian/copyright debian/dirs debian/docs \
debian/examples debian/pptpd-options debian/pptpd.conf \
debian/pptpd.init debian/pptpdconfig.pl debian/rules debian/templates \
debian/po/POTFILES.in debian/po/fr.po debian/po/pt_BR.po \
debian/po/templates.pot

EXTRA_PROGRAMS = 
sbin_PROGRAMS = pptpd pptpctrl bcrelay

# Header files are only used to determine what to put in a distribution, not
# for dependencies, so just attribute them all to pptpd.

pptpd_SOURCES = \
	 pqueue.c pptpd.c configfile.c pptpmanager.c compat.c inststr.c getopt.c getopt1.c \
	pqueue.h compat.h configfile.h ctrlpacket.h defaults.h inststr.h our_getopt.h \
	our_syslog.h ppphdlc.h pptpctrl.h pptpdefs.h pptpgre.h pptpmanager.h

pptpctrl_SOURCES = \
	pqueue.c pptpctrl.c ctrlpacket.c inststr.c compat.c pptpgre.c ppphdlc.c

pptpd_LDADD = $(XTRALIBS_MGR)

pptpctrl_LDADD = $(XTRALIBS_CTRL)

bcrelay_SOURCES = bcrelay.c defaults.h our_syslog.h our_getopt.h

subdirs = plugins

all-local:
	for d in $(subdirs); do $(MAKE) $(MFLAGS) -C $$d all; done

install-exec-local:
	for d in $(subdirs); do $(MAKE) $(MFLAGS) -C $$d prefix=$(prefix) DESTDIR=$(DESTDIR) install; done

clean-local:
	for d in $(subdirs); do $(MAKE) $(MFLAGS) -C $$d clean; done

uninstall-local:
	for d in $(subdirs); do $(MAKE) $(MFLAGS) -C $$d prefix=$(prefix) DESTDIR=$(DESTDIR) uninstall; done

package: deb rpm

deb:
	fakeroot dpkg-buildpackage -us -uc

rpm:
	fakeroot rpmbuild -ta pptpd-$(VERSION).tar.gz

##CLEANFILES = 

## DO NOT ADD BELOW THIS POINT, DEPS ARE AUTOMATICALLY ADDED
bcrelay.o: bcrelay.c config.h defaults.h our_syslog.h our_getopt.h

compat.o: compat.c config.h compat.h our_syslog.h inststr.h

configfile.o: configfile.c config.h defaults.h configfile.h our_syslog.h

ctrlpacket.o: ctrlpacket.c config.h our_syslog.h pptpdefs.h pptpctrl.h \
  ctrlpacket.h compat.h

getopt.o: getopt.c config.h our_getopt.h

getopt1.o: getopt1.c config.h our_getopt.h

inststr.o: inststr.c config.h inststr.h compat.h

ppphdlc.o: ppphdlc.c config.h ppphdlc.h

pptpctrl.o: pptpctrl.c config.h our_syslog.h compat.h pptpctrl.h \
  pptpgre.h pptpdefs.h ctrlpacket.h defaults.h

pptpd.o: pptpd.c config.h our_syslog.h our_getopt.h configfile.h \
  defaults.h compat.h pptpmanager.h

pptpgre.o: pptpgre.c config.h our_syslog.h ppphdlc.h pptpgre.h pptpdefs.h \
  pptpctrl.h defaults.h pqueue.h compat.h

pptpmanager.o: pptpmanager.c config.h our_syslog.h configfile.h \
  defaults.h pptpctrl.h pptpdefs.h pptpmanager.h compat.h

pqueue.o: pqueue.c pqueue.h

