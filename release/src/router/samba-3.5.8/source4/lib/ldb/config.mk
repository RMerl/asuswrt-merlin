################################################
# Start MODULE ldb_asq
[MODULE::ldb_asq]
PRIVATE_DEPENDENCIES = LIBTALLOC LIBTEVENT
CFLAGS = -I$(ldbsrcdir)/include
INIT_FUNCTION = LDB_MODULE(asq)
SUBSYSTEM = LIBLDB

ldb_asq_OBJ_FILES = $(ldbsrcdir)/modules/asq.o
# End MODULE ldb_asq
################################################

################################################
# Start MODULE ldb_server_sort
[MODULE::ldb_server_sort]
PRIVATE_DEPENDENCIES = LIBTALLOC LIBTEVENT
CFLAGS = -I$(ldbsrcdir)/include
INIT_FUNCTION = LDB_MODULE(server_sort)
SUBSYSTEM = LIBLDB

# End MODULE ldb_sort
################################################
ldb_server_sort_OBJ_FILES = $(ldbsrcdir)/modules/sort.o

################################################
# Start MODULE ldb_paged_results
[MODULE::ldb_paged_results]
INIT_FUNCTION = LDB_MODULE(paged_results)
CFLAGS = -I$(ldbsrcdir)/include
PRIVATE_DEPENDENCIES = LIBTALLOC LIBTEVENT
SUBSYSTEM = LIBLDB
# End MODULE ldb_paged_results
################################################

ldb_paged_results_OBJ_FILES = $(ldbsrcdir)/modules/paged_results.o

################################################
# Start MODULE ldb_paged_results
[MODULE::ldb_paged_searches]
INIT_FUNCTION = LDB_MODULE(paged_searches)
CFLAGS = -I$(ldbsrcdir)/include
PRIVATE_DEPENDENCIES = LIBTALLOC LIBTEVENT
SUBSYSTEM = LIBLDB
# End MODULE ldb_paged_results
################################################

ldb_paged_searches_OBJ_FILES = $(ldbsrcdir)/modules/paged_searches.o

################################################
# Start MODULE ldb_rdn_name
[MODULE::ldb_rdn_name]
SUBSYSTEM = LIBLDB
CFLAGS = -I$(ldbsrcdir)/include
PRIVATE_DEPENDENCIES = LIBTALLOC LIBTEVENT
INIT_FUNCTION = LDB_MODULE(rdn_name)
# End MODULE ldb_rdn_name
################################################

ldb_rdn_name_OBJ_FILES = $(ldbsrcdir)/modules/rdn_name.o

ldb_map_OBJ_FILES = $(addprefix $(ldbsrcdir)/ldb_map/, ldb_map_inbound.o ldb_map_outbound.o ldb_map.o)

$(ldb_map_OBJ_FILES): CFLAGS+=-I$(ldbsrcdir)/ldb_map

################################################
# Start MODULE ldb_skel
[MODULE::ldb_skel]
SUBSYSTEM = LIBLDB
CFLAGS = -I$(ldbsrcdir)/include
PRIVATE_DEPENDENCIES = LIBTALLOC LIBTEVENT
INIT_FUNCTION = LDB_MODULE(skel)
# End MODULE ldb_skel
################################################

ldb_skel_OBJ_FILES = $(ldbsrcdir)/modules/skel.o

################################################
# Start MODULE ldb_sqlite3
[MODULE::ldb_sqlite3]
SUBSYSTEM = LIBLDB
CFLAGS = -I$(ldbsrcdir)/include
PRIVATE_DEPENDENCIES = LIBTALLOC SQLITE3 LIBTEVENT
INIT_FUNCTION = LDB_BACKEND(sqlite3)
# End MODULE ldb_sqlite3
################################################

ldb_sqlite3_OBJ_FILES = $(ldbsrcdir)/ldb_sqlite3/ldb_sqlite3.o

################################################
# Start MODULE ldb_tdb
[MODULE::ldb_tdb]
SUBSYSTEM = LIBLDB
CFLAGS = -I$(ldbsrcdir)/include -I$(ldbsrcdir)/ldb_tdb
PRIVATE_DEPENDENCIES = \
		LIBTDB LIBTALLOC LIBTEVENT
INIT_FUNCTION = LDB_BACKEND(tdb)
# End MODULE ldb_tdb
################################################

ldb_tdb_OBJ_FILES = $(addprefix $(ldbsrcdir)/ldb_tdb/, ldb_tdb.o ldb_search.o ldb_pack.o ldb_index.o ldb_cache.o ldb_tdb_wrap.o)


################################################
# Start SUBSYSTEM ldb
[LIBRARY::LIBLDB]
CFLAGS = -I$(ldbsrcdir)/include
PUBLIC_DEPENDENCIES = \
		LIBTALLOC LIBTEVENT
PRIVATE_DEPENDENCIES = \
		SOCKET_WRAPPER

PC_FILES += $(ldbsrcdir)/ldb.pc
#
# End SUBSYSTEM ldb
################################################

LIBLDB_VERSION = 0.0.1
LIBLDB_SOVERSION = 0

LIBLDB_OBJ_FILES = $(addprefix $(ldbsrcdir)/common/, ldb.o ldb_ldif.o ldb_parse.o ldb_msg.o ldb_utf8.o ldb_debug.o ldb_modules.o ldb_match.o ldb_attributes.o attrib_handlers.o ldb_dn.o ldb_controls.o qsort.o) $(ldb_map_OBJ_FILES)

$(LIBLDB_OBJ_FILES): CFLAGS+=-I$(ldbsrcdir)/include

PUBLIC_HEADERS += $(ldbsrcdir)/include/ldb.h $(ldbsrcdir)/include/ldb_errors.h

MANPAGES += $(ldbsrcdir)/man/ldb.3

################################################
# Start BINARY ldbtest
[BINARY::ldbtest]
PRIVATE_DEPENDENCIES = \
		LIBLDB_CMDLINE
# End BINARY ldbtest
################################################

ldbtest_OBJ_FILES = $(ldbsrcdir)/tools/ldbtest.o

mkinclude tools/config.mk
mkinclude ldb_ildap/config.mk
