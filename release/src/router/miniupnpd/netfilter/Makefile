# $Id: Makefile,v 1.1 2007/10/06 11:36:22 nanard Exp $
CFLAGS=-Wall -g

all:	iptcrdr.o testiptcrdr

clean:
	$(RM) *.o testiptcrdr

testiptcrdr:	testiptcrdr.o iptcrdr.o -liptc

iptcrdr.o:	iptcrdr.c iptcrdr.h

