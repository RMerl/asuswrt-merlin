AUTOMAKE_OPTIONS = gnu 1.5
ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = config.rpath mkinstalldirs autogen.sh debian wol.spec

SUBDIRS = intl demo doc include lib src m4 po

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 config.h.in config.h.in~ \
config.log config.status configure depcomp install-sh ltconfig ltmain.sh \
missing stamp-h.in COPYING INSTALL \
mkinstalldirs config.sub config.guess \
po/Makefile.in po/ChangeLog po/wol.pot

dist-hook:
	rm -rf $(distdir)/debian/wol $(distdir)/debian/wol.substvars \
		$(distdir)/debian/CVS $(distdir)/debian/*~
