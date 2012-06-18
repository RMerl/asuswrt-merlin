
#################################
# Start SUBSYSTEM TORTURE_WINBIND
[MODULE::TORTURE_WINBIND]
SUBSYSTEM = smbtorture
OUTPUT_TYPE = MERGED_OBJ
INIT_FUNCTION = torture_winbind_init
PRIVATE_DEPENDENCIES = \
		LIBWBCLIENT LIBWINBIND-CLIENT torture PAM_ERRORS
# End SUBSYSTEM TORTURE_WINBIND
#################################

TORTURE_WINBIND_OBJ_FILES = $(addprefix $(torturesrcdir)/winbind/, winbind.o struct_based.o) ../nsswitch/libwbclient/tests/wbclient.o

$(eval $(call proto_header_template,$(torturesrcdir)/winbind/proto.h,$(TORTURE_WINBIND_OBJ_FILES:.o=.c)))

