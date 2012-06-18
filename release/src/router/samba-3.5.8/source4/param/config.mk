[LIBRARY::LIBSAMBA-HOSTCONFIG]
PUBLIC_DEPENDENCIES = LIBSAMBA-UTIL 
PRIVATE_DEPENDENCIES = DYNCONFIG LIBREPLACE_EXT CHARSET

LIBSAMBA-HOSTCONFIG_VERSION = 0.0.1
LIBSAMBA-HOSTCONFIG_SOVERSION = 0

LIBSAMBA-HOSTCONFIG_OBJ_FILES = $(addprefix $(paramsrcdir)/,  \
			loadparm.o generic.o util.o) 

PUBLIC_HEADERS += param/param.h

PC_FILES += $(paramsrcdir)/samba-hostconfig.pc

[SUBSYSTEM::PROVISION]
PRIVATE_DEPENDENCIES = LIBPYTHON pyldb pyparam_util

PROVISION_OBJ_FILES = $(paramsrcdir)/provision.o $(param_OBJ_FILES)

#################################
# Start SUBSYSTEM share
[SUBSYSTEM::share]
PRIVATE_DEPENDENCIES = LIBSAMBA-UTIL
# End SUBSYSTEM share
#################################

share_OBJ_FILES = $(paramsrcdir)/share.o

$(eval $(call proto_header_template,$(paramsrcdir)/share_proto.h,$(share_OBJ_FILES:.o=.c)))

PUBLIC_HEADERS += param/share.h

################################################
# Start MODULE share_classic
[MODULE::share_classic]
SUBSYSTEM = share
INIT_FUNCTION = share_classic_init
PRIVATE_DEPENDENCIES = LIBSAMBA-UTIL
# End MODULE share_classic
################################################

share_classic_OBJ_FILES = $(paramsrcdir)/share_classic.o 

################################################
# Start MODULE share_ldb
[MODULE::share_ldb]
SUBSYSTEM = share
INIT_FUNCTION = share_ldb_init
PRIVATE_DEPENDENCIES = LIBLDB LDB_WRAP
# End MODULE share_ldb
################################################

share_ldb_OBJ_FILES = $(paramsrcdir)/share_ldb.o 

[SUBSYSTEM::SECRETS]
PRIVATE_DEPENDENCIES = LIBLDB TDB_WRAP UTIL_TDB NDR_SECURITY

SECRETS_OBJ_FILES = $(paramsrcdir)/secrets.o

[PYTHON::param]
LIBRARY_REALNAME = samba/param.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = LIBSAMBA-HOSTCONFIG PYTALLOC

param_OBJ_FILES = $(paramsrcdir)/pyparam.o

[SUBSYSTEM::pyparam_util]
PRIVATE_DEPENDENCIES = LIBPYTHON

pyparam_util_OBJ_FILES = $(paramsrcdir)/pyparam_util.o
