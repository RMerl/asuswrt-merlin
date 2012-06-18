################################################
# Start MODULE ldb_ildap
[MODULE::ldb_ildap]
SUBSYSTEM = LIBLDB
CFLAGS = -I$(ldbsrcdir)/include
PRIVATE_DEPENDENCIES = LIBTALLOC LIBCLI_LDAP CREDENTIALS
INIT_FUNCTION = LDB_BACKEND(ldapi),LDB_BACKEND(ldaps),LDB_BACKEND(ldap)
ALIASES = ldapi ldaps ldap
# End MODULE ldb_ildap
################################################

ldb_ildap_OBJ_FILES = $(ldbsrcdir)/ldb_ildap/ldb_ildap.o

