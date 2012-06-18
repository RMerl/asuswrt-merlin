# NBTD server subsystem

#######################
# Start SUBSYSTEM WINSDB
[SUBSYSTEM::WINSDB]
PUBLIC_DEPENDENCIES = \
		LIBLDB
# End SUBSYSTEM WINSDB
#######################

WINSDB_OBJ_FILES = $(addprefix $(nbt_serversrcdir)/wins/, winsdb.o wins_hook.o)

$(eval $(call proto_header_template,$(nbt_serversrcdir)/wins/winsdb_proto.h,$(WINSDB_OBJ_FILES:.o=.c)))

#######################
# Start MODULE ldb_wins_ldb
[MODULE::ldb_wins_ldb]
SUBSYSTEM = LIBLDB
INIT_FUNCTION = LDB_MODULE(wins_ldb)
PRIVATE_DEPENDENCIES = \
		LIBLDB LIBNETIF LIBSAMBA-HOSTCONFIG LIBSAMBA-UTIL
# End MODULE ldb_wins_ldb
#######################

ldb_wins_ldb_OBJ_FILES = $(nbt_serversrcdir)/wins/wins_ldb.o

#######################
# Start SUBSYSTEM NBTD_WINS
[SUBSYSTEM::NBTD_WINS]
PRIVATE_DEPENDENCIES = \
		LIBCLI_NBT WINSDB
# End SUBSYSTEM NBTD_WINS
#######################


NBTD_WINS_OBJ_FILES = $(addprefix $(nbt_serversrcdir)/wins/, winsserver.o winsclient.o winswack.o wins_dns_proxy.o)

$(eval $(call proto_header_template,$(nbt_serversrcdir)/wins/winsserver_proto.h,$(NBTD_WINS_OBJ_FILES:.o=.c)))

#######################
# Start SUBSYSTEM NBTD_DGRAM
[SUBSYSTEM::NBTD_DGRAM]
PRIVATE_DEPENDENCIES = \
		LIBCLI_DGRAM CLDAPD
# End SUBSYSTEM NBTD_DGRAM
#######################

NBTD_DGRAM_OBJ_FILES = $(addprefix $(nbt_serversrcdir)/dgram/, request.o netlogon.o browse.o)

$(eval $(call proto_header_template,$(nbt_serversrcdir)/dgram/proto.h,$(NBTD_DGRAM_OBJ_FILES:.o=.c)))

#######################
# Start SUBSYSTEM NBTD
[SUBSYSTEM::NBT_SERVER]
PRIVATE_DEPENDENCIES = \
		LIBCLI_NBT NBTD_WINS NBTD_DGRAM 
# End SUBSYSTEM NBTD
#######################

NBT_SERVER_OBJ_FILES = $(addprefix $(nbt_serversrcdir)/, \
		interfaces.o \
		register.o \
		query.o \
		nodestatus.o \
		defense.o \
		packet.o \
		irpc.o)

$(eval $(call proto_header_template,$(nbt_serversrcdir)/nbt_server_proto.h,$(NBT_SERVER_OBJ_FILES:.o=.c)))

[MODULE::service_nbtd]
INIT_FUNCTION = server_service_nbtd_init
SUBSYSTEM = service
PRIVATE_DEPENDENCIES = NBT_SERVER process_model

service_nbtd_OBJ_FILES = \
		$(nbt_serversrcdir)/nbt_server.o
