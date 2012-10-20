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

			array_free(s->access_deny);

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
static int mod_sharelink_patch_connection(server *srv, connection *con, plugin_data *p) {
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

void substr(char *dest, const char* src, unsigned int start, unsigned int cnt) {
	strncpy(dest, src + start, cnt);
	dest[cnt] = 0;
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
	if(con->is_share_link == 0)  return HANDLER_GO_ON;
	if(!con->share_link_shortpath->used)  return HANDLER_GO_ON;
	
	char mnt_path[5] = "/mnt/";
	int mlen = 5;
	struct stat st;
	int r;

	if( con->mode == DIRECT && strncmp( con->physical.path->ptr, mnt_path,  mlen ) == 0 ){
		r =  stat(con->physical.path->ptr, &st);
	}
	else if( con->mode == SMB_BASIC ){
		r =  smbc_wrapper_stat(con, con->url.path->ptr, &st);
	}

	if (-1 == r) {		
		/* don't about it yet, rmdir will fail too */
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
				"<title>AiCloud Share Link</title>\n"
			));
				
		buffer_append_string_len(out, CONST_STR_LEN(
				"<link rel='stylesheet' id='mainCss' href='/smb/css/sharelink.css' type='text/css'/>\n"
				"<script type='text/javascript' src='/smb/js/tools.js'></script>\n"
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

		buffer_append_string_len(out, CONST_STR_LEN("<div id='fileview' class='unselectable' style='height:100%;'>\n"));

		DIR *dir;
		
		if( con->mode == DIRECT ){
			if (NULL != (dir = opendir(con->physical.path->ptr))) {
				struct dirent *de;
				physical d;
				physical *dst = &(con->physical);

				d.path = buffer_init();
				d.rel_path = buffer_init();

				while(NULL != (de = readdir(dir))) {
					if ( de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0' ) {
						continue;
						/* ignore the parent dir */
					}

					if ( de->d_name[0] == '.' ) {
						continue;
						/* ignore the hidden file */
					}
					
					buffer_copy_string_buffer(d.path, dst->path);
					BUFFER_APPEND_SLASH(d.path);

					buffer_copy_string_buffer(d.rel_path, dst->rel_path);
					BUFFER_APPEND_SLASH(d.rel_path);

					if (de->d_name[0] == '.' && de->d_name[1] == '\0') {
						/* don't append the . */
					} else {
						buffer_append_string(d.path, de->d_name);
						buffer_append_string(d.rel_path, de->d_name);
					}

					Cdbg(DBE,"SMB_FILE_QUERY: d.path %s", d.path->ptr);

					r =  stat(d.path->ptr, &st);
					if (r != -1 && S_ISREG(st.st_mode))
						output_file_list(srv, con, d.path->ptr, de->d_name, out);
				}

				closedir(dir);
				buffer_free(d.path);
				buffer_free(d.rel_path);
			}
		}
		else if( con->mode == SMB_BASIC ){			
			if (-1 != (dir = smbc_wrapper_opendir(con, con->url.path->ptr))) {
				struct smbc_dirent *de;
				physical d;
					
				physical *dst = &(con->url);

				d.path = buffer_init();
				d.rel_path = buffer_init();

				while(NULL != (de = smbc_wrapper_readdir(con, dir))) {
					if( (de->name[0] == '.' && de->name[1] == '.' && de->name[2] == '\0') ||
						(de->name[0] == '.') ){
						continue;
						// ignore the parent dir
					}

					buffer* de_name = buffer_init();
					buffer_copy_string(de_name, "");						
					buffer_append_string_encoded(de_name, de->name, strlen(de->name), ENCODING_REL_URI);

					buffer_copy_string_buffer(d.path, dst->path);
					BUFFER_APPEND_SLASH(d.path);

					buffer_copy_string_buffer(d.rel_path, dst->rel_path);
					BUFFER_APPEND_SLASH(d.rel_path);

					if (de->name[0] == '.' && de->name[1] == '\0') {
						/* don't append the . */
					} else {
						buffer_append_string_buffer(d.path, de_name);
						buffer_append_string_buffer(d.rel_path, de_name);
					}
						
					int r = smbc_wrapper_stat(con, d.path->ptr, &st);
					if (r != -1 && S_ISREG(st.st_mode)) {
						//Cdbg(DBE,"SMB_FILE_QUERY: de->name %s, d.path %s", de_name->ptr, d.path->ptr);			
						output_file_list(srv, con, d.path->ptr, de_name->ptr, out);			
					}
					
					buffer_free(de_name);
						
				}
				smbc_wrapper_closedir(con,dir);
				buffer_free(d.path);
				buffer_free(d.rel_path);
			}
		}

		buffer_append_string_len(out, CONST_STR_LEN("</div>\n"));

		buffer_append_string_len(out, CONST_STR_LEN("<div id='bottom_region' class='unselectable'>"));
		buffer_append_string_len(out, CONST_STR_LEN("<span>ASUSTeK Computer Inc. All rights reserved</span>"));
		buffer_append_string_len(out, CONST_STR_LEN("</div>"));

		buffer_append_string_len(out, CONST_STR_LEN("</body>\n"));
		
		con->file_finished = 1;

		Cdbg(DBE, "123This is share link! %s", con->url.path->ptr);
			
		return HANDLER_FINISHED;

	}
	
	/* not found */
	return HANDLER_GO_ON;
}

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
