################################################
# Start MODULE gensec_ntlmssp
[MODULE::gensec_ntlmssp]
SUBSYSTEM = gensec
INIT_FUNCTION = gensec_ntlmssp_init
PRIVATE_DEPENDENCIES = MSRPC_PARSE CREDENTIALS
OUTPUT_TYPE = MERGED_OBJ
# End MODULE gensec_ntlmssp
################################################

gensec_ntlmssp_OBJ_FILES = $(addprefix $(authsrcdir)/ntlmssp/, ntlmssp.o ntlmssp_sign.o ntlmssp_client.o ntlmssp_server.o) 

$(eval $(call proto_header_template,$(authsrcdir)/ntlmssp/proto.h,$(gensec_ntlmssp_OBJ_FILES:.o=.c)))
