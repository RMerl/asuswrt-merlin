# Makefile.am for libatalk/cnid/

libcnid_cdb_la_SOURCES = cnid_cdb_add.c \
		     cnid_cdb_close.c \
			 cnid_cdb_delete.c \
			 cnid_cdb_get.c \
			 cnid_cdb_lookup.c \
			 cnid_cdb_open.c \
			 cnid_cdb_resolve.c \
			 cnid_cdb_update.c \
			 cnid_cdb_rebuild_add.c \
			 cnid_cdb.h
libcnid_cdb_la_CFLAGS = @BDB_CFLAGS@
libcnid_cdb_la_LIBADD = @BDB_LIBS@ @PTHREAD_LIBS@

if USE_CDB_BACKEND
noinst_LTLIBRARIES = libcnid_cdb.la
endif

noinst_HEADERS = cnid_cdb_meta.h cnid_cdb_private.h 
EXTRA_DIST = README cnid_cdb_meta.c cnid_cdb_nextid.c
