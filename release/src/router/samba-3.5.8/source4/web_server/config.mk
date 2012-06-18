# web server subsystem

#######################
# Start SUBSYSTEM WEB
[MODULE::WEB]
INIT_FUNCTION = server_service_web_init
SUBSYSTEM = service
PRIVATE_DEPENDENCIES = LIBTLS smbcalls process_model LIBPYTHON
# End SUBSYSTEM WEB
#######################

WEB_OBJ_FILES = $(addprefix $(web_serversrcdir)/, web_server.o wsgi.o) 

$(eval $(call proto_header_template,$(web_serversrcdir)/proto.h,$(WEB_OBJ_FILES:.o=.c)))
