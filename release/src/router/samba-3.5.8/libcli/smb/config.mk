# common SMB and SMB2 stuff
[SUBSYSTEM::LIBCLI_SMB_COMMON]
PUBLIC_DEPENDENCIES = LIBTALLOC

LIBCLI_SMB_COMMON_OBJ_FILES = $(addprefix ../libcli/smb/, \
		smb2_create_blob.o)

$(eval $(call proto_header_template, \
	../libcli/smb/smb_common_proto.h, \
	$(LIBCLI_SMB_COMMON_OBJ_FILES:.o=.c)))

