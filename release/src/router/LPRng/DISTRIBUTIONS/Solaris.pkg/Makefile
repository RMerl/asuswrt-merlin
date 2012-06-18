#
# Build a pkg for Solaris
# Makefile,v 1.3 2000/05/10 18:33:26 papowell Exp
#
# Hacked, mangled, twisted and warped by Patrick Powell <papowell@astart.com>
# rewritten by Willi Burmeister (wib@cs.uni-kiel.de) for LPRng
#
# 1. We make the package in WORKING
# 2. install it in /var/tmp/LPRng
# 3. generate a packing list using pkgproto + massage
#

PKG=LPRng
PKG_NAME='Line Printer Daemon'
VENDOR='Patrick Powell <papowell@astart.com>'

PREFIX=/usr
FILTERDIR=/usr/libexec/filters
SYSCONFDIR=/etc
SBINDIR=${PREFIX}/sbin
MANDIR=/usr/share/man
VERSION=3.8.22

CONFIGURE_ARGS=--prefix=${PREFIX} --sysconfdir=${SYSCONFDIR} --with-filterdir=${FILTERDIR} --mandir=${MANDIR}

WORKING?=../..
CONFIG?=../..
WORKING:=$(shell cd ${WORKING}; pwd)
CONFIG:=$(shell cd ${CONFIG}; pwd)
COPY_DIR=/var/tmp/LPRng

TMP=/tmp

.PHONY: all add remove rebuild clean check

all: ${PKG}.pkg

addit:
	pkgadd -d ${PKG}.pkg
removeit:
	pkgrm ${PKG}

${PKG}.pkg: pkginfo prototype
	pkgmk -o -d ${TMP} -f prototype -p `date +%Y%m%d%H%M`
	pkgtrans -s ${TMP} `pwd`/${PKG}.pkg ${PKG}
	rm -rf ${TMP}/${PKG}

pkginfo: Makefile
	echo CONFIG ${CONFIG} WORKING ${WORKING} COPY_DIR ${COPY_DIR}
	rm -f pkginfo
	echo "PKG=${PKG}" >> pkginfo
	echo "NAME=LPRng-${VERSION}" >> pkginfo
	echo "ARCH=`uname -p`" >> pkginfo
	echo "VERSION=$(VERSION)" >> pkginfo
	echo "CATEGORY=application" >> pkginfo
	echo "DESC=${PKG_NAME}" >> pkginfo
	echo "CLASSES=none preserve" >> pkginfo
	echo "VENDOR=${VENDOR}" >> pkginfo
	echo "BASEDIR=/" >> pkginfo

SCRIPTS:= postinstall preremove postremove

prototype: prototype.pre prototype.files ${COPY_DIR}${SBINDIR}/lpd ${SCRIPTS}
	cat prototype.pre prototype.files >prototype

${COPY_DIR}${SBINDIR}/lpd: ${WORKING}/config.h
	-rm -rf ${COPY_DIR}
	cd ${WORKING}; ${MAKE} install DESTDIR=${COPY_DIR} MAKEPACKAGE=YES

${WORKING}/config.h:
	if [ '!' -d ${WORKING} ] ; then mkdir ${WORKING} ; fi
	cd ${WORKING}; ${CONFIG}/configure ${CONFIGURE_ARGS}

${SCRIPTS}: prototype.files
	cp ${WORKING}/$@ $@

prototype.files: ${COPY_DIR}${SBINDIR}/lpd
	chown -R root ${COPY_DIR}
	chgrp -R bin ${COPY_DIR}
	pkgproto ${COPY_DIR}=/ \
	 | awk '$$3 == "/"{ next; } \
		/lpd.perms=/{ next; } \
		/lpd.conf=/{  next; } \
		/printcap=/{  next; } \
		/^d/{ print $$1,$$2,$$3," ? ? ? "; next; } \
		{ print; }' >prototype.files

rebuild:
	-rm -rf ${COPY_DIR}
	cd ${WORKING}; ${MAKE} clean

clean:
	-rm -f pkginfo prototype prototype.files ${PKG}.pkg \
		postinstall preremove postremove /tmp/LPRng
	-rm -rf ${COPY_DIR}
	if [ -d ${WORKING} ] ; then cd ${WORKING}; rm -f config.h; fi

checkit:
	cd ../..; ./configure ${CONFIGURE_ARGS}; ${MAKE} all install
	${MAKE} clean all
	-${MAKE} removeit
	${MAKE} addit removeit
