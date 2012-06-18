# Directory Service subsystem

mkinclude samdb/ldb_modules/config.mk

################################################
# Start SUBSYSTEM SAMDB
[SUBSYSTEM::SAMDB]
PUBLIC_DEPENDENCIES = HEIMDAL_KRB5  
PRIVATE_DEPENDENCIES = LIBNDR NDR_DRSUAPI NDR_DRSBLOBS NSS_WRAPPER \
					   auth_system_session LDAP_ENCODE LIBCLI_AUTH LIBNDR \
					   SAMDB_SCHEMA LDB_WRAP SAMDB_COMMON \
						LIBCLI_DRSUAPI LIBCLI_LDAP_NDR LIBSAMBA-UTIL 


SAMDB_OBJ_FILES = $(addprefix $(dsdbsrcdir)/, \
		samdb/samdb.o \
		samdb/samdb_privilege.o \
		samdb/cracknames.o \
		repl/replicated_objects.o)

$(eval $(call proto_header_template,$(dsdbsrcdir)/samdb/samdb_proto.h,$(SAMDB_OBJ_FILES:.o=.c)))
# PUBLIC_HEADERS += dsdb/samdb/samdb.h

[SUBSYSTEM::SAMDB_COMMON]
PRIVATE_DEPENDENCIES = LIBLDB

SAMDB_COMMON_OBJ_FILES = $(addprefix $(dsdbsrcdir)/common/, \
		sidmap.o \
		util.o) \
		../libds/common/flag_mapping.o
$(eval $(call proto_header_template,$(dsdbsrcdir)/common/proto.h,$(SAMDB_COMMON_OBJ_FILES:.o=.c)))

[SUBSYSTEM::SAMDB_SCHEMA]
PRIVATE_DEPENDENCIES = SAMDB_COMMON NDR_DRSUAPI NDR_DRSBLOBS

SAMDB_SCHEMA_OBJ_FILES = $(addprefix $(dsdbsrcdir)/schema/, \
		schema_init.o \
		schema_set.o \
		schema_query.o \
		schema_syntax.o \
		schema_description.o \
		schema_convert_to_ol.o \
		schema_inferiors.o)

$(eval $(call proto_header_template,$(dsdbsrcdir)/schema/proto.h,$(SAMDB_SCHEMA_OBJ_FILES:.o=.c)))
# PUBLIC_HEADERS += dsdb/schema/schema.h

#######################
# Start SUBSYSTEM DREPL_SRV
[MODULE::DREPL_SRV]
INIT_FUNCTION = server_service_drepl_init
SUBSYSTEM = service
PRIVATE_DEPENDENCIES = \
		SAMDB \
		process_model \
		RPC_NDR_DRSUAPI
# End SUBSYSTEM DREPL_SRV
#######################

DREPL_SRV_OBJ_FILES = $(addprefix $(dsdbsrcdir)/repl/, \
		drepl_service.o \
		drepl_periodic.o \
		drepl_partitions.o \
		drepl_out_pull.o \
		drepl_out_helpers.o \
		drepl_notify.o)

$(eval $(call proto_header_template,$(dsdbsrcdir)/repl/drepl_service_proto.h,$(DREPL_SRV_OBJ_FILES:.o=.c)))

#######################
# Start SUBSYSTEM KCC_SRV
[MODULE::KCC_SRV]
INIT_FUNCTION = server_service_kcc_init
SUBSYSTEM = service
PRIVATE_DEPENDENCIES = \
		SAMDB \
		process_model \
		RPC_NDR_DRSUAPI
# End SUBSYSTEM KCC_SRV
#######################

KCC_SRV_OBJ_FILES = $(addprefix $(dsdbsrcdir)/kcc/, \
		kcc_service.o \
		kcc_periodic.o)

$(eval $(call proto_header_template,$(dsdbsrcdir)/kcc/kcc_service_proto.h,$(KCC_SRV_OBJ_FILES:.o=.c)))
