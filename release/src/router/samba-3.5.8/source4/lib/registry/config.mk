[SUBSYSTEM::TDR_REGF]
PUBLIC_DEPENDENCIES = TDR 

TDR_REGF_OBJ_FILES = $(libregistrysrcdir)/tdr_regf.o

# Special support for external builddirs
$(libregistrysrcdir)/regf.c: $(libregistrysrcdir)/tdr_regf.c
$(libregistrysrcdir)/tdr_regf.h: $(libregistrysrcdir)/tdr_regf.c
$(libregistrysrcdir)/tdr_regf.c: $(libregistrysrcdir)/regf.idl
	@CPP="$(CPP)" $(PERL) $(pidldir)/pidl $(PIDL_ARGS) \
		--header --outputdir=$(libregistrysrcdir) \
		--tdr-parser -- $(libregistrysrcdir)/regf.idl

clean::
	@-rm -f $(libregistrysrcdir)/regf.h $(libregistrysrcdir)/tdr_regf*

################################################
# Start SUBSYSTEM registry
[LIBRARY::registry]
PUBLIC_DEPENDENCIES = \
		LIBSAMBA-UTIL CHARSET TDR_REGF LIBLDB \
		RPC_NDR_WINREG LDB_WRAP
# End MODULE registry_ldb
################################################

PC_FILES += $(libregistrysrcdir)/registry.pc

registry_VERSION = 0.0.1
registry_SOVERSION = 0

registry_OBJ_FILES = $(addprefix $(libregistrysrcdir)/, interface.o util.o samba.o \
					patchfile_dotreg.o patchfile_preg.o patchfile.o regf.o \
					hive.o local.o ldb.o dir.o rpc.o)

PUBLIC_HEADERS += $(libregistrysrcdir)/registry.h

[SUBSYSTEM::registry_common]
PUBLIC_DEPENDENCIES = registry

registry_common_OBJ_FILES = $(libregistrysrcdir)/tools/common.o

$(eval $(call proto_header_template,$(libregistrysrcdir)/tools/common.h,$(registry_common_OBJ_FILES:.o=.c)))

################################################
# Start BINARY regdiff
[BINARY::regdiff]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBSAMBA-HOSTCONFIG registry LIBPOPT POPT_SAMBA POPT_CREDENTIALS
# End BINARY regdiff
################################################

regdiff_OBJ_FILES = $(libregistrysrcdir)/tools/regdiff.o

MANPAGES += $(libregistrysrcdir)/man/regdiff.1

################################################
# Start BINARY regpatch
[BINARY::regpatch]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBSAMBA-HOSTCONFIG registry LIBPOPT POPT_SAMBA POPT_CREDENTIALS \
		registry_common
# End BINARY regpatch
################################################

regpatch_OBJ_FILES = $(libregistrysrcdir)/tools/regpatch.o

MANPAGES += $(libregistrysrcdir)/man/regpatch.1

################################################
# Start BINARY regshell
[BINARY::regshell]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBSAMBA-HOSTCONFIG LIBPOPT registry POPT_SAMBA POPT_CREDENTIALS \
		SMBREADLINE registry_common
# End BINARY regshell
################################################

regshell_OBJ_FILES = $(libregistrysrcdir)/tools/regshell.o

MANPAGES += $(libregistrysrcdir)/man/regshell.1

################################################
# Start BINARY regtree
[BINARY::regtree]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBSAMBA-HOSTCONFIG LIBPOPT registry POPT_SAMBA POPT_CREDENTIALS \
		registry_common
# End BINARY regtree
################################################

regtree_OBJ_FILES = $(libregistrysrcdir)/tools/regtree.o

MANPAGES += $(libregistrysrcdir)/man/regtree.1

[SUBSYSTEM::torture_registry]
PRIVATE_DEPENDENCIES = torture registry

torture_registry_OBJ_FILES = $(addprefix $(libregistrysrcdir)/tests/, generic.o hive.o diff.o registry.o)

$(eval $(call proto_header_template,$(libregistrysrcdir)/tests/proto.h,$(torture_registry_OBJ_FILES:.o=.c)))

[PYTHON::py_registry]
LIBRARY_REALNAME = samba/registry.$(SHLIBEXT)
PUBLIC_DEPENDENCIES = registry PYTALLOC pycredentials pyparam_util

py_registry_OBJ_FILES = $(libregistrysrcdir)/pyregistry.o
