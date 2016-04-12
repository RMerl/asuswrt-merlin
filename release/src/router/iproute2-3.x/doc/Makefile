PSFILES=ip-cref.ps ip-tunnels.ps api-ip6-flowlabels.ps ss.ps nstat.ps arpd.ps rtstat.ps
# tc-cref.ps
# api-rtnl.tex api-pmtudisc.tex api-news.tex
# iki-netdev.ps iki-neighdst.ps


LATEX=latex
DVIPS=dvips
SGML2DVI=sgml2latex
SGML2HTML=sgml2html -s 0
LPR=lpr -Zsduplex
SHELL=bash
PAGESIZE=a4
PAGESPERPAGE=2

HTMLFILES=$(subst .sgml,.html,$(shell echo *.sgml))
DVIFILES=$(subst .ps,.dvi,$(PSFILES))
PDFFILES=$(subst .ps,.pdf,$(PSFILES))


all: pstwocol

pstwocol: $(PSFILES)

html: $(HTMLFILES)

dvi: $(DVIFILES)

pdf: $(PDFFILES)

print: $(PSFILES)
	$(LPR) $(PSFILES)

%.tex: %.sgml
	$(SGML2DVI) --output=tex $<

%.dvi: %.sgml
	$(SGML2DVI) --output=dvi $<

%.dvi: %.tex
	@set -e; pass=2; echo "Running LaTeX $<"; \
	while [ `$(LATEX) $< </dev/null 2>&1 | \
		 grep -c '^\(LaTeX Warning: Label(s) may\|No file \|! Emergency stop\)'` -ge 1 ]; do \
		if [ $$pass -gt 3 ]; then \
			echo "Seems, something is wrong. Try by hands." ; exit 1 ; \
		fi; \
		echo "Re-running LaTeX $<, $${pass}d pass"; pass=$$[$$pass + 1]; \
	done

%.pdf: %.tex
	@set -e; pass=2; echo "Running pdfLaTeX $<"; \
	while [ `pdflatex $< </dev/null 2>&1 | \
		 grep -c '^\(LaTeX Warning: Label(s) may\|No file \|! Emergency stop\)'` -ge 1 ]; do \
		if [ $$pass -gt 3 ]; then \
			echo "Seems, something is wrong. Try by hands." ; exit 1 ; \
		fi; \
		echo "Re-running pdfLaTeX $<, $${pass}d pass"; pass=$$[$$pass + 1]; \
	done
#%.pdf: %.ps
#	ps2pdf $<

%.ps: %.dvi
	$(DVIPS) $< -o $@

%.html: %.sgml
	$(SGML2HTML) $<

install:
	install -m 0644 $(shell echo *.tex) $(DESTDIR)$(DOCDIR)
	install -m 0644 $(shell echo *.sgml) $(DESTDIR)$(DOCDIR)

clean:
	rm -f *.aux *.log *.toc $(PSFILES) $(DVIFILES) *.html *.pdf
