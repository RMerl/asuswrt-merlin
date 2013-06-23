# -*- Makefile -*-

AM_CFLAGS        = ${regular_CFLAGS}
AM_CPPFLAGS      = ${regular_CPPFLAGS} -I${top_builddir}/include -I${top_srcdir}/include ${kinclude_CPPFLAGS}

xtables_multi_SOURCES  = xtables-multi.c iptables-xml.c
xtables_multi_CFLAGS   = ${AM_CFLAGS}
xtables_multi_LDADD    = ../extensions/libext.a
if ENABLE_STATIC
xtables_multi_CFLAGS  += -DALL_INCLUSIVE
endif
if ENABLE_IPV4
xtables_multi_SOURCES += iptables-save.c iptables-restore.c \
                         iptables-standalone.c iptables.c
xtables_multi_CFLAGS  += -DENABLE_IPV4
xtables_multi_LDADD   += ../libiptc/libip4tc.la ../extensions/libext4.a
endif
if ENABLE_IPV6
xtables_multi_SOURCES += ip6tables-save.c ip6tables-restore.c \
                          ip6tables-standalone.c ip6tables.c
xtables_multi_CFLAGS  += -DENABLE_IPV6
xtables_multi_LDADD   += ../libiptc/libip6tc.la ../extensions/libext6.a
endif
xtables_multi_SOURCES += xshared.c
xtables_multi_LDADD   += ../libxtables/libxtables.la -lm

sbin_PROGRAMS    = xtables-multi
man_MANS         = iptables.8 iptables-restore.8 iptables-save.8 \
                   iptables-xml.1 ip6tables.8 ip6tables-restore.8 \
                   ip6tables-save.8
CLEANFILES       = iptables.8 ip6tables.8

vx_bin_links   = iptables-xml
if ENABLE_IPV4
v4_sbin_links  = iptables iptables-restore iptables-save
endif
if ENABLE_IPV6
v6_sbin_links  = ip6tables ip6tables-restore ip6tables-save
endif

iptables.8: ${srcdir}/iptables.8.in ../extensions/matches4.man ../extensions/targets4.man
	${AM_VERBOSE_GEN} sed -e 's/@PACKAGE_AND_VERSION@/${PACKAGE} ${PACKAGE_VERSION}/g' -e '/@MATCH@/ r ../extensions/matches4.man' -e '/@TARGET@/ r ../extensions/targets4.man' $< >$@;

ip6tables.8: ${srcdir}/ip6tables.8.in ../extensions/matches6.man ../extensions/targets6.man
	${AM_VERBOSE_GEN} sed -e 's/@PACKAGE_AND_VERSION@/${PACKAGE} ${PACKAGE_VERSION}/g' -e '/@MATCH@/ r ../extensions/matches6.man' -e '/@TARGET@/ r ../extensions/targets6.man' $< >$@;

pkgconfig_DATA = xtables.pc

# Using if..fi avoids an ugly "error (ignored)" message :)
install-exec-hook:
	-if test -z "${DESTDIR}"; then /sbin/ldconfig; fi;
	${INSTALL} -dm0755 "${DESTDIR}${bindir}";
	for i in ${vx_bin_links}; do ${LN_S} -f "${sbindir}/xtables-multi" "${DESTDIR}${bindir}/$$i"; done;
	for i in ${v4_sbin_links}; do ${LN_S} -f xtables-multi "${DESTDIR}${sbindir}/$$i"; done;
	for i in ${v6_sbin_links}; do ${LN_S} -f xtables-multi "${DESTDIR}${sbindir}/$$i"; done;
