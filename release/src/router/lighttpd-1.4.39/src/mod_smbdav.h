#ifndef _MOD_SMBDAV_H_
#define _MOD_SMBDAV_H_

#if defined(HAVE_LIBXML_H) && defined(HAVE_SQLITE3_H)
#define USE_PROPPATCH
#define USE_MINIDLNA_DB
#include <libxml/tree.h>
#include <libxml/parser.h>

#include <sqlite3.h>
#endif

#if defined(HAVE_LIBXML_H) && defined(HAVE_SQLITE3_H) && defined(HAVE_UUID_UUID_H)
#define USE_LOCKS

#ifndef EMBEDDED_EANBLE
#include <uuid/uuid.h>
#endif

#endif

#include <unistd.h>
#include <dirent.h>
#include <libsmbclient.h>
#include "base.h"
#include <dlinklist.h>

typedef struct {
	size_t  namelen;
	size_t  iplen;
	size_t  maclen;
	time_t  mtime;
	int online;
	off_t   size;
} smbc_dirls_entry_t;

typedef struct {
	smbc_dirls_entry_t **ent;
	size_t used;
	size_t size;
} smbc_dirls_list_t;

#define DIRLIST_ENT_NAME(ent)	((char*)(ent) + sizeof(smbc_dirls_entry_t))
#define DIRLIST_BLOB_SIZE		16

/**
 * this is a smbdav for a lighttpd plugin
 *
 * at least a very basic one.
 * - for now it is read-only and we only support PROPFIND
 *
 */

#define WEBDAV_FILE_MODE S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
#define WEBDAV_DIR_MODE  S_IRWXU | S_IRWXG | S_IRWXO

/* plugin config for all request/connections */

typedef struct {
	unsigned short enabled;
	unsigned short is_readonly;
	unsigned short log_xml;

	buffer *sqlite_db_name;
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

	//- 20130304 JerryLin add
	buffer *sqlite_minidlna_db_name;
#ifdef USE_MINIDLNA_DB
	sqlite3 *sql_minidlna;
#endif

} plugin_config;

typedef struct {
	PLUGIN_DATA;

	buffer *tmp_buf;
	request_uri uri;
	physical physical;

	plugin_config **config_storage;

	plugin_config conf;
/*
	smb_info_t *smb_info_list;
	buffer *username;
	buffer *password;
*/

} plugin_data;

#if 0
typedef struct smbdav_conn_s {
	buffer *sess_id;
	struct cli_state *cli;
	//buffer *smb_host;
	//buffer *smb_home_dir;
	buffer *uri;
	buffer *workgroup;
	buffer *server;
	buffer *share;
	buffer *path;
	buffer *home;
	uint8_t qflag;	
	unsigned short smb_port;
	NTLM_MESSAGE_TYPE state;
	void *ntlmssp_state;
	//int authed;
	buffer *src_ipaddr;
	uint16_t src_port;
	struct smbdav_conn_s *next, *prev;	
}smbdav_conn_t;
#endif

#endif
