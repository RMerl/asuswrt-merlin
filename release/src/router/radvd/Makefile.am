#
#
#   Authors:
#    Lars Fenneberg		<lf@elemental.net>	 
#
#   This software is Copyright 1996,1997 by the above mentioned author(s), 
#   All Rights Reserved.
#
#   The license which is distributed with this software in the file COPYRIGHT
#   applies to this software. If your distribution is missing this file, you
#   may request it from <pekkas@netcore.fi>.
#

AUTOMAKE_OPTIONS = 1.2 foreign

DEFS = \
	-DINET6=1 \
	-DLOG_FACILITY=$(LOG_FACILITY) \
	-DPATH_RADVD_CONF=\"$(PATH_RADVD_CONF)\" \
	-DPATH_RADVD_LOG=\"$(PATH_RADVD_LOG)\" \
	-DPATH_RADVD_PID=\"$(PATH_RADVD_PID)\" \
	-DVERSION=\"$(VERSION)\"

INCLUDES = \
	-I$(srcdir) \
	-I.

COMMON_SRC = \
	defaults.h \
	includes.h \
	log.c \
	log.h \
	pathnames.h \
	radvd.h \
	recv.c \
	socket.c \
	util.c

sbin_PROGRAMS = \
	radvd \
	radvdump

radvd_SOURCES = \
	$(COMMON_SRC) \
	device-common.c \
	gram.h \
	gram.y \
	interface.c \
	process.c \
	radvd.c \
	scanner.l \
	send.c \
	timer.c

EXTRA_radvd_SOURCES = \
	device-bsd44.c \
	device-linux.c \
	netlink.c \
	netlink.h \
	privsep-linux.c

radvd_LDADD = \
	@CONDITIONAL_SOURCES@

radvd_DEPENDENCIES = \
	@CONDITIONAL_SOURCES@

AM_YFLAGS = -d

AM_CFLAGS = \
	-Wall \
	-Wcast-align \
	-Wcast-qual \
	-Wpointer-arith \
	-Wmissing-declarations \
	-Wmissing-prototypes \
	-Wno-strict-aliasing \
	-Wstrict-prototypes

radvdump_SOURCES = \
	$(COMMON_SRC) \
	radvdump.c

man_MANS = \
	radvd.8 \
	radvd.conf.5 \
	radvdump.8

EXTRA_DIST = \
	CHANGES \
	COPYRIGHT \
	INTRO.html \
	VERSION \
	copyright.blurb \
	radvd.8.man \
	radvd.conf.5.man \
	radvd.conf.example \
	radvdump.8.man \
	redhat/radvd.conf.empty \
	redhat/radvd.init \
	redhat/radvd.spec \
	redhat/radvd.sysconfig \
	redhat/radvd-tmpfs.conf

CLEANFILES = \
	gram.c \
	gram.h \
	radvd.8 \
	radvd.conf.5 \
	radvdump.8 \
	scanner.c

SUFFIXES = .man

.man:
	$(AM_V_GEN)$(SED) \
		-e 's,@''VERSION''@,$(VERSION),g' \
		-e 's,@''sbindir''@,$(sbindir),g' \
		-e 's,@''PATH_RADVD_CONF''@,$(PATH_RADVD_CONF),g' \
		-e 's,@''PATH_RADVD_PID''@,$(PATH_RADVD_PID),g' \
		-e 's,@''PATH_RADVD_LOG''@,$(PATH_RADVD_LOG),g' \
		-e 's,@''LOG_FACILITY''@,$(LOG_FACILITY),g' \
		$< > $@

dist-hook:
	rm -f $(distdir)/gram.c
	rm -f $(distdir)/gram.h
	rm -f $(distdir)/scanner.c

scanner.o: gram.h

