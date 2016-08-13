info_TEXINFOS = nano.texi

MAKEINFO = makeinfo --no-split

BUILT_SOURCES = nano.html

nano.html: nano.texi
	makeinfo --no-split --html < $< > $@

EXTRA_DIST = $(info_TEXINFOS) $(BUILT_SOURCES)
