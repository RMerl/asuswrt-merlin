include $(top_srcdir)/config/include-Makefile.am

EXTRA_DIST = uuidd.rc

bin_PROGRAMS =
sbin_PROGRAMS =
usrsbin_exec_PROGRAMS =

usrbin_exec_PROGRAMS = cal logger look mcookie namei whereis

logger_SOURCES = logger.c $(top_srcdir)/lib/strutils.c
mcookie_SOURCES = mcookie.c $(top_srcdir)/lib/md5.c

usrbin_exec_SCRIPTS = chkdupexe

CLEANFILES = chkdupexe

dist_man_MANS = cal.1 chkdupexe.1 logger.1 look.1 mcookie.1 \
		namei.1 whereis.1

namei_SOURCES = namei.c $(top_srcdir)/lib/strutils.c

if BUILD_DDATE
usrbin_exec_PROGRAMS += ddate
dist_man_MANS += ddate.1
endif

if BUILD_LSBLK
bin_PROGRAMS += lsblk
dist_man_MANS += lsblk.8
lsblk_SOURCES = lsblk.c \
		$(top_srcdir)/lib/canonicalize.c \
		$(top_srcdir)/lib/ismounted.c \
		$(top_srcdir)/lib/tt.c \
		$(top_srcdir)/lib/mbsalign.c \
		$(top_srcdir)/lib/strutils.c \
		$(top_srcdir)/lib/sysfs.c \
		$(top_srcdir)/lib/at.c
lsblk_LDADD = $(ul_libblkid_la)
lsblk_CFLAGS = $(AM_CFLAGS) -I$(ul_libblkid_incdir)
if HAVE_UDEV
lsblk_LDADD += -ludev
endif
endif

if BUILD_LIBUUID
usrbin_exec_PROGRAMS += uuidgen
dist_man_MANS += uuidgen.1
uuidgen_LDADD = $(ul_libuuid_la)
uuidgen_CFLAGS = $(AM_CFLAGS) -I$(ul_libuuid_srcdir)
endif

if BUILD_UUIDD
usrsbin_exec_PROGRAMS += uuidd
dist_man_MANS += uuidd.8
uuidd_LDADD = $(ul_libuuid_la)
uuidd_CFLAGS = $(AM_CFLAGS) -I$(ul_libuuid_srcdir)
endif

if BUILD_LIBBLKID
sbin_PROGRAMS += blkid findfs wipefs
dist_man_MANS += blkid.8 findfs.8 wipefs.8
blkid_SOURCES = blkid.c $(top_srcdir)/lib/ismounted.c \
		$(top_srcdir)/lib/strutils.c
blkid_LDADD = $(ul_libblkid_la)
blkid_CFLAGS = $(AM_CFLAGS) -I$(ul_libblkid_incdir)

findfs_LDADD = $(ul_libblkid_la)
findfs_CFLAGS = $(AM_CFLAGS) -I$(ul_libblkid_incdir)

wipefs_SOURCES = wipefs.c \
		 $(top_srcdir)/lib/strutils.c \
		 $(top_srcdir)/lib/match.c
wipefs_LDADD = $(ul_libblkid_la)
wipefs_CFLAGS = $(AM_CFLAGS) -I$(ul_libblkid_incdir)

if HAVE_STATIC_BLKID
sbin_PROGRAMS += blkid.static
blkid_static_SOURCES = $(blkid_SOURCES)
blkid_static_LDFLAGS = -all-static
blkid_static_LDADD = $(ul_libblkid_la)
blkid_static_CFLAGS = $(AM_CFLAGS) -I$(ul_libblkid_incdir)
endif
endif # BUILD_LIBBLKID

if BUILD_LIBMOUNT
bin_PROGRAMS += findmnt
dist_man_MANS += findmnt.8
findmnt_LDADD = $(ul_libmount_la)
findmnt_CFLAGS = $(AM_CFLAGS) -I$(ul_libmount_incdir)
findmnt_SOURCES =	findmnt.c \
			$(top_srcdir)/lib/tt.c \
			$(top_srcdir)/lib/mbsalign.c \
			$(top_srcdir)/lib/strutils.c

if !HAVE_LANGINFO
findmnt_SOURCES += $(top_srcdir)/lib/langinfo.c
endif
endif # BUILD_LIBMOUNT

cal_SOURCES = cal.c $(top_srcdir)/lib/mbsalign.c $(top_srcdir)/lib/strutils.c
if !HAVE_LANGINFO
cal_SOURCES += $(top_srcdir)/lib/langinfo.c
endif
if HAVE_TINFO
cal_LDADD = -ltinfo @NCURSES_LIBS@
else
if HAVE_NCURSES
cal_LDADD = @NCURSES_LIBS@
else
if HAVE_TERMCAP
cal_LDADD = -ltermcap
endif
endif
endif

if BUILD_KILL
bin_PROGRAMS += kill
kill_SOURCES = kill.c procs.c kill.h $(top_srcdir)/lib/strutils.c
dist_man_MANS += kill.1
endif

if BUILD_RENAME
usrbin_exec_PROGRAMS += rename
dist_man_MANS += rename.1
endif
