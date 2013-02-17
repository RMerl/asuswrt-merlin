## Process this file with automake to produce Makefile.in


GENFILES_CLNT	= mount_clnt.c
GENFILES_XDR	= mount_xdr.c
GENFILES_H	= mount.h

GENFILES	= $(GENFILES_CLNT) $(GENFILES_SVC) $(GENFILES_XDR) $(GENFILES_H)

EXTRA_DIST	= mount.x

noinst_LIBRARIES = libexport.a
libexport_a_SOURCES = client.c export.c hostname.c nfsctl.c rmtab.c \
		      xtab.c mount_clnt.c mount_xdr.c
BUILT_SOURCES 	= $(GENFILES)

noinst_HEADERS = mount.h

dist-hook:
	for f in $(GENFILES); do \
	  rm ${distdir}/$$f; \
	done

if CONFIG_RPCGEN
RPCGEN		= $(top_builddir)/tools/rpcgen/rpcgen
$(RPCGEN):
	make -C $(top_srcdir)/tools/rpcgen all
else
RPCGEN = @RPCGEN_PATH@
endif

$(GENFILES_CLNT): %_clnt.c: %.x $(RPCGEN)
	test -f $@ && rm -rf $@ || true
	$(RPCGEN) -l -o $@ $<

$(GENFILES_XDR): %_xdr.c: %.x $(RPCGEN)
	test -f $@ && rm -rf $@ || true
	$(RPCGEN) -c -o $@ $<

$(GENFILES_H): %.h: %.x $(RPCGEN)
	test -f $@ && rm -rf $@ || true
	$(RPCGEN) -h -o $@ $<
	rm -f $(top_builddir)/support/include/mount.h
	$(LN_S) ../export/mount.h $(top_builddir)/support/include/mount.h

MAINTAINERCLEANFILES = Makefile.in

CLEANFILES = $(GENFILES) $(top_builddir)/support/include/mount.h
