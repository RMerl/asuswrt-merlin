#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <dlinklist.h>
#include "md5.h"

#if EMBEDDED_EANBLE
#ifndef APP_IPKG
#include "disk_share.h"
#endif
#endif

#define DBE 1

typedef struct {
	array *access_deny;
	array *auth_deny;

#if 0
	int loglevel;
    buffer *name;    // cookie name to extract auth info
    int override;    // how to handle incoming Auth header
    buffer *authurl; // page to go when unauthorized
    buffer *key;     // key for cookie verification
    int timeout;     // life duration of last-stage auth token
    buffer *options; // options for last-stage auth token cookie
#endif

} plugin_config;

typedef struct {
	PLUGIN_DATA;

	plugin_config **config_storage;

	plugin_config conf;
	
	buffer *tmp_buf;
	
	//smb_info_t *smb_info_list;

#if 0
	array *users;
#endif

} plugin_data;

#if 0
#define HEADER(con, key)                                                \
    (data_string *)array_get_element((con)->request.headers, (key))

#define MD5_LEN 16

/**********************************************************************
 * supporting functions
 **********************************************************************/

//
// helper to generate "configuration in current context".
//
static plugin_config *
merge_config(server *srv, connection *con, plugin_data *pd) {
#define PATCH(x) pd->conf.x = pc->x
#define MATCH(k) if (buffer_is_equal_string(du->key, CONST_STR_LEN(k)))
#define MERGE(k, x) MATCH(k) PATCH(x)

    size_t i, j;
    plugin_config *pc = pd->config_storage[0]; // start from global context

    // load initial config in global context
    PATCH(loglevel);
    PATCH(name);
    PATCH(override);
    PATCH(authurl);
    PATCH(key);
    PATCH(timeout);
    PATCH(options);

    // merge config from sub-contexts
    for (i = 1; i < srv->config_context->used; i++) {
        data_config *dc = (data_config *)srv->config_context->data[i];

        // condition didn't match
        if (! config_check_cond(srv, con, dc)) continue;

        // merge config
        pc = pd->config_storage[i];
        for (j = 0; j < dc->value->used; j++) {
            data_unset *du = dc->value->data[j];

            // describe your merge-policy here...
            MERGE("auth-ticket.loglevel", loglevel);
            MERGE("auth-ticket.name", name);
            MERGE("auth-ticket.override", override);
            MERGE("auth-ticket.authurl", authurl);
            MERGE("auth-ticket.key", key);
            MERGE("auth-ticket.timeout", timeout);
            MERGE("auth-ticket.options", options);
        }
    }
    return &(pd->conf);
#undef PATCH
#undef MATCH
#undef MERGE
}

//
// fills (appends) given buffer with "current" URL.
//
static buffer *
self_url(connection *con, buffer *url, buffer_encoding_t enc) {
    buffer_append_string_encoded(url, CONST_BUF_LEN(con->uri.scheme), enc);
    buffer_append_string_encoded(url, CONST_STR_LEN("://"), enc);
    buffer_append_string_encoded(url, CONST_BUF_LEN(con->uri.authority), enc);
    buffer_append_string_encoded(url, CONST_BUF_LEN(con->request.uri), enc);
    return url;
}

//
// Generates appropriate response depending on policy.
//
static handler_t
endauth2(server *srv, connection *con, plugin_config *pc) {
    // pass through if no redirect target is specified
    if (buffer_is_empty(pc->authurl)) {
        Cdbg(DBE, "endauth - continuing");
        return HANDLER_GO_ON;
    }
    Cdbg(DBE, "endauth - redirecting:%s", pc->authurl->ptr);

    // prepare redirection header
    buffer *url = buffer_init_buffer(pc->authurl);
    buffer_append_string(url, strchr(url->ptr, '?') ? "&url=" : "?url=");
    self_url(con, url, ENCODING_REL_URI);
    response_header_insert(srv, con, 
                           CONST_STR_LEN("Location"), CONST_BUF_LEN(url));
    buffer_free(url);

    // prepare response
    con->http_status = 307;
    con->mode = DIRECT;
    con->file_finished = 1;

    return HANDLER_FINISHED;
}

static handler_t
endauth(server *srv, connection *con, plugin_config *pc) {    
    // prepare response
    con->http_status = 401;
    con->mode = DIRECT;
    con->file_finished = 1;

    return HANDLER_FINISHED;
}


inline static int
min(int a, int b) {
    return a > b ? b : a;
}

// generate hex-encoded random string
static int
gen_random(buffer *b, int len) {
    buffer_string_prepare_append(b, len);
    while (len--) {
        char c = int2hex(rand() >> 24);
        buffer_append_string_len(b, &c, 1);
    }
    return 0;
}

// encode bytes into hexstring
static int
hex_encode(buffer *b, const uint8_t *s, int len) {
    buffer_copy_string_hex(b, (const char *)s, len);
	return 0;
}

// decode hexstring into bytes
static int
hex_decode(buffer *b, const char *s) {
    char c0, c1;

    buffer_string_prepare_append(b, strlen(s) >> 1);
    while ((c0 = *s++) && (c1 = *s++)) {
        char v = (hex2int(c0) << 4) | hex2int(c1);
        buffer_append_string_len(b, &v, 1);
    }
    return 0;
}

// XOR-based encryption
static int
decrypt(buffer *buf, uint8_t *key, int keylen) {
    int i;

    for (i = buf->used - 1; i >= 0; i--) {
        buf->ptr[i] ^= (i > 0 ? buf->ptr[i - 1] : 0) ^ key[i % keylen];

        // sanity check - result should be base64-encoded authinfo
        if (! isprint(buf->ptr[i])) {
            return -1;
        }
    }
    return 0;
}

//
// update header using (verified) authentication info.
//
static void
update_header(server *srv, connection *con,
              plugin_data *pd, plugin_config *pc, buffer *authinfo) {
    buffer *field, *token;

    //DEBUG("sb", "decrypted authinfo:", authinfo);

    // insert auth header
    field = buffer_init_string("Basic ");

    //rescbr: bugfix: authinfo does not end with \0
    buffer_append_string_len(field, authinfo->ptr, authinfo->used);
    array_set_key_value(con->request.headers,
                        CONST_STR_LEN("Authorization"), CONST_BUF_LEN(field));

    // generate random token and relate it with authinfo
    gen_random(token = buffer_init(), MD5_LEN * 2); // length in hex string
    Cdbg(DBE, "pairing authinfo with token:%s", token->ptr);
    buffer_copy_int(field, time(NULL));
    buffer_append_string(field, ":");
    
    //rescbr: bugfix: authinfo does not end with \0
    buffer_append_string_len(field, authinfo->ptr, authinfo->used);
    array_set_key_value(pd->users, CONST_BUF_LEN(token), CONST_BUF_LEN(field));

    // insert opaque auth token
    buffer_copy_buffer(field, pc->name);
    buffer_append_string(field, "=token:");
    buffer_append_string_buffer(field, token);
    buffer_append_string(field, "; ");
    buffer_append_string_buffer(field, pc->options);
    Cdbg(DBE, "generating token cookie:%s", field->ptr);
    response_header_append(srv, con,
                           CONST_STR_LEN("Set-Cookie"), CONST_BUF_LEN(field));

    // update REMOTE_USER field
    base64_decode(field, authinfo->ptr);
    char *pw = strchr(field->ptr, ':'); *pw = '\0';
    Cdbg(DBE, "identified username:%s", field->ptr);

#if 0
#if LIGHTTPD_VERSION_ID < VER_ID(1, 4, 33)
    buffer_copy_string_len(con->authed_user, field->ptr, strlen(field->ptr));
#else
    data_string *ds;
    if (NULL == (ds = (data_string *)array_get_element(con->environment, "REMOTE_USER"))) {
        if (NULL == (ds = (data_string *)array_get_unused_element(con->environment, TYPE_STRING))) {
            ds = data_string_init();
        }
        buffer_copy_string(ds->key, "REMOTE_USER");
        array_insert_unique(con->environment, (data_unset *)ds);
    }
    buffer_copy_string_len(ds->value, field->ptr, strlen(field->ptr));
#endif
#else
	data_string *ds;
    if (NULL == (ds = (data_string *)array_get_element(con->environment, "REMOTE_USER"))) {
        if (NULL == (ds = (data_string *)array_get_unused_element(con->environment, TYPE_STRING))) {
            ds = data_string_init();
        }
        buffer_copy_string(ds->key, "REMOTE_USER");
        array_insert_unique(con->environment, (data_unset *)ds);
    }
    buffer_copy_string_len(ds->value, field->ptr, strlen(field->ptr));
#endif

    buffer_free(field);
    buffer_free(token);
}

//
// Handle token given in cookie.
//
// Expected Cookie Format:
//   <name>=token:<random-token-to-be-verified>
//
static handler_t
handle_token(server *srv, connection *con,
             plugin_data *pd, plugin_config *pc, char *token) {
    data_string *entry =
        (data_string *)array_get_element(pd->users, token);

    // Check for existence
    if (! entry) return endauth(srv, con, pc);

    Cdbg(DBE, "found token entry:%s", entry->value->ptr);

    // Check for timeout
    time_t t0 = time(NULL);
    time_t t1 = strtol(entry->value->ptr, NULL, 10);
    Cdbg(DBE, "t0: %d, t1: %d, timeout: %d", t0, t1, pc->timeout);
    if (t0 - t1 > pc->timeout) return endauth(srv, con, pc);

    // Check for existence of actual authinfo
    char *authinfo = strchr(entry->value->ptr, ':');
    if (! authinfo) return endauth(srv, con, pc);

    // All passed. Inject as BasicAuth header
    buffer *field = buffer_init_string("Basic ");
    buffer_append_string(field, authinfo + 1);
    array_set_key_value(con->request.headers,
                        CONST_STR_LEN("Authorization"), CONST_BUF_LEN(field));

    // update REMOTE_USER field
    base64_decode(field, authinfo + 1);
    char *pw = strchr(field->ptr, ':'); *pw = '\0';
    Cdbg(DBE, "identified user:", field->ptr);

#if 0
#if LIGHTTPD_VERSION_ID < VER_ID(1, 4, 33)
    buffer_copy_string_len(con->authed_user, field->ptr, strlen(field->ptr));
#else
    data_string *ds;
    if (NULL == (ds = (data_string *)array_get_element(con->environment, "REMOTE_USER"))) {
        if (NULL == (ds = (data_string *)array_get_unused_element(con->environment, TYPE_STRING))) {
            ds = data_string_init();
        }
        buffer_copy_string(ds->key, "REMOTE_USER");
        array_insert_unique(con->environment, (data_unset *)ds);
    }
    buffer_copy_string_len(ds->value, field->ptr, strlen(field->ptr));
#endif
#else
	data_string *ds;
    if (NULL == (ds = (data_string *)array_get_element(con->environment, "REMOTE_USER"))) {
        if (NULL == (ds = (data_string *)array_get_unused_element(con->environment, TYPE_STRING))) {
            ds = data_string_init();
        }
        buffer_copy_string(ds->key, "REMOTE_USER");
        array_insert_unique(con->environment, (data_unset *)ds);
    }
    buffer_copy_string_len(ds->value, field->ptr, strlen(field->ptr));
#endif

    buffer_free(field);

    Cdbg(DBE, "all check passed");
    return HANDLER_GO_ON;
}

//
// Check for redirected auth request in cookie.
//
// Expected Cookie Format:
//   <name>=crypt:<hash>:<data>
//
//   hash    = hex(MD5(key + timesegment + data))
//   data    = hex(encrypt(MD5(timesegment + key), payload))
//   payload = base64(username + ":" + password)
//
static handler_t
handle_crypt(server *srv, connection *con,
             plugin_data *pd, plugin_config *pc, char *line) {
    li_MD5_CTX ctx;
    uint8_t hash[MD5_LEN];
    char    tmp[256];

    // Check for existence of data part
    char *data = strchr(line, ':');
    if (! data) return endauth(srv, con, pc);

    Cdbg(DBE, "verifying crypt cookie...data=%s", data);

    // Verify signature.
    // Also, find time segment when this auth request was encrypted.
    time_t t1, t0 = time(NULL);
    buffer *buf = buffer_init();
    for (t1 = t0 - (t0 % 5); t0 - t1 < 10; t1 -= 5) {
        Cdbg(DBE, "t0: %d, t1: %d", t0, t1);

        // compute hash for this time segment
        sprintf(tmp, "%lu", t1);
        li_MD5_Init(&ctx);
        li_MD5_Update(&ctx, CONST_BUF_LEN(pc->key));
        li_MD5_Update(&ctx, tmp, strlen(tmp));
        li_MD5_Update(&ctx, data + 1, strlen(data + 1));
        li_MD5_Final(hash, &ctx);
        hex_encode(buf, hash, sizeof(hash));

        Cdbg(DBE, "computed hash: %s", buf->ptr);

        // verify by comparing hash
        if (strncasecmp(buf->ptr, line, data - line) == 0) {
            break; // hash verified and time segment found
        }
    }
    buffer_free(buf);

    // Has this found time segment expired?
    if (! (t0 - t1 < 10)) {
        Cdbg(DBE, "timeout detected");
        return endauth(srv, con, pc);
    }

    Cdbg(DBE, "timeout check passed");

    // compute temporal encryption key (= MD5(t1, key))
    sprintf(tmp, "%lu", t1);
    li_MD5_Init(&ctx);
    li_MD5_Update(&ctx, tmp, strlen(tmp));
    li_MD5_Update(&ctx, CONST_BUF_LEN(pc->key));
    li_MD5_Final(hash, &ctx);

    // decrypt
    hex_decode(buf = buffer_init(), data + 1);
    if (decrypt(buf, hash, sizeof(hash)) != 0) {
        Cdbg(DBE, "decryption error");

        buffer_free(buf);
        return endauth(srv, con, pc);
    }

    // update header using decrypted authinfo
    update_header(srv, con, pd, pc, buf);

    buffer_free(buf);
    return HANDLER_GO_ON;
}
#endif

/**********************************************************************
 * module interface
 **********************************************************************/

INIT_FUNC(mod_aicloud_auth_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

#if 0
	p->users = array_init();
#endif

	return p;
}

FREE_FUNC(mod_aicloud_auth_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

#if 0
	// Free plugin data
    array_free(p->users);
#endif

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			array_free(s->access_deny);
			array_free(s->auth_deny);

#if 0
			// free configuration
            buffer_free(s->name);
            buffer_free(s->authurl);
            buffer_free(s->key);
#endif

			free(s);
		}
		free(p->config_storage);
	}

	free(p);

	return HANDLER_GO_ON;
}

SETDEFAULTS_FUNC(mod_aicloud_auth_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ "url.access-deny",             NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },
		{ "url.aicloud-auth-deny",       NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },
#if 0
		{ "auth-ticket.loglevel",        NULL, T_CONFIG_INT, T_CONFIG_SCOPE_CONNECTION },
		{ "auth-ticket.name",            NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },
		{ "auth-ticket.override",        NULL, T_CONFIG_INT, T_CONFIG_SCOPE_CONNECTION },
		{ "auth-ticket.authurl",         NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },
		{ "auth-ticket.key",             NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },
		{ "auth-ticket.timeout",         NULL, T_CONFIG_INT, T_CONFIG_SCOPE_CONNECTION },
		{ "auth-ticket.options",         NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },
#endif
		{ NULL,                          NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));
	
	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->access_deny    = array_init();
		s->auth_deny      = array_init();

#if 0
		s->loglevel = 1;
        s->name     = buffer_init();
        s->override = 2;
        s->authurl  = buffer_init();
        s->key      = buffer_init();
        s->timeout  = 86400;
        s->options  = buffer_init();
#endif

		cv[0].destination = s->access_deny;
		cv[1].destination = s->auth_deny;

#if 0
		cv[2].destination = &(s->loglevel);
        cv[3].destination = s->name;
        cv[4].destination = &(s->override);
        cv[5].destination = s->authurl;
        cv[6].destination = s->key;
        cv[7].destination = &(s->timeout);
        cv[8].destination = s->options;
#endif

		p->config_storage[i] = s;
		
		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}
	}

	return HANDLER_GO_ON;
}

static void aicloud_check_direct_file(server *srv, connection *con)
{
	config_values_t cv[] = {
		{ "alias.url",		NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ NULL,             NULL, T_CONFIG_UNSET,  T_CONFIG_SCOPE_UNSET }
	};
	
	size_t i, j;
	for (i = 1; i < srv->config_context->used; i++) {
		int found = 0;
		array* alias = array_init();
		cv[0].destination = alias;

		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			continue;
		}
		
		for (j = 0; j < alias->used; j++) {
			data_string *ds = (data_string *)alias->data[j];			
						
			if( strncmp(con->request.uri->ptr, ds->key->ptr, ds->key->used-1) == 0 ){
				con->mode = DIRECT;
				found = 1;				
				break;
			}
		}

		array_free(alias);

		if(found==1)
			break;
	}
}

#define PATCH(x) \
	p->conf.x = s->x;

static int check_aicloud_auth_url(server *srv, connection *con, plugin_data *p){
	smb_info_t *c;
	int i, j, k;
	plugin_config *s;
	
	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("url.aicloud-auth-deny"))) {
				PATCH(auth_deny);
			}
		}
	}

	if(p->conf.auth_deny){
		
		for (k = 0; k < p->conf.auth_deny->used; k++) {
			data_string *ds = (data_string *)p->conf.auth_deny->data[k];
			
			if (ds->value->used == 0) continue;

			if (strstr(con->uri.path->ptr, ds->value->ptr)) {

				data_string *ds_user_agent = (data_string *)array_get_element(con->request.headers, "user-Agent");	
				if(!ds_user_agent){
					return 0;
				}
		
				if(srv->smb_srv_info_list==NULL)
					return 0;
				
				for (c = srv->smb_srv_info_list; c; c = c->next) {
					
					if( buffer_is_empty(c->server) &&
						buffer_is_empty(c->share) &&
						buffer_is_equal(c->src_ip, con->dst_addr_buf) &&
						buffer_is_equal(c->user_agent, ds_user_agent->value) ){
						
						if(buffer_is_empty(c->username)){
							return 0;
						}
						else{
							return 1;			
						}				
					}
				}

				return 0;
			}
		}

	}
	
	return 1;
}

static void get_aicloud_connection_auth_type(server *srv, connection *con)
{
	data_string *ds;
	int found = 0;
		
	aicloud_check_direct_file(srv, con);

	if(con->mode==DIRECT)
		return;

	if (NULL == (ds = (data_string *)array_get_element(con->request.headers, "user-Agent")))
		return;
		
	config_values_t cv[] = {
		{ "smbdav.auth_ntlm",    NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ NULL,                         NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};
	
	size_t i, j;
	for (i = 1; i < srv->config_context->used; i++) {
		int found = 0;
		array* auth_ntlm_array = array_init();
		cv[0].destination = auth_ntlm_array;

		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			continue;
		}
		
		for (j = 0; j < auth_ntlm_array->used; j++) {
			data_string *ds2 = (data_string *)auth_ntlm_array->data[j];

			if(ds2->key->used){
				/*
				Cdbg(DBE, "ds2->key=%s, ds2->value=%s", 
					ds2->key->ptr, 
					ds2->value->ptr );
				*/
				if( strncmp(ds->value->ptr, ds2->value->ptr, ds2->value->used-1) == 0 ){
					con->mode = SMB_NTLM;
					found = 1;
					break;
				}
			}
		}

		array_free(auth_ntlm_array);

		if(found==1)
			break;
	}

}

static void aicloud_connection_smb_info_url_patch(server *srv, connection *con)
{
	char strr[2048]="\0";
	char uri[2048]="\0";
	
	UNUSED(srv);
	
	char* pch = strchr(con->request.uri->ptr,'?');
	if(pch){	
		buffer_copy_string_len(con->url_options, pch+1, strlen(pch)-1);
		int len = pch-con->request.uri->ptr;
		strncpy(uri,con->request.uri->ptr, len);
	}
	else{
		strcpy(uri,con->request.uri->ptr);
	}
	
	if(con->mode == DIRECT){
		sprintf(strr, "%s", uri);
	}
	else {
		if(con->smb_info&&con->smb_info->server->used) {
			if(con->mode == SMB_BASIC){
				if(con->smb_info->username->used&&con->smb_info->password->used){
					sprintf(strr, "smb://%s:%s@%s", con->smb_info->username->ptr, con->smb_info->password->ptr, uri+1);
				}
				else
					sprintf(strr, "smb://%s", uri+1);
			}
			else if(con->mode == SMB_NTLM){
				sprintf(strr, "smb://%s", uri+1);		
			}
		} else {
			sprintf(strr, "smb://");
		}		
	}

	buffer_copy_string(con->url.path, strr);
	buffer_copy_string(con->url.rel_path, uri);	

	buffer_urldecode_path(con->url.path);
	buffer_urldecode_path(con->url.rel_path);
	
}

static smb_info_t *smbdav_get_smb_info_from_pool(server *srv, connection *con, plugin_data *p)
{
	smb_info_t *c;
	
	if(srv->smb_srv_info_list==NULL||con->mode==DIRECT)
		return NULL;
	
	//- Get user-Agent
	data_string *ds = (data_string *)array_get_element(con->request.headers, "user-Agent");
	if(ds==NULL){
		return NULL;
	}

	if(buffer_is_empty(con->asus_token)){
		return NULL;
	}

	char pWorkgroup[30]={0};
	char pServer[64]={0};
	char pShare[1280]={0};
	char pPath[1280]={0};
	
	smbc_wrapper_parse_path2(con, pWorkgroup, pServer, pShare, pPath);

	buffer* buffer_server = buffer_init();
	if(pServer[0] != '\0')
		buffer_append_string(buffer_server,pServer);
	
	buffer* buffer_share = buffer_init();
	if(pShare[0] != '\0')
		buffer_append_string(buffer_share,pShare);
	
	int count = 0;
	
	for (c = srv->smb_srv_info_list; c; c = c->next) {
		
		count++;

		Cdbg(DBE, "%d, c->asus_token=[%s], con->asus_token=[%s]", count, c->asus_token->ptr, con->asus_token->ptr);
		if(!buffer_is_equal(c->asus_token, con->asus_token))
			continue;

		Cdbg(DBE, "%d, c->server=[%s], buffer_server=[%s]", count, c->server->ptr, buffer_server->ptr);
		if(!buffer_is_equal(c->server, buffer_server))
			continue;

		//Cdbg(DBE, "c->share=[%s], buffer_share=[%s]", c->share->ptr, buffer_share->ptr);
		//if(con->mode==SMB_BASIC && !buffer_is_equal(c->share, buffer_share))
		//	continue;

		Cdbg(DBE, "%d, c->src_ip=[%s], dst_addr_buf=[%s]", count, c->src_ip->ptr, con->dst_addr_buf->ptr);
		if(!buffer_is_equal(c->src_ip, con->dst_addr_buf))
			continue;
		
		Cdbg(DBE, "%d, c->user_agent=[%s], user_agent=[%s]", count, c->user_agent->ptr, ds->value->ptr);
		if(!buffer_is_equal(c->user_agent, ds->value)){
			continue;
		}
		
		//Cdbg(DBE, "return %d, c->server=[%s]", count, c->server->ptr);

		buffer_free(buffer_server);
		buffer_free(buffer_share);
	
		return c;
	}

	buffer_free(buffer_server);
	buffer_free(buffer_share);
	
	return NULL;
}

static int aicloud_connection_smb_info_init(server *srv, connection *con, plugin_data *p) 
{
	UNUSED(srv);

	char pWorkgroup[30]={0};
	char pServer[64]={0};
	char pShare[1280]={0};
	char pPath[1280]={0};
	
	smbc_wrapper_parse_path2(con, pWorkgroup, pServer, pShare, pPath);
	
	buffer* bworkgroup = buffer_init();
	buffer* bserver = buffer_init();
	buffer* bshare = buffer_init();
	buffer* bpath = buffer_init();
	URI_QUERY_TYPE qflag = SMB_FILE_QUERY;
	
	if(pWorkgroup[0] != '\0')
		buffer_copy_string(bworkgroup, pWorkgroup);
	
	if(pServer[0] != '\0') {		
		int isHost = smbc_check_connectivity(con->physical_auth_url->ptr);
		if(isHost) {
			buffer_copy_string(bserver, pServer);
		}
		else{
			buffer_free(bworkgroup);
			buffer_free(bserver);
			buffer_free(bshare);
			buffer_free(bpath);
			return 2;
		}
	} 
	else {
		if(qflag == SMB_FILE_QUERY) {
			qflag = SMB_HOST_QUERY;
		}
	}
	
	if(pServer[0] != '\0' && pShare[0] != '\0') {
		buffer_copy_string(bshare, pShare);
	} 
	else {
		if(qflag == SMB_FILE_QUERY)  {
			qflag = SMB_SHARE_QUERY;
		}
	}
	
	if(pServer[0] != '\0' && pShare[0] != '\0' && pPath[0] != '\0') {
		buffer_copy_string(bpath, pPath);
		qflag = SMB_FILE_QUERY;
	}

	data_string *ds = (data_string *)array_get_element(con->request.headers, "user-Agent");

	smb_info_t *smb_info;

	if( isBrowser(con) == 1 || con->mode == SMB_NTLM ){
	    
		//- From browser, like IE, Chrome, Firefox, Safari		
		if(smb_info = smbdav_get_smb_info_from_pool(srv, con, p)){ 
			Cdbg(DBE, "Get smb_info from pool smb_info->qflag=[%d], smb_info->user=[%s], smb_info->pass=[%s]", 
				smb_info->qflag, smb_info->username->ptr, smb_info->password->ptr);
		}
		else{			
			smb_info = calloc(1, sizeof(smb_info_t));
			smb_info->username = buffer_init();
			smb_info->password = buffer_init();
			smb_info->workgroup = buffer_init();
			smb_info->server = buffer_init();
			smb_info->share = buffer_init();
			smb_info->path = buffer_init();
			smb_info->user_agent = buffer_init();
			smb_info->src_ip = buffer_init();
			smb_info->asus_token = buffer_init();
			
			if(con->mode == SMB_NTLM){
				smb_info->cli = smbc_cli_initialize();
				if(!buffer_is_empty(bserver)){
					smbc_cli_connect(smb_info->cli, bserver->ptr, SMB_PORT);
				}
				smb_info->ntlmssp_state = NULL; 
				smb_info->state = NTLMSSP_INITIAL;
			}

			DLIST_ADD(srv->smb_srv_info_list, smb_info);			
		}	
		con->smb_info = smb_info;
			
	}
	else{		
		smb_info = calloc(1, sizeof(smb_info_t));
		smb_info->username = buffer_init();
		smb_info->password = buffer_init();
		smb_info->workgroup = buffer_init();
		smb_info->server = buffer_init();
		smb_info->share = buffer_init();
		smb_info->path = buffer_init();
		smb_info->user_agent = buffer_init();
		smb_info->src_ip = buffer_init();			
		smb_info->asus_token = buffer_init();
		
		con->smb_info = smb_info;
	}
	
	con->smb_info->auth_time = time(NULL);	
	con->smb_info->auth_right = 0;

	if(ds)
		buffer_copy_string(con->smb_info->user_agent, ds->value->ptr);
	
	con->smb_info->qflag = qflag;
	buffer_copy_buffer(con->smb_info->workgroup, bworkgroup);
	buffer_copy_buffer(con->smb_info->server, bserver);
	buffer_copy_buffer(con->smb_info->share, bshare);
	buffer_copy_buffer(con->smb_info->path, bpath);
	buffer_copy_buffer(con->smb_info->src_ip, con->dst_addr_buf);
	buffer_copy_buffer(con->smb_info->asus_token, con->asus_token);
	
	Cdbg(DBE, "con->smb_info->workgroup=[%s]", con->smb_info->workgroup->ptr);
	Cdbg(DBE, "con->smb_info->server=[%s]", con->smb_info->server->ptr);
	Cdbg(DBE, "con->smb_info->share=[%s]", con->smb_info->share->ptr);
	Cdbg(DBE, "con->smb_info->path=[%s]", con->smb_info->path->ptr);
	Cdbg(DBE, "con->smb_info->user_agent=[%s]", con->smb_info->user_agent->ptr);
	Cdbg(DBE, "con->smb_info->src_ip=[%s]", con->smb_info->src_ip->ptr);
	Cdbg(DBE, "con->smb_info->qflag=[%d]", con->smb_info->qflag);
	Cdbg(DBE, "con->smb_info->asus_token=[%s]", con->smb_info->asus_token->ptr);
		
	buffer_free(bworkgroup);
	buffer_free(bserver);
	buffer_free(bshare);
	buffer_free(bpath);

	return 1;
}

void sambaname2ip(server *srv, connection *con){

#if EMBEDDED_EANBLE
	
	char* aa = nvram_get_smbdav_str();

	if(aa==NULL){
		return;
	}
	
	char* str_smbdav_list = (char*)malloc(strlen(aa)+1);
	strcpy(str_smbdav_list, aa);
	#ifdef APP_IPKG
	free(aa);
	#endif
	if(str_smbdav_list!=NULL){
		char * pch1;
		char * pch;
		pch = strtok(str_smbdav_list, "<>");	
		
		while(pch!=NULL){
			
			char name[50]={0}, ip[20]={0};
			int name_len, ip_len;
			
			//- PC Name
			name_len = strlen(pch);			
			strncpy(name, pch, name_len);
			name[name_len] = '\0';
						
			//- IP Address
			pch = strtok(NULL,"<>");
			ip_len = strlen(pch);
			strncpy(ip, pch, ip_len);
			ip[ip_len] = '\0';
						
			//- MAC Address
			pch = strtok(NULL,"<>");
						
			//- PC Online?
			pch = strtok(NULL,"<>");
			
			int index = strstr(con->request.uri->ptr, name) - con->request.uri->ptr;			
			if(index==1 && strcmp(pch, "1")==0){
				char buff[4096];
				char* tmp = replace_str(con->request.uri->ptr, 
									    name, 
									    ip, 
									    (char *)&buff[0]);
								
				buffer_copy_string(con->request.uri, tmp);	
				
				buffer_copy_string(con->match_smb_ip, ip);
				buffer_copy_string(con->replace_smb_name, name);
				
				break;
			}
			
			pch = strtok(NULL,"<>");			
		}

		free(str_smbdav_list);
	}
#else
	size_t j;
	int length, filesize;
	char* g_temp_file = "/tmp/arpping_list";
	FILE* fp = fopen(g_temp_file, "r");
	if(fp!=NULL){		
		
		char str[1024];

		while(fgets(str,sizeof(str),fp) != NULL)
		{
      		// strip trailing '\n' if it exists
      		int len = strlen(str)-1;
      		if(str[len] == '\n') 
         		str[len] = 0;

			char name[50]={0}, ip[20]={0};
			int name_len, ip_len;
			char * pch;

			//- PC Name
			pch = strtok(str,"<");
			name_len = strlen(pch);
			strncpy(name, pch, name_len);
			name[name_len] = '\0';
			
			//- IP Address
			pch = strtok(NULL,"<");
			ip_len = strlen(pch);
			strncpy(ip, pch, ip_len);
			ip[ip_len] = '\0';
			
			//- MAC Address
			pch = strtok(NULL,"<");
			
			//- PC Online?
			pch = strtok(NULL,"<");			

			int index = strstr(con->request.uri->ptr, name) - con->request.uri->ptr;			
			if(index==1 && strcmp(pch, "1")==0){
				char buff[4096];
				char* tmp = replace_str(con->request.uri->ptr, 
									    name, 
									    ip,
									    (char *)&buff[0]);
								
				buffer_copy_string(con->request.uri, tmp);	
				
				buffer_copy_string(con->match_smb_ip, ip);
				buffer_copy_string(con->replace_smb_name, name);				
				
				break;
			}
			
		}
			
		fclose(fp);
	}
#endif

}

URIHANDLER_FUNC(mod_aicloud_auth_physical_handler){
	plugin_data *p = p_d;
	//plugin_config *pc = merge_config(srv, con, p);
	int s_len;
	size_t k;
	int res = HANDLER_UNSET;
	//char buf[1024]; // cookie content
    //char key[32];   // <AuthName> key
    //char *cs;       // pointer to (some part of) <AuthName> key
	data_string *ds;
	
	if (con->uri.path->used == 0) return HANDLER_GO_ON;

#if EMBEDDED_EANBLE
#if 0
	data_string *ds_useragent = (data_string *)array_get_element(con->request.headers, "user-Agent");
	
	int isBrowser = ( ds_useragent && (strstr( ds_useragent->value->ptr, "Mozilla" ) || strstr( ds_useragent->value->ptr, "Opera" ))) ? 1 : 0;
	if( isBrowser && 
		con->srv_socket->is_ssl==0 && 
		buffer_is_equal_string(con->request.uri, CONST_STR_LEN("/")) ){
		con->http_status = 452;
		return HANDLER_FINISHED;
	}
#else
	if( con->srv_socket->is_ssl==0 && 
		buffer_is_equal_string(con->request.uri, CONST_STR_LEN("/")) ){
		con->http_status = 452;
		return HANDLER_FINISHED;
	}
#endif
#endif
	
	sambaname2ip(srv, con);

	//Cdbg(DBE, "con->request.uri=%s", con->request.uri->ptr);
		
	con->mode = SMB_BASIC;
	
	get_aicloud_connection_auth_type(srv, con);

	if( con->mode == DIRECT ){
		if( !check_aicloud_auth_url(srv, con, p) ){
			smbc_wrapper_response_401(srv, con);
			return HANDLER_FINISHED;
		}
		
		aicloud_connection_smb_info_url_patch(srv, con);
		return HANDLER_GO_ON;
	}
	else if( strncmp(con->request.uri->ptr, "/query_field.json", 17)==0 ||
		     strncmp(con->request.uri->ptr, "/GetCaptchaImage", 16)==0 ){
		if( !check_aicloud_auth_url(srv, con, p) ){
			smbc_wrapper_response_401(srv, con);
			return HANDLER_FINISHED;
		}
		return HANDLER_GO_ON;
	}
	
	Cdbg(DBE,"***************************************");
	Cdbg(DBE,"enter do_connection_auth..con->mode = %d, con->request.uri=[%s]", con->mode, con->request.uri->ptr);
	
	config_cond_cache_reset(srv, con);
	config_patch_connection(srv, con, COMP_SERVER_SOCKET);
	config_patch_connection(srv, con, COMP_HTTP_URL);
		
	buffer_copy_buffer(con->physical_auth_url, con->conf.document_root);
	buffer_append_string(con->physical_auth_url, con->request.uri->ptr+1);
	
	int result = aicloud_connection_smb_info_init(srv, con, p);		
	if( result == 0 ){
		return HANDLER_FINISHED;
	}
	else if( result == 2 ){
		//- Unable to complete the connection, the device is not turned on, or network problems caused!
		con->http_status = 451;
		return HANDLER_FINISHED;
	}
	
	if(con->mode == SMB_NTLM) {
		//try to get NTLM authentication information from HTTP request
		res = ntlm_authentication_handler(srv, con, p);		
	}
	else if(con->mode == SMB_BASIC){
		//try to get username/password from HTTP request
		res = basic_authentication_handler(srv, con, p);
	}

#if 0
	// check for cookie
	if ((ds = HEADER(con, "Cookie")) == NULL && res != HANDLER_UNSET) return endauth(srv, con, pc);
	
	if( ds!=NULL && res != HANDLER_UNSET ){
		Cdbg(DBE, "Cookie=%s", ds->value->ptr);
		// prepare cstring for processing
    	memset(key, 0, sizeof(key));
	    memset(buf, 0, sizeof(buf));
    	strncpy(key, pc->name->ptr,  min(sizeof(key) - 1, pc->name->used));
	    strncpy(buf, ds->value->ptr, min(sizeof(buf) - 1, ds->value->used));
		Cdbg(DBE, "parsing for key:%s", key);
		
		 // check for "<AuthName>=" entry in a cookie
	    for (cs = buf; (cs = strstr(cs, key)) != NULL; ) {
	        Cdbg(DBE, "checking cookie entry: %s", cs);

	        // check if found entry matches exactly for "KEY=" part.
	        cs += pc->name->used - 1;  // jump to the end of "KEY" part
	        while (isspace(*cs)) cs++; // whitespace can be skipped

	        // break forward if this was an exact match
	        if (*cs++ == '=') {
	            char *eot = strchr(cs, ';');
	            if (eot) *eot = '\0';
	            break;
	        }
	    }
		
	    if (! cs) return endauth(srv, con, pc); // not found - rejecting

		// unescape payload
	    buffer *tmp = buffer_init_string(cs);
	    buffer_urldecode_path(tmp);
	    memset(buf, 0, sizeof(buf));
	    strncpy(buf, tmp->ptr, min(sizeof(buf) - 1, tmp->used));
	    buffer_free(tmp);
	    cs = buf;

	    // Allow access if client already has an "authorized" token.
	    if (strncmp(cs, "token:", 6) == 0) {
	        return handle_token(srv, con, p, pc, cs + 6);
	    }

	    // Verify "non-authorized" CookieAuth request in encrypted format.
	    // Once verified, give out authorized token ("token:..." cookie).
	    if (strncmp(cs, "crypt:", 6) == 0) {
	        return handle_crypt(srv, con, p, pc, cs + 6);
	    }
	}
#endif

	//- 20120202 Jerry add
	//srv->smb_srv_info_list = p->smb_info_list;	
	aicloud_connection_smb_info_url_patch(srv, con);
	
	buffer_reset(con->physical_auth_url);

	if(res != HANDLER_UNSET){
		return HANDLER_FINISHED;
	}

	/* not found */
	return HANDLER_GO_ON;
}

int mod_aicloud_auth_plugin_init(plugin *p);
int mod_aicloud_auth_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("aicloud_auth");

	p->init        = mod_aicloud_auth_init;
	p->set_defaults = mod_aicloud_auth_set_defaults;	
	p->handle_physical = mod_aicloud_auth_physical_handler;
	p->cleanup     = mod_aicloud_auth_free;

	p->data        = NULL;

	return 0;
}
