################################################
# Start SUBSYSTEM LIBTDB
[LIBRARY::LIBTDB]
VERSION = 0.0.1
SO_VERSION = 0
DESCRIPTION = Trivial Database Library
OBJ_FILES = \
	common/tdb.o common/dump.o common/io.o common/lock.o \
	common/open.o common/traverse.o common/freelist.o \
	common/error.o common/transaction.o common/tdbutil.o
CFLAGS = -Ilib/tdb/include
PUBLIC_HEADERS = include/tdb.h
#
# End SUBSYSTEM ldb
################################################

################################################
# Start BINARY tdbtool
[BINARY::tdbtool]
INSTALLDIR = BINDIR
ENABLE = NO
OBJ_FILES= \
		tools/tdbtool.o
PRIVATE_DEPENDENCIES = \
		LIBTDB
# End BINARY tdbtool
################################################

################################################
# Start BINARY tdbtorture
[BINARY::tdbtorture]
INSTALLDIR = BINDIR
OBJ_FILES= \
		tools/tdbtorture.o
PRIVATE_DEPENDENCIES = \
		LIBTDB
# End BINARY tdbtorture
################################################

################################################
# Start BINARY tdbdump
[BINARY::tdbdump]
INSTALLDIR = BINDIR
OBJ_FILES= \
		tools/tdbdump.o
PRIVATE_DEPENDENCIES = \
		LIBTDB
# End BINARY tdbdump
################################################

################################################
# Start BINARY tdbbackup
[BINARY::tdbbackup]
INSTALLDIR = BINDIR
ENABLE = NO
OBJ_FILES= \
		tools/tdbbackup.o
PRIVATE_DEPENDENCIES = \
		LIBTDB
# End BINARY tdbbackup
################################################

#######################
# Start LIBRARY swig_tdb
[LIBRARY::swig_tdb]
LIBRARY_REALNAME = swig/_tdb.$(SHLIBEXT)
OBJ_FILES = swig/tdb_wrap.o
PUBLIC_DEPENDENCIES = LIBTDB DYNCONFIG
# End LIBRARY swig_tdb
#######################
