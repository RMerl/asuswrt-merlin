MAKE = make
VERSION = @VERSION@
SRCDIR = .
CC = mips-linux-uclibc-gcc -mips32r2 -msoft-float
CFLAGS = -g -O2 -Wall -W -I/home/javi/vdsl/releasebsp/merge_dsl-n55u-c1/apps/public/email-3.1.3 -I/home/javi/vdsl/releasebsp/merge_dsl-n55u-c1/apps/public/email-3.1.3/src -I/home/javi/vdsl/releasebsp/merge_dsl-n55u-c1/apps/public/email-3.1.3/include -I../include -I/home/javi/vdsl/releasebsp/merge_dsl-n55u-c1/apps/public/email-3.1.3/dlib/include -I../dlib/include -DEMAIL_VERSION='"3.1.3"' -DCOMPILE_DATE='"10/31/2010-01:13:35PM_EDT"' -DEMAIL_DIR='"/etc/email"' -DHAVE_CONFIG_H
LDFLAGS = 
LIBS =   -lm 
DLIB = ../dlib/libdlib.a

prefix = /usr/local
bindir = ${exec_prefix}/bin
sbindir = ${exec_prefix}/sbin
mandir = ${datarootdir}/man
sysconfdir = /etc
datarootdir = ${prefix}/share

FILES = email.o addr_parse.o addy_book.o conf.o error.o execgpg.o file_io.o \
        message.o mimeutils.o processmail.o progress_bar.o \
	remotesmtp.o sig_file.o smtpcommands.o utils.o

all: $(FILES)
	$(CC) $(CFLAGS) -o email $(FILES) $(OTHER_FILES) $(DLIB) $(LDFLAGS) $(LIBS)

clean:
	rm -f *.o *.d email

clean-all:
	rm -rf Makefile *.o *.d email

