#######################
# Start SUBSYSTEM SMB2_PROTOCOL
[SUBSYSTEM::SMB2_PROTOCOL]
PUBLIC_DEPENDENCIES = \
		ntvfs LIBPACKET LIBCLI_SMB2 samba_server_gensec
# End SUBSYSTEM SMB2_PROTOCOL
#######################

SMB2_PROTOCOL_OBJ_FILES = $(addprefix $(smb_serversrcdir)/smb2/, \
		receive.o \
		negprot.o \
		sesssetup.o \
		tcon.o \
		fileio.o \
		fileinfo.o \
		find.o \
		keepalive.o)

$(eval $(call proto_header_template,$(smb_serversrcdir)/smb2/smb2_proto.h,$(SMB2_PROTOCOL_OBJ_FILES:.o=.c)))
