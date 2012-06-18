mkinclude ldap/config.mk
mkinclude security/config.mk
mkinclude wbclient/config.mk

[SUBSYSTEM::LIBSAMBA-ERRORS]

LIBSAMBA-ERRORS_OBJ_FILES = $(addprefix ../libcli/util/, doserr.o ) $(libclisrcdir)/util/errormap.o $(libclisrcdir)/util/nterr.o

PUBLIC_HEADERS += $(addprefix ../libcli/util/, error.h ntstatus.h doserr.h werror.h)

[SUBSYSTEM::LIBCLI_LSA]
PUBLIC_DEPENDENCIES = RPC_NDR_LSA
PRIVATE_DEPENDENCIES = LIBSECURITY

LIBCLI_LSA_OBJ_FILES = $(libclisrcdir)/util/clilsa.o

$(eval $(call proto_header_template,$(libclisrcdir)/util/clilsa.h,$(LIBCLI_LSA_OBJ_FILES:.o=.c)))

[SUBSYSTEM::LIBCLI_COMPOSITE]
PUBLIC_DEPENDENCIES = LIBEVENTS

LIBCLI_COMPOSITE_OBJ_FILES = $(libclisrcdir)/composite/composite.o
$(eval $(call proto_header_template,$(libclisrcdir)/composite/proto.h,$(LIBCLI_COMPOSITE_OBJ_FILES:.o=.c)))

[SUBSYSTEM::LIBCLI_SMB_COMPOSITE]
PUBLIC_DEPENDENCIES = LIBCLI_COMPOSITE CREDENTIALS gensec LIBCLI_RESOLVE

LIBCLI_SMB_COMPOSITE_OBJ_FILES = $(addprefix $(libclisrcdir)/smb_composite/, \
	loadfile.o \
	savefile.o \
	connect.o \
	sesssetup.o \
	fetchfile.o \
	appendacl.o \
	fsinfo.o \
	smb2.o)

$(eval $(call proto_header_template,$(libclisrcdir)/smb_composite/proto.h,$(LIBCLI_SMB_COMPOSITE_OBJ_FILES:.o=.c)))

[PYTHON::python_netbios]
LIBRARY_REALNAME = samba/netbios.$(SHLIBEXT)
PUBLIC_DEPENDENCIES = LIBCLI_NBT DYNCONFIG LIBSAMBA-HOSTCONFIG

python_netbios_OBJ_FILES = $(libclinbtsrcdir)/pynbt.o

[SUBSYSTEM::LIBCLI_DGRAM]
PUBLIC_DEPENDENCIES = LIBCLI_NBT LIBNDR LIBCLI_RESOLVE LIBCLI_NETLOGON

LIBCLI_DGRAM_OBJ_FILES = $(addprefix $(libclisrcdir)/dgram/, \
	dgramsocket.o \
	mailslot.o \
	netlogon.o \
	browse.o)

[SUBSYSTEM::LIBCLI_WREPL]
PUBLIC_DEPENDENCIES = NDR_WINSREPL samba_socket LIBEVENTS LIBPACKET

LIBCLI_WREPL_OBJ_FILES = $(libclisrcdir)/wrepl/winsrepl.o

$(eval $(call proto_header_template,$(libclisrcdir)/wrepl/winsrepl_proto.h,$(LIBCLI_WREPL_OBJ_FILES:.o=.c)))

[SUBSYSTEM::LIBCLI_RESOLVE]
PUBLIC_DEPENDENCIES = NDR_NBT

LIBCLI_RESOLVE_OBJ_FILES = $(libclisrcdir)/resolve/resolve.o

$(eval $(call proto_header_template,$(libclisrcdir)/resolve/proto.h,$(LIBCLI_RESOLVE_OBJ_FILES:.o=.c)))

[SUBSYSTEM::LP_RESOLVE]
PRIVATE_DEPENDENCIES = LIBCLI_NBT LIBSAMBA-HOSTCONFIG LIBNETIF 

LP_RESOLVE_OBJ_FILES = $(addprefix $(libclisrcdir)/resolve/, \
					  bcast.o nbtlist.o wins.o \
					  dns_ex.o \
					  host.o resolve_lp.o)

$(eval $(call proto_header_template,$(libclisrcdir)/resolve/lp_proto.h,$(LP_RESOLVE_OBJ_FILES:.o=.c)))

[SUBSYSTEM::LIBCLI_FINDDCS]
PUBLIC_DEPENDENCIES = LIBCLI_NBT MESSAGING

LIBCLI_FINDDCS_OBJ_FILES = $(libclisrcdir)/finddcs.o

$(eval $(call proto_header_template,$(libclisrcdir)/finddcs.h,$(LIBCLI_FINDDCS_OBJ_FILES:.o=.c)))

[SUBSYSTEM::LIBCLI_SMB]
PUBLIC_DEPENDENCIES = LIBCLI_RAW LIBSAMBA-ERRORS LIBCLI_AUTH \
	LIBCLI_SMB_COMPOSITE LIBCLI_NBT LIBSECURITY LIBCLI_RESOLVE \
	LIBCLI_DGRAM LIBCLI_SMB2 LIBCLI_FINDDCS samba_socket

LIBCLI_SMB_OBJ_FILES = $(addprefix $(libclisrcdir)/, \
		clireadwrite.o \
		cliconnect.o \
		clifile.o \
		clilist.o \
		clitrans2.o \
		climessage.o \
		clideltree.o)

$(eval $(call proto_header_template,$(libclisrcdir)/libcli_proto.h,$(LIBCLI_SMB_OBJ_FILES:.o=.c)))

# PUBLIC_HEADERS += $(libclisrcdir)/libcli.h

[SUBSYSTEM::LIBCLI_RAW]
PRIVATE_DEPENDENCIES = LIBCLI_COMPOSITE LP_RESOLVE gensec LIBCLI_RESOLVE LIBSECURITY LIBNDR
#LDFLAGS = $(LIBCLI_SMB_COMPOSITE_OUTPUT)
PUBLIC_DEPENDENCIES = samba_socket LIBPACKET gensec LIBCRYPTO CREDENTIALS LIBCLI_SMB_COMMON

LIBCLI_RAW_OBJ_FILES = $(addprefix $(libclisrcdir)/raw/, rawfile.o smb_signing.o clisocket.o \
					  clitransport.o clisession.o clitree.o clierror.o rawrequest.o \
					  rawreadwrite.o rawsearch.o rawsetfileinfo.o raweas.o rawtrans.o \
					  clioplock.o rawnegotiate.o rawfsinfo.o rawfileinfo.o rawnotify.o \
					  rawioctl.o rawacl.o rawdate.o rawlpq.o rawshadow.o)


$(eval $(call proto_header_template,$(libclisrcdir)/raw/raw_proto.h,$(LIBCLI_RAW_OBJ_FILES:.o=.c)))

mkinclude smb2/config.mk
