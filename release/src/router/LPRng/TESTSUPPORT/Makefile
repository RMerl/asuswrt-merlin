# Makefile  for use with GNU Make
###########################################################################
# LPRng - An Extended Print Spooler System
#
# Copyright 1988-1995 Patrick Powell, San Diego State University
#     papowell@sdsu.edu
# See LICENSE for conditions of use.
#
###########################################################################
# MODULE: TESTSUPPORT/Makefile
# PURPOSE: install test version of LPD software
# Makefile.in,v 3.6 1998/03/30 02:15:10 papowell Exp
########################################################################## 
#
# make all - will make the binaries, install them in ${TMP}
# after this is run, set path to include ${TMP} as first entry
# try running the tests
#
# The HOST is the name of the local host, should be fully-qualified,
#   otherwise lpd.perms lookups won't work proper ly
# You can use checkpc -D1 to see what the fully qualified name is.
#
# The USER is the name of the local user
#   Enjoy.
#
# Patrick Powell (papowell@sdsu.edu)
#
# Mon Jul 10 07:13:35 PDT 1995
#
###########################################################################
#

#************** GENERAL CONFIGURATION INFORMATION ************************

CCOPTFLAGS =  -g # -Wall -O


#**************************** SYSTEM DEPENDENCIES **************************
#
# These are automatically sedded in by the "configure" script, but you
# may need to change them if you're on a strange version of UNIX.

# any libs that need to be used to get these programs to compile:
#
LIBS=
# LIBS=-lposix -s       # (on A/UX, if the above doesn't work)

# the compiler optimisation/debugging flags you wish to use.
#
# what C compiler to use.
#
CC=mipsel-linux-gcc

# what C preprocessor to use.
#
CPP=mipsel-linux-gcc -E $(CCOPTFLAGS)

# how to install stuff.
#
INSTALLCMD=/usr/bin/install -c

# a shell interpreter that's as close to the POSIX shell as possible.
#
SHELL_PATH=/bin/sh
SHELL = /bin/sh

#**********************  Helpful definitions *******************************
SRC=.

# CHANGE THESE!!
#HOST=taco.sdsu.edu
#USER=papowell
#
# Note: the following line is needed when creating binaries in a
#  different directory than the src directory;  see autoconf(1)
#  and the configure --src_dir option
#
BIN=../src

VPATH=.:..:${SRC}/.

CFLAGS = $(CCOPTFLAGS) -DHAVE_CONFIG_H -I. -I ${SRC} -I ..

TMP=/tmp/
CONFIG_FILE=${TMP}LPD/lpd.conf
COMMON_CONFIG=${TMP}LPD/common.conf
PRINTCAP=${TMP}LPD/printcap.${HOST}
LPD_PRINTCAP=${TMP}LPD/lpd_printcap.${HOST}
PRINTCAP_DB=${TMP}LPD/printcap.db.${HOST}
PERMS=${TMP}LPD/lpd.perms.${HOST}
PERMS_DB=${TMP}LPD/lpd.perms.db.${HOST}

install all: warn files bin perms pc
	@echo "set the LPD_CONF environment variable to " ${CONFIG_FILE}
	@echo "and put " ${TMP}LPD " in your path"


.PHONY: all warn files bin perms pc ci clean \
	uninstall realclean mostlyclean distclean \
	tags info

warn:
	@echo "HOST is " ${HOST} " and USER is " ${USER}
	@echo "HOST should be the fully qualified domain name"
	@echo "USER is the USERID under which the software runs"
	@if [ ! -n "$${USER}" ] ; then \
		echo "USER name missing"; \
		exit 1; \
	fi;
	@full=`echo $${HOST} | sed -n '/\./p'`; \
	if [ ! -n "$$full" ] ; then \
		echo "HOST name '$$HOST' not fully qualified"; \
		exit 1 ; \
	fi;

files:
	${SRC}/mkinstalldirs ${TMP}LPD
	cp -r ${SRC}/LPD/* ${TMP}LPD
	touch ${TMP}t1_lp ${TMP}t5_lp ${TMP}t6_lp

pc: $(CONFIG_FILE) $(COMMON_CONFIG) $(PRINTCAP) $(LPD_PRINTCAP) $(PRINTCAP_DB)

$(CONFIG_FILE): lpd.conf.proto
	DOMAIN=`echo ${HOST} | sed -e 's/\./ /' -e 's/.* //' `; \
	sed -e s/_DOMAIN_/$${DOMAIN}/g -e s/_HOST_/${HOST}/g -e "s:_TMP_:${TMP}:g" \
		 -e s/_USER_/${USER}/ \
		lpd.conf.proto >${CONFIG_FILE}
$(COMMON_CONFIG): common.conf.proto
	sed -e s/_HOST_/${HOST}/ -e 's/^#[0-$(LEVEL)]//' -e "s:_TMP_:${TMP}:g" \
		common.conf.proto  >$@
$(PRINTCAP): printcap.proto
	sed -e s/_HOST_/${HOST}/ -e 's/^#[0-$(LEVEL)]//' -e "s:_TMP_:${TMP}:g" \
		printcap.proto  >$@
$(LPD_PRINTCAP): lpd_printcap.proto
	sed -e s/_HOST_/${HOST}/ -e 's/^#[0-$(LEVEL)]//' -e "s:_TMP_:${TMP}:g" \
		lpd_printcap.proto  >$@
$(PRINTCAP_DB): printcap.proto
	sed -e s/_HOST_/${HOST}/ -e 's/^#[0-$(LEVEL)]//' -e "s:_TMP_:${TMP}:g" \
		printcap.proto | sh ${SRC}/makedb >$@

perms: $(PERMS) $(PERMS_DB)
$(PERMS): lpd.perms.proto
	sed -e s/_HOST_/${HOST}/ -e s/_USER_/${USER}/ lpd.perms.proto >$@
$(PERMS_DB): lpd.perms.proto
	sed -e s/_HOST_/${HOST}/ -e s/_USER_/${USER}/ lpd.perms.proto | sh ${SRC}/makedb >$@



bin: ${TMP}LPD/lpbanner \
	${TMP}LPD/lpf ${TMP}LPD/lpd ${TMP}LPD/lpc ${TMP}LPD/lpr \
	${TMP}LPD/lpq ${TMP}LPD/lprm ${TMP}LPD/checkpc \
	${TMP}LPD/filter ${TMP}LPD/dbserver ${TMP}LPD/clearlog \
	${TMP}LPD/bqfilter ${TMP}LPD/router

${TMP}LPD/lpbanner: $(BIN)/lpbanner
	$(INSTALLCMD) -m 755 $(BIN)/lpbanner ${TMP}LPD

${TMP}LPD/lpd: $(BIN)/lpd
	$(INSTALLCMD) -m 755 $(BIN)/lpd $@

${TMP}LPD/lpc: $(BIN)/lpc
	$(INSTALLCMD) -m 755 $(BIN)/lpc $@

${TMP}LPD/lpr: $(BIN)/lpr
	$(INSTALLCMD) -m 755 $(BIN)/lpr $@

${TMP}LPD/lpq: $(BIN)/lpq
	$(INSTALLCMD) -m 755 $(BIN)/lpq $@

${TMP}LPD/lprm: $(BIN)/lprm
	$(INSTALLCMD) -m 755 $(BIN)/lprm $@

${TMP}LPD/lpf: $(BIN)/lpf
	$(INSTALLCMD) -m 755 $(BIN)/lpf $@

${TMP}LPD/checkpc: $(BIN)/checkpc
	$(INSTALLCMD) -m 755 $(BIN)/checkpc $@

${TMP}LPD/filter: ${SRC}/filter.sh
	$(INSTALLCMD) -m 755 ${SRC}/filter.sh $@

${TMP}LPD/bqfilter: ${SRC}/bqfilter.sh
	$(INSTALLCMD) -m 755 ${SRC}/bqfilter.sh $@

${TMP}LPD/router: ${SRC}/router.sh
	$(INSTALLCMD) -m 755 ${SRC}/router.sh $@

${TMP}LPD/dbserver: ${SRC}/dbserver.sh
	$(INSTALLCMD) -m 755 ${SRC}/dbserver.sh $@

${TMP}LPD/clearlog: ${SRC}/clearlog
	$(INSTALLCMD) -m 755 ${SRC}/clearlog $@

CI=
CO=-kv
cifast ci:
	checkin() { \
		(set +e; rcs -l $$1; exit 0); \
		ci $(CI) -l -u -f -mUpdate -t-Initial $$1; \
	}; \
	for i in *; do \
		if test -f "$$i" ; then checkin $$i; fi; \
	done;

clean::
	-rm -f *.o core *.old

realclean mostlyclean distclean:: clean

UNINSTAll::
	-rm -rf ${TMP}LPD

TAGS info:
