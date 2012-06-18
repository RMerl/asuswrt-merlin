
[SUBSYSTEM::pvfs_acl]

pvfs_acl_OBJ_FILES = $(ntvfssrcdir)/posix/pvfs_acl.o

$(eval $(call proto_header_template,$(ntvfssrcdir)/posix/vfs_acl_proto.h,$(pvfs_acl_OBJ_FILES:.o=.c)))

################################################
# Start MODULE pvfs_acl_xattr
[MODULE::pvfs_acl_xattr]
INIT_FUNCTION = pvfs_acl_xattr_init 
SUBSYSTEM = pvfs_acl
PRIVATE_DEPENDENCIES = NDR_XATTR
# End MODULE pvfs_acl_xattr
################################################

pvfs_acl_xattr_OBJ_FILES = $(ntvfssrcdir)/posix/pvfs_acl_xattr.o

################################################
# Start MODULE pvfs_acl_nfs4
[MODULE::pvfs_acl_nfs4]
INIT_FUNCTION = pvfs_acl_nfs4_init 
SUBSYSTEM = pvfs_acl
PRIVATE_DEPENDENCIES = NDR_NFS4ACL SAMDB
# End MODULE pvfs_acl_nfs4
################################################

pvfs_acl_nfs4_OBJ_FILES = $(ntvfssrcdir)/posix/pvfs_acl_nfs4.o

################################################
[SUBSYSTEM::pvfs_aio]
PRIVATE_DEPENDENCIES = LIBTEVENT LIBTEVENT_EXT
################################################

pvfs_aio_OBJ_FILES = $(ntvfssrcdir)/posix/pvfs_aio.o

################################################
# Start MODULE ntvfs_posix
[MODULE::ntvfs_posix]
SUBSYSTEM = ntvfs
OUTPUT_TYPE = MERGED_OBJ
INIT_FUNCTION = ntvfs_posix_init 
#PRIVATE_DEPENDENCIES = pvfs_acl_xattr pvfs_acl_nfs4
PRIVATE_DEPENDENCIES = NDR_XATTR WRAP_XATTR BLKID ntvfs_common MESSAGING \
			LIBWBCLIENT_OLD pvfs_acl pvfs_aio
# End MODULE ntvfs_posix
################################################

ntvfs_posix_OBJ_FILES = $(addprefix $(ntvfssrcdir)/posix/, \
		vfs_posix.o \
		pvfs_util.o \
		pvfs_search.o \
		pvfs_dirlist.o \
		pvfs_fileinfo.o \
		pvfs_unlink.o \
		pvfs_mkdir.o \
		pvfs_open.o \
		pvfs_read.o \
		pvfs_flush.o \
		pvfs_write.o \
		pvfs_fsinfo.o \
		pvfs_qfileinfo.o \
		pvfs_setfileinfo.o \
		pvfs_rename.o \
		pvfs_resolve.o \
		pvfs_shortname.o \
		pvfs_lock.o \
		pvfs_oplock.o \
		pvfs_wait.o \
		pvfs_seek.o \
		pvfs_ioctl.o \
		pvfs_xattr.o \
		pvfs_streams.o \
		pvfs_notify.o \
		xattr_system.o \
		xattr_tdb.o)

$(eval $(call proto_header_template,$(ntvfssrcdir)/posix/vfs_posix_proto.h,$(ntvfs_posix_OBJ_FILES:.o=.c)))

