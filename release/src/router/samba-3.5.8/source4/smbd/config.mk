# server subsystem

[SUBSYSTEM::service]
PRIVATE_DEPENDENCIES = \
		LIBTEVENT MESSAGING samba_socket \
		NDR_NAMED_PIPE_AUTH NAMED_PIPE_AUTH_TSTREAM \
		HEIMDAL_GSSAPI CREDENTIALS

service_OBJ_FILES = $(addprefix $(smbdsrcdir)/, \
		service.o \
		service_stream.o \
		service_named_pipe.o \
		service_task.o)

$(eval $(call proto_header_template,$(smbdsrcdir)/service_proto.h,$(service_OBJ_FILES:.o=.c)))

[SUBSYSTEM::PIDFILE]

PIDFILE_OBJ_FILES = $(smbdsrcdir)/pidfile.o

$(eval $(call proto_header_template,$(smbdsrcdir)/pidfile.h,$(PIDFILE_OBJ_FILES:.o=.c)))

[BINARY::samba]
INSTALLDIR = SBINDIR
PRIVATE_DEPENDENCIES = \
		LIBEVENTS \
		process_model \
		service \
		LIBSAMBA-HOSTCONFIG \
		LIBSAMBA-UTIL \
		POPT_SAMBA \
		PIDFILE \
		LIBPOPT \
		gensec \
		registry \
		ntptr \
		ntvfs \
		share \
		CLUSTER

samba_OBJ_FILES = $(smbdsrcdir)/server.o
$(samba_OBJ_FILES): CFLAGS+=-DSTATIC_service_MODULES="$(service_INIT_FUNCTIONS)NULL"

MANPAGES += $(smbdsrcdir)/samba.8
