# CLDAP server subsystem
#
[MODULE::service_cldap]
INIT_FUNCTION = server_service_cldapd_init
SUBSYSTEM = service
PRIVATE_DEPENDENCIES = \
		CLDAPD process_model LIBNETIF

service_cldap_OBJ_FILES = $(addprefix $(cldap_serversrcdir)/, \
		cldap_server.o)


#######################
# Start SUBSYSTEM CLDAPD
[SUBSYSTEM::CLDAPD]
PRIVATE_DEPENDENCIES = LIBCLI_CLDAP
# End SUBSYSTEM CLDAPD
#######################

CLDAPD_OBJ_FILES = $(addprefix $(cldap_serversrcdir)/, \
		netlogon.o \
		rootdse.o)

$(eval $(call proto_header_template,$(cldap_serversrcdir)/proto.h,$(CLDAPD_OBJ_FILES:.o=.c)))
