## Process this file with automake to produce Makefile.in

man5_MANS	= idmapd.conf.man
man8_MANS	= idmapd.man

RPCPREFIX	= rpc.
KPREFIX		= @kprefix@
sbin_PROGRAMS	= idmapd

EXTRA_DIST = \
	$(man5_MANS) \
	$(man8_MANS) \
	idmapd.conf

idmapd_SOURCES = \
	atomicio.c \
	cfg.c \
	idmapd.c \
	strlcat.c \
	strlcpy.c \
	\
	cfg.h \
	nfs_idmap.h \
	queue.h

idmapd_LDADD = -levent -lnfsidmap ../../support/nfs/libnfs.a

MAINTAINERCLEANFILES = Makefile.in

#######################################################################
# The following allows the current practice of having
# daemons renamed during the install to include RPCPREFIX
# and the KPREFIX
# This could all be done much easier with program_transform_name
# ( program_transform_name = s/^/$(RPCPREFIX)$(KPREFIX)/ )
# but that also renames the man pages, which the current
# practice does not do.
install-exec-hook:
	(cd $(DESTDIR)$(sbindir) && \
	  for p in $(sbin_PROGRAMS); do \
	    mv -f $$p$(EXEEXT) $(RPCPREFIX)$(KPREFIX)$$p$(EXEEXT) ;\
	  done)
uninstall-hook:
	(cd $(DESTDIR)$(sbindir) && \
	  for p in $(sbin_PROGRAMS); do \
	    rm -f $(RPCPREFIX)$(KPREFIX)$$p$(EXEEXT) ;\
	  done)


# XXX This makes some assumptions about what automake does.
# XXX But there is no install-man-hook or install-man-local.
install-man: install-man5 install-man8 install-man-links
uninstall-man: uninstall-man5 uninstall-man8 uninstall-man-links

install-man-links:
	(cd $(DESTDIR)$(man8dir) && \
	  for m in $(man8_MANS) $(dist_man8_MANS) $(nodist_man8_MANS); do \
	    inst=`echo $$m | sed -e 's/man$$/8/'`; \
	    rm -f $(RPCPREFIX)$$inst ; \
	    $(LN_S) $$inst $(RPCPREFIX)$$inst ; \
	  done)

uninstall-man-links:
	(cd $(DESTDIR)$(man8dir) && \
	  for m in $(man8_MANS) $(dist_man8_MANS) $(nodist_man8_MANS); do \
	    inst=`echo $$m | sed -e 's/man$$/8/'`; \
	    rm -f $(RPCPREFIX)$$inst ; \
	  done)

