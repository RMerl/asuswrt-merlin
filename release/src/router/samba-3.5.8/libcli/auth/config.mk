[SUBSYSTEM::ntlm_check]
PRIVATE_DEPENDENCIES = LIBSAMBA-UTIL

ntlm_check_OBJ_FILES = $(addprefix $(libclicommonsrcdir)/auth/, ntlm_check.o)

[SUBSYSTEM::MSRPC_PARSE]

MSRPC_PARSE_OBJ_FILES = $(addprefix $(libclicommonsrcdir)/auth/, msrpc_parse.o)

[SUBSYSTEM::LIBCLI_AUTH]
PUBLIC_DEPENDENCIES = \
		MSRPC_PARSE \
		LIBSAMBA-HOSTCONFIG

LIBCLI_AUTH_OBJ_FILES = $(addprefix $(libclicommonsrcdir)/auth/, \
		credentials.o \
		session.o \
		smbencrypt.o \
		smbdes.o)

PUBLIC_HEADERS += ../libcli/auth/credentials.h

[SUBSYSTEM::COMMON_SCHANNELDB]
PRIVATE_DEPENDENCIES = LDB_WRAP

COMMON_SCHANNELDB_OBJ_FILES = $(addprefix $(libclicommonsrcdir)/auth/, schannel_state_ldb.o)
