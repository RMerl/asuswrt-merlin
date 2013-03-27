# Makefile for testing lsof
#
# V. Abell
# Purdue University
#
# $Id: Makefile,v 1.17 2005/05/17 00:40:53 abe Exp abe $

DEBUG=
CFLAGS=	${DEBUG} -I. -I..

HDR=	LsofTest.h

CKTSTDB= CkTestDB
CONFCFL= ./config.cflags
CONFIG=	./config.cc ${CONFCFL} ./config.xobj
LTOBJ=	LTlib.o
LTSRC=	LTlib.c
LIBOBJ=	${LTOBJ}

BASTST=	LTbasic
STDTST=	LTnlink LTsock LTszoff LTunix
OPTTST=	LTbigf LTdnlc LTlock LTnfs 

all:	${CKTSTDB} ${BASTST} ${STDTST} FRC
	@./${CKTSTDB}; xv=$$?; \
	if [ $$xv -ne 0 ]; then \
	  exit 1 ;\
	fi
	@rm -f config.LT*
	-@err=0; \
	echo ""; \
	echo "Basic test:"; \
	./${BASTST}; \
	if [ $$? -ne 0 ]; then \
	  exit 1; \
	fi; \
	echo ""; \
	echo "Standard tests:"; \
	for i in ${STDTST}; do \
	  ./$$i; \
	  if [ $$? -ne 0 ]; then \
	    err=`expr $$err + 1`; \
	  fi; \
	done; \
	if [ $$err -ne 0 ]; then \
	  echo "Failed tests: $$err"; \
	  echo ""; \
	  echo "See 00FAQ and 00TEST for more information."; \
	else \
	  echo "All standard tests succeeded."; \
	  echo ""; \
	  grep LT_DIAL_darwin ${CONFCFL} > /dev/null 2>&1; \
	  if [ $$? -ne 0 ]; then \
	    echo "Suggestion: try the optional tests: \"make opt\""; \
	    echo ""; \
	  fi; \
	fi;
	@rm -f config.LT*

auto:	ckDB silent FRC

ckDB:	${CKTSTDB} FRC
	@echo "" | ./${CKTSTDB}; xv=$$?; \
	if [ $$xv -ne 0 ]; then \
	  exit 1 ;\
	fi

clean:	FRC
	rm -f ${BASTST} ${STDTST} ${OPTTST} *.o *.err *.out config.LT*

FRC:

LTbasic: LTbasic.c ${CONFIG} ${LIBOBJ} ${HDR}
	`cat config.cc` ${CFLAGS} `cat config.cflags` LTbasic.c \
	${LIBOBJ} `cat config.xobj` -o LTbasic

LTbigf: LTbigf.c ${CONFIG} ${LIBOBJ} ${HDR}
	`cat config.cc` ${CFLAGS} `cat config.cflags` LTbigf.c \
	${LIBOBJ} `cat config.xobj` -o LTbigf

LTdnlc: LTdnlc.c ${CONFIG} ${LIBOBJ} ${HDR}
	`cat config.cc` ${CFLAGS} `cat config.cflags` LTdnlc.c \
	${LIBOBJ} `cat config.xobj` -o LTdnlc

LTlock: LTlock.c ${CONFIG} ${LIBOBJ} ${HDR}
	`cat config.cc` ${CFLAGS} `cat config.cflags` LTlock.c \
	${LIBOBJ} `cat config.xobj` -o LTlock

${LTOBJ}: ${HDR} ${LTSRC} config.cflags config.cc
	`cat config.cc` ${CFLAGS} `cat config.cflags` -c ${LTSRC} \
	-o ${LTOBJ}

LTnfs:	LTnfs.c ${CONFIG} ${LIBOBJ} ${HDR}
	`cat config.cc` ${CFLAGS} `cat config.cflags` LTnfs.c \
	${LIBOBJ} `cat config.xobj` -o LTnfs

LTnlink: LTnlink.c ${CONFIG} ${LIBOBJ} ${HDR}
	`cat config.cc` ${CFLAGS} `cat config.cflags` LTnlink.c \
	${LIBOBJ} `cat config.xobj` -o LTnlink

LTsock: LTsock.c ${CONFIG} ${LIBOBJ} ${HDR}
	`cat config.cc` ${CFLAGS} `cat config.cflags` LTsock.c \
	${LIBOBJ} `cat config.xobj` -o LTsock `cat config.ldflags`

LTszoff: LTszoff.c ${CONFIG} ${LIBOBJ} ${HDR}
	`cat config.cc` ${CFLAGS} `cat config.cflags` LTszoff.c \
	${LIBOBJ} `cat config.xobj` -o LTszoff

LTunix: LTunix.c ${CONFIG} ${LIBOBJ} ${HDR} config.ldflags
	`cat config.cc` ${CFLAGS} `cat config.cflags` LTunix.c \
	${LIBOBJ} `cat config.xobj` -o LTunix `cat config.ldflags`

opt:	${CKTSTDB} ${OPTTST} FRC
	@rm -f config.LT*
	-@err=0; \
	echo ""; \
	echo "Optional tests:"; \
	for i in ${OPTTST}; do \
	  ./$$i; \
	  if [ $$? -ne 0 ]; then \
	    err=`expr $$err + 1`; \
	  fi; \
	done; \
	if [ $$err -ne 0 ]; then \
	  echo "Failed tests: $$err"; \
	else \
	  echo "All optional tests succeeded."; \
	fi; \
	echo "";
	@rm -f config.LT*

optional: opt

silent:	${BASTST} ${STDTST} FRC
	@rm -f config.LT*
	@err=0; \
	./${BASTST} > /dev/null 2>&1; \
	if [ $$? -ne 0 ]; then \
	  exit 1; \
	fi; \
	for i in ${STDTST}; do \
	  ./$$i > /dev/null 2>&1; \
	  if [ $$? -ne 0 ]; then \
	    err=`expr $$err + 1`; \
	  fi; \
	done; \
	rm -f config.LT*; \
	if [ $$err -ne 0 ]; then \
	  exit 1; \
	fi

spotless: clean
	rm -f config.*

standard: all

std: all

test: all
