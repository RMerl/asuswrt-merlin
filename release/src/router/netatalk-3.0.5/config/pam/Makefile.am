## Makefile for distrib/pam/

SUFFIXES = .tmpl .
EXTRA_DIST = netatalk.tmpl
noinst_SCRIPTS = netatalk
CLEANFILES = netatalk

.tmpl:
	sed -e "s,[@]PAM_DIRECTIVE[@],${PAM_DIRECTIVE},g" \
		-e "s,[@]PAM_AUTH[@],${PAM_AUTH}," \
		-e "s,[@]PAM_ACCOUNT[@],${PAM_ACCOUNT}," \
		-e "s,[@]PAM_PASSWORD[@],${PAM_PASSWORD}," \
		-e "s,[@]PAM_SESSION[@],${PAM_SESSION}," \
	    <$< >$@

if USE_PAM
pamdir = $(PAMDIR)
pam_DATA = netatalk
endif
