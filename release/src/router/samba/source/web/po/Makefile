GETTEXT = xgettext --keyword --keyword=_ --keyword=__

# Enter here all .po files
POFILES = ja.po
MOFILES = $(POFILES:.po=.mo)

SOURCES = ../cgi.c ../diagnose.c ../startstop.c ../statuspage.c ../swat.c

catalog: $(SOURCES)
	pofiles='$(POFILES)'; \
	for pof in $$pofiles; do \
		$(GETTEXT) $(SOURCES) ; \
		if [ -f $$pof ]; then \
			msgmerge $$pof messages.po > $$pof.new; \
		else \
			mv messages.po $$pof; \
		fi ; \
		rm -f messages.po ; \
	done

install: text
	mofiles='$(MOFILES)'; \
	for mof in $$mofiles; do \
		lang=`echo $$mof | sed 's/.mo$$//'`; \
		mkdir -p $(PREFIX)/usr/share/locale/$$lang/LC_MESSAGES; \
		cp -a $$mof  $(PREFIX)/usr/share/locale/$$lang/LC_MESSAGES/swat.mo; \
	done

text: $(POFILES)
	pofiles='$(POFILES)'; \
	for pof in $$pofiles; do \
		msgfmt  $$pof ; \
		lang=`echo $$pof | sed 's/.po$$//'`; \
		mv messages  $$lang.mo; \
	done

clean:
	rm -f $(MOFILES)
	rm -f *.new
	rm -f *~



