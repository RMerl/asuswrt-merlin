#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <dlinklist.h>
#include <dirent.h>

#if defined(HAVE_LIBXML_H) && defined(HAVE_SQLITE3_H)
#include <sqlite3.h>
#endif

#ifdef EMBEDDED_EANBLE
#ifndef APP_IPKG
//#ifdef RTCONFIG_USB
#include <disk_initial.h>
//#endif
#endif
#endif

#define DBE 1

typedef struct {
	array *access_deny;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

typedef struct folder_info_s {
	buffer *path;
	struct folder_info_s *prev, *next;
}folder_info_t;

typedef enum { 
	INVITE_FINISHED,
	INVITE_ACCOUNT_EXISTED,
	INVITE_TOKEN_INVALID,
	INVITE_SECURITY_CODE_INVALID,
	INVITE_ERROR
} update_invite_t;

INIT_FUNC(mod_aicloud_invite_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	return p;
}

FREE_FUNC(mod_aicloud_invite_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			array_free(s->access_deny);

			free(s);
		}
		free(p->config_storage);
	}

	free(p);

	return HANDLER_GO_ON;
}

SETDEFAULTS_FUNC(mod_aicloud_invite_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ "url.access-deny",             NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },
		{ NULL,                          NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->access_deny    = array_init();

		cv[0].destination = s->access_deny;

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}
	}

	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_craete_captcha_image_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(access_deny);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("url.access-deny"))) {
				PATCH(access_deny);
			}
		}
	}

	return 0;
}
#undef PATCH

char* get_url_param_ualue(const char* url_param, const char* key){

	if(key==NULL || url_param==NULL)
		return NULL;

	if(strcmp(key,"")==0 || strcmp(url_param,"")==0)
		return NULL;
	
	buffer* buffer_param = buffer_init();
	buffer_copy_string(buffer_param, url_param);
	
	char* pch = strtok(buffer_param->ptr, "&");
	int key_len = strlen(key);
	char* value = NULL;
	
	while(pch!=NULL){
		if(strncmp(pch, key, key_len)==0){
			int len = strlen(pch) - key_len - 1;				

			value = (char*)malloc(len+1);
			memset(value, '\0', len); 			
			strncpy(value, pch+key_len+1, len);
			value[len]='\0';

			break;
		}

		pch = strtok( NULL, "&" );
	}

	buffer_free(buffer_param);
	
	return value;
}

update_invite_t update_account_invite(const char* token, const char* username, const char* password, const char* security_code){
	
	if(token==NULL || username==NULL || password==NULL)
		return INVITE_ERROR;

	int bFoundToken = 0;
	aicloud_acc_invite_info_t *c;
	for (c = aicloud_acc_invite_info_list; c; c = c->next) {
		if(buffer_is_equal_string(c->token, token, strlen(token))){
			bFoundToken = 1;
			break;
		}
	}

	if(bFoundToken==0){
		return INVITE_TOKEN_INVALID;
	}

	if(c->status!=0){
		return INVITE_ERROR;
	}

	//- check security code
	Cdbg(DBE, "c->security_code=%s", c->security_code->ptr);
	if(!buffer_is_empty(c->security_code) && !buffer_is_equal_string(c->security_code, security_code, strlen(security_code))){
		return INVITE_SECURITY_CODE_INVALID;
	}

	//- check username
	if(is_account_existed(username)==1){
		return INVITE_ACCOUNT_EXISTED;
	}

	//- add account
	aicloud_acc_info_t *aicloud_acc_info;
	aicloud_acc_info = (aicloud_acc_info_t *)calloc(1, sizeof(aicloud_acc_info_t));
	
	//- User Name
	aicloud_acc_info->username = buffer_init();
	buffer_copy_string(aicloud_acc_info->username, username);
		
	//- User Password
	aicloud_acc_info->password = buffer_init();
	buffer_copy_string(aicloud_acc_info->password, password);
						
	DLIST_ADD(aicloud_acc_info_list, aicloud_acc_info);
							
	save_aicloud_acc_list();

	//- create permission
	if(!buffer_is_empty(c->permission)){

		char * pch;
		pch = strtok(c->permission->ptr, ",;");	
				
		while(pch!=NULL){
					
			//- partion
			buffer* partion = buffer_init();
			buffer_copy_string_len(partion, CONST_STR_LEN("/mnt/") );
			buffer_append_string_len(partion, pch, strlen(pch));
			buffer_urldecode_path(partion);

			//- folder
			pch = strtok(NULL,",;");
			buffer* folder = buffer_init();
			buffer_copy_string_len(folder, pch, strlen(pch));
			buffer_urldecode_path(folder);

			//- permission
			pch = strtok(NULL,",;");
			buffer* permission = buffer_init();
			buffer_copy_string_len(permission, pch, strlen(pch));
					
			Cdbg(DBE, "result: partion=%s, folder=%s, permission=%s", partion->ptr, folder->ptr, permission->ptr);

#if EMBEDDED_EANBLE
			set_aicloud_permission( username,
									partion->ptr,
			  					    folder->ptr,
              						atoi(c->permission->ptr) );
#endif
			buffer_free(partion);
			buffer_free(folder);
			buffer_free(permission);
					
			pch = strtok(NULL,",;");
					
		}
						
	}

	//- delete invite token
	free_aicloud_acc_invite_info(c);
	DLIST_REMOVE(aicloud_acc_invite_info_list, c);
	free(c);

	save_aicloud_acc_invite_list();

	return INVITE_FINISHED;
}

#if 0
int parse_postdata_from_chunkqueue(server *srv, connection *con, plugin_data *p, chunkqueue *cq, char **ret_data) {
	
	int res = -1;
	
	chunk *c;
	char* result_data = NULL;
	
	UNUSED(con);
	
	for (c = cq->first; cq->bytes_out != cq->bytes_in; c = cq->first) {
		size_t weWant = cq->bytes_out - cq->bytes_in;
		size_t weHave;

		switch(c->type) {
			case MEM_CHUNK:
				weHave = c->mem->used - 1 - c->offset;

				if (weHave > weWant) weHave = weWant;
				
				result_data = (char*)malloc(sizeof(char)*(weHave+1));
				memset(result_data, 0, sizeof(char)*(weHave+1));
				strcpy(result_data, c->mem->ptr + c->offset);
				
				c->offset += weHave;
				cq->bytes_out += weHave;

				res = 0;
				
				break;
				
			default:
				break;
		}
		
		chunkqueue_remove_finished_chunks(cq);
	}
	
	*ret_data = result_data;

	return res;
}
#endif

URIHANDLER_FUNC(mod_aicloud_invite_physical_handler){
	plugin_data *p = p_d;
	int s_len;
	size_t k;
	int is_root_path = 0;
	char* param_val = NULL;
	buffer *b = NULL;
	
	if (con->mode != SMB_BASIC&&con->mode != DIRECT) return HANDLER_GO_ON;
	if (con->uri.path->used == 0) return HANDLER_GO_ON;

	if( strncmp( con->request.uri->ptr, "/AINVITE",  7 ) != 0 )
		return HANDLER_GO_ON;
	
	//- extract invite token	
	char* str = strstr(con->request.uri->ptr, "/AINVITE" ) + 1;

	if(str==NULL){
		con->http_status = 400;
		con->file_finished = 1;
		return HANDLER_FINISHED;
	}

	if(con->request.http_method == HTTP_METHOD_POST){
		
		char *post_data;
		if( parse_postdata_from_chunkqueue(srv, con, con->request_content_queue, &post_data) == 0 ){
			
			if(post_data!=NULL){
				Cdbg(DBE, "post_data=%s", post_data);
				
				char* action = get_url_param_ualue(post_data, "action");

				if(strcmp(action, "register")==0){
					char* token = get_url_param_ualue(post_data, "token");
					char* username = get_url_param_ualue(post_data, "username");
					char* password = get_url_param_ualue(post_data, "password");
					char* security_code = get_url_param_ualue(post_data, "security_code");

					update_invite_t ret = update_account_invite(token, username, password, security_code);
					
					if(post_data!=NULL) free(post_data);
					if(action!=NULL) free(action);
					if(token!=NULL) free(token);
					if(username!=NULL) free(username);
					if(password!=NULL) free(password);
					if(security_code!=NULL) free(security_code);
					
					if(ret == INVITE_FINISHED){
						response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text"));
						b = buffer_init();						
						buffer_append_string_len(b, CONST_STR_LEN("OK"));
						chunkqueue_append_buffer(con->write_queue, b);
						buffer_free(b);						
						con->http_status = 200;
						con->file_finished = 1;
						return HANDLER_FINISHED;
					}
					else if(ret == INVITE_SECURITY_CODE_INVALID){
						response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text"));
						b = buffer_init();
						buffer_append_string_len(b, CONST_STR_LEN("SECURITY_CODE_INVALID"));
						chunkqueue_append_buffer(con->write_queue, b);
						buffer_free(b);
						con->http_status = 200;
						con->file_finished = 1;
						return HANDLER_FINISHED;
					}
					else if(ret == INVITE_ACCOUNT_EXISTED){
						response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text"));
						b = buffer_init();
						buffer_append_string_len(b, CONST_STR_LEN("ACCOUNT_EXISTED"));
						chunkqueue_append_buffer(con->write_queue, b);
						buffer_free(b);
						con->http_status = 200;
						con->file_finished = 1;
						return HANDLER_FINISHED;
					}
					else if(ret == INVITE_TOKEN_INVALID){
						response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text"));
						b = buffer_init();
						buffer_append_string_len(b, CONST_STR_LEN("TOKEN_INVALID"));
						chunkqueue_append_buffer(con->write_queue, b);
						buffer_free(b);
						con->http_status = 200;
						con->file_finished = 1;
						return HANDLER_FINISHED;
					}
					else{
						con->http_status = 411;
						con->file_finished = 1;
						return HANDLER_FINISHED;
					}
				}
				else if(strcmp(action, "get_invite_info")==0){
					
					int need_security_code = 0;
					buffer* buffer_product_id = NULL;
					buffer* buffer_device_id = NULL;
					int auth_type = 0;
					char* token = get_url_param_ualue(post_data, "token");
					int bTokenExist = 0;

					Cdbg(DBE, "token=%s", token);
					
					b = buffer_init();
					
					response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("application/json"));
					
					if (token==NULL) {
						//- result
						buffer_append_string_len(b, CONST_STR_LEN("{"));
						buffer_append_string_len(b, CONST_STR_LEN("\"result\":\"TOKEN_INVALID\""));
						buffer_append_string_len(b, CONST_STR_LEN("}"));
												
						if(action!=NULL) free(action);
						if(post_data!=NULL) free(post_data);

						chunkqueue_append_buffer(con->write_queue, b);
						buffer_free(b);
					
						con->http_status = 200;
						con->file_finished = 1;
						return HANDLER_FINISHED;
					}
					
					aicloud_acc_invite_info_t *c;
					for (c = aicloud_acc_invite_info_list; c; c = c->next) {
						if(buffer_is_equal_string(c->token, token, strlen(token))){
							bTokenExist = 1;
							break;
						}
					}
					
					if(bTokenExist==0){
						//- result
						buffer_append_string_len(b, CONST_STR_LEN("{"));
						buffer_append_string_len(b, CONST_STR_LEN("\"result\":\"TOKEN_INVALID\""));
						buffer_append_string_len(b, CONST_STR_LEN("}"));
												
						if(action!=NULL) free(action);
						if(post_data!=NULL) free(post_data);

						chunkqueue_append_buffer(con->write_queue, b);
						buffer_free(b);
					
						con->http_status = 200;
						con->file_finished = 1;
						return HANDLER_FINISHED;
					}
					
					if( buffer_is_empty(c->security_code) || 
						strcmp(c->security_code->ptr, "none")==0 || 
						strcmp(c->security_code->ptr, "")==0 ){
						need_security_code = 0;
					}
					else{
						need_security_code = 1;
					}
					
					if(action!=NULL) free(action);
					if(post_data!=NULL) free(post_data);
					if(token!=NULL) free(token);
					
					//- result
					buffer_append_string_len(b, CONST_STR_LEN("{"));
					buffer_append_string_len(b, CONST_STR_LEN("\"result\":\"OK\""));
					buffer_append_string_len(b, CONST_STR_LEN(","));

					buffer_append_string_len(b, CONST_STR_LEN("\"data\": {"));

					//- product id
					buffer_append_string_len(b, CONST_STR_LEN("\"productid\":\""));
					buffer_append_string_buffer(b, c->productid);
					buffer_append_string_len(b, CONST_STR_LEN("\""));						
					buffer_append_string_len(b, CONST_STR_LEN(","));
					buffer_free(buffer_product_id);
					
					//- device id
					buffer_append_string_len(b, CONST_STR_LEN("\"deviceid\":\""));
					buffer_append_string_buffer(b, c->deviceid);
					buffer_append_string_len(b, CONST_STR_LEN("\""));						
					buffer_append_string_len(b, CONST_STR_LEN(","));
					buffer_free(buffer_device_id);
					
					//- security code needed
					buffer_append_string_len(b, CONST_STR_LEN("\"need_security_code\":\""));
					if(need_security_code==1)
						buffer_append_string_len(b, CONST_STR_LEN("1"));
					else
						buffer_append_string_len(b, CONST_STR_LEN("0"));
					buffer_append_string_len(b, CONST_STR_LEN("\""));
					buffer_append_string_len(b, CONST_STR_LEN(","));
					
					//- auth type: 0:aicloud, 1:facebook, 2:google
					buffer_append_string_len(b, CONST_STR_LEN("\"auth_type\":\""));
					char c_auth_type[2] = "\0";
					sprintf(c_auth_type, "%d", c->auth_type);
					buffer_append_string_len(b, c_auth_type, strlen(c_auth_type));
					buffer_append_string_len(b, CONST_STR_LEN("\""));
					
					buffer_append_string_len(b, CONST_STR_LEN("}"));
					
					buffer_append_string_len(b, CONST_STR_LEN("}"));

					chunkqueue_append_buffer(con->write_queue, b);
					buffer_free(b);
					
					con->http_status = 200;
					con->file_finished = 1;
					return HANDLER_FINISHED;
				}
				
				free(post_data);
				free(action);
		
			}
			
		}

		con->http_status = 501;
		con->file_finished = 1;
		return HANDLER_FINISHED;
				
	}

	buffer *out;
	int is_valid_token = 0;
	buffer* buffer_token = buffer_init();
	buffer_copy_string_len(buffer_token, str, strlen(str));
	
	aicloud_acc_invite_info_t *c;
	for (c = aicloud_acc_invite_info_list; c; c = c->next) {
		if(buffer_is_equal(c->token, buffer_token)){
			if(c->status==0){
				is_valid_token = 1;
				break;
			}
		}
	}

	con->file_finished = 1;

	response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/html; charset=UTF-8"));
	
	out = buffer_init();
	
	if(is_valid_token==1){
		buffer_append_string_len(out, CONST_STR_LEN("<input type=\"hidden\" id=\"invite_token\" value=\""));
		buffer_append_string_buffer(out, buffer_token);
		buffer_append_string_len(out, CONST_STR_LEN("\">"));
						
		con->http_status = 461;
	}
	else{
		//- redirect to aicloud url
		//buffer_append_string_len(out, CONST_STR_LEN("<meta http-equiv=\"refresh\" content=\"0; url=/\" />"));
		//con->http_status = 200;
		con->http_status = 462;
	}

	buffer_free(buffer_token);

	chunkqueue_append_buffer(con->write_queue, out);
	buffer_free(out);
					
	return HANDLER_FINISHED;
}
//#ifndef APP_IPKG
int mod_aicloud_invite_plugin_init(plugin *p);
int mod_aicloud_invite_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("aicloud_invite");

	p->init        = mod_aicloud_invite_init;
	p->set_defaults = mod_aicloud_invite_set_defaults;
	p->handle_physical = mod_aicloud_invite_physical_handler;
	p->cleanup     = mod_aicloud_invite_free;

	p->data        = NULL;

	return 0;
}
//#else
/*
int aicloud_mod_aicloud_invite_plugin_init(plugin *p);
int aicloud_mod_aicloud_invite_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("aicloud_invite");

	p->init        = mod_aicloud_invite_init;
	p->set_defaults = mod_aicloud_invite_set_defaults;
	p->handle_physical = mod_aicloud_invite_physical_handler;
	p->cleanup     = mod_aicloud_invite_free;

	p->data        = NULL;

	return 0;
}*/
//#endif
