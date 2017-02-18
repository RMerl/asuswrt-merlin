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
#else
#include "disk_data.h"
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


INIT_FUNC(mod_query_field_json_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	return p;
}

FREE_FUNC(mod_query_field_json_free) {
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

SETDEFAULTS_FUNC(mod_query_field_json_set_defaults) {
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

URIHANDLER_FUNC(mod_query_field_json_physical_handler){
	plugin_data *p = p_d;
	int s_len;
	size_t k;
	int is_root_path = 0;
	char* param_val = NULL;
	
	if (con->mode != SMB_BASIC&&con->mode != DIRECT) return HANDLER_GO_ON;
	if (con->uri.path->used == 0) return HANDLER_GO_ON;
	
	if( strncmp( con->request.uri->ptr, "/query_field.json",  17 ) != 0 )
		return HANDLER_GO_ON;

	buffer* url_options = buffer_init();
	char* pch = strchr(con->request.uri->ptr, '?');
	if(pch){	
		buffer_copy_string_len(url_options, pch+1, strlen(pch)-1);
	}
	
	if(buffer_is_empty(url_options)){
		con->http_status = 200;
		con->file_finished = 1;
		return HANDLER_FINISHED;
	}

	char* usbdisk_name;
#if EMBEDDED_EANBLE
	char *a = nvram_get_productid();
	int l = strlen(a)+1;
	usbdisk_name = (char*)malloc(l);
	memset(usbdisk_name,'\0', l);
	strcpy(usbdisk_name, a);
	#ifdef APP_IPKG
	free(a);
	#endif
	char usbdisk_rel_root_path[20] = "/tmp/mnt/";
#else
	usbdisk_name = (char*)malloc(8);
	memset(usbdisk_name,'\0', 8);
	strcpy(usbdisk_name, "usbdisk");
	char usbdisk_rel_root_path[20] = "/mnt/";
#endif

	buffer* url_options_action = buffer_init();
	buffer* url_options_id = buffer_init();
	buffer* url_options_path = buffer_init();
	buffer* url_options_rel_path = buffer_init();
	buffer* url_options_account = buffer_init();
	
	param_val = get_url_param_ualue(url_options->ptr, "action");
	if(param_val!=NULL){
		buffer_copy_string( url_options_action, param_val );
		buffer_urldecode_path(url_options_action);	
		free(param_val);
	}
	
	param_val = get_url_param_ualue(url_options->ptr, "id");
	if(param_val!=NULL){
		buffer_copy_string( url_options_id, param_val );
		buffer_urldecode_path(url_options_id);	
		free(param_val);
	}

	param_val = get_url_param_ualue(url_options->ptr, "path");
	if(param_val!=NULL){
		buffer_copy_string( url_options_path, param_val );
		buffer_urldecode_path(url_options_path);	
		free(param_val);

		if(strcmp(url_options_path->ptr, "")==0 || strcmp(url_options_path->ptr, usbdisk_name)==0){
			buffer_copy_string(url_options_path, "/");
			buffer_append_string(url_options_path, usbdisk_name);

			is_root_path = 1;
		}
		
		//- replace 'usbdisk_name' to 'usbdisk_rel_root_path'
		char buff[4096]="\0";
		char* tmp2 = replace_str(url_options_path->ptr,
		                         usbdisk_name, 
		                  	     usbdisk_rel_root_path,
		                	     (char *)&buff[0]);

		buffer_copy_string(url_options_rel_path, "");
		buffer_append_string(url_options_rel_path, tmp2);
	}

	param_val = get_url_param_ualue(url_options->ptr, "account");
	if(param_val!=NULL){
		buffer_copy_string( url_options_account, param_val );
		buffer_urldecode_path(url_options_account);	
		free(param_val);
	}

	Cdbg(DBE, "url_options_action: %s", url_options_action->ptr);
	Cdbg(DBE, "url_options_path: %s", url_options_path->ptr);
	Cdbg(DBE, "url_options_rel_path: %s", url_options_rel_path->ptr);
	Cdbg(DBE, "url_options_account: %s", url_options_account->ptr);
	///////////////////////////////////////////////////////////////////////////
	
	response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("application/json"));

	buffer *b = buffer_init();
	
	buffer_append_string_len(b,CONST_STR_LEN("["));
	
	if(buffer_is_equal_string(url_options_action, CONST_STR_LEN("query_disk_folder"))){	
		folder_info_t *folder_list = NULL;

		buffer* url_options_account_type = buffer_init();
		param_val = get_url_param_ualue(url_options->ptr, "type");
		if(param_val!=NULL){
			buffer_copy_string( url_options_account_type, param_val );
			buffer_urldecode_path(url_options_account_type);	
			free(param_val);
		}

		Cdbg(DBE, "url_options_type: %s", url_options_account_type->ptr);
			
#if EMBEDDED_EANBLE
		if(is_root_path==1){
			disk_info_t *disks_info, *follow_disk;
			partition_info_t *follow_partition;
			char *partion_name;
			
			disks_info = read_disk_data();
		    if (disks_info != NULL){
				
		        for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
					for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
						if (follow_partition->mount_point == NULL){
							continue;
						}

						partion_name = rindex(follow_partition->mount_point, '/');

						if (partion_name == NULL){
							continue;
						}

						++partion_name;

						if ( check_skip_folder_name(partion_name) == 1 ) {
							continue;
						}
						
						folder_info_t* folder_info = calloc(1, sizeof(folder_info_t));
						folder_info->path = buffer_init();
					
						buffer_append_string_len(folder_info->path, CONST_STR_LEN("{"));

						//- parent id
						buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"parent\" : \""));
						buffer_append_string_buffer(folder_info->path, url_options_id);
						buffer_append_string_len(folder_info->path, CONST_STR_LEN("\", "));

						//- node text
						buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"text\" : \""));
						buffer_append_string(folder_info->path, partion_name);
						buffer_append_string_len(folder_info->path, CONST_STR_LEN("\", "));

						//- node state
						buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"state\" : {"));

						if(is_root_path==1)
							buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"opened\" : true"));
						else
							buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"opened\" : false"));
					
						buffer_append_string_len(folder_info->path, CONST_STR_LEN("}, "));

						//- has children
						buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"children\" : true, "));

						//- node path
						buffer* path = buffer_init();
						buffer_copy_buffer(path, url_options_path);
						buffer_append_slash(path);
						buffer_append_string(path, partion_name);
						buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"li_attr\" : {"));
						buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"data-path\" : \""));
						buffer_append_string_buffer(folder_info->path, path);
						buffer_append_string_len(folder_info->path, CONST_STR_LEN("\","));
						buffer_free(path);
						
						//- is root?
						if(is_root_path==1){
							buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"data-root\" : 1"));
						}
						else{
							buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"data-root\" : 0"));
						}

						buffer_append_string_len(folder_info->path, CONST_STR_LEN("}"));
						
						//- permission
						int permission = -1;
						if(!buffer_is_empty(url_options_account) && is_root_path==0){

#if EMBEDDED_EANBLE
							char buff[4096];
							
							char* tmp = replace_str(&url_options_path->ptr[0], 
							                    	usbdisk_name,
							                    	"/mnt",
								                	(char *)&buff[0]);
							
							Cdbg(DBE, "url_options_account=%s, tmp=%s, partion_name=%s", 
								url_options_account->ptr, tmp, partion_name);
							
							permission = smbc_get_usbdisk_permission(url_options_account->ptr, 
								                                     tmp, 
								                                     partion_name);
#else
							permission = 3;
#endif
						}
						
						buffer_append_string_len(folder_info->path, CONST_STR_LEN(", \"permission\" : "));
						char str_permission[10] = "\0";
						sprintf( str_permission, "%d", permission );
						buffer_append_string(folder_info->path, str_permission);
							
						buffer_append_string_len(folder_info->path, CONST_STR_LEN("}"));
							
						DLIST_ADD(folder_list, folder_info);

					}
				}

				free_disk_data(&disks_info);
		    }
			
		}
		else {
#endif

		DIR *dir;
		struct stat st;
		
		if (NULL != (dir = opendir(url_options_rel_path->ptr))) {
			struct dirent *de;
			physical d;

			d.path = buffer_init();
			d.rel_path = buffer_init();

			while(NULL != (de = readdir(dir))) {
				if ( de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0' ) {
					continue;
					//- ignore the parent dir
				}

				if ( de->d_name[0] == '.' ) {
					continue;
					//- ignore the hidden file
				}

				if ( check_skip_folder_name(de->d_name) == 1 ) {
					continue;
				}
				
				buffer_copy_buffer(d.path, url_options_path);
				buffer_append_slash(d.path);

				buffer_copy_buffer(d.rel_path, url_options_rel_path);
				buffer_append_slash(d.rel_path);
				
				if (de->d_name[0] == '.' && de->d_name[1] == '\0') {
					/* don't append the . */
				} else {
					buffer_append_string(d.path, de->d_name);
					buffer_append_string(d.rel_path, de->d_name);
				}
				
				if (-1 == stat(d.rel_path->ptr, &st)) {
					continue;
				} 

				if (!S_ISDIR(st.st_mode)) {
					continue;
				}
				
				folder_info_t* folder_info = calloc(1, sizeof(folder_info_t));
				folder_info->path = buffer_init();
					
				buffer_append_string_len(folder_info->path, CONST_STR_LEN("{"));

				//- parent id
				buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"parent\" : \""));
				buffer_append_string_buffer(folder_info->path, url_options_id);
				buffer_append_string_len(folder_info->path, CONST_STR_LEN("\", "));

				//- node text
				buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"text\" : \""));
				buffer_append_string(folder_info->path, de->d_name);
				buffer_append_string_len(folder_info->path, CONST_STR_LEN("\", "));

				//- node state
				buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"state\" : {"));

				if(is_root_path==1)
					buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"opened\" : true"));
				else
					buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"opened\" : false"));
					
				buffer_append_string_len(folder_info->path, CONST_STR_LEN("}, "));

				//- has children
				buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"children\" : true, "));

				//- node path
				buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"li_attr\" : {"));
				buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"data-path\" : \""));
				buffer_append_string_buffer(folder_info->path, d.path);
				buffer_append_string_len(folder_info->path, CONST_STR_LEN("\","));

				//- is root?
				if(is_root_path==1){
					buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"data-root\" : 1"));
				}
				else{
					buffer_append_string_len(folder_info->path, CONST_STR_LEN("\"data-root\" : 0"));
				}

				buffer_append_string_len(folder_info->path, CONST_STR_LEN("}"));
				
#if 1
				//- permission
				int permission = -1;
				if(!buffer_is_empty(url_options_account) && is_root_path==0){

			#if EMBEDDED_EANBLE
					char buff[4096];
					char* tmp = replace_str(&url_options_path->ptr[0], 
					                   	    usbdisk_name,
			        	            	    "/mnt",
				        	        	    (char *)&buff[0]);

					Cdbg(1, "url_options_account=%s, tmp=%s, de->d_name=%s", 
							url_options_account->ptr, tmp, de->d_name);

					permission = smbc_get_usbdisk_permission(url_options_account->ptr, 
							                                 tmp, 
							                                 de->d_name);			
			#else
					permission = 3;
			#endif
				}
				
				buffer_append_string_len(folder_info->path, CONST_STR_LEN(", \"permission\" : "));
				char str_permission[10] = "\0";
				sprintf( str_permission, "%d", permission );
				buffer_append_string(folder_info->path, str_permission);
#endif
					
				buffer_append_string_len(folder_info->path, CONST_STR_LEN("}"));
					
				DLIST_ADD(folder_list, folder_info);
								
			}

			closedir(dir);
		}

#if EMBEDDED_EANBLE
		}
#endif

		folder_info_t* c;
		
		for (c = folder_list; c; c = c->next) {		
			buffer_append_string_buffer(b, c->path);		
			if(c->next)
				buffer_append_string_len(b, CONST_STR_LEN(","));
		}

		//- Free
		while(1){
			for (c = folder_list; c; c = c->next) {
				buffer_free(c->path);
				DLIST_REMOVE(folder_list, c);
				free(c);
				break;
			}

			if(c==NULL)
				break;
		}
	}
	else if(buffer_is_equal_string(url_options_action, CONST_STR_LEN("query_aicam_folder"))){	
		
		DIR *mount_dir;
		DIR *partition_dir;
		DIR *aicam_dir;
		struct stat st;

		//- Open mount dir
		if (NULL != (mount_dir = opendir(usbdisk_rel_root_path))) {
			struct dirent *de;
			buffer* partition_path = buffer_init();
			
			while(NULL != (de = readdir(mount_dir))) {
				if ( de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0' ) {
					continue;
					//- ignore the parent dir
				}

				if ( de->d_name[0] == '.' ) {
					continue;
					//- ignore the hidden file
				}

				buffer_copy_string(partition_path, usbdisk_rel_root_path);

				if (de->d_name[0] == '.' && de->d_name[1] == '\0') {
					/* don't append the . */
				} else {
					buffer_append_string(partition_path, de->d_name);
				}

				//- Open partition dir
				if (NULL != (partition_dir = opendir(partition_path->ptr))) {
					struct dirent *de2;
					buffer* aicam_path = buffer_init();
					
					while(NULL != (de2 = readdir(partition_dir))) {
						if ( de2->d_name[0] == '.' && de2->d_name[1] == '.' && de2->d_name[2] == '\0' ) {
							continue;
							//- ignore the parent dir
						}

						if ( de2->d_name[0] == '.' ) {
							continue;
						}

						if(strcmp(de2->d_name, "AiCam")==0){
							buffer_copy_buffer(aicam_path, partition_path);
							buffer_append_slash(aicam_path);
							buffer_append_string(aicam_path, de2->d_name);

							//- Open aicam dir
							if (NULL != (aicam_dir = opendir(aicam_path->ptr))) {
								struct dirent *de3;
								buffer* aicam_record_path = buffer_init();
								int a = 0;
								
								while(NULL != (de3 = readdir(aicam_dir))) {
									if ( de3->d_name[0] == '.' && de3->d_name[1] == '.' && de3->d_name[2] == '\0' ) {
										continue;
									}

									if ( de3->d_name[0] == '.' ) {
										continue;
									}

									if(strstr(de3->d_name, "AiCam")){

										if(a==1){
											buffer_append_string_len(b, CONST_STR_LEN(","));
										}
										
										buffer_copy_buffer(aicam_record_path, aicam_path);
										buffer_append_slash(aicam_record_path);
										buffer_append_string(aicam_record_path, de3->d_name);

										char tmp[1024] = "\0";
										strcpy(tmp, "/");
										strcat(tmp, usbdisk_name);
										strcat(tmp, "/");

										char buff2[4096] = "\0";
										char* tmp2 = replace_str(aicam_record_path->ptr,
					                             				 usbdisk_rel_root_path, 
					                    	     				 tmp,
						                	                     (char *)&buff2[0]);

										char buff3[4096] = "\0";
										char* tmp3 = replace_str(aicam_path->ptr,
					                             				 usbdisk_rel_root_path, 
					                    	     				 tmp,
						                	                     (char *)&buff3[0]);
										
										buffer_append_string_len(b, CONST_STR_LEN("{"));
										buffer_append_string_len(b, CONST_STR_LEN("\"name\" : \""));
										buffer_append_string(b, de3->d_name);
										buffer_append_string_len(b, CONST_STR_LEN("\", "));
										buffer_append_string_len(b, CONST_STR_LEN("\"path\" : \""));
										buffer_append_string(b, tmp2);
										buffer_append_string_len(b, CONST_STR_LEN("\", "));
										buffer_append_string_len(b, CONST_STR_LEN("\"root_path\" : \""));
										buffer_append_string(b, tmp3);
										buffer_append_string_len(b, CONST_STR_LEN("\""));
										buffer_append_string_len(b, CONST_STR_LEN("}"));

										a = 1;
									}

									
								}

								buffer_free(aicam_record_path);
								closedir(aicam_dir);
							}
							
						}
					}

					buffer_free(aicam_path);
					closedir(partition_dir);
				}
				
			}

			buffer_free(partition_path);
			closedir(mount_dir);
		}
	}
	#if 0
	else if(buffer_is_equal_string(url_options_action, CONST_STR_LEN("query_account_list"))){	
		char* aicloud_db_file_path = get_aicloud_db_file_path();
		sqlite3 *sql_aicloud;
		char sql_query[2048] = "\0";
		char **result;
		int rows;
		int column_count;
		int row_count;

		char *nvram_acc_list;
		
#if EMBEDDED_EANBLE
		char *a = nvram_get_acc_list();
		if(a==NULL) return -1;
		int l = strlen(a);
		nvram_acc_list = (char*)malloc(l+1);
		strncpy(nvram_acc_list, a, l);
		nvram_acc_list[l] = '\0';
		#ifdef APP_IPKG
		free(a);
		#endif
#else
		int i = 100;
		nvram_acc_list = (char*)malloc(100);
		strcpy(nvram_acc_list, "admin>admin<jerry>jerry");
#endif
		char * pch;
		pch = strtok(nvram_acc_list, "<>");
		while(pch!=NULL){
			int len;
				
			//- User Name
			len = strlen(pch);
			char *name = (char*)malloc(len+1);
			strncpy(name, pch, len);
			name[len] = '\0';
			buffer* buffer_acc_name = buffer_init();
			buffer_copy_string(buffer_acc_name, name);
			buffer_urldecode_path(buffer_acc_name);
			free(name);

			//- User Password
			pch = strtok(NULL,"<>");
		
			buffer_append_string_len(b, CONST_STR_LEN("{"));
				
			//- id
			buffer_append_string_len(b, CONST_STR_LEN("\"id\" : \""));
			buffer_append_string_len(b, CONST_STR_LEN("\", "));

			//- username
			buffer_append_string_len(b, CONST_STR_LEN("\"username\" : \""));
			buffer_append_string_buffer(b, buffer_acc_name);
			buffer_append_string_len(b, CONST_STR_LEN("\", "));

			//- name
			buffer_append_string_len(b, CONST_STR_LEN("\"name\" : \""));
			buffer_append_string_buffer(b, buffer_acc_name);
			buffer_append_string_len(b, CONST_STR_LEN("\", "));
				
			//- type
			buffer_append_string_len(b, CONST_STR_LEN("\"type\" : \""));
			buffer_append_string(b, "samba");
			buffer_append_string_len(b, CONST_STR_LEN("\"}"));
			
			buffer_free(buffer_acc_name);

			//- next Name>Password
			pch = strtok(NULL,"<>");

			if(pch!=NULL){
				buffer_append_string_len(b, CONST_STR_LEN(","));
			}
		}
			
		free(nvram_acc_list);
		
		if (SQLITE_OK != sqlite3_open(aicloud_db_file_path, &(sql_aicloud))) {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}

		sprintf(sql_query, "SELECT id, username, name FROM account_info");
		if( sql_get_table(sql_aicloud, sql_query, &result, &rows, NULL) == SQLITE_OK ){
			int count = 0;
			int i=0;
			column_count = 3;
			row_count = 0;
			
			for( i=column_count; i<=rows*column_count; i+=column_count ){
				buffer_append_string_len(b, CONST_STR_LEN(",{"));

				//- id
				buffer_append_string_len(b, CONST_STR_LEN("\"id\" : \""));
				buffer_append_string(b, result[i]);
				buffer_append_string_len(b, CONST_STR_LEN("\", "));

				//- username
				buffer_append_string_len(b, CONST_STR_LEN("\"username\" : \""));
				buffer_append_string(b, result[i+1]);
				buffer_append_string_len(b, CONST_STR_LEN("\", "));

				//- name
				buffer_append_string_len(b, CONST_STR_LEN("\"name\" : \""));
				buffer_append_string(b, result[i+2]);
				buffer_append_string_len(b, CONST_STR_LEN("\", "));

				//- type
				buffer_append_string_len(b, CONST_STR_LEN("\"type\" : \""));
				buffer_append_string(b, "aicloud");
				buffer_append_string_len(b, CONST_STR_LEN("\"}"));
				
				//if(row_count!=rows-1)
				//	buffer_append_string_len(b, CONST_STR_LEN(","));

				row_count++;
			}
							
			sqlite3_free_table(result);
		}
			
		if (sql_aicloud) {
			sqlite3_close(sql_aicloud);
		}
	}
	
	else if(buffer_is_equal_string(url_options_action, CONST_STR_LEN("query_account_info"))){
		char* aicloud_db_file_path = get_aicloud_db_file_path();
		sqlite3 *sql_aicloud;
		char sql_query[2048] = "\0";
		char **result;
		int rows;
		int column_count;
		int row_count;
		int user_id = atoi(url_options_id->ptr);

		buffer* url_options_account_type = buffer_init();
		param_val = get_url_param_ualue(url_options->ptr, "type");
		if(param_val!=NULL){
			buffer_copy_string( url_options_account_type, param_val );
			buffer_urldecode_path(url_options_account_type);	
			free(param_val);
		}

		Cdbg(DBE, "url_options_account_type: %s", url_options_account_type->ptr);

		if(buffer_is_equal_string(url_options_account_type, CONST_STR_LEN("aicloud"))){
		
			if (SQLITE_OK != sqlite3_open(aicloud_db_file_path, &(sql_aicloud))) {
				con->http_status = 400;
				return HANDLER_FINISHED;
			}

			sprintf(sql_query, "SELECT username, password, name FROM account_info WHERE id=%d LIMIT 1", user_id);
			
			if( sql_get_table(sql_aicloud, sql_query, &result, &rows, NULL) == SQLITE_OK ){
				int count = 0;
				int i=0;
				column_count = 3;
				row_count = 0;
				
				for( i=column_count; i<=rows*column_count; i+=column_count ){

					buffer_append_string_len(b, CONST_STR_LEN("{"));
						
					//- username
					buffer_append_string_len(b, CONST_STR_LEN("\"username\" : \""));
					buffer_append_string(b, result[i]);
					buffer_append_string_len(b, CONST_STR_LEN("\", "));

					//- password
					buffer_append_string_len(b, CONST_STR_LEN("\"password\" : \""));
					buffer_append_string(b, result[i+1]);
					buffer_append_string_len(b, CONST_STR_LEN("\", "));

					//- name
					buffer_append_string_len(b, CONST_STR_LEN("\"name\" : \""));
					buffer_append_string(b, result[i+2]);
					buffer_append_string_len(b, CONST_STR_LEN("\"}"));
					
					row_count++;
				}
								
				sqlite3_free_table(result);
			}
				
			if (sql_aicloud) {
				sqlite3_close(sql_aicloud);
			}
		}
	}
	#endif
	
	buffer_append_string_len(b, CONST_STR_LEN("]"));
	
	//- Free
	free(usbdisk_name);
	buffer_free(url_options);
	buffer_free(url_options_action);
	buffer_free(url_options_id);
	buffer_free(url_options_path);
	buffer_free(url_options_rel_path);
	
	Cdbg(DBE, "b=%s", b->ptr);

	chunkqueue_append_buffer(con->write_queue, b);
	buffer_free(b);
	
	con->http_status = 200;
	con->file_finished = 1;
	return HANDLER_FINISHED;
}
#ifndef APP_IPKG
int mod_query_field_json_plugin_init(plugin *p);
int mod_query_field_json_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("query_field_json");

	p->init        = mod_query_field_json_init;
	p->set_defaults = mod_query_field_json_set_defaults;
	p->handle_physical = mod_query_field_json_physical_handler;
	p->cleanup     = mod_query_field_json_free;

	p->data        = NULL;

	return 0;
}
#else
int aicloud_mod_query_field_json_plugin_init(plugin *p);
int aicloud_mod_query_field_json_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("query_field_json");

	p->init        = mod_query_field_json_init;
	p->set_defaults = mod_query_field_json_set_defaults;
	p->handle_physical = mod_query_field_json_physical_handler;
	p->cleanup     = mod_query_field_json_free;

	p->data        = NULL;

	return 0;
}
#endif
