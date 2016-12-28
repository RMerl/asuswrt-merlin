#include "base.h"
#include "log.h"
#include "buffer.h"
#include "response.h"

#include "plugin.h"

#include "stream.h"
#include "stat_cache.h"

#include "sys-mmap.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>

#include <unistd.h>
#include <dirent.h>
#include <dlinklist.h>
//#include <sys/socket.h>

//#define USE_LIBEXIF
#ifdef USE_LIBEXIF
#include <libexif/exif-loader.h>
#endif

#if EMBEDDED_EANBLE
#ifndef APP_IPKG
#include "disk_share.h"
#endif
#endif

#if defined(HAVE_LIBXML_H) && defined(HAVE_SQLITE3_H)
#define USE_PROPPATCH
#define USE_MINIDLNA_DB
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <sqlite3.h>
#endif

#include <stdarg.h>

#include <curl/curl.h>
#include <openssl/md5.h>

//#include "sql.h"
/*
#if defined(HAVE_LIBXML_H) && defined(HAVE_SQLITE3_H) && defined(HAVE_UUID_UUID_H)
#define USE_LOCKS
#include <uuid/uuid.h>
#endif
*/

#if defined(HAVE_LIBXML_H) && defined(HAVE_SQLITE3_H)
#define USE_LOCKS
#endif

#define MUSIC_ID		"1"
#define MUSIC_ALL_ID		"1$4"
#define MUSIC_GENRE_ID		"1$5"
#define MUSIC_ARTIST_ID		"1$6"
#define MUSIC_ALBUM_ID		"1$7"
#define MUSIC_PLIST_ID		"1$F"
#define MUSIC_DIR_ID		"1$14"
#define MUSIC_CONTRIB_ARTIST_ID	"1$100"
#define MUSIC_ALBUM_ARTIST_ID	"1$107"
#define MUSIC_COMPOSER_ID	"1$108"
#define MUSIC_RATING_ID		"1$101"

#define VIDEO_ID		"2"
#define VIDEO_ALL_ID		"2$8"
#define VIDEO_GENRE_ID		"2$9"
#define VIDEO_ACTOR_ID		"2$A"
#define VIDEO_SERIES_ID		"2$E"
#define VIDEO_PLIST_ID		"2$10"
#define VIDEO_DIR_ID		"2$15"
#define VIDEO_RATING_ID		"2$200"

#define IMAGE_ID		"3"
#define IMAGE_ALL_ID		"3$B"
#define IMAGE_DATE_ID		"3$C"
#define IMAGE_ALBUM_ID		"3$D"
#define IMAGE_CAMERA_ID		"3$D2" // PlaysForSure == Keyword
#define IMAGE_PLIST_ID		"3$11"
#define IMAGE_DIR_ID		"3$16"
#define IMAGE_RATING_ID		"3$300"

/**
 * this is a webdav for a lighttpd plugin
 *
 * at least a very basic one.
 * - for now it is read-only and we only support PROPFIND
 *
 */

#define WEBDAV_FILE_MODE S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
#define WEBDAV_DIR_MODE  S_IRWXU | S_IRWXG | S_IRWXO

#define DBG_ENABLE_MOD_SMBDAV 1
#define DBE	DBG_ENABLE_MOD_SMBDAV

/* plugin config for all request/connections */

typedef struct {
	unsigned short enabled;
	unsigned short is_readonly;
	unsigned short log_xml;

	buffer *sqlite_db_name;
	array  *alias_url;
#ifdef USE_PROPPATCH
	sqlite3 *sql;
	sqlite3_stmt *stmt_update_prop;
	sqlite3_stmt *stmt_delete_prop;
	sqlite3_stmt *stmt_select_prop;
	sqlite3_stmt *stmt_select_propnames;

	sqlite3_stmt *stmt_delete_uri;
	sqlite3_stmt *stmt_move_uri;
	sqlite3_stmt *stmt_copy_uri;

	sqlite3_stmt *stmt_remove_lock;
	sqlite3_stmt *stmt_create_lock;
	sqlite3_stmt *stmt_read_lock;
	sqlite3_stmt *stmt_read_lock_by_uri;
	sqlite3_stmt *stmt_refresh_lock;
#endif
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	buffer *tmp_buf;
	request_uri uri;
	physical physical;

	plugin_config **config_storage;

	plugin_config conf;

	//- 20130304 Sungmin add
	buffer *minidlna_db_dir;
	buffer *minidlna_db_file;
	buffer *minidlna_media_dir;
	buffer *mnt_path;
	
} plugin_data;

/* init the plugin data */
INIT_FUNC(mod_webdav_init) {
	
	plugin_data *p;
	
	p = calloc(1, sizeof(*p));

	p->tmp_buf = buffer_init();

	p->uri.scheme = buffer_init();
	p->uri.path_raw = buffer_init();
	p->uri.path = buffer_init();
	p->uri.authority = buffer_init();

	p->physical.path = buffer_init();
	p->physical.rel_path = buffer_init();
	p->physical.doc_root = buffer_init();
	p->physical.basedir = buffer_init();

	//- 20130304 Sungmin add
	p->minidlna_db_dir = buffer_init();
	p->minidlna_db_file = buffer_init();
	p->minidlna_media_dir = buffer_init();
	p->mnt_path = buffer_init();
	
	return p;
}

/* detroy the plugin data */
FREE_FUNC(mod_webdav_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			if (NULL == s) continue;

			array_free(s->alias_url);
			buffer_free(s->sqlite_db_name);
#ifdef USE_PROPPATCH
			if (s->sql) {
				sqlite3_finalize(s->stmt_delete_prop);
				sqlite3_finalize(s->stmt_delete_uri);
				sqlite3_finalize(s->stmt_copy_uri);
				sqlite3_finalize(s->stmt_move_uri);
				sqlite3_finalize(s->stmt_update_prop);
				sqlite3_finalize(s->stmt_select_prop);
				sqlite3_finalize(s->stmt_select_propnames);

				sqlite3_finalize(s->stmt_read_lock);
				sqlite3_finalize(s->stmt_read_lock_by_uri);
				sqlite3_finalize(s->stmt_create_lock);
				sqlite3_finalize(s->stmt_remove_lock);
				sqlite3_finalize(s->stmt_refresh_lock);
				sqlite3_close(s->sql);
			}
#endif
			free(s);
		}
		free(p->config_storage);
	}

	buffer_free(p->uri.scheme);
	buffer_free(p->uri.path_raw);
	buffer_free(p->uri.path);
	buffer_free(p->uri.authority);

	buffer_free(p->physical.path);
	buffer_free(p->physical.rel_path);
	buffer_free(p->physical.doc_root);
	buffer_free(p->physical.basedir);

	buffer_free(p->tmp_buf);

	//- 20130304 Sungmin add
	buffer_free(p->minidlna_db_dir);
	buffer_free(p->minidlna_db_file);
	buffer_free(p->minidlna_media_dir);
	buffer_free(p->mnt_path);
	
	free(p);

	return HANDLER_GO_ON;
}

/* handle plugin config and check values */

SETDEFAULTS_FUNC(mod_webdav_set_defaults) {
	
	plugin_data *p = p_d;
	size_t i = 0;
	
	config_values_t cv[] = {
		{ "webdav.activate",            NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ "webdav.is-readonly",         NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION },       /* 1 */
		{ "webdav.sqlite-db-name",      NULL, T_CONFIG_STRING,  T_CONFIG_SCOPE_CONNECTION },       /* 2 */
		{ "webdav.log-xml",             NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION },       /* 3 */
		{ "alias.url",                  NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },         /* 4 */
		
		{ NULL,                         NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;
	
	p->config_storage = calloc(1, srv->config_context->used * sizeof(plugin_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		data_config const* config = (data_config const*)srv->config_context->data[i];
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->sqlite_db_name = buffer_init();
		s->alias_url = array_init();

		cv[0].destination = &(s->enabled);
		cv[1].destination = &(s->is_readonly);
		cv[2].destination = s->sqlite_db_name;
		cv[3].destination = &(s->log_xml);
		cv[4].destination = s->alias_url;

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, config->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}
		
#if EMBEDDED_EANBLE
		char *usbdiskname = nvram_get_productid();
#else
		char *usbdiskname = "usbdisk";
#endif
		
		for (size_t j = 0; j < s->alias_url->used; j++) {
			data_string *ds = (data_string *)s->alias_url->data[j];
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

		if (!buffer_string_is_empty(s->sqlite_db_name)) {
#ifdef USE_PROPPATCH
			const char *next_stmt;
			char *err;

			if (SQLITE_OK != sqlite3_open(s->sqlite_db_name->ptr, &(s->sql))) {
				log_error_write(srv, __FILE__, __LINE__, "sbs", "sqlite3_open failed for",
						s->sqlite_db_name,
						sqlite3_errmsg(s->sql));
				return HANDLER_ERROR;
			}

			if (SQLITE_OK != sqlite3_exec(s->sql,
					"CREATE TABLE properties ("
					"  resource TEXT NOT NULL,"
					"  prop TEXT NOT NULL,"
					"  ns TEXT NOT NULL,"
					"  value TEXT NOT NULL,"
					"  PRIMARY KEY(resource, prop, ns))",
					NULL, NULL, &err)) {

				if (0 != strcmp(err, "table properties already exists")) {
					log_error_write(srv, __FILE__, __LINE__, "ss", "can't open transaction:", err);
					sqlite3_free(err);

					return HANDLER_ERROR;
				}
				sqlite3_free(err);
			}

			if (SQLITE_OK != sqlite3_prepare(s->sql,
				CONST_STR_LEN("SELECT value FROM properties WHERE resource = ? AND prop = ? AND ns = ?"),
				&(s->stmt_select_prop), &next_stmt)) {
				/* prepare failed */

				log_error_write(srv, __FILE__, __LINE__, "ss", "sqlite3_prepare failed:", sqlite3_errmsg(s->sql));
				return HANDLER_ERROR;
			}

			if (SQLITE_OK != sqlite3_prepare(s->sql,
				CONST_STR_LEN("SELECT ns, prop FROM properties WHERE resource = ?"),
				&(s->stmt_select_propnames), &next_stmt)) {
				/* prepare failed */

				log_error_write(srv, __FILE__, __LINE__, "ss", "sqlite3_prepare failed:", sqlite3_errmsg(s->sql));
				return HANDLER_ERROR;
			}


			if (SQLITE_OK != sqlite3_prepare(s->sql,
				CONST_STR_LEN("REPLACE INTO properties (resource, prop, ns, value) VALUES (?, ?, ?, ?)"),
				&(s->stmt_update_prop), &next_stmt)) {
				/* prepare failed */

				log_error_write(srv, __FILE__, __LINE__, "ss", "sqlite3_prepare failed:", sqlite3_errmsg(s->sql));
				return HANDLER_ERROR;
			}

			if (SQLITE_OK != sqlite3_prepare(s->sql,
				CONST_STR_LEN("DELETE FROM properties WHERE resource = ? AND prop = ? AND ns = ?"),
				&(s->stmt_delete_prop), &next_stmt)) {
				/* prepare failed */
				log_error_write(srv, __FILE__, __LINE__, "ss", "sqlite3_prepare failed", sqlite3_errmsg(s->sql));

				return HANDLER_ERROR;
			}

			if (SQLITE_OK != sqlite3_prepare(s->sql,
				CONST_STR_LEN("DELETE FROM properties WHERE resource = ?"),
				&(s->stmt_delete_uri), &next_stmt)) {
				/* prepare failed */
				log_error_write(srv, __FILE__, __LINE__, "ss", "sqlite3_prepare failed", sqlite3_errmsg(s->sql));

				return HANDLER_ERROR;
			}

			if (SQLITE_OK != sqlite3_prepare(s->sql,
				CONST_STR_LEN("INSERT INTO properties SELECT ?, prop, ns, value FROM properties WHERE resource = ?"),
				&(s->stmt_copy_uri), &next_stmt)) {
				/* prepare failed */
				log_error_write(srv, __FILE__, __LINE__, "ss", "sqlite3_prepare failed", sqlite3_errmsg(s->sql));

				return HANDLER_ERROR;
			}

			if (SQLITE_OK != sqlite3_prepare(s->sql,
				CONST_STR_LEN("UPDATE properties SET resource = ? WHERE resource = ?"),
				&(s->stmt_move_uri), &next_stmt)) {
				/* prepare failed */
				log_error_write(srv, __FILE__, __LINE__, "ss", "sqlite3_prepare failed", sqlite3_errmsg(s->sql));

				return HANDLER_ERROR;
			}

			/* LOCKS */

			if (SQLITE_OK != sqlite3_exec(s->sql,
					"CREATE TABLE locks ("
					"  locktoken TEXT NOT NULL,"
					"  resource TEXT NOT NULL,"
					"  lockscope TEXT NOT NULL,"
					"  locktype TEXT NOT NULL,"
					"  owner TEXT NOT NULL,"
					"  depth INT NOT NULL,"
					"  timeout TIMESTAMP NOT NULL,"
					"  PRIMARY KEY(locktoken))",
					NULL, NULL, &err)) {

				if (0 != strcmp(err, "table locks already exists")) {
					log_error_write(srv, __FILE__, __LINE__, "ss", "can't open transaction:", err);
					sqlite3_free(err);

					return HANDLER_ERROR;
				}
				sqlite3_free(err);
			}

			if (SQLITE_OK != sqlite3_prepare(s->sql,
				CONST_STR_LEN("INSERT INTO locks (locktoken, resource, lockscope, locktype, owner, depth, timeout) VALUES (?,?,?,?,?,?, CURRENT_TIME + 600)"),
				&(s->stmt_create_lock), &next_stmt)) {
				/* prepare failed */
				log_error_write(srv, __FILE__, __LINE__, "ss", "sqlite3_prepare failed", sqlite3_errmsg(s->sql));

				return HANDLER_ERROR;
			}

			if (SQLITE_OK != sqlite3_prepare(s->sql,
				CONST_STR_LEN("DELETE FROM locks WHERE locktoken = ?"),
				&(s->stmt_remove_lock), &next_stmt)) {
				/* prepare failed */
				log_error_write(srv, __FILE__, __LINE__, "ss", "sqlite3_prepare failed", sqlite3_errmsg(s->sql));

				return HANDLER_ERROR;
			}

			if (SQLITE_OK != sqlite3_prepare(s->sql,
				CONST_STR_LEN("SELECT locktoken, resource, lockscope, locktype, owner, depth, timeout FROM locks WHERE locktoken = ?"),
				&(s->stmt_read_lock), &next_stmt)) {
				/* prepare failed */
				log_error_write(srv, __FILE__, __LINE__, "ss", "sqlite3_prepare failed", sqlite3_errmsg(s->sql));

				return HANDLER_ERROR;
			}

			if (SQLITE_OK != sqlite3_prepare(s->sql,
				CONST_STR_LEN("SELECT locktoken, resource, lockscope, locktype, owner, depth, timeout FROM locks WHERE resource = ?"),
				&(s->stmt_read_lock_by_uri), &next_stmt)) {
				/* prepare failed */
				log_error_write(srv, __FILE__, __LINE__, "ss", "sqlite3_prepare failed", sqlite3_errmsg(s->sql));

				return HANDLER_ERROR;
			}

			if (SQLITE_OK != sqlite3_prepare(s->sql,
				CONST_STR_LEN("UPDATE locks SET timeout = CURRENT_TIME + 600 WHERE locktoken = ?"),
				&(s->stmt_refresh_lock), &next_stmt)) {
				/* prepare failed */
				log_error_write(srv, __FILE__, __LINE__, "ss", "sqlite3_prepare failed", sqlite3_errmsg(s->sql));

				return HANDLER_ERROR;
			}


#else
			log_error_write(srv, __FILE__, __LINE__, "s", "Sorry, no sqlite3 and libxml2 support include, compile with --with-webdav-props");
			return HANDLER_ERROR;
#endif
		}
	}
	
	return HANDLER_GO_ON;
}

#define PATCH_OPTION(x) \
	p->conf.x = s->x;
static int mod_webdav_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH_OPTION(enabled);
	PATCH_OPTION(is_readonly);
	PATCH_OPTION(log_xml);

#ifdef USE_PROPPATCH
	PATCH_OPTION(sql);
	PATCH_OPTION(stmt_update_prop);
	PATCH_OPTION(stmt_delete_prop);
	PATCH_OPTION(stmt_select_prop);
	PATCH_OPTION(stmt_select_propnames);

	PATCH_OPTION(stmt_delete_uri);
	PATCH_OPTION(stmt_move_uri);
	PATCH_OPTION(stmt_copy_uri);

	PATCH_OPTION(stmt_remove_lock);
	PATCH_OPTION(stmt_refresh_lock);
	PATCH_OPTION(stmt_create_lock);
	PATCH_OPTION(stmt_read_lock);
	PATCH_OPTION(stmt_read_lock_by_uri);
#endif
	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("webdav.activate"))) {
				PATCH_OPTION(enabled);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("webdav.is-readonly"))) {
				PATCH_OPTION(is_readonly);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("webdav.log-xml"))) {
				PATCH_OPTION(log_xml);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("webdav.sqlite-db-name"))) {
#ifdef USE_PROPPATCH
				PATCH_OPTION(sql);
				PATCH_OPTION(stmt_update_prop);
				PATCH_OPTION(stmt_delete_prop);
				PATCH_OPTION(stmt_select_prop);
				PATCH_OPTION(stmt_select_propnames);

				PATCH_OPTION(stmt_delete_uri);
				PATCH_OPTION(stmt_move_uri);
				PATCH_OPTION(stmt_copy_uri);

				PATCH_OPTION(stmt_remove_lock);
				PATCH_OPTION(stmt_refresh_lock);
				PATCH_OPTION(stmt_create_lock);
				PATCH_OPTION(stmt_read_lock);
				PATCH_OPTION(stmt_read_lock_by_uri);
#endif
			}
		}
	}

	return 0;
}

URIHANDLER_FUNC(mod_webdav_uri_handler) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (buffer_is_empty(con->uri.path)) return HANDLER_GO_ON;

	mod_webdav_patch_connection(srv, con, p);

	if (!p->conf.enabled) return HANDLER_GO_ON;

	switch (con->request.http_method) {
	case HTTP_METHOD_OPTIONS:
		/* we fake a little bit but it makes MS W2k happy and it let's us mount the volume */
		response_header_overwrite(srv, con, CONST_STR_LEN("DAV"), CONST_STR_LEN("1,2"));
		response_header_overwrite(srv, con, CONST_STR_LEN("MS-Author-Via"), CONST_STR_LEN("DAV"));

		if (p->conf.is_readonly) {
			response_header_insert(srv, con, CONST_STR_LEN("Allow"), CONST_STR_LEN("PROPFIND"));
		} else {
			response_header_insert(srv, con, CONST_STR_LEN("Allow"), CONST_STR_LEN("PROPFIND, DELETE, MKCOL, PUT, MOVE, COPY, PROPPATCH, LOCK, UNLOCK"));
		}
		break;
	default:
		break;
	}

	/* not found */
	return HANDLER_GO_ON;
}

static size_t curl_write_callback_func(void *ptr, size_t size, size_t count, void *stream)
{
   	/* ptr - your string variable.
     	stream - data chuck you received */
	char **response_ptr =  (char**)stream;

    /* assuming the response is a string */
    *response_ptr = strndup(ptr, (size_t)(size *count));
	
  	//printf("%.*s", size, (char*)stream));
   	//Cdbg(1, "callback_func.... %s", (char*)ptr);
}
#if 0

/* md5sum:
 * Concatenates a series of strings together then generates an md5
 * message digest. Writes the result directly into 'output', which
 * MUST be large enough to accept the result. (Size ==
 * 128 + null terminator.)
 */
char secret[100] = "804b51d14d840d6e";
/* sprint_hex:
 * print a long hex value into a string buffer. This function will
 * write 'len' bytes of data from 'char_buf' into 2*len bytes of space
 * in 'output'. (Each hex value is 4 bytes.) Space in output must be
 * pre-allocated. */
void sprint_hex(char* output, unsigned char* char_buf, int len)
{
	int i;
	for (i=0; i < len; i++) {
		sprintf(output + i*2, "%02x", char_buf[i]);
	}
}

void md5sum(char* output, int counter, ...)
{
	va_list argp;
	unsigned char temp[MD5_DIGEST_LENGTH];
	char* md5_string;
	int full_len;
	int i, str_len;
	int buffer_size = 10;

	md5_string = (char*)malloc(buffer_size);
	md5_string[0] = '\0';

	full_len = 0;
	va_start(argp, secret);
	for (i = 0; i < counter; i++) {
		char* string = va_arg(argp, char*);
		int len = strlen(string);

		/* If the buffer is not large enough, expand until it is. */
		while (len + full_len > buffer_size - 1) {
			buffer_size += buffer_size;
			md5_string = realloc(md5_string, buffer_size);
		}

		strncat(md5_string, string, len);

		full_len += len;
		md5_string[full_len] = '\0';
	}
	va_end(argp);

	str_len = strlen(md5_string);
	MD5((const unsigned char*)md5_string, (unsigned int)str_len, temp);

	free(md5_string);

	sprint_hex(output, temp, MD5_DIGEST_LENGTH);	
	output[129] = '\0';
}
#endif

static int get_minidlna_db_path(plugin_data *p){

	if(is_dms_enabled()==0){
		return 0;
	}
	
	//- 20130603 JerryLin add
	int bOpenConfigFle = 1;
#if EMBEDDED_EANBLE
	char* db_dir = nvram_get_dms_dbcwd();
	if(db_dir){
		buffer_copy_string_len(p->minidlna_db_dir, db_dir, strlen(db_dir));
		buffer_copy_string_len(p->minidlna_db_file, db_dir, strlen(db_dir));		
		buffer_append_string(p->minidlna_db_file, "/files.db");	
		bOpenConfigFle = 0;
	}

	char* media_dir = nvram_get_dms_dir();
	if(media_dir){
		buffer_copy_string_len(p->minidlna_media_dir, media_dir, strlen(media_dir));
		bOpenConfigFle = 0;
	}
#endif
	if(bOpenConfigFle==1){
		FILE* fp2;
		char line[128];
		char* minidla_config_file = NULL;                
		char* minidla_config_file_tmp = "/etc/minidlna.conf";                
		if(access(minidla_config_file_tmp,R_OK) == 0)                    
			minidla_config_file = "/etc/minidlna.conf";                
		else                    
			minidla_config_file = "/opt/etc/minidlna.conf";
		
		if((fp2 = fopen(minidla_config_file, "r")) != NULL){
			memset(line, 0, sizeof(line));
			while(fgets(line, 128, fp2) != NULL){
				if(strncmp(line, "db_dir=", 7)==0){
					char tmp[100] = "\0";
					strncpy(tmp, line + 7, strlen(line)-7);
					tmp[strlen(tmp)-1]='\0';

					buffer_copy_string_len(p->minidlna_db_dir, tmp, strlen(tmp) );
					
					strcat(tmp, "/files.db");
					buffer_copy_string_len(p->minidlna_db_file, tmp, strlen(tmp) );
				}
				else if(strncmp(line, "media_dir=", 10)==0){
					char tmp[100] = "\0";
					strncpy(tmp, line + 10, strlen(line)-10);
					tmp[strlen(tmp)-1]='\0';

					buffer_copy_string_len(p->minidlna_media_dir, tmp, strlen(tmp) );					
				}
			}
			fclose(fp2);
		}
	}

#if EMBEDDED_EANBLE
#ifdef APP_IPKG        
	if(db_dir)            
		free(db_dir);

	if(media_dir)
		free(media_dir);
#endif
#endif
	//Cdbg(DBE, ".............................minidlna_db_dir=%s", p->minidlna_db_dir->ptr);
	//Cdbg(DBE, ".............................minidlna_db_file=%s", p->minidlna_db_file->ptr);
	//Cdbg(DBE, ".............................minidlna_media_dir=%s", p->minidlna_media_dir->ptr);
	
}

static int get_thumb_image(char* path, plugin_data *p, unsigned char **out){
	if(is_dms_enabled()==0){
		return 0;
	}
	
#if EMBEDDED_EANBLE
	char* thumb_dir = (char *)malloc(PATH_MAX);
	if(!thumb_dir) return 0;

	if (buffer_is_empty(p->minidlna_db_dir))
		get_minidlna_db_path(p);
	
	strcpy(thumb_dir, p->minidlna_db_dir->ptr);
	strcat(thumb_dir, "/art_cache/tmp");
	
	char* filename = NULL;	
	extract_filename(path, &filename);
	const char *dot = strrchr(filename, '.');
	int len = dot - filename;
	
	char* filepath = NULL;
	extract_filepath(path, &filepath);
					
	strcat(thumb_dir, filepath);
	strncat(thumb_dir, filename, len);
	strcat(thumb_dir, ".jpg");
	
	free(filename);
	free(filepath);
#else
	char* db_dir = "/var/lib/minidlna/";
	char* thumb_dir = (char *)malloc(PATH_MAX);
	
	if(!thumb_dir){
		return 0;
	}	
	strcpy(thumb_dir, db_dir);
	strcat(thumb_dir, "art_cache");
	
	char* filename = NULL;	
	extract_filename(path, &filename);
	const char *dot = strrchr(filename, '.');
	int index = dot - filename;
	//Cdbg(DBE,"dot = %s, index=%d", dot, index);
	
	char* filepath = NULL;
	extract_filepath(path, &filepath);

	Cdbg(DBE,"filepath=%s, filename=%s", filepath, filename);
	
	strcat(thumb_dir, filepath);
	strncat(thumb_dir, filename, index);
	strcat(thumb_dir, ".jpg");
	
	free(filename);
	free(filepath);
	
#endif
	Cdbg(DBE,"thumb_dir=%s", thumb_dir);

	int result = 0;

	FILE* fp = fopen(thumb_dir, "rb");
		
	if(fp!=NULL){
		Cdbg(DBE, "success to open thumb_dir=%s", thumb_dir);
		//Get file length
       	fseek(fp, 0, SEEK_END);
       	int fileLen = ftell(fp);
       	fseek(fp, 0, SEEK_SET);
			
		char* buffer = (char *)malloc(fileLen+1);
		if(buffer){		
			fread( buffer, fileLen, sizeof(unsigned char), fp );
			
			unsigned char* tmp = ldb_base64_encode(buffer, fileLen);			
			uint32 olen = strlen(tmp) + 1;
			*out = (char*)malloc(olen);
			memcpy(*out, tmp, olen);
				
			free(tmp);			
			free(buffer);
				
			fclose(fp);

			result = 1;
		}
	}
	
	free(thumb_dir);	
	return result;
}

static int get_album_cover_image(sqlite3 *sql_minidlna, sqlite_int64 plAlbumArt, unsigned char **out){
	if(is_dms_enabled()==0){
		return 0;
	}
	
	if(plAlbumArt<=0){
		return 0;
	}
	
	int rows2;
	char **result2;
	char* base64_image = NULL;
	char sql_query[2048] = "\0";
	
	sprintf(sql_query, "SELECT PATH FROM ALBUM_ART WHERE ID = '%lld'", plAlbumArt );
	if( sql_get_table(sql_minidlna, sql_query, &result2, &rows2, NULL) == SQLITE_OK ){
		//Cdbg(DBE, "get_album_cover_image, album cover path=%s", result2[1]);

		char* album_cover_file = result2[1];
									
		FILE* fp = fopen(album_cover_file, "rb");
		
		if(fp!=NULL){
			//Get file length
	       	fseek(fp, 0, SEEK_END);
	       	int fileLen = ftell(fp);
	       	fseek(fp, 0, SEEK_SET);
			
			char* buffer = (char *)malloc(fileLen+1);
			if(!buffer){
				return 0;
			}
			char* aa = get_filename_ext(album_cover_file);
			int len = strlen(aa)+1; 		
			char* file_ext = (char*)malloc(len);
			memset(file_ext,'\0', len);
			strcpy(file_ext, aa);
			for (int i = 0; file_ext[i]; i++)
				file_ext[i] = tolower(file_ext[i]);
										
			fread( buffer, fileLen, sizeof(unsigned char), fp );
			
			unsigned char* tmp = ldb_base64_encode(buffer, fileLen);			
			uint32 olen = strlen(tmp) + 1;
			*out = (char*)malloc(olen);
			memcpy(*out, tmp, olen);
			
			free(tmp);			
			free(file_ext);
			free(buffer);
			
			fclose(fp);
		}
									
	}
	sqlite3_free_table(result2);
	
	return 1;
}					

static int webdav_gen_prop_tag(server *srv, connection *con,
		char *prop_name,
		char *prop_ns,
		char *value,
		buffer *b) {

	UNUSED(srv);
	UNUSED(con);

	if (value) {
		buffer_append_string_len(b,CONST_STR_LEN("<"));
		buffer_append_string(b, prop_name);
		buffer_append_string_len(b, CONST_STR_LEN(" xmlns=\""));
		buffer_append_string(b, prop_ns);
		buffer_append_string_len(b, CONST_STR_LEN("\">"));

		buffer_append_string(b, value);

		buffer_append_string_len(b,CONST_STR_LEN("</"));
		buffer_append_string(b, prop_name);
		buffer_append_string_len(b, CONST_STR_LEN(">"));
	} else {
		buffer_append_string_len(b,CONST_STR_LEN("<"));
		buffer_append_string(b, prop_name);
		buffer_append_string_len(b, CONST_STR_LEN(" xmlns=\""));
		buffer_append_string(b, prop_ns);
		buffer_append_string_len(b, CONST_STR_LEN("\"/>"));
	}

	return 0;
}


static int webdav_gen_response_status_tag(server *srv, connection *con, physical *dst, int status, buffer *b) {
	UNUSED(srv);

	buffer_append_string_len(b,CONST_STR_LEN("<D:response xmlns:ns0=\"urn:uuid:c2f41010-65b3-11d1-a29f-00aa00c14882/\">\n"));

	buffer_append_string_len(b,CONST_STR_LEN("<D:href>\n"));
	buffer_append_string_buffer(b, dst->rel_path);
	buffer_append_string_len(b,CONST_STR_LEN("</D:href>\n"));
	buffer_append_string_len(b,CONST_STR_LEN("<D:status>\n"));

	if (con->request.http_version == HTTP_VERSION_1_1) {
		buffer_copy_string_len(b, CONST_STR_LEN("HTTP/1.1 "));
	} else {
		buffer_copy_string_len(b, CONST_STR_LEN("HTTP/1.0 "));
	}
	buffer_append_int(b, status);
	buffer_append_string_len(b, CONST_STR_LEN(" "));
	buffer_append_string(b, get_http_status_name(status));

	buffer_append_string_len(b,CONST_STR_LEN("</D:status>\n"));
	buffer_append_string_len(b,CONST_STR_LEN("</D:response>\n"));

	return 0;
}

static int webdav_delete_file(server *srv, connection *con, plugin_data *p, physical *dst, buffer *b) {
	int status = 0;

	/* try to unlink it */
	if (-1 == unlink(dst->path->ptr)) {
		switch(errno) {
		case EACCES:
		case EPERM:
			/* 403 */
			status = 403;
			break;
		default:
			status = 501;
			break;
		}
		webdav_gen_response_status_tag(srv, con, dst, status, b);
	} else {
#ifdef USE_PROPPATCH
		sqlite3_stmt *stmt = p->conf.stmt_delete_uri;

		if (!stmt) {
			status = 403;
			webdav_gen_response_status_tag(srv, con, dst, status, b);
		} else {
			sqlite3_reset(stmt);

			/* bind the values to the insert */

			sqlite3_bind_text(stmt, 1,
				CONST_BUF_LEN(dst->rel_path),
				SQLITE_TRANSIENT);

			if (SQLITE_DONE != sqlite3_step(stmt)) {
				/* */
			}
		}
#else
		UNUSED(p);
#endif
	}

	return (status != 0);
}

static int webdav_delete_dir(server *srv, connection *con, plugin_data *p, physical *dst, buffer *b) {
	DIR *dir;
	int have_multi_status = 0;
	physical d;

	d.path = buffer_init();
	d.rel_path = buffer_init();

	if (NULL != (dir = opendir(dst->path->ptr))) {
		struct dirent *de;

		while(NULL != (de = readdir(dir))) {
			struct stat st;
			int status = 0;

			if ((de->d_name[0] == '.' && de->d_name[1] == '\0')  ||
			  (de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0')) {
				continue;
				/* ignore the parent dir */
			}

			buffer_copy_buffer(d.path, dst->path);
			buffer_append_slash(d.path);
			buffer_append_string(d.path, de->d_name);

			buffer_copy_buffer(d.rel_path, dst->rel_path);
			buffer_append_slash(d.rel_path);
			buffer_append_string(d.rel_path, de->d_name);

			/* stat and unlink afterwards */
			if (-1 == stat(d.path->ptr, &st)) {
				/* don't about it yet, rmdir will fail too */
			} else if (S_ISDIR(st.st_mode)) {
				have_multi_status = webdav_delete_dir(srv, con, p, &d, b);

				/* try to unlink it */
				if (-1 == rmdir(d.path->ptr)) {
					switch(errno) {
					case EACCES:
					case EPERM:
						/* 403 */
						status = 403;
						break;
					default:
						status = 501;
						break;
					}
					have_multi_status = 1;

					webdav_gen_response_status_tag(srv, con, &d, status, b);
				} else {
#ifdef USE_PROPPATCH
					sqlite3_stmt *stmt = p->conf.stmt_delete_uri;

					status = 0;

					if (stmt) {
						sqlite3_reset(stmt);

						/* bind the values to the insert */

						sqlite3_bind_text(stmt, 1,
							CONST_BUF_LEN(d.rel_path),
							SQLITE_TRANSIENT);

						if (SQLITE_DONE != sqlite3_step(stmt)) {
							/* */
						}
					}
#endif
				}
			} else {
				have_multi_status = webdav_delete_file(srv, con, p, &d, b);
			}
		}
		closedir(dir);

		buffer_free(d.path);
		buffer_free(d.rel_path);
	}

	return have_multi_status;
}

static int webdav_copy_file(server *srv, connection *con, plugin_data *p, physical *src, physical *dst, int overwrite) {
	stream s;
	int status = 0, ofd;
	UNUSED(srv);
	UNUSED(con);

	if (stream_open(&s, src->path)) {
		return 403;
	}

	if (-1 == (ofd = open(dst->path->ptr, O_WRONLY|O_TRUNC|O_CREAT|(overwrite ? 0 : O_EXCL), WEBDAV_FILE_MODE))) {
		/* opening the destination failed for some reason */
		switch(errno) {
		case EEXIST:
			status = 412;
			break;
		case EISDIR:
			status = 409;
			break;
		case ENOENT:
			/* at least one part in the middle wasn't existing */
			status = 409;
			break;
		default:
			status = 403;
			break;
		}
		stream_close(&s);
		return status;
	}

	if (-1 == write(ofd, s.start, s.size)) {
		switch(errno) {
		case ENOSPC:
			status = 507;
			break;
		default:
			status = 403;
			break;
		}
	}

	stream_close(&s);
	close(ofd);

#ifdef USE_PROPPATCH
	if (0 == status) {
		/* copy worked fine, copy connected properties */
		sqlite3_stmt *stmt = p->conf.stmt_copy_uri;

		if (stmt) {
			sqlite3_reset(stmt);

			/* bind the values to the insert */
			sqlite3_bind_text(stmt, 1,
				CONST_BUF_LEN(dst->rel_path),
				SQLITE_TRANSIENT);

			sqlite3_bind_text(stmt, 2,
				CONST_BUF_LEN(src->rel_path),
				SQLITE_TRANSIENT);

			if (SQLITE_DONE != sqlite3_step(stmt)) {
				/* */
			}
		}
	}
#else
	UNUSED(p);
#endif
	return status;
}

static int webdav_copy_dir(server *srv, connection *con, plugin_data *p, physical *src, physical *dst, int overwrite) {
	DIR *srcdir;
	int status = 0;

	if (NULL != (srcdir = opendir(src->path->ptr))) {
		struct dirent *de;
		physical s, d;

		s.path = buffer_init();
		s.rel_path = buffer_init();

		d.path = buffer_init();
		d.rel_path = buffer_init();

		while (NULL != (de = readdir(srcdir))) {
			struct stat st;

			if ((de->d_name[0] == '.' && de->d_name[1] == '\0')
				|| (de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0')) {
				continue;
			}

			buffer_copy_buffer(s.path, src->path);
			buffer_append_slash(s.path);
			buffer_append_string(s.path, de->d_name);

			buffer_copy_buffer(d.path, dst->path);
			buffer_append_slash(d.path);
			buffer_append_string(d.path, de->d_name);

			buffer_copy_buffer(s.rel_path, src->rel_path);
			buffer_append_slash(s.rel_path);
			buffer_append_string(s.rel_path, de->d_name);

			buffer_copy_buffer(d.rel_path, dst->rel_path);
			buffer_append_slash(d.rel_path);
			buffer_append_string(d.rel_path, de->d_name);

			if (-1 == stat(s.path->ptr, &st)) {
				/* why ? */
			} else if (S_ISDIR(st.st_mode)) {
				/* a directory */
				if (-1 == mkdir(d.path->ptr, WEBDAV_DIR_MODE) &&
				    errno != EEXIST) {
					/* WTH ? */
				} else {
#ifdef USE_PROPPATCH
					sqlite3_stmt *stmt = p->conf.stmt_copy_uri;

					if (0 != (status = webdav_copy_dir(srv, con, p, &s, &d, overwrite))) {
						break;
					}
					/* directory is copied, copy the properties too */

					if (stmt) {
						sqlite3_reset(stmt);

						/* bind the values to the insert */
						sqlite3_bind_text(stmt, 1,
							CONST_BUF_LEN(dst->rel_path),
							SQLITE_TRANSIENT);

						sqlite3_bind_text(stmt, 2,
							CONST_BUF_LEN(src->rel_path),
							SQLITE_TRANSIENT);

						if (SQLITE_DONE != sqlite3_step(stmt)) {
							/* */
						}
					}
#endif
				}
			} else if (S_ISREG(st.st_mode)) {
				/* a plain file */
				if (0 != (status = webdav_copy_file(srv, con, p, &s, &d, overwrite))) {
					break;
				}
			}
		}

		buffer_free(s.path);
		buffer_free(s.rel_path);
		buffer_free(d.path);
		buffer_free(d.rel_path);

		closedir(srcdir);
	}

	return status;
}

static int webdav_get_live_property(server *srv, connection *con, plugin_data *p, physical *dst, char *prop_name, buffer *b) {
	stat_cache_entry *sce = NULL;
	int found = 0;

	UNUSED(p);

	if (HANDLER_ERROR != (stat_cache_get_entry(srv, con, dst->path, &sce))) {
		char ctime_buf[] = "2005-08-18T07:27:16Z";
		char mtime_buf[] = "Thu, 18 Aug 2005 07:27:16 GMT";
		size_t k;

		if (0 == strcmp(prop_name, "resourcetype")) {
			if (S_ISDIR(sce->st.st_mode)) {
				buffer_append_string_len(b, CONST_STR_LEN("<D:resourcetype><D:collection/></D:resourcetype>"));
				found = 1;
			}
		} else if (0 == strcmp(prop_name, "getcontenttype")) {
			if (S_ISDIR(sce->st.st_mode)) {
				buffer_append_string_len(b, CONST_STR_LEN("<D:getcontenttype>httpd/unix-directory</D:getcontenttype>"));
				found = 1;
			} else if(S_ISREG(sce->st.st_mode)) {
				for (k = 0; k < con->conf.mimetypes->used; k++) {
					data_string *ds = (data_string *)con->conf.mimetypes->data[k];

					if (buffer_is_empty(ds->key)) continue;

					if (buffer_is_equal_right_len(dst->path, ds->key, buffer_string_length(ds->key))) {
						buffer_append_string_len(b,CONST_STR_LEN("<D:getcontenttype>"));
						buffer_append_string_buffer(b, ds->value);
						buffer_append_string_len(b, CONST_STR_LEN("</D:getcontenttype>"));
						found = 1;

						break;
					}
				}
			}
		} else if (0 == strcmp(prop_name, "creationdate")) {
			//buffer_append_string_len(b, CONST_STR_LEN("<D:creationdate ns0:dt=\"dateTime.tz\">"));
                        buffer_append_string_len(b, CONST_STR_LEN("<D:creationdate>"));			
			strftime(ctime_buf, sizeof(ctime_buf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&(sce->st.st_ctime)));
			buffer_append_string(b, ctime_buf);
			buffer_append_string_len(b, CONST_STR_LEN("</D:creationdate>"));
			found = 1;
		} else if (0 == strcmp(prop_name, "getlastmodified")) {
			//buffer_append_string_len(b,CONST_STR_LEN("<D:getlastmodified ns0:dt=\"dateTime.rfc1123\">"));
			buffer_append_string_len(b,CONST_STR_LEN("<D:getlastmodified>"));			
			strftime(mtime_buf, sizeof(mtime_buf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&(sce->st.st_mtime)));
			//strftime(mtime_buf, sizeof(mtime_buf), "%Y/%m/%d %H:%M:%S", localtime(&(sce->st.st_mtime)));
			buffer_append_string(b, mtime_buf);
			buffer_append_string_len(b, CONST_STR_LEN("</D:getlastmodified>"));
			found = 1;
		} else if (0 == strcmp(prop_name, "getcontentlength")) {
			buffer_append_string_len(b,CONST_STR_LEN("<D:getcontentlength>"));
			buffer_append_int(b, sce->st.st_size);
			buffer_append_string_len(b, CONST_STR_LEN("</D:getcontentlength>"));
			found = 1;
		} else if (0 == strcmp(prop_name, "getcontentlanguage")) {
			buffer_append_string_len(b,CONST_STR_LEN("<D:getcontentlanguage>"));
			buffer_append_string_len(b, CONST_STR_LEN("en"));
			buffer_append_string_len(b, CONST_STR_LEN("</D:getcontentlanguage>"));
			found = 1;
		}else if (0 == strcmp(prop_name, "getmac")) {
			//- 20111124 Jerry add
			buffer_append_string_len(b,CONST_STR_LEN("<D:getmac>"));		
			buffer_append_string_len(b, CONST_STR_LEN("</D:getmac>"));
			found = 1;
		}else if (0 == strcmp(prop_name, "getonline")) {
			//- 20111219 Jerry add
			buffer_append_string_len(b,CONST_STR_LEN("<D:getonline>"));
			buffer_append_string_len(b, CONST_STR_LEN("1"));
			buffer_append_string_len(b, CONST_STR_LEN("</D:getonline>"));
			found = 1;
		}else if (0 == strcmp(prop_name, "getuniqueid")) {
			//- 20120104 Jerry add
			buffer_append_string_len(b,CONST_STR_LEN("<D:getuniqueid>"));
			char* result;
			md5String(dst->rel_path->ptr, NULL, &result);				
			buffer_append_string(b, result);
			free(result);			
			buffer_append_string_len(b, CONST_STR_LEN("</D:getuniqueid>"));
			found = 1;
		}else if (0 == strcmp(prop_name, "getattr")) {
			//- 20120302 Jerry add
			buffer_append_string_len(b,CONST_STR_LEN("<D:getattr>"));
			
			int permission = -1;
		#if EMBEDDED_EANBLE
			char* usbdisk_rel_sub_path = NULL;
			char* usbdisk_sub_share_folder = NULL;
						
			smbc_parse_mnt_path(dst->path->ptr, 
				                p->mnt_path->ptr, 
				                p->mnt_path->used-1, 
				                &usbdisk_rel_sub_path, 
				                &usbdisk_sub_share_folder);

			permission = smbc_get_usbdisk_permission(con->aidisk_username->ptr, usbdisk_rel_sub_path, usbdisk_sub_share_folder);
			
			free(usbdisk_rel_sub_path);
			free(usbdisk_sub_share_folder);
			
			buffer_append_string_len(b, CONST_STR_LEN("<D:readonly>"));
			if( permission == 3 )
				buffer_append_string_len(b, CONST_STR_LEN("false"));
			else
				buffer_append_string_len(b, CONST_STR_LEN("true"));
			buffer_append_string_len(b, CONST_STR_LEN("</D:readonly>"));
		#else
			buffer_append_string_len(b, CONST_STR_LEN("<D:readonly>"));			
			buffer_append_string_len(b, CONST_STR_LEN("true"));
			buffer_append_string_len(b, CONST_STR_LEN("</D:readonly>"));
		#endif

			buffer_append_string_len(b, CONST_STR_LEN("<D:hidden>"));
			buffer_append_string_len(b, CONST_STR_LEN("false"));				
			buffer_append_string_len(b, CONST_STR_LEN("</D:hidden>"));
			
			buffer_append_string_len(b, CONST_STR_LEN("</D:getattr>"));
				
			found = 1;
		}
		else if (0 == strcmp(prop_name, "gettype")) {
			//- 20120208 Jerry add
			buffer_append_string_len(b,CONST_STR_LEN("<D:gettype>"));			
			buffer_append_string(b, "usbdisk");
			buffer_append_string_len(b, CONST_STR_LEN("</D:gettype>"));
			found = 1;
		}
		else if (0 == strcmp(prop_name, "getuseragent")) {
			//- 20121205 Jerry add
			buffer_append_string_len(b,CONST_STR_LEN("<D:getuseragent>"));			
			if(con->smb_info){
				buffer_append_string_buffer(b, con->smb_info->user_agent);
			}
			buffer_append_string_len(b, CONST_STR_LEN("</D:getuseragent>"));
			found = 1;
		}
		else if (0 == strcmp(prop_name, "getroutersync")) {
			//- 20121206 Jerry add
			buffer_append_string_len(b,CONST_STR_LEN("<D:getroutersync>"));			
			
			share_link_info_t* c;
			int is_router_sync_folder = 0;
			buffer* temp = buffer_init();
			
			for (c = share_link_info_list; c; c = c->next) {

				if(c->toshare != 2)
					continue;
					
				buffer_append_string_buffer(temp, c->realpath);
				buffer_append_string(temp, "/");
				buffer_append_string_buffer(temp, c->filename);

				if(buffer_is_equal(temp,dst->rel_path)){
					is_router_sync_folder = 1;
					break;
				}
			}

			buffer_free(temp);

			if(is_router_sync_folder==1)
				buffer_append_string(b, "1");
			else
				buffer_append_string(b, "0");
			
			buffer_append_string_len(b, CONST_STR_LEN("</D:getroutersync>"));
			found = 1;
		}
		else if (0 == strcmp(prop_name, "getmetadata")) {
#if 1
			buffer_append_string_len(b,CONST_STR_LEN("<D:getmetadata>"));		

			if(is_dms_enabled()==1){
		
				int rows, i;
				sqlite3 *sql_minidlna = NULL;
				char **result;
				char sql_query[2048] = "\0";

				get_minidlna_db_path(p);
				
				if (!buffer_is_empty(p->minidlna_db_file)) {			
					if (SQLITE_OK != sqlite3_open(p->minidlna_db_file->ptr, &(sql_minidlna))) {				
						Cdbg(DBE, "Fail to open minidlna db %s", p->minidlna_db_file->ptr);
					}
				}

				if (sql_minidlna) {
					
					int column_count = 11;
					sprintf(sql_query, "SELECT ID, TITLE, SIZE, TIMESTAMP, THUMBNAIL, RESOLUTION, CREATOR, ARTIST, MIME, ALBUM, ALBUM_ART from DETAILS");

					#if EMBEDDED_EANBLE
					if (0 == strcmp(dst->path->ptr, "/tmp"))
						sprintf(sql_query, "%s WHERE PATH = '%s'", sql_query, dst->path->ptr);
					else
						sprintf(sql_query, "%s WHERE PATH = '/tmp%s'", sql_query, dst->path->ptr);
					#else
					sprintf(sql_query, "%s WHERE PATH = '%s'", sql_query, dst->path->ptr);
					#endif
					
					//Cdbg(DBE, "sql_query=%s", sql_query);

					if( sql_get_table(sql_minidlna, sql_query, &result, &rows, NULL) == SQLITE_OK ){
						if( rows ){
							sqlite_int64 plID = strtoll(result[column_count], NULL, 10);
							char* plname = result[column_count+1];						
							char* thumbnail = result[column_count+4];
							char* resolution = result[column_count+5];
							char* creator = result[column_count+6];	
							char* artist = result[column_count+7];
							char* mime = result[column_count+8];
							char* album = result[column_count+9];
							char* album_art = result[column_count+10];
							
							if(plname){
								int thumb = atoi(thumbnail);

								buffer_append_string_len(b, CONST_STR_LEN("<D:title>"));
								buffer_append_string(b, plname);
								buffer_append_string_len(b, CONST_STR_LEN("</D:title>"));

								buffer_append_string_len(b, CONST_STR_LEN("<D:resolution>"));
								buffer_append_string(b, resolution);
								buffer_append_string_len(b, CONST_STR_LEN("</D:resolution>"));

								buffer_append_string_len(b, CONST_STR_LEN("<D:creator><![CDATA["));								
								buffer_append_string(b, creator);								
								buffer_append_string_len(b, CONST_STR_LEN("]]></D:creator>"));	
								
								buffer_append_string_len(b, CONST_STR_LEN("<D:artist><![CDATA["));								
								buffer_append_string(b, artist);								
								buffer_append_string_len(b, CONST_STR_LEN("]]></D:artist>"));

								buffer_append_string_len(b, CONST_STR_LEN("<D:mime>"));
								buffer_append_string(b, mime);
								buffer_append_string_len(b, CONST_STR_LEN("</D:mime>"));

								buffer_append_string_len(b, CONST_STR_LEN("<D:thumb>"));
								//buffer_append_string(b, thumbnail);
								buffer_append_string_len(b, CONST_STR_LEN("1"));
								buffer_append_string_len(b, CONST_STR_LEN("</D:thumb>"));
								
								//Cdbg(DBE, "plname=%s, thumb=%d, resolution=%s, creator=%s, artist=%s, mime=%s, album_art=%s", plname, thumb, resolution, creator, artist, mime, album_art);

								#if 0
								char* image = NULL;
								if(thumb==1){
									if(get_thumb_image(dst->path->ptr, &image)==1){
										buffer_append_string_len(b, CONST_STR_LEN("<D:thumb_image>"));
										buffer_append_string(b, image);
										buffer_append_string_len(b, CONST_STR_LEN("</D:thumb_image>"));
										free(image);
									}
								}
								else{
									sqlite_int64 plAlbumArt = (album_art ? strtoll(album_art, NULL, 10) : 0);
									if(get_album_cover_image(sql_minidlna, plAlbumArt, &image)==1){
										buffer_append_string_len(b, CONST_STR_LEN("<D:thumb_image>"));
										buffer_append_string(b, image);
										buffer_append_string_len(b, CONST_STR_LEN("</D:thumb_image>"));
										free(image);
									}
								}
								#endif
							}						
						}

						sqlite3_free_table(result);
					}
					
					sqlite3_close(sql_minidlna);
				}
			}

#if 0
			//- http request test
			struct sockaddr_in their_addr; /* connector's address information */
			int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
			if (serverSocket == -1)
				Cdbg(DBE, "serverSocket == -1" );

			bzero(&(their_addr), sizeof(their_addr)); /* zero the rest of the struct */
    		their_addr.sin_family = AF_INET; /* host byte order */
    		their_addr.sin_port = htons(8082); /* short, network byte order */
   	 		their_addr.sin_addr.s_addr = htonl(INADDR_ANY);

			bzero(&(their_addr.sin_zero), sizeof(their_addr.sin_zero)); /* zero the rest of the struct */
		    if (connect(serverSocket, (struct sockaddr *)&their_addr,sizeof(struct sockaddr)) == -1) {
		                                                              
		        Cdbg(DBE, "connect error");
		    }
	
			//- http://imdbapi.org/?title=Batman Begins&type=xml
			char request[1024]="\0";
			sprintf(request, "GET %s HTTP/1.0\r\nHOST:%s \r\n", "/?title=Batman&type=xml", "http://imdbapi.org/");

			Cdbg(DBE, "request = %s", request );
			
			if( send(serverSocket, &request, sizeof(request), 0) < 0 )
    			Cdbg(DBE, "send()");

			int numbytes;
			char buf[1024];
			if ((numbytes=recv(serverSocket, buf, 1024, 0)) == -1) {
        		Cdbg(DBE, "recv error");
			}

    		buf[numbytes] = '\0';
	
			close(serverSocket);
			Cdbg(DBE, "buf=%s", buf);
			////////////////////////////////////
#endif		

#if 0		
			
			char url[1024]="\0";
			sprintf(url, "%s", "http://imdbapi.org/?title=Batman&type=xml");
			
			char init_upload_xml[1024]="/tmp/aaa.xml";
			int status = sendRequest(init_upload_xml,url,NULL,NULL,NULL);
#endif

			buffer_append_string_len(b, CONST_STR_LEN("</D:getmetadata>"));
			
			found = 1;
#endif			
		}
		/*else if (0 == strcmp(prop_name, "getname")) {
			//- 20120920 Jerry add			
			buffer_append_string_len(b,CONST_STR_LEN("<D:getname>"));
			buffer_append_string_len(b,CONST_STR_LEN("sda"));
			buffer_append_string_len(b, CONST_STR_LEN("</D:getname>"));
			found = 1;
		}else if (0 == strcmp(prop_name, "getpath")) {
			//- 20120920 Jerry add
			buffer_append_string_len(b,CONST_STR_LEN("<D:getpath>"));
			buffer_append_string_buffer(b,dst->rel_path);
			buffer_append_string_len(b, CONST_STR_LEN("</D:getpath>"));
			found = 1;
		}*/
	}

	return found ? 0 : -1;
}

static int webdav_get_property(server *srv, connection *con, plugin_data *p, physical *dst, char *prop_name, char *prop_ns, buffer *b) {
	if (0 == strcmp(prop_ns, "DAV:")) {
		/* a local 'live' property */
		return webdav_get_live_property(srv, con, p, dst, prop_name, b);
	} else {
		int found = 0;
#ifdef USE_PROPPATCH
		sqlite3_stmt *stmt = p->conf.stmt_select_prop;

		if (stmt) {
			/* perhaps it is in sqlite3 */
			sqlite3_reset(stmt);

			/* bind the values to the insert */

			sqlite3_bind_text(stmt, 1,
				CONST_BUF_LEN(dst->rel_path),
				SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2,
				prop_name,
				strlen(prop_name),
				SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3,
				prop_ns,
				strlen(prop_ns),
				SQLITE_TRANSIENT);

			/* it is the PK */
			while (SQLITE_ROW == sqlite3_step(stmt)) {
				/* there is a row for us, we only expect a single col 'value' */
				webdav_gen_prop_tag(srv, con, prop_name, prop_ns, (char *)sqlite3_column_text(stmt, 0), b);
				found = 1;
			}
		}
#endif
		return found ? 0 : -1;
	}

	/* not found */
	return -1;
}

typedef struct {
	char *ns;
	char *prop;
} webdav_property;

static webdav_property live_properties[] = {
	{ "DAV:", "creationdate" },
	{ "DAV:", "displayname" },
	{ "DAV:", "getcontentlanguage" },
	{ "DAV:", "getcontentlength" },
	{ "DAV:", "getcontenttype" },
	{ "DAV:", "getetag" },
	{ "DAV:", "getlastmodified" },
	{ "DAV:", "resourcetype" },
	{ "DAV:", "lockdiscovery" },
	{ "DAV:", "source" },
	{ "DAV:", "supportedlock" },
	{ "DAV:", "getmac" },
	{ "DAV:", "getonline" },
	{ "DAV:", "getuniqueid" },
	{ "DAV:", "gettype" },
	{ "DAV:", "getattr" },
	{ "DAV:", "getname" },
	{ "DAV:", "getpath" },
	{ "DAV:", "getuseragent" },
	{ "DAV:", "getroutersync" },
	{ "DAV:", "getmetadata" },

	{ NULL, NULL }
};

typedef struct {
	webdav_property **ptr;

	size_t used;
	size_t size;
} webdav_properties;

static int webdav_get_props(server *srv, connection *con, plugin_data *p, physical *dst, webdav_properties *props, buffer *b_200, buffer *b_404) {
	size_t i;

	if (props) {
		for (i = 0; i < props->used; i++) {
			webdav_property *prop;

			prop = props->ptr[i];

			if (0 != webdav_get_property(srv, con, p,
				dst, prop->prop, prop->ns, b_200)) {
				webdav_gen_prop_tag(srv, con, prop->prop, prop->ns, NULL, b_404);
			}
		}
	} else {
		for (i = 0; live_properties[i].prop; i++) {
			/* a local 'live' property */
			webdav_get_live_property(srv, con, p, dst, live_properties[i].prop, b_200);
		}
	}

	return 0;
}

#ifdef USE_PROPPATCH
static int webdav_parse_chunkqueue(server *srv, connection *con, plugin_data *p, chunkqueue *cq, xmlDoc **ret_xml) {
	xmlParserCtxtPtr ctxt;
	xmlDoc *xml;
	int res;
	int err;

	chunk *c;

	UNUSED(con);

	/* read the chunks in to the XML document */
	ctxt = xmlCreatePushParserCtxt(NULL, NULL, NULL, 0, NULL);

	for (c = cq->first; cq->bytes_out != cq->bytes_in; c = cq->first) {
		size_t weWant = cq->bytes_out - cq->bytes_in;
		size_t weHave;

		switch(c->type) {
		case FILE_CHUNK:
			weHave = c->file.length - c->offset;

			if (weHave > weWant) weHave = weWant;

			/* xml chunks are always memory, mmap() is our friend */
			if (c->file.mmap.start == MAP_FAILED) {
				if (-1 == c->file.fd &&  /* open the file if not already open */
				    -1 == (c->file.fd = open(c->file.name->ptr, O_RDONLY))) {
					log_error_write(srv, __FILE__, __LINE__, "ss", "open failed: ", strerror(errno));

					return -1;
				}

				if (MAP_FAILED == (c->file.mmap.start = mmap(0, c->file.length, PROT_READ, MAP_SHARED, c->file.fd, 0))) {
					log_error_write(srv, __FILE__, __LINE__, "ssbd", "mmap failed: ",
							strerror(errno), c->file.name,  c->file.fd);
					close(c->file.fd);
					c->file.fd = -1;

					return -1;
				}

				close(c->file.fd);
				c->file.fd = -1;

				c->file.mmap.length = c->file.length;

				/* chunk_reset() or chunk_free() will cleanup for us */
			}

			if (XML_ERR_OK != (err = xmlParseChunk(ctxt, c->file.mmap.start + c->offset, weHave, 0))) {
				log_error_write(srv, __FILE__, __LINE__, "sodd", "xmlParseChunk failed at:", cq->bytes_out, weHave, err);
			}

			chunkqueue_mark_written(cq, weHave);

			break;
		case MEM_CHUNK:
			/* append to the buffer */
			weHave = buffer_string_length(c->mem) - c->offset;

			if (weHave > weWant) weHave = weWant;

			if (p->conf.log_xml) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "XML-request-body:", c->mem->ptr + c->offset);
			}

			if (XML_ERR_OK != (err = xmlParseChunk(ctxt, c->mem->ptr + c->offset, weHave, 0))) {
				log_error_write(srv, __FILE__, __LINE__, "sodd", "xmlParseChunk failed at:", cq->bytes_out, weHave, err);
			}

			chunkqueue_mark_written(cq, weHave);

			break;
		}
	}

	switch ((err = xmlParseChunk(ctxt, 0, 0, 1))) {
	case XML_ERR_DOCUMENT_END:
	case XML_ERR_OK:
		break;
	default:
		log_error_write(srv, __FILE__, __LINE__, "sd", "xmlParseChunk failed at final packet:", err);
		break;
	}

	xml = ctxt->myDoc;
	res = ctxt->wellFormed;
	xmlFreeParserCtxt(ctxt);

	if (res == 0) {
		xmlFreeDoc(xml);
	} else {
		*ret_xml = xml;
	}

	return res;
}
#endif

#ifdef USE_LOCKS
static int webdav_lockdiscovery(server *srv, connection *con,
		buffer *locktoken, const char *lockscope, const char *locktype, int depth) {

	buffer *b = buffer_init();

	response_header_overwrite(srv, con, CONST_STR_LEN("Lock-Token"), CONST_BUF_LEN(locktoken));

	response_header_overwrite(srv, con,
		CONST_STR_LEN("Content-Type"),
		CONST_STR_LEN("text/xml; charset=\"utf-8\""));

	buffer_copy_string_len(b, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"));

	buffer_append_string_len(b,CONST_STR_LEN("<D:prop xmlns:D=\"DAV:\" xmlns:ns0=\"urn:uuid:c2f41010-65b3-11d1-a29f-00aa00c14882/\">\n"));
	buffer_append_string_len(b,CONST_STR_LEN("<D:lockdiscovery>\n"));
	buffer_append_string_len(b,CONST_STR_LEN("<D:activelock>\n"));

	buffer_append_string_len(b,CONST_STR_LEN("<D:lockscope>"));
	buffer_append_string_len(b,CONST_STR_LEN("<D:"));
	buffer_append_string(b, lockscope);
	buffer_append_string_len(b, CONST_STR_LEN("/>"));
	buffer_append_string_len(b,CONST_STR_LEN("</D:lockscope>\n"));

	buffer_append_string_len(b,CONST_STR_LEN("<D:locktype>"));
	buffer_append_string_len(b,CONST_STR_LEN("<D:"));
	buffer_append_string(b, locktype);
	buffer_append_string_len(b, CONST_STR_LEN("/>"));
	buffer_append_string_len(b,CONST_STR_LEN("</D:locktype>\n"));

	buffer_append_string_len(b,CONST_STR_LEN("<D:depth>"));
	buffer_append_string(b, depth == 0 ? "0" : "infinity");
	buffer_append_string_len(b,CONST_STR_LEN("</D:depth>\n"));

	buffer_append_string_len(b,CONST_STR_LEN("<D:timeout>"));
	buffer_append_string_len(b, CONST_STR_LEN("Second-600"));
	buffer_append_string_len(b,CONST_STR_LEN("</D:timeout>\n"));

	buffer_append_string_len(b,CONST_STR_LEN("<D:owner>"));
	buffer_append_string_len(b,CONST_STR_LEN("</D:owner>\n"));

	buffer_append_string_len(b,CONST_STR_LEN("<D:locktoken>"));
	buffer_append_string_len(b, CONST_STR_LEN("<D:href>"));
	buffer_append_string_buffer(b, locktoken);
	buffer_append_string_len(b, CONST_STR_LEN("</D:href>"));
	buffer_append_string_len(b,CONST_STR_LEN("</D:locktoken>\n"));

	buffer_append_string_len(b,CONST_STR_LEN("</D:activelock>\n"));
	buffer_append_string_len(b,CONST_STR_LEN("</D:lockdiscovery>\n"));
	buffer_append_string_len(b,CONST_STR_LEN("</D:prop>\n"));

	chunkqueue_append_buffer(con->write_queue, b);
	buffer_free(b);

	return 0;
}
#endif

/**
 * check if resource is having the right locks to access to resource
 *
 *
 *
 */
static int webdav_has_lock(server *srv, connection *con, plugin_data *p, buffer *uri) {
	int has_lock = 1;

#ifdef USE_LOCKS
	data_string *ds;
	UNUSED(srv);

	/**
	 * This implementation is more fake than real
	 * we need a parser for the If: header to really handle the full scope
	 *
	 * X-Litmus: locks: 11 (owner_modify)
	 * If: <http://127.0.0.1:1025/dav/litmus/lockme> (<opaquelocktoken:2165478d-0611-49c4-be92-e790d68a38f1>)
	 * - a tagged check:
	 *   if http://127.0.0.1:1025/dav/litmus/lockme is locked with
	 *   opaquelocktoken:2165478d-0611-49c4-be92-e790d68a38f1, go on
	 *
	 * X-Litmus: locks: 16 (fail_cond_put)
	 * If: (<DAV:no-lock> ["-1622396671"])
	 * - untagged:
	 *   go on if the resource has the etag [...] and the lock
	 */
	if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "If"))) {
		/* Ooh, ooh. A if tag, now the fun begins.
		 *
		 * this can only work with a real parser
		 **/
	} else {
		/* we didn't provided a lock-token -> */
		/* if the resource is locked -> 423 */

		sqlite3_stmt *stmt = p->conf.stmt_read_lock_by_uri;

		sqlite3_reset(stmt);

		sqlite3_bind_text(stmt, 1,
			CONST_BUF_LEN(uri),
			SQLITE_TRANSIENT);

		while (SQLITE_ROW == sqlite3_step(stmt)) {
			has_lock = 0;
		}
	}
#else
	UNUSED(srv);
	UNUSED(con);
	UNUSED(p);
	UNUSED(uri);
#endif

	return has_lock;
}

//- 20111209 Sungmin add
static const char* change_webdav_file_path(server *srv, connection *con, const char* source, const char* out)
{
	UNUSED(con);
	
	config_values_t cv[] = {
		{ "alias.url",                  NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ NULL,                         NULL, T_CONFIG_UNSET,  T_CONFIG_SCOPE_UNSET }
	};

	size_t i, j;
	for (i = 1; i < srv->config_context->used; i++) {
		array* alias = array_init();
		cv[0].destination = alias;

		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			continue;
		}
		
		for (j = 0; j < alias->used; j++) {

			data_string *ds = (data_string *)alias->data[j];			
			if( strncmp(source, ds->key->ptr, ds->key->used-1) == 0 ){
				char* buff = replace_str(source, ds->key->ptr, ds->value->ptr, out);
				array_free(alias);
				return buff;
			}
		}

		array_free(alias);
	}

	return source;
}

URIHANDLER_FUNC(mod_webdav_subrequest_handler) {
	plugin_data *p = p_d;
	buffer *b;
	DIR *dir;
	data_string *ds;
	int depth = -1;
	struct stat st;
	buffer *prop_200;
	buffer *prop_404;
	webdav_properties *req_props;
	stat_cache_entry *sce = NULL;

	UNUSED(srv);

	if (!p->conf.enabled) return HANDLER_GO_ON;
	/* physical path is setup */
	if (buffer_is_empty(con->physical.path)) return HANDLER_GO_ON;

	/* PROPFIND need them */
	if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "Depth"))) {
		depth = strtol(ds->value->ptr, NULL, 10);
	}

#if EMBEDDED_EANBLE
	if( !con->srv_socket->is_ssl ){
		if(srv->is_streaming_port_opend && con->request.http_method==HTTP_METHOD_GET){
		}
		else{
			con->http_status = 403;
			return HANDLER_FINISHED;
		}
	}
#endif


	Cdbg(DBE, "http_method=[%d][%s], depth=[%d], con->url->path=[%s]", 
		con->request.http_method, get_http_method_name(con->request.http_method), 
		depth, con->url.path->ptr);	
	
	switch (con->request.http_method) {
	case HTTP_METHOD_PROPFIND:	

		/* they want to know the properties of the directory */
		req_props = NULL;

		/* is there a content-body ? */

		switch (stat_cache_get_entry(srv, con, con->physical.path, &sce)) {
		case HANDLER_ERROR:
			if (errno == ENOENT) {
				con->http_status = 404;
				return HANDLER_FINISHED;
			}
			break;
		default:
			break;
		}


#ifdef USE_PROPPATCH
		/* any special requests or just allprop ? */
		if (con->request.content_length) {
			xmlDocPtr xml;

			if (1 == webdav_parse_chunkqueue(srv, con, p, con->request_content_queue, &xml)) {
				xmlNode *rootnode = xmlDocGetRootElement(xml);

				force_assert(rootnode);

				if (0 == xmlStrcmp(rootnode->name, BAD_CAST "propfind")) {
					xmlNode *cmd;

					req_props = calloc(1, sizeof(*req_props));

					for (cmd = rootnode->children; cmd; cmd = cmd->next) {

						if (0 == xmlStrcmp(cmd->name, BAD_CAST "prop")) {
							/* get prop by name */
							xmlNode *prop;

							for (prop = cmd->children; prop; prop = prop->next) {
								if (prop->type == XML_TEXT_NODE) continue; /* ignore WS */

								if (prop->ns &&
								  (0 == xmlStrcmp(prop->ns->href, BAD_CAST "")) &&
								    (0 != xmlStrcmp(prop->ns->prefix, BAD_CAST ""))) {
									size_t i;
									log_error_write(srv, __FILE__, __LINE__, "ss",
											"no name space for:",
											prop->name);

									xmlFreeDoc(xml);

									for (i = 0; i < req_props->used; i++) {
										free(req_props->ptr[i]->ns);
										free(req_props->ptr[i]->prop);
										free(req_props->ptr[i]);
									}
									free(req_props->ptr);
									free(req_props);

									con->http_status = 400;
									return HANDLER_FINISHED;
								}

								/* add property to requested list */
								if (req_props->size == 0) {
									req_props->size = 16;
									req_props->ptr = malloc(sizeof(*(req_props->ptr)) * req_props->size);
								} else if (req_props->used == req_props->size) {
									req_props->size += 16;
									req_props->ptr = realloc(req_props->ptr, sizeof(*(req_props->ptr)) * req_props->size);
								}

								req_props->ptr[req_props->used] = malloc(sizeof(webdav_property));
								req_props->ptr[req_props->used]->ns = (char *)xmlStrdup(prop->ns ? prop->ns->href : (xmlChar *)"");
								req_props->ptr[req_props->used]->prop = (char *)xmlStrdup(prop->name);
								req_props->used++;
							}
						} else if (0 == xmlStrcmp(cmd->name, BAD_CAST "propname")) {
							sqlite3_stmt *stmt = p->conf.stmt_select_propnames;

							if (stmt) {
								/* get all property names (EMPTY) */
								sqlite3_reset(stmt);
								/* bind the values to the insert */

								sqlite3_bind_text(stmt, 1,
									CONST_BUF_LEN(con->uri.path),
									SQLITE_TRANSIENT);

								if (SQLITE_DONE != sqlite3_step(stmt)) {
								}
							}
						} else if (0 == xmlStrcmp(cmd->name, BAD_CAST "allprop")) {
							/* get all properties (EMPTY) */
							req_props = NULL;
						}
					}
				}

				xmlFreeDoc(xml);
			} else {
				con->http_status = 400;
				return HANDLER_FINISHED;
			}
		}
#endif
		con->http_status = 207;

		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/xml; charset=\"utf-8\""));

		b = buffer_init();

		buffer_copy_string_len(b, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"));

		//buffer_append_string_len(b,CONST_STR_LEN("<D:multistatus xmlns:D=\"DAV:\" xmlns:ns0=\"urn:uuid:c2f41010-65b3-11d1-a29f-00aa00c14882/\">\n"));
		buffer_append_string_len(b,CONST_STR_LEN("<D:multistatus xmlns:D=\"DAV:\" xmlns:ns0=\"urn:uuid:c2f41010-65b3-11d1-a29f-00aa00c14882/\" "));

		char usbdisk_path[50] = "/usbdisk";

#if EMBEDDED_EANBLE
		buffer_append_string_len(b, CONST_STR_LEN(" readonly=\""));	
		#ifndef APP_IPKG
		strcpy(usbdisk_path, "/");
		strcat(usbdisk_path, nvram_get_productid());
		#else
		char *productid = nvram_get_productid();
		strcpy(usbdisk_path, "/");
        strcat(usbdisk_path, productid);
        free(productid);
		#endif
		char* usbdisk_rel_path=NULL;
		char* usbdisk_share_folder=NULL;
		int qry_type=0;
		
		smbc_parse_mnt_path(con->physical.path->ptr, 
						    p->mnt_path->ptr, 
						    p->mnt_path->used-1, 
						    &usbdisk_rel_path, 
						    &usbdisk_share_folder);
		
		if(usbdisk_share_folder!=NULL){
			
			int perm = smbc_get_usbdisk_permission(con->aidisk_username->ptr, usbdisk_rel_path, usbdisk_share_folder);
			
			if(perm==3)
				buffer_append_string(b, "0");
			else
				buffer_append_string(b, "1");

			qry_type = 0;
		}
		else{
			buffer_append_string(b, "1");
			qry_type = 1;
		}
		
		//- Query Type
		buffer_append_string_len(b, CONST_STR_LEN("\" qtype=\""));	
		if(qry_type==1)
			buffer_append_string(b, "1");
		else
			buffer_append_string(b, "0");

		//- Computer Name
		#ifndef APP_IPKG
		buffer_append_string_len(b, CONST_STR_LEN("\" computername=\""));
		buffer_append_string(b, nvram_get_computer_name());
		#else
		char *computer_name = nvram_get_computer_name();
		buffer_append_string_len(b, CONST_STR_LEN("\" computername=\""));
        buffer_append_string(b, computer_name);
        //buffer_append_string(b, nvram_get_computer_name());
        free(computer_name);
		#endif
#else
		buffer_append_string_len(b, CONST_STR_LEN(" readonly=\"0"));
	
		//- Query Type
		buffer_append_string_len(b, CONST_STR_LEN("\" qtype=\""));	
		if(strcmp(con->uri.path->ptr, usbdisk_path)==0)
			buffer_append_string(b, "1");
		else
			buffer_append_string(b, "0");

		//- Computer Name
		buffer_append_string_len(b, CONST_STR_LEN("\" computername=\"WebDAV"));
#endif

		buffer_append_string_len(b, CONST_STR_LEN("\" isusb=\"1"));
	
		buffer_append_string_len(b,CONST_STR_LEN("\">\n"));

		/* allprop */

		prop_200 = buffer_init();
		prop_404 = buffer_init();

		switch(depth) {
		case 0:
			/* Depth: 0 */
			webdav_get_props(srv, con, p, &(con->physical), req_props, prop_200, prop_404);

			buffer_append_string_len(b,CONST_STR_LEN("<D:response>\n"));
			buffer_append_string_len(b,CONST_STR_LEN("<D:href>"));
			buffer_append_string_buffer(b, con->uri.scheme);
			buffer_append_string_len(b,CONST_STR_LEN("://"));
			buffer_append_string_buffer(b, con->uri.authority);
			buffer_append_string_encoded(b, CONST_BUF_LEN(con->uri.path), ENCODING_REL_URI);
			buffer_append_string_len(b,CONST_STR_LEN("</D:href>\n"));

			if (!buffer_string_is_empty(prop_200)) {
				buffer_append_string_len(b,CONST_STR_LEN("<D:propstat>\n"));
				buffer_append_string_len(b,CONST_STR_LEN("<D:prop>\n"));

				buffer_append_string_buffer(b, prop_200);

				buffer_append_string_len(b,CONST_STR_LEN("</D:prop>\n"));

				buffer_append_string_len(b,CONST_STR_LEN("<D:status>HTTP/1.1 200 OK</D:status>\n"));

				buffer_append_string_len(b,CONST_STR_LEN("</D:propstat>\n"));
			}
			if (!buffer_string_is_empty(prop_404)) {
				buffer_append_string_len(b,CONST_STR_LEN("<D:propstat>\n"));
				buffer_append_string_len(b,CONST_STR_LEN("<D:prop>\n"));

				buffer_append_string_buffer(b, prop_404);

				buffer_append_string_len(b,CONST_STR_LEN("</D:prop>\n"));

				buffer_append_string_len(b,CONST_STR_LEN("<D:status>HTTP/1.1 404 Not Found</D:status>\n"));

				buffer_append_string_len(b,CONST_STR_LEN("</D:propstat>\n"));
			}

			buffer_append_string_len(b,CONST_STR_LEN("</D:response>\n"));

			break;
		case 1:
			if (NULL != (dir = opendir(con->physical.path->ptr))) {
				struct dirent *de;
				physical d;
				physical *dst = &(con->physical);

				d.path = buffer_init();
				d.rel_path = buffer_init();

				while(NULL != (de = readdir(dir))) {
					if (de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0') {
						continue;
						/* ignore the parent dir */
					}

					if ( de->d_name[0] == '.' ) {
						continue;
						//- ignore the hidden file
					}
					
					if ( check_skip_folder_name(de->d_name) == 1 ) {
						continue;
					}

					buffer_copy_buffer(d.path, dst->path);
					buffer_append_slash(d.path);

					buffer_copy_buffer(d.rel_path, dst->rel_path);
					buffer_append_slash(d.rel_path);

					if (de->d_name[0] == '.' && de->d_name[1] == '\0') {
						/* don't append the . */
					} else {
						buffer_append_string(d.path, de->d_name);
						buffer_append_string(d.rel_path, de->d_name);
					}

#if EMBEDDED_EANBLE
					int permission = -1;
					char* usbdisk_rel_sub_path = NULL;
					char* usbdisk_sub_share_folder = NULL;
					smbc_parse_mnt_path(d.path->ptr, 
										p->mnt_path->ptr, 
										p->mnt_path->used-1, 
										&usbdisk_rel_sub_path, 
										&usbdisk_sub_share_folder);
					
					if( usbdisk_sub_share_folder != NULL ){						
						permission = smbc_get_usbdisk_permission(con->aidisk_username->ptr, usbdisk_rel_sub_path, usbdisk_sub_share_folder);
						
						free(usbdisk_rel_sub_path);
						free(usbdisk_sub_share_folder);
						
						if(permission!=1 && permission!=3)
							continue;
					}
					else
						free(usbdisk_rel_sub_path);
#endif
				
					buffer_reset(prop_200);
					buffer_reset(prop_404);

					webdav_get_props(srv, con, p, &d, req_props, prop_200, prop_404);

					buffer_append_string_len(b,CONST_STR_LEN("<D:response>\n"));
					buffer_append_string_len(b,CONST_STR_LEN("<D:href>"));
					buffer_append_string_buffer(b, con->uri.scheme);
					buffer_append_string_len(b,CONST_STR_LEN("://"));
					buffer_append_string_buffer(b, con->uri.authority);

					if(con->share_link_shortpath->used){
						char buff[4096];
						replace_str(&d.rel_path->ptr[0], 
							                    (char*)con->share_link_realpath->ptr, 
							                    (char*)con->share_link_shortpath->ptr, 
							        buff);
						
						buffer_append_string(b, "/");
						buffer_append_string_encoded(b, buff, strlen(buff), ENCODING_REL_URI);
					}
					else{
						//Cdbg(DBE, "d.rel_path=%s", d.rel_path->ptr);
						buffer_append_string_encoded(b, CONST_BUF_LEN(d.rel_path), ENCODING_REL_URI);
					}
					
					buffer_append_string_len(b,CONST_STR_LEN("</D:href>\n"));

					if (!buffer_string_is_empty(prop_200)) {
						buffer_append_string_len(b,CONST_STR_LEN("<D:propstat>\n"));
						buffer_append_string_len(b,CONST_STR_LEN("<D:prop>\n"));

						buffer_append_string_buffer(b, prop_200);

						buffer_append_string_len(b,CONST_STR_LEN("</D:prop>\n"));

						buffer_append_string_len(b,CONST_STR_LEN("<D:status>HTTP/1.1 200 OK</D:status>\n"));

						buffer_append_string_len(b,CONST_STR_LEN("</D:propstat>\n"));
					}
					if (!buffer_string_is_empty(prop_404)) {
						buffer_append_string_len(b,CONST_STR_LEN("<D:propstat>\n"));
						buffer_append_string_len(b,CONST_STR_LEN("<D:prop>\n"));

						buffer_append_string_buffer(b, prop_404);

						buffer_append_string_len(b,CONST_STR_LEN("</D:prop>\n"));

						buffer_append_string_len(b,CONST_STR_LEN("<D:status>HTTP/1.1 404 Not Found</D:status>\n"));

						buffer_append_string_len(b,CONST_STR_LEN("</D:propstat>\n"));
					}

					buffer_append_string_len(b,CONST_STR_LEN("</D:response>\n"));
				}
				closedir(dir);
				buffer_free(d.path);
				buffer_free(d.rel_path);
			}
			break;
		}

		if (req_props) {
			size_t i;
			for (i = 0; i < req_props->used; i++) {
				free(req_props->ptr[i]->ns);
				free(req_props->ptr[i]->prop);
				free(req_props->ptr[i]);
			}
			free(req_props->ptr);
			free(req_props);
		}

		buffer_free(prop_200);
		buffer_free(prop_404);

		buffer_append_string_len(b,CONST_STR_LEN("</D:multistatus>\n"));

		if (p->conf.log_xml) {
			log_error_write(srv, __FILE__, __LINE__, "sb", "XML-response-body:", b);
		}

		chunkqueue_append_buffer(con->write_queue, b);
		buffer_free(b);

		con->file_finished = 1;

		return HANDLER_FINISHED;
	case HTTP_METHOD_MKCOL:
		if (p->conf.is_readonly) {
			con->http_status = 403;
			return HANDLER_FINISHED;
		}

		if (con->request.content_length != 0) {
			/* we don't support MKCOL with a body */
			con->http_status = 415;

			return HANDLER_FINISHED;
		}

		/* let's create the directory */

		if (-1 == mkdir(con->physical.path->ptr, WEBDAV_DIR_MODE)) {
			switch(errno) {
			case EPERM:
				con->http_status = 403;
				break;
			case ENOENT:
			case ENOTDIR:
				con->http_status = 409;
				break;
			case EEXIST:
			default:
				con->http_status = 405; /* not allowed */
				break;
			}
		} else {
			log_sys_write(srv, "sbss", "Create folder", con->url.rel_path, "from ip", con->dst_addr_buf->ptr);
			con->http_status = 201;
			con->file_finished = 1;
		}

		return HANDLER_FINISHED;
	case HTTP_METHOD_DELETE:
		if (p->conf.is_readonly) {
			con->http_status = 403;
			return HANDLER_FINISHED;
		}

		/* does the client have a lock for this connection ? */
		if (!webdav_has_lock(srv, con, p, con->uri.path)) {
			con->http_status = 423;
			return HANDLER_FINISHED;
		}

		/* stat and unlink afterwards */
		if (-1 == stat(con->physical.path->ptr, &st)) {
			/* don't about it yet, unlink will fail too */
			switch(errno) {
			case ENOENT:
				 con->http_status = 404;
				 break;
			default:
				 con->http_status = 403;
				 break;
			}
		} else if (S_ISDIR(st.st_mode)) {
			buffer *multi_status_resp = buffer_init();

			if (webdav_delete_dir(srv, con, p, &(con->physical), multi_status_resp)) {
				/* we got an error somewhere in between, build a 207 */
				response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/xml; charset=\"utf-8\""));

				b = buffer_init();

				buffer_copy_string_len(b, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"));

				buffer_append_string_len(b,CONST_STR_LEN("<D:multistatus xmlns:D=\"DAV:\">\n"));

				buffer_append_string_buffer(b, multi_status_resp);

				buffer_append_string_len(b,CONST_STR_LEN("</D:multistatus>\n"));

				if (p->conf.log_xml) {
					log_error_write(srv, __FILE__, __LINE__, "sb", "XML-response-body:", b);
				}

				chunkqueue_append_buffer(con->write_queue, b);
				buffer_free(b);

				con->http_status = 207;
				con->file_finished = 1;
			} else {
				/* everything went fine, remove the directory */

				if (-1 == rmdir(con->physical.path->ptr)) {
					switch(errno) {
					case ENOENT:
						con->http_status = 404;
						break;
					default:
						con->http_status = 501;
						break;
					}
				} else {
					log_sys_write(srv, "sbss", "Delete folder", con->url.rel_path, "from ip", con->dst_addr_buf->ptr);
					con->http_status = 204;
				}
			}

			buffer_free(multi_status_resp);
		} else if (-1 == unlink(con->physical.path->ptr)) {
			switch(errno) {
			case EPERM:
				con->http_status = 403;
				break;
			case ENOENT:
				con->http_status = 404;
				break;
			default:
				con->http_status = 501;
				break;
			}
		} else {
			log_sys_write(srv, "sbss", "Delete file", con->url.rel_path, "from ip", con->dst_addr_buf->ptr);
			con->http_status = 204;
		}
		return HANDLER_FINISHED;
	case HTTP_METHOD_PUT: {
		int fd;
		chunkqueue *cq = con->request_content_queue;
		chunk *c;
		data_string *ds_range;

		if (p->conf.is_readonly) {
			con->http_status = 403;
			return HANDLER_FINISHED;
		}

		/* is a exclusive lock set on the source */
		if (!webdav_has_lock(srv, con, p, con->uri.path)) {
			con->http_status = 423;
			return HANDLER_FINISHED;
		}

		//- Sungmin add
		int autoCreateFolder = 0;
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "Auto-CreateFolder"))) {			
			if (ds->value->used != 2 ||
			    (ds->value->ptr[0] != 'F' &&
			     ds->value->ptr[0] != 'T') )  {
				con->http_status = 400;
				return HANDLER_FINISHED;
			}
			autoCreateFolder = (ds->value->ptr[0] == 'F' ? 0 : 1);
		}

		if(autoCreateFolder){
			char* file_path = NULL;			
			extract_filepath(con->physical.path->ptr, &file_path);
			createDirectory(file_path);
			free(file_path);
		}
		////////////////////////////////

		assert(chunkqueue_length(cq) == (off_t)con->request.content_length);

		/* RFC2616 Section 9.6 PUT requires us to send 501 on all Content-* we don't support
		 * - most important Content-Range
		 *
		 *
		 * Example: Content-Range: bytes 100-1037/1038 */

		if (NULL != (ds_range = (data_string *)array_get_element(con->request.headers, "Content-Range"))) {
			const char *num = ds_range->value->ptr;
			off_t offset;
			char *err = NULL;

			if (0 != strncmp(num, "bytes ", 6)) {
				con->http_status = 501; /* not implemented */

				return HANDLER_FINISHED;
			}

			/* we only support <num>- ... */

			num += 6;

			/* skip WS */
			while (*num == ' ' || *num == '\t') num++;

			if (*num == '\0') {
				con->http_status = 501; /* not implemented */

				return HANDLER_FINISHED;
			}

			offset = strtoll(num, &err, 10);

			if (*err != '-' || offset < 0) {
				con->http_status = 501; /* not implemented */

				return HANDLER_FINISHED;
			}

			//- Sungmin modify if offset is zero, we create a new file.
			if(offset==0){				
				if (-1 == (fd = open(con->physical.path->ptr, O_WRONLY|O_TRUNC, WEBDAV_FILE_MODE))) {
					if (errno == ENOENT &&
					    -1 == (fd = open(con->physical.path->ptr, O_WRONLY|O_CREAT|O_TRUNC|O_EXCL, WEBDAV_FILE_MODE))) {
						/* we can't open the file */						
						switch(errno) {
						case EEXIST:
							con->http_status = 412;
							break;
						case EISDIR:
							con->http_status = 427;
							break;
						case ENOENT:
							/* at least one part in the middle wasn't existing */
							con->http_status = 427;
							break;
						default:
							con->http_status = 403;
							break;
						}

						return HANDLER_FINISHED;
					} 
					else {
						con->http_status = 201; /* created */
					}
				} 
				else {
					con->http_status = 200; /* modified */
				}
			}
			else{				
				if (-1 == (fd = open(con->physical.path->ptr, O_WRONLY, WEBDAV_FILE_MODE))) {
					switch (errno) {
					case ENOENT:
						con->http_status = 404; /* not found */
						break;
					default:
						con->http_status = 403; /* not found */
						break;
					}
					return HANDLER_FINISHED;
				}
			}

			if (-1 == lseek(fd, offset, SEEK_SET)) {
				con->http_status = 501; /* not implemented */

				close(fd);

				return HANDLER_FINISHED;
			}
			con->http_status = 200; /* modified */
		} 
		else {
			/* take what we have in the request-body and write it to a file */

			/* if the file doesn't exist, create it */
			if (-1 == (fd = open(con->physical.path->ptr, O_WRONLY|O_TRUNC, WEBDAV_FILE_MODE))) {
				if (errno != ENOENT ||
				    -1 == (fd = open(con->physical.path->ptr, O_WRONLY|O_CREAT|O_TRUNC|O_EXCL, WEBDAV_FILE_MODE))) {
					/* we can't open the file */
					con->http_status = 403;

					return HANDLER_FINISHED;
				} 
				else {
					con->http_status = 201; /* created */
				}
			} 
			else {
				con->http_status = 200; /* modified */
			}
		}

		con->file_finished = 1;

		for (c = cq->first; c; c = cq->first) {
			int r = 0;

			/* copy all chunks */
			switch(c->type) {
			case FILE_CHUNK:

				if (c->file.mmap.start == MAP_FAILED) {
					if (-1 == c->file.fd &&  /* open the file if not already open */
					    -1 == (c->file.fd = open(c->file.name->ptr, O_RDONLY))) {
						log_error_write(srv, __FILE__, __LINE__, "ss", "open failed: ", strerror(errno));
						close(fd);
						return HANDLER_ERROR;
					}

					if (MAP_FAILED == (c->file.mmap.start = mmap(NULL, c->file.length, PROT_READ, MAP_SHARED, c->file.fd, 0))) {
						log_error_write(srv, __FILE__, __LINE__, "ssbd", "mmap failed: ",
								strerror(errno), c->file.name,  c->file.fd);
						close(c->file.fd);
						c->file.fd = -1;
						close(fd);
						return HANDLER_ERROR;
					}

					c->file.mmap.length = c->file.length;

					close(c->file.fd);
					c->file.fd = -1;

					/* chunk_reset() or chunk_free() will cleanup for us */
				}

				if ((r = write(fd, c->file.mmap.start + c->offset, c->file.length - c->offset)) < 0) {
					switch(errno) {
					case ENOSPC:
						con->http_status = 507;

						break;
					default:
						con->http_status = 403;
						break;
					}
				}
				break;
			case MEM_CHUNK:
				if ((r = write(fd, c->mem->ptr + c->offset, buffer_string_length(c->mem) - c->offset)) < 0) {
					switch(errno) {
					case ENOSPC:
						con->http_status = 507;

						break;
					default:
						con->http_status = 403;
						break;
					}
				}
				break;
			}

			if (r > 0) {
				chunkqueue_mark_written(cq, r);
			} else {
				break;
			}
		}
		close(fd);

		return HANDLER_FINISHED;
	}
	case HTTP_METHOD_MOVE:
	case HTTP_METHOD_COPY: {
		buffer *destination = NULL;
		char *sep, *sep2, *start;
		int overwrite = 1;

		if (p->conf.is_readonly) {
			con->http_status = 403;
			return HANDLER_FINISHED;
		}

		/* is a exclusive lock set on the source */
		if (con->request.http_method == HTTP_METHOD_MOVE) {
			if (!webdav_has_lock(srv, con, p, con->uri.path)) {
				con->http_status = 423;
				return HANDLER_FINISHED;
			}
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "Destination"))) {
			destination = ds->value;
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "Overwrite"))) {
			if (buffer_string_length(ds->value) != 1 ||
			    (ds->value->ptr[0] != 'F' &&
			     ds->value->ptr[0] != 'T') )  {
				con->http_status = 400;
				return HANDLER_FINISHED;
			}
			overwrite = (ds->value->ptr[0] == 'F' ? 0 : 1);
		}
		/* let's parse the Destination
		 *
		 * http://127.0.0.1:1025/dav/litmus/copydest
		 *
		 * - host has to be the same as the Host: header we got
		 * - we have to stay inside the document root
		 * - the query string is thrown away
		 *  */

		buffer_reset(p->uri.scheme);
		buffer_reset(p->uri.path_raw);
		buffer_reset(p->uri.authority);

		start = destination->ptr;

		if (NULL == (sep = strstr(start, "://"))) {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}
		buffer_copy_string_len(p->uri.scheme, start, sep - start);

		start = sep + 3;

		if (NULL == (sep = strchr(start, '/'))) {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}
		if (NULL != (sep2 = memchr(start, '@', sep - start))) {
			/* skip login information */
			start = sep2 + 1;
		}
		buffer_copy_string_len(p->uri.authority, start, sep - start);

		start = sep + 1;

		if (NULL == (sep = strchr(start, '?'))) {
			/* no query string, good */
			buffer_copy_string(p->uri.path_raw, start);
		} else {
			buffer_copy_string_len(p->uri.path_raw, start, sep - start);
		}

		if (!buffer_is_equal(p->uri.authority, con->uri.authority)) {
			/* not the same host */
			con->http_status = 502;
			return HANDLER_FINISHED;
		}

		buffer_copy_buffer(p->tmp_buf, p->uri.path_raw);
		buffer_urldecode_path(p->tmp_buf);
		buffer_path_simplify(p->uri.path, p->tmp_buf);

		//- 20121115 Sungmin add for router sync.
		if(con->share_link_shortpath->used){
			
			char* a;
			if (NULL != ( a = strstr(p->uri.path->ptr, con->share_link_shortpath->ptr))){
				char buff[4096];
				replace_str(a, con->share_link_shortpath->ptr, con->share_link_realpath->ptr, buff);
				buffer_copy_string( p->uri.path, buff );
			}
			else{
				con->http_status = 502;
				return HANDLER_FINISHED;
			}
		}
		
		//- 20111209 Sungmin add
		char buff[4096];
		memset(buff, 0, sizeof(buff));
		buffer_copy_string(p->uri.path, change_webdav_file_path(srv, con, p->uri.path->ptr, buff));
		
		/* we now have a URI which is clean. transform it into a physical path */
		buffer_copy_buffer(p->physical.doc_root, con->physical.doc_root);
		buffer_copy_buffer(p->physical.rel_path, p->uri.path);

		if (con->conf.force_lowercase_filenames) {
			buffer_to_lower(p->physical.rel_path);
		}

		buffer_copy_buffer(p->physical.path, p->physical.doc_root);
		buffer_append_slash(p->physical.path);
		buffer_copy_buffer(p->physical.basedir, p->physical.path);

		/* don't add a second / */
		if (p->physical.rel_path->ptr[0] == '/') {
			buffer_append_string_len(p->physical.path, p->physical.rel_path->ptr + 1, buffer_string_length(p->physical.rel_path) - 1);
		} else {
			buffer_append_string_buffer(p->physical.path, p->physical.rel_path);
		}

		/* let's see if the source is a directory
		 * if yes, we fail with 501 */

		if (-1 == stat(con->physical.path->ptr, &st)) {
			/* don't about it yet, unlink will fail too */
			switch(errno) {
			case ENOENT:
				 con->http_status = 404;
				 break;
			default:
				 con->http_status = 403;
				 break;
			}
		} else if (S_ISDIR(st.st_mode)) {
			int r;
			/* src is a directory */

			if (-1 == stat(p->physical.path->ptr, &st)) {
				//- dst is not exist.
				if(in_the_same_folder(con->physical.path, p->physical.path)) {
					if( rename(con->physical.path->ptr, p->physical.path->ptr) ) {
						con->http_status = 403;
					} else {
						con->http_status = 201; //Created
						log_sys_write(srv, "sbsbss", "Move", con->physical.rel_path, "to", p->physical.rel_path ,"from ip", con->dst_addr_buf->ptr);						
					}
					
					con->file_finished = 1;
					return HANDLER_FINISHED;
				} else if (-1 == mkdir(p->physical.path->ptr, WEBDAV_DIR_MODE)) {
					con->http_status = 403;
					return HANDLER_FINISHED;
				}
			} 
			else if (!S_ISDIR(st.st_mode)) {
				//- dst is not folder.				
				if (overwrite == 0) {
					/* copying into a non-dir ? */
					con->http_status = 409;
					return HANDLER_FINISHED;
				} else {
					unlink(p->physical.path->ptr);
					if (-1 == mkdir(p->physical.path->ptr, WEBDAV_DIR_MODE)) {
						con->http_status = 403;
						return HANDLER_FINISHED;
					}
				}
			}
			else if (S_ISDIR(st.st_mode)) {
				//- dst folder is exist.
				if (overwrite == 0) {
					/* copying into a non-dir ? */
					con->http_status = 412;
					return HANDLER_FINISHED;
				}
			}

			/* copy the content of src to dest */
			if (0 != (r = webdav_copy_dir(srv, con, p, &(con->physical), &(p->physical), overwrite))) {
				con->http_status = r;
				return HANDLER_FINISHED;
			}
			if (con->request.http_method == HTTP_METHOD_MOVE) {
				b = buffer_init();
				webdav_delete_dir(srv, con, p, &(con->physical), b); /* content */
				buffer_free(b);
				Cdbg(DBE, "Move %s to %s from ip", con->url.rel_path->ptr, p->physical.rel_path->ptr);
				rmdir(con->physical.path->ptr);

				log_sys_write(srv, "sbsbss", "Move", con->url.rel_path, "to", p->physical.rel_path ,"from ip", con->dst_addr_buf->ptr);
			}
			con->http_status = 201;
			con->file_finished = 1;
		} else {
			/* it is just a file, good */
			int r;

			/* does the client have a lock for this connection ? */
			if (!webdav_has_lock(srv, con, p, p->uri.path)) {
				con->http_status = 423;
				return HANDLER_FINISHED;
			}

			/* destination exists */
			if (0 == (r = stat(p->physical.path->ptr, &st))) {
				if (S_ISDIR(st.st_mode)) {
					if (overwrite == 0) {
						/* copying into a non-dir ? */
						con->http_status = 409;
						return HANDLER_FINISHED;
					}
					
					/* file to dir/
					 * append basename to physical path */

					if (NULL != (sep = strrchr(con->physical.path->ptr, '/'))) {
						buffer_append_string(p->physical.path, sep);
						r = stat(p->physical.path->ptr, &st);
					}
				}
			}

			if (-1 == r) {
				con->http_status = 201; /* we will create a new one */
				con->file_finished = 1;

				switch(errno) {
				case ENOTDIR:
					con->http_status = 409;
					return HANDLER_FINISHED;
				}
			} else if (overwrite == 0) {
				/* destination exists, but overwrite is not set */
				con->http_status = 412;
				return HANDLER_FINISHED;
			} else {
				con->http_status = 204; /* resource already existed */
			}

			if (con->request.http_method == HTTP_METHOD_MOVE) {
				/* try a rename */

				if (0 == rename(con->physical.path->ptr, p->physical.path->ptr)) {
#ifdef USE_PROPPATCH
					sqlite3_stmt *stmt;

					stmt = p->conf.stmt_delete_uri;
					if (stmt) {

						sqlite3_reset(stmt);

						/* bind the values to the insert */
						sqlite3_bind_text(stmt, 1,
							CONST_BUF_LEN(con->uri.path),
							SQLITE_TRANSIENT);

						if (SQLITE_DONE != sqlite3_step(stmt)) {
							log_error_write(srv, __FILE__, __LINE__, "ss", "sql-move(delete old) failed:", sqlite3_errmsg(p->conf.sql));
						}
					}

					stmt = p->conf.stmt_move_uri;
					if (stmt) {

						sqlite3_reset(stmt);

						/* bind the values to the insert */
						sqlite3_bind_text(stmt, 1,
							CONST_BUF_LEN(p->uri.path),
							SQLITE_TRANSIENT);

						sqlite3_bind_text(stmt, 2,
							CONST_BUF_LEN(con->uri.path),
							SQLITE_TRANSIENT);

						if (SQLITE_DONE != sqlite3_step(stmt)) {
							log_error_write(srv, __FILE__, __LINE__, "ss", "sql-move failed:", sqlite3_errmsg(p->conf.sql));
						}
					}
#endif
					log_sys_write(srv, "sbsbss", "Move", con->url.rel_path, "to", p->physical.rel_path ,"from ip", con->dst_addr_buf->ptr);

					return HANDLER_FINISHED;
				}

				/* rename failed, fall back to COPY + DELETE */
			}

			if (0 != (r = webdav_copy_file(srv, con, p, &(con->physical), &(p->physical), overwrite))) {
				con->http_status = r;

				return HANDLER_FINISHED;
			}

			if (con->request.http_method == HTTP_METHOD_MOVE) {
				b = buffer_init();
				webdav_delete_file(srv, con, p, &(con->physical), b);
				buffer_free(b);

				log_sys_write(srv, "sbsbss", "Move", con->url.rel_path, "to", p->physical.rel_path ,"from ip", con->dst_addr_buf->ptr);
			}
		}

		return HANDLER_FINISHED;
	}
	case HTTP_METHOD_PROPPATCH:
		if (p->conf.is_readonly) {
			con->http_status = 403;
			return HANDLER_FINISHED;
		}

		if (!webdav_has_lock(srv, con, p, con->uri.path)) {
			con->http_status = 423;
			return HANDLER_FINISHED;
		}

		/* check if destination exists */
		if (-1 == stat(con->physical.path->ptr, &st)) {
			switch(errno) {
			case ENOENT:
				con->http_status = 404;
				break;
			}
		}

#ifdef USE_PROPPATCH
		if (con->request.content_length) {
			xmlDocPtr xml;

			if (1 == webdav_parse_chunkqueue(srv, con, p, con->request_content_queue, &xml)) {
				xmlNode *rootnode = xmlDocGetRootElement(xml);

				if (0 == xmlStrcmp(rootnode->name, BAD_CAST "propertyupdate")) {
					xmlNode *cmd;
					char *err = NULL;
					int empty_ns = 0; /* send 400 on a empty namespace attribute */

					/* start response */

					if (SQLITE_OK != sqlite3_exec(p->conf.sql, "BEGIN TRANSACTION", NULL, NULL, &err)) {
						log_error_write(srv, __FILE__, __LINE__, "ss", "can't open transaction:", err);
						sqlite3_free(err);

						goto propmatch_cleanup;
					}

					/* a UPDATE request, we know 'set' and 'remove' */
					for (cmd = rootnode->children; cmd; cmd = cmd->next) {
						xmlNode *props;
						/* either set or remove */

						if ((0 == xmlStrcmp(cmd->name, BAD_CAST "set")) ||
						    (0 == xmlStrcmp(cmd->name, BAD_CAST "remove"))) {

							sqlite3_stmt *stmt;

							stmt = (0 == xmlStrcmp(cmd->name, BAD_CAST "remove")) ?
								p->conf.stmt_delete_prop : p->conf.stmt_update_prop;

							for (props = cmd->children; props; props = props->next) {
								if (0 == xmlStrcmp(props->name, BAD_CAST "prop")) {
									xmlNode *prop;
									int r;

									prop = props->children;

									if (prop->ns &&
									    (0 == xmlStrcmp(prop->ns->href, BAD_CAST "")) &&
									    (0 != xmlStrcmp(prop->ns->prefix, BAD_CAST ""))) {
										log_error_write(srv, __FILE__, __LINE__, "ss",
												"no name space for:",
												prop->name);

										empty_ns = 1;
										break;
									}

									sqlite3_reset(stmt);

									/* bind the values to the insert */

									sqlite3_bind_text(stmt, 1,
										CONST_BUF_LEN(con->uri.path),
										SQLITE_TRANSIENT);
									sqlite3_bind_text(stmt, 2,
										(char *)prop->name,
										strlen((char *)prop->name),
										SQLITE_TRANSIENT);
									if (prop->ns) {
										sqlite3_bind_text(stmt, 3,
											(char *)prop->ns->href,
											strlen((char *)prop->ns->href),
											SQLITE_TRANSIENT);
									} else {
										sqlite3_bind_text(stmt, 3,
											"",
											0,
											SQLITE_TRANSIENT);
									}
									if (stmt == p->conf.stmt_update_prop) {
										sqlite3_bind_text(stmt, 4,
											(char *)xmlNodeGetContent(prop),
											strlen((char *)xmlNodeGetContent(prop)),
											SQLITE_TRANSIENT);
									}

									if (SQLITE_DONE != (r = sqlite3_step(stmt))) {
										log_error_write(srv, __FILE__, __LINE__, "ss",
												"sql-set failed:", sqlite3_errmsg(p->conf.sql));
									}
								}
							}
							if (empty_ns) break;
						}
					}

					if (empty_ns) {
						if (SQLITE_OK != sqlite3_exec(p->conf.sql, "ROLLBACK", NULL, NULL, &err)) {
							log_error_write(srv, __FILE__, __LINE__, "ss", "can't rollback transaction:", err);
							sqlite3_free(err);

							goto propmatch_cleanup;
						}

						con->http_status = 400;
					} else {
						if (SQLITE_OK != sqlite3_exec(p->conf.sql, "COMMIT", NULL, NULL, &err)) {
							log_error_write(srv, __FILE__, __LINE__, "ss", "can't commit transaction:", err);
							sqlite3_free(err);

							goto propmatch_cleanup;
						}
						con->http_status = 200;
					}
					con->file_finished = 1;

					return HANDLER_FINISHED;
				}

propmatch_cleanup:

				xmlFreeDoc(xml);
			} else {
				con->http_status = 400;
				return HANDLER_FINISHED;
			}
		}
#endif
		con->http_status = 501;
		return HANDLER_FINISHED;
	case HTTP_METHOD_LOCK:
		/**
		 * a mac wants to write
		 *
		 * LOCK /dav/expire.txt HTTP/1.1\r\n
		 * User-Agent: WebDAVFS/1.3 (01308000) Darwin/8.1.0 (Power Macintosh)\r\n
		 * Accept: * / *\r\n
		 * Depth: 0\r\n
		 * Timeout: Second-600\r\n
		 * Content-Type: text/xml; charset=\"utf-8\"\r\n
		 * Content-Length: 229\r\n
		 * Connection: keep-alive\r\n
		 * Host: 192.168.178.23:1025\r\n
		 * \r\n
		 * <?xml version=\"1.0\" encoding=\"utf-8\"?>\n
		 * <D:lockinfo xmlns:D=\"DAV:\">\n
		 *  <D:lockscope><D:exclusive/></D:lockscope>\n
		 *  <D:locktype><D:write/></D:locktype>\n
		 *  <D:owner>\n
		 *   <D:href>http://www.apple.com/webdav_fs/</D:href>\n
		 *  </D:owner>\n
		 * </D:lockinfo>\n
		 */

		if (depth != 0 && depth != -1) {
			con->http_status = 400;

			return HANDLER_FINISHED;
		}

#ifdef USE_LOCKS
		if (con->request.content_length) {
			xmlDocPtr xml;
			buffer *hdr_if = NULL;

			if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "If"))) {
				hdr_if = ds->value;
			}

			/* we don't support Depth: Infinity on locks */
			if (hdr_if == NULL && depth == -1) {
				con->http_status = 409; /* Conflict */

				return HANDLER_FINISHED;
			}

			if (1 == webdav_parse_chunkqueue(srv, con, p, con->request_content_queue, &xml)) {
				xmlNode *rootnode = xmlDocGetRootElement(xml);

				force_assert(rootnode);

				if (0 == xmlStrcmp(rootnode->name, BAD_CAST "lockinfo")) {
					xmlNode *lockinfo;
					const xmlChar *lockscope = NULL, *locktype = NULL; /* TODO: compiler says unused: *owner = NULL; */

					for (lockinfo = rootnode->children; lockinfo; lockinfo = lockinfo->next) {
						if (0 == xmlStrcmp(lockinfo->name, BAD_CAST "lockscope")) {
							xmlNode *value;
							for (value = lockinfo->children; value; value = value->next) {
								if ((0 == xmlStrcmp(value->name, BAD_CAST "exclusive")) ||
								    (0 == xmlStrcmp(value->name, BAD_CAST "shared"))) {
									lockscope = value->name;
								} else {
									con->http_status = 400;

									xmlFreeDoc(xml);
									return HANDLER_FINISHED;
								}
							}
						} else if (0 == xmlStrcmp(lockinfo->name, BAD_CAST "locktype")) {
							xmlNode *value;
							for (value = lockinfo->children; value; value = value->next) {
								if ((0 == xmlStrcmp(value->name, BAD_CAST "write"))) {
									locktype = value->name;
								} else {
									con->http_status = 400;

									xmlFreeDoc(xml);
									return HANDLER_FINISHED;
								}
							}

						} else if (0 == xmlStrcmp(lockinfo->name, BAD_CAST "owner")) {
						}
					}

					if (lockscope && locktype) {
						sqlite3_stmt *stmt = p->conf.stmt_read_lock_by_uri;

						/* is this resourse already locked ? */

						/* SELECT locktoken, resource, lockscope, locktype, owner, depth, timeout
						 *   FROM locks
						 *  WHERE resource = ? */

						if (stmt) {

							sqlite3_reset(stmt);

							sqlite3_bind_text(stmt, 1,
								CONST_BUF_LEN(p->uri.path),
								SQLITE_TRANSIENT);

							/* it is the PK */
							while (SQLITE_ROW == sqlite3_step(stmt)) {
								/* we found a lock
								 * 1. is it compatible ?
								 * 2. is it ours */
								char *sql_lockscope = (char *)sqlite3_column_text(stmt, 2);

								if (strcmp(sql_lockscope, "exclusive")) {
									con->http_status = 423;
								} else if (0 == xmlStrcmp(lockscope, BAD_CAST "exclusive")) {
									/* resourse is locked with a shared lock
									 * client wants exclusive */
									con->http_status = 423;
								}
							}
							if (con->http_status == 423) {
								xmlFreeDoc(xml);
								return HANDLER_FINISHED;
							}
						}

						stmt = p->conf.stmt_create_lock;
						if (stmt) {
							/* create a lock-token */
							//uuid_t id;
							char uuid[37] /* 36 + \0 */;

							sprintf( uuid, "%d", rand() );

							//uuid_generate(id);
							//uuid_unparse(id, uuid);

							buffer_copy_string_len(p->tmp_buf, CONST_STR_LEN("opaquelocktoken:"));
							buffer_append_string(p->tmp_buf, uuid);

							/* "CREATE TABLE locks ("
							 * "  locktoken TEXT NOT NULL,"
							 * "  resource TEXT NOT NULL,"
							 * "  lockscope TEXT NOT NULL,"
							 * "  locktype TEXT NOT NULL,"
							 * "  owner TEXT NOT NULL,"
							 * "  depth INT NOT NULL,"
							 */

							sqlite3_reset(stmt);

							/* locktoken */
							sqlite3_bind_text(stmt, 1,
									CONST_BUF_LEN(p->tmp_buf),
									SQLITE_TRANSIENT);

							/* resource */
							sqlite3_bind_text(stmt, 2,
									CONST_BUF_LEN(con->uri.path),
									SQLITE_TRANSIENT);

							/* lockscope */
							sqlite3_bind_text(stmt, 3,
									(const char *)lockscope,
									xmlStrlen(lockscope),
									SQLITE_TRANSIENT);

							/* locktype */
							sqlite3_bind_text(stmt, 4,
									(const char *)locktype,
									xmlStrlen(locktype),
									SQLITE_TRANSIENT);

							/* owner */
							sqlite3_bind_text(stmt, 5,
									"",
									0,
									SQLITE_TRANSIENT);

							/* depth */
							sqlite3_bind_int(stmt, 6,
									depth);


							if (SQLITE_DONE != sqlite3_step(stmt)) {
								log_error_write(srv, __FILE__, __LINE__, "ss",
										"create lock:", sqlite3_errmsg(p->conf.sql));
							}

							/* looks like we survived */
							webdav_lockdiscovery(srv, con, p->tmp_buf, (const char *)lockscope, (const char *)locktype, depth);

							con->http_status = 201;
							con->file_finished = 1;
						}
					}
				}

				xmlFreeDoc(xml);
				return HANDLER_FINISHED;
			} else {
				con->http_status = 400;
				return HANDLER_FINISHED;
			}
		} else {

			if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "If"))) {
				buffer *locktoken = ds->value;
				sqlite3_stmt *stmt = p->conf.stmt_refresh_lock;

				/* remove the < > around the token */
				if (buffer_string_length(locktoken) < 5) {
					con->http_status = 400;

					return HANDLER_FINISHED;
				}

				buffer_copy_string_len(p->tmp_buf, locktoken->ptr + 2, buffer_string_length(locktoken) - 4);

				sqlite3_reset(stmt);

				sqlite3_bind_text(stmt, 1,
					CONST_BUF_LEN(p->tmp_buf),
					SQLITE_TRANSIENT);

				if (SQLITE_DONE != sqlite3_step(stmt)) {
					log_error_write(srv, __FILE__, __LINE__, "ss",
						"refresh lock:", sqlite3_errmsg(p->conf.sql));
				}

				webdav_lockdiscovery(srv, con, p->tmp_buf, "exclusive", "write", 0);

				con->http_status = 200;
				con->file_finished = 1;
				return HANDLER_FINISHED;
			} else {
				/* we need a lock-token to refresh */
				con->http_status = 400;

				return HANDLER_FINISHED;
			}
		}
		break;
#else
		con->http_status = 501;
		return HANDLER_FINISHED;
#endif
	case HTTP_METHOD_UNLOCK:
#ifdef USE_LOCKS
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "Lock-Token"))) {
			buffer *locktoken = ds->value;
			sqlite3_stmt *stmt = p->conf.stmt_remove_lock;

			/* remove the < > around the token */
			if (buffer_string_length(locktoken) < 3) {
				con->http_status = 400;

				return HANDLER_FINISHED;
			}

			/**
			 * FIXME:
			 *
			 * if the resourse is locked:
			 * - by us: unlock
			 * - by someone else: 401
			 * if the resource is not locked:
			 * - 412
			 *  */

			buffer_copy_string_len(p->tmp_buf, locktoken->ptr + 1, buffer_string_length(locktoken) - 2);

			sqlite3_reset(stmt);

			sqlite3_bind_text(stmt, 1,
				CONST_BUF_LEN(p->tmp_buf),
				SQLITE_TRANSIENT);

			sqlite3_bind_text(stmt, 2,
				CONST_BUF_LEN(con->uri.path),
				SQLITE_TRANSIENT);

			if (SQLITE_DONE != sqlite3_step(stmt)) {
				log_error_write(srv, __FILE__, __LINE__, "ss",
					"remove lock:", sqlite3_errmsg(p->conf.sql));
			}

			if (0 == sqlite3_changes(p->conf.sql)) {
				con->http_status = 401;
			} else {
				con->http_status = 204;
			}
			return HANDLER_FINISHED;
		} else {
			/* we need a lock-token to unlock */
			con->http_status = 400;

			return HANDLER_FINISHED;
		}
		break;
#else
		con->http_status = 501;
		return HANDLER_FINISHED;
#endif

	case HTTP_METHOD_WOL:{	
		con->http_status = 200;
		return HANDLER_FINISHED;
	}

	case HTTP_METHOD_GSL:{
#if EMBEDDED_EANBLE			
		if(!con->srv_socket->is_ssl){
			con->http_status = 403;
			return HANDLER_FINISHED;
		}
#endif
		buffer *buffer_url = NULL;
		buffer *buffer_filename = NULL;
		buffer *buffer_result_share_link = NULL;
		int expire = 1;
		int toShare = 1;
		Cdbg(DBE, "do HTTP_METHOD_GSL....................");
		
		if (p->conf.is_readonly) {
			con->http_status = 403;
			return HANDLER_FINISHED;
		}
		
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "URL"))) {
			buffer_url = ds->value;
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}
		
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "FILENAME"))) {
			buffer_filename = ds->value;
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "EXPIRE"))) {
			expire = atoi(ds->value->ptr);
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "TOSHARE"))) {
			toShare = atoi(ds->value->ptr);
		}

		int sharelink_save_count = get_sharelink_save_count();
		int file_count = 0;
		
		char* tmp_filename = strdup(buffer_filename->ptr);		
		char *pch = strtok(tmp_filename, ";");				
		while(pch!=NULL){
			file_count++;
			pch = strtok(NULL,";");
		}
		free(tmp_filename);

		if(toShare==1&&sharelink_save_count+file_count>srv->srvconf.max_sharelink){
			con->http_status = 405;
			return HANDLER_FINISHED;
		}
		
		char auth[100]="\0";		
		if(con->aidisk_username->used && con->aidisk_passwd->used)
			sprintf(auth, "%s:%s", con->aidisk_username->ptr, con->aidisk_passwd->ptr);
		else{
			con->http_status = 400;
			return HANDLER_FINISHED;
		}

		char* base64_auth = ldb_base64_encode(auth, strlen(auth));

		if( generate_sharelink(srv, 
			                   con, 
			                   buffer_filename->ptr, 
			                   buffer_url->ptr, 
			                   base64_auth, 
			                   expire, 
			                   toShare, 
			                   &buffer_result_share_link) == 0){
			free(base64_auth);
			con->http_status = 400;
			return HANDLER_FINISHED;
		}
		
		con->http_status = 200;

		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/xml; charset=\"utf-8\""));

		b = buffer_init();

		buffer_copy_string_len(b, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));
		buffer_append_string_len(b,CONST_STR_LEN("<result>"));
		buffer_append_string_len(b,CONST_STR_LEN("<sharelink>"));
		buffer_append_string_buffer(b,buffer_result_share_link);
		buffer_append_string_len(b,CONST_STR_LEN("</sharelink>"));
		buffer_append_string_len(b,CONST_STR_LEN("</result>"));

		chunkqueue_append_buffer(con->write_queue, b);
		buffer_free(b);
		
		con->file_finished = 1;

		buffer_free(buffer_result_share_link);
		free(base64_auth);

		if(toShare==1)
			save_sharelink_list(srv);

		return HANDLER_FINISHED;
	}
	
	case HTTP_METHOD_GETSRVTIME:{
#if EMBEDDED_EANBLE			
		if(!con->srv_socket->is_ssl){
			con->http_status = 403;
			return HANDLER_FINISHED;
		}
#endif

		char stime[1024]="\0";
		time_t server_time = time(NULL);
		sprintf(stime, "%ld", server_time);
		Cdbg(DBE, "do HTTP_METHOD_GETSRVTIME....................%s", stime);
		
		con->http_status = 200;
		
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/xml; charset=\"utf-8\""));
		
		b = buffer_init();
		
		buffer_copy_string_len(b, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"));
		buffer_append_string_len(b,CONST_STR_LEN("<result>\n"));
		buffer_append_string_len(b,CONST_STR_LEN("<servertime>\n"));
		buffer_append_string(b,stime);
		buffer_append_string_len(b,CONST_STR_LEN("</servertime>\n"));
		buffer_append_string_len(b,CONST_STR_LEN("</result>\n"));

		chunkqueue_append_buffer(con->write_queue, b);
		buffer_free(b);
		
		con->file_finished = 1;
		return HANDLER_FINISHED;
	}

	case HTTP_METHOD_GETROUTERMAC:{

#if EMBEDDED_EANBLE
		char* router_mac = nvram_get_router_mac();
#else
		char router_mac[20]="\0";
		get_mac_address("eth0", &router_mac); 
#endif

		con->http_status = 200;
		
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/xml; charset=\"utf-8\""));
		
		b = buffer_init();
		
		buffer_copy_string_len(b, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"));
		buffer_append_string_len(b,CONST_STR_LEN("<result>\n"));
		buffer_append_string_len(b,CONST_STR_LEN("<mac>\n"));
		buffer_append_string(b,router_mac);
		buffer_append_string_len(b,CONST_STR_LEN("</mac>\n"));
		buffer_append_string_len(b,CONST_STR_LEN("</result>\n"));

		chunkqueue_append_buffer(con->write_queue, b);
		buffer_free(b);
		
		con->file_finished = 1;
#if EMBEDDED_EANBLE
#ifdef APP_IPKG
		free(router_mac);
#endif
#endif
		return HANDLER_FINISHED;
	}
	
	case HTTP_METHOD_GETFIRMVER:{

#if EMBEDDED_EANBLE			
		if(!con->srv_socket->is_ssl){
			con->http_status = 403;
			return HANDLER_FINISHED;
		}
#endif

#if EMBEDDED_EANBLE
		char* firmware_version = nvram_get_firmware_version();
		char* build_no = nvram_get_build_no();
#else
		char* firmware_version = "1.0.0";
		char* build_no = "0";
#endif
			
		con->http_status = 200;
		
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/xml; charset=\"utf-8\""));
		
		b = buffer_init();
		
		buffer_copy_string_len(b, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"));
		buffer_append_string_len(b,CONST_STR_LEN("<result>\n"));
		buffer_append_string_len(b,CONST_STR_LEN("<version>\n"));
		buffer_append_string(b,firmware_version);
		buffer_append_string(b,".");
		buffer_append_string(b,build_no);
		buffer_append_string_len(b,CONST_STR_LEN("</version>\n"));
		buffer_append_string_len(b,CONST_STR_LEN("</result>\n"));

		chunkqueue_append_buffer(con->write_queue, b);
		buffer_free(b);
		
		con->file_finished = 1;
#if EMBEDDED_EANBLE
#ifdef APP_IPKG
        free(firmware_version);
        free(build_no);
#endif
#endif
		return HANDLER_FINISHED;
	}

#if 0	
	case HTTP_METHOD_GETROUTERINFO:{
#if EMBEDDED_EANBLE			
		if(!con->srv_socket->is_ssl){
			con->http_status = 403;
			return HANDLER_FINISHED;
		}
#endif

		Cdbg(DBE, "do HTTP_METHOD_GETROUTERINFO....................");
		
		char stime[1024]="\0";
		time_t server_time = time(NULL);
		sprintf(stime, "%ld", server_time);
		
#if EMBEDDED_EANBLE
		char* router_mac = nvram_get_router_mac();
		
		char* firmware_version = nvram_get_firmware_version();
		char* build_no = nvram_get_build_no();

		char* modal_name = nvram_get_productid();
				
		//- Computer Name		
		char* computer_name = nvram_get_computer_name();
		char* st_webdav_mode = nvram_get_st_webdav_mode();
		char* webdav_http_port = nvram_get_webdav_http_port();
		char* webdav_https_port = nvram_get_webdav_https_port();
		char* http_enable = nvram_get_http_enable();
		char* lan_http_port = "80";
		char* lan_https_port = nvram_get_lan_https_port();
		char* misc_http_x = nvram_get_misc_http_x();
		char* misc_http_port = nvram_get_misc_http_port();
		char* misc_https_port = nvram_get_misc_https_port();
		char* ddns_host_name = nvram_get_ddns_host_name();
		char* disk_path = "/mnt/";
		char* wan_ip = nvram_get_wan_ip();
		char *usbdiskname = nvram_get_productid();
		
		//- Get aicloud version
		#ifdef APP_IPKG
		char* aicloud_version_file = "/opt/lib/ipkg/info/aicloud.control";
		char* aicloud_app_type = "install";
		char* smartsync_version_file = "/opt/lib/ipkg/info/smartsync.control";
		#else
		char* aicloud_version_file = "/usr/lighttpd/control";
		char* aicloud_app_type = "embed";
		char* smartsync_version_file = "/usr/lighttpd/smartsync_control";
		#endif
		char aicloud_version[30]="\0";
		char smartsync_version[30]="\0";
		char *swpjverno = nvram_get_swpjverno();
		char *extendno = nvram_get_extendno();

		char *https_crt_cn = nvram_get_https_crt_cn();

		char *apps_sq = nvram_get_apps_sq();
#ifdef USE_TCAPI
		char *is_dsl_platform = "1";
#else
		char *is_dsl_platform = "0";
#endif

#else
		char router_mac[20]="\0";
		get_mac_address("eth0", &router_mac); 

		char* firmware_version = "1.0.0";
		char* build_no = "0";

		char* modal_name = "WebDAV";
		
		//- Computer Name
		char* computer_name = "WebDAV";
		char* st_webdav_mode = "0";
		char* webdav_http_port = "8082";
		char* webdav_https_port = "443";
		char* http_enable = "2";
		char* lan_http_port = "80";
		char* lan_https_port = "8443";
		char* misc_http_x = "0";
		char* misc_http_port = "8080";
		char* misc_https_port = "8443";
		char* ddns_host_name = "";
		char* disk_path = "/mnt/";
		char* wan_ip = "192.168.1.10";
		char *usbdiskname = "usbdisk";
						
		//- Get aicloud version
		char* aicloud_version_file = "/usr/css/control";
		char* smartsync_version_file = "/usr/css/smartsync_control";
		char aicloud_version[30]="\0";
		char smartsync_version[30]="\0";
		char *swpjverno = "";
		char *extendno = "";
		char* aicloud_app_type = "embed";

		char *https_crt_cn = "192.168.1.1";

		char *apps_sq = "0";
		char *is_dsl_platform = "0";
#endif
		int sharelink_save_count = get_sharelink_save_count();
		int dms_enable = is_dms_enabled();
		int jffs_supported = is_jffs_supported();

		if(buffer_is_empty(srv->srvconf.aicloud_version)){
			//- Parser version file
			FILE* fp2;
			char line[128];
			if((fp2 = fopen(aicloud_version_file, "r")) != NULL){
				memset(line, 0, sizeof(line));
				while(fgets(line, 128, fp2) != NULL){
					if(strncmp(line, "Version:", 8)==0){
						strncpy(aicloud_version, line + 9, strlen(line)-8);
					}
				}
				fclose(fp2);
			}
		}
		else{
			strcpy(aicloud_version, srv->srvconf.aicloud_version->ptr);
		}

		if(buffer_is_empty(srv->srvconf.smartsync_version)){
			//- Parser version file
			FILE* fp2;
			char line[128];
			if((fp2 = fopen(smartsync_version_file, "r")) != NULL){
				memset(line, 0, sizeof(line));
				while(fgets(line, 128, fp2) != NULL){
					if(strncmp(line, "Version:", 8)==0){
						strncpy(smartsync_version, line + 9, strlen(line)-8);
					}
				}
				fclose(fp2);
			}
		}
		else{
			strcpy(smartsync_version, srv->srvconf.smartsync_version->ptr);
		}
		
#ifndef APP_IPKG
		if( swpjverno!=NULL && strncmp(swpjverno,"", 1)!=0){
			strcpy(aicloud_version, swpjverno);
			if(extendno!=NULL && strncmp(extendno,"", 1)!=0)
			{
				strcat(aicloud_version, "_");
				strcat(aicloud_version, extendno);
			}
		}
#endif
		con->http_status = 200;
		
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/xml; charset=\"utf-8\""));
		
		b = buffer_init();
		
		buffer_copy_string_len(b, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));
		buffer_append_string_len(b,CONST_STR_LEN("<result>"));
		buffer_append_string_len(b,CONST_STR_LEN("<servertime>"));
		buffer_append_string(b,stime);
		buffer_append_string_len(b,CONST_STR_LEN("</servertime>"));
		buffer_append_string_len(b,CONST_STR_LEN("<mac>"));
		buffer_append_string(b,router_mac);
		buffer_append_string_len(b,CONST_STR_LEN("</mac>"));
		buffer_append_string_len(b,CONST_STR_LEN("<version>"));
		buffer_append_string(b,firmware_version);
		buffer_append_string(b,".");
		buffer_append_string(b,build_no);
		buffer_append_string_len(b,CONST_STR_LEN("</version>"));
		buffer_append_string_len(b,CONST_STR_LEN("<aicloud_version>"));
		buffer_append_string(b,aicloud_version);
		buffer_append_string_len(b,CONST_STR_LEN("</aicloud_version>"));
		buffer_append_string_len(b,CONST_STR_LEN("<extendno>"));
		if(extendno!=NULL && strncmp(extendno,"", 1)!=0) buffer_append_string(b,extendno);
		buffer_append_string_len(b,CONST_STR_LEN("</extendno>"));
		buffer_append_string_len(b,CONST_STR_LEN("<aicloud_app_type>"));		
		buffer_append_string(b,aicloud_app_type);		
		buffer_append_string_len(b,CONST_STR_LEN("</aicloud_app_type>"));
		buffer_append_string_len(b,CONST_STR_LEN("<smartsync_version>"));
		buffer_append_string(b,smartsync_version);
		buffer_append_string_len(b,CONST_STR_LEN("</smartsync_version>"));
		buffer_append_string_len(b,CONST_STR_LEN("<modalname>"));
		buffer_append_string(b,modal_name);
		buffer_append_string_len(b,CONST_STR_LEN("</modalname>"));
		buffer_append_string_len(b,CONST_STR_LEN("<computername>"));
		buffer_append_string(b,computer_name);
		buffer_append_string_len(b,CONST_STR_LEN("</computername>"));
		buffer_append_string_len(b,CONST_STR_LEN("<usbdiskname>"));
		buffer_append_string(b,usbdiskname);
		buffer_append_string_len(b,CONST_STR_LEN("</usbdiskname>"));
		buffer_append_string_len(b,CONST_STR_LEN("<webdav_mode>"));
		buffer_append_string(b,st_webdav_mode);
		buffer_append_string_len(b,CONST_STR_LEN("</webdav_mode>"));
		buffer_append_string_len(b,CONST_STR_LEN("<http_port>"));
		buffer_append_string(b,webdav_http_port);
		buffer_append_string_len(b,CONST_STR_LEN("</http_port>"));
		buffer_append_string_len(b,CONST_STR_LEN("<https_port>"));
		buffer_append_string(b,webdav_https_port);
		buffer_append_string_len(b,CONST_STR_LEN("</https_port>"));
		buffer_append_string_len(b,CONST_STR_LEN("<http_enable>"));
		buffer_append_string(b,http_enable);
		buffer_append_string_len(b,CONST_STR_LEN("</http_enable>"));
		buffer_append_string_len(b,CONST_STR_LEN("<lan_http_port>"));
		buffer_append_string(b,lan_http_port);
		buffer_append_string_len(b,CONST_STR_LEN("</lan_http_port>"));
		buffer_append_string_len(b,CONST_STR_LEN("<lan_https_port>"));
		buffer_append_string(b,lan_https_port);
		buffer_append_string_len(b,CONST_STR_LEN("</lan_https_port>"));
		buffer_append_string_len(b,CONST_STR_LEN("<misc_http_enable>"));
		buffer_append_string(b,misc_http_x);
		buffer_append_string_len(b,CONST_STR_LEN("</misc_http_enable>"));
		buffer_append_string_len(b,CONST_STR_LEN("<misc_http_port>"));
		buffer_append_string(b,misc_http_port);
		buffer_append_string_len(b,CONST_STR_LEN("</misc_http_port>"));
		buffer_append_string_len(b,CONST_STR_LEN("<misc_https_port>"));
		buffer_append_string(b,misc_https_port);
		buffer_append_string_len(b,CONST_STR_LEN("</misc_https_port>"));
		buffer_append_string_len(b,CONST_STR_LEN("<last_login_info>"));
		if(buffer_is_empty(srv->last_login_info))
			buffer_append_string(b,"");
		else
			buffer_append_string(b,srv->last_login_info->ptr);
		buffer_append_string_len(b,CONST_STR_LEN("</last_login_info>"));
		buffer_append_string_len(b,CONST_STR_LEN("<ddns_host_name>"));
		buffer_append_string(b,ddns_host_name);
		buffer_append_string_len(b,CONST_STR_LEN("</ddns_host_name>"));
		buffer_append_string_len(b,CONST_STR_LEN("<wan_ip>"));
		buffer_append_string(b,wan_ip);
		buffer_append_string_len(b,CONST_STR_LEN("</wan_ip>"));
		buffer_append_string_len(b,CONST_STR_LEN("<dms_enable>"));
		buffer_append_string(b, ((dms_enable==1) ? "1" : "0"));
		buffer_append_string_len(b,CONST_STR_LEN("</dms_enable>"));
		buffer_append_string_len(b,CONST_STR_LEN("<account_manager_enable>"));		
		buffer_append_string(b, "0");
		buffer_append_string_len(b,CONST_STR_LEN("</account_manager_enable>"));
		buffer_append_string_len(b,CONST_STR_LEN("<https_crt_cn>"));
		buffer_append_string(b,https_crt_cn);
		buffer_append_string_len(b,CONST_STR_LEN("</https_crt_cn>"));
		buffer_append_string_len(b,CONST_STR_LEN("<app_installation_url>"));
		buffer_append_string(b, buffer_is_empty(srv->srvconf.app_installation_url) ? "" : srv->srvconf.app_installation_url->ptr);
		buffer_append_string_len(b,CONST_STR_LEN("</app_installation_url>"));	
		buffer_append_string_len(b,CONST_STR_LEN("<apps_sq>"));
		buffer_append_string(b,apps_sq);
		buffer_append_string_len(b,CONST_STR_LEN("</apps_sq>"));
		buffer_append_string_len(b,CONST_STR_LEN("<is_dsl_platform>"));
		buffer_append_string(b,is_dsl_platform);
		buffer_append_string_len(b,CONST_STR_LEN("</is_dsl_platform>"));
		buffer_append_string_len(b,CONST_STR_LEN("<used_sharelink>"));
		char used_sharelink[5] = "\0";
		sprintf(used_sharelink, "%d", sharelink_save_count);
		buffer_append_string(b, used_sharelink);
		buffer_append_string_len(b,CONST_STR_LEN("</used_sharelink>"));	
		buffer_append_string_len(b,CONST_STR_LEN("<max_sharelink>"));
		char max_sharelink[5] = "\0";
		sprintf(max_sharelink, "%d", srv->srvconf.max_sharelink);
		buffer_append_string(b, max_sharelink);
		buffer_append_string_len(b,CONST_STR_LEN("</max_sharelink>"));
		
		//DIR *dir;
		if (NULL != (dir = opendir(disk_path))) {

			buffer_append_string_len(b,CONST_STR_LEN("<disk_space>"));
			
			struct dirent *de;

			while(NULL != (de = readdir(dir))) {

				if ( de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0' ) {
					continue;
					//- ignore the parent dir
				}

				if ( de->d_name[0] == '.' ) {
					continue;
					//- ignore the hidden file
				}

				buffer_append_string_len(b,CONST_STR_LEN("<item>"));
					
				char querycmd[100] = "\0";		

				sprintf(querycmd, "df|grep -i '%s%s'", disk_path, de->d_name);
								
				buffer_append_string_len(b,CONST_STR_LEN("<DiskName>"));
				buffer_append_string(b,de->d_name);
				buffer_append_string_len(b,CONST_STR_LEN("</DiskName>"));
						
				char mybuffer[BUFSIZ]="\0";
				FILE* fp = popen( querycmd, "r");
				if(fp){
					int len = fread(mybuffer, sizeof(char), BUFSIZ, fp);
					mybuffer[len-1]="\0";
					pclose(fp);
					
					char * pch;
					pch = strtok(mybuffer, " ");
					int count=1;
					while(pch!=NULL){				
						if(count==3){
							buffer_append_string_len(b,CONST_STR_LEN("<DiskUsed>"));
							buffer_append_string(b,pch);
							buffer_append_string_len(b,CONST_STR_LEN("</DiskUsed>"));
						}
						else if(count==4){
							buffer_append_string_len(b,CONST_STR_LEN("<DiskAvailable>"));
							buffer_append_string(b,pch);
							buffer_append_string_len(b,CONST_STR_LEN("</DiskAvailable>"));
						}
						else if(count==5){
							buffer_append_string_len(b,CONST_STR_LEN("<DiskUsedPercent>"));
							buffer_append_string(b,pch);
							buffer_append_string_len(b,CONST_STR_LEN("</DiskUsedPercent>"));
						}
						
						//- Next
						pch = strtok(NULL," ");
						count++;
					}
					
				}
				
				buffer_append_string_len(b,CONST_STR_LEN("</item>"));
			}

			buffer_append_string_len(b,CONST_STR_LEN("</disk_space>"));

			closedir(dir);
		}
		
		buffer_append_string_len(b,CONST_STR_LEN("</result>"));

		chunkqueue_append_buffer(con->write_queue, b);
		buffer_free(b);
		
		con->file_finished = 1;
#if EMBEDDED_EANBLE
#ifdef APP_IPKG
        free(router_mac);
        free(firmware_version);
        free(build_no);
        free(modal_name);
        free(computer_name);
        free(st_webdav_mode);
        free(webdav_http_port);
        free(webdav_https_port);
        free(misc_http_x);
        free(misc_http_port);
		free(ddns_host_name);
		free(wan_ip);
		free(http_enable);
		free(misc_https_port);
		if(swpjverno!=NULL)
			free(swpjverno);
		if(extendno!=NULL)
			free(extendno);
		free(https_crt_cn);
#endif
#endif
		return HANDLER_FINISHED;
	}
#endif

	case HTTP_METHOD_RESCANSMBPC:{
#if EMBEDDED_EANBLE			
		if(!con->srv_socket->is_ssl){
			con->http_status = 403;
			return HANDLER_FINISHED;
		}
#endif
		Cdbg(DBE, "do HTTP_METHOD_RESCANSMBPC");
		
		stop_arpping_process();

		#if EMBEDDED_EANBLE
		nvram_set_smbdav_str("");			
		#else
		unlink("/tmp/arpping_list");
		#endif
			
		con->http_status = 200;
		con->file_finished = 1;
		return HANDLER_FINISHED;
	}

	case HTTP_METHOD_PROPFINDMEDIALIST:{
#if EMBEDDED_EANBLE			
		if(!con->srv_socket->is_ssl){
			con->http_status = 403;
			return HANDLER_FINISHED;
		}
#endif

		int dms_enable = is_dms_enabled();

		if(dms_enable == 0){
			//- Service Not Available
			con->http_status = 503;
			return HANDLER_FINISHED;
		}
		
		/* they want to know the properties of the directory */
		req_props = NULL;
				
		/* is there a content-body ? */ 	
		switch (stat_cache_get_entry(srv, con, con->physical.path, &sce)) {
			case HANDLER_ERROR:
				if (errno == ENOENT) {
					con->http_status = 404;
					return HANDLER_FINISHED;
				}
				break;
			default:
				break;
		}
		
#ifdef USE_PROPPATCH
		/* any special requests or just allprop ? */
		if (con->request.content_length) {
			xmlDocPtr xml;
			
			if (1 == webdav_parse_chunkqueue(srv, con, p, con->request_content_queue, &xml)) {
				xmlNode *rootnode = xmlDocGetRootElement(xml);
		
				assert(rootnode);
				
				if (0 == xmlStrcmp(rootnode->name, BAD_CAST "propfind")) {
					xmlNode *cmd;
		
					req_props = calloc(1, sizeof(*req_props));
		
					for (cmd = rootnode->children; cmd; cmd = cmd->next) {
		
						if (0 == xmlStrcmp(cmd->name, BAD_CAST "prop")) {
							/* get prop by name */
							xmlNode *prop;
							for (prop = cmd->children; prop; prop = prop->next) {
								if (prop->type == XML_TEXT_NODE) continue; /* ignore WS */								
								if (prop->ns &&
									(0 == xmlStrcmp(prop->ns->href, BAD_CAST "")) &&
									(0 != xmlStrcmp(prop->ns->prefix, BAD_CAST ""))) {
									size_t i;
									log_error_write(srv, __FILE__, __LINE__, "ss",
											"no name space for:",
											prop->name);
									
									xmlFreeDoc(xml);
		
									for (i = 0; i < req_props->used; i++) {
										free(req_props->ptr[i]->ns);
										free(req_props->ptr[i]->prop);
										free(req_props->ptr[i]);
									}
									free(req_props->ptr);
									free(req_props);									
									con->http_status = 400;
									return HANDLER_FINISHED;
								}
		
								/* add property to requested list */
								if (req_props->size == 0) {
									req_props->size = 16;
									req_props->ptr = malloc(sizeof(*(req_props->ptr)) * req_props->size);
								} else if (req_props->used == req_props->size) {
									req_props->size += 16;
									req_props->ptr = realloc(req_props->ptr, sizeof(*(req_props->ptr)) * req_props->size);
								}
		
								req_props->ptr[req_props->used] = malloc(sizeof(webdav_property));
								req_props->ptr[req_props->used]->ns = (char *)xmlStrdup(prop->ns ? prop->ns->href : (xmlChar *)"");
								req_props->ptr[req_props->used]->prop = (char *)xmlStrdup(prop->name);
								req_props->used++;
							}
						} else if (0 == xmlStrcmp(cmd->name, BAD_CAST "propname")) {
							sqlite3_stmt *stmt = p->conf.stmt_select_propnames;
		
							if (stmt) {
								/* get all property names (EMPTY) */
								sqlite3_reset(stmt);
								/* bind the values to the insert */
		
								sqlite3_bind_text(stmt, 1,
												  con->uri.path->ptr,
												  con->uri.path->used - 1,
												  SQLITE_TRANSIENT);
		
								if (SQLITE_DONE != sqlite3_step(stmt)) {
								}
							}
						} else if (0 == xmlStrcmp(cmd->name, BAD_CAST "allprop")) {
							/* get all properties (EMPTY) */
							req_props = NULL;
						}
					}
				}
		
				xmlFreeDoc(xml);
			} else {
				con->http_status = 400;
				return HANDLER_FINISHED;
			}
		}
#endif

#ifdef USE_MINIDLNA_DB
		Cdbg(DBE, "do HTTP_METHOD_PROPFINDMEDIALIST");
		
		sqlite3 *sql_minidlna = NULL;
		buffer* media_type = NULL;  //- 0: All(default), 1: Image, 2:Audio, 3:Video
		buffer* start = NULL;       //- start query index
		buffer* end = NULL;         //- end query index
		buffer* keyword = NULL;     //- query keyword
		buffer* orderby = NULL;     //- query sortby
		buffer* orderrule = NULL;   //- sort rule: DESC, ASC
		buffer* parentid = NULL;   //- parentid
		
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "MediaType"))) {
		 	media_type = ds->value;
		}
		else{
			Cdbg(DBE, "No value 'MediaType' specified!");
			con->http_status = 207;
			con->file_finished = 1;
			return HANDLER_FINISHED;	
		}
			
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "Start"))) {
			start = ds->value;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "End"))) {
			end = ds->value;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "Keyword"))) {
			keyword = ds->value;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "Orderby"))) {
			orderby = ds->value;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "Orderrule"))) {
			orderrule = ds->value;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "Parentid"))) {
			parentid = ds->value;
		}

		get_minidlna_db_path(p);
		
		if (!buffer_is_empty(p->minidlna_db_file)) {			
			if (SQLITE_OK != sqlite3_open(p->minidlna_db_file->ptr, &(sql_minidlna))) {				
				Cdbg(DBE, "Fail to open minidlna db %s", p->minidlna_db_file->ptr);
			}
		}

		if (!sql_minidlna) {
			Cdbg(DBE, "NULL value 'sql_minidlna'!");
			con->http_status = 207;
			con->file_finished = 1;
			return HANDLER_FINISHED;	
		}
		
		int rows, i;
		char **result;
		sqlite_int64 plID;
		char *plpath, *plname, *thumbnail, *resolution;		
		char sql_query[2048] = "\0";
		
		int column_count = 7;
		sprintf(sql_query, "SELECT d.ID as ID, "
		                   "d.PATH as PATH, "
		                   "d.TITLE as TITLE, "
		                   "d.SIZE as SIZE, "
		                   "d.TIMESTAMP as TIMESTAMP, "
		                   "d.THUMBNAIL as THUMBNAIL, "
		                   "d.RESOLUTION as RESOLUTION "
			               "from DETAILS d ");
		
		if(!buffer_is_empty(parentid)){
			sprintf(sql_query, "%s left join OBJECTS o on (o.DETAIL_ID = d.ID)", sql_query);
		}
		
		if( buffer_is_equal_string(media_type, CONST_STR_LEN("1")) ){
			sprintf(sql_query, "%s where d.MIME glob 'i*'", sql_query);			
		}
		else if( buffer_is_equal_string(media_type, CONST_STR_LEN("2")) ){
			sprintf(sql_query, "%s where d.MIME glob 'a*'", sql_query);
		}
		else if( buffer_is_equal_string(media_type, CONST_STR_LEN("3")) ){
			sprintf(sql_query, "%s where d.MIME glob 'v*'", sql_query);
		}
		else if( buffer_is_equal_string(media_type, CONST_STR_LEN("0")) ){
			sprintf(sql_query, "%s where d.MIME glob 'i*' or d.MIME glob 'a*' or d.MIME glob 'v*'", sql_query);
		}
	
		if(!buffer_is_empty(keyword)){			
			buffer_urldecode_path(keyword);

			if(strstr(keyword->ptr, "*")||strstr(keyword->ptr, "?")){
				char buff[4096];
				replace_str(keyword->ptr, "*", "%", buff);
				replace_str(buff, "?", "_", buff);
				sprintf(sql_query, "%s and ( PATH LIKE '%s' or TITLE LIKE '%s' )", sql_query, buff, buff);
			}
			else
				sprintf(sql_query, "%s and ( PATH LIKE '%s%s%s' or TITLE LIKE '%s%s%s' )", sql_query, "%", keyword->ptr, "%", "%", keyword->ptr, "%");
		}
		
		if(!buffer_is_empty(parentid)){
			sprintf(sql_query, "%s and o.PARENT_ID='%s'", sql_query, parentid->ptr );			
		}
				
#if EMBEDDED_EANBLE
		//- Get mount partition
		char* disk_path = "/mnt/";
		//DIR *dir;
		if (NULL != (dir = opendir(disk_path))) {
			struct dirent *de;
			while(NULL != (de = readdir(dir))) {
				if ( de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0' ) {
					continue;
					//- ignore the parent dir
				}

				if ( de->d_name[0] == '.' ) {
					continue;
					//- ignore the hidden file
				}
				
				char partion_path[100] = "\0";
				sprintf(partion_path, "%s%s", disk_path, de->d_name);
				
				//if(strstr( con->physical.path->ptr, partion_path ) && buffer_is_empty(parentid)){
				if(strstr( con->physical.path->ptr, partion_path )){
					sprintf(sql_query, "%s and PATH LIKE '%s%s/%s'", sql_query, "%", partion_path, "%");
					break;
				}
			}
			closedir(dir);
		}
#endif
		
		Cdbg(DBE, "1. sql_query=%s", sql_query);
		if( sql_get_table(sql_minidlna, sql_query, &result, &rows, NULL) != SQLITE_OK ){
			sqlite3_close(sql_minidlna);
			con->http_status = 207;
			con->file_finished = 1;
			Cdbg(DBE, "Fail to sql_get_table");
			return HANDLER_FINISHED;
		}
		#if 0
		if( !rows ){
			sqlite3_free_table(result);
			sqlite3_close(sql_minidlna);
			con->http_status = 207;
			con->file_finished = 1;
			Cdbg(DBE, "rows = 0!");
			return HANDLER_FINISHED;
		}
		#endif
		////////////////////////////////////////////////////////////////////////////////////
		
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/xml; charset=\"utf-8\""));

		b = buffer_init();

		buffer_copy_string_len(b, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"));
		buffer_append_string_len(b,CONST_STR_LEN("<D:multistatus xmlns:D=\"DAV:\" xmlns:ns0=\"urn:uuid:c2f41010-65b3-11d1-a29f-00aa00c14882/\" "));

		//- Total count
		buffer_append_string_len(b, CONST_STR_LEN(" qcount=\""));
		char qcount[1024]="\0";
		sprintf(qcount, "%d", rows);
		buffer_append_string(b, qcount);
		buffer_append_string_len(b, CONST_STR_LEN("\""));

		buffer_append_string_len(b, CONST_STR_LEN(" scan_status=\""));
#if EMBEDDED_EANBLE
		//- Get scan status
		char dms_scanfile[64];
    	FILE *fp;

		if (!buffer_is_empty(p->minidlna_db_dir)) {	    
	        sprintf(dms_scanfile, "%s/scantag", p->minidlna_db_dir->ptr);

	        fp = fopen(dms_scanfile, "r");

	        if(fp) {
				buffer_append_string(b, "Scanning");
	            fclose(fp);
	        }
			else
				buffer_append_string(b, "Idle");
	    }
#else
		buffer_append_string(b, "Scanning");
#endif
		buffer_append_string_len(b, CONST_STR_LEN("\""));

		int query_again = 0;		
		if(!buffer_is_empty(orderby)){
			if(!buffer_is_empty(orderrule))
				sprintf(sql_query, "%s ORDER BY %s %s", sql_query, orderby->ptr, orderrule->ptr);
			else
				sprintf(sql_query, "%s ORDER BY %s ASC", sql_query, orderby->ptr);
			
			query_again = 1;
		}
		
		if(!buffer_is_empty(start) && !buffer_is_empty(end)){
			sprintf(sql_query, "%s LIMIT %s, %s", sql_query, start->ptr, end->ptr);
			query_again = 1;

			//- Start Index
			buffer_append_string_len(b, CONST_STR_LEN(" qstart=\""));			
			buffer_append_string_buffer(b, start);
			buffer_append_string_len(b, CONST_STR_LEN("\""));

			//- End Index
			int query_end = atoi(end->ptr);
			if( query_end > rows) query_end = rows;
			buffer_append_string_len(b, CONST_STR_LEN(" qend=\""));	
			char qend[1024]="\0";
			sprintf(qend, "%d", query_end);
			buffer_append_string(b, qend);
			buffer_append_string_len(b, CONST_STR_LEN("\""));
		}
		
		Cdbg(DBE, "sql_query=%s, qcount=%s", sql_query, qcount);
			
		buffer_append_string_len(b,CONST_STR_LEN(">\n"));
		
		if(query_again==1){
			sqlite3_free_table(result);			
			if( sql_get_table(sql_minidlna, sql_query, &result, &rows, NULL) != SQLITE_OK ){
				sqlite3_close(sql_minidlna);
				con->http_status = 207;
				con->file_finished = 1;
				Cdbg(DBE, "Fail to sql_get_table 2");
				return HANDLER_FINISHED;
			}
		}

		Cdbg(DBE, "rows=%d", rows);
		
		rows++;
		
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
#else
		usbdisk_name = (char*)malloc(8);
		memset(usbdisk_name,'\0', 8);
		strcpy(usbdisk_name, "usbdisk");
#endif

		/* allprop */
		prop_200 = buffer_init();
		prop_404 = buffer_init();
		physical d;					
		d.path = buffer_init();
		d.rel_path = buffer_init();
		
		for( i=column_count; i<rows*column_count; i+=column_count ){

			//- ID, PATH, TITLE, SIZE, TIMESTAMP, THUMBNAIL
			plID = strtoll(result[i], NULL, 10);
			plpath = result[i+1];
			plname = result[i+2];
			thumbnail = result[i+5];
			resolution = result[i+6];
			
			if(!plpath)
				continue;

			int thumb = atoi(thumbnail);
			
			if(thumb){
				//Cdbg(DBE, "plpath=%s, thumbnail = %d, resolution = %s", plpath, atoi(thumbnail), resolution);

				#ifdef USE_LIBEXIF
				ExifLoader *l = exif_loader_new();
				exif_loader_write_file(l, plpath);
				ExifData *ed = exif_loader_get_data(l);
				exif_loader_unref(l);

				if( ed ){
					//Cdbg(DBE, "success");

					#if 0
					/* Make sure the image had a thumbnail before trying to write it */
		            if (ed->data && ed->size) {
		                FILE *thumb;
		                char thumb_name[1024];

		                /* Try to create a unique name for the thumbnail file */
		                snprintf(thumb_name, sizeof(thumb_name),
		                         "%s_thumb.jpg", plpath);

		                thumb = fopen(thumb_name, "wb");
		                if (thumb) {
		                    /* Write the thumbnail image to the file */
		                    fwrite(ed->data, 1, ed->size, thumb);
		                    fclose(thumb);
		                    Cdbg(DBE, "Wrote thumbnail to %s\n", thumb_name);
		                } else {
		                    Cdbg(DBE, "Could not create file %s\n", thumb_name);
		                }
		            } else {
		                Cdbg(DBE, "No EXIF thumbnail in file\n");
		            }
					#endif
					
		            /* Free the EXIF data */
		            exif_data_unref(ed);
				}
				#endif
			}
			
			char buff[4096];
			#if EMBEDDED_EANBLE
			replace_str(&plpath[0], "tmp/mnt", usbdisk_name, buff);
			#else
			replace_str(&plpath[0], "mnt", usbdisk_name, buff);
			#endif

			//Cdbg(DBE, "tmp=%s, con->url.path=%s", tmp, con->url.path->ptr);
			
			buffer_copy_string(d.path, plpath);
			buffer_copy_string(d.rel_path, plpath);
						
			buffer_reset(prop_200);
			buffer_reset(prop_404);

			//Cdbg(DBE, "d.path=%s", d.path->ptr);
			//Cdbg(DBE, "d.rel_path=%s", d.rel_path->ptr);
			
			webdav_get_props(srv, con, p, &d, req_props, prop_200, prop_404);
			
			buffer_append_string_len(b,CONST_STR_LEN("<D:response>\n"));
			buffer_append_string_len(b,CONST_STR_LEN("<D:href>"));
			buffer_append_string_buffer(b, con->uri.scheme);
			buffer_append_string_len(b,CONST_STR_LEN("://"));
			buffer_append_string_buffer(b, con->uri.authority);
			buffer_append_string_encoded(b, buff, strlen(buff), ENCODING_REL_URI);						
			buffer_append_string_len(b,CONST_STR_LEN("</D:href>\n"));
			
			if (!buffer_is_empty(prop_200)) {
				buffer_append_string_len(b,CONST_STR_LEN("<D:propstat>\n"));
				buffer_append_string_len(b,CONST_STR_LEN("<D:prop>\n"));
				buffer_append_string_buffer(b, prop_200);
				buffer_append_string_len(b, CONST_STR_LEN("<D:title>"));
				buffer_append_string(b, plname);
				buffer_append_string_len(b, CONST_STR_LEN("</D:title>"));
				buffer_append_string_len(b,CONST_STR_LEN("</D:prop>\n"));
				buffer_append_string_len(b,CONST_STR_LEN("<D:status>HTTP/1.1 200 OK</D:status>\n"));
				buffer_append_string_len(b,CONST_STR_LEN("</D:propstat>\n"));
			}
			if (!buffer_is_empty(prop_404)) {
				buffer_append_string_len(b,CONST_STR_LEN("<D:propstat>\n"));
				buffer_append_string_len(b,CONST_STR_LEN("<D:prop>\n"));
				buffer_append_string_buffer(b, prop_404);

				//- for test
				buffer_append_string_len(b, CONST_STR_LEN("<D:mtitle>"));
				buffer_append_string(b, plname);
				buffer_append_string_len(b, CONST_STR_LEN("</D:mtitle>"));

				buffer_append_string_len(b,CONST_STR_LEN("</D:prop>\n"));
				buffer_append_string_len(b,CONST_STR_LEN("<D:status>HTTP/1.1 404 Not Found</D:status>\n"));
				buffer_append_string_len(b,CONST_STR_LEN("</D:propstat>\n"));
			}
				
			buffer_append_string_len(b,CONST_STR_LEN("</D:response>\n"));
			
		}
		
		buffer_free(d.path);
		buffer_free(d.rel_path);

		buffer_free(prop_200);
		buffer_free(prop_404);

		buffer_append_string_len(b,CONST_STR_LEN("</D:multistatus>\n"));

		sqlite3_free_table(result);
		/*
		//- test
		Cdbg(DBE, "------------------------------");

		//- 專輯
		column_count = 4;
		//sprintf(sql_query, "SELECT PATH, TITLE, ALBUM_ART, ARTIST from DETAILS where TITLE='- All Albums -'");				
		sprintf(sql_query, "SELECT d.PATH as PATH, "
		                   "d.TITLE as TITLE, "
		                   "d.ALBUM_ART as ALBUM_ART, "
		                   "d.ARTIST as ARTIST "
		                   "from OBJECTS o "
						   "left join DETAILS d on (o.DETAIL_ID = d.ID)"
						   " where o.CLASS = 'container.album.musicAlbum'");

		if( sql_get_table(sql_minidlna, sql_query, &result, &rows, NULL) == SQLITE_OK ){
			Cdbg(DBE, "sql_query=%s, rows=%d", sql_query, rows);
			int i=0;
			for( i=column_count; i<rows*column_count; i+=column_count ){
									
				Cdbg(DBE, "1, i=%d, PATH=%s, TITLE=%s, ALBUM_ART=%s, ARTIST=%s", 
					i, result[i], result[i+1], result[i+2], result[i+3]);
			}
					
			sqlite3_free_table(result);
		}
		
		//- 演唱者
		column_count = 4;
		//sprintf(sql_query, "SELECT PATH, TITLE, ALBUM_ART, ARTIST from DETAILS where TITLE='- All Albums -'");				
		sprintf(sql_query, "SELECT d.PATH as PATH, d.TITLE as TITLE, d.ALBUM_ART as ALBUM_ART, d.ARTIST as ARTIST from OBJECTS o "
								   "left join DETAILS d on (o.DETAIL_ID = d.ID)"
								   " where o.CLASS = 'container.person.musicArtist'");

		if( sql_get_table(sql_minidlna, sql_query, &result, &rows, NULL) == SQLITE_OK ){
			Cdbg(DBE, "sql_query=%s, rows=%d", sql_query, rows);
			int i=0;
			for( i=column_count; i<rows*column_count; i+=column_count ){
									
				Cdbg(DBE, "2, i=%d, PATH=%s, TITLE=%s, ALBUM_ART=%s, ARTIST=%s", 
					i, result[i], result[i+1], result[i+2], result[i+3]);
			}
					
			sqlite3_free_table(result);
		}
		
		column_count = 4;
		sprintf(sql_query, "SELECT PATH, TITLE, ALBUM_ART, ARTIST from DETAILS");				
		Cdbg(DBE, "sql_query=%s", sql_query);

		if( sql_get_table(sql_minidlna, sql_query, &result, &rows, NULL) == SQLITE_OK ){
					
			int i=0;
			for( i=column_count; i<rows*column_count; i+=column_count ){

						
				Cdbg(DBE, "nbbb, i=%d, PATH=%s, TITLE=%s, ALBUM_ART=%s, ARTIST=%s", 
					i, result[i], result[i+1], result[i+2], result[i+3]);
			}
					
			sqlite3_free_table(result);
		}
		
		
		column_count = 5;
		sprintf(sql_query, "SELECT NAME, CLASS, OBJECT_ID, PARENT_ID, REF_ID from OBJECTS");				
		
		if( sql_get_table(sql_minidlna, sql_query, &result, &rows, NULL) == SQLITE_OK ){
			Cdbg(DBE, "sql_query=%s, rows=%d", sql_query, rows);			
			int i=0;
			for( i=column_count; i<rows*column_count; i+=column_count ){
				Cdbg(DBE, "nccc, i=%d, NAME=%s, OBJECT_ID=%s, PARENT_ID=%s, REF_ID=%s", 
					i, result[i], result[i+2], result[i+3], result[i+4]);
			}
					
			sqlite3_free_table(result);
		}
		
		
		column_count = 1;
		sprintf(sql_query, "SELECT PATH from ALBUM_ART");
		if( sql_get_table(sql_minidlna, sql_query, &result2, &rows2, NULL) == SQLITE_OK ){
			Cdbg(DBE, "sql_query=%s, rows=%d", sql_query, rows2);
			int i=0;
			for( i=column_count; i<rows2*column_count; i+=column_count ){
				//sqlite_int64 plID = strtoll(result[i], NULL, 10);
				//char* plpath = result[i+1];			
				//Cdbg(DBE, "nddd, i=%d, ID=%lld, PATH=%s", i, plID, plpath);

				char* plpath = result2[i];
				Cdbg(DBE, "nddd, i=%d, PATH=%s", i, plpath);
			}
		}

		sqlite3_free_table(result2);
		//sqlite3_close(sql_minidlna);
		
		
		Cdbg(DBE, "------------------------------");	
		*/
		
		sqlite3_close(sql_minidlna);
#endif
		chunkqueue_append_buffer(con->write_queue, b);
		buffer_free(b);
		
		con->http_status = 207;
		con->file_finished = 1;
		return HANDLER_FINISHED;
	}

	case HTTP_METHOD_GETMUSICCLASSIFICATION:{
		Cdbg(DBE, "do HTTP_METHOD_GETMUSICCLASSIFICATION");

#if EMBEDDED_EANBLE			
		if(!con->srv_socket->is_ssl){
			con->http_status = 403;
			return HANDLER_FINISHED;
		}
#endif

		if(is_dms_enabled() == 0){
			//- Service Not Available
			con->http_status = 503;
			return HANDLER_FINISHED;
		}		
		
		buffer* classify = NULL;
		
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "Classify"))) {
			classify = ds->value;
		}
		else{
			con->http_status = 400;
			con->file_finished = 1;
			return HANDLER_FINISHED;
		}
		
#ifdef USE_MINIDLNA_DB
		sqlite3 *sql_minidlna = NULL;
		if (!buffer_is_empty(p->minidlna_db_file)) {			
			if (SQLITE_OK != sqlite3_open(p->minidlna_db_file->ptr, &(sql_minidlna))) { 			
				Cdbg(DBE, "Fail to open minidlna db %s", p->minidlna_db_file->ptr);
			}
		}
		
		if (!sql_minidlna) {
			Cdbg(DBE, "NULL value 'sql_minidlna'!");
			con->http_status = 207;
			con->file_finished = 1;
			return HANDLER_FINISHED;	
		}
		
		int column_count = 5;
		int rows, i;
		char **result;
		char sql_query[2048] = "\0";

		if(buffer_is_equal_string(classify, CONST_STR_LEN("album"))){
			sprintf(sql_query, "SELECT d.TITLE as TITLE, "
						   	   "d.ALBUM_ART as ALBUM_ART, "
						   	   "d.ARTIST as ARTIST, "
						   	   "o.PARENT_ID as PARENT_ID, "
						       "o.OBJECT_ID as OBJECT_ID "
						       "from OBJECTS o "
						       "left join DETAILS d on (o.DETAIL_ID = d.ID) "
						       "where o.CLASS = 'container.album.musicAlbum' "
						       "and o.PARENT_ID = '%s'", MUSIC_ALBUM_ID);
		}
		else if(buffer_is_equal_string(classify, CONST_STR_LEN("artist"))){
			sprintf(sql_query, "SELECT d.TITLE as TITLE, "
							   "d.ALBUM_ART as ALBUM_ART, "
							   "d.ARTIST as ARTIST, "
							   "o.PARENT_ID as PARENT_ID, "
                               "o.OBJECT_ID as OBJECT_ID "
						       "from OBJECTS o "
						       "left join DETAILS d on (o.DETAIL_ID = d.ID) "
						       "where o.CLASS = 'container.person.musicArtist'"
						       "and o.PARENT_ID = '%s'", MUSIC_ARTIST_ID);
		}
		else{
			con->http_status = 400;
			con->file_finished = 1;
			sqlite3_close(sql_minidlna);
			return HANDLER_FINISHED;
		}
		
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/xml; charset=\"utf-8\""));
				
		b = buffer_init();
				
		buffer_copy_string_len(b, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));
		buffer_append_string_len(b,CONST_STR_LEN("<result>"));
		
		Cdbg(DBE, "do HTTP_METHOD_GETMUSICCLASSIFICATION, sql_query=%s", sql_query);
				
		if( sql_get_table(sql_minidlna, sql_query, &result, &rows, NULL) == SQLITE_OK ){
			Cdbg(DBE, "sql_query=%s, rows=%d", sql_query, rows);
			int i=0;
					
			for( i=column_count; i<=rows*column_count; i+=column_count ){			
		
				char* title = result[i];
				char* album_art = result[i+1];
				char* artist = result[i+2];
				char* parent_id = result[i+3];
				char* object_id = result[i+4];

				buffer* id = buffer_init();
				buffer_copy_string(id,object_id);
				if(buffer_is_equal_string(classify, CONST_STR_LEN("artist")))
					buffer_append_string(id,"$0");
				
				Cdbg(DBE, "1, TITLE=%s, ALBUM_ART=%s, ARTIST=%s", title, album_art, artist);

#if 1
				int column_count2 = 1;
				int rows2, j;
				char **result2;
				int partion_count = 0;
				
				sprintf(sql_query, "SELECT COUNT(*) as COUNT from DETAILS d "
					"LEFT JOIN OBJECTS o on (o.DETAIL_ID = d.ID) "
					"WHERE d.MIME glob 'a*' AND o.PARENT_ID='%s' ", id->ptr);
				
#if EMBEDDED_EANBLE
				//- Get mount partition
				char* disk_path = "/mnt/";
				char partion_path[100] = "\0";
				//DIR *dir;
				if (NULL != (dir = opendir(disk_path))) {
					struct dirent *de;
					while(NULL != (de = readdir(dir))) {
						if ( de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0' ) {
							continue;
							//- ignore the parent dir
						}
				
						if ( de->d_name[0] == '.' ) {
							continue;
							//- ignore the hidden file
						}

						sprintf(partion_path, "%s%s", disk_path, de->d_name);
						
						if(strstr( con->physical.path->ptr, partion_path )){
							sprintf(sql_query, "%s AND PATH LIKE '%s%s/%s'", sql_query, "%", partion_path, "%");
						}

						partion_count++;
					}
					closedir(dir);
				}
#else
				char partion_path[100] = "/mnt/sda";
				sprintf(sql_query, "%s AND PATH LIKE '%s%s/%s'", sql_query, "%", partion_path, "%");
				partion_count = 1;
#endif
				if(partion_count>1){
					Cdbg(1, "sql_query=%s", sql_query);
					int count = 0;
					if( sql_get_table(sql_minidlna, sql_query, &result2, &rows2, NULL) == SQLITE_OK ){
						Cdbg(DBE, "aaaaaaaaaaaaaa rows2=%d", rows2);

						for( j=column_count2; j<=rows2*column_count2; j+=column_count2 ){					
							count = atoi(result2[j]);						
						}

						sqlite3_free_table(result2);
					}

					Cdbg(DBE, "bbbbbbbbbbbbb count=%d", count);

					if(count==0){
						buffer_free(id);
						continue;
					}
				}
#endif

				buffer_append_string_len(b,CONST_STR_LEN("<item>"));
				buffer_append_string_len(b,CONST_STR_LEN("<id>"));
				//buffer_append_string(b,object_id);
				//if(buffer_is_equal_string(classify, CONST_STR_LEN("artist")))
				//	buffer_append_string(b,"$0");	
				buffer_append_string_buffer(b, id);
				buffer_append_string_len(b,CONST_STR_LEN("</id>"));
				buffer_append_string_len(b,CONST_STR_LEN("<title>"));
				buffer_append_string(b,title);
				buffer_append_string_len(b,CONST_STR_LEN("</title>"));
				buffer_append_string_len(b,CONST_STR_LEN("<artist>"));
				buffer_append_string(b,artist);
				buffer_append_string_len(b,CONST_STR_LEN("</artist>"));
				
				char* image = NULL;
				sqlite_int64 plAlbumArt = (album_art ? strtoll(album_art, NULL, 10) : 0);
				if(get_album_cover_image(sql_minidlna, plAlbumArt, &image)==1){
					buffer_append_string_len(b, CONST_STR_LEN("<thumb_image>"));
					buffer_append_string(b, image);
					buffer_append_string_len(b, CONST_STR_LEN("</thumb_image>"));
					free(image);
				}
					
				buffer_append_string_len(b,CONST_STR_LEN("</item>"));

				buffer_free(id);
			}
						
			sqlite3_free_table(result);
		}

		sqlite3_close(sql_minidlna);
		buffer_append_string_len(b,CONST_STR_LEN("</result>"));
#endif

		chunkqueue_append_buffer(con->write_queue, b);
		buffer_free(b);

		con->http_status = 200;
		con->file_finished = 1;
		return HANDLER_FINISHED;
	}

	case HTTP_METHOD_GETMUSICPLAYLIST:{
		Cdbg(DBE, "do HTTP_METHOD_GETMUSICPLAYLIST");
#if EMBEDDED_EANBLE			
		if(!con->srv_socket->is_ssl){
			con->http_status = 403;
			return HANDLER_FINISHED;
		}
#endif

		if(is_dms_enabled() == 0){
			//- Service Not Available
			con->http_status = 503;
			return HANDLER_FINISHED;
		}

		buffer* play_id = NULL;
		
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "id"))) {
			play_id = ds->value;
		}
		else{
			con->http_status = 400;
			con->file_finished = 1;
			return HANDLER_FINISHED;
		}
#ifdef USE_MINIDLNA_DB
		sqlite3 *sql_minidlna = NULL;
		if (!buffer_is_empty(p->minidlna_db_file)) {			
			if (SQLITE_OK != sqlite3_open(p->minidlna_db_file->ptr, &(sql_minidlna))) { 			
				Cdbg(DBE, "Fail to open minidlna db %s", p->minidlna_db_file->ptr);
			}
		}
		
		if (!sql_minidlna) {
			Cdbg(DBE, "NULL value 'sql_minidlna'!");
			con->http_status = 207;
			con->file_finished = 1;
			return HANDLER_FINISHED;	
		}
		
		int column_count = 2;
		int rows, i;
		char **result;
		char sql_query[2048] = "\0";

		sprintf(sql_query, "SELECT d.TITLE as TITLE, "
						   "d.PATH as PATH "
						   "from OBJECTS o "
						   "left join DETAILS d on (o.DETAIL_ID = d.ID) "
						   "where o.PARENT_ID = '%s'", play_id->ptr);
		
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/xml; charset=\"utf-8\""));
				
		b = buffer_init();
				
		buffer_copy_string_len(b, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));
		buffer_append_string_len(b,CONST_STR_LEN("<result>"));
		
		Cdbg(DBE, "do HTTP_METHOD_GETMUSICPLAYLIST, sql_query=%s", sql_query);

		char auth[100]="\0";		
		if(con->aidisk_username->used && con->aidisk_passwd->used)
			sprintf(auth, "%s:%s", con->aidisk_username->ptr, con->aidisk_passwd->ptr);
		else{
			con->http_status = 400;
			sqlite3_close(sql_minidlna);
			return HANDLER_FINISHED;
		}

		char* base64_auth = ldb_base64_encode(auth, strlen(auth));
		
		if( sql_get_table(sql_minidlna, sql_query, &result, &rows, NULL) == SQLITE_OK ){
			Cdbg(DBE, "sql_query=%s, rows=%d", sql_query, rows);
			int i=0;
					
			for( i=column_count; i<=rows*column_count; i+=column_count ){			
		
				char* title = result[i];
				char* path = result[i+1];
				Cdbg(DBE, "title=%s", title);	
				Cdbg(DBE, "path=%s", path);	
				buffer_append_string_len(b,CONST_STR_LEN("<item>"));
				buffer_append_string_len(b,CONST_STR_LEN("<title>"));
				if(title) buffer_append_string(b,title);
				buffer_append_string_len(b,CONST_STR_LEN("</title>"));
				buffer_append_string_len(b,CONST_STR_LEN("<path>"));
				if(path) buffer_append_string(b,path);
				buffer_append_string_len(b,CONST_STR_LEN("</path>"));

				if(path){
					char* filename = NULL;
					buffer* buffer_filename = buffer_init();
					extract_filename(path, &filename);
					buffer_copy_string(buffer_filename, "");
					buffer_append_string_encoded(buffer_filename,filename,strlen(filename),ENCODING_REL_URI);
					free(filename);
					
					char* filepath = NULL;
					extract_filepath(path, &filepath);

					char buff[4096];
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
					replace_str(&filepath[0], "tmp/mnt", usbdisk_name, buff);
					#else
					usbdisk_name = (char*)malloc(8);
					memset(usbdisk_name,'\0', 8);
					strcpy(usbdisk_name, "usbdisk");
					replace_str(&filepath[0], "mnt", usbdisk_name, buff);
					#endif

					buffer* buffer_filepath = buffer_init();
					buffer_copy_string(buffer_filepath, "");
					buffer_append_string_encoded(buffer_filepath, buff, strlen(buff), ENCODING_REL_URI);
										
					Cdbg(DBE, "filename=%s, filepath=%s", buffer_filename->ptr, buffer_filepath->ptr);
					
					buffer* buffer_result_share_link;
					if( generate_sharelink(srv, 
						                   con, 
						                   buffer_filename->ptr, 
						                   buffer_filepath->ptr, 
						                   base64_auth, 0, 0, 
						                   &buffer_result_share_link) == 1){
						buffer_append_string_len(b,CONST_STR_LEN("<sharelink>"));
						Cdbg(DBE, "sharelink=%s", buffer_result_share_link->ptr);
						buffer_append_string_buffer(b,buffer_result_share_link);
						buffer_append_string_len(b,CONST_STR_LEN("</sharelink>"));					
					}

					free(usbdisk_name);
					free(filepath);
					buffer_free(buffer_filename);
					buffer_free(buffer_filepath);
					buffer_free(buffer_result_share_link);
				}
				
				buffer_append_string_len(b,CONST_STR_LEN("</item>"));
			}
						
			sqlite3_free_table(result);
		}

		sqlite3_close(sql_minidlna);
		free(base64_auth);
		
		buffer_append_string_len(b,CONST_STR_LEN("</result>"));
#endif

		//Cdbg(DBE, "b=%s", b->ptr);
		chunkqueue_append_buffer(con->write_queue, b);
		buffer_free(b);

		con->http_status = 200;
		con->file_finished = 1;
		return HANDLER_FINISHED;
	}

	case HTTP_METHOD_GETTHUMBIMAGE:{
		Cdbg(DBE, "do HTTP_METHOD_GETTHUMBIMAGE");

#if EMBEDDED_EANBLE			
		if(!con->srv_socket->is_ssl){
			con->http_status = 403;
			return HANDLER_FINISHED;
		}
#endif

		if(is_dms_enabled() == 0){
			//- Service Not Available
			con->http_status = 503;
			return HANDLER_FINISHED;
		}

		buffer* file = NULL;
		
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "File"))) {
			file = ds->value;
		}
		else{
			con->http_status = 400;
			con->file_finished = 1;
			return HANDLER_FINISHED;
		}

		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/xml; charset=\"utf-8\""));
				
		b = buffer_init();
				
		buffer_copy_string_len(b, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));
		buffer_append_string_len(b,CONST_STR_LEN("<result>"));
		
		buffer* file_path = buffer_init();
		buffer_copy_buffer(file_path, con->physical.path);
		buffer_append_string_len(file_path, CONST_STR_LEN("/"));
		buffer_append_string_buffer(file_path, file);
		
		char* image = NULL;
		Cdbg(1, "file_path = %s", file_path->ptr);
		if(get_thumb_image(file_path->ptr, p, &image)==1){
			buffer_append_string_len(b, CONST_STR_LEN("<thumb_image>"));
			buffer_append_string(b, image);
			buffer_append_string_len(b, CONST_STR_LEN("</thumb_image>"));
			free(image);
		}

		buffer_append_string_len(b,CONST_STR_LEN("</result>"));

		buffer_free(file_path);

		chunkqueue_append_buffer(con->write_queue, b);
		buffer_free(b);
		
		con->http_status = 200;
		con->file_finished = 1;
		return HANDLER_FINISHED;
	}
	
	case HTTP_METHOD_GETVIDEOSUBTITLE:{
#if EMBEDDED_EANBLE			
		if(!con->srv_socket->is_ssl){
			con->http_status = 403;
			return HANDLER_FINISHED;
		}
#endif

		buffer* filename = NULL;
		
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "FILENAME"))) {
			filename = ds->value;
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}
		
		Cdbg(DBE, "do HTTP_METHOD_GETVIDEOSUBTITLE %s, filename=%s", con->physical.path->ptr, filename->ptr);

		char auth[100]="\0";		
		if(con->aidisk_username->used && con->aidisk_passwd->used)
			sprintf(auth, "%s:%s", con->aidisk_username->ptr, con->aidisk_passwd->ptr);
		else{
			con->http_status = 400;
			return HANDLER_FINISHED;
		}

		char* base64_auth = ldb_base64_encode(auth, strlen(auth));
		
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/xml; charset=\"utf-8\""));
		
		b = buffer_init();
		
		buffer_copy_string_len(b, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));
		buffer_append_string_len(b,CONST_STR_LEN("<result>"));
		
		if (NULL != (dir = opendir(con->physical.path->ptr))) {
			struct dirent *de;

			while(NULL != (de = readdir(dir))) {
				struct stat st;
				int status = 0;

				if ((de->d_name[0] == '.' && de->d_name[1] == '\0')  ||
				    (de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0')) {
					continue;
					/* ignore the parent dir */
				}

				char* aa = get_filename_ext(de->d_name);
				int len = strlen(aa)+1; 		
				char* file_ext = (char*)malloc(len);
				memset(file_ext,'\0', len);
				strcpy(file_ext, aa);
				for (int i = 0; file_ext[i]; i++)
					file_ext[i] = tolower(file_ext[i]);
				
				if( strcmp(file_ext, "srt")==0 &&
					strstr(de->d_name, filename->ptr)){

				#if 0
					#define BUF_LEN 256
					//char utf[1024] = "我的測試字串";
					unsigned long uni2[BUF_LEN] = {
				        0x8BF7, 0x4FDD, 0x8BC1, 0x8BE5, 0x6587, 0x4EF6, 0x662F, 0x4EE5,
				        0x20, 0x75, 0x74, 0x66, 0x2D, 0x38, 0x20, 0x683C, 0x5F0F,
				        0x5B58, 0x50A8, 0x7684, 0x21, 0x0
				    };
					char utf2[BUF_LEN];
					int utflen2 = BUF_LEN;
					int ret2 = enc_unicode_to_utf8_str(uni2, (unsigned char *)utf2, &utflen2);
					if ( ret2 == 1 ){
				    	Cdbg(DBE, "Ooooooooooooooooooooooooooooooooooook!");
				        utf2[utflen2] = '\0';
				        Cdbg(DBE, "utf2=%s", utf2);
				    }
				#endif
#if 0
					#define BUF_LEN 1024
					buffer* srt_file = buffer_init();
					buffer_copy_buffer(srt_file, con->physical.path);
					buffer_append_string(srt_file, "/");	
					buffer_append_string(srt_file, de->d_name);	
					Cdbg(DBE, "srt_file=%s", srt_file->ptr);
					
					if(!is_utf8_file(srt_file->ptr)){
					
						buffer* new_srt_file = buffer_init();
						
						buffer_copy_buffer(new_srt_file, con->physical.path);
						buffer_append_string(new_srt_file, "/");	
						buffer_append_string_buffer(new_srt_file, filename);
						buffer_append_string(new_srt_file, "_utf8.srt");
						
						//buffer_append_string(new_srt_file, "/tmp/out.srt");
						
						Cdbg(DBE, "new_srt_file=%s", new_srt_file->ptr);
						
						FILE* fp3, *fp4;
						char line[BUF_LEN];
							 
						fp4 = fopen(new_srt_file->ptr, "w");

						//- write utf-8 file header
						unsigned char utf8_header[3] = {0xEF, 0xBB, 0xBF};
						fputs(utf8_header,fp4);

						if((fp3 = fopen(srt_file->ptr, "r")) != NULL){

							#if 1
							memset(line, 0, sizeof(line));
							while(fgets(line, BUF_LEN, fp3) != NULL){
								
								//Cdbg(1, "line=%s", line);
								//- ANSI to UTF-8
								char *iconv_buf = (char*)calloc(BUF_LEN, sizeof(char));
								
								int rc = do_iconv("UTF-8", "CP950",
														   line, 
														   strlen(line), 
														   iconv_buf, 
														   BUF_LEN);
								//Cdbg(1, "iconv_buf=%s", iconv_buf);				 
								fputs(iconv_buf,fp4); 
								
								free(iconv_buf);
							}
							#else
							setlocale(LC_ALL, "");
							char line2[BUF_LEN];
							while(fgets(line2, BUF_LEN, fp3) != NULL){
								
								//Cdbg(DBE, "line2=%s", line2);
								
								char utf2[BUF_LEN];
								int utflen2 = BUF_LEN;
								//char uni2[BUF_LEN] = "我的測試字串";
								/*
								unsigned long uni2[BUF_LEN] = {
								        0x8BF7, 0x4FDD, 0x8BC1, 0x8BE5, 0x6587, 0x4EF6, 0x662F, 0x4EE5,
								        0x20, 0x75, 0x74, 0x66, 0x2D, 0x38, 0x20, 0x683C, 0x5F0F,
								        0x5B58, 0x50A8, 0x7684, 0x21, 0x0
								};
								*/

								#if 0
								wchar_t wt1[BUF_LEN];
								int size4 = ansiToWCHAR(wt1, BUF_LEN, line2 ,BUF_LEN+1, "en_ZW.utf8");
								if(size4>0){
									int ret2 = enc_unicode_to_utf8_str(wt1, (unsigned char *)utf2, &utflen2);
									if ( ret2 == 1 ){
								    	utf2[utflen2] = '\0';
								        Cdbg(DBE, "utf2=%s", utf2);
										fputs(utf2,fp4); 
								    }
								}
								#else
								int ret2 = enc_unicode_to_utf8_str((unsigned long *)m2w(line2), (unsigned char *)utf2, &utflen2);
								if ( ret2 == 1 ){
								   	utf2[utflen2] = '\0';
								    Cdbg(DBE, "utf2=%s", utf2);
									fputs(utf2,fp4); 
								}
								#endif
							}
							#endif
								
							fclose(fp3);
						}
						
						physical s, d;

						s.path = buffer_init();
						s.rel_path = buffer_init();
						buffer_copy_buffer(s.path, new_srt_file);
						
						d.path = buffer_init();
						d.rel_path = buffer_init();
						buffer_copy_buffer(d.path,srt_file);

						Cdbg(DBE, "copy=%s -> %s", new_srt_file->ptr, srt_file->ptr);
						
						//webdav_copy_file(srv, con, p, &s, &d, 1);

						fclose(fp4);					
						buffer_free(srt_file);
						buffer_free(new_srt_file);
					}

#endif					
					buffer* buffer_filepath = buffer_init();
					buffer_copy_string(buffer_filepath, "");
					buffer_append_string_encoded(buffer_filepath, con->url.path->ptr, strlen(con->url.path->ptr), ENCODING_REL_URI);
					
					buffer* buffer_result_share_link;
					if( generate_sharelink(srv, 
						                   con, 
						                   de->d_name, 
						                   buffer_filepath->ptr, 
						                   base64_auth, 0, 0, 
						                   &buffer_result_share_link) == 1){
						buffer_append_string_len(b,CONST_STR_LEN("<file>"));
						buffer_append_string_len(b,CONST_STR_LEN("<name>"));
						buffer_append_string(b,de->d_name);
						buffer_append_string_len(b,CONST_STR_LEN("</name>"));
						buffer_append_string_len(b,CONST_STR_LEN("<sharelink>"));
						buffer_append_string_buffer(b,buffer_result_share_link);
						buffer_append_string_len(b,CONST_STR_LEN("</sharelink>"));
						buffer_append_string_len(b,CONST_STR_LEN("</file>"));
					}

					buffer_free(buffer_filepath);
					buffer_free(buffer_result_share_link);
				}

				free(file_ext);
			}

			closedir(dir);
		}

		buffer_append_string_len(b,CONST_STR_LEN("</result>"));

		chunkqueue_append_buffer(con->write_queue, b);
		buffer_free(b);
		
		con->http_status = 200;
		con->file_finished = 1;
		return HANDLER_FINISHED;
	}

	case HTTP_METHOD_UPLOADTOFACEBOOK:{
		Cdbg(1, "do HTTP_METHOD_UPLOADTOFACEBOOK");

#if EMBEDDED_EANBLE			
		if(!con->srv_socket->is_ssl){
			con->http_status = 403;
			return HANDLER_FINISHED;
		}
#endif

		buffer* filename = NULL;		
		buffer* title = NULL;		
		buffer* album = NULL;		
		buffer* auth_token = NULL;
		
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "FILENAME"))) {
			filename = ds->value;
			buffer_urldecode_path(filename);
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "TITLE"))) {
			title = ds->value;
			buffer_urldecode_path(title);
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}
		
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "TOKEN"))) {
			auth_token = ds->value;
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "ALBUM"))) {
			album = ds->value;
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}
		
		CURL *curl;
		CURLcode rt;
		struct curl_httppost *formpost = NULL;
		struct curl_httppost *lastptr = NULL;
		char md5[129];
		char* response_str;
		char url_upload_facebook[1024] = "\0";
		curl = curl_easy_init();

		strcpy(url_upload_facebook, "https://graph.facebook.com/");
		strcat(url_upload_facebook, album->ptr);
		strcat(url_upload_facebook, "/photos");

		Cdbg(1, "url_upload_facebook=%s", url_upload_facebook);
		
		if(curl) {
			Cdbg(1, "curl_easy_init OK");

			/* Set Host to target in HTTP header, and set response handler
		 	* function */
			curl_easy_setopt(curl, CURLOPT_URL, url_upload_facebook);


			/* Build the form post */			
			curl_formadd(&formpost, &lastptr,
			             CURLFORM_COPYNAME, "access_token",
			             CURLFORM_COPYCONTENTS, auth_token->ptr, CURLFORM_END);

			curl_formadd(&formpost, &lastptr,
			             CURLFORM_COPYNAME, "message",
			             CURLFORM_COPYCONTENTS, title->ptr, CURLFORM_END);

			curl_formadd(&formpost, &lastptr,
			             CURLFORM_COPYNAME, "status",
			             CURLFORM_COPYCONTENTS, "success", CURLFORM_END);
			
			char photo_path[1024] = "\0";
			sprintf(photo_path, "%s/%s", con->physical.path->ptr, filename->ptr);
			Cdbg(1, "photo_path=%s", photo_path);

			//- check file exists
			if(!file_exist(photo_path)){
				curl_easy_cleanup(curl);
				curl_formfree(formpost);
				con->http_status = 404;
				return HANDLER_FINISHED;
			}
			
			curl_formadd(&formpost, &lastptr,
			             CURLFORM_COPYNAME, "source",
			             CURLFORM_FILE, photo_path, CURLFORM_END);

			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

			curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback_func);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_str);
			
			Cdbg(1, "Uploading...");
			rt = curl_easy_perform(curl);

			#if 1
			/* now extract transfer info */ 
			double speed_upload, total_time;
      		curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD, &speed_upload);
      		curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);
 
      		Cdbg(1, "Speed: %.3f bytes/sec during %.3f seconds, response_str=%s",
              		speed_upload, total_time, response_str);
			
			free(response_str);			
			#else
			if (rt) {
				Cdbg(1, "An error occurred during upload! %s", curl_easy_strerror(rt));
			}
			else{
				/* now extract transfer info */ 
				double speed_upload, total_time;
      			curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD, &speed_upload);
      			curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);
 
      			Cdbg(1, "Speed: %.3f bytes/sec during %.3f seconds",
              		speed_upload, total_time);
			}
			#endif
			
			/* Done. Cleanup. */ 
			curl_easy_cleanup(curl);
			curl_formfree(formpost);
		}
		
		con->http_status = 200;
		con->file_finished = 1;
		return HANDLER_FINISHED;
	}

	case HTTP_METHOD_UPLOADTOPICASA:{
		Cdbg(DBE, "do HTTP_METHOD_UPLOADTOPICASA");

#if EMBEDDED_EANBLE			
		if(!con->srv_socket->is_ssl){
			con->http_status = 403;
			return HANDLER_FINISHED;
		}
#endif

		buffer* filename = NULL;
		buffer* title = NULL;
		buffer* auth_token = NULL;
		buffer* user_id = NULL;
		buffer* album_id = NULL;
		long response_result = 0;
		
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "FILENAME"))) {
			filename = ds->value;
			buffer_urldecode_path(filename);
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "TITLE"))) {
			title = ds->value;
			buffer_urldecode_path(title);
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "UID"))) {
			user_id = ds->value;
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "AID"))) {
			album_id = ds->value;
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}
		
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "TOKEN"))) {
			auth_token = ds->value;
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}
		
		CURL *curl;
		CURLcode rt;

		#if 0
		_buffer_t upload_response;
		#endif
		
		buffer* buffer_photoid;
		
		curl = curl_easy_init();
		if(curl) {
			Cdbg(DBE, "curl_easy_init OK");

			char request_url[1024] = "\0";
			sprintf(request_url, "https://picasaweb.google.com/data/feed/api/user/%s/albumid/%s", user_id->ptr, album_id->ptr);
			curl_easy_setopt(curl, CURLOPT_URL, request_url);
			
			char photo_path[1024] = "\0";
			sprintf(photo_path, "%s/%s", con->physical.path->ptr, filename->ptr);
			
			FILE *fd;
 			fd = fopen(photo_path, "rb"); /* open file to upload */ 
		  	if(!fd) {
				curl_easy_cleanup(curl);
		    	con->http_status = 404;
				return HANDLER_FINISHED;
		 	}

			/* to get the file size */
			struct stat file_info;
  			if(fstat(fileno(fd), &file_info) != 0) {
				fclose(fd);
				curl_easy_cleanup(curl);
		    	con->http_status = 404;
				return HANDLER_FINISHED;
			}

			long file_size = file_info.st_size;
			char* file_data = (char*) malloc (sizeof(char)*file_size);
  			if(file_data == NULL) {
				fclose(fd);
				curl_easy_cleanup(curl);
		    	con->http_status = 400;
				return HANDLER_FINISHED;
			}

  			// copy the file into the buffer:
		  	size_t result = fread( file_data, 1, file_size, fd );

			if (result != file_size) {
				free(file_data);
				fclose(fd);
				curl_easy_cleanup(curl);
		    	con->http_status = 400;
				return HANDLER_FINISHED;
			}

			char mpart1[4096] = "\0";
  			sprintf(mpart1, "\nMedia multipart posting\n"
				"--END_OF_PART\n"
				"Content-Type: application/atom+xml\n\n"
				"<entry xmlns='http://www.w3.org/2005/Atom'>\n"
				"<title>%s</title>\n"
                "<summary></summary>\n"
                "<category scheme=\"http://schemas.google.com/g/2005#kind\"\n"
                " term=\"http://schemas.google.com/photos/2007#photo\"/>"
				"</entry>\n"
				"--END_OF_PART\n"
				"Content-Type: image/jpeg\n\n", title->ptr);

			int mpart1size = strlen(mpart1);
			int postdata_length = mpart1size + file_size + strlen("\n--END_OF_PART--");
  			char *postdata = (char*)malloc(sizeof(char)*postdata_length);

			if(postdata == NULL) {
				free(file_data);
				fclose(fd);
				curl_easy_cleanup(curl);
		    	con->http_status = 400;
				return HANDLER_FINISHED;
			}
			
			memcpy( postdata, mpart1, mpart1size);
			memcpy( postdata + mpart1size, file_data, file_size);
			memcpy( postdata + mpart1size + file_size, "\n--END_OF_PART--", strlen("\n--END_OF_PART--") );

			free(file_data);
			fclose(fd);
			
			struct curl_slist *headers = NULL;
			headers = curl_slist_append(headers,"Content-Type: multipart/related; boundary=\"END_OF_PART\"");
			headers = curl_slist_append(headers,"MIME-version: 1.0");
			headers = curl_slist_append(headers,"Expect:");
			headers = curl_slist_append(headers,"GData-Version: 2");
			
			char authHeader[1024] = "\0";
			sprintf(authHeader, "Authorization: OAuth %s", auth_token->ptr);
			headers = curl_slist_append(headers, authHeader);
  
			char content_length[1024] = "\0";
			sprintf(content_length, "Content-Length=%d", file_size);
			headers = curl_slist_append(headers, content_length);
			
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 2);
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
			curl_easy_setopt(curl, CURLOPT_UPLOAD, 0);
			curl_easy_setopt(curl, CURLOPT_POST, 1);
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata);
  			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, postdata_length);
  			
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

			#if 0
			memset(&upload_response,0,sizeof(_buffer_t));
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _picasa_api_buffer_write_func);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &upload_response);
			#endif
			
			Cdbg(DBE, "Uploading...");
			rt = curl_easy_perform(curl);
			
			curl_slist_free_all(headers);
			
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_result);

			#if 0
			if( response_result == 201 ){
				Cdbg(DBE, "complete upload...%s", upload_response.data);
				
				xmlDocPtr xml = xmlParseDoc((xmlChar *)upload_response.data);
				if(xml){
					xmlNode *rootnode = xmlDocGetRootElement(xml);
					Cdbg(1, "rootnode->name=%s", rootnode->name);
					if (0 == xmlStrcmp(rootnode->name, BAD_CAST "entry")) {

						xmlNodePtr entryChilds = rootnode->xmlChildrenNode;

						if( entryChilds != NULL ){
				        	do{
								if ((!xmlStrcmp(entryChilds->name, BAD_CAST "id")) ){
						        	// Get the photo id used in uri for update
						            xmlChar *id= xmlNodeListGetString(xml, entryChilds->xmlChildrenNode, 1);
						            if( xmlStrncmp(id, BAD_CAST "http://", 7) ){
						            	buffer_photoid = buffer_init();
										buffer_copy_string( buffer_photoid, id );
										Cdbg(DBE, "buffer_photoid=%s", buffer_photoid->ptr);
						            }
						            xmlFree(id);

									break;
					          	}
							}while( (entryChilds = entryChilds->next)!=NULL );
      					}
					}

					xmlFreeDoc(xml);
				}
			}
			#endif
  			
			/* Done. Cleanup. */ 
			free(postdata);

			#if 0
			free(upload_response.data);
			#endif
			
			curl_easy_cleanup(curl);
		}
		
		con->http_status = ( response_result == 201 ) ? 200 : 501;
		con->file_finished = 1;
		return HANDLER_FINISHED;
	}
	
	case HTTP_METHOD_UPLOADTOFLICKR:{
		Cdbg(1, "do HTTP_METHOD_UPLOADTOFLICKR");

#if EMBEDDED_EANBLE			
		if(!con->srv_socket->is_ssl){
			con->http_status = 403;
			return HANDLER_FINISHED;
		}
#endif

		buffer* filename = NULL;
		buffer* title = NULL;
		buffer* auth_token = NULL;
		
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "FILENAME"))) {
			filename = ds->value;
			buffer_urldecode_path(filename);
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "TITLE"))) {
			title = ds->value;
			buffer_urldecode_path(title);
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}
		
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "TOKEN"))) {
			auth_token = ds->value;
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}
#if 0
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "PHOTOSET"))) {
			photoset = ds->value;
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}
#endif		
		//char api_key[100] = "37140360286c5cd9952023fa8b662a64";
		//char secret[100] = "804b51d14d840d6e";
		char api_key[100] = "c0466d7736e0275d062ce64aefaacfe0";
		char secret[100] = "228e160cf8805246";

		CURL *curl;
		CURLcode rt;
		struct curl_httppost *formpost = NULL;
		struct curl_httppost *lastptr = NULL;
		char md5[129];
		char* response_str;
		buffer* buffer_photoid = buffer_init();
		
		curl = curl_easy_init();
		if(curl) {
			Cdbg(1, "curl_easy_init OK");

			/* Set Host to target in HTTP header, and set response handler
		 	* function */
			curl_easy_setopt(curl, CURLOPT_URL, "https://api.flickr.com/services/upload/");
			/* curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); */

			/*
			char input_string[1024] = "\0";
			sprintf(input_string, "api_key%sauth_token%sdescription%stags%stitle%s", api_key, auth_token->ptr, description, tags, title);
			Cdbg(1, "input_string=%s", input_string);
			md5String(secret, input_string, md5);	
			*/
			
			md5sum(md5, 7, secret, "api_key", api_key, "auth_token", auth_token->ptr, "title", title->ptr);			
			Cdbg(1, "md5=%s", md5);
			
			/* Build the form post */
			curl_formadd(&formpost, &lastptr,
			             CURLFORM_COPYNAME, "api_key",
			             CURLFORM_COPYCONTENTS, api_key, CURLFORM_END);
			
			curl_formadd(&formpost, &lastptr,
			             CURLFORM_COPYNAME, "auth_token",
			             CURLFORM_COPYCONTENTS, auth_token->ptr, CURLFORM_END);

			curl_formadd(&formpost, &lastptr,
			             CURLFORM_COPYNAME, "api_sig",
			             CURLFORM_COPYCONTENTS, md5, CURLFORM_END);
			
			curl_formadd(&formpost, &lastptr,
			             CURLFORM_COPYNAME, "title",
			             CURLFORM_COPYCONTENTS, title->ptr, CURLFORM_END);

			char photo_path[1024] = "\0";
			sprintf(photo_path, "%s/%s", con->physical.path->ptr, filename->ptr);
			Cdbg(1, "photo_path=%s", photo_path);

			//- check file exists
			if(!file_exist(photo_path)){
				curl_easy_cleanup(curl);
				curl_formfree(formpost);
				buffer_free(buffer_photoid);
				con->http_status = 404;
				return HANDLER_FINISHED;
			}
							
			curl_formadd(&formpost, &lastptr,
			             CURLFORM_COPYNAME, "photo",
			             CURLFORM_FILE, photo_path, CURLFORM_END);

			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			
			curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback_func);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_str);
			
			Cdbg(1, "Uploading...");
			rt = curl_easy_perform(curl);

			#if 0
			/* now extract transfer info */ 
			double speed_upload, total_time;
      		curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD, &speed_upload);
      		curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);
 
      		Cdbg(1, "Speed: %.3f bytes/sec during %.3f seconds, response_str=%s",
              		speed_upload, total_time, response_str);
			#endif
			
			xmlDocPtr xml = xmlParseMemory(response_str, strlen(response_str));
			if(xml){
				xmlNode *rootnode = xmlDocGetRootElement(xml);
				
				if (0 == xmlStrcmp(rootnode->name, BAD_CAST "rsp")) {

					xmlChar *stat = xmlGetProp(rootnode,(const xmlChar*) "stat"); 

					if (0 == xmlStrcmp(stat, BAD_CAST "ok")) {
						
						xmlNode *childnode;
						for (childnode = rootnode->children; childnode; childnode = childnode->next) {
							if (0 == xmlStrcmp(childnode->name, BAD_CAST "photoid")) {
								xmlChar *photoid = xmlNodeGetContent(childnode);
								buffer_copy_string( buffer_photoid, photoid );								
								break;
							}
						}
					}
				}

				xmlFreeDoc(xml);
			}

			free(response_str);
			
			/* Done. Cleanup. */ 
			curl_easy_cleanup(curl);
			curl_formfree(formpost);
		}

		if(!buffer_is_empty(buffer_photoid)){
				
			Cdbg(1, "buffer_photoid=%s", buffer_photoid->ptr);
#if 0

			//- set photo to photoset
			//if(!buffer_is_empty(photoset)){
			if(buffer_is_empty(photoset)){

				char* response_str2;
				struct curl_httppost *formpost2 = NULL;
				struct curl_httppost *lastptr2 = NULL;
				curl = curl_easy_init();
				if(curl) {
					curl_easy_setopt(curl, CURLOPT_URL, "http://api.flickr.com/services/rest/");
							
					md5sum(md5, 11, secret, "method", "flickr.photosets.addPhoto", "api_key", api_key, "auth_token", auth_token->ptr, "photoset_id", photoset->ptr, "photo_id", buffer_photoid->ptr); 		
					Cdbg(1, "md5=%s, photoset=%s", md5, photoset->ptr);
							
					/* Build the form post */
					curl_formadd(&formpost2, &lastptr2,
								 CURLFORM_COPYNAME, "method",
								 CURLFORM_COPYCONTENTS, "flickr.photosets.addPhoto", CURLFORM_END);
					
					curl_formadd(&formpost2, &lastptr2,
								 CURLFORM_COPYNAME, "api_key",
								 CURLFORM_COPYCONTENTS, api_key, CURLFORM_END);

					curl_formadd(&formpost2, &lastptr2,
								 CURLFORM_COPYNAME, "photoset_id",
								 CURLFORM_COPYCONTENTS, photoset->ptr, CURLFORM_END);

					curl_formadd(&formpost2, &lastptr2,
								 CURLFORM_COPYNAME, "photo_id",
								 CURLFORM_COPYCONTENTS, buffer_photoid->ptr, CURLFORM_END);
					
					curl_formadd(&formpost2, &lastptr2,
								 CURLFORM_COPYNAME, "auth_token",
								 CURLFORM_COPYCONTENTS, auth_token->ptr, CURLFORM_END);
				
					curl_formadd(&formpost2, &lastptr2,
								 CURLFORM_COPYNAME, "api_sig",
								 CURLFORM_COPYCONTENTS, md5, CURLFORM_END);
					
					curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost2);

					#if 1
					rt = curl_easy_perform(curl);

					if (rt) {
						Cdbg(1, "An error occurred during upload! %s", curl_easy_strerror(rt));
					}
					#else
					curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback_func);
					curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_str2);							
					Cdbg(1, "2", md5);
					
					rt = curl_easy_perform(curl);

					Cdbg(1, "2, response_str2=%s", response_str2);
					xmlDocPtr xml = xmlParseMemory(response_str2, strlen(response_str2));
					if(xml){
						xmlNode *rootnode = xmlDocGetRootElement(xml);
								
						if (0 == xmlStrcmp(rootnode->name, BAD_CAST "rsp")) {
				
							xmlChar *stat = xmlGetProp(rootnode,(const xmlChar*) "stat"); 
				
							if (0 == xmlStrcmp(stat, BAD_CAST "ok")) {
								Cdbg(1, "set photoset OK...");
							}
				
							xmlFreeDoc(xml);
						}
					}

					free(response_str2);
					#endif
					
					/* Done. Cleanup. */ 
					curl_easy_cleanup(curl);
					curl_formfree(formpost2);
				}
				
			}
#endif			
			response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/xml; charset=\"utf-8\""));
			
			b = buffer_init();
			
			buffer_copy_string_len(b, CONST_STR_LEN("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));
			buffer_append_string_len(b,CONST_STR_LEN("<result>"));
			buffer_append_string_len(b,CONST_STR_LEN("<photoid>"));
			buffer_append_string_buffer(b,buffer_photoid);
			buffer_append_string_len(b,CONST_STR_LEN("</photoid>"));
			buffer_append_string_len(b,CONST_STR_LEN("</result>"));

			chunkqueue_append_buffer(con->write_queue, b);
			buffer_free(b);
		
			con->http_status = 200;
		}
		else
			con->http_status = 501;

		buffer_free(buffer_photoid);
		
		con->file_finished = 1;
		return HANDLER_FINISHED;
	}

	case HTTP_METHOD_UPLOADTOTWITTER:{
		Cdbg(DBE, "do HTTP_METHOD_UPLOADTOTWITTER");

#if EMBEDDED_EANBLE			
		if(!con->srv_socket->is_ssl){
			con->http_status = 403;
			return HANDLER_FINISHED;
		}
#endif

		buffer* filename = NULL;
		buffer* title = NULL;
		buffer* auth_token = NULL;
		buffer* auth_secret = NULL;
		buffer* nonce = NULL;
		buffer* timestamp = NULL;
		buffer* signature = NULL;
		buffer* photo_size_limit = NULL;
		
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "FILENAME"))) {
			filename = ds->value;
			buffer_urldecode_path(filename);
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "TITLE"))) {
			title = ds->value;
			buffer_urldecode_path(title);
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}
		
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "TOKEN"))) {
			auth_token = ds->value;
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "SECRET"))) {
			auth_secret = ds->value;
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "NONCE"))) {
			nonce = ds->value;
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "TIMESTAMP"))) {
			timestamp = ds->value;
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}
		
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "SIGNATURE"))) {
			signature = ds->value;
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "PHOTOSIZELIMIT"))) {
			photo_size_limit = ds->value;
		} else {
			con->http_status = 400;
			return HANDLER_FINISHED;
		}
			
		char consumer_key[100] = "hBQrdHdHClXylEI6by40DTMVA";
		//char consumer_secret[100] = "2PlfH4Bx9XVZTYjOz3hoeqAY31aPUx916hU0R4gu0D0uCZ4Y4L";
		
		CURL *curl;
		CURLcode rt;
		struct curl_httppost *formpost = NULL;
		struct curl_httppost *lastptr = NULL;
		char* response_str = NULL;
		long response_code = 0;
		
		curl = curl_easy_init();
		if(curl) {
			Cdbg(DBE, "curl_easy_init OK");

			/* Set Host to target in HTTP header, and set response handler
		 	* function */
		 	//curl_easy_setopt(curl, CURLOPT_URL, "https://upload.twitter.com/1.1/statuses/update_with_media.json");
			curl_easy_setopt(curl, CURLOPT_URL, "https://api.twitter.com/1.1/statuses/update_with_media.json");

			struct curl_slist *headers = NULL;
			char authHeader[1024] = "\0";
			sprintf(authHeader, "Authorization: OAuth"
				" oauth_consumer_key=\"%s\","
				" oauth_nonce=\"%s\","
				" oauth_signature_method=\"HMAC-SHA1\","
				" oauth_timestamp=\"%s\","
				" oauth_token=\"%s\","
				" oauth_version=\"1.0\","
				" oauth_signature=\"%s\"",
				consumer_key,
				nonce->ptr,
				timestamp->ptr,
				auth_token->ptr,
				signature->ptr);
			headers = curl_slist_append(headers, authHeader);
			
			headers = curl_slist_append(headers,"Content-Type: multipart/form-data");
			
			char photo_path[1024] = "\0";
			sprintf(photo_path, "%s/%s", con->physical.path->ptr, filename->ptr);
			Cdbg(DBE, "photo_path=%s", photo_path);

			FILE *fd;
 			fd = fopen(photo_path, "rb"); /* open file to upload */ 
		  	if(!fd) {
				curl_easy_cleanup(curl);
		    	con->http_status = 404;
				return HANDLER_FINISHED;
		 	}

			/* to get the file size */
			struct stat file_info;
  			if(fstat(fileno(fd), &file_info) != 0) {
				fclose(fd);
				curl_easy_cleanup(curl);
		    	con->http_status = 404;
				return HANDLER_FINISHED;
			}

			long file_size = file_info.st_size;
			long l_photo_size_limit = atol(photo_size_limit->ptr);
			fclose(fd);

			if(file_size>=l_photo_size_limit){
				curl_easy_cleanup(curl);
		    	con->http_status = 501;
				return HANDLER_FINISHED;
			}

			curl_formadd(&formpost, &lastptr,
			             CURLFORM_COPYNAME, "status",
			             CURLFORM_COPYCONTENTS, title->ptr, CURLFORM_END);
			
			curl_formadd(&formpost, &lastptr,
			             CURLFORM_COPYNAME, "media[]",
			             CURLFORM_FILE, photo_path, CURLFORM_END);
			
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 2);
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			
			curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
						
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback_func);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_str);
						
			Cdbg(DBE, "Uploading...");
			rt = curl_easy_perform(curl);
			
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
			
			Cdbg(DBE, "response_code=%d, response_str = %s", response_code, response_str);
			
			free(response_str);
			
			/* Done. Cleanup. */ 
			curl_easy_cleanup(curl);
			curl_formfree(formpost);
		}

		
		if(response_code==200){
			con->http_status = 200;
		}
		else
			con->http_status = 501;

		con->file_finished = 1;
		return HANDLER_FINISHED;
	}
	
	default:
		break;
	}

	/* not found */
	return HANDLER_GO_ON;
}


/* this function is called at dlopen() time and inits the callbacks */
#ifndef APP_IPKG
int mod_webdav_plugin_init(plugin *p);
int mod_webdav_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("webdav");
	
	p->init        = mod_webdav_init;
	p->handle_uri_clean  = mod_webdav_uri_handler;
	p->handle_physical   = mod_webdav_subrequest_handler;
	p->set_defaults  = mod_webdav_set_defaults;
	p->cleanup     = mod_webdav_free;

	p->data        = NULL;

	return 0;
}
#else
int aicloud_mod_webdav_plugin_init(plugin *p);
int aicloud_mod_webdav_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("webdav");

	p->init        = mod_webdav_init;
	p->handle_uri_clean  = mod_webdav_uri_handler;
	p->handle_physical   = mod_webdav_subrequest_handler;
	p->set_defaults  = mod_webdav_set_defaults;
	p->cleanup     = mod_webdav_free;

	p->data        = NULL;

	return 0;
}
#endif
