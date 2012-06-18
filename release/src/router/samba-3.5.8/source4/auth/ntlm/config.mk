# NTLM auth server subsystem

#######################
# Start MODULE auth_sam
[MODULE::auth_sam_module]
INIT_FUNCTION = auth_sam_init
SUBSYSTEM = auth
PRIVATE_DEPENDENCIES = \
		SAMDB auth_sam ntlm_check
# End MODULE auth_sam
#######################

auth_sam_module_OBJ_FILES = $(addprefix $(authsrcdir)/ntlm/, auth_sam.o)

#######################
# Start MODULE auth_anonymous
[MODULE::auth_anonymous]
INIT_FUNCTION = auth_anonymous_init
SUBSYSTEM = auth
# End MODULE auth_anonymous
#######################

auth_anonymous_OBJ_FILES = $(addprefix $(authsrcdir)/ntlm/, auth_anonymous.o)

#######################
# Start MODULE auth_anonymous
[MODULE::auth_server]
INIT_FUNCTION = auth_server_init
SUBSYSTEM = auth
PRIVATE_DEPENDENCIES = LIBSAMBA-UTIL LIBCLI_SMB
# End MODULE auth_server
#######################

auth_server_OBJ_FILES = $(addprefix $(authsrcdir)/ntlm/, auth_server.o)

#######################
# Start MODULE auth_winbind
[MODULE::auth_winbind]
INIT_FUNCTION = auth_winbind_init
SUBSYSTEM = auth
PRIVATE_DEPENDENCIES = NDR_WINBIND MESSAGING LIBWINBIND-CLIENT LIBWBCLIENT
# End MODULE auth_winbind
#######################

auth_winbind_OBJ_FILES = $(addprefix $(authsrcdir)/ntlm/, auth_winbind.o)

#######################
# Start MODULE auth_developer
[MODULE::auth_developer]
INIT_FUNCTION = auth_developer_init
SUBSYSTEM = auth
# End MODULE auth_developer
#######################

auth_developer_OBJ_FILES = $(addprefix $(authsrcdir)/ntlm/, auth_developer.o)

[MODULE::auth_unix]
INIT_FUNCTION = auth_unix_init
SUBSYSTEM = auth
PRIVATE_DEPENDENCIES = CRYPT PAM PAM_ERRORS NSS_WRAPPER UID_WRAPPER

auth_unix_OBJ_FILES = $(addprefix $(authsrcdir)/ntlm/, auth_unix.o)

[SUBSYSTEM::PAM_ERRORS]

#VERSION = 0.0.1
#SO_VERSION = 0
PAM_ERRORS_OBJ_FILES = $(addprefix $(authsrcdir)/ntlm/, pam_errors.o)

[MODULE::auth]
INIT_FUNCTION = server_service_auth_init
SUBSYSTEM = service
OUTPUT_TYPE = MERGED_OBJ
PRIVATE_DEPENDENCIES = LIBSAMBA-UTIL LIBSECURITY SAMDB CREDENTIALS 

auth_OBJ_FILES = $(addprefix $(authsrcdir)/ntlm/, auth.o auth_util.o auth_simple.o)
$(eval $(call proto_header_template,$(authsrcdir)/auth_proto.h,$(auth_OBJ_FILES:.o=.c)))

# PUBLIC_HEADERS += auth/auth.h

