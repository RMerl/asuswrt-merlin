################################################
# Start LIBRARY ntvfs_common
[SUBSYSTEM::ntvfs_common]
PUBLIC_DEPENDENCIES = NDR_OPENDB NDR_NOTIFY sys_notify sys_lease share
# End LIBRARY ntvfs_common
################################################

ntvfs_common_OBJ_FILES = $(addprefix $(ntvfssrcdir)/common/, init.o brlock.o brlock_tdb.o opendb.o opendb_tdb.o notify.o)

$(eval $(call proto_header_template,$(ntvfssrcdir)/common/proto.h,$(ntvfs_common_OBJ_FILES:.o=.c)))

