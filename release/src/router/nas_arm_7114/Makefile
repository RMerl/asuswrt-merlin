#
# 802.1x Linux Builds Makefile
#
# Copyright (C) 2002 Broadcom Corporation
#
# $Id: Makefile 271576 2011-07-12 20:31:22Z $
#
# targets/builds
BUILDS = nas

# default build
.PHONY: all
all : ${BUILDS}

.PHONY: ${BUILDS}
${BUILDS} ::
	+$(MAKE) -C $@

# clean up
BUILDS-CLEAN = ${BUILDS:=-clean}

.PHONY: clean
clean : ${BUILDS-CLEAN}

.PHONY: ${BUILDS-CLEAN}
${BUILDS-CLEAN} ::
	$(MAKE) -C ${@:-clean=} clean

# install
BUILDS-INSTALL = ${BUILDS:=-install}

.PHONY: install
install : ${BUILDS-INSTALL}

.PHONY: ${BUILDS-INSTALL}
${BUILDS-INSTALL} ::
	+$(MAKE) -C ${@:-install=} install
