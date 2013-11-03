# Makefile.am for libatalk/cnid/

if USE_TDB_BACKEND
noinst_LTLIBRARIES = libcnid_tdb.la
endif

libcnid_tdb_la_SOURCES = cnid_tdb_add.c \
		     cnid_tdb_close.c \
			 cnid_tdb_delete.c \
			 cnid_tdb_get.c \
			 cnid_tdb_lookup.c \
			 cnid_tdb_open.c \
			 cnid_tdb_resolve.c \
			 cnid_tdb_update.c \
			 cnid_tdb.h

EXTRA_DIST = README cnid_tdb_nextid.c
