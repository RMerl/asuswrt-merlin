# Generated automatically from Makefile.in by configure.
###########################################################################
# LPRng - An Extended Print Spooler System
#
# Copyright 1988-1997 Patrick Powell, San Diego, California
#     papowell@sdsu.edu
# See LICENSE for conditions of use.
#
###########################################################################
# MODULE: DISTRIBUTIONS/Makefile
# PURPOSE: generate distribution package and checkout
# Makefile.in,v 3.7 1998/03/30 01:54:50 papowell Exp
########################################################################## 

all: ci

.PHONY: all clean install ci realclean mostlyclean distclean clean

CI=
CO=-kv

ci:
	checkin() { \
		(set +e; rcs -l $$1; exit 0); \
		ci $(CI) -l -u -f -mUpdate -t-Initial $$1; \
	}; \
	dirs=`find . -type d -name RCS -print | sed -e s/.RCS//`; \
	echo dirs $$dirs; \
	for i in $$dirs ; do \
		(cd $$i; echo CWD `pwd`; \
		for j in * ; do if [ -f $$j ] ; then checkin $$j; fi ; done; \
	); done;

realclean mostlyclean distclean clean:
	cd FreeBSD-4.ports.systutils.LPRng; make clean
