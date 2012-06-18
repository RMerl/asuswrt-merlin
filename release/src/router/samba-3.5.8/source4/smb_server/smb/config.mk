#######################
# Start SUBSYSTEM SMB_PROTOCOL
[SUBSYSTEM::SMB_PROTOCOL]
PUBLIC_DEPENDENCIES = \
		ntvfs LIBPACKET CREDENTIALS samba_server_gensec
# End SUBSYSTEM SMB_PROTOCOL
#######################

SMB_PROTOCOL_OBJ_FILES = $(addprefix $(smb_serversrcdir)/smb/, \
		receive.o \
		negprot.o \
		nttrans.o \
		reply.o \
		request.o \
		search.o \
		service.o \
		sesssetup.o \
		srvtime.o \
		trans2.o \
		signing.o)

$(eval $(call proto_header_template,$(smb_serversrcdir)/smb/smb_proto.h,$(SMB_PROTOCOL_OBJ_FILES:.o=.c)))
