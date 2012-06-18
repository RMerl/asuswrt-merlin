[SUBSYSTEM::LIBSAMBA-NET]
PUBLIC_DEPENDENCIES = CREDENTIALS dcerpc dcerpc_samr RPC_NDR_LSA RPC_NDR_SRVSVC RPC_NDR_DRSUAPI LIBCLI_COMPOSITE LIBCLI_RESOLVE LIBCLI_FINDDCS LIBCLI_CLDAP LIBCLI_FINDDCS gensec_schannel LIBCLI_AUTH LIBNDR SMBPASSWD PROVISION LIBCLI_SAMSYNC HDB_SAMBA4

LIBSAMBA-NET_OBJ_FILES = $(addprefix $(libnetsrcdir)/, \
	libnet.o libnet_passwd.o libnet_time.o libnet_rpc.o \
	libnet_join.o libnet_site.o libnet_become_dc.o libnet_unbecome_dc.o \
	libnet_vampire.o libnet_samdump.o libnet_samdump_keytab.o \
	libnet_samsync_ldb.o libnet_user.o libnet_group.o libnet_share.o \
	libnet_lookup.o libnet_domain.o userinfo.o groupinfo.o userman.o \
	groupman.o prereq_domain.o libnet_samsync.o libnet_export_keytab.o)

$(eval $(call proto_header_template,$(libnetsrcdir)/libnet_proto.h,$(LIBSAMBA-NET_OBJ_FILES:.o=.c)))

[PYTHON::python_net]
LIBRARY_REALNAME = samba/net.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = LIBSAMBA-NET

python_net_OBJ_FILES = $(libnetsrcdir)/py_net.o
