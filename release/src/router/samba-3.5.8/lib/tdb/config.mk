################################################
# Start SUBSYSTEM LIBTDB
[LIBRARY::LIBTDB]
OUTPUT_TYPE = MERGED_OBJ
CFLAGS = -I$(tdbsrcdir)/include
#
# End SUBSYSTEM ldb
################################################

LIBTDB_OBJ_FILES = $(addprefix $(tdbsrcdir)/common/, \
	tdb.o dump.o io.o lock.o \
	open.o traverse.o freelist.o \
	error.o transaction.o check.o)

################################################
# Start BINARY tdbtool
[BINARY::tdbtool]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBTDB
# End BINARY tdbtool
################################################

tdbtool_OBJ_FILES = $(tdbsrcdir)/tools/tdbtool.o

################################################
# Start BINARY tdbtorture
[BINARY::tdbtorture]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBTDB
# End BINARY tdbtorture
################################################

tdbtorture_OBJ_FILES = $(tdbsrcdir)/tools/tdbtorture.o

################################################
# Start BINARY tdbdump
[BINARY::tdbdump]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBTDB
# End BINARY tdbdump
################################################

tdbdump_OBJ_FILES = $(tdbsrcdir)/tools/tdbdump.o

################################################
# Start BINARY tdbbackup
[BINARY::tdbbackup]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBTDB
# End BINARY tdbbackup
################################################

tdbbackup_OBJ_FILES = $(tdbsrcdir)/tools/tdbbackup.o
