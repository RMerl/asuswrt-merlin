include $(top_srcdir)/config/include-Makefile.am

EXTRA_DIST = README.clear README.col

usrbin_exec_PROGRAMS = col colcrt colrm column hexdump rev tailf

column_SOURCES = column.c $(top_srcdir)/lib/strutils.c

col_SOURCES = col.c $(top_srcdir)/lib/strutils.c

colrm_SOURCES = colrm.c $(top_srcdir)/lib/strutils.c

hexdump_SOURCES = hexdump.c conv.c display.c hexsyntax.c parse.c \
		  hexdump.h $(top_srcdir)/lib/strutils.c

tailf_SOURCES = tailf.c $(top_srcdir)/lib/strutils.c

dist_man_MANS = col.1 colcrt.1 colrm.1 column.1 hexdump.1 rev.1 tailf.1

if BUILD_LINE
usrbin_exec_PROGRAMS += line
dist_man_MANS += line.1
endif

if HAVE_NCURSES
bin_PROGRAMS = more
usrbin_exec_PROGRAMS += ul pg
if HAVE_TINFO
more_LDADD = -ltinfo
pg_LDADD = -ltinfo @NCURSES_LIBS@
ul_LDADD = -ltinfo
else
more_LDADD = @NCURSES_LIBS@
pg_LDADD = @NCURSES_LIBS@
ul_LDADD = @NCURSES_LIBS@
endif
dist_man_MANS += ul.1 more.1 pg.1
else
if HAVE_TERMCAP
bin_PROGRAMS = more
more_LDADD = -ltermcap
dist_man_MANS += more.1
endif
endif

EXTRA_DIST += README.pg LICENSE.pg
