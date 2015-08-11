# $Id: Makefile.am 1639 2007-08-27 00:55:17Z rpedde $
#
SUBDIRS = init.d

EXTRA_DIST = README mt-daapd.spec mt-daapd.conf.templ mt-daapd-ssc.sh mt-daapd-ssc.pl

BUILT_SOURCES = mt-daapd.conf
bin_SCRIPTS = mt-daapd-ssc.sh

dbdir="$(localstatedir)/cache/mt-daapd"

mt-daapd.conf: mt-daapd.conf.templ ../config.h
	sed -e s,[@]prefix[@],$(prefix), \
		-e s,[@]sysconfdir[@],$(sysconfdir), \
		-e s,[@]libdir[@],$(libdir), \
		-e s,[@]dbdir[@],$(dbdir),g mt-daapd.conf.templ >$@



