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

#define DBE 0

#ifndef EMBEDDED_EANBLE
char* g_is_webdav_block = "0";
#endif

handler_t basic_authentication_handler(server *srv, connection *con, plugin_data *p)
{
	data_string *ds_auth = (data_string *)array_get_element(con->request.headers, "Authorization");
	data_string *ds_useragent = (data_string *)array_get_element(con->request.headers, "user-Agent");
	
	buffer *user = buffer_init();
	buffer *pass = buffer_init();
	buffer_copy_string(user, " ");
	buffer_copy_string(pass, " ");
	
	int get_account_from_smb_info = 0;
	char *auth_username = NULL;
	char *auth_password = NULL;
	if( smbc_parser_basic_authentication(srv, con, &auth_username, &auth_password) != 1 ){
			
		if(con->smb_info==NULL)
			goto error_401;
		
		if( con->smb_info->username->used && con->smb_info->password->used ){
			buffer_copy_string_buffer(user, con->smb_info->username);			
			buffer_copy_string_buffer(pass, con->smb_info->password);
			get_account_from_smb_info = 1;
		}
		
		Cdbg(DBE, "fail smbc_parser_basic_authentication and get account from smb_info %s, %s", user->ptr, pass->ptr);
	}
	else{
		buffer_copy_string(user, auth_username);
		buffer_copy_string(pass, auth_password);
		free(auth_username);
		free(auth_password);
	}
	
	time_t cur_time = time(NULL);
	
	double result = difftime(cur_time, con->smb_info->auth_time);
	
	if(con->smb_info->qflag == SMB_HOST_QUERY) {
		//data_string *ds2 = (data_string *)array_get_element(con->request.headers, "user-Agent");	
	
		//- MUST login again more than 30 minutes
		if(result>1800){
			goto error_401;
		}
		
		if( con->smb_info && buffer_is_equal_string(con->smb_info->username, "RELOGIN", 7) ){
			buffer_reset(con->smb_info->username);
			buffer_reset(con->smb_info->password);
			goto error_401;
		}
		/*
		if(con->smb_info->username->used && con->smb_info->password->used){
			buffer_copy_string_buffer(user, con->smb_info->username);			
			buffer_copy_string_buffer(pass, con->smb_info->password);
			Cdbg(DBE, "SMB_HOST_QUERY-->copy from smb_info user=[%s], pass=[%s]", user->ptr, pass->ptr);
		}
		*/
		#if EMBEDDED_EANBLE
		char* webav_user = nvram_get_http_username();
		char* webav_pass = nvram_get_http_passwd();
		char* enable_webdav_block = nvram_get_enable_webdav_lock();
		char* is_webdav_block = nvram_get_webdav_acc_lock();
		#ifndef APP_IPKG
		int try_times = atoi(nvram_get_webdav_lock_times());
		int try_interval = atoi(nvram_get_webdav_lock_interval())*60;
		#else
		char* lock_times = nvram_get_webdav_lock_times();
		char* lock_interval = nvram_get_webdav_lock_interval();
		int try_times = atoi(lock_times);
        int try_interval = atoi(lock_interval)*60;
		#endif
		#else
		char* webav_user = "admin";
		char* webav_pass = "admin";
		char* enable_webdav_block = "1";
		char* is_webdav_block = g_is_webdav_block;
		int try_times = 3;
		int try_interval = 1*60; //- 1 minutes
		#endif
		int isBrowser = ( ds_useragent && (strstr( ds_useragent->value->ptr, "Mozilla" ) || strstr( ds_useragent->value->ptr, "Opera" ))) ? 1 : 0;

		if( isBrowser==1 && strcmp(enable_webdav_block, "1") == 0 && strcmp(is_webdav_block, "1") == 0 ){
			Cdbg(DBE, "Direct go to 455 error page");
#if EMBEDDED_EANBLE
#ifdef APP_IPKG
	        free(webav_user);
	        free(webav_pass);
	        free(enable_webdav_block);
	        free(is_webdav_block);
	        free(lock_times);
	        free(lock_interval);
#endif
#endif
			goto error_455;
		}
		
		if( strcmp(user->ptr, webav_user)!=0 || 
		    strcmp(pass->ptr, webav_pass)!=0 ){
			
			if( isBrowser==1 && strcmp(enable_webdav_block, "1") == 0 && con->smb_info && ds_auth!=NULL ){
			//if( isBrowser==1 && strcmp(enable_webdav_block, "1") == 0 && con->smb_info ){
				con->smb_info->login_count++;
				
				time_t current_time = time(NULL);
				
				double result2 = difftime(cur_time, con->smb_info->login_begin_time);
				if( result2 > try_interval ){
					con->smb_info->login_count = 1;
				}

				if(con->smb_info->login_count==1)
					con->smb_info->login_begin_time = time(NULL);
				
				Cdbg(DBE, "con->smb_info->login_count=[%d][%d]", con->smb_info->login_count, try_times);
				if(con->smb_info->login_count>=try_times){

					con->smb_info->login_count = 0;
					
					#if EMBEDDED_EANBLE
					nvram_set_webdav_acc_lock("1");
					nvram_do_commit();
					#else
					g_is_webdav_block = "1";
					#endif

					Cdbg(DBE, "error_455...");
#if EMBEDDED_EANBLE
#ifdef APP_IPKG
                    free(webav_user);
                    free(webav_pass);
                    free(enable_webdav_block);
                    free(is_webdav_block);
                    free(lock_times);
                    free(lock_interval);
#endif
#endif
					goto error_455;
				}
			}
#if EMBEDDED_EANBLE
#ifdef APP_IPKG
            free(webav_user);
            free(webav_pass);
            free(enable_webdav_block);
            free(is_webdav_block);
            free(lock_times);
            free(lock_interval);
#endif
#endif
			goto error_401;
		}
		
		if(con->smb_info){
			con->smb_info->login_count = 0;
		}

		if(!get_account_from_smb_info){
			log_sys_write(srv, "ssss", "User", user->ptr, "login from ip", con->dst_addr_buf->ptr);

			buffer_copy_string_buffer(srv->last_login_info, srv->cur_login_info);
			
			buffer_copy_string(srv->cur_login_info, user->ptr);
			buffer_append_string(srv->cur_login_info, ">");

			char srv_time[255];
			strftime(srv_time, 254, "%Y/%m/%d %H:%M:%S", localtime(&(srv->cur_ts)));
			buffer_append_string(srv->cur_login_info, srv_time);
			buffer_append_string(srv->cur_login_info, ">");
			
			buffer_append_string(srv->cur_login_info, con->dst_addr_buf->ptr);

			#if EMBEDDED_EANBLE
			nvram_set_webdav_last_login_info(srv->last_login_info->ptr);
			#endif
		}
#if EMBEDDED_EANBLE
#ifdef APP_IPKG
            free(webav_user);
            free(webav_pass);
            free(enable_webdav_block);
            free(is_webdav_block);
            free(lock_times);
            free(lock_interval);
#endif
#endif
	}
	else {
		
		//- check user / password
		struct stat st;
		
		if( con->smb_info && buffer_is_equal_string(con->smb_info->username, "RELOGIN", 7) ){
			buffer_reset(con->smb_info->username);
			buffer_reset(con->smb_info->password);

			goto error_401;
		}
		
		int res = smbc_server_check_creds( con->smb_info->server->ptr, 
									       con->smb_info->share->ptr, 
									       con->smb_info->workgroup->ptr, 
									       user->ptr, 
									       pass->ptr );
		
		//if( res == NT_STATUS_V(NT_STATUS_NOT_SUPPORTED)) {
		if( res == 0xc00000bb ){
			buffer_free(user);
			buffer_free(pass);
			con->http_status = 406;
			
			return HANDLER_FINISHED;
		}
		else if(res != 0) { //the username/password for smb_server is not correct		
			if(con->smb_info->username->used && con->smb_info->password->used){
				buffer_copy_string_buffer(user, con->smb_info->username);			
				buffer_copy_string_buffer(pass, con->smb_info->password);
				Cdbg(DBE, "Try to login again, server=%s, share=%s, user=%s, pass=%s", 
						con->smb_info->server->ptr,
						con->smb_info->share->ptr,
						user->ptr, pass->ptr);

				//- MUST login again more than 30 minutes
				if(result>1800){
					buffer_copy_string(pass, "");
				}
				
				int res = smbc_server_check_creds(con->smb_info->server->ptr, 
											    con->smb_info->share->ptr, 
											    con->smb_info->workgroup->ptr, 
											    user->ptr, 
											    pass->ptr);
				
				if(res != 0) 
					goto error_401;
			}
			else		
				goto error_401;
		}

		if(!get_account_from_smb_info)
			log_sys_write(srv, "sssbss", "User", user->ptr, "login", con->smb_info->server, "from ip", con->dst_addr_buf->ptr);
	}
	
	con->smb_info->auth_time = time(NULL);
	
	if( !buffer_is_equal_string(user, "no", 2) && !buffer_is_equal_string(pass, "no", 2)){
		buffer_copy_string_buffer(con->smb_info->username, user);
		buffer_copy_string_buffer(con->smb_info->password, pass);
		Cdbg(DBE, "save username=[%s], password=[%s], time=[%d] to con->smb_info", con->smb_info->username->ptr, con->smb_info->password->ptr, con->smb_info->auth_time);
	}
	
	buffer_free(user);
	buffer_free(pass);
	
	return HANDLER_UNSET;

error_401:
	buffer_free(user);
	buffer_free(pass);
	
	if(con->smb_info)
		con->smb_info->auth_time = time(NULL);
	
	smbc_wrapper_response_401(srv, con);
	
	return HANDLER_FINISHED;
	
error_455:
	//- Block webdav
	buffer_free(user);
	buffer_free(pass);
	con->http_status = 455;
	return HANDLER_FINISHED;
	
}
