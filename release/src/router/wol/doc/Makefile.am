info_TEXINFOS = wol.texi

if POD2TEXI
man_MANS = wol.1
else
man_MANS =
endif

EXTRA_DIST = texi2pod.pl $(man_MANS)
MAINTAINERCLEANFILES = Makefile.in texinfo.tex wol.1 wol.pod

wol.pod: wol.texi
	$(PERL) -w $(srcdir)/texi2pod.pl $? $@

wol.1: wol.pod
	pod2man --section=1 --center="wol" --release="wol @VERSION@" $? > $@

wol.pdf: wol.texi
	texi2pdf wol.texi

wol.html: wol.texi
	texi2html -no-section_navigation wol.texi

clean-local:
	rm -f wol.pdf wol.html wol.pod
