## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

SUBDIRS = tools support utils linux-nfs

MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = \
	autogen.sh \
	\
	debian/changelog \
	debian/control \
	debian/copyright \
	debian/etc.exports \
	debian/idmapd.conf \
	debian/nfs-common.conffiles \
	debian/nfs-common.default \
	debian/nfs-common.dirs \
	debian/nfs-common.files \
	debian/nfs-common.init \
	debian/nfs-common.install \
	debian/nfs-common.postinst \
	debian/nfs-common.postrm \
	debian/nfs-common.prerm \
	debian/nfs-kernel-server.NEWS \
	debian/nfs-kernel-server.conffiles \
	debian/nfs-kernel-server.default \
	debian/nfs-kernel-server.dirs \
	debian/nfs-kernel-server.init \
	debian/nfs-kernel-server.postinst \
	debian/nfs-kernel-server.postrm \
	debian/nfs-kernel-server.prerm \
	debian/nhfsstone.dirs \
	debian/nhfsstone.files \
	debian/nhfsstone.postinst \
	debian/nhfsstone.prerm \
	debian/rules \
	\
	aclocal/bsdsignals.m4 \
	aclocal/nfs-utils.m4 \
	aclocal/kerberos5.m4 \
	aclocal/tcp-wrappers.m4 \
	aclocal/libtirpc.m4 \
	aclocal/libevent.m4 \
	aclocal/libnfsidmap.m4 \
	aclocal/rpcsec_vers.m4 \
	aclocal/ipv6.m4

ACLOCAL_AMFLAGS = -I aclocal

install-data-hook:
	if [ ! -d $(DESTDIR)$(statedir) ]; then mkdir -p $(DESTDIR)$(statedir); fi
	touch $(DESTDIR)$(statedir)/xtab; chmod 644 $(DESTDIR)$(statedir)/xtab
	touch $(DESTDIR)$(statedir)/etab; chmod 644 $(DESTDIR)$(statedir)/etab
	touch $(DESTDIR)$(statedir)/rmtab; chmod 644 $(DESTDIR)$(statedir)/rmtab
	mkdir -p $(DESTDIR)$(statedir)/sm $(DESTDIR)$(statedir)/sm.bak
	touch $(DESTDIR)$(statedir)/state
	chmod go-rwx $(DESTDIR)$(statedir)/sm $(DESTDIR)$(statedir)/sm.bak $(DESTDIR)$(statedir)/state
	-chown $(statduser) $(DESTDIR)$(statedir)/sm $(DESTDIR)$(statedir)/sm.bak $(DESTDIR)$(statedir)/state

uninstall-hook:
	rm $(DESTDIR)$(statedir)/xtab
	rm $(DESTDIR)$(statedir)/etab
	rm $(DESTDIR)$(statedir)/rmtab
	rm $(DESTDIR)$(statedir)/state
