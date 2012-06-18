#################################
# Start SUBSYSTEM gensec
[LIBRARY::gensec]
PUBLIC_DEPENDENCIES = \
		CREDENTIALS LIBSAMBA-UTIL LIBCRYPTO ASN1_UTIL samba_socket LIBPACKET
# End SUBSYSTEM gensec
#################################

PC_FILES += $(gensecsrcdir)/gensec.pc

gensec_VERSION = 0.0.1
gensec_SOVERSION = 0
gensec_OBJ_FILES = $(addprefix $(gensecsrcdir)/, gensec.o socket.o)

PUBLIC_HEADERS += $(gensecsrcdir)/gensec.h

$(eval $(call proto_header_template,$(gensecsrcdir)/gensec_proto.h,$(gensec_OBJ_FILES:.o=.c)))

################################################
# Start MODULE gensec_krb5
[MODULE::gensec_krb5]
SUBSYSTEM = gensec
INIT_FUNCTION = gensec_krb5_init
PRIVATE_DEPENDENCIES = CREDENTIALS KERBEROS auth_session
# End MODULE gensec_krb5
################################################

gensec_krb5_OBJ_FILES = $(addprefix $(gensecsrcdir)/, gensec_krb5.o)

################################################
# Start MODULE gensec_gssapi
[MODULE::gensec_gssapi]
SUBSYSTEM = gensec
INIT_FUNCTION = gensec_gssapi_init
PRIVATE_DEPENDENCIES = HEIMDAL_GSSAPI CREDENTIALS KERBEROS 
# End MODULE gensec_gssapi
################################################

gensec_gssapi_OBJ_FILES = $(addprefix $(gensecsrcdir)/, gensec_gssapi.o)

################################################
# Start MODULE cyrus_sasl
[MODULE::cyrus_sasl]
SUBSYSTEM = gensec
INIT_FUNCTION = gensec_sasl_init
PRIVATE_DEPENDENCIES = CREDENTIALS SASL 
# End MODULE cyrus_sasl
################################################

cyrus_sasl_OBJ_FILES = $(addprefix $(gensecsrcdir)/, cyrus_sasl.o)

################################################
# Start MODULE gensec_spnego
[MODULE::gensec_spnego]
SUBSYSTEM = gensec
INIT_FUNCTION = gensec_spnego_init
PRIVATE_DEPENDENCIES = ASN1_UTIL CREDENTIALS
# End MODULE gensec_spnego
################################################

gensec_spnego_OBJ_FILES = $(addprefix $(gensecsrcdir)/, spnego.o) ../libcli/auth/spnego_parse.o

$(eval $(call proto_header_template,$(gensecsrcdir)/spnego_proto.h,$(gensec_spnego_OBJ_FILES:.o=.c)))

################################################
# Start MODULE gensec_schannel
[MODULE::gensec_schannel]
SUBSYSTEM = gensec
INIT_FUNCTION = gensec_schannel_init
PRIVATE_DEPENDENCIES = SCHANNELDB NDR_SCHANNEL CREDENTIALS LIBNDR auth_session
OUTPUT_TYPE = MERGED_OBJ
# End MODULE gensec_schannel
################################################

gensec_schannel_OBJ_FILES = $(addprefix $(gensecsrcdir)/, schannel.o) ../libcli/auth/schannel_sign.o
$(eval $(call proto_header_template,$(gensecsrcdir)/schannel_proto.h,$(gensec_schannel_OBJ_FILES:.o=.c)))

################################################
# Start SUBSYSTEM SCHANNELDB
[SUBSYSTEM::SCHANNELDB]
PRIVATE_DEPENDENCIES = LDB_WRAP COMMON_SCHANNELDB
# End SUBSYSTEM SCHANNELDB
################################################

SCHANNELDB_OBJ_FILES = $(addprefix $(gensecsrcdir)/, schannel_state.o)
$(eval $(call proto_header_template,$(gensecsrcdir)/schannel_state.h,$(SCHANNELDB_OBJ_FILES:.o=.c)))

[PYTHON::pygensec]
PRIVATE_DEPENDENCIES = gensec PYTALLOC
LIBRARY_REALNAME = samba/gensec.$(SHLIBEXT)

pygensec_OBJ_FILES = $(gensecsrcdir)/pygensec.o
