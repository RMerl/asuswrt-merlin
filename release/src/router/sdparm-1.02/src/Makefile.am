bin_PROGRAMS = sdparm

# INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/include/scsi

AM_CFLAGS = -I$(top_srcdir)/include -Wall -W
# AM_CFLAGS = -I$(top_srcdir)/include -Wall -W -pedantic -std=c99

sdparm_SOURCES =	sdparm.c	\
			sdparm.h	\
			sdparm_data.c	\
			sdparm_data_vendor.c	\
			sdparm_access.c	\
			sdparm_vpd.c	\
			sdparm_cmd.c	\
			../lib/sg_lib.c	\
			../include/sg_lib.h	\
			../lib/sg_cmds_basic.c	\
			../include/sg_cmds_basic.h	\
			../include/sg_pt.h

sdparm_LDADD = @os_deps@ @GETOPT_O_FILES@ @os_libs@
sdparm_DEPENDENCIES = @os_deps@ @GETOPT_O_FILES@
EXTRA_sdparm_SOURCES =	../lib/sg_pt_linux.c	\
			../include/sg_linux_inc.h	\
			../lib/sg_pt_freebsd.c	\
			../lib/sg_pt_osf1.c	\
			../lib/sg_pt_solaris.c	\
			../lib/sg_pt_win32.c	\
			../include/sg_pt_win32.h	\
			getopt_long.c	\
			port_getopt.h
