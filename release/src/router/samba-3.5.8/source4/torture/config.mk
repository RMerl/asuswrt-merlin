[SUBSYSTEM::TORTURE_UTIL]
PRIVATE_DEPENDENCIES = LIBCLI_RAW
PUBLIC_DEPENDENCIES = torture POPT_CREDENTIALS

TORTURE_UTIL_OBJ_FILES = $(addprefix $(torturesrcdir)/, util_smb.o)

#################################
# Start SUBSYSTEM TORTURE_BASIC
[MODULE::TORTURE_BASIC]
SUBSYSTEM = smbtorture
INIT_FUNCTION = torture_base_init
OUTPUT_TYPE = MERGED_OBJ
PRIVATE_DEPENDENCIES = \
		LIBCLI_SMB POPT_CREDENTIALS \
		TORTURE_UTIL LIBCLI_RAW \
		TORTURE_RAW
# End SUBSYSTEM TORTURE_BASIC
#################################

TORTURE_BASIC_OBJ_FILES = $(addprefix $(torturesrcdir)/basic/,  \
		base.o \
		misc.o \
		scanner.o \
		utable.o \
		charset.o \
		mangle_test.o \
		denytest.o \
		aliases.o \
		locking.o \
		secleak.o \
		rename.o \
		dir.o \
		delete.o \
		unlink.o \
		disconnect.o \
		delaywrite.o \
		attr.o \
		properties.o)

$(eval $(call proto_header_template,$(torturesrcdir)/basic/proto.h,$(TORTURE_BASIC_OBJ_FILES:.o=.c)))

#################################
# Start SUBSYSTEM TORTURE_RAW
[MODULE::TORTURE_RAW]
OUTPUT_TYPE = MERGED_OBJ
SUBSYSTEM = smbtorture
INIT_FUNCTION = torture_raw_init
PRIVATE_DEPENDENCIES = \
		LIBCLI_SMB LIBCLI_LSA LIBCLI_SMB_COMPOSITE \
		POPT_CREDENTIALS TORTURE_UTIL
# End SUBSYSTEM TORTURE_RAW
#################################

TORTURE_RAW_OBJ_FILES = $(addprefix $(torturesrcdir)/raw/, \
		qfsinfo.o \
		qfileinfo.o \
		setfileinfo.o \
		search.o \
		close.o \
		open.o \
		mkdir.o \
		oplock.o \
		notify.o \
		mux.o \
		ioctl.o \
		chkpath.o \
		unlink.o \
		read.o \
		context.o \
		write.o \
		lock.o \
		pingpong.o \
		lockbench.o \
		lookuprate.o \
		tconrate.o \
		openbench.o \
		rename.o \
		eas.o \
		streams.o \
		acls.o \
		seek.o \
		samba3hide.o \
		samba3misc.o \
		composite.o \
		raw.o \
		offline.o)

$(eval $(call proto_header_template,$(torturesrcdir)/raw/proto.h,$(TORTURE_RAW_OBJ_FILES:.o=.c)))

mkinclude smb2/config.mk
mkinclude winbind/config.mk
mkinclude libnetapi/config.mk

[SUBSYSTEM::TORTURE_NDR]
PRIVATE_DEPENDENCIES = torture SERVICE_SMB

TORTURE_NDR_OBJ_FILES = $(addprefix $(torturesrcdir)/ndr/, ndr.o winreg.o atsvc.o lsa.o epmap.o dfs.o netlogon.o drsuapi.o spoolss.o samr.o)

$(eval $(call proto_header_template,$(torturesrcdir)/ndr/proto.h,$(TORTURE_NDR_OBJ_FILES:.o=.c)))

[MODULE::torture_rpc]
OUTPUT_TYPE = MERGED_OBJ
# TORTURE_NET and TORTURE_NBT use functions from torture_rpc...
#OUTPUT_TYPE = MERGED_OBJ
SUBSYSTEM = smbtorture
INIT_FUNCTION = torture_rpc_init
PRIVATE_DEPENDENCIES = \
		NDR_TABLE RPC_NDR_UNIXINFO dcerpc_samr RPC_NDR_WINREG RPC_NDR_INITSHUTDOWN \
		RPC_NDR_OXIDRESOLVER RPC_NDR_EVENTLOG RPC_NDR_ECHO RPC_NDR_SVCCTL \
		RPC_NDR_NETLOGON dcerpc_atsvc dcerpc_mgmt RPC_NDR_DRSUAPI \
		RPC_NDR_LSA RPC_NDR_EPMAPPER RPC_NDR_DFS RPC_NDR_FRSAPI RPC_NDR_SPOOLSS \
		RPC_NDR_SRVSVC RPC_NDR_WKSSVC RPC_NDR_ROT RPC_NDR_DSSETUP \
		RPC_NDR_REMACT RPC_NDR_OXIDRESOLVER RPC_NDR_NTSVCS WB_HELPER LIBSAMBA-NET \
		LIBCLI_AUTH POPT_CREDENTIALS TORTURE_LDAP TORTURE_UTIL TORTURE_RAP \
		dcerpc_server service process_model ntvfs SERVICE_SMB RPC_NDR_BROWSER LIBCLI_DRSUAPI TORTURE_LDB_MODULE

torture_rpc_OBJ_FILES = $(addprefix $(torturesrcdir)/rpc/, \
		join.o lsa.o lsa_lookup.o session_key.o echo.o dfs.o drsuapi.o \
		drsuapi_cracknames.o dssync.o spoolss.o spoolss_notify.o spoolss_win.o \
		unixinfo.o samr.o samr_accessmask.o wkssvc.o srvsvc.o svcctl.o atsvc.o \
		eventlog.o epmapper.o winreg.o initshutdown.o oxidresolve.o remact.o mgmt.o \
		scanner.o autoidl.o countcalls.o testjoin.o schannel.o netlogon.o remote_pac.o samlogon.o \
		samsync.o bind.o dssetup.o alter_context.o bench.o samba3rpc.o rpc.o async_bind.o \
		handles.o frsapi.o object_uuid.o ntsvcs.o browser.o)

$(eval $(call proto_header_template,$(torturesrcdir)/rpc/proto.h,$(torture_rpc_OBJ_FILES:.o=.c)))

#################################
# Start SUBSYSTEM TORTURE_RAP
[MODULE::TORTURE_RAP]
OUTPUT_TYPE = MERGED_OBJ
SUBSYSTEM = smbtorture
INIT_FUNCTION = torture_rap_init
PRIVATE_DEPENDENCIES = TORTURE_UTIL LIBCLI_SMB
# End SUBSYSTEM TORTURE_RAP
#################################

TORTURE_RAP_OBJ_FILES = $(torturesrcdir)/rap/rap.o

$(eval $(call proto_header_template,$(torturesrcdir)/rap/proto.h,$(TORTURE_RAP_OBJ_FILES:.o=.c)))

#################################
# Start SUBSYSTEM TORTURE_AUTH
[MODULE::TORTURE_AUTH]
OUTPUT_TYPE = MERGED_OBJ
SUBSYSTEM = smbtorture
PRIVATE_DEPENDENCIES = \
		LIBCLI_SMB gensec auth KERBEROS \
		POPT_CREDENTIALS SMBPASSWD torture
# End SUBSYSTEM TORTURE_AUTH
#################################

TORTURE_AUTH_OBJ_FILES = $(addprefix $(torturesrcdir)/auth/, ntlmssp.o pac.o)

$(eval $(call proto_header_template,$(torturesrcdir)/auth/proto.h,$(TORTURE_AUTH_OBJ_FILES:.o=.c)))

mkinclude local/config.mk

#################################
# Start MODULE TORTURE_NBENCH
[MODULE::TORTURE_NBENCH]
OUTPUT_TYPE = MERGED_OBJ
SUBSYSTEM = smbtorture
INIT_FUNCTION = torture_nbench_init
PRIVATE_DEPENDENCIES = TORTURE_UTIL 
# End MODULE TORTURE_NBENCH
#################################

TORTURE_NBENCH_OBJ_FILES = $(addprefix $(torturesrcdir)/nbench/, nbio.o nbench.o)

$(eval $(call proto_header_template,$(torturesrcdir)/nbench/proto.h,$(TORTURE_NBENCH_OBJ_FILES:.o=.c)))

#################################
# Start MODULE TORTURE_UNIX
[MODULE::TORTURE_UNIX]
SUBSYSTEM = smbtorture
OUTPUT_TYPE = MERGED_OBJ
INIT_FUNCTION = torture_unix_init
PRIVATE_DEPENDENCIES = TORTURE_UTIL 
# End MODULE TORTURE_UNIX
#################################

TORTURE_UNIX_OBJ_FILES = $(addprefix $(torturesrcdir)/unix/, unix.o whoami.o unix_info2.o)

$(eval $(call proto_header_template,$(torturesrcdir)/unix/proto.h,$(TORTURE_UNIX_OBJ_FILES:.o=.c)))

#################################
# Start SUBSYSTEM TORTURE_LDAP
[MODULE::TORTURE_LDAP]
SUBSYSTEM = smbtorture
OUTPUT_TYPE = MERGED_OBJ
INIT_FUNCTION = torture_ldap_init
PRIVATE_DEPENDENCIES = \
		LIBCLI_LDAP LIBCLI_CLDAP SAMDB POPT_CREDENTIALS torture LDB_WRAP
# End SUBSYSTEM TORTURE_LDAP
#################################

TORTURE_LDAP_OBJ_FILES = $(addprefix $(torturesrcdir)/ldap/, common.o basic.o schema.o uptodatevector.o cldap.o cldapbench.o ldap_sort.o)

$(eval $(call proto_header_template,$(torturesrcdir)/ldap/proto.h,$(TORTURE_LDAP_OBJ_FILES:.o=.c)))

#################################
# Start SUBSYSTEM TORTURE_NBT
[MODULE::TORTURE_NBT]
SUBSYSTEM = smbtorture
OUTPUT_TYPE = MERGED_OBJ
INIT_FUNCTION = torture_nbt_init
PRIVATE_DEPENDENCIES = \
		LIBCLI_SMB LIBCLI_NBT LIBCLI_DGRAM LIBCLI_WREPL torture_rpc
# End SUBSYSTEM TORTURE_NBT
#################################

TORTURE_NBT_OBJ_FILES = $(addprefix $(torturesrcdir)/nbt/, query.o register.o \
	wins.o winsbench.o winsreplication.o dgram.o nbt.o)

$(eval $(call proto_header_template,$(torturesrcdir)/nbt/proto.h,$(TORTURE_NBT_OBJ_FILES:.o=.c)))

#################################
# Start SUBSYSTEM TORTURE_NET
[MODULE::TORTURE_NET]
SUBSYSTEM = smbtorture
OUTPUT_TYPE = MERGED_OBJ
INIT_FUNCTION = torture_net_init
PRIVATE_DEPENDENCIES = \
		LIBSAMBA-NET \
		POPT_CREDENTIALS \
		torture_rpc \
		PROVISION
# End SUBSYSTEM TORTURE_NET
#################################

TORTURE_NET_OBJ_FILES = $(addprefix $(torturesrcdir)/libnet/, libnet.o \
					   utils.o userinfo.o userman.o groupinfo.o groupman.o \
					   domain.o libnet_lookup.o libnet_user.o libnet_group.o \
					   libnet_share.o libnet_rpc.o libnet_domain.o libnet_BecomeDC.o)

$(eval $(call proto_header_template,$(torturesrcdir)/libnet/proto.h,$(TORTURE_NET_OBJ_FILES:.o=.c)))

#################################
# Start SUBSYSTEM TORTURE_NTP
[MODULE::TORTURE_NTP]
SUBSYSTEM = smbtorture
OUTPUT_TYPE = MERGED_OBJ
INIT_FUNCTION = torture_ntp_init
PRIVATE_DEPENDENCIES = \
		POPT_CREDENTIALS \
		torture_rpc 
# End SUBSYSTEM TORTURE_NTP
#################################

TORTURE_NTP_OBJ_FILES = $(addprefix $(torturesrcdir)/ntp/, ntp_signd.o)

$(eval $(call proto_header_template,$(torturesrcdir)/ntp/proto.h,$(TORTURE_NET_OBJ_FILES:.o=.c)))

#################################
# Start BINARY smbtorture
[BINARY::smbtorture]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		torture \
		LIBPOPT \
		POPT_SAMBA \
		POPT_CREDENTIALS \
		dcerpc \
		LIBCLI_SMB \
		SMBREADLINE
# End BINARY smbtorture
#################################

smbtorture_OBJ_FILES = $(torturesrcdir)/smbtorture.o $(torturesrcdir)/torture.o 

PUBLIC_HEADERS += $(torturesrcdir)/smbtorture.h
MANPAGES += $(torturesrcdir)/man/smbtorture.1

#################################
# Start BINARY gentest
[BINARY::gentest]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBSAMBA-HOSTCONFIG \
		LIBSAMBA-UTIL \
		LIBPOPT \
		POPT_SAMBA \
		POPT_CREDENTIALS \
		LIBCLI_SMB \
		LIBCLI_RAW
# End BINARY gentest
#################################

gentest_OBJ_FILES = $(torturesrcdir)/gentest.o

MANPAGES += $(torturesrcdir)/man/gentest.1

#################################
# Start BINARY masktest
[BINARY::masktest]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBSAMBA-HOSTCONFIG \
		LIBSAMBA-UTIL \
		LIBPOPT \
		POPT_SAMBA \
		POPT_CREDENTIALS \
		LIBCLI_SMB
# End BINARY masktest
#################################

masktest_OBJ_FILES = $(torturesrcdir)/masktest.o

MANPAGES += $(torturesrcdir)/man/masktest.1

#################################
# Start BINARY locktest
[BINARY::locktest]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBPOPT \
		POPT_SAMBA \
		POPT_CREDENTIALS \
		LIBSAMBA-UTIL \
		LIBCLI_SMB \
		LIBSAMBA-HOSTCONFIG
# End BINARY locktest
#################################

locktest_OBJ_FILES = $(torturesrcdir)/locktest.o

MANPAGES += $(torturesrcdir)/man/locktest.1

GCOV=0

ifeq ($(MAKECMDGOALS),gcov)
GCOV=1
endif

ifeq ($(MAKECMDGOALS),lcov)
GCOV=1
endif

ifeq ($(MAKECMDGOALS),testcov-html)
GCOV=1
endif

ifeq ($(GCOV),1)
CFLAGS += --coverage
LDFLAGS += --coverage
endif

COV_TARGET = test

gcov: test
	for I in $(sort $(dir $(ALL_OBJS))); \
		do $(GCOV) -p -o $$I $$I/*.c; \
	done

samba4.info: test
	-rm heimdal/lib/*/{lex,parse,sel-lex}.{gcda,gcno}
	cd .. && lcov --base-directory `pwd`/source4 --directory source4 --directory nsswitch --directory libcli --directory librpc --directory lib --capture --output-file source4/samba4.info

lcov: samba4.info
	genhtml -o coverage $<

testcov-html:: lcov

clean::
	@rm -f samba.info
