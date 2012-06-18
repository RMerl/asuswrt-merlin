# client subsystem

#################################
# Start BINARY smbclient
[BINARY::smbclient]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBSAMBA-HOSTCONFIG \
		SMBREADLINE \
		LIBSAMBA-UTIL \
		LIBCLI_SMB \
		RPC_NDR_SRVSVC \
		LIBCLI_LSA \
		LIBPOPT \
		POPT_SAMBA \
		POPT_CREDENTIALS \
		LIBCLI_RAW 
# End BINARY smbclient
#################################

smbclient_OBJ_FILES = $(clientsrcdir)/client.o

[BINARY::mount.cifs]
INSTALLDIR = BINDIR

mount.cifs_OBJ_FILES = ../client/mount.cifs.o \
					   ../client/mtab.o

[BINARY::umount.cifs]
INSTALLDIR = BINDIR

umount.cifs_OBJ_FILES = ../client/umount.cifs.o \
					   ../client/mtab.o

#[BINARY::cifs.upcall]
#INSTALLDIR = BINDIR
#cifs.upcall_OBJ_FILES = ../client/cifs.upcall.o

#################################
# Start BINARY cifsdd
[BINARY::cifsdd]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBSAMBA-HOSTCONFIG \
		LIBCLI_SMB \
		LIBPOPT \
		POPT_SAMBA \
		POPT_CREDENTIALS
# End BINARY sdd
#################################

cifsdd_OBJ_FILES = $(addprefix $(clientsrcdir)/, cifsdd.o cifsddio.o)
