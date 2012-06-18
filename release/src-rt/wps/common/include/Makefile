#
# include/Makefile
#
# Copyright 2005, Broadcom, Inc.
#
# $Id: Makefile 241182 2011-02-17 21:50:03Z $
#

SRCBASE	= ..

TARGETS	= wpsvers.h


all release:
	bash wpsvers.sh
	mv wpsvers.h wps_version.h

clean:
	rm -rf ${TARGETS} *.prev


.PHONY: all release clean
