if WITH_MAN_PAGES

# decide which pages to install
man_MANS = madwimax.8

CLEANFILES = madwimax.8 madwimax.8.xml

madwimax.8.xml: $(srcdir)/madwimax.8.txt
	asciidoc -b docbook -d manpage -o $@ $<

madwimax.8: madwimax.8.xml
	$(db2xman) $<

endif

# distribute the source in asciidoc format
EXTRA_DIST = madwimax.8.txt
