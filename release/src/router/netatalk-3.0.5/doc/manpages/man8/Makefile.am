XSLTPROC=@XSLTPROC@
XSLTPROC_FLAGS=@XSLTPROC_FLAGS@
MAN_STYLESHEET=$(top_srcdir)/doc/man.xsl
CLEANFILES =

MAN_MANPAGES = \
	afpd.8 \
	cnid_dbd.8 \
	cnid_metad.8 \
	netatalk.8

EXTRA_DIST = \
	afpd.8.xml \
	cnid_dbd.8.xml \
	cnid_metad.8.xml \
	netatalk.8.xml

if HAVE_XSLTPROC
CLEANFILES += $(MAN_MANPAGES)

%.8 : %.8.xml
	@xsltproc $(MAN_STYLESHEET) $<
	@cp $@ $(top_builddir)/man/man8/$@.in

html-local: $(MAN_MANPAGES) $(MAN_STYLESHEET)
endif