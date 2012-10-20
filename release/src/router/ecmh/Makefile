# /**************************************
#  ecmh - Easy Cast du Multi Hub
#  by Jeroen Massar <jeroen@unfix.org>
# ***************************************
# $Author: fuzzel $
# $Id: Makefile,v 1.19 2005/02/09 17:58:52 fuzzel Exp $
# $Date: 2005/02/09 17:58:52 $
# **************************************/
#
# Toplevel Makefile allowing easy distribution.
# Use this makefile for doing almost anything
# 'make help' shows the possibilities
#

# The name of the application
ECMH=ecmh

# The version of this release
ECMH_VERSION=2005.02.09
export ECMH_VERSION

# ECMH Compile Time Options
# Append one of the following option on wish to
# include certain features, -O3 is the default
#
# Optimize             : -O3
# Enable Debugging     : -DDEBUG
# Enable IPv4 Support  : -DECMH_SUPPORT_IPV4
# Enable MLDv2 Support : -DECMH_SUPPORT_MLD2
# GetIfAddr Support    : -DECMH_GETIFADDR
# BPF Support (BSD)    : _DECMH_BPF
ECMH_OPTIONS=-DECMH_SUPPORT_MLD2 -DECMH_GETIFADDR

# Not Linux? -> Enable BPF Mode
ifeq ($(shell uname | grep -c "Linux"),0)
ECMH_OPTIONS:=$(ECMH_OPTIONS) ECMH_BPF
endif

# Export it to the other Makefile
export ECMH_OPTIONS

# Tag it with debug when it is run with debug set
ifeq ($(shell echo $(ECMH_OPTIONS) | grep -c "DEBUG"),1)
ECMH_VERSION:=$(ECMH_VERSION)-debug
endif

# Do not print "Entering directory ..."
MAKEFLAGS += --no-print-directory

# Misc bins, making it easy to quiet them :)
#CC=gcc
RM=@rm -f
MV=@mv
MAKE:=@${MAKE}
CP=@echo [Copy]; cp
RPMBUILD=@echo [RPMBUILD]; rpmbuild
RPMBUILD_SILENCE=>/dev/null 2>/dev/null

export CC
export MAKE
export RM

# Configure a default RPMDIR
ifeq ($(shell echo "${RPMDIR}/" | grep -c "/"),1)
RPMDIR=/usr/src/redhat/
endif

# Change this if you want to install into another dirtree
# Required for eg the Debian Package builder
DESTDIR=

# Get the source dir, needed for eg debsrc
SOURCEDIR := $(shell pwd)
SOURCEDIRNAME := $(shell basename `pwd`)

# Paths
sbindir=/usr/sbin/
srcdir=src/
toolsdir=tools/

#all:	ecmh tools
all:	ecmh

ecmh:	$(srcdir)
	$(MAKE) -C src all

tools:	$(toolsdir)
	$(MAKE) -C tools all

help:
	@echo "ecmh - Easy Cast du Multi Hub"
	@echo "Website: http://unfix.org/projects/ecmh/"
	@echo "Author : Jeroen Massar <jeroen@unfix.org>"
	@echo
	@echo "Makefile targets:"
	@echo "all      : Build everything"
	@echo "ecmh	: Build only ecmh"
	@echo "tools	: Build only the tools"
	@echo "help     : This little text"
	@echo "install  : Build & Install"
	@echo "clean    : Clean the dirs to be pristine in bondage"
	@echo
	@echo "Distribution targets:"
	@echo "dist     : Make all distribution targets"
	@echo "tar      : Make source tarball (tar.gz)"
	@echo "bz2      : Make source tarball (tar.bz2)"
	@echo "deb      : Make Debian binary package (.deb)"
	@echo "debsrc   : Make Debian source packages"
	@echo "rpm      : Make RPM package (.rpm)"
	@echo "rpmsrc   : Make RPM source packages"

install: all
	mkdir -p $(DESTDIR)${sbindir}
	${CP} src/$(ECMH) $(DESTDIR)${sbindir}

# Clean all the output files etc
distclean: clean

clean: debclean
	$(MAKE) -C src clean
	$(MAKE) -C tools clean

# Generate Distribution files
dist:	tar bz2 deb debsrc rpm rpmsrc

# tar.gz
tar:	clean
	-${RM} ../ecmh_${ECMH_VERSION}.tar.gz
	cd ..; tar -zclof ecmh_${ECMH_VERSION}.tar.gz ${SOURCEDIRNAME}

# tar.bz2
bz2:	clean
	-${RM} ../ecmh_${ECMH_VERSION}.tar.bz2
	cd ..; tar -jclof ecmh_${ECMH_VERSION}.tar.bz2 ${SOURCEDIRNAME}

# .deb
deb:	clean
	# Copy the changelog
	${CP} doc/changelog debian/changelog
	debian/rules binary
	${RM} debian/changelog
	${MAKE} clean

# Source .deb
debsrc: clean
	# Copy the changelog
	${CP} doc/changelog debian/changelog
	cd ..; dpkg-source -b ${SOURCEDIR}; cd ${SOURCEDIR}
	${RM} debian/changelog
	${MAKE} clean

# Cleanup after debian
debclean:
	if [ -f configure-stamp ]; then debian/rules clean; fi;

# RPM
rpm:	rpmclean tar
	-${RM} ${RPMDIR}/RPMS/i386/ecmh-${ECMH_VERSION}*.rpm
	${RPMBUILD} -tb --define 'ecmh_version ${ECMH_VERSION}' ../ecmh_${ECMH_VERSION}.tar.gz ${RPMBUILD_SILENCE}
	${MV} ${RPMDIR}/RPMS/i386/ecmh-*.rpm ../
	@echo "Resulting RPM's:"
	@ls -l ../ecmh-${ECMH_VERSION}*.rpm
	${MAKE} clean
	@echo "RPMBuild done"

rpmsrc:	rpmclean tar
	-${RM} ${RPMDIR}/RPMS/i386/ecmh-${ECMH_VERSION}*src.rpm
	${RPMBUILD} -ts --define 'ecmh_version ${PROJECT_VERSION}' ../ecmh_${ECMH_VERSION}.tar.gz ${RPMBUILD_SILENCE}
	${MV} ${RPMDIR}/RPMS/i386/ecmh-*.src.rpm ../
	@echo "Resulting RPM's:"
	@ls -l ../ecmh-${ECMH_VERSION}*.rpm
	${MAKE} clean}
	@echo "RPMBuild-src done"

rpmclean: clean
	-${RM} ../${PROJECT}_${PROJECT_VERSION}.tar.gz

# Mark targets as phony
.PHONY : all ecmh tools install help clean dist tar bz2 deb debsrc debclean rpm rpmsrc

