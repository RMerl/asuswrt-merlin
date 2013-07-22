include $(top_srcdir)/config/include-Makefile.am

srcs_common = $(top_srcdir)/lib/strutils.c $(top_srcdir)/lib/procutils.c

usrbin_exec_PROGRAMS = chrt
dist_man_MANS = chrt.1

chrt_SOURCES = chrt.c $(srcs_common)

if HAVE_IOPRIO_GET
if HAVE_IOPRIO_SET
usrbin_exec_PROGRAMS += ionice
ionice_SOURCES = ionice.c $(srcs_common)
dist_man_MANS += ionice.1
endif
endif

if HAVE_SCHED_GETAFFINITY
usrbin_exec_PROGRAMS += taskset
taskset_SOURCES = taskset.c $(top_srcdir)/lib/cpuset.c $(srcs_common)
dist_man_MANS += taskset.1
endif
