# $Id: Makefile,v 1.3 2012/09/20 12:46:01 nanard Exp $
CC=gcc
CFLAGS=-Wall -g -I.
RM=rm -f

all: testipfwrdr

clean:
	$(RM) *.o testipfwrdr

testipfwrdr: testipfwrdr.o ipfwrdr.o ipfwaux.o
	$(CC) -o $@ $^

ipfwrdr.o: ipfwrdr.c ipfwaux.c

testipfwrdr.o: testipfwrdr.c

