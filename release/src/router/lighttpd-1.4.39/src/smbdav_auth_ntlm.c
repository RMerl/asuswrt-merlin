#include "base.h"
#include "log.h"
#include "buffer.h"
#include "response.h"

#include "plugin.h"

#include "stream.h"
#include "stat_cache.h"

#include "sys-mmap.h"
////////////////////////////////

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

#include <unistd.h>
#include <dirent.h>
#include <libsmbclient.h>
#include <dlinklist.h>
#include "mod_smbdav.h"

#define DBE 1


int get_ntlm_type(buffer *msg)
{
	return msg->ptr[8];
}

int ntlm_negotiate_response_handler(struct cli_state *cli, char *out) //, char *challenge, int challengeLen)
{
	uint32 olen = 0;
	unsigned char *tmp;
	unsigned char blob[512];
	int blobLen = smbc_cli_get_smb_secblob(cli, blob);

	int proto = smbc_cli_get_protocol(cli);
	if (proto >= PROTOCOL_NT1) { 
		//tmp = base64_encode(blob, blobLen, &olen);
		tmp = ldb_base64_encode(blob, blobLen);
		olen = strlen(tmp);
		memcpy(out, tmp, olen);
		free(tmp);
	} else if (proto >= PROTOCOL_LANMAN1) {

	} else  {

	}

	return olen;
}

int smb_backend_get_secblob(struct cli_state *cli, uint8_t *blob)
{
	return smbc_cli_get_smb_secblob(cli, blob);
}

int smb_backend_get_challenge(struct cli_state *cli, char *out)
{
	int prot = smbc_cli_get_protocol(cli);
	
	if(!out)
		return -1;

	char blob[512] = {0};
	unsigned char *tmp;

	char z = 0;
	char msg1[] =
                {(uint8_t)'N', (uint8_t)'T', (uint8_t)'L', (uint8_t)'M', (uint8_t)'S',(uint8_t)'S', (uint8_t)'P', z,(uint8_t)2, z,
                z, z, z, z, z, z, (uint8_t)40, z, z, z, 
                (uint8_t)1, (uint8_t)130, z, z, z, (uint8_t)2, (uint8_t)2, (uint8_t)2, z, z, 
                z, z, z, z, z, z, z, z, z, z};

	int len, olen;
	if (prot >= PROTOCOL_NT1) { 
		len = smbc_cli_get_smb_challenge(cli, blob);
		memcpy(&msg1[24], blob, 8);
		tmp = ldb_base64_encode(msg1, 40);
		olen = strlen(tmp);
		memcpy(out, tmp, olen);
		free(tmp);
		return olen;
	} else if (prot >= PROTOCOL_LANMAN1) {
		len = smbc_cli_get_smb_challenge(cli, blob);
		tmp = ldb_base64_encode(blob, len);
		olen = strlen(tmp);
		memcpy(out, tmp, olen);
		free(tmp);
		return olen;
	} else {
		return 0;
	}
	return 0;
}

int smb_backend_send_negprot(struct cli_state *cli, char *host)
{
	UNUSED(host);
	if(smbc_cli_send_negprot(cli)) 
		return -1;
	return 0;
}

int smb_backend_send_negprot_done(struct cli_state *cli)
{
	if(smbc_cli_send_negprot_done(cli))
		return -1;
	return 0;
}

int smb_backend_get_protocol(struct cli_state *cli)
{
	return smbc_cli_get_protocol(cli);
}

int smb_backend_send_session_setup_nego(struct cli_state *cli, 
	void *ntlmssp_state, char *ntlm_msg, int ntlm_len)
{
	if(smbc_cli_send_session_setup_nego(cli, ntlmssp_state, ntlm_msg, ntlm_len))
		return -1;
	return 0;
}

uint32 smb_backend_send_session_setup_auth(struct cli_state *cli, 
	void *ntlmssp_state, char *ntlm_msg, int ntlm_len)
{
	return smbc_cli_session_setup_ntlmssp_auth(cli, ntlmssp_state, ntlm_msg, ntlm_len);
}

handler_t ntlm_authentication_handler(server *srv, connection *con, plugin_data *p)
{
	//ntlm_handler_ctx *hctx;
	data_string *ds_auth;

	//hctx = con->plugin_ctx[p->id];
	ds_auth = (data_string *)array_get_element(con->request.headers, "Authorization");

	Cdbg(DBE, "enter ntlm_authentication_handler...");
	Cdbg(DBE, "con->smb_info->state=[%d]", con->smb_info->state);
	Cdbg(DBE, "con->smb_info->qflag=[%d]", con->smb_info->qflag);
	Cdbg(DBE, "con->smb_info->cli=[%p]", con->smb_info->cli);
	
	if(con->smb_info->state == NTLMSSP_DONE) {
		Cdbg(DBE, "con->smb_info->state == NTLMSSP_DONE");
		//re-authentication??
#ifdef SUPPORT_REAUTH
		if (ds_auth != NULL) {
			NTLM_MESSAGE_TYPE state;
			char *http_authorization = NULL;
			http_authorization = ds_auth->value->ptr;
			Cdbg(DBE, "con->smb_info->state == NTLMSSP_DONE-->re-authentication");
			if(strncmp(http_authorization, "NTLM ", 5) == 0) {				
				buffer *ntlm_msg = buffer_init();
				if (!base64_decode(ntlm_msg, &http_authorization[5])) {
					log_error_write(srv, __FILE__, __LINE__, "sb", "decodeing base64-string failed", ntlm_msg);
					buffer_free(ntlm_msg);
					return HANDLER_GO_ON;
				}
				state = get_ntlm_type(ntlm_msg);

				if(state == NTLMSSP_NEGOTIATE) {
					int challLen, olen;
					char out[512]={0}, challenge[512]={0};
					smb_connection_free(srv, con, p, con->smb_info);
					ntlm_handler_ctx_free(hctx);
					hctx = ntlm_handler_ctx_init(srv, con, p);
					con->plugin_ctx[p->id] = hctx;
					con->smb_info->state = state;
					int res = smb_backend_send_negprot(con->smb_info->cli, con->smb_info->server->ptr);
					res = smb_backend_send_negprot_done(con->smb_info->cli);
					int proto = smb_backend_get_protocol(con->smb_info->cli);
					if(proto >= PROTOCOL_NT1) {
						uint32_t caps = smbc_cli_get_capabilities(con->smb_info->cli);
						//fprintf(stderr, "\tRE-AUTH, caps=[%08X]\n", caps);						
						if(caps & CAP_EXTENDED_SECURITY) {
							con->smb_info->ntlmssp_state = smbc_cli_ntlmssp_state_alloc();
							res = smb_backend_send_session_setup_nego(con->smb_info->cli, con->smb_info->ntlmssp_state, ntlm_msg->ptr, ntlm_msg->used);
							buffer_free(ntlm_msg);
							olen = ntlm_negotiate_response_handler(con->smb_info->cli, out);
							//sprintf(challenge, "NTLM %s", out);
						} else {
							olen = smb_backend_get_challenge(con->smb_info->cli, out);
							//sprintf(challenge, "NTLM %s", out);
						}
					} else {
						olen = smb_backend_get_challenge(con->smb_info->cli, out);
						//sprintf(challenge, "NTLM %s", out);
					}
					Cdbg(DBE, "\tRE-AUTH, challenge=[%s]", challenge);
					buffer_copy_string(p->tmp_buf, challenge);
					response_header_insert(srv, con, CONST_STR_LEN("WWW-Authenticate"), CONST_BUF_LEN(p->tmp_buf));
					con->http_status = 401;
					return HANDLER_FINISHED;
				}
			}
		}
#endif		
		
		//had authenticated.
	}
	else if(con->smb_info->qflag == SMB_HOST_QUERY) {
		//host_query  do the BASIC authentication
		Cdbg(DBE, "host_query  do the BASIC authentication");
	}
	else  {					
		if (ds_auth == NULL) {
			Cdbg(DBE, "ds_auth == NULL");
			//send out 401 unauthorized
			//ntlm_response_401(srv, con, p);
			smbc_wrapper_response_401(srv, con);
			con->smb_info->state = NTLMSSP_UNKNOWN;
			Cdbg(DBE, "\t2 set state to [%d]", con->smb_info->state);				
			return HANDLER_FINISHED;
		} 
		else if (con->smb_info->state == NTLMSSP_UNKNOWN) {		
			Cdbg(DBE, "con->smb_info->state == NTLMSSP_UNKNOWN");
			//send out 401 unauthorized
			//ntlm_response_401(srv, con, p);
			smbc_wrapper_response_401(srv, con);
			con->smb_info->state = NTLMSSP_NEGOTIATE;
			Cdbg(DBE, "\t3 set state to [%d]", con->smb_info->state);				
			return HANDLER_FINISHED;
		} 
		else {
			char *http_authorization = NULL;
			http_authorization = ds_auth->value->ptr;			
			if(strncmp(http_authorization, "NTLM ", 5) == 0) {
				buffer *ntlm_msg = buffer_init();
				if (!base64_decode(ntlm_msg, &http_authorization[5])) {
					log_error_write(srv, __FILE__, __LINE__, "sb", "decodeing base64-string failed", ntlm_msg);
					buffer_free(ntlm_msg);
					return HANDLER_GO_ON;
				}
				
				Cdbg(DBE, "\tntlm_msg = [%s]", ntlm_msg->ptr);				
				con->smb_info->state = get_ntlm_type(ntlm_msg);
				Cdbg(DBE, "\tstate=[%d]", con->smb_info->state);
				
				switch(con->smb_info->state) {
				case NTLMSSP_NEGOTIATE: { //NTLMSSP_NEGOTIATE
					int challLen, olen;
					char out[512]={0}, challenge[512]={0};
					if(smbc_cli_get_socket(con->smb_info->cli) < 0) {
#ifdef TODO						
						smb_connection_free(srv, con, p, con->smb_info);
						ntlm_handler_ctx_free(hctx);
						hctx = ntlm_handler_ctx_init(srv, con, p);
						con->plugin_ctx[p->id] = hctx;
#endif						
					}
					int res = smb_backend_send_negprot(con->smb_info->cli, con->smb_info->server->ptr);
					res = smb_backend_send_negprot_done(con->smb_info->cli);
					int proto = smb_backend_get_protocol(con->smb_info->cli);
					Cdbg(DBE, "\tNEGO, proto=[%d], res=[%d]", proto, res);
					
					if(proto >= PROTOCOL_NT1) {
						uint32_t caps = smbc_cli_get_capabilities(con->smb_info->cli);
						Cdbg(DBE, "\tNEGO, caps=[%08X]", caps);
						
						if(caps & CAP_EXTENDED_SECURITY) {
							con->smb_info->ntlmssp_state = smbc_cli_ntlmssp_state_alloc();
							res = smb_backend_send_session_setup_nego(con->smb_info->cli, con->smb_info->ntlmssp_state, ntlm_msg->ptr, ntlm_msg->used);
							buffer_free(ntlm_msg);
							olen = ntlm_negotiate_response_handler(con->smb_info->cli, out);
							sprintf(challenge, "NTLM %s", out);
						} else {
							olen = smb_backend_get_challenge(con->smb_info->cli, out);
							sprintf(challenge, "NTLM %s", out);
						}
					} 
					else {
						olen = smb_backend_get_challenge(con->smb_info->cli, out);
						sprintf(challenge, "NTLM %s", out);
					}
					
					Cdbg(DBE, "\tchallenge=[%s]", challenge);
					buffer_copy_string(p->tmp_buf, challenge);
					response_header_insert(srv, con, CONST_STR_LEN("WWW-Authenticate"), CONST_BUF_LEN(p->tmp_buf));
					con->http_status = 401;
					return HANDLER_FINISHED;
				}
				case NTLMSSP_AUTH: { //NTLMSSP_AUTH
					uint32 res = 0;
					int proto = smb_backend_get_protocol(con->smb_info->cli);
					Cdbg(DBE, "AUTH, proto=[%d]", proto);
					
					if(proto >= PROTOCOL_NT1) {
						uint32_t caps = smbc_cli_get_capabilities(con->smb_info->cli);
						Cdbg(DBE, "AUTH, caps=[%08X]", caps);							
						if(caps & CAP_EXTENDED_SECURITY) {
							//copy the secblob from http to smb
							Cdbg(DBE, "copy the secblob from http to smb");
							res = smb_backend_send_session_setup_auth(con->smb_info->cli, con->smb_info->ntlmssp_state, ntlm_msg->ptr, ntlm_msg->used);
							Cdbg(DBE, "copy the secblob from http to smb, res=[%d][%x]", res, res);
							buffer_free(ntlm_msg);
							smbc_cli_ntlmssp_state_free(con->smb_info->ntlmssp_state);
							con->smb_info->ntlmssp_state = NULL;
						} else {
							res = smbc_cli_session_setup_lanman2(con->smb_info->cli, ntlm_msg->ptr, ntlm_msg->used);
							buffer_free(ntlm_msg);
						}
					} 
					else {
						res = smbc_cli_session_setup_lanman2(con->smb_info->cli, ntlm_msg->ptr, ntlm_msg->used);
						buffer_free(ntlm_msg);
					}
					
					if(res) {
						//authenticate fail, request another authentication
						char str[5];
						sprintf(str, "NTLM");
						buffer_copy_string(p->tmp_buf, str);
						con->smb_info->state = NTLMSSP_UNKNOWN;
						Cdbg(DBE, "\tauthenticate fail, request another authentication set state to [%d]", con->smb_info->state);				
						response_header_insert(srv, con, CONST_STR_LEN("WWW-Authenticate"), CONST_BUF_LEN(p->tmp_buf));
						con->http_status = 401;
						return HANDLER_FINISHED;
					}
					
					con->smb_info->state = NTLMSSP_DONE;
					//Cdbg(DBE, "\t5 set state to [%d]\n", con->smb_info->state);				
					break; 
				}
				default:
					break;
				}
			} else {
				con->http_status = 404;
				return HANDLER_FINISHED;
			}
		}
	}

	Cdbg(DBE, "1leave ntlm_authentication_handler, con->smb_info->qflag=[%d]", con->smb_info->qflag);
	if(con->smb_info->state == NTLMSSP_DONE) {
		//unsigned int res;
		int res;
		char turi[256];
		switch(con->smb_info->qflag) {
		case SMB_HOST_QUERY:
			break;
		case SMB_SHARE_QUERY:
			sprintf(turi, "%s/IPC$", con->physical_auth_url->ptr);
			res = smbc_cli_tree_connect(con->smb_info->cli, turi);
			break;
		case SMB_FILE_QUERY:
		default:
		{
			res = smbc_cli_tree_connect(con->smb_info->cli, con->physical_auth_url->ptr);			
			Cdbg(DBE, "leave smbc_cli_tree_connect res=[%X]\n", res);			
			break;
		}
		}
		if( res == NT_STATUS_V(NT_STATUS_LOGON_FAILURE) ||
			res == NT_STATUS_V(NT_STATUS_ACCESS_DENIED) ) 
		{
			//ntlm_response_401(srv, con, p);
			smbc_wrapper_response_401(srv, con);
			con->smb_info->state = NTLMSSP_INITIAL;			
			return HANDLER_FINISHED;
		}
		else if(res == 0xc000023a){
			Cdbg(DBE, "NT_STATUS_INVALID_CONNECTION\n");
			//ntlm_response_401(srv, con, p);
			smbc_wrapper_response_401(srv, con);
			con->smb_info->state = NTLMSSP_UNKNOWN;			
			return HANDLER_FINISHED;
		}
		
	}	

	Cdbg(DBE, "leave ntlm_authentication_handler");	
	
	return HANDLER_UNSET;
}

