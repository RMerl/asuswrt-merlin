## Process this file with automake to produce Makefile.in

man8_MANS	= mountd.man
EXTRA_DIST	= $(man8_MANS)

RPCPREFIX	= rpc.
KPREFIX		= @kprefix@
sbin_PROGRAMS	= mountd

mountd_SOURCES = mountd.c mount_dispatch.c auth.c rmtab.c cache.c \
		 svc_run.c fsloc.c mountd.h
mountd_LDADD = ../../support/export/libexport.a \
	       ../../support/nfs/libnfs.a \
	       ../../support/misc/libmisc.a \
	       $(LIBBSD) $(LIBWRAP) $(LIBNSL) $(LIBBLKID)
mountd_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS) \
		  -I$(top_builddir)/support/include \
		  -I$(top_srcdir)/support/export

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
install-man: install-man8 install-man-links
uninstall-man: uninstall-man8 uninstall-man-links

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

