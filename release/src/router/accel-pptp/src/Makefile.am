
# set the include path found by configure
INCLUDES = $(all_includes)

# the library search path.
lib_LTLIBRARIES = pptp.la
pptp_la_LDFLAGS = -module -avoid-version
pptp_la_SOURCES = pptp.c pptp_callmgr.c pptp_ctrl.c util.c vector.c \
	dirutil.c pptp_quirks.c orckit_quirks.c inststr.c
noinst_HEADERS = pptp_callmgr.h pptp_ctrl.h util.h vector.h pptp_options.h \
		dirutil.h pptp_quirks.h orckit_quirks.h inststr.h
AM_CFLAGS = -Wall
man_MANS = pppd-pptp.8
