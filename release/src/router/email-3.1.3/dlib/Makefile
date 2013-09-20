# Dlib makefile
MAKE = make
VERSION = @ver@
SRCDIR = src/
DESTDIR = 
CC = mips-linux-uclibc-gcc -mips32r2 -msoft-float
CFLAGS = -g -O2 -Wall -W -I/home/javi/vdsl/releasebsp/merge_dsl-n55u-c1/apps/public/email-3.1.3/dlib -I/home/javi/vdsl/releasebsp/merge_dsl-n55u-c1/apps/public/email-3.1.3/dlib/src -I/home/javi/vdsl/releasebsp/merge_dsl-n55u-c1/apps/public/email-3.1.3/dlib/include -I../include  -DHAVE_CONFIG_H
LDFLAGS = 
LIBS = -lnsl -lm 

prefix = /usr/local
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin
sbindir = ${exec_prefix}/sbin
mandir = ${datarootdir}/man
docdir = ${datarootdir}/doc/${PACKAGE_TARNAME}
sysconfdir = ${prefix}/etc
datarootdir = ${prefix}/share

bin_suffix = 

FILES = src/dnet.o src/dhash.o src/dlist.o src/dstrbuf.o src/dutil.o src/dvector.o

.PHONY: all clean-all clean distclean install uninstall

all: $(FILES)
	ar rc libdlib.a $(FILES)

test:
	$(CC) $(CFLAGS) -o test test.c libdlib.a -lm

dlisttest:
	$(CC) $(CFLAGS) -o dlisttest dlisttest.c libdlib.a -lm

dhashtest:
	$(CC) $(CFLAGS) -o dhashtest dhashtest.c libdlib.a -lm

clean:
	rm -f src/*.o *.so *.a test

