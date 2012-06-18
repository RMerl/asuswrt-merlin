###########################################################################
# LPRng - An Extended Print Spooler System
#
# Copyright 1988-1997 Patrick Powell, San Diego, California
#     papowell@sdsu.edu
# See LICENSE for conditions of use.
#
###########################################################################
# MODULE: UTILS/Makefile
# PURPOSE: generate utilities
# Makefile.in,v 3.7 1998/03/30 01:54:50 papowell Exp
########################################################################## 

# Manual pages installation Makefile.
#
SRC = .
VPATH=${SRC}
INSTALL=/usr/bin/install -c

# change MANEXT to .gz or .Z if you use compressed manpages.
MANEXT =

## fix up prefix to be a make variable
prefix = /usr/local
MAN =		${prefix}/man
SHELL =		/bin/sh

# Install man pages in compressed form.
# COMPRESS     = compression utility
# COMPRESS_DIR = man compressed directory
# COMPRESS_EXT = man extension for compressed files
COMPRESS_DIR=
COMPRESS_EXT=.gz
COMPRESS=gzip -c

PAGES= cancel.1 checkpc.8 lp.1 lpbanner.1 lpc.8 lpd.8 lpd.conf.5 \
	 lpd.perms.5 lpf.1 lpq.1 lpr.1 lprm.1 lpstat.1 monitor.1 pclbanner.1 \
	 printcap.5 psbanner.1 lprng_certs.1 lprng_index_certs.1
RAW= cancel.n checkpc.n lp.n lpbanner.n lpc.n lpd.n lpd.conf.n \
	 lpd.perms.n lpf.n lpq.n lpr.n lprm.n lpstat.n monitor.n pclbanner.n \
	 printcap.n psbanner.n  lprng_certs.n lprng_index_certs.n


# prefix
prefix=/usr/local
# exec prefix
exec_prefix=${prefix}
# for binaries
bindir=${exec_prefix}/bin
# for admin type of binaries
sbindir=${exec_prefix}/sbin
# for the filters
libexecdir=${exec_prefix}/libexec
# for the configuration stuff
sysconfdir=${prefix}/etc
# for the man pages
mandir=${prefix}/man
# top build directory
top_builddir=..

LPD_PERMS_PATH=${sysconfdir}/lpd/lpd.perms
LPD_CONF_PATH=${sysconfdir}/lpd/lpd.conf
PRINTCAP_PATH=${sysconfdir}/printcap
LPD_PATH=${sbindir}/lpd
LOCKFILE=/var/run/lpd
SSL_CA_FILE=${sysconfdir}/lpd/ssl.ca/ca.crt
SSL_CA_KEY=${sysconfdir}/lpd/ssl.ca/ca.key
SSL_CERTS_DIR=${sysconfdir}/lpd/ssl.certs
SSL_SERVER_CERT=${sysconfdir}/lpd/ssl.server/server.crt
SSL_SERVER_PASSWORD=@SSL_SERVER_PASSWORD@

all: ${PAGES}

${PAGES}: ${RAW} Makefile
	for i in ${PAGES} ; do \
		v=`expr "$$i" : "\(.*\).[0-9]"` ; \
		echo ${SRC}/$$i; \
		rm -f $$i; \
		sed \
		 -e "s!_PRINTCAP_PATH_!${PRINTCAP_PATH}!" \
		 -e "s!_LPD_PERMS_PATH_!${LPD_PERMS_PATH}!" \
		 -e "s!_LPD_CONF_PATH_!${LPD_CONF_PATH}!" \
		 -e "s!_LOCKFILE_!${LOCKFILE}!" \
		 -e "s!_SSL_CA_FILE_!${SSL_CA_FILE}!" \
		 -e "s!_SSL_CA_KEY_!${SSL_CA_KEY}!" \
		 -e "s!_SSL_CERTS_DIR_!${SSL_CERTS_DIR}!" \
		 -e "s!_SSL_SERVER_CERT_!${SSL_SERVER_CERT}!" \
		 -e "s!_SSL_SERVER_PASSWORD_!${SSL_SERVER_PASSWORD}!" \
		${SRC}/$$v.n > $$i; \
	done



.PHONY: all clean install ci install.zman

CI=
CO=-kv
update:
	- for i in *.[0-9]; do \
		rcs -l ./$$i; \
		ci $(CI) -l -mUpdate -t-Initial ./$$i; \
	done;

	- VER=`cat ${SRC}/../VERSION`; \
	  for i in *.[0-9]; do \
		sed -e "s/^.ds VE.*/.ds VE $$VER/" ./$$i >/tmp/$$i; \
		cp /tmp/$$i ./$$i; \
	done;

	- for i in *.[0-9]; do \
		ci $(CI) -mUpdate -t-Initial ./$$i; \
		yes | co $(CO) ./$$i; \
	done

# Standard man install target.
#
install:
	for suffix in 1 2 3 4 5 6 7 8 ; do \
	  if ls *.$${suffix} >/dev/null 2>&1 ; then \
	    if [ ! -d $(DESTDIR)$(MAN)/man$${suffix} ] ; then \
		    echo ${SRC}/mkinstalldirs $(DESTDIR)$(MAN)/man$${suffix}; \
		    ${SRC}/mkinstalldirs $(DESTDIR)$(MAN)/man$${suffix}; \
	    fi; \
	    for i in *.$$suffix; do \
		echo   $(INSTALL) -m 644 $$i $(DESTDIR)$(MAN)/man$$suffix/`basename $$i`$(MANEXT); \
		$(INSTALL)   -m 644 $$i $(DESTDIR)$(MAN)/man$$suffix/`basename $$i`$(MANEXT); \
	    done; \
	  fi; \
	done;

uninstall:
	for suffix in 1 2 3 4 5 6 7 8 ; do \
	  if ls *.$${suffix} >/dev/null 2>&1 ; then \
	    for i in *.$$suffix; do \
		echo   rm $(DESTDIR)$(MAN)/man$$suffix/`basename $$i`$(MANEXT); \
		rm   $(DESTDIR)$(MAN)/man$$suffix/`basename $$i`$(MANEXT); \
	    done; \
	  fi; \
	done;
	
realclean mostlyclean distclean:: clean

clean::
	rm -f *.[0-9] ?

TAGS info:

cifast ci:
	if test ! -d RCS ; then mkdir RCS; fi;
	checkin() { \
		(set +e; rcs -l $$1; exit 0); \
		ci $(CI) -l -u -f -mUpdate -t-Initial $$1; \
	}; \
	for i in *.n Makefile.in Makefile; do \
		checkin $$i; \
	done;
