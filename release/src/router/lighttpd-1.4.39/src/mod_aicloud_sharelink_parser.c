#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <dlinklist.h>
#include <dirent.h>

#if EMBEDDED_EANBLE
#ifndef APP_IPKG
#include "disk_share.h"
#endif
#endif

#define DBE 1

typedef struct {
	array *alias;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	plugin_config **config_storage;

	plugin_config conf;

	buffer *mnt_path;
} plugin_data;

INIT_FUNC(mod_aicloud_sharelink_parser_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	p->mnt_path = buffer_init();
	
	return p;
}

FREE_FUNC(mod_aicloud_sharelink_parser_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			array_free(s->alias);

			free(s);
		}
		free(p->config_storage);
	}

	buffer_free(p->mnt_path);
	
	free(p);

	return HANDLER_GO_ON;
}

#if 0
SETDEFAULTS_FUNC(mod_aicloud_sharelink_parser_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ "alias.url",             NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },
		{ NULL,                    NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->alias    = array_init();

		cv[0].destination = s->alias;

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}

#if EMBEDDED_EANBLE
		char *usbdiskname = nvram_get_productid();
#else
		char *usbdiskname = "usbdisk";
#endif
		for (size_t j = 0; j < s->alias->used; j++) {
			data_string *ds = (data_string *)s->alias->data[j];
			if(strstr(ds->key->ptr, usbdiskname)!=NULL){	
				buffer_copy_buffer(p->mnt_path, ds->value);
				
				if(strcmp( p->mnt_path->ptr + (p->mnt_path->used -1 ), "/" )!=0)
					buffer_append_string(p->mnt_path, "/");
			}
		}
#if EMBEDDED_EANBLE
#ifdef APP_IPKG
		free(usbdiskname);
#endif
#endif
	}

	return HANDLER_GO_ON;
}
#endif
SETDEFAULTS_FUNC(mod_aicloud_sharelink_parser_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ "alias.url",                  NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ NULL,                         NULL, T_CONFIG_UNSET,  T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(plugin_config *));
	
	for (i = 0; i < srv->config_context->used; i++) {
		data_config const* config = (data_config const*)srv->config_context->data[i];
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->alias = array_init();
		cv[0].destination = s->alias;

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, config->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}

#if EMBEDDED_EANBLE
		char *usbdiskname = nvram_get_productid();
#else
		char *usbdiskname = "usbdisk";
#endif

		#if 1
		if (s->alias->used >= 2) {
			const array *a = s->alias;
			size_t j, k;

			for (j = 0; j < a->used; j ++) {
				const buffer *prefix = a->data[a->sorted[j]]->key;
				for (k = j + 1; k < a->used; k ++) {
					const buffer *key = a->data[a->sorted[k]]->key;
					data_string *ds = (data_string *)a->data[a->sorted[k]];
					
					if(strstr(key->ptr, usbdiskname)!=NULL){
						buffer_copy_buffer(p->mnt_path, ds->value);
						
						if(strcmp( p->mnt_path->ptr + (p->mnt_path->used -1 ), "/" )!=0)
							buffer_append_string(p->mnt_path, "/");
					}
					
					if (buffer_string_length(key) < buffer_string_length(prefix)) {
						break;
					}
					if (memcmp(key->ptr, prefix->ptr, buffer_string_length(prefix)) != 0) {
						break;
					}
					
					/* ok, they have same prefix. check position */
					if (a->sorted[j] < a->sorted[k]) {
						log_error_write(srv, __FILE__, __LINE__, "SBSBS",
							"url.alias: `", key, "' will never match as `", prefix, "' matched first");
						return HANDLER_ERROR;
					}
				}
			}
		}
		#endif
		
#if EMBEDDED_EANBLE
#ifdef APP_IPKG
		free(usbdiskname);
#endif
#endif

	}

	return HANDLER_GO_ON;
}



//void substr(char *dest, const char* src, unsigned int start, unsigned int cnt) {
//	strncpy(dest, src + start, cnt);
//	dest[cnt] = 0;
//}

output_folder_list(server *srv, connection *con, char* fullpath, char* filename, buffer *out) {
	buffer* buffer_filename = buffer_init();
	buffer_copy_string(buffer_filename,filename);
	buffer_urldecode_path(buffer_filename);
	
	buffer_append_string_len(out, CONST_STR_LEN("<div class='albumDiv' title=''>\n"));
	buffer_append_string_len(out, CONST_STR_LEN("<table class='thumb-table-parent'>\n"));
	buffer_append_string_len(out, CONST_STR_LEN("<tbody><tr><td>\n"));
	buffer_append_string_len(out, CONST_STR_LEN("<div class='picDiv'>"));

	int use_http_connect = 0;
	
	buffer_append_string_len(out, CONST_STR_LEN("<div id='fileviewicon' class='folderDiv bicon'>"));
	
	buffer_append_string_len(out, CONST_STR_LEN("<div class='selectDiv sicon'></div>"));
	buffer_append_string_len(out, CONST_STR_LEN("<div class='selectHintDiv sicon'></div>"));
	buffer_append_string_len(out, CONST_STR_LEN("</div></div></td></tr><tr><td><div class='albuminfo' style='font-size:80%'>"));
	buffer_append_string_len(out, CONST_STR_LEN("<a id='list_item' qtype='1' isdir='0' playhref='"));

#if EMBEDDED_EANBLE
	char* webdav_http_port = nvram_get_webdav_http_port();
#else
	char* webdav_http_port = "8082";
#endif
	
	if(use_http_connect==1){
		buffer_append_string_len(out, CONST_STR_LEN("http://"));
		buffer_append_string_buffer(out, con->uri.authority);
		
		if( !strstr( con->uri.authority->ptr, ":" ) ){
			buffer_append_string_len(out, CONST_STR_LEN(":"));
			buffer_append_string(out, webdav_http_port);
		}
	}
	else{
		buffer_append_string_buffer(out, con->uri.scheme);
		buffer_append_string_len(out, CONST_STR_LEN("://"));
		buffer_append_string_buffer(out, con->uri.authority);
	}
	
	buffer_append_string_len(out, CONST_STR_LEN("/"));
	buffer_append_string_buffer(out, con->share_link_shortpath);
	buffer_append_string_len(out, CONST_STR_LEN("/"));
	buffer_append_string_buffer(out, con->share_link_filename);	
	buffer_append_string_len(out, CONST_STR_LEN("/"));
	buffer_append_string(out, filename);

	buffer_append_string_len(out, CONST_STR_LEN("' uhref='"));

	buffer_append_string_buffer(out, con->uri.scheme);
	buffer_append_string_len(out, CONST_STR_LEN("://"));
	buffer_append_string_buffer(out, con->uri.authority);
	buffer_append_string_len(out, CONST_STR_LEN("/"));
	buffer_append_string_buffer(out, con->share_link_shortpath);
	buffer_append_string_len(out, CONST_STR_LEN("/"));
	buffer_append_string_buffer(out, con->share_link_filename);	
	buffer_append_string_len(out, CONST_STR_LEN("/"));
	buffer_append_string(out, filename);
	
	buffer_append_string_len(out, CONST_STR_LEN("' title='"));
	buffer_append_string_buffer(out, buffer_filename);
	
	buffer_append_string_len(out, CONST_STR_LEN("' ext='"));
							
	buffer_append_string_len(out, CONST_STR_LEN("' freadonly='true' fhidden='false' target='_parent'>"));
	
	buffer_append_string_buffer( out, buffer_filename );
	buffer_append_string_len(out, CONST_STR_LEN("</a></div></td></tr>"));
	
	buffer_append_string_len(out, CONST_STR_LEN("</tbody></table></div>"));

	buffer_free(buffer_filename);
}

output_file_list(server *srv, connection *con, char* fullpath, char* filename, buffer *out) {

	buffer* buffer_filename = buffer_init();
	buffer_copy_string(buffer_filename,filename);
	buffer_urldecode_path(buffer_filename);
	
	buffer_append_string_len(out, CONST_STR_LEN("<div class='albumDiv' title=''>\n"));
	buffer_append_string_len(out, CONST_STR_LEN("<table class='thumb-table-parent'>\n"));
	buffer_append_string_len(out, CONST_STR_LEN("<tbody><tr><td>\n"));
	buffer_append_string_len(out, CONST_STR_LEN("<div class='picDiv'>"));

	int use_http_connect = 0;
	
	char* aa = get_filename_ext(fullpath);
	int len = strlen(aa)+1; 		
	char* file_ext = (char*)malloc(len);
	memset(file_ext,'\0', len);
	strcpy(file_ext, aa);
	for (int i = 0; file_ext[i]; i++)
		file_ext[i] = tolower(file_ext[i]);
							
	if( strcmp(file_ext,"jpg")==0 ||
		strcmp(file_ext,"jpeg")==0||
		strcmp(file_ext,"png")==0 ||
		strcmp(file_ext,"gif")==0 ||
		strcmp(file_ext,"bmp")==0 ){
		buffer_append_string_len(out, CONST_STR_LEN("<div id='fileviewicon' class='imgfileDiv bicon'>"));
	}
	else if( strcmp(file_ext,"mp3")==0 ||
			 strcmp(file_ext,"m4a")==0 ||
			 strcmp(file_ext,"m4r")==0 ||
			 strcmp(file_ext,"wav")==0 ){	
		use_http_connect = 1;
		buffer_append_string_len(out, CONST_STR_LEN("<div id='fileviewicon' class='audiofileDiv bicon'>"));
	}
	else if( strcmp(file_ext,"mp4")==0 ||
			 strcmp(file_ext,"rmvb")==0 ||
		 	 strcmp(file_ext,"m4v")==0 ||
			 strcmp(file_ext,"wmv")==0 ||
			 strcmp(file_ext,"avi")==0 ||
			 strcmp(file_ext,"mpg")==0 ||
			 strcmp(file_ext,"mpeg")==0 ||
			 strcmp(file_ext,"mkv")==0 ||
			 strcmp(file_ext,"mov")==0 ||
			 strcmp(file_ext,"flv")==0 ||
			 strcmp(file_ext,"3gp")==0 ||
			 strcmp(file_ext,"m2v")==0 ||
			 strcmp(file_ext,"rm")==0 ){
		use_http_connect = 1;
		buffer_append_string_len(out, CONST_STR_LEN("<div id='fileviewicon' class='videofileDiv bicon'>"));
	}
	else if(strcmp(file_ext,"doc")==0||strcmp(file_ext,"docx")==0)			
		buffer_append_string_len(out, CONST_STR_LEN("<div id='fileviewicon' class='docfileDiv bicon'>"));
	else if(strcmp(file_ext,"ppt")==0||strcmp(file_ext,"pptx")==0)
		buffer_append_string_len(out, CONST_STR_LEN("<div id='fileviewicon' class='pptfileDiv bicon'>"));
	else if(strcmp(file_ext,"xls")==0||strcmp(file_ext,"xlsx")==0)
		buffer_append_string_len(out, CONST_STR_LEN("<div id='fileviewicon' class='xlsfileDiv bicon'>"));
	else if(strcmp(file_ext,"pdf")==0)
		buffer_append_string_len(out, CONST_STR_LEN("<div id='fileviewicon' class='pdffileDiv bicon'>"));
	else
		buffer_append_string_len(out, CONST_STR_LEN("<div id='fileviewicon' class='fileDiv bicon'>"));
	
	buffer_append_string_len(out, CONST_STR_LEN("<div class='selectDiv sicon'></div>"));
	buffer_append_string_len(out, CONST_STR_LEN("<div class='selectHintDiv sicon'></div>"));
	buffer_append_string_len(out, CONST_STR_LEN("</div></div></td></tr><tr><td><div class='albuminfo' style='font-size:80%'>"));
	buffer_append_string_len(out, CONST_STR_LEN("<a id='list_item' qtype='1' isdir='0' playhref='"));

#if EMBEDDED_EANBLE
	char* webdav_http_port = nvram_get_webdav_http_port();
#else
	char* webdav_http_port = "8082";
#endif
	
	if(use_http_connect==1){
		buffer_append_string_len(out, CONST_STR_LEN("http://"));
		buffer_append_string_buffer(out, con->uri.authority);
		
		if( !strstr( con->uri.authority->ptr, ":" ) ){
			buffer_append_string_len(out, CONST_STR_LEN(":"));
			buffer_append_string(out, webdav_http_port);
#if EMBEDDED_EANBLE
#ifdef APP_IPKG
			free(webdav_http_port);
#endif
#endif
		}
	}
	else{
		buffer_append_string_buffer(out, con->uri.scheme);
		buffer_append_string_len(out, CONST_STR_LEN("://"));
		buffer_append_string_buffer(out, con->uri.authority);
	}
	
	buffer_append_string_len(out, CONST_STR_LEN("/"));
	buffer_append_string_buffer(out, con->share_link_shortpath);
	buffer_append_string_len(out, CONST_STR_LEN("/"));
	buffer_append_string_buffer(out, con->share_link_filename);	
	buffer_append_string_len(out, CONST_STR_LEN("/"));
	buffer_append_string(out, filename);

	buffer_append_string_len(out, CONST_STR_LEN("' uhref='"));

	buffer_append_string_buffer(out, con->uri.scheme);
	buffer_append_string_len(out, CONST_STR_LEN("://"));
	buffer_append_string_buffer(out, con->uri.authority);
	buffer_append_string_len(out, CONST_STR_LEN("/"));
	buffer_append_string_buffer(out, con->share_link_shortpath);
	buffer_append_string_len(out, CONST_STR_LEN("/"));
	buffer_append_string_buffer(out, con->share_link_filename);	
	buffer_append_string_len(out, CONST_STR_LEN("/"));
	buffer_append_string(out, filename);
	
	buffer_append_string_len(out, CONST_STR_LEN("' title='"));
	buffer_append_string_buffer(out, buffer_filename);
	
	buffer_append_string_len(out, CONST_STR_LEN("' ext='"));
							
	buffer_append_string(out, file_ext);
	free(file_ext);
							
	buffer_append_string_len(out, CONST_STR_LEN("' freadonly='true' fhidden='false'>"));
	
	buffer_append_string_buffer( out, buffer_filename );
	buffer_append_string_len(out, CONST_STR_LEN("</a></div></td></tr>"));

	buffer_append_string_len(out, CONST_STR_LEN("<tr><td>"));
	buffer_append_string_len(out, CONST_STR_LEN("<input type='button' class='btnDownload' value='Downloads!'>"));
	buffer_append_string_len(out, CONST_STR_LEN("</td></tr>"));
	
	buffer_append_string_len(out, CONST_STR_LEN("</tbody></table></div>"));

	buffer_free(buffer_filename);
	
}


int parser_share_link(server *srv, connection *con){	
	int result=0;
	
	if(strncmp(con->request.uri->ptr, "/ASUSHARE", 9)==0){
		
		char mac[20]="\0";		
		char* decode_str=NULL;
		int bExpired=0;

#if EMBEDDED_EANBLE
		#ifdef APP_IPKG
		char *router_mac=nvram_get_router_mac();
      	sprintf(mac,"%s",router_mac);
		free(router_mac);
		#else
		char *router_mac=nvram_get_router_mac();
		strcpy(mac, (router_mac==NULL) ? "00:12:34:56:78:90" : router_mac);
		#endif
#else
		get_mac_address("eth0", &mac);					
#endif
		
		Cdbg(DBE, "mac=%s", mac);
		
		char* key = ldb_base64_encode(mac, strlen(mac));		
		int y = strstr (con->request.uri->ptr+1, "/") - (con->request.uri->ptr);
		
		if(y<=9){
			if(key){
				free(key);
				key=NULL;
			}
			return -1;
		}
		
		buffer* filename = buffer_init();
		buffer_copy_string_len(filename, con->request.uri->ptr+1+y, con->request.uri->used-2-y);
		
		buffer* substrencode = buffer_init();
		buffer_copy_string_len(substrencode,con->request.uri->ptr+9,y-9);

		decode_str = x123_decode(substrencode->ptr, key, &decode_str);		
		Cdbg(DBE, "decode_str=%s", decode_str);

		if(decode_str){
			int x = strstr (decode_str,"?") - decode_str;

			int param_len = strlen(decode_str) - x;
			buffer* param = buffer_init();
			buffer* user = buffer_init();
			buffer* pass = buffer_init();
			buffer* expire = buffer_init();
			buffer_copy_string_len(param, decode_str + x + 1, param_len);
			//Cdbg(DBE, "123param=[%s]", param->ptr);

			char * pch;
			pch = strtok(param->ptr, "&");
			while(pch!=NULL){
			
				if(strncmp(pch, "auth=", 5)==0){

					int len = strlen(pch)-5;				
					char* temp = (char*)malloc(len+1);
					memset(temp,'\0', len);				
					strncpy(temp, pch+5, len);
					temp[len]='\0';

					buffer_copy_string( con->share_link_basic_auth, "Basic " );
					buffer_append_string( con->share_link_basic_auth, temp );

					if(temp){
						free(temp);
						temp=NULL;
					}
				}
				else if(strncmp(pch, "expire=", 7)==0){
					int len = strlen(pch)-7;				
					char* temp2 = (char*)malloc(len+1);
					memset(temp2,'\0', len);				
					strncpy(temp2, pch+7, len);
					temp2[len]='\0';
					Cdbg(DBE, "expire(temp2) = %s", temp2);

					unsigned long expire_time = atol(temp2);
					time_t cur_time = time(NULL);
					unsigned long cur_time2 = cur_time;
					
					//double offset = difftime((time_t)expire_time, cur_time);
					unsigned long offset = expire_time - cur_time;
					Cdbg(DBE, "expire_time=%lu, cur_time2=%lu, offset = %lu", expire_time, cur_time2, offset);
					if( offset < 0.0 ){
						buffer_reset(con->share_link_basic_auth);	
						bExpired = 1;
					}
					
					if(temp2){
						free(temp2);
						temp2=NULL;
					}
				}
				
				pch = strtok( NULL, "&" );
			}

			buffer_free(param);
			buffer_free(user);
			buffer_free(pass);
			buffer_free(expire);
			
			buffer_copy_string( con->share_link_shortpath, "ASUSHARE");
			buffer_append_string_buffer( con->share_link_shortpath, substrencode );
			buffer_append_slash(con->share_link_shortpath);
			
			buffer_copy_string_len( con->share_link_realpath, decode_str, x );
			buffer_append_slash(con->share_link_realpath);

			buffer_urldecode_path(filename);
			buffer_copy_buffer( con->share_link_filename, filename );

			buffer_reset(con->request.uri);
			buffer_copy_buffer(con->request.uri, con->share_link_realpath);
			buffer_append_string_buffer(con->request.uri, filename);

			con->share_link_type = 0;
			
			if(decode_str){
				free(decode_str);
				decode_str = NULL;
			}
		
		}
		
		buffer_free(substrencode);		
		buffer_free(filename);

		if(key){
			free(key);
			key=NULL;
		}

		Cdbg(DBE, "con->share_link_basic_auth=%s", con->share_link_basic_auth->ptr);
		Cdbg(DBE, "con->share_link_shortpath=%s", con->share_link_shortpath->ptr);
		Cdbg(DBE, "con->share_link_realpath=%s", con->share_link_realpath->ptr);
		Cdbg(DBE, "con->share_link_filename=%s", con->share_link_filename->ptr);
		Cdbg(DBE, "con->request.uri=%s", con->request.uri->ptr);

		if(bExpired==0)
			result = 1;
		else if(bExpired==1)
			result = -1;
		
	}
	else if(strncmp(con->request.uri->ptr, "/AICLOUD", 8)==0){
		int is_illegal = 0;
		int y = strstr (con->request.uri->ptr+1,"/") - (con->request.uri->ptr);
		
		//- Not valid share link
		if( y <= 8 )
			return -1;

		Cdbg(DBE, "AICLOUD sharelink, con->request.uri=%s", con->request.uri->ptr);
		
		buffer* filename = buffer_init();
		buffer_copy_string_len(filename, con->request.uri->ptr+y+1, con->request.uri->used-y);
		buffer_urldecode_path(filename);
		Cdbg(DBE, "filename=%s", filename->ptr);
				
		buffer* sharepath = buffer_init();
		buffer_copy_string_len(sharepath, con->request.uri->ptr+1, y-1);
		Cdbg(DBE, "sharepath=%s", sharepath->ptr );
		
		share_link_info_t* c=NULL;

		for (c = share_link_info_list; c; c = c->next) {
			if(buffer_is_equal(c->shortpath, sharepath)){				

				buffer_reset(con->share_link_basic_auth);	
				
				time_t cur_time = time(NULL);
				double offset = difftime(c->expiretime, cur_time);	

				if( c->expiretime!=0 && offset < 0.0 ){
					is_illegal = 1;
					free_share_link_info(c);
					DLIST_REMOVE(share_link_info_list, c);
					break;
				}

				buffer* filename2 = buffer_init();
				buffer_copy_buffer(filename2,c->filename);
				
				int com_result = strncmp( filename->ptr, filename2->ptr, filename2->used-1) ;

				buffer_free(filename2);

				if( com_result!= 0 ){					
					is_illegal = 1;					
					break;
				}

				buffer_copy_string( con->share_link_basic_auth, "Basic " );
				buffer_append_string_buffer( con->share_link_basic_auth, c->auth );

				buffer_copy_buffer( con->share_link_shortpath, c->shortpath );
				buffer_append_slash(con->share_link_shortpath);
				
				buffer_copy_buffer( con->share_link_realpath, c->realpath );
				buffer_append_slash(con->share_link_realpath);
				
				buffer_copy_buffer( con->share_link_filename, c->filename );
				
				//- share_link_type: 0: none, 1: sharelink for general use, 2: sharelink for router sync
				con->share_link_type = c->toshare;
				
				Cdbg(DBE, "realpath=%s, con->request.uri=%s, toShare=%d", c->realpath->ptr, con->request.uri->ptr, c->toshare);
				
				break;
			}
		}

		if(c==NULL||is_illegal==1){
			buffer_free(sharepath);
			buffer_free(filename);
			return -1;
		}
		
		buffer_reset(con->request.uri);		

#if 0
		buffer_copy_string_buffer(con->request.uri, c->realpath);
		buffer_append_string(con->request.uri, "/");
		buffer_append_string_buffer(con->request.uri, filename);
		//buffer_append_string_buffer(con->request.uri, c->filename);
#else
		buffer_copy_string(con->request.uri, "");
		buffer_append_string_encoded(con->request.uri, CONST_BUF_LEN(c->realpath), ENCODING_REL_URI);		
		buffer_append_string(con->request.uri, "/");
		buffer_append_string_encoded(con->request.uri, CONST_BUF_LEN(filename), ENCODING_REL_URI);
#endif

		buffer_free(sharepath);
		buffer_free(filename);

		Cdbg(DBE, "con->share_link_basic_auth=%s", con->share_link_basic_auth->ptr);
		Cdbg(DBE, "con->share_link_shortpath=%s", con->share_link_shortpath->ptr);
		Cdbg(DBE, "con->share_link_realpath=%s", con->share_link_realpath->ptr);
		Cdbg(DBE, "con->share_link_filename=%s", con->share_link_filename->ptr);
		Cdbg(DBE, "con->request.uri=%s", con->request.uri->ptr);

		if(con->share_link_type==1)
			log_sys_write(srv, "sbss", "Download file", c->filename, "from ip", con->dst_addr_buf->ptr);
		  
		return 1;
	}

	return result;
}

int redirect_mobile_share_link(server *srv, connection *con){
	data_string *ds_userAgent = (data_string *)array_get_element(con->request.headers, "user-Agent");
	char* aa = get_filename_ext(con->request.uri->ptr);
	int len = strlen(aa)+1; 		
	char* file_ext = (char*)malloc(len);
	memset(file_ext,'\0', len);
	strcpy(file_ext, aa);
	for (int i = 0; file_ext[i]; i++)
		file_ext[i] = tolower(file_ext[i]);

	if( con->share_link_basic_auth->used &&
		ds_userAgent && 
		con->conf.ssl_enabled &&
		( //strstr( ds_userAgent->value->ptr, "Chrome" ) ||
		  strstr( ds_userAgent->value->ptr, "Android"  ) ) ){

		buffer *out = buffer_init();
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/html; charset=UTF-8"));
		
	#if EMBEDDED_EANBLE
		char* webdav_http_port = nvram_get_webdav_http_port();
	#else
		char* webdav_http_port = "8082";
	#endif

		//- https -> http url
		buffer* redirect_url = buffer_init();
		buffer_copy_string_len(redirect_url, CONST_STR_LEN("http://"));
		
		char* b = strstr(con->request.http_host->ptr, ":");
		if(b!=NULL)
			buffer_append_string_len(redirect_url, con->request.http_host->ptr, b-con->request.http_host->ptr);
		else
			buffer_append_string_buffer(redirect_url, con->request.http_host);
		
		buffer_append_string_len(redirect_url, CONST_STR_LEN(":"));
		buffer_append_string(redirect_url, webdav_http_port);
		buffer_append_string_len(redirect_url, CONST_STR_LEN("/"));
		buffer_append_string_buffer(redirect_url, con->share_link_shortpath);
		buffer_append_string_encoded(redirect_url, con->share_link_filename->ptr, strlen(con->share_link_filename->ptr), ENCODING_REL_URI);		

		Cdbg(DBE, "redirect_url = %s", redirect_url->ptr);
				
		buffer_append_string_len(out, CONST_STR_LEN("<html>"));
		buffer_append_string_len(out, CONST_STR_LEN("<head>"));
		buffer_append_string_len(out, CONST_STR_LEN("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">"));
		buffer_append_string_len(out, CONST_STR_LEN("<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">"));
		buffer_append_string_len(out, CONST_STR_LEN("<meta http-equiv=\"refresh\" content=\"0;url="));
		buffer_append_string_buffer(out, redirect_url);
		buffer_append_string_len(out, CONST_STR_LEN("\">"));
		buffer_append_string_len(out, CONST_STR_LEN("</head>"));
		buffer_append_string_len(out, CONST_STR_LEN("</html>"));

		buffer_free(redirect_url);

		chunkqueue_append_buffer(con->write_queue, out);
		buffer_free(out);
		
		con->file_finished = 1;
		con->http_status = 200; 

		return 1;
	}
	else if( con->share_link_basic_auth->used &&
		ds_userAgent && 
		con->conf.ssl_enabled &&
		( strncmp(file_ext, "mp3", 3) == 0 ||
		  strncmp(file_ext, "mp4", 3) == 0 ||
		  strncmp(file_ext, "m4v", 3) == 0 ) &&
		( //strstr( ds_userAgent->value->ptr, "Chrome" ) ||
		  //strstr( ds_userAgent->value->ptr, "Android" ) || 
		  strstr( ds_userAgent->value->ptr, "iPhone" ) || 
		  strstr( ds_userAgent->value->ptr, "iPad"	 ) || 
		  strstr( ds_userAgent->value->ptr, "iPod"	 ) ) ){
					
		buffer *out = buffer_init();
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/html; charset=UTF-8"));
		
	#if EMBEDDED_EANBLE
		char* webdav_http_port = nvram_get_webdav_http_port();
	#else
		char* webdav_http_port = "8082";
	#endif
	
		buffer_append_string_len(out, CONST_STR_LEN("<div id=\"http_port\" content=\""));				
		buffer_append_string(out, webdav_http_port);
		buffer_append_string_len(out, CONST_STR_LEN("\"></div>\n"));
#ifdef APP_IPKG
#if EMBEDDED_EANBLE
        free(webdav_http_port);
#endif
#endif
		stat_cache_entry *sce = NULL;
					
		buffer* html_file = buffer_init();

#if EMBEDDED_EANBLE		
#ifndef APP_IPKG
		if( strncmp(file_ext, "mp3", 3) == 0 )
			buffer_copy_string(html_file, "/usr/lighttpd/css/iaudio.html");
		else if( strncmp(file_ext, "mp4", 3) == 0 ||
				 strncmp(file_ext, "m4v", 3) == 0 )
			buffer_copy_string(html_file, "/usr/lighttpd/css/ivideo.html");
#else
		if( strncmp(file_ext, "mp3", 3) == 0 )
			buffer_copy_string(html_file, "/opt/etc/aicloud_UI/css/iaudio.html");
		else if( strncmp(file_ext, "mp4", 3) == 0 ||
				 strncmp(file_ext, "m4v", 3) == 0 )
			buffer_copy_string(html_file, "/opt/etc/aicloud_UI/css/ivideo.html");
#endif
#else
		if( strncmp(file_ext, "mp3", 3) == 0 )
			buffer_copy_string(html_file, "/usr/css/iaudio.html");
		else if( strncmp(file_ext, "mp4", 3) == 0 ||
				 strncmp(file_ext, "m4v", 3) == 0 )
			buffer_copy_string(html_file, "/usr/css/ivideo.html");
#endif

		con->mode = DIRECT;

		chunkqueue_append_buffer(con->write_queue, out);
		buffer_free(out);
		
		if (HANDLER_ERROR != stat_cache_get_entry(srv, con, html_file, &sce)) {
			http_chunk_append_file(srv, con, html_file, 0, sce->st.st_size);
			response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_BUF_LEN(sce->content_type));
			con->file_finished = 1;
			con->http_status = 200; 				
		}
		else{
			con->file_finished = 1;
			con->http_status = 404;
		}
		
		Cdbg(DBE, "ds_userAgent->value=%s, file_ext=%s, html_file=%s", ds_userAgent->value->ptr, file_ext, html_file->ptr);
	
		free(file_ext);
		buffer_free(html_file);
					
		return 1;
	}
	
	free(file_ext);

	return 0;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_aicloud_sharelink_parser_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(alias);
	//Cdbg(1, "mod_aicloud_sharelink_parser_patch_connection..........");
#if EMBEDDED_EANBLE
	char *usbdiskname = nvram_get_productid();
#else
	char *usbdiskname = "usbdisk";
#endif

	//Cdbg(1, "2, srv->config_context->used=%d", srv->config_context->used);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		//Cdbg(1, "1, dc->string->ptr=%s", dc->string->ptr);
		
		/* condition didn't match */
		//if (!config_check_cond(srv, con, dc)) continue;
		//Cdbg(1, "2, dc->string->ptr=%s, usbdiskname=%s", dc->string->ptr, usbdiskname);
		if(strstr(dc->string->ptr, usbdiskname)==NULL)
			continue;
		//Cdbg(1, "3");
		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("alias.url"))) {
				//Cdbg(1, "du->key=%s", du->key->ptr);
				PATCH(alias);
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

//URIHANDLER_FUNC(mod_aicloud_sharelink_parser_physical_handler){
PHYSICALPATH_FUNC(mod_aicloud_sharelink_parser_physical_handler) {

	plugin_data *p = p_d;
	int s_len;
	size_t k;
	int uri_len, basedir_len;
	char *uri_ptr;
	
	if (con->mode != SMB_BASIC&&con->mode != DIRECT) return HANDLER_GO_ON;
		
	if (con->uri.path->used == 0) return HANDLER_GO_ON;
	
	//- If url is encrypted share link, then use basic auth
	int result = parser_share_link(srv, con);
	if( result == -1 ){		
		con->http_status = 404;
		return HANDLER_FINISHED;
	}
	else if( result == 0 )
		return HANDLER_GO_ON;
	
	/* not found */
	return HANDLER_GO_ON;
}
#ifndef APP_IPKG
int mod_aicloud_sharelink_parser_plugin_init(plugin *p);
int mod_aicloud_sharelink_parser_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("mod_aicloud_sharelink_parser");

	p->init        = mod_aicloud_sharelink_parser_init;
	p->set_defaults = mod_aicloud_sharelink_parser_set_defaults;
	p->handle_physical = mod_aicloud_sharelink_parser_physical_handler;
	p->cleanup     = mod_aicloud_sharelink_parser_free;

	p->data        = NULL;

	return 0;
}
#else
int aicloud_mod_aicloud_sharelink_parser_plugin_init(plugin *p);
int aicloud_mod_aicloud_sharelink_parser_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("mod_aicloud_sharelink_parser");

	p->init        = mod_aicloud_sharelink_parser_init;
	p->set_defaults = mod_aicloud_sharelink_parser_set_defaults;
	p->handle_physical = mod_aicloud_sharelink_parser_physical_handler;
	p->cleanup     = mod_aicloud_sharelink_parser_free;

	p->data        = NULL;

	return 0;
}
#endif
