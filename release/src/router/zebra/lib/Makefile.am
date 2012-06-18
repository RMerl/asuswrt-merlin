## Process this file with automake to produce Makefile.in.

INCLUDES = @INCLUDES@ -I.. -I$(top_srcdir) -I$(top_srcdir)/lib
DEFS = @DEFS@ -DSYSCONFDIR=\"$(sysconfdir)/\"

noinst_LIBRARIES = libzebra.a

libzebra_a_SOURCES = \
	version.c network.c pid_output.c getopt.c getopt1.c daemon.c \
	print_version.c checksum.c vector.c linklist.c vty.c command.c \
	sockunion.c prefix.c thread.c if.c memory.c buffer.c table.c hash.c \
	filter.c routemap.c distribute.c stream.c str.c log.c plist.c \
	zclient.c sockopt.c smux.c md5.c keychain.c pqueue.c

libzebra_a_DEPENDENCIES = @LIB_REGEX@

libzebra_a_LIBADD = @LIB_REGEX@

noinst_HEADERS = \
	buffer.h command.h filter.h getopt.h hash.h if.h linklist.h log.h \
	memory.h network.h prefix.h routemap.h distribute.h sockunion.h \
	str.h stream.h table.h thread.h checksum.h vector.h version.h vty.h \
	zebra.h plist.h zclient.h sockopt.h smux.h md5-gnu.h keychain.h \
	pqueue.h

EXTRA_DIST = regex.c regex-gnu.h

version.c: Makefile
	echo '' >version.c
	echo 'char *host_name = "$(host_alias)";' >>version.c
