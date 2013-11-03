XSLTPROC=@XSLTPROC@
XSLTPROC_FLAGS=@XSLTPROC_FLAGS@
MAN_STYLESHEET=$(top_srcdir)/doc/man.xsl
CLEANFILES =

MAN_MANPAGES = \
	afp.conf.5 \
	afp_signature.conf.5 \
	afp_voluuid.conf.5 \
	extmap.conf.5

EXTRA_DIST = \
	afp_signature.conf.5.xml \
	afp_voluuid.conf.5.xml \
	afp.conf.5.xml \
	extmap.conf.5.xml

if HAVE_XSLTPROC
CLEANFILES += $(MAN_MANPAGES)

%.5 : %.5.xml
	@xsltproc $(MAN_STYLESHEET) $<
	@cp $@ $(top_builddir)/man/man5/$@.in

html-local: $(MAN_MANPAGES) $(MAN_STYLESHEET)
endif