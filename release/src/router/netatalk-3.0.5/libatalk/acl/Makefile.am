# Makefile.am for libatalk/acl/

noinst_HEADERS = aclldap.h cache.h

noinst_LTLIBRARIES = libacl.la
libacl_la_SOURCES = cache.c unix.c uuid.c
libacl_la_CFLAGS =
libacl_la_LDFLAGS =
libacl_la_LIBADD  = @ACL_LIBS@

if HAVE_LDAP
libacl_la_SOURCES += ldap.c ldap_config.c
libacl_la_CFLAGS  += @LDAP_CFLAGS@
libacl_la_LIBADD  += @LDAP_LIBS@
libacl_la_LDFLAGS += @LDAP_LDFLAGS@
endif

