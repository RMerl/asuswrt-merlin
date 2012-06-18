## Process this file with Automake to create Makefile.in

INCLUDES = @INCLUDES@ -I.. -I$(top_srcdir) -I$(top_srcdir)/lib
DEFS = @DEFS@ -DSYSCONFDIR=\"$(sysconfdir)/\"

LIBS = @LIBS@ @CURSES@ @LIBPAM@

bin_PROGRAMS = vtysh

vtysh_SOURCES = vtysh_main.c vtysh.c vtysh_cmd.c vtysh_user.c vtysh_config.c
noinst_HEADERS = vtysh.h vtysh_user.h
vtysh_LDADD = ../lib/libzebra.a

sysconf_DATA = vtysh.conf.sample

EXTRA_DIST = extract.pl vtysh.conf.sample

rebuild4:
	./extract.pl ../zebra/*.c ../ripd/*.c ../ospfd/*.c ../bgpd/*.c ../lib/keychain.c ../lib/routemap.c ../lib/filter.c ../lib/plist.c ../lib/distribute.c > vtysh_cmd.c

rebuild:
	./extract.pl ../zebra/*.c ../ripd/*.c ../ripngd/*.c ../ospfd/*.c ../ospf6d/*.c ../bgpd/*.c ../lib/keychain.c ../lib/routemap.c ../lib/filter.c ../lib/plist.c ../lib/distribute.c > vtysh_cmd.c
