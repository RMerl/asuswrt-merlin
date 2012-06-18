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

handler_t basic_authentication_handler(server *srv, connection *con, plugin_data *p)
{
	data_string *ds_auth = (data_string *)array_get_element(con->request.headers, "Authorization");
	
	buffer *user = buffer_init();
	buffer *pass = buffer_init();
	buffer_copy_string(user, " ");
	buffer_copy_string(pass, " ");
	
	//Cdbg(DBE, "*********** con->request.uri..%s", con->request.uri->ptr);
	
	char *auth_username = NULL;
	char *auth_password = NULL;
	if( smbc_parser_basic_authentication(srv, con, &auth_username, &auth_password) != 1 ){
			
		if(con->smb_info==NULL)
			goto error_401;

		//Cdbg(DBE, "1111111, %s, %s", con->smb_info->username->ptr, con->smb_info->password->ptr);

		if( con->smb_info->username->used && con->smb_info->password->used ){
			buffer_copy_string_buffer(user, con->smb_info->username);			
			buffer_copy_string_buffer(pass, con->smb_info->password);
		}
		
		Cdbg(DBE, "fail smbc_parser_basic_authentication-> %s, %s", user->ptr, pass->ptr);
	}
	else{
		buffer_copy_string(user, auth_username);
		buffer_copy_string(pass, auth_password);
		free(auth_username);
		free(auth_password);
	}

	time_t cur_time = time(NULL);
	
	double result = difftime(cur_time, con->smb_info->auth_time);
	Cdbg(DBE, "difftime=[%1f]", result);
	
	if(con->smb_info->qflag == SMB_HOST_QUERY) {
		data_string *ds2 = (data_string *)array_get_element(con->request.headers, "user-Agent");	
		
		//- MUST login again more than 30 minutes
		if(result>1800){
			goto error_401;
		}
		
		if( con->smb_info && buffer_is_equal_string(con->smb_info->username, "RELOGIN", 7) ){
			buffer_reset(con->smb_info->username);
			buffer_reset(con->smb_info->password);
			goto error_401;
		}
		
		if(con->smb_info->username->used && con->smb_info->password->used){
			buffer_copy_string_buffer(user, con->smb_info->username);			
			buffer_copy_string_buffer(pass, con->smb_info->password);
			Cdbg(DBE, "SMB_HOST_QUERY-->copy from smb_info user=[%s], pass=[%s]", user->ptr, pass->ptr);
		}

		#if EMBEDDED_EANBLE
		if( strcmp(user->ptr, nvram_get_http_username())!=0 || 
		     strcmp(pass->ptr, nvram_get_http_passwd())!=0 ){
			Cdbg(DBE, "smbc_host_account_authentication fail user=[%s], pass=[%s]", user->ptr, pass->ptr);
			goto error_401;
		}
		#else
		if(  strcmp(user->ptr, "admin")!=0 || 
		     strcmp(pass->ptr, "admin")!=0 ){
			Cdbg(DBE, "smbc_host_account_authentication fail user=[%s], pass=[%s]", user->ptr, pass->ptr);
			goto error_401;
		}
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
				
				//sprintf(strr, "smb://%s:%s@%s", user->ptr, pass->ptr, con->request.uri->ptr+1);
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
}
