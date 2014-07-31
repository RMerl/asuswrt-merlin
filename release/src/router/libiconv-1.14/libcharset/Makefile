# Makefile for libcharset

#### Start of system configuration section. ####


# Directories used by "make":
srcdir = .

# Directories used by "make install":
prefix = /home/mars/asus-git/asuswrt/release/src-rt/router/libiconv-1.14/out
local_prefix = /usr/local
exec_prefix = ${prefix}
datarootdir = ${prefix}/share
datadir = ${datarootdir}
libdir = ${exec_prefix}/lib
includedir = ${prefix}/include
mandir = ${datarootdir}/man

# Programs used by "make":
CP = cp
RM = rm -f


# Programs used by "make install":
INSTALL = /usr/bin/install -c
INSTALL_DATA = ${INSTALL} -m 644
mkinstalldirs = $(SHELL) ./build-aux/mkinstalldirs

#### End of system configuration section. ####

SHELL = /bin/sh

all : include/libcharset.h force
	cd lib && $(MAKE) all

include/libcharset.h :
	if [ ! -d include ] ; then mkdir include ; fi
	$(CP) $(srcdir)/include/libcharset.h.in include/libcharset.h

# Installs the library and include files only. Typically called with only
# $(libdir) and $(includedir) - don't use $(prefix) and $(exec_prefix) here.
install-lib : all force
	cd lib && $(MAKE) install-lib libdir='$(libdir)' includedir='$(includedir)'
	$(mkinstalldirs) $(includedir)
	$(INSTALL_DATA) include/libcharset.h $(includedir)/libcharset.h
	$(INSTALL_DATA) include/localcharset.h.inst $(includedir)/localcharset.h

install : include/libcharset.h include/localcharset.h force
	cd lib && $(MAKE) install prefix='$(prefix)' exec_prefix='$(exec_prefix)' libdir='$(libdir)'
	$(mkinstalldirs) $(DESTDIR)$(includedir)
	$(INSTALL_DATA) include/libcharset.h $(DESTDIR)$(includedir)/libcharset.h
	$(INSTALL_DATA) include/localcharset.h.inst $(DESTDIR)$(includedir)/localcharset.h

install-strip : install

installdirs : force
	cd lib && $(MAKE) installdirs prefix='$(prefix)' exec_prefix='$(exec_prefix)' libdir='$(libdir)'
	$(mkinstalldirs) $(DESTDIR)$(includedir)

uninstall : force
	cd lib && $(MAKE) uninstall prefix='$(prefix)' exec_prefix='$(exec_prefix)' libdir='$(libdir)'
	$(RM) $(DESTDIR)$(includedir)/libcharset.h
	$(RM) $(DESTDIR)$(includedir)/localcharset.h

check : force
	cd lib && $(MAKE) check

mostlyclean : force
	cd lib && $(MAKE) mostlyclean

clean : force
	cd lib && $(MAKE) clean

distclean : force
	cd lib && if test -f Makefile; then $(MAKE) distclean; fi
	$(RM) include/libcharset.h include/localcharset.h include/localcharset.h.inst
	$(RM) config.status config.log config.cache Makefile config.h libtool

maintainer-clean : force
	cd lib && if test -f Makefile; then $(MAKE) maintainer-clean; fi
	$(RM) include/libcharset.h include/localcharset.h include/localcharset.h.inst
	$(RM) config.status config.log config.cache Makefile config.h libtool

force :

