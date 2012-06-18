# $(utilssrcdir)/net subsystem

#################################
# Start BINARY net
[BINARY::net]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBSAMBA-HOSTCONFIG \
		LIBSAMBA-UTIL \
		LIBSAMBA-NET \
		LIBPOPT \
		POPT_SAMBA \
		POPT_CREDENTIALS
# End BINARY net
#################################

net_OBJ_FILES = $(addprefix $(utilssrcdir)/net/,  \
		net.o \
		net_machinepw.o \
		net_password.o \
		net_time.o \
		net_join.o \
		net_vampire.o \
		net_user.o \
		net_export_keytab.o)


$(eval $(call proto_header_template,$(utilssrcdir)/net/net_proto.h,$(net_OBJ_FILES:.o=.c)))
