# Makefile.am - master Makefile for ndisc6
# $Id: Makefile.am 639 2010-01-24 08:27:53Z remi $

# Copyright © 2006 Rémi Denis-Courmont
# This file is distributed under the same license as the ndisc6 package.

ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = \
	1.11 \
	check-news \
	dist-bzip2 \
	no-dist-gzip \
	no-exeext \
	std-options \
	-Wall

SUBDIRS = po compat rdnssd
DIST_SUBDIRS = m4 $(SUBDIRS)
noinst_HEADERS = include/gettext.h

DISTCLEANFILES = stamp-svn

distcheck-hook:
	cd doc && $(MAKE) $(AM_MAKEFLAGS) distcheck-hook

distclean-local:
	test "$(top_srcdir)" = "$(top_builddir)" || rm -f ChangeLog

dist-hook: stamp-svn

stamp-svn:
	@rev=$$(LANG=C svnversion "$(srcdir)" 2>/dev/null || echo exported) ; \
	oldrev=$$(cat stamp-svn 2>/dev/null || true) ; \
	if test "$$rev" != "$$oldrev"; then \
		echo "New SVN revision is $$rev." ; \
		if test "$$rev" != "exported"; then \
			echo -n "Rebuilding ChangeLog... " ; \
			LANG=C svn -v --non-interactive log "$(srcdir)" > ChangeLog || exit $$? ; \
			echo "OK" ; \
		fi ; \
		echo "$$rev" > $@ ; \
	fi

ChangeLog:
	$(MAKE) $(AM_MAKEFLAGS) stamp-svn
	touch $@

.PHONY: stamp-svn
