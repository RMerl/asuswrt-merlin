################################################
# Start SUBSYSTEM LDBSAMBA
[SUBSYSTEM::LDBSAMBA]
PUBLIC_DEPENDENCIES = LIBLDB
PRIVATE_DEPENDENCIES = LIBSECURITY SAMDB_SCHEMA LIBNDR NDR_DRSBLOBS
# End SUBSYSTEM LDBSAMBA
################################################

LDBSAMBA_OBJ_FILES = $(ldb_sambasrcdir)/ldif_handlers.o
$(eval $(call proto_header_template,$(ldb_sambasrcdir)/ldif_handlers_proto.h,$(LDBSAMBA_OBJ_FILES:.o=.c)))

