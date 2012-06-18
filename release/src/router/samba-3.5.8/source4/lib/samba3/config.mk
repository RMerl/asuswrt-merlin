################################################
# Start SUBSYSTEM LIBSAMBA3
[SUBSYSTEM::SMBPASSWD]
PRIVATE_DEPENDENCIES = CHARSET LIBSAMBA-UTIL
# End SUBSYSTEM LIBSAMBA3
################################################

SMBPASSWD_OBJ_FILES = $(libsrcdir)/samba3/smbpasswd.o
