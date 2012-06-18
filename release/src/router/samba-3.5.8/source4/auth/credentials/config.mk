#################################
# Start SUBSYSTEM CREDENTIALS
[SUBSYSTEM::CREDENTIALS]
PUBLIC_DEPENDENCIES = \
		LIBCLI_AUTH SECRETS LIBCRYPTO KERBEROS UTIL_LDB HEIMDAL_GSSAPI 
PRIVATE_DEPENDENCIES = \
		SECRETS


CREDENTIALS_OBJ_FILES = $(addprefix $(authsrcdir)/credentials/, credentials.o credentials_files.o credentials_ntlm.o credentials_krb5.o ../kerberos/kerberos_util.o)

$(eval $(call proto_header_template,$(authsrcdir)/credentials/credentials_proto.h,$(CREDENTIALS_OBJ_FILES:.o=.c)))

PUBLIC_HEADERS += $(authsrcdir)/credentials/credentials.h

[PYTHON::pycredentials]
LIBRARY_REALNAME = samba/credentials.$(SHLIBEXT)
PUBLIC_DEPENDENCIES = CREDENTIALS LIBCMDLINE_CREDENTIALS PYTALLOC pyparam_util

pycredentials_OBJ_FILES = $(authsrcdir)/credentials/pycredentials.o
