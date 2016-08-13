mandir = @mandir@/fr
htmldir = @htmldir@/fr

BUILT_SOURCES =

dist_man_MANS = nano.1 rnano.1
html_pages = nano.1.html rnano.1.html

if USE_NANORC
dist_man_MANS += nanorc.5
html_pages += nanorc.5.html
endif

nano.1.html: nano.1
	groff -t -mandoc -Thtml < $? > $@
nanorc.5.html: nanorc.5
	groff -t -mandoc -Thtml < $? > $@
rnano.1.html: rnano.1
	groff -t -mandoc -Thtml < $? > $@

if GROFF_HTML
BUILT_SOURCES += $(html_pages)
dist_html_DATA = $(html_pages)
endif

EXTRA_DIST = $(BUILT_SOURCES)
