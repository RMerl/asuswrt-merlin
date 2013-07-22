include $(top_srcdir)/config/include-Makefile.am

bin_PROGRAMS =
usrbin_exec_PROGRAMS = script scriptreplay
sbin_PROGRAMS =
usrsbin_exec_PROGRAMS =

dist_man_MANS = script.1 scriptreplay.1

EXTRA_DIST = README.modems-with-agetty

script_LDADD =

if HAVE_UTIL
script_LDADD += -lutil
endif

if HAVE_UTEMPTER
script_LDADD += -lutempter
endif

if BUILD_AGETTY
sbin_PROGRAMS += agetty
dist_man_MANS += agetty.8
agetty_SOURCES = agetty.c
if !HAVE_LANGINFO
agetty_SOURCES += $(top_srcdir)/lib/langinfo.c
endif
endif

if HAVE_NCURSES
if LINUX
usrbin_exec_PROGRAMS += setterm
dist_man_MANS += setterm.1
endif
if HAVE_TINFO
setterm_LDADD = -ltinfo
else
setterm_LDADD = @NCURSES_LIBS@
endif
endif

if BUILD_RESET
dist_usrbin_exec_SCRIPTS = reset
dist_man_MANS += reset.1
endif
EXTRA_DIST += reset.033c

if BUILD_MESG
usrbin_exec_PROGRAMS += mesg
dist_man_MANS += mesg.1
endif

if BUILD_WALL
usrbin_exec_PROGRAMS += wall
wall_SOURCES = wall.c ttymsg.c ttymsg.h $(top_srcdir)/lib/strutils.c
dist_man_MANS += wall.1
wall_CFLAGS = $(SUID_CFLAGS) $(AM_CFLAGS)
wall_LDFLAGS = $(SUID_LDFLAGS) $(AM_LDFLAGS)
endif

if BUILD_WRITE
usrbin_exec_PROGRAMS += write
dist_man_MANS += write.1
write_CFLAGS = $(SUID_CFLAGS) $(AM_CFLAGS)
write_LDFLAGS = $(SUID_LDFLAGS) $(AM_LDFLAGS)
endif

if USE_TTY_GROUP
if MAKEINSTALL_DO_CHOWN
install-exec-hook::
if BUILD_WALL
	chgrp tty $(DESTDIR)$(usrbin_execdir)/wall
	chmod g+s $(DESTDIR)$(usrbin_execdir)/wall
endif
if BUILD_WRITE
	chgrp tty $(DESTDIR)$(usrbin_execdir)/write
	chmod g+s $(DESTDIR)$(usrbin_execdir)/write
endif
endif
endif
