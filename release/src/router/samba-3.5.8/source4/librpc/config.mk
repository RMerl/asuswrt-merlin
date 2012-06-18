ndrsrcdir = $(librpcsrcdir)/ndr
gen_ndrsrcdir = $(librpcsrcdir)/gen_ndr
dcerpcsrcdir = $(librpcsrcdir)/rpc

################################################
# Start SUBSYSTEM LIBNDR
[LIBRARY::LIBNDR]
PUBLIC_DEPENDENCIES = LIBSAMBA-ERRORS LIBTALLOC LIBSAMBA-UTIL CHARSET \
					  LIBSAMBA-HOSTCONFIG

LIBNDR_OBJ_FILES = $(addprefix $(ndrsrcdir)/, ndr_string.o) ../librpc/ndr/ndr_basic.o ../librpc/ndr/uuid.o ../librpc/ndr/ndr.o ../librpc/gen_ndr/ndr_misc.o ../librpc/ndr/ndr_misc.o

PC_FILES += ../librpc/ndr.pc
LIBNDR_VERSION = 0.0.1
LIBNDR_SOVERSION = 0

# End SUBSYSTEM LIBNDR
################################################

PUBLIC_HEADERS += ../librpc/ndr/libndr.h
PUBLIC_HEADERS += ../librpc/gen_ndr/misc.h ../librpc/gen_ndr/ndr_misc.h

#################################
# Start BINARY ndrdump
[BINARY::ndrdump]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBSAMBA-HOSTCONFIG \
		LIBSAMBA-UTIL \
		LIBPOPT \
		POPT_SAMBA \
		NDR_TABLE \
		LIBSAMBA-ERRORS
# FIXME: ndrdump shouldn't have to depend on RPC...
# End BINARY ndrdump
#################################

ndrdump_OBJ_FILES = ../librpc/tools/ndrdump.o

MANPAGES += ../librpc/tools/ndrdump.1

################################################
# Start SUBSYSTEM NDR_COMPRESSION
[SUBSYSTEM::NDR_COMPRESSION]
PRIVATE_DEPENDENCIES = ZLIB LZXPRESS
PUBLIC_DEPENDENCIES = LIBSAMBA-ERRORS LIBNDR
# End SUBSYSTEM NDR_COMPRESSION
################################################

NDR_COMPRESSION_OBJ_FILES = ../librpc/ndr/ndr_compression.o

[SUBSYSTEM::NDR_SECURITY]
PUBLIC_DEPENDENCIES = LIBNDR LIBSECURITY

NDR_SECURITY_OBJ_FILES = ../librpc/gen_ndr/ndr_security.o \
			 ../librpc/ndr/ndr_sec_helper.o \
			 $(gen_ndrsrcdir)/ndr_server_id.o

PUBLIC_HEADERS += ../librpc/gen_ndr/security.h
PUBLIC_HEADERS += $(gen_ndrsrcdir)/server_id.h

[SUBSYSTEM::NDR_AUDIOSRV]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_AUDIOSRV_OBJ_FILES = ../librpc/gen_ndr/ndr_audiosrv.o

[SUBSYSTEM::NDR_NAMED_PIPE_AUTH]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_NAMED_PIPE_AUTH_OBJ_FILES = ../librpc/gen_ndr/ndr_named_pipe_auth.o

[SUBSYSTEM::NDR_DNSSERVER]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_DNSSERVER_OBJ_FILES = ../librpc/gen_ndr/ndr_dnsserver.o

[SUBSYSTEM::NDR_WINSTATION]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_WINSTATION_OBJ_FILES = $(gen_ndrsrcdir)/ndr_winstation.o

[SUBSYSTEM::NDR_IRPC]
PUBLIC_DEPENDENCIES = LIBNDR NDR_SECURITY NDR_NBT

NDR_IRPC_OBJ_FILES = $(gen_ndrsrcdir)/ndr_irpc.o

[SUBSYSTEM::NDR_DCOM]
PUBLIC_DEPENDENCIES = LIBNDR NDR_SECURITY NDR_ORPC

NDR_DCOM_OBJ_FILES = ../librpc/gen_ndr/ndr_dcom.o

[SUBSYSTEM::NDR_WMI]
PUBLIC_DEPENDENCIES = LIBNDR NDR_SECURITY NDR_DCOM

NDR_WMI_OBJ_FILES = ../librpc/gen_ndr/ndr_wmi.o ../librpc/ndr/ndr_wmi.o

[SUBSYSTEM::NDR_DSBACKUP]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_DSBACKUP_OBJ_FILES = ../librpc/gen_ndr/ndr_dsbackup.o

[SUBSYSTEM::NDR_EFS]
PUBLIC_DEPENDENCIES = LIBNDR NDR_SECURITY

NDR_EFS_OBJ_FILES = ../librpc/gen_ndr/ndr_efs.o

[SUBSYSTEM::NDR_ROT]
PUBLIC_DEPENDENCIES = LIBNDR NDR_ORPC

NDR_ROT_OBJ_FILES = ../librpc/gen_ndr/ndr_rot.o

[SUBSYSTEM::NDR_FRSRPC]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_FRSRPC_OBJ_FILES = ../librpc/gen_ndr/ndr_frsrpc.o ../librpc/ndr/ndr_frsrpc.o

[SUBSYSTEM::NDR_FRSAPI]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_FRSAPI_OBJ_FILES = ../librpc/gen_ndr/ndr_frsapi.o

[SUBSYSTEM::NDR_FRSTRANS]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_FRSTRANS_OBJ_FILES = ../librpc/gen_ndr/ndr_frstrans.o

[SUBSYSTEM::NDR_DRSUAPI]
PUBLIC_DEPENDENCIES = LIBNDR NDR_COMPRESSION NDR_SECURITY NDR_STANDARD ASN1_UTIL

NDR_DRSUAPI_OBJ_FILES = ../librpc/gen_ndr/ndr_drsuapi.o ../librpc/ndr/ndr_drsuapi.o

[SUBSYSTEM::NDR_DRSBLOBS]
PUBLIC_DEPENDENCIES = LIBNDR NDR_DRSUAPI

NDR_DRSBLOBS_OBJ_FILES = ../librpc/gen_ndr/ndr_drsblobs.o ../librpc/ndr/ndr_drsblobs.o

[SUBSYSTEM::NDR_SASL_HELPERS]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_SASL_HELPERS_OBJ_FILES = $(gen_ndrsrcdir)/ndr_sasl_helpers.o

[SUBSYSTEM::NDR_POLICYAGENT]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_POLICYAGENT_OBJ_FILES = ../librpc/gen_ndr/ndr_policyagent.o

[SUBSYSTEM::NDR_UNIXINFO]
PUBLIC_DEPENDENCIES = LIBNDR NDR_SECURITY

NDR_UNIXINFO_OBJ_FILES = ../librpc/gen_ndr/ndr_unixinfo.o

[SUBSYSTEM::NDR_NFS4ACL]
PUBLIC_DEPENDENCIES = LIBNDR NDR_SECURITY

NDR_NFS4ACL_OBJ_FILES = $(gen_ndrsrcdir)/ndr_nfs4acl.o

[SUBSYSTEM::NDR_SPOOLSS]
PUBLIC_DEPENDENCIES = LIBNDR NDR_SPOOLSS_BUF NDR_SECURITY

NDR_SPOOLSS_OBJ_FILES = ../librpc/gen_ndr/ndr_spoolss.o

[SUBSYSTEM::NDR_SPOOLSS_BUF]

NDR_SPOOLSS_BUF_OBJ_FILES = ../librpc/ndr/ndr_spoolss_buf.o

[SUBSYSTEM::NDR_EPMAPPER]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_EPMAPPER_OBJ_FILES = ../librpc/gen_ndr/ndr_epmapper.o

[SUBSYSTEM::NDR_DBGIDL]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_DBGIDL_OBJ_FILES = ../librpc/gen_ndr/ndr_dbgidl.o

[SUBSYSTEM::NDR_DSSETUP]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_DSSETUP_OBJ_FILES = ../librpc/gen_ndr/ndr_dssetup.o

[SUBSYSTEM::NDR_MSGSVC]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_MSGSVC_OBJ_FILES = ../librpc/gen_ndr/ndr_msgsvc.o

[SUBSYSTEM::NDR_WINSIF]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_WINSIF_OBJ_FILES = $(gen_ndrsrcdir)/ndr_winsif.o

[SUBSYSTEM::NDR_MGMT]
PUBLIC_DEPENDENCIES = LIBNDR 

NDR_MGMT_OBJ_FILES = ../librpc/gen_ndr/ndr_mgmt.o

[SUBSYSTEM::NDR_PROTECTED_STORAGE]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_PROTECTED_STORAGE_OBJ_FILES = ../librpc/gen_ndr/ndr_protected_storage.o

[SUBSYSTEM::NDR_ORPC]
PUBLIC_DEPENDENCIES = LIBNDR 

NDR_ORPC_OBJ_FILES = ../librpc/gen_ndr/ndr_orpc.o ../librpc/ndr/ndr_orpc.o 

[SUBSYSTEM::NDR_OXIDRESOLVER]
PUBLIC_DEPENDENCIES = LIBNDR NDR_ORPC

NDR_OXIDRESOLVER_OBJ_FILES = ../librpc/gen_ndr/ndr_oxidresolver.o

[SUBSYSTEM::NDR_REMACT]
PUBLIC_DEPENDENCIES = LIBNDR NDR_ORPC

NDR_REMACT_OBJ_FILES = ../librpc/gen_ndr/ndr_remact.o

[SUBSYSTEM::NDR_WZCSVC]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_WZCSVC_OBJ_FILES = ../librpc/gen_ndr/ndr_wzcsvc.o

[SUBSYSTEM::NDR_BROWSER]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_BROWSER_OBJ_FILES = ../librpc/gen_ndr/ndr_browser.o

[SUBSYSTEM::NDR_W32TIME]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_W32TIME_OBJ_FILES = ../librpc/gen_ndr/ndr_w32time.o

[SUBSYSTEM::NDR_SCERPC]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_SCERPC_OBJ_FILES = ../librpc/gen_ndr/ndr_scerpc.o

[SUBSYSTEM::NDR_TRKWKS]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_TRKWKS_OBJ_FILES = ../librpc/gen_ndr/ndr_trkwks.o

[SUBSYSTEM::NDR_KEYSVC]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_KEYSVC_OBJ_FILES = ../librpc/gen_ndr/ndr_keysvc.o

[SUBSYSTEM::NDR_KRB5PAC]
PUBLIC_DEPENDENCIES = LIBNDR NDR_STANDARD NDR_SECURITY

NDR_KRB5PAC_OBJ_FILES = ../librpc/gen_ndr/ndr_krb5pac.o ../librpc/ndr/ndr_krb5pac.o

[SUBSYSTEM::NDR_XATTR]
PUBLIC_DEPENDENCIES = LIBNDR NDR_SECURITY

NDR_XATTR_OBJ_FILES = ../librpc/gen_ndr/ndr_xattr.o ../librpc/ndr/ndr_xattr.o

[SUBSYSTEM::NDR_OPENDB]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_OPENDB_OBJ_FILES = $(gen_ndrsrcdir)/ndr_opendb.o

[SUBSYSTEM::NDR_NOTIFY]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_NOTIFY_OBJ_FILES = $(gen_ndrsrcdir)/ndr_notify.o

[SUBSYSTEM::NDR_SCHANNEL]
PUBLIC_DEPENDENCIES = LIBNDR NDR_NBT

NDR_SCHANNEL_OBJ_FILES = ../librpc/gen_ndr/ndr_schannel.o ../librpc/ndr/ndr_schannel.o

[SUBSYSTEM::NDR_NBT]
PUBLIC_DEPENDENCIES = LIBNDR NDR_NBT_BUF NDR_SECURITY NDR_STANDARD LIBCLI_NDR_NETLOGON

NDR_NBT_OBJ_FILES = ../librpc/gen_ndr/ndr_nbt.o

PUBLIC_HEADERS += ../librpc/gen_ndr/nbt.h

[SUBSYSTEM::NDR_NTP_SIGND]
PUBLIC_DEPENDENCIES = LIBNDR 

NDR_NTP_SIGND_OBJ_FILES = $(gen_ndrsrcdir)/ndr_ntp_signd.o

[SUBSYSTEM::NDR_WINSREPL]
PUBLIC_DEPENDENCIES = LIBNDR NDR_NBT

NDR_WINSREPL_OBJ_FILES = $(gen_ndrsrcdir)/ndr_winsrepl.o

[SUBSYSTEM::NDR_WINBIND]
PUBLIC_DEPENDENCIES = LIBNDR NDR_STANDARD

NDR_WINBIND_OBJ_FILES = $(gen_ndrsrcdir)/ndr_winbind.o
#PUBLIC_HEADERS += $(gen_ndrsrcdir)/winbind.h

[SUBSYSTEM::NDR_NTLMSSP]
PUBLIC_DEPENDENCIES = LIBNDR NDR_STANDARD

NDR_NTLMSSP_OBJ_FILES = ../librpc/gen_ndr/ndr_ntlmssp.o ../librpc/ndr/ndr_ntlmssp.o

$(librpcsrcdir)/idl-deps:
	$(PERL) $(librpcsrcdir)/idl-deps.pl $(wildcard $(librpcsrcdir)/idl/*.idl ../librpc/idl/*.idl) >$@

clean:: 
	rm -f $(librpcsrcdir)/idl-deps

-include $(librpcsrcdir)/idl-deps

$(gen_ndrsrcdir)/tables.c: $(IDL_NDR_PARSE_H_FILES)
	@echo Generating $@
	@$(PERL) ../librpc/tables.pl --output=$@ $^ > $(gen_ndrsrcdir)/tables.x
	@mv $(gen_ndrsrcdir)/tables.x $@

[LIBRARY::NDR_STANDARD]
PUBLIC_DEPENDENCIES = LIBNDR
PRIVATE_DEPENDENCIES = NDR_SECURITY

NDR_STANDARD_OBJ_FILES = ../librpc/gen_ndr/ndr_echo.o \
						 ../librpc/gen_ndr/ndr_lsa.o \
						 ../librpc/gen_ndr/ndr_samr.o \
						 ../librpc/gen_ndr/ndr_netlogon.o \
						 ../librpc/ndr/ndr_netlogon.o \
						 ../librpc/gen_ndr/ndr_dfs.o \
						 ../librpc/gen_ndr/ndr_atsvc.o \
						 ../librpc/gen_ndr/ndr_wkssvc.o \
						 ../librpc/gen_ndr/ndr_srvsvc.o \
						 ../librpc/gen_ndr/ndr_svcctl.o \
						 ../librpc/ndr/ndr_svcctl.o \
						 ../librpc/gen_ndr/ndr_winreg.o \
						 ../librpc/gen_ndr/ndr_initshutdown.o \
						 ../librpc/gen_ndr/ndr_eventlog.o \
						 ../librpc/gen_ndr/ndr_ntsvcs.o

PC_FILES += ../librpc/ndr_standard.pc

PUBLIC_HEADERS += $(addprefix ../librpc/gen_ndr/, samr.h ndr_samr.h lsa.h netlogon.h atsvc.h ndr_atsvc.h ndr_svcctl.h svcctl.h)

NDR_STANDARD_VERSION = 0.0.1
NDR_STANDARD_SOVERSION = 0

[SUBSYSTEM::NDR_TABLE]
PUBLIC_DEPENDENCIES = \
	NDR_STANDARD \
	NDR_AUDIOSRV \
	NDR_DSBACKUP NDR_EFS NDR_DRSUAPI \
	NDR_POLICYAGENT NDR_UNIXINFO NDR_SPOOLSS \
	NDR_EPMAPPER NDR_DBGIDL NDR_DSSETUP NDR_MSGSVC NDR_WINSIF \
	NDR_MGMT NDR_PROTECTED_STORAGE NDR_OXIDRESOLVER \
	NDR_REMACT NDR_WZCSVC NDR_BROWSER NDR_W32TIME NDR_SCERPC \
	NDR_TRKWKS NDR_KEYSVC NDR_KRB5PAC NDR_XATTR NDR_SCHANNEL \
	NDR_ROT NDR_DRSBLOBS NDR_NBT NDR_WINSREPL NDR_SECURITY \
	NDR_DNSSERVER NDR_WINSTATION NDR_IRPC NDR_OPENDB \
	NDR_SASL_HELPERS NDR_NOTIFY NDR_WINBIND \
	NDR_FRSRPC NDR_FRSAPI NDR_FRSTRANS \
	NDR_NFS4ACL NDR_NTP_SIGND \
	NDR_DCOM NDR_WMI NDR_NAMED_PIPE_AUTH \
	NDR_NTLMSSP

NDR_TABLE_OBJ_FILES = ../librpc/ndr/ndr_table.o $(gen_ndrsrcdir)/tables.o

[SUBSYSTEM::RPC_NDR_ROT]
PUBLIC_DEPENDENCIES = NDR_ROT dcerpc

RPC_NDR_ROT_OBJ_FILES = ../librpc/gen_ndr/ndr_rot_c.o

[SUBSYSTEM::RPC_NDR_AUDIOSRV]
PUBLIC_DEPENDENCIES = NDR_AUDIOSRV dcerpc

RPC_NDR_AUDIOSRV_OBJ_FILES = ../librpc/gen_ndr/ndr_audiosrv_c.o

[SUBSYSTEM::RPC_NDR_ECHO]
PUBLIC_DEPENDENCIES = dcerpc NDR_STANDARD

RPC_NDR_ECHO_OBJ_FILES = ../librpc/gen_ndr/ndr_echo_c.o

[SUBSYSTEM::RPC_NDR_DSBACKUP]
PUBLIC_DEPENDENCIES = dcerpc NDR_DSBACKUP

RPC_NDR_DSBACKUP_OBJ_FILES = ../librpc/gen_ndr/ndr_dsbackup_c.o

[SUBSYSTEM::RPC_NDR_EFS]
PUBLIC_DEPENDENCIES = dcerpc NDR_EFS

RPC_NDR_EFS_OBJ_FILES = ../librpc/gen_ndr/ndr_efs_c.o

[SUBSYSTEM::RPC_NDR_LSA]
PUBLIC_DEPENDENCIES = dcerpc NDR_STANDARD

RPC_NDR_LSA_OBJ_FILES = ../librpc/gen_ndr/ndr_lsa_c.o

[SUBSYSTEM::RPC_NDR_DFS]
PUBLIC_DEPENDENCIES = dcerpc NDR_STANDARD

RPC_NDR_DFS_OBJ_FILES = ../librpc/gen_ndr/ndr_dfs_c.o

[SUBSYSTEM::RPC_NDR_FRSAPI]
PUBLIC_DEPENDENCIES = dcerpc NDR_FRSAPI

RPC_NDR_FRSAPI_OBJ_FILES = ../librpc/gen_ndr/ndr_frsapi_c.o

[SUBSYSTEM::RPC_NDR_DRSUAPI]
PUBLIC_DEPENDENCIES = dcerpc NDR_DRSUAPI

RPC_NDR_DRSUAPI_OBJ_FILES = ../librpc/gen_ndr/ndr_drsuapi_c.o

[SUBSYSTEM::RPC_NDR_POLICYAGENT]
PUBLIC_DEPENDENCIES = dcerpc NDR_POLICYAGENT

RPC_NDR_POLICYAGENT_OBJ_FILES = ../librpc/gen_ndr/ndr_policyagent_c.o

[SUBSYSTEM::RPC_NDR_UNIXINFO]
PUBLIC_DEPENDENCIES = dcerpc NDR_UNIXINFO

RPC_NDR_UNIXINFO_OBJ_FILES = ../librpc/gen_ndr/ndr_unixinfo_c.o

[SUBSYSTEM::RPC_NDR_BROWSER]
PUBLIC_DEPENDENCIES = dcerpc NDR_BROWSER

RPC_NDR_BROWSER_OBJ_FILES = ../librpc/gen_ndr/ndr_browser_c.o

[SUBSYSTEM::RPC_NDR_IRPC]
PUBLIC_DEPENDENCIES = dcerpc NDR_IRPC

RPC_NDR_IRPC_OBJ_FILES = $(gen_ndrsrcdir)/ndr_irpc_c.o

[LIBRARY::dcerpc_samr]
PUBLIC_DEPENDENCIES = dcerpc NDR_STANDARD

PC_FILES += $(librpcsrcdir)/dcerpc_samr.pc

dcerpc_samr_VERSION = 0.0.1
dcerpc_samr_SOVERSION = 0
dcerpc_samr_OBJ_FILES = ../librpc/gen_ndr/ndr_samr_c.o

PUBLIC_HEADERS += ../librpc/gen_ndr/ndr_samr_c.h

[SUBSYSTEM::RPC_NDR_SPOOLSS]
PUBLIC_DEPENDENCIES = dcerpc NDR_SPOOLSS

RPC_NDR_SPOOLSS_OBJ_FILES = ../librpc/gen_ndr/ndr_spoolss_c.o

[SUBSYSTEM::RPC_NDR_WKSSVC]
PUBLIC_DEPENDENCIES = dcerpc NDR_STANDARD

RPC_NDR_WKSSVC_OBJ_FILES = ../librpc/gen_ndr/ndr_wkssvc_c.o

[SUBSYSTEM::RPC_NDR_SRVSVC]
PUBLIC_DEPENDENCIES = dcerpc NDR_SRVSVC

RPC_NDR_SRVSVC_OBJ_FILES = ../librpc/gen_ndr/ndr_srvsvc_c.o

[SUBSYSTEM::RPC_NDR_SVCCTL]
PUBLIC_DEPENDENCIES = dcerpc NDR_STANDARD

RPC_NDR_SVCCTL_OBJ_FILES = ../librpc/gen_ndr/ndr_svcctl_c.o

PUBLIC_HEADERS += ../librpc/gen_ndr/ndr_svcctl_c.h

[LIBRARY::dcerpc_atsvc]
PUBLIC_DEPENDENCIES = dcerpc NDR_STANDARD

dcerpc_atsvc_VERSION = 0.0.1
dcerpc_atsvc_SOVERSION = 0

dcerpc_atsvc_OBJ_FILES = ../librpc/gen_ndr/ndr_atsvc_c.o
PC_FILES += $(librpcsrcdir)/dcerpc_atsvc.pc

PUBLIC_HEADERS += ../librpc/gen_ndr/ndr_atsvc_c.h

[SUBSYSTEM::RPC_NDR_EVENTLOG]
PUBLIC_DEPENDENCIES = dcerpc NDR_STANDARD

RPC_NDR_EVENTLOG_OBJ_FILES = ../librpc/gen_ndr/ndr_eventlog_c.o

[SUBSYSTEM::RPC_NDR_EPMAPPER]
PUBLIC_DEPENDENCIES = NDR_EPMAPPER 

RPC_NDR_EPMAPPER_OBJ_FILES = ../librpc/gen_ndr/ndr_epmapper_c.o

[SUBSYSTEM::RPC_NDR_DBGIDL]
PUBLIC_DEPENDENCIES = dcerpc NDR_DBGIDL

RPC_NDR_DBGIDL_OBJ_FILES = ../librpc/gen_ndr/ndr_dbgidl_c.o

[SUBSYSTEM::RPC_NDR_DSSETUP]
PUBLIC_DEPENDENCIES = dcerpc NDR_DSSETUP

RPC_NDR_DSSETUP_OBJ_FILES = ../librpc/gen_ndr/ndr_dssetup_c.o

[SUBSYSTEM::RPC_NDR_MSGSVC]
PUBLIC_DEPENDENCIES = dcerpc NDR_MSGSVC

RPC_NDR_MSGSVC_OBJ_FILES = ../librpc/gen_ndr/ndr_msgsvc_c.o

[SUBSYSTEM::RPC_NDR_WINSIF]
PUBLIC_DEPENDENCIES = dcerpc NDR_WINSIF

RPC_NDR_WINSIF_OBJ_FILES = $(gen_ndrsrcdir)/ndr_winsif_c.o

[SUBSYSTEM::RPC_NDR_WINREG]
PUBLIC_DEPENDENCIES = dcerpc NDR_STANDARD

RPC_NDR_WINREG_OBJ_FILES = ../librpc/gen_ndr/ndr_winreg_c.o

[SUBSYSTEM::RPC_NDR_INITSHUTDOWN]
PUBLIC_DEPENDENCIES = dcerpc NDR_STANDARD

RPC_NDR_INITSHUTDOWN_OBJ_FILES = ../librpc/gen_ndr/ndr_initshutdown_c.o

[SUBSYSTEM::RPC_NDR_MGMT]
PRIVATE_DEPENDENCIES = NDR_MGMT

RPC_NDR_MGMT_OBJ_FILES = ../librpc/gen_ndr/ndr_mgmt_c.o

[SUBSYSTEM::RPC_NDR_PROTECTED_STORAGE]
PUBLIC_DEPENDENCIES = dcerpc NDR_PROTECTED_STORAGE

RPC_NDR_PROTECTED_STORAGE_OBJ_FILES = ../librpc/gen_ndr/ndr_protected_storage_c.o

[SUBSYSTEM::RPC_NDR_OXIDRESOLVER]
PUBLIC_DEPENDENCIES = dcerpc NDR_OXIDRESOLVER

RPC_NDR_OXIDRESOLVER_OBJ_FILES = ../librpc/gen_ndr/ndr_oxidresolver_c.o

[SUBSYSTEM::RPC_NDR_REMACT]
PUBLIC_DEPENDENCIES = dcerpc NDR_REMACT

RPC_NDR_REMACT_OBJ_FILES = ../librpc/gen_ndr/ndr_remact_c.o

[SUBSYSTEM::RPC_NDR_WZCSVC]
PUBLIC_DEPENDENCIES = dcerpc NDR_WZCSVC

RPC_NDR_WZCSVC_OBJ_FILES = ../librpc/gen_ndr/ndr_wzcsvc_c.o

[SUBSYSTEM::RPC_NDR_W32TIME]
PUBLIC_DEPENDENCIES = dcerpc NDR_W32TIME

RPC_NDR_W32TIME_OBJ_FILES = ../librpc/gen_ndr/ndr_w32time_c.o

[SUBSYSTEM::RPC_NDR_SCERPC]
PUBLIC_DEPENDENCIES = dcerpc NDR_SCERPC

RPC_NDR_SCERPC_OBJ_FILES = ../librpc/gen_ndr/ndr_scerpc_c.o

[SUBSYSTEM::RPC_NDR_NTSVCS]
PUBLIC_DEPENDENCIES = dcerpc NDR_STANDARD

RPC_NDR_NTSVCS_OBJ_FILES = ../librpc/gen_ndr/ndr_ntsvcs_c.o

[SUBSYSTEM::RPC_NDR_NETLOGON]
PUBLIC_DEPENDENCIES = NDR_STANDARD

RPC_NDR_NETLOGON_OBJ_FILES = ../librpc/gen_ndr/ndr_netlogon_c.o

[SUBSYSTEM::RPC_NDR_TRKWKS]
PUBLIC_DEPENDENCIES = dcerpc NDR_TRKWKS

RPC_NDR_TRKWKS_OBJ_FILES = ../librpc/gen_ndr/ndr_trkwks_c.o

[SUBSYSTEM::RPC_NDR_KEYSVC]
PUBLIC_DEPENDENCIES = dcerpc NDR_KEYSVC

RPC_NDR_KEYSVC_OBJ_FILES = ../librpc/gen_ndr/ndr_keysvc_c.o

[SUBSYSTEM::NDR_DCERPC]
PUBLIC_DEPENDENCIES = LIBNDR

NDR_DCERPC_OBJ_FILES = ../librpc/gen_ndr/ndr_dcerpc.o

PUBLIC_HEADERS += ../librpc/gen_ndr/dcerpc.h ../librpc/gen_ndr/ndr_dcerpc.h

################################################
# Start SUBSYSTEM dcerpc
[LIBRARY::dcerpc]
PRIVATE_DEPENDENCIES = \
		samba_socket LIBCLI_RESOLVE LIBCLI_SMB LIBCLI_SMB2 \
		LIBNDR NDR_DCERPC RPC_NDR_EPMAPPER \
		NDR_SCHANNEL RPC_NDR_NETLOGON \
		RPC_NDR_MGMT \
		gensec LIBCLI_AUTH LIBCLI_RAW \
		LP_RESOLVE
PUBLIC_DEPENDENCIES = CREDENTIALS 
# End SUBSYSTEM dcerpc
################################################

PC_FILES += $(librpcsrcdir)/dcerpc.pc
dcerpc_VERSION = 0.0.1
dcerpc_SOVERSION = 0

dcerpc_OBJ_FILES = $(addprefix $(dcerpcsrcdir)/, dcerpc.o dcerpc_auth.o dcerpc_schannel.o dcerpc_util.o \
				  dcerpc_smb.o dcerpc_smb2.o dcerpc_sock.o dcerpc_connect.o dcerpc_secondary.o) \
					../librpc/rpc/binding.o ../librpc/rpc/dcerpc_error.o

$(eval $(call proto_header_template,$(dcerpcsrcdir)/dcerpc_proto.h,$(dcerpc_OBJ_FILES:.o=.c)))


PUBLIC_HEADERS += $(addprefix $(librpcsrcdir)/, rpc/dcerpc.h) \
			$(addprefix ../librpc/gen_ndr/, mgmt.h ndr_mgmt.h ndr_mgmt_c.h \
			epmapper.h ndr_epmapper.h ndr_epmapper_c.h)


[PYTHON::python_dcerpc]
LIBRARY_REALNAME = samba/dcerpc/base.$(SHLIBEXT)
PUBLIC_DEPENDENCIES = LIBCLI_SMB LIBSAMBA-UTIL LIBSAMBA-HOSTCONFIG dcerpc_samr RPC_NDR_LSA DYNCONFIG pycredentials pyparam_util

python_dcerpc_OBJ_FILES = $(dcerpcsrcdir)/pyrpc.o

$(eval $(call python_py_module_template,samba/dcerpc/__init__.py,$(dcerpcsrcdir)/dcerpc.py))


[PYTHON::python_echo]
LIBRARY_REALNAME = samba/dcerpc/echo.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = RPC_NDR_ECHO PYTALLOC pyparam_util pycredentials python_dcerpc

python_echo_OBJ_FILES = ../librpc/gen_ndr/py_echo.o

[PYTHON::python_winreg]
LIBRARY_REALNAME = samba/dcerpc/winreg.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = RPC_NDR_WINREG PYTALLOC pyparam_util pycredentials python_dcerpc

python_winreg_OBJ_FILES = ../librpc/gen_ndr/py_winreg.o

[PYTHON::python_dcerpc_misc]
LIBRARY_REALNAME = samba/dcerpc/misc.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = PYTALLOC python_dcerpc NDR_MISC NDR_KRB5PAC

python_dcerpc_misc_OBJ_FILES = ../librpc/gen_ndr/py_misc.o

[PYTHON::python_initshutdown]
LIBRARY_REALNAME = samba/dcerpc/initshutdown.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = RPC_NDR_INITSHUTDOWN PYTALLOC pyparam_util pycredentials python_dcerpc

python_initshutdown_OBJ_FILES = ../librpc/gen_ndr/py_initshutdown.o

[PYTHON::python_epmapper]
LIBRARY_REALNAME = samba/dcerpc/epmapper.$(SHLIBEXT)
PRIVATE_DEPENDENCIES =  dcerpc PYTALLOC pyparam_util pycredentials python_dcerpc

python_epmapper_OBJ_FILES = ../librpc/gen_ndr/py_epmapper.o

[PYTHON::python_mgmt]
LIBRARY_REALNAME = samba/dcerpc/mgmt.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = PYTALLOC param pycredentials dcerpc python_dcerpc

python_mgmt_OBJ_FILES = ../librpc/gen_ndr/py_mgmt.o

[PYTHON::python_atsvc]
LIBRARY_REALNAME = samba/dcerpc/atsvc.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = dcerpc_atsvc PYTALLOC pyparam_util pycredentials python_dcerpc

python_atsvc_OBJ_FILES = ../librpc/gen_ndr/py_atsvc.o

[PYTHON::python_dcerpc_nbt]
LIBRARY_REALNAME = samba/dcerpc/nbt.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = NDR_NBT PYTALLOC pyparam_util pycredentials python_dcerpc

python_dcerpc_nbt_OBJ_FILES = ../librpc/gen_ndr/py_nbt.o

[PYTHON::python_samr]
LIBRARY_REALNAME = samba/dcerpc/samr.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = dcerpc_samr PYTALLOC pycredentials pyparam_util python_dcerpc

python_samr_OBJ_FILES = ../librpc/gen_ndr/py_samr.o

[PYTHON::python_svcctl]
LIBRARY_REALNAME = samba/dcerpc/svcctl.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = RPC_NDR_SVCCTL PYTALLOC pyparam_util pycredentials python_dcerpc

python_svcctl_OBJ_FILES = ../librpc/gen_ndr/py_svcctl.o

[PYTHON::python_lsa]
LIBRARY_REALNAME = samba/dcerpc/lsa.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = RPC_NDR_LSA PYTALLOC pyparam_util pycredentials python_dcerpc

python_lsa_OBJ_FILES = ../librpc/gen_ndr/py_lsa.o

[PYTHON::python_wkssvc]
LIBRARY_REALNAME = samba/dcerpc/wkssvc.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = RPC_NDR_WKSSVC PYTALLOC pyparam_util pycredentials python_dcerpc

python_wkssvc_OBJ_FILES = ../librpc/gen_ndr/py_wkssvc.o

[PYTHON::python_dfs]
LIBRARY_REALNAME = samba/dcerpc/dfs.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = RPC_NDR_DFS PYTALLOC pyparam_util pycredentials python_dcerpc

python_dfs_OBJ_FILES = ../librpc/gen_ndr/py_dfs.o

[PYTHON::python_unixinfo]
LIBRARY_REALNAME = samba/dcerpc/unixinfo.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = RPC_NDR_UNIXINFO PYTALLOC pyparam_util pycredentials python_dcerpc

python_unixinfo_OBJ_FILES = ../librpc/gen_ndr/py_unixinfo.o

[PYTHON::python_irpc]
LIBRARY_REALNAME = samba/dcerpc/irpc.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = RPC_NDR_IRPC PYTALLOC pyparam_util pycredentials python_dcerpc

python_irpc_OBJ_FILES = $(gen_ndrsrcdir)/py_irpc.o

[PYTHON::python_drsuapi]
LIBRARY_REALNAME = samba/dcerpc/drsuapi.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = RPC_NDR_DRSUAPI PYTALLOC pyparam_util pycredentials python_dcerpc

python_drsuapi_OBJ_FILES = ../librpc/gen_ndr/py_drsuapi.o

[PYTHON::python_dcerpc_security]
LIBRARY_REALNAME = samba/dcerpc/security.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = PYTALLOC python_dcerpc_misc python_dcerpc NDR_SECURITY

python_dcerpc_security_OBJ_FILES = ../librpc/gen_ndr/py_security.o

$(IDL_HEADER_FILES) $(IDL_NDR_PARSE_H_FILES) $(IDL_NDR_PARSE_C_FILES) \
	$(IDL_NDR_CLIENT_C_FILES) $(IDL_NDR_CLIENT_H_FILES) \
	$(IDL_NDR_SERVER_C_FILES) $(IDL_SWIG_FILES) \
	$(IDL_NDR_PY_C_FILES) $(IDL_NDR_PY_H_FILES): idl

idl_full:: $(pidldir)/lib/Parse/Pidl/IDL.pm $(pidldir)/lib/Parse/Pidl/Expr.pm 
	@PIDL_OUTPUTDIR="../librpc/gen_ndr" PIDL_ARGS="$(PIDL_ARGS)" CPP="$(CPP)" srcdir="$(srcdir)" PIDL="$(PIDL)" ../librpc/build_idl.sh --full ../librpc/idl/*.idl
	@CPP="$(CPP)" PIDL="$(PIDL)" $(librpcsrcdir)/scripts/build_idl.sh FULL $(librpcsrcdir)/gen_ndr $(librpcsrcdir)/idl/*.idl

idl:: $(pidldir)/lib/Parse/Pidl/IDL.pm $(pidldir)/lib/Parse/Pidl/Expr.pm 
	@PIDL_OUTPUTDIR="../librpc/gen_ndr" PIDL_ARGS="$(PIDL_ARGS)" CPP="$(CPP)" srcdir="$(srcdir)" PIDL="$(PIDL)" ../librpc/build_idl.sh ../librpc/idl/*.idl
	@CPP="$(CPP)" PIDL="$(PIDL)" $(librpcsrcdir)/scripts/build_idl.sh PARTIAL $(librpcsrcdir)/gen_ndr $(librpcsrcdir)/idl/*.idl

clean::
	@echo "Remove ../librpc/gen_ndr files which are not commited to git"
	@cat ../.gitignore | grep "^librpc/gen_ndr" | xargs rm -f
