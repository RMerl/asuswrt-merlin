################################################
# Start SUBSYSTEM LIBLDB_CMDLINE
[SUBSYSTEM::LIBLDB_CMDLINE]
CFLAGS = -I$(ldbsrcdir) -I$(ldbsrcdir)/include
PUBLIC_DEPENDENCIES = LIBLDB LIBPOPT
PRIVATE_DEPENDENCIES = LIBSAMBA-UTIL POPT_SAMBA POPT_CREDENTIALS gensec
# End SUBSYSTEM LIBLDB_CMDLINE
################################################

LIBLDB_CMDLINE_OBJ_FILES = $(ldbsrcdir)/tools/cmdline.o

################################################
# Start BINARY ldbadd
[BINARY::ldbadd]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBLDB_CMDLINE LIBCLI_RESOLVE
# End BINARY ldbadd
################################################


ldbadd_OBJ_FILES = $(ldbsrcdir)/tools/ldbadd.o

MANPAGES += $(ldbsrcdir)/man/ldbadd.1

################################################
# Start BINARY ldbdel
[BINARY::ldbdel]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBLDB_CMDLINE
# End BINARY ldbdel
################################################

ldbdel_OBJ_FILES = $(ldbsrcdir)/tools/ldbdel.o

MANPAGES += $(ldbsrcdir)/man/ldbdel.1

################################################
# Start BINARY ldbmodify
[BINARY::ldbmodify]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBLDB_CMDLINE
# End BINARY ldbmodify
################################################

ldbmodify_OBJ_FILES = $(ldbsrcdir)/tools/ldbmodify.o
MANPAGES += $(ldbsrcdir)/man/ldbmodify.1

################################################
# Start BINARY ldbsearch
[BINARY::ldbsearch]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBLDB_CMDLINE 
# End BINARY ldbsearch
################################################

ldbsearch_OBJ_FILES = $(ldbsrcdir)/tools/ldbsearch.o

MANPAGES += $(ldbsrcdir)/man/ldbsearch.1

################################################
# Start BINARY ldbedit
[BINARY::ldbedit]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBLDB_CMDLINE
# End BINARY ldbedit
################################################

ldbedit_OBJ_FILES = $(ldbsrcdir)/tools/ldbedit.o

MANPAGES += $(ldbsrcdir)/man/ldbedit.1

################################################
# Start BINARY ldbrename
[BINARY::ldbrename]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBLDB_CMDLINE
# End BINARY ldbrename
################################################

ldbrename_OBJ_FILES = $(ldbsrcdir)/tools/ldbrename.o

MANPAGES += $(ldbsrcdir)/man/ldbrename.1


