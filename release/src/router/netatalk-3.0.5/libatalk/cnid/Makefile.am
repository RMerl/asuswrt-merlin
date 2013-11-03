# Makefile.am for libatalk/cnid/

SUBDIRS = last cdb dbd tdb

noinst_LTLIBRARIES = libcnid.la
LIBCNID_DEPS = dbd/libcnid_dbd.la

if USE_CDB_BACKEND
LIBCNID_DEPS += cdb/libcnid_cdb.la
endif

if USE_LAST_BACKEND
LIBCNID_DEPS += last/libcnid_last.la
endif

if USE_TDB_BACKEND
LIBCNID_DEPS += tdb/libcnid_tdb.la
endif

libcnid_la_SOURCES = cnid.c cnid_init.c
libcnid_la_LIBADD = $(LIBCNID_DEPS)

EXTRA_DIST = README

