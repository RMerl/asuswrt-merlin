# ntptr server subsystem

################################################
# Start MODULE ntptr_simple_ldb
[MODULE::ntptr_simple_ldb]
INIT_FUNCTION = ntptr_simple_ldb_init
SUBSYSTEM = ntptr
PRIVATE_DEPENDENCIES = \
		LIBLDB NDR_SPOOLSS DCERPC_COMMON
# End MODULE ntptr_simple_ldb
################################################

ntptr_simple_ldb_OBJ_FILES = $(ntptrsrcdir)/simple_ldb/ntptr_simple_ldb.o

################################################
# Start SUBSYSTEM ntptr
[SUBSYSTEM::ntptr]
PUBLIC_DEPENDENCIES = DCERPC_COMMON
#
# End SUBSYSTEM ntptr
################################################

ntptr_OBJ_FILES = \
		$(ntptrsrcdir)/ntptr_base.o \
		$(ntptrsrcdir)/ntptr_interface.o

$(eval $(call proto_header_template,$(ntptrsrcdir)/ntptr_proto.h,$(ntptr_OBJ_FILES:.o=.c)))
