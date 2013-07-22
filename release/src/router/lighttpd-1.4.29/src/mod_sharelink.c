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

#define DBE 0

typedef struct {
	array *alias_url;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

INIT_FUNC(mod_sharelink_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	return p;
}

FREE_FUNC(mod_sharelink_free) {
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

SETDEFAULTS_FUNC(mod_sharelink_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ "alias.url",             NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },
		{ NULL,                          NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->alias_url    = array_init();

		cv[0].destination = s->alias_url;

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv)) {
			return HANDLER_ERROR;
		}
	}

	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_sharelink_patch_connection(server *srv, connection *con, plugin_data *p) {
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

		/* merge config */
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

void substr(char *dest, const char* src, unsigned int start, unsigned int cnt) {
	strncpy(dest, src + start, cnt);
	dest[cnt] = 0;
}

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

URIHANDLER_FUNC(mod_sharelink_physical_handler){
	plugin_data *p = p_d;
	int s_len;
	size_t k;
	
	if (con->mode != SMB_BASIC&&con->mode != DIRECT) return HANDLER_GO_ON;
	
	if (con->uri.path->used == 0) return HANDLER_GO_ON;
	
	//- share_link_type: 0: none, 1: sharelink for general use, 2: sharelink for router sync
	if(con->share_link_type != 1)  return HANDLER_GO_ON;
	
	if(!con->share_link_shortpath->used)  return HANDLER_GO_ON;
	
	//char mnt_path[5] = "/mnt/";
	//int mlen = 5;
	struct stat st;
	int r;
	
	if( con->request.http_method != HTTP_METHOD_GET )
		return HANDLER_GO_ON;

	Cdbg(DBE, "mod_sharelink_physical_handler");
	
	mod_sharelink_patch_connection(srv, con, p);
	
	buffer* buffer_mnt_path = buffer_init();

#if EMBEDDED_EANBLE
	char *usbdiskname = nvram_get_productid();
#else
	char *usbdiskname = "usbdisk";
#endif

	for (k = 0; k < p->conf.alias_url->used; k++) {
		data_string *ds = (data_string *)p->conf.alias_url->data[k];

		if(strstr(ds->key->ptr, usbdiskname)){
			buffer_copy_string_buffer(buffer_mnt_path, ds->value);
			buffer_append_string(buffer_mnt_path, "/");
			break;
		}
	}
	
#if EMBEDDED_EANBLE
#ifdef APP_IPKG
	free(usbdiskname);
#endif
#endif

	if( con->mode == DIRECT && 
		strncmp( con->physical.path->ptr, buffer_mnt_path->ptr, buffer_mnt_path->used-1) == 0 ){
		r =  stat(con->physical.path->ptr, &st);
	}
	else if( con->mode == SMB_BASIC ){
		r =  smbc_wrapper_stat(con, con->url.path->ptr, &st);
	}

	buffer_free(buffer_mnt_path);
		
	if (-1 == r) {		
		con->http_status = 404;
		return HANDLER_FINISHED;
	}
	
	if (S_ISDIR(st.st_mode)) {
			
		buffer *out;

		out = chunkqueue_get_append_buffer(con->write_queue);
		buffer_copy_string_len(out, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\""));
		buffer_append_string_len(out, CONST_STR_LEN("utf-8"));
		buffer_append_string_len(out, CONST_STR_LEN("\"?>\n"));
			
		buffer_append_string_len(out, CONST_STR_LEN(
				"<!DOCTYPE html>\n"
				"<html>\n"
				"<head>\n"
				"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n"
				"<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">\n"
			));

		buffer_append_string_len(out, CONST_STR_LEN("<title>"));
		buffer_append_string_buffer(out, con->share_link_filename);
		buffer_append_string_len(out, CONST_STR_LEN("</title>"));
		
		buffer_append_string_len(out, CONST_STR_LEN(
				"<link rel='stylesheet' id='mainCss' href='/smb/css/sharelink.css' type='text/css'/>\n"
				"<script type='text/javascript' src='/smb/js/tools.js'></script>\n"
				"<script type='text/javascript' src='/smb/js/davclient_tools.js'></script>\n"
				"<script type='text/javascript' src='/smb/js/smbdav-sharelink.min.js'></script>\n"
			));
				
		buffer_append_string_len(out, CONST_STR_LEN("</head>\n<body>\n"));

		/* Insert possible charset to Content-Type */
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/html; charset=UTF-8"));

		buffer_append_string_len(out, CONST_STR_LEN("<div id='header_region' class='unselectable' width='100%' border='0'>"));
		buffer_append_string_len(out, CONST_STR_LEN("<div id='logo'></div></div>"));
			
		buffer_append_string_len(out, CONST_STR_LEN("<div id='toolbar' class='unselectable'>\n"));

		buffer_append_string_len(out, CONST_STR_LEN("<input type='button' class='btnDownloadAll' value='Download All'>"));		
		
		buffer_append_string_len(out, CONST_STR_LEN("</div>"));

		buffer_append_string_len(out, CONST_STR_LEN("<div id='fileview' class='unselectable' style='height:100%;'"));

		buffer_append_string_len(out, CONST_STR_LEN(" rootpath='/"));
		buffer_append_string_buffer(out, con->share_link_shortpath);
		buffer_append_string_len(out, CONST_STR_LEN("/"));
		//buffer_append_string_buffer(out, con->share_link_filename);
		buffer_append_string_encoded(out, CONST_BUF_LEN(con->share_link_filename), ENCODING_REL_URI);
		buffer_append_string_len(out, CONST_STR_LEN("'"));

		buffer_append_string_len(out, CONST_STR_LEN(" port='"));
#if EMBEDDED_EANBLE	
#ifndef APP_IPKG
		buffer_append_string(out, nvram_get_webdav_http_port());
#else
		char *webdav_http_port=nvram_get_webdav_http_port();
		buffer_append_string(out,webdav_http_port );
		free(webdav_http_port);
#endif
#else
		buffer_append_string_len(out, CONST_STR_LEN("8082"));
#endif
		buffer_append_string_len(out, CONST_STR_LEN("'"));
		
		buffer_append_string_len(out, CONST_STR_LEN(">\n"));
		
		buffer_append_string_len(out, CONST_STR_LEN("</div>\n"));
		
		buffer_append_string_len(out, CONST_STR_LEN("<div id='bottom_region' class='unselectable'>"));
		buffer_append_string_len(out, CONST_STR_LEN("<span>ASUSTeK Computer Inc. All rights reserved</span>"));
		buffer_append_string_len(out, CONST_STR_LEN("</div>"));

		buffer_append_string_len(out, CONST_STR_LEN("</body>\n"));
		
		con->file_finished = 1;
			
		return HANDLER_FINISHED;

	}
	
	/* not found */
	return HANDLER_GO_ON;
}
#ifndef APP_IPKG
int mod_sharelink_plugin_init(plugin *p);
int mod_sharelink_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("mod_sharelink");

	p->init        = mod_sharelink_init;
	p->set_defaults = mod_sharelink_set_defaults;
	p->handle_physical = mod_sharelink_physical_handler;
	p->cleanup     = mod_sharelink_free;

	p->data        = NULL;

	return 0;
}
#else
int aicloud_mod_sharelink_plugin_init(plugin *p);
int aicloud_mod_sharelink_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("mod_sharelink");

	p->init        = mod_sharelink_init;
	p->set_defaults = mod_sharelink_set_defaults;
	p->handle_physical = mod_sharelink_physical_handler;
	p->cleanup     = mod_sharelink_free;

	p->data        = NULL;

	return 0;
}
#endif