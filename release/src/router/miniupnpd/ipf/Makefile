# $Id: Makefile,v 1.2 2013/05/21 08:54:57 nanard Exp $
CC=gcc
CFLAGS=-Wall -g -I.
#CFLAGS=-Wall -g -I. -I../../../../ip_fil5.1.2/
#CFLAGS=-Wall -g -I. -I../../../../ip_fil4.1.35/
#CFLAGS=-Wall -g -I. -I../../../../ip_fil3.4.35/

all:	testipfrdr

clean:
	rm -f *.o testipfrdr

testipfrdr:	testipfrdr.o ipfrdr.o
	$(CC) -o $@ ${.ALLSRC}
#	$(CC) -o $@ $^

ipfrdr.o:	ipfrdr.c

testipfrdr.o:	testipfrdr.c

