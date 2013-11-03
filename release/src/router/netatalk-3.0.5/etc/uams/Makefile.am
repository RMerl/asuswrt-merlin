# Makefile.am for etc/uams/

#
# conditionally build some modules
#

UAMS_GENERIC = uams_guest.la uams_passwd.la
uams_LINKS = uams_clrtxt.so
dhx_exec_hook = echo
dhx2_exec_hook = echo

# these are complex: check if DHX and DHX2 module should be build

if USE_DHX
if HAVE_OPENSSL
uams_LINKS += uams_dhx.so
UAMS_DHX = uams_dhx_passwd.la
if BUILD_PAM
UAMS_DHX_PAM = uams_dhx_pam.la
endif
if USE_PAM_SO
dhx_exec_hook += && $(LN_S) uams_dhx_pam.so uams_dhx.so
else
dhx_exec_hook += && $(LN_S) uams_dhx_passwd.so uams_dhx.so
endif

endif
endif

if USE_DHX2
if HAVE_LIBGCRYPT
uams_LINKS += uams_dhx2.so
UAMS_DHX2 = uams_dhx2_passwd.la
if BUILD_PAM
UAMS_DHX2_PAM = uams_dhx2_pam.la
endif
if USE_PAM_SO
dhx2_exec_hook += && $(LN_S) uams_dhx2_pam.so uams_dhx2.so
else
dhx2_exec_hook += && $(LN_S) uams_dhx2_passwd.so uams_dhx2.so
endif
endif
endif

# these are simple, though some the last three depend on OpenSSL

if BUILD_PAM
UAMS_PAM = uams_pam.la
endif

if USE_RANDNUM
UAMS_RANDNUM = uams_randnum.la
endif

if USE_PGP
UAMS_PGP = uams_pgp.la
endif

if USE_GSSAPI
UAMS_GSSAPI = uams_gss.la
endif

# links

if USE_PAM_SO
UAMS_CLRTXT_LINK = uams_pam.so
else
UAMS_CLRTXT_LINK = uams_passwd.so
endif

#
# source files
#

uams_guest_la_SOURCES      = uams_guest.c
uams_randnum_la_SOURCES    = uams_randnum.c
uams_passwd_la_SOURCES     = uams_passwd.c
uams_pam_la_SOURCES        = uams_pam.c
uams_pgp_la_SOURCES        = uams_pgp.c
uams_dhx_passwd_la_SOURCES = uams_dhx_passwd.c
uams_dhx_pam_la_SOURCES    = uams_dhx_pam.c
uams_dhx2_passwd_la_SOURCES	= uams_dhx2_passwd.c
uams_dhx2_pam_la_SOURCES	= uams_dhx2_pam.c
uams_gss_la_SOURCES   = uams_gss.c

#
# flags
#

# these should be sorted out, applying both to AM_CFLAGS is senseless
AM_CFLAGS = @SSL_CFLAGS@ @LIBGCRYPT_CFLAGS@

uams_pam_la_CFLAGS         = @PAM_CFLAGS@
uams_dhx_pam_la_CFLAGS     = @SSL_CFLAGS@ @PAM_CFLAGS@
uams_dhx2_pam_la_CFLAGS    = @LIBGCRYPT_CFLAGS@ @PAM_CFLAGS@
uams_gss_la_CFLAGS   	   = @GSSAPI_CFLAGS@

uams_guest_la_LDFLAGS      = -module -avoid-version
uams_randnum_la_LDFLAGS    = -module -avoid-version @SSL_LIBS@
uams_passwd_la_LDFLAGS     = -module -avoid-version @CRYPT_LIBS@
uams_pam_la_LDFLAGS        = -module -avoid-version @PAM_LIBS@
uams_pgp_la_LDFLAGS        = -module -avoid-version @SSL_LIBS@
uams_dhx_passwd_la_LDFLAGS = -module -avoid-version @CRYPT_LIBS@ @SSL_LIBS@
uams_dhx_pam_la_LDFLAGS		= -module -avoid-version @CRYPT_LIBS@ @SSL_LIBS@ @PAM_LIBS@
uams_dhx2_passwd_la_LDFLAGS	= -module -avoid-version @CRYPT_LIBS@ @LIBGCRYPT_LIBS@
uams_dhx2_pam_la_LDFLAGS	= -module -avoid-version @LIBGCRYPT_LIBS@ @PAM_LIBS@
uams_gss_la_LDFLAGS   	   = -module -avoid-version @GSSAPI_LIBS@ 

#
# module compilation
#

uamsdir = @UAMS_PATH@
uams_LTLIBRARIES =		\
	$(UAMS_GENERIC)		\
	$(UAMS_PGP)		\
	$(UAMS_PAM)		\
	$(UAMS_RANDNUM)		\
	$(UAMS_DHX)		\
	$(UAMS_DHX_PAM)		\
	$(UAMS_DHX2)		\
	$(UAMS_DHX2_PAM)	\
	$(UAMS_GSSAPI)

#
# link creation
#

install-data-hook:
	(if [ -d $(DESTDIR)$(uamsdir) ] ; then \
		cd $(DESTDIR)$(uamsdir)					&& \
		rm -f $(uams_LINKS)  					&& \
		$(LN_S) $(UAMS_CLRTXT_LINK) uams_clrtxt.so		&& \
		$(dhx_exec_hook)						&& \
		$(dhx2_exec_hook) ; 					\
	fi)



uninstall-hook:
	(if [ -d $(DESTDIR)$(uamsdir) ] ; then \
		cd $(DESTDIR)$(uamsdir)			&& \
		rm -f $(uams_LINKS)	;		   	\
	fi)
