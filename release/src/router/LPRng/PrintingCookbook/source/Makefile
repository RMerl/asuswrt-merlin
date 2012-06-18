# Generated automatically from Makefile.in by configure.
###########################################################################
# LPRng - An Extended Print Spooler System
#
# Copyright 1988-1997 Patrick Powell, San Diego, California
#     papowell@sdsu.edu
# See LICENSE for conditions of use.
#
###########################################################################
# MODULE: FAQ/Makefile
# PURPOSE: generate utilities
# Makefile.in,v 1.7 1998/03/30 02:37:07 papowell Exp
########################################################################## 

# FAQ Makefile
#

DSSSL=docbook-1.54
DSSSL=modular

SRC=.

CATALOGS= \
      -c /usr/local/share/sgml/catalog \
      -c /usr/local/share/sgml/docbook/dsssl/${DSSSL}/catalog

DOC=Cookbook
DSL=-d $(DOC).dsl

FIGS= \
chooser.png clients.png configfiles.png filter.png network.png parallel.png pooling.png printserver.png samba.png serial.png spooler.png \
chooser.eps clients.eps configfiles.eps filter.eps network.eps parallel.eps pooling.eps printserver.eps samba.eps serial.eps spooler.eps \



all: figs $(DOC).html $(DOC).pdf purge # ${DOC}-Multipart/index.htm

#all: figs $(DOC).ps # ${DOC}-Multipart/index.htm purge

x_advanced.png:
	cd images; ${MAKE}
figs: $(FIGS) x_advanced.png
	touch figs

$(DOC).html: $(DOC).sgml $(DOC).dsl Makefile license.enc
	fix_html ${DOC}.sgml >${DOC}.htmp
	jade -i output.html -V nochunks -t sgml \
		$(CATALOGS) ${DSL} $(DOC).htmp >$(DOC).html
	-tidy -i -m -f /dev/null ${TIDYFLAGS} $@
	./updateheader $@

$(DOC).tex: $(DOC).sgml $(DOC).dsl Makefile
	fix_ps ${DOC}.sgml >${DOC}.htmp
	jade -Vtex-backend -ioutput.print -t tex -o $(DOC).tex \
		$(CATALOGS) ${DSL} $(DOC).htmp

$(DOC).dvi: $(DOC).tex $(DOC).dsl Makefile
	echo "==> TeX pass 1/3"
	-tex "&jadetex" $(DOC).tex  2>&1 >/dev/null
	echo "==> TeX pass 2/3"
	-tex "&jadetex" $(DOC).tex  2>&1 >/dev/null
	echo "==> TeX pass 3/3"
	tex "&jadetex" $(DOC).tex

$(DOC).ps: $(DOC).dvi
	dvips -q -t letter -o $(DOC).ps $(DOC).dvi
$(DOC).pdf: $(DOC).ps
	ps2pdf $(DOC).ps $(DOC).pdf

${DOC}-Multipart/index.htm: $(DOC).sgml $(DOC).dsl Makefile license.enc
	rm -rf ${DOC}-Multipart;
	if [ ! -d ${DOC}-Multipart ] ; then mkdir ${DOC}-Multipart; fi
	cp LPRng.jpg LPRngT-L.jpg ${DOC}-Multipart
	fix_html ${DOC}.sgml >${DOC}.htmp
	jade -i output.html -V html-manifest -t sgml \
		$(CATALOGS) ${DSL} $(DOC).htmp
	-tidy -i -m -f /dev/null ${TIDYFLAGS} `xargs < HTML.manifest`
	updateheader index.htm
	mv *.htm ${DOC}-Multipart
	cp *.png *.jpg ${DOC}-Multipart

purge:
	rm -f *.aux *.log *.toc *.fot *.dvi *.tex figs *.htmp *.fig


.PHONY: all clean install ci install.zman update purge

update:
	./updateversion *.sgml
	perl collateindex.pl -t "Index" -N -o genindex.sgml
	jade -V html-index -V nochunks -t sgml \
		$(CATALOGS) -d \
		/usr/local/share/sgml/docbook/dsssl/modular/html/docbook.dsl \
		$(DOC).sgml >/dev/null
	perl collateindex.pl -t "Index" -o genindex.sgml HTML.index

install:
	for i in HTML Multipart PDF ; do \
		rm -rf ../$$i; mkdir ../$$i; done;
	cp *.png *.jpg ../HTML/
	cp ${DOC}.html ../HTML/index.html
#	cp ${DOC}.ps ../PDF/
	cp ${DOC}.pdf ../PDF/
#	cp ${DOC}-Multipart/* ../Multipart

clean:
	-rm -f *.errs HTML.* *.tex *.log *.aux *.toc *.dvi *.ps *.tex *.fig
	-rm -f ${DOC}.html index.html *.htm */*.htm *.pdf *.ps *.bak
	-rm -rf ${DOC}-Multipart
	-rm -f x_* */x_*
	for i in *.pic; do s=`echo $$i | sed 's/\.pic/\.png/'`; rm -f $$s; done
	for i in *.pic; do s=`echo $$i | sed 's/\.pic/\.eps/'`; rm -f $$s; done

mostlyclean distclean:: clean purge
	-rm -f genindex.sgml
	-rm -f *.ps *.dvi *.htm *.html *.pdf *.eps
	

#CI=
#CO=-kv
CO=-l
cifast ci: clean
	checkin() { \
		(set +e; rcs -l $$1; exit 0); \
		ci $(CI) -l -u -f -mUpdate -t-Initial $$1; \
	}; \
	for i in  *.sgml Makefile.in Makefile; do \
		checkin $$i; \
	done;

correct:
	correct -d dict *.sgml

# convert PIC figures to eps and png
# Example:
# FIGS=existing.eps existing.png
# figs: ${FIGS}

.SUFFIXES: .pic .fig .eps .png

.pic.fig:
	pic2fig $<
	perl -spi.bak -e 'if( /\\\\/ ) { \
		s/\\\\fP//; \
		if( s/\\\\fB// ) { s/ 16 / 18 /; } \
		if( s/\\\\fI// ) { s/ 16 / 17 /; } \
		} ' $@
.fig.eps:
	fig2dev -Leps $< $@
.fig.png:
	fig2dev -Lpng $< $@
