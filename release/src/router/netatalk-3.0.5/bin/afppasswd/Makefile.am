# Makefile.am for bin/afppasswd/

pkgconfdir = @PKGCONFDIR@

if HAVE_OPENSSL
bin_PROGRAMS = afppasswd
else
bin_PROGRAMS =
endif

afppasswd_SOURCES = afppasswd.c
afppasswd_LDADD = $(top_builddir)/libatalk/libatalk.la @SSL_LIBS@

AM_CFLAGS = @SSL_CFLAGS@ -I$(top_srcdir)/sys \
    -D_PATH_AFPDPWFILE=\"$(pkgconfdir)/afppasswd\"

install-exec-hook:
if HAVE_OPENSSL
	chmod u+s $(DESTDIR)$(bindir)/afppasswd
endif
