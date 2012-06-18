# LDAP server subsystem

#######################
# Start SUBSYSTEM LDAP
[MODULE::LDAP]
INIT_FUNCTION = server_service_ldap_init
SUBSYSTEM = service
PRIVATE_DEPENDENCIES = CREDENTIALS \
		LIBCLI_LDAP SAMDB \
		process_model \
		gensec \
		LIBSAMBA-HOSTCONFIG
# End SUBSYSTEM SMB
#######################

LDAP_OBJ_FILES = $(addprefix $(ldap_serversrcdir)/, \
		ldap_server.o \
		ldap_backend.o \
		ldap_bind.o \
		ldap_extended.o)

$(eval $(call proto_header_template,$(ldap_serversrcdir)/proto.h,$(LDAP_OBJ_FILES:.o=.c)))
