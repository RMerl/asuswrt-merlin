# Makefile.am for etc/netatlk/

pkgconfdir = @PKGCONFDIR@

sbin_PROGRAMS = netatalk

netatalk_SOURCES = netatalk.c
netatalk_CFLAGS = \
	-D_PATH_CONFDIR=\"$(pkgconfdir)/\" \
	-D_PATH_AFPD=\"$(sbindir)/afpd\" \
	-D_PATH_CNID_METAD=\"$(sbindir)/cnid_metad\"

netatalk_LDADD = \
	$(top_builddir)/libatalk/libatalk.la

netatalk_LDFLAGS =

if USE_BUILTIN_LIBEVENT
netatalk_CFLAGS += \
	-I$(top_srcdir)/libevent/include \
	-I$(top_builddir)/libevent/include

netatalk_LDADD += \
	$(top_builddir)/libevent/libevent.la
else
netatalk_CFLAGS += @LIBEVENT_CFLAGS@
netatalk_LDFLAGS += @LIBEVENT_LDFLAGS@ -levent
endif
