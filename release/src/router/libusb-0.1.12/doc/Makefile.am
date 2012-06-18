sgml_files = manual.sgml intro.sgml api.sgml functions.sgml examples.sgml

# For when we have a man page :)
man_MANS = 

EXTRA_DIST = manual.sgml api.sgml examples.sgml functions.sgml \
	intro.sgml website.dsl $(man_MANS)

# I grabbed this same hack from the VACM docs/Makfile.am
CLEANFILES = manual.dvi manual.aux manual.tex manual.log \
	manual.ps.gz; rm -rf html

if BUILD_DOCS
#MANUALS = manual.ps.gz html/index.html
MANUALS = html/index.html
# Generating postscript takes forever on my laptop apparentely
else
MANUALS =
endif

all: $(MANUALS)

manual.ps.gz: $(sgml_files) website.dsl
	@JADE@ -t ps -d $(srcdir)/website.dsl\#print $(srcdir)/manual.sgml
	gzip manual.ps

html/index.html: $(sgml_files) website.dsl
	rm -rf html
	mkdir html
	@JADE@ -t sgml -d $(srcdir)/website.dsl\#html $(srcdir)/manual.sgml

