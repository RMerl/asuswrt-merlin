#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <dlinklist.h>

#if EMBEDDED_EANBLE
#ifndef APP_IPKG
#include "disk_share.h"
#endif
#endif

#define DBE 1

typedef struct {
	array *alias_url;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

INIT_FUNC(mod_aidisk_access_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	return p;
}

FREE_FUNC(mod_aidisk_access_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			array_free(s->alias_url);

			free(s);
		}
		free(p->config_storage);
	}

	free(p);

	return HANDLER_GO_ON;
}

SETDEFAULTS_FUNC(mod_aidisk_access_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ "alias.url",                  NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },
		{ NULL,                          NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->alias_url    = array_init();
		
		cv[0].destination = s->alias_url;
		
		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}
	}

	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_aidisk_access_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(alias_url);

#if EMBEDDED_EANBLE
	char *usbdiskname = nvram_get_productid();
#else
	char *usbdiskname = "usbdisk";
#endif

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		if( dc->comp != COMP_HTTP_URL )
			continue;

		if(strstr(dc->string->ptr, usbdiskname)==NULL)
			continue;
		
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("alias.url"))) {				
				PATCH(alias_url);
			}
		}
	}
#if EMBEDDED_EANBLE
#ifdef APP_IPKG
	free(usbdiskname);
#endif
#endif

	return 0;
}
#undef PATCH

URIHANDLER_FUNC(mod_aidisk_access_physical_handler){
	plugin_data *p = p_d;
	int s_len;
	size_t k;

	if (con->mode != SMB_BASIC&&con->mode != DIRECT) return HANDLER_GO_ON;
	if (con->uri.path->used == 0) return HANDLER_GO_ON;

	mod_aidisk_access_patch_connection(srv, con, p);

	buffer* buffer_mnt_path = buffer_init();

#if EMBEDDED_EANBLE
	char *usbdiskname = nvram_get_productid();
#else
	char *usbdiskname = "usbdisk";
#endif
	
	int bFound = 0;
	for (k = 0; k < p->conf.alias_url->used; k++) {
		data_string *ds = (data_string *)p->conf.alias_url->data[k];

		if(strstr(ds->key->ptr, usbdiskname)){
			buffer_copy_buffer(buffer_mnt_path, ds->value);
			buffer_append_string(buffer_mnt_path, "/");
			bFound = 1;
			break;
		}
	}

#if EMBEDDED_EANBLE
#ifdef APP_IPKG
	free(usbdiskname);
#endif
#endif
	
	//- Check is the mount path
	if( bFound == 0 || strncmp( con->physical.path->ptr, buffer_mnt_path->ptr, buffer_mnt_path->used-1) != 0 ){
		buffer_free(buffer_mnt_path);
		return HANDLER_GO_ON;
	}

	char *auth_username = NULL;
	char *auth_password = NULL;

	data_string *ds = (data_string *)array_get_element(con->request.headers, "user-Agent");	
	
	smb_info_t *smb_info = NULL;
	int b_save_smb_info = 0;

#if 1	
	if( isBrowser(con) == 1 ){

		for (smb_info = srv->smb_srv_info_list; smb_info; smb_info = smb_info->next) {

			Cdbg(DBE, "smb_info->user_agent=[%s], ds->value=[%s]", 
					smb_info->user_agent->ptr, 
					ds->value->ptr);

			Cdbg(DBE, "smb_info->src_ip=[%s], con->dst_addr_buf=[%s]", 
					smb_info->src_ip->ptr, 
					con->dst_addr_buf->ptr);

			Cdbg(DBE, "smb_info->server=[%s]", 
					smb_info->server->ptr);

			Cdbg(DBE, "smb_info->asus_token=[%s]", 
					smb_info->asus_token->ptr);
			
			if( buffer_is_equal(smb_info->asus_token, con->asus_token) && 
				buffer_is_equal(smb_info->user_agent, ds->value) && 
			    buffer_is_equal(smb_info->src_ip, con->dst_addr_buf) &&
			    ( buffer_is_equal_string(smb_info->server, CONST_STR_LEN("aidisk")) ||
			      buffer_is_empty(smb_info->server) )){

				Cdbg(DBE, "server=[%s], user=[%s], pass=[%s]", 
					smb_info->server->ptr, smb_info->username->ptr, smb_info->password->ptr);
				
				break;
			}
			
		}
		
	}
#endif

	if( smbc_parser_basic_authentication(srv, con, &auth_username, &auth_password) != 1 ){
		if(smb_info==NULL){		
			Cdbg(DBE, "smbc_parser_basic_authentication fail 1");
			buffer_free(buffer_mnt_path);
			smbc_wrapper_response_401(srv, con);
			return HANDLER_FINISHED;
		}

		if(smb_info->username->ptr==NULL || smb_info->password->ptr==NULL){		
			Cdbg(DBE, "smbc_parser_basic_authentication fail 2");
			buffer_free(buffer_mnt_path);
			smbc_wrapper_response_401(srv, con);
			return HANDLER_FINISHED;
		}
		
		auth_username = (char*)malloc(smb_info->username->used);
		strcpy(auth_username, smb_info->username->ptr);

		auth_password = (char*)malloc(smb_info->password->used);
		strcpy(auth_password, smb_info->password->ptr);
	}


	int st_samba_mode = 0;
	
#if EMBEDDED_EANBLE
	st_samba_mode = nvram_get_st_samba_mode();
#endif

	//- Share All, use guest account, no need account and password.
	if(st_samba_mode==1){
		buffer_copy_string(con->aidisk_username, "guest");
		buffer_copy_string(con->aidisk_passwd, "");
	}
	else{
		//if( smbc_acc_account_authentication(con, auth_username, auth_password) != 1 ){
		if(do_account_authentication(auth_username, auth_password) != 1){
			Cdbg(DBE, "smbc_acc_account_authentication fail");
			free(auth_username);
			free(auth_password);
			buffer_free(buffer_mnt_path);
			smbc_wrapper_response_401(srv, con);
			return HANDLER_FINISHED;
		}

		buffer_copy_string(con->aidisk_username, auth_username);
		buffer_copy_string(con->aidisk_passwd, auth_password);
	}
	
	//- Save username password
	if( smb_info == NULL ){
		smb_info = calloc(1, sizeof(smb_info_t));
		smb_info->username = buffer_init();
		smb_info->password = buffer_init();
		smb_info->server = buffer_init();
		smb_info->user_agent = buffer_init();
		smb_info->src_ip = buffer_init();
		smb_info->asus_token = buffer_init();	

		if(ds)
			buffer_copy_buffer(smb_info->user_agent, ds->value);
		else
			buffer_copy_string(smb_info->user_agent, "");
		
		buffer_copy_buffer(smb_info->src_ip, con->dst_addr_buf);
		buffer_copy_string(smb_info->server, "aidisk");
		buffer_copy_string(smb_info->username, auth_username);
		buffer_copy_string(smb_info->password, auth_password);
		buffer_copy_buffer(smb_info->asus_token, con->asus_token);
		
		//DLIST_ADD(srv->aidisk_info_list, smb_info);
		DLIST_ADD(srv->smb_srv_info_list, smb_info);
	}
	
	int denied = 0;
	int permission = -1;
	char* usbdisk_path = NULL;
	char* usbdisk_share_folder = NULL;
	smbc_parse_mnt_path(con->physical.path->ptr, 
						buffer_mnt_path->ptr, 
						buffer_mnt_path->used-1, 
						&usbdisk_path, 
						&usbdisk_share_folder);
	
	Cdbg(DBE, "con->aidisk_username=%s, usbdisk_path=%s, usbdisk_share_folder=%s", con->aidisk_username->ptr, usbdisk_path, usbdisk_share_folder);
#if EMBEDDED_EANBLE
	permission = smbc_get_usbdisk_permission(con->aidisk_username->ptr, usbdisk_path, usbdisk_share_folder);	
#else
	//- For test
	permission = 3;
#endif	
	
	if( ( permission == 1 && con->request.http_method == HTTP_METHOD_GET ) ||
	      permission == 3 ||
	      con->request.http_method == HTTP_METHOD_PROPFIND ||
	      con->request.http_method == HTTP_METHOD_GSL )
		denied = 0;
	else
		denied = 1;

#if EMBEDDED_EANBLE
	if( con->share_link_type==1 && !con->srv_socket->is_ssl){
		Cdbg(DBE, "con->share_link_type=%d", con->share_link_type);
		denied = 1;
	}
#endif

	if(strstr(con->physical.path->ptr, "asusware")!=NULL){
		Cdbg(DBE, "2");
		denied = 1;
	}
	
	Cdbg(DBE, "con->physical.path = %s, usbdisk_path = %s, usbdisk_share_folder=%s, permission=%d, denied=%d, con->request.http_method=%s, aidisk_username=%s", 
		con->physical.path->ptr, 
		usbdisk_path, 
		usbdisk_share_folder,
		permission, denied, 
		get_http_method_name(con->request.http_method),
		con->aidisk_username->ptr);

	buffer_free(buffer_mnt_path);
	free(usbdisk_path);
	free(usbdisk_share_folder);
	free(auth_username);
	free(auth_password);
	
	if (denied) {		
		con->http_status = 403;
		con->mode = DIRECT;
		return HANDLER_FINISHED;
	}
	
	if(con->request.http_method == HTTP_METHOD_GET){
		log_sys_write(srv, "sbss", "Download cloud disk file", con->url.rel_path, "from ip", con->dst_addr_buf->ptr);
	}
	
	Cdbg(DBE, "Allow to access this file!");
	
	/* not found */
	return HANDLER_GO_ON;
}
#ifndef APP_IPKG
int mod_aidisk_access_plugin_init(plugin *p);
int mod_aidisk_access_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("smbdav_access");

	p->init        = mod_aidisk_access_init;
	p->set_defaults = mod_aidisk_access_set_defaults;
	//p->handle_uri_clean = mod_aidisk_access_uri_handler;
	p->handle_physical = mod_aidisk_access_physical_handler;
	//p->handle_subrequest_start  = mod_aidisk_access_uri_handler;
	p->cleanup     = mod_aidisk_access_free;

	p->data        = NULL;

	return 0;
}
#else
int aicloud_mod_aidisk_access_plugin_init(plugin *p);
int aicloud_mod_aidisk_access_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("smbdav_access");

	p->init        = mod_aidisk_access_init;
	p->set_defaults = mod_aidisk_access_set_defaults;
	//p->handle_uri_clean = mod_aidisk_access_uri_handler;
	p->handle_physical = mod_aidisk_access_physical_handler;
	//p->handle_subrequest_start  = mod_aidisk_access_uri_handler;
	p->cleanup     = mod_aidisk_access_free;

	p->data        = NULL;

	return 0;
}
#endif