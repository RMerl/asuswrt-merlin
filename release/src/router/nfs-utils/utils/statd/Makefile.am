## Process this file with automake to produce Makefile.in

man8_MANS = statd.man sm-notify.man

GENFILES_CLNT	= sm_inter_clnt.c
GENFILES_SVC	= sm_inter_svc.c
GENFILES_XDR	= sm_inter_xdr.c
GENFILES_H	= sm_inter.h

GENFILES	= $(GENFILES_CLNT) $(GENFILES_SVC) $(GENFILES_XDR) $(GENFILES_H)

RPCPREFIX	= rpc.
KPREFIX		= @kprefix@
sbin_PROGRAMS	= statd sm-notify
dist_sbin_SCRIPTS	= start-statd
statd_SOURCES = callback.c notlist.c log.c misc.c monitor.c \
	        simu.c stat.c statd.c svc_run.c rmtcall.c \
	        sm_inter_clnt.c sm_inter_svc.c sm_inter_xdr.c log.h \
	        notlist.h statd.h system.h version.h sm_inter.h
sm_notify_SOURCES = sm-notify.c

BUILT_SOURCES = $(GENFILES)
statd_LDADD = ../../support/export/libexport.a \
	      ../../support/nfs/libnfs.a \
	      ../../support/misc/libmisc.a \
	      $(LIBWRAP) $(LIBNSL)
sm_notify_LDADD = $(LIBNSL)

EXTRA_DIST = sim_sm_inter.x sm_inter.x $(man8_MANS) COPYRIGHT simulate.c

if CONFIG_RPCGEN
RPCGEN	= $(top_builddir)/tools/rpcgen/rpcgen
$(RPCGEN):
	make -C ../../tools/rpcgen all
else
RPCGEN = @RPCGEN_PATH@
endif

$(GENFILES_CLNT): %_clnt.c: %.x $(RPCGEN)
	test -f $@ && rm -rf $@ || true
	$(RPCGEN) -l -o $@ $<

$(GENFILES_SVC): %_svc.c: %.x $(RPCGEN)
	test -f $@ && rm -rf $@ || true
	$(RPCGEN) -m -o $@ $<

$(GENFILES_XDR): %_xdr.c: %.x $(RPCGEN)
	test -f $@ && rm -rf $@ || true
	$(RPCGEN) -c -o $@ $<

$(GENFILES_H): %.h: %.x $(RPCGEN)
	test -f $@ && rm -rf $@ || true
	$(RPCGEN) -h -o $@ $<

MAINTAINERCLEANFILES = Makefile.in

CLEANFILES = $(GENFILES)

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
	    [ $$p = sm-notify ] || mv -f $$p$(EXEEXT) $(RPCPREFIX)$(KPREFIX)$$p$(EXEEXT) ;\
	  done)
uninstall-hook:
	(cd $(DESTDIR)$(sbindir) && \
	  for p in $(sbin_PROGRAMS); do \
	    [ $$p = sm-notify ] || rm -f $(RPCPREFIX)$(KPREFIX)$$p$(EXEEXT) ;\
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

