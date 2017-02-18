#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_OPENSSL
# include <openssl/md5.h>
#else
# include "md5.h"

typedef li_MD5_CTX MD5_CTX;
#define MD5_Init li_MD5_Init
#define MD5_Update li_MD5_Update
#define MD5_Final li_MD5_Final

#endif

#if EMBEDDED_EANBLE 
#ifndef APP_IPKG
#include <utils.h>
#include <shutils.h>
#include <shared.h>
#endif
#include "nvram_control.h"
#if HAVE_PUSHLOG
#include <push_log.h>
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


INIT_FUNC(mod_captive_portal_uam_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	return p;
}

FREE_FUNC(mod_captive_portal_uam_free) {
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

SETDEFAULTS_FUNC(mod_captive_portal_uam_set_defaults) {
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

		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv, 
			i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}
	}

	return HANDLER_GO_ON;
}

/* Convert 32+1 octet ASCII hex string to 16 octet unsigned char */
static int hextochar(char *src, unsigned char * dst) {

  char x[3];
  int n;
  int y;
  
  for (n=0; n< MD5_DIGEST_LENGTH; n++) {
    x[0] = src[n*2+0];
    x[1] = src[n*2+1];
    x[2] = 0;
    if (sscanf (x, "%2x", &y) != 1) {
      //sys_err(LOG_ERR, __FILE__, __LINE__, 0, "HEX conversion failed!");
      return -1;
    }
    dst[n] = (unsigned char) y;
  }

  return 0;
}

/* Convert 16 octet unsigned char to 32+1 octet ASCII hex string */
static int chartohex(unsigned char *src, char *dst) {

  char x[3];
  int n;
  
  for (n=0; n<MD5_DIGEST_LENGTH; n++) {
    snprintf(x, 3, "%.2x", src[n]);
    dst[n*2+0] = x[0];
    dst[n*2+1] = x[1];
  }
  dst[MD5_DIGEST_LENGTH*2] = 0;
  return 0;
}

URIHANDLER_FUNC(mod_captive_portal_uam_physical_handler){
	//plugin_data *p = p_d;
	int flag = 0;
	
//	if (con->mode != SMB_BASIC&&con->mode != DIRECT) return HANDLER_GO_ON;
	if (con->uri.path->used == 0) return HANDLER_GO_ON;
	
	//if( strncmp( con->request.uri->ptr, "/hs/Uam",  7 ) != 0 )
	if( strncmp( con->request.uri->ptr, "/Uam",  4 ) != 0 ){
		if( strncmp( con->request.uri->ptr, "/FreeUam",  8 ) != 0 )
			return HANDLER_GO_ON;
	}
	buffer* url_options = buffer_init();
	char* pch = strchr(con->request.uri->ptr, '?');
	if(pch){	
		buffer_copy_string_len(url_options, pch+1, strlen(pch)-1);
	}
	
	if(buffer_is_empty(url_options)){
		return HANDLER_GO_ON;
	}

	if (strstr(url_options->ptr, "login=login"))
		flag = 1;	/* 1 for encoding password */
	else if (strstr(url_options->ptr, "res=success"))
		flag = 2;	/* 2 for redirect page */
	else if (strstr(url_options->ptr, "res=already"))
		flag = 2;	/* 2 for redirect page */
	else
		return HANDLER_GO_ON;

	buffer* url_opt_challenge = buffer_init();
	buffer* url_opt_uamip = buffer_init();
	buffer* url_opt_uamport = buffer_init();
	buffer* url_opt_username = buffer_init();
	buffer* url_opt_password = buffer_init();
	buffer* url_opt_userurl = buffer_init();

	pch = strtok(url_options->ptr, "&");
	while (pch!=NULL) {
		if(strncmp(pch, "chal=", 5)==0){
			int len = strlen(pch)-5;				
			char* temp = (char*)malloc(len+1);
			memset(temp,'\0', len);				
			strncpy(temp, pch+5, len);
			temp[len]='\0';

			buffer_copy_string(url_opt_challenge, temp );
			buffer_urldecode_path(url_opt_challenge);	
			free(temp);
		}
		else if(strncmp(pch, "uamip=", 6)==0){
			int len = strlen(pch)-6;				
			char* temp = (char*)malloc(len+1);
			memset(temp,'\0', len);				
			strncpy(temp, pch+6, len);
			temp[len]='\0';

			buffer_copy_string(url_opt_uamip, temp );
			buffer_urldecode_path(url_opt_uamip);	
			free(temp);
		}
		else if(strncmp(pch, "uamport=", 8)==0){
			int len = strlen(pch)-8;				
			char* temp = (char*)malloc(len+1);
			memset(temp,'\0', len);				
			strncpy(temp, pch+8, len);
			temp[len]='\0';

			buffer_copy_string(url_opt_uamport, temp );
			buffer_urldecode_path(url_opt_uamport);	
			free(temp);
		}
		else if(strncmp(pch, "UserName=", 9)==0){
			int len = strlen(pch)-9;				
			char* temp = (char*)malloc(len+1);
			memset(temp,'\0', len);				
			strncpy(temp, pch+9, len);
			temp[len]='\0';

			buffer_copy_string(url_opt_username, temp );
			buffer_urldecode_path(url_opt_username);	
			free(temp);
		}
		else if(strncmp(pch, "Password=", 9)==0){
			int len = strlen(pch)-9;				
			char* temp = (char*)malloc(len+1);
			memset(temp,'\0', len);				
			strncpy(temp, pch+9, len);
			temp[len]='\0';

			buffer_copy_string(url_opt_password, temp );
			buffer_urldecode_path(url_opt_password);	
			free(temp);
		}
		else if(strncmp(pch, "userurl=", 8)==0){
			int len = strlen(pch)-8;				
			char* temp = (char*)malloc(len+1);
			memset(temp,'\0', len);				
			strncpy(temp, pch+8, len);
			temp[len]='\0';

			buffer_copy_string(url_opt_userurl, temp );
			buffer_urldecode_path(url_opt_userurl);	
			free(temp);
		}
		pch = strtok(NULL, "&");
	}

	if (flag == 1)
	{
		unsigned char chap_challenge[MD5_DIGEST_LENGTH];
		unsigned char user_password[MD5_DIGEST_LENGTH + 1];
		unsigned char in_challenge[MD5_DIGEST_LENGTH];
		char *uam_secret = nvram_get_uamsecret(url_opt_uamip->ptr);
		unsigned char in_password[64];
		char out_password[MD5_DIGEST_LENGTH * 2 + 1];
		MD5_CTX context;
		int n = 0;
		char redir_url[256];

		memset(in_password, 0, sizeof(in_password));
		memset(out_password, 0, sizeof(out_password));
		memset(redir_url, 0, sizeof(redir_url));

		/* if url_opt_username is empty, using user name w/ noauth */
		if (url_opt_username->used == 0)
			buffer_copy_string(url_opt_username, "noauth");

		/* if url_opt_password is empty, using password w/ noauth */
		if (url_opt_password->used == 0)
			buffer_copy_string(url_opt_password, "noauth");

		hextochar(url_opt_challenge->ptr, in_challenge);
		snprintf(in_password, sizeof(in_password), "%s", url_opt_password->ptr);

		if (strlen(uam_secret)) {
			/* Get MD5 hash on challenge and uamsecret */
			MD5_Init(&context);
			MD5_Update(&context, in_challenge, MD5_DIGEST_LENGTH);
			MD5_Update(&context, (unsigned char*) uam_secret, strlen(uam_secret));
			MD5_Final(chap_challenge, &context);
		}
		else
			memcpy(chap_challenge, in_challenge, MD5_DIGEST_LENGTH);

		for (n=0; n<MD5_DIGEST_LENGTH; n++) 
			user_password[n] = in_password[n] ^ chap_challenge[n];
		user_password[MD5_DIGEST_LENGTH] = 0;
		chartohex(user_password, out_password);

#if 0
		if (strlen(uam_secret) && strlen(user_password))
			snprintf(redir_url, sizeof(redir_url), "url=http://%s:%s/logon?username=%s&password=%s", 
					url_opt_uamip->ptr, url_opt_uamport->ptr, url_opt_username->ptr, out_password);
		//else
		//	snprintf(redir_url, sizeof(redir_url), "url=http://%s:%s/logon?username=%s&response=%s&userurl", 
		//			url_opt_uamip->ptr, url_opt_uamport->ptr, url_opt_username->ptr, url_opt_password->ptr);			

		/* prepare the html content */
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/html"));
		buffer *b = chunkqueue_get_append_buffer(con->write_queue);

		buffer_append_string_len(b, CONST_STR_LEN("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">"));
		buffer_append_string_len(b, CONST_STR_LEN("<html><head><title>Logging in to Captive Portal</title>"));
		buffer_append_string_len(b, CONST_STR_LEN("<meta http-equiv=\"Cache-control\" content=\"no-cache\">"));
		buffer_append_string_len(b, CONST_STR_LEN("<meta http-equiv=\"Pragma\" content=\"no-cache\">"));
		buffer_append_string_len(b, CONST_STR_LEN("<meta http-equiv=\"refresh\" content=\"0;"));
		buffer_append_string(b, redir_url);
		buffer_append_string_len(b, CONST_STR_LEN("\">"));
    		buffer_append_string_len(b, CONST_STR_LEN("</head>"));
		buffer_append_string_len(b, CONST_STR_LEN("<body>"));
		buffer_append_string_len(b, CONST_STR_LEN("<h1 style=\"text-align: center;\">Logging in to Captive Portal</h1>"));
		buffer_append_string_len(b, CONST_STR_LEN("<center>Please wait......</center></body></html>"));

		buffer_free(url_options);
		buffer_free(url_opt_challenge);
		buffer_free(url_opt_uamip);
		buffer_free(url_opt_uamport);
		buffer_free(url_opt_username);
		buffer_free(url_opt_password);
#else
		if (strlen(uam_secret) && strlen(user_password))
			snprintf(redir_url, sizeof(redir_url), "http://%s:%s/logon?username=%s&password=%s&userurl=%s", 
					url_opt_uamip->ptr, url_opt_uamport->ptr, url_opt_username->ptr, out_password, url_opt_userurl->ptr);
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/html"));
		//buffer *b = chunkqueue_get_append_buffer(con->write_queue);
		buffer *b = buffer_init();
		buffer_append_string(b, redir_url);
		chunkqueue_append_buffer(con->write_queue, b);		
		
		buffer_free(url_options);
		buffer_free(url_opt_challenge);
		buffer_free(url_opt_uamip);
		buffer_free(url_opt_uamport);
		buffer_free(url_opt_username);
		buffer_free(url_opt_password);
		buffer_free(url_opt_userurl);
		buffer_free(b);
#endif

		con->http_status = 200;
		con->file_finished = 1;
		return HANDLER_FINISHED;
	}
	else if (flag == 2)
	{
		if (url_opt_userurl->used == 0) {
			buffer_free(url_options);
			buffer_free(url_opt_challenge);
			buffer_free(url_opt_uamip);
			buffer_free(url_opt_uamport);
			buffer_free(url_opt_username);
			buffer_free(url_opt_password);
			buffer_free(url_opt_userurl);

			return HANDLER_GO_ON;
		}
		else
		{
	#if 0 
			if(nvram_get_value("external_UI")){
			   char external_url[64];
			   memset(external_url, 0, sizeof(external_url));			   
			   sprintf(external_url, "%s:50000", nvram_get_value("external_UI"));
			   response_header_insert(srv, con, CONST_STR_LEN("Location"), external_url, strlen(external_url));
			   response_header_insert(srv, con, CONST_STR_LEN("Location"), nvram_get_value("external_UI"), strlen(nvram_get_value("external_UI")));

			}else

	#endif	  
			   response_header_insert(srv, con, CONST_STR_LEN("Location"), CONST_BUF_LEN(url_opt_userurl));
			buffer_free(url_options);
			buffer_free(url_opt_challenge);
			buffer_free(url_opt_uamip);
			buffer_free(url_opt_uamport);
			buffer_free(url_opt_username);
			buffer_free(url_opt_password);
			buffer_free(url_opt_userurl);

			con->http_status = 302;
			con->file_finished = 1;
			return HANDLER_FINISHED;
		}
	}
}

#ifndef APP_IPKG
int mod_captive_portal_uam_plugin_init(plugin *p);
int mod_captive_portal_uam_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("captive_portal_uam");

	p->init        = mod_captive_portal_uam_init;
	p->set_defaults = mod_captive_portal_uam_set_defaults;
	p->handle_physical = mod_captive_portal_uam_physical_handler;
	p->cleanup     = mod_captive_portal_uam_free;

	p->data        = NULL;

	return 0;
}
#else
int aicloud_mod_captive_portal_uam_plugin_init(plugin *p);
int aicloud_mod_captive_portal_uam_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("captive_portal_uam");

	p->init        = mod_captive_portal_v_init;
	p->set_defaults = mod_captive_portal_uam_set_defaults;
	p->handle_physical = mod_captive_portal_uam_physical_handler;
	p->cleanup     = mod_captive_portal_uam_free;

	p->data        = NULL;

	return 0;
}
#endif
