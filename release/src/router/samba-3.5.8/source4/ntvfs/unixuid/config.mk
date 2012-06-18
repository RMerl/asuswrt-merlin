################################################
# Start MODULE ntvfs_unixuid
[MODULE::ntvfs_unixuid]
INIT_FUNCTION = ntvfs_unixuid_init
SUBSYSTEM = ntvfs
PRIVATE_DEPENDENCIES = SAMDB NSS_WRAPPER UID_WRAPPER
# End MODULE ntvfs_unixuid
################################################

ntvfs_unixuid_OBJ_FILES = $(ntvfssrcdir)/unixuid/vfs_unixuid.o
