# Makefile.am for libatalk/util/

pkgconfdir = @PKGCONFDIR@

noinst_LTLIBRARIES = libutil.la

libutil_la_SOURCES = \
	bprint.c	\
	cnid.c		\
	fault.c		\
	getiface.c	\
	gettok.c	\
	locking.c   \
	logger.c	\
	module.c	\
	netatalk_conf.c \
	queue.c     \
	server_child.c	\
	server_ipc.c	\
	server_lock.c	\
	socket.c        \
	strdicasecmp.c	\
	unix.c

libutil_la_CFLAGS = \
	-D_PATH_CONFDIR='"$(pkgconfdir)/"' \
	-DSERVERTEXT='"$(SERVERTEXT)/"' \
	-D_PATH_AFPDPWFILE='"$(pkgconfdir)/afppasswd"' \
	-D_PATH_AFPDUAMPATH='"$(UAMS_PATH)/"' \
	-D_PATH_STATEDIR='"$(localstatedir)/netatalk/"'

if HAVE_ATFUNCS
libutil_la_SOURCES += ftw.c
endif
