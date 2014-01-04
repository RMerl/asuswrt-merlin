# Makefile.am - rdnssd/ directory Makefile for ndisc6
# $Id: Makefile.am 640 2010-01-24 08:32:21Z remi $

# Copyright © 2006-2007 Rémi Denis-Courmont
# This file is distributed under the same license as the ndisc6 package.

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/include \
	-DLOCALEDIR=\"$(localedir)\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-DLOCALSTATEDIR=\"$(localstatedir)\"
confdir = $(sysconfdir)/rdnssd
myrundir = $(localstatedir)/run/rdnssd

subst_script = sed \
	-e 's,[@]PACKAGE[@],$(PACKAGE),g' \
	-e 's,[@]VERSION[@],$(VERSION),g' \
	-e 's,[@]LOCALSTATEDIR[@],$(localstatedir),g' \
	-e 's,[@]SYSCONFDIR[@],$(sysconfdir),g'

sed_verbose = $(sed_verbose_$(V))
sed_verbose_ = $(sed_verbose_$(AM_DEFAULT_VERBOSITY))
sed_verbose_0 = @echo "  SED    $@";

EXTRA_DIST = merge-hook.in
CLEANFILES = $(conf_SCRIPTS)
AM_INSTALLCHECK_STD_OPTIONS_EXEMPT = merge-hook
sbin_PROGRAMS = rdnssd
conf_SCRIPTS = merge-hook

# rdnssd
rdnssd_SOURCES = rdnssd.c rdnssd.h \
	icmp.c \
	netlink.c
rdnssd_LDADD = $(LIBRT) \
		@top_builddir@/compat/libcompat.a

# merge-hook
merge-hook: merge-hook.in Makefile.in $(top_builddir)/config.status
	$(AM_V_at)rm -f -- $@.tmp $@
	$(sed_verbose)$(subst_script) < $(srcdir)/$@.in > $@.tmp
	$(AM_V_at)chmod -- +x $@.tmp
	$(AM_V_at)mv -f -- $@.tmp $@

install-data-hook:
	$(install_sh) -d -- $(DESTDIR)$(myrundir)
	-chown -- nobody $(DESTDIR)$(myrundir)

uninstall-hook:
	-rmdir $(DESTDIR)$(myrundir)
