SGMLFILES=$(shell echo *.sgml)
HTMLFILES=$(subst .sgml,.html,$(SGMLFILES)) index.html
MANFILES=$(subst .sgml,.8,$(SGMLFILES))

all: html

html: $(HTMLFILES) iputils.html

man: $(MANFILES)

# docbook scripts are incredibly dirty in the sense that they leak
# lots of some strange temporary junk directories and files.
# So, scope it to a temporary dir and clean all after each run.

$(HTMLFILES): index.db
	@-rm -rf tmp.db2html
	@mkdir tmp.db2html
	@set -e; cd tmp.db2html; docbook2html ../$< ; mv *.html ..
	@-rm -rf tmp.db2html

iputils.html: iputils.db
	@-rm -rf tmp.db2html
	@mkdir tmp.db2html
	@set -e; cd tmp.db2html; docbook2html -u -o html ../$< ; mv html/$@ ..
	@-rm -rf tmp.db2html

# docbook2man produces utterly ugly output and I did not find
# any way to customize this but hacking backend perl script a little.
# Well, hence...

$(MANFILES): index.db
	@-mkdir tmp.db2man
	@set -e; cd tmp.db2man; nsgmls ../$< | sgmlspl ../docbook2man-spec.pl ;	mv $@ ..
	@-rm -rf tmp.db2man

clean:
	@rm -rf $(MANFILES) $(HTMLFILES) iputils.html tmp.db2html tmp.db2man

snapshot:
	@date "+%y%m%d" > snapshot.db


$(MANFILES): $(SGMLFILES)

$(HTMLFILES): $(SGMLFILES)
