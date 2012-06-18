# DCERPC Server subsystem

################################################
# Start SUBSYSTEM DCERPC_COMMON
[SUBSYSTEM::DCERPC_COMMON]
PRIVATE_DEPENDENCIES = LIBLDB
#
# End SUBSYSTEM DCERPC_COMMON
################################################

DCERPC_COMMON_OBJ_FILES = $(addprefix $(rpc_serversrcdir)/common/, \
	server_info.o share_info.o forward.o)

$(eval $(call proto_header_template,$(rpc_serversrcdir)/common/proto.h,$(DCERPC_COMMON_OBJ_FILES:.o=.c)))

PUBLIC_HEADERS += $(rpc_serversrcdir)/common/common.h

################################################
# Start MODULE dcerpc_rpcecho
[MODULE::dcerpc_rpcecho]
INIT_FUNCTION = dcerpc_server_rpcecho_init
SUBSYSTEM = dcerpc_server
PRIVATE_DEPENDENCIES = NDR_STANDARD LIBEVENTS
# End MODULE dcerpc_rpcecho
################################################

dcerpc_rpcecho_OBJ_FILES = $(rpc_serversrcdir)/echo/rpc_echo.o

################################################
# Start MODULE dcerpc_epmapper
[MODULE::dcerpc_epmapper]
INIT_FUNCTION = dcerpc_server_epmapper_init
SUBSYSTEM = dcerpc_server
PRIVATE_DEPENDENCIES = NDR_EPMAPPER
# End MODULE dcerpc_epmapper
################################################

dcerpc_epmapper_OBJ_FILES = $(rpc_serversrcdir)/epmapper/rpc_epmapper.o

################################################
# Start MODULE dcerpc_remote
[MODULE::dcerpc_remote]
INIT_FUNCTION = dcerpc_server_remote_init
SUBSYSTEM = dcerpc_server
PRIVATE_DEPENDENCIES = \
		LIBCLI_SMB NDR_TABLE
# End MODULE dcerpc_remote
################################################

dcerpc_remote_OBJ_FILES = $(rpc_serversrcdir)/remote/dcesrv_remote.o

################################################
# Start MODULE dcerpc_srvsvc
[MODULE::dcerpc_srvsvc]
INIT_FUNCTION = dcerpc_server_srvsvc_init
SUBSYSTEM = dcerpc_server
PRIVATE_DEPENDENCIES = \
		DCERPC_COMMON NDR_SRVSVC share
# End MODULE dcerpc_srvsvc
################################################


dcerpc_srvsvc_OBJ_FILES = $(addprefix $(rpc_serversrcdir)/srvsvc/, dcesrv_srvsvc.o srvsvc_ntvfs.o)

$(eval $(call proto_header_template,$(rpc_serversrcdir)/srvsvc/proto.h,$(dcerpc_srvsvc_OBJ_FILES:.o=.c)))

################################################
# Start MODULE dcerpc_wkssvc
[MODULE::dcerpc_wkssvc]
INIT_FUNCTION = dcerpc_server_wkssvc_init
SUBSYSTEM = dcerpc_server
PRIVATE_DEPENDENCIES = \
		DCERPC_COMMON NDR_STANDARD
# End MODULE dcerpc_wkssvc
################################################

dcerpc_wkssvc_OBJ_FILES = $(rpc_serversrcdir)/wkssvc/dcesrv_wkssvc.o

################################################
# Start MODULE dcerpc_unixinfo
[MODULE::dcerpc_unixinfo]
INIT_FUNCTION = dcerpc_server_unixinfo_init
SUBSYSTEM = dcerpc_server
PRIVATE_DEPENDENCIES = \
		DCERPC_COMMON \
		SAMDB \
		NDR_UNIXINFO \
		NSS_WRAPPER \
		LIBWBCLIENT_OLD
# End MODULE dcerpc_unixinfo
################################################

dcerpc_unixinfo_OBJ_FILES = $(rpc_serversrcdir)/unixinfo/dcesrv_unixinfo.o

################################################
# Start MODULE dcesrv_samr
[MODULE::dcesrv_samr]
INIT_FUNCTION = dcerpc_server_samr_init
SUBSYSTEM = dcerpc_server
PRIVATE_DEPENDENCIES = \
		SAMDB \
		DCERPC_COMMON \
		NDR_STANDARD
# End MODULE dcesrv_samr
################################################

dcesrv_samr_OBJ_FILES = $(addprefix $(rpc_serversrcdir)/samr/, dcesrv_samr.o samr_password.o)

$(eval $(call proto_header_template,$(rpc_serversrcdir)/samr/proto.h,$(dcesrv_samr_OBJ_FILES:.o=.c)))

################################################
# Start MODULE dcerpc_winreg
[MODULE::dcerpc_winreg]
INIT_FUNCTION = dcerpc_server_winreg_init
SUBSYSTEM = dcerpc_server
OUTPUT_TYPE = MERGED_OBJ
PRIVATE_DEPENDENCIES = \
		registry NDR_STANDARD
# End MODULE dcerpc_winreg
################################################

dcerpc_winreg_OBJ_FILES = $(rpc_serversrcdir)/winreg/rpc_winreg.o

################################################
# Start MODULE dcerpc_netlogon
[MODULE::dcerpc_netlogon]
INIT_FUNCTION = dcerpc_server_netlogon_init
SUBSYSTEM = dcerpc_server
PRIVATE_DEPENDENCIES = \
		DCERPC_COMMON \
		SCHANNELDB \
		NDR_STANDARD \
		auth_sam \
		LIBSAMBA-HOSTCONFIG
# End MODULE dcerpc_netlogon
################################################

dcerpc_netlogon_OBJ_FILES = $(rpc_serversrcdir)/netlogon/dcerpc_netlogon.o

################################################
# Start MODULE dcerpc_lsa
[MODULE::dcerpc_lsarpc]
INIT_FUNCTION = dcerpc_server_lsa_init
SUBSYSTEM = dcerpc_server
PRIVATE_DEPENDENCIES = \
		SAMDB \
		DCERPC_COMMON \
		NDR_STANDARD \
		LIBCLI_AUTH \
		NDR_DSSETUP
# End MODULE dcerpc_lsa
################################################

dcerpc_lsarpc_OBJ_FILES = $(addprefix $(rpc_serversrcdir)/lsa/, dcesrv_lsa.o lsa_init.o lsa_lookup.o)

$(eval $(call proto_header_template,$(rpc_serversrcdir)/lsa/proto.h,$(dcerpc_lsarpc_OBJ_FILES:.o=.c)))


################################################
# Start MODULE dcerpc_spoolss
[MODULE::dcerpc_spoolss]
INIT_FUNCTION = dcerpc_server_spoolss_init
SUBSYSTEM = dcerpc_server
OUTPUT_TYPE = MERGED_OBJ
PRIVATE_DEPENDENCIES = \
		DCERPC_COMMON \
		NDR_SPOOLSS \
		ntptr \
		RPC_NDR_SPOOLSS
# End MODULE dcerpc_spoolss
################################################

dcerpc_spoolss_OBJ_FILES = $(rpc_serversrcdir)/spoolss/dcesrv_spoolss.o

################################################
# Start MODULE dcerpc_drsuapi
[MODULE::dcerpc_drsuapi]
INIT_FUNCTION = dcerpc_server_drsuapi_init
SUBSYSTEM = dcerpc_server
PRIVATE_DEPENDENCIES = \
		SAMDB \
		DCERPC_COMMON \
		NDR_DRSUAPI
# End MODULE dcerpc_drsuapi
################################################

dcerpc_drsuapi_OBJ_FILES = $(rpc_serversrcdir)/drsuapi/dcesrv_drsuapi.o \
	$(rpc_serversrcdir)/drsuapi/updaterefs.o \
	$(rpc_serversrcdir)/drsuapi/getncchanges.o \
	$(rpc_serversrcdir)/drsuapi/addentry.o \
	$(rpc_serversrcdir)/drsuapi/drsutil.o

################################################
# Start MODULE dcerpc_browser
[MODULE::dcerpc_browser]
INIT_FUNCTION = dcerpc_server_browser_init
SUBSYSTEM = dcerpc_server
PRIVATE_DEPENDENCIES = \
		DCERPC_COMMON \
		NDR_BROWSER
# End MODULE dcerpc_browser
################################################

dcerpc_browser_OBJ_FILES = $(rpc_serversrcdir)/browser/dcesrv_browser.o

################################################
# Start SUBSYSTEM dcerpc_server
[SUBSYSTEM::dcerpc_server]
PRIVATE_DEPENDENCIES = \
		LIBCLI_AUTH \
		LIBNDR \
		dcerpc samba_server_gensec

dcerpc_server_OBJ_FILES = $(addprefix $(rpc_serversrcdir)/, \
		dcerpc_server.o \
		dcesrv_auth.o \
		dcesrv_mgmt.o \
		handles.o)

$(eval $(call proto_header_template,$(rpc_serversrcdir)/dcerpc_server_proto.h,$(dcerpc_server_OBJ_FILES:.o=.c)))

# End SUBSYSTEM DCERPC
################################################

PUBLIC_HEADERS += $(rpc_serversrcdir)/dcerpc_server.h

[MODULE::DCESRV]
INIT_FUNCTION = server_service_rpc_init
SUBSYSTEM = service
PRIVATE_DEPENDENCIES = dcerpc_server

DCESRV_OBJ_FILES = $(rpc_serversrcdir)/service_rpc.o

$(eval $(call proto_header_template,$(rpc_serversrcdir)/service_rpc.h,$(DCESRV_OBJ_FILES:.o=.c)))

