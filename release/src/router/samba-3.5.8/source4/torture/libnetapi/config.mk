#################################
# Start SUBSYSTEM TORTURE_LIBNETAPI
[MODULE::TORTURE_LIBNETAPI]
SUBSYSTEM = smbtorture
OUTPUT_TYPE = MERGED_OBJ
INIT_FUNCTION = torture_libnetapi_init
PRIVATE_DEPENDENCIES = \
		POPT_CREDENTIALS \
		NETAPI
# End SUBSYSTEM TORTURE_LIBNETAPI
#################################

TORTURE_LIBNETAPI_OBJ_FILES = $(addprefix $(torturesrcdir)/libnetapi/, libnetapi.o \
					libnetapi_user.o \
					libnetapi_group.o)

$(eval $(call proto_header_template,$(torturesrcdir)/libnetapi/proto.h,$(TORTURE_LIBNETAPI_OBJ_FILES:.o=.c)))
