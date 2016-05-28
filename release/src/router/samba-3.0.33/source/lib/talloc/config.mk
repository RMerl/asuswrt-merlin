################################################
# Start LIBRARY LIBTALLOC
[LIBRARY::LIBTALLOC]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = talloc.o
MANPAGE = talloc.3
CFLAGS = -Ilib/talloc
PUBLIC_HEADERS = talloc.h
DESCRIPTION = A hierarchical pool based memory system with destructors
#
# End LIBRARY LIBTALLOC
################################################

[BINARY::TALLOC]
OBJ_FILES = testsuite.o
PRIVATE_DEPENDENCIES = LIBTALLOC
INSTALLDIR = TORTUREDIR/LOCAL
