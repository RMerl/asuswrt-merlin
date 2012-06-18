[SUBSYSTEM::LIBWINBIND-CLIENT]
PRIVATE_DEPENDENCIES = SOCKET_WRAPPER

LIBWINBIND-CLIENT_OBJ_FILES = $(nsswitchsrcdir)/wb_common.o
$(LIBWINBIND-CLIENT_OBJ_FILES): CFLAGS+=-DWINBINDD_SOCKET_DIR=\"$(winbindd_socket_dir)\"

#################################
# Start BINARY nsstest
[BINARY::nsstest]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBSAMBA-UTIL \
		LIBREPLACE_EXT \
		LIBSAMBA-HOSTCONFIG
# End BINARY nsstest
#################################

nsstest_OBJ_FILES = $(nsswitchsrcdir)/nsstest.o

#################################
# Start BINARY wbinfo
[BINARY::wbinfo]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBSAMBA-UTIL \
		LIBREPLACE_EXT \
		LIBCLI_AUTH \
		LIBPOPT \
		POPT_SAMBA \
		LIBWINBIND-CLIENT \
		LIBWBCLIENT \
		LIBTEVENT \
		UTIL_TEVENT \
		LIBASYNC_REQ \
		UID_WRAPPER
# End BINARY nsstest
#################################

wbinfo_OBJ_FILES = \
		$(nsswitchsrcdir)/wbinfo.o
$(wbinfo_OBJ_FILES): CFLAGS+=-DWINBINDD_SOCKET_DIR=\"$(winbindd_socket_dir)\"
