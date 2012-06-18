

if OS_LINUX

libsgutils_la_SOURCES = \
	sg_lib.c \
	sg_cmds_basic.c \
	sg_cmds_extra.c \
	sg_pt_linux.c

EXTRA_libsgutils_la_SOURCES = \
	sg_pt_freebsd.c \
	sg_pt_osf1.c \
	sg_pt_solaris.c \
	sg_pt_win32.c \
	getopt_long.c

endif


if OS_WIN32_MINGW

libsgutils_la_SOURCES = \
	sg_lib.c \
	sg_cmds_basic.c \
	sg_cmds_extra.c \
	sg_pt_win32.c

EXTRA_libsgutils_la_SOURCES = \
	sg_pt_linux.c \
	sg_pt_freebsd.c \
	sg_pt_osf1.c \
	sg_pt_solaris.c \
	getopt_long.c

endif


if OS_WIN32_CYGWIN

libsgutils_la_SOURCES = \
	sg_lib.c \
	sg_cmds_basic.c \
	sg_cmds_extra.c \
	sg_pt_win32.c

EXTRA_libsgutils_la_SOURCES = \
	sg_pt_linux.c \
	sg_pt_freebsd.c \
	sg_pt_osf1.c \
	sg_pt_solaris.c \
	getopt_long.c

endif


if OS_FREEBSD

libsgutils_la_SOURCES = \
	sg_lib.c \
	sg_cmds_basic.c \
	sg_cmds_extra.c \
	sg_pt_freebsd.c

EXTRA_libsgutils_la_SOURCES = \
	sg_pt_linux.c \
	sg_linux_inc.h \
	sg_pt_osf1.c \
	sg_pt_solaris.c \
	sg_pt_win32.c \
	getopt_long.c

endif


if OS_SOLARIS

libsgutils_la_SOURCES = \
	sg_lib.c \
	sg_cmds_basic.c \
	sg_cmds_extra.c \
	sg_pt_solaris.c

EXTRA_libsgutils_la_SOURCES = \
	sg_pt_linux.c \
	sg_linux_inc.h \
	sg_pt_freebsd.c \
	sg_pt_osf1.c \
	sg_pt_win32.c \
	getopt_long.c

endif


if OS_OSF

libsgutils_la_SOURCES = \
	sg_lib.c \
	sg_cmds_basic.c \
	sg_cmds_extra.c \
	sg_pt_osf1.c

EXTRA_libsgutils_la_SOURCES = \
	sg_pt_linux.c \
	sg_pt_freebsd.c \
	sg_pt_solaris.c \
	sg_pt_win32.c \
	getopt_long.c

endif


AM_CFLAGS = -I ../include -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -W
# AM_CFLAGS = -I ../include -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -W -pedantic -std=c99

lib_LTLIBRARIES = libsgutils.la

libsgutils_la_LDFLAGS = -version-info 1:0:0

libsgutils_la_LIBADD = @GETOPT_O_FILES@ @os_libs@
libsgutils_la_DEPENDENCIES = @GETOPT_O_FILES@


