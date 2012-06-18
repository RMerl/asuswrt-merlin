#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <dlinklist.h>

#if EMBEDDED_EANBLE
#include "disk_share.h"
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

INIT_FUNC(mod_smbdav_access_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	return p;
}

FREE_FUNC(mod_smbdav_access_free) {
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

SETDEFAULTS_FUNC(mod_smbdav_access_set_defaults) {
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

		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv)) {
			return HANDLER_ERROR;
		}
	}

	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_smbdav_access_patch_connection(server *srv, connection *con, plugin_data *p) {
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

URIHANDLER_FUNC(mod_smbdav_access_physical_handler){
	plugin_data *p = p_d;
	int s_len;
	size_t k;

	if (con->mode != SMB_BASIC&&con->mode != DIRECT) return HANDLER_GO_ON;
	if (con->uri.path->used == 0) return HANDLER_GO_ON;

	char mnt_path[5] = "/mnt/";
	int mlen = 5;

	//- Check is the mount path
	if( strncmp( con->physical.path->ptr, mnt_path,  mlen ) != 0 )
		return HANDLER_GO_ON;
	
	char *auth_username = NULL;
	char *auth_password = NULL;

	data_string *ds = (data_string *)array_get_element(con->request.headers, "user-Agent");	
	smb_info_t *smb_info = NULL;
	if( ds && strstr( ds->value->ptr, "Mozilla" ) ){
		
		for (smb_info = srv->aidisk_info_list; smb_info; smb_info = smb_info->next) {

			Cdbg(DBE, "11 server=[%s], src_ip=[%s], user=[%s], pass=[%s]", 
					smb_info->server->ptr, smb_info->src_ip->ptr, smb_info->username->ptr, smb_info->password->ptr);
			
			if( buffer_is_equal(smb_info->user_agent, ds->value) && 
			    buffer_is_equal(smb_info->src_ip, con->dst_addr_buf) &&
			    buffer_is_equal_string(smb_info->server, "aidisk", 6) ){

				Cdbg(DBE, "server=[%s], user=[%s], pass=[%s]", 
					smb_info->server->ptr, smb_info->username->ptr, smb_info->password->ptr);
					
				break;
			}
		}
	}
		
	if( smbc_parser_basic_authentication(srv, con, &auth_username, &auth_password) != 1 ){
		if(smb_info==NULL){		
			Cdbg(DBE, "smbc_parser_basic_authentication fail");		
			smbc_wrapper_response_401(srv, con);
			return HANDLER_FINISHED;
		}

		auth_username = (char*)malloc(smb_info->username->used);
		strcpy(auth_username, smb_info->username->ptr);

		auth_password = (char*)malloc(smb_info->password->used);
		strcpy(auth_password, smb_info->password->ptr);
	}
	
	if( smbc_aidisk_account_authentication(con, auth_username, auth_password) == -1 ){
		Cdbg(DBE, "smbc_aidisk_account_authentication fail");
		free(auth_username);
		free(auth_password);
		smbc_wrapper_response_401(srv, con);
		return HANDLER_FINISHED;
	}

	if(smb_info==NULL){
		smb_info = calloc(1, sizeof(smb_info_t));
		smb_info->username = buffer_init();
		smb_info->password = buffer_init();
		//smb_info->workgroup = buffer_init();
		smb_info->server = buffer_init();
		//smb_info->share = buffer_init();
		//smb_info->path = buffer_init();
		smb_info->user_agent = buffer_init();
		smb_info->src_ip = buffer_init();			

		buffer_copy_string_buffer(smb_info->user_agent, ds->value);
		buffer_copy_string_buffer(smb_info->src_ip, con->dst_addr_buf);
		buffer_copy_string(smb_info->server, "aidisk");
		buffer_copy_string(smb_info->username, auth_username);
		buffer_copy_string(smb_info->password, auth_password);
		DLIST_ADD(srv->aidisk_info_list, smb_info);
	}
	
	int denied = 0;
	int permission = -1;
	char* usbdisk_path = NULL;
	char* usbdisk_share_folder = NULL;
	smbc_parse_mnt_path(con->physical.path->ptr, mnt_path, mlen, &usbdisk_path, &usbdisk_share_folder);
	
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

	Cdbg(DBE, "con->physical.path = %s, usbdisk_path = %s, usbdisk_share_folder=%s, permission=%d, denied=%d, con->request.http_method=%s, aidisk_username=%s", 
		con->physical.path->ptr, 
		usbdisk_path, 
		usbdisk_share_folder,
		permission, denied, 
		get_http_method_name(con->request.http_method),
		con->aidisk_username->ptr);
	
	free(usbdisk_path);
	free(usbdisk_share_folder);
	free(auth_username);
	free(auth_password);
	
	if (denied) {
		con->http_status = 403;
		con->mode = DIRECT;
		return HANDLER_FINISHED;
	}

	Cdbg(DBE, "Allow to access this file!");
	
	/* not found */
	return HANDLER_GO_ON;
}

int mod_smbdav_access_plugin_init(plugin *p);
int mod_smbdav_access_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("smbdav_access");

	p->init        = mod_smbdav_access_init;
	p->set_defaults = mod_smbdav_access_set_defaults;
	//p->handle_uri_clean = mod_smbdav_access_uri_handler;
	p->handle_physical = mod_smbdav_access_physical_handler;
	//p->handle_subrequest_start  = mod_smbdav_access_uri_handler;
	p->cleanup     = mod_smbdav_access_free;

	p->data        = NULL;

	return 0;
}
