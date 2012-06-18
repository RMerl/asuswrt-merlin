[SUBSYSTEM::LIBCLI_SMB2]
PUBLIC_DEPENDENCIES = LIBCLI_RAW LIBPACKET gensec

LIBCLI_SMB2_OBJ_FILES = $(addprefix $(libclisrcdir)/smb2/, \
	transport.o request.o negprot.o session.o tcon.o \
	create.o close.o connect.o getinfo.o write.o read.o \
	setinfo.o find.o ioctl.o logoff.o tdis.o flush.o \
	lock.o notify.o cancel.o keepalive.o break.o util.o signing.o \
	lease_break.o)

$(eval $(call proto_header_template,$(libclisrcdir)/smb2/smb2_proto.h,$(LIBCLI_SMB2_OBJ_FILES:.o=.c)))
