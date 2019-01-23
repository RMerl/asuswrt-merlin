#ifndef _BASE_H_
#define _BASE_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "settings.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <limits.h>

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#include "buffer.h"
#include "array.h"
#include "chunk.h"
#include "keyvalue.h"
#include "fdevent.h"
#include "sys-socket.h"
#include "splaytree.h"
#include "etag.h"


#if defined HAVE_LIBSSL && defined HAVE_OPENSSL_SSL_H
# define USE_OPENSSL
# include <openssl/ssl.h>
# if ! defined OPENSSL_NO_TLSEXT && ! defined SSL_CTRL_SET_TLSEXT_HOSTNAME
#  define OPENSSL_NO_TLSEXT
# endif
#endif

#ifdef HAVE_FAM_H
# include <fam.h>
#endif

#ifndef O_BINARY
# define O_BINARY 0
#endif

#ifndef O_LARGEFILE
# define O_LARGEFILE 0
#endif

#ifndef SIZE_MAX
# ifdef SIZE_T_MAX
#  define SIZE_MAX SIZE_T_MAX
# else
#  define SIZE_MAX ((size_t)~0)
# endif
#endif

#ifndef SSIZE_MAX
# define SSIZE_MAX ((size_t)~0 >> 1)
#endif

#ifdef __APPLE__
#include <crt_externs.h>
#define environ (* _NSGetEnviron())
#else
extern char **environ;
#endif

/* for solaris 2.5 and NetBSD 1.3.x */
#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

/* solaris and NetBSD 1.3.x again */
#if (!defined(HAVE_STDINT_H)) && (!defined(HAVE_INTTYPES_H)) && (!defined(uint32_t))
# define uint32_t u_int32_t
#endif


#ifndef SHUT_WR
# define SHUT_WR 1
#endif

typedef enum { T_CONFIG_UNSET,
		T_CONFIG_STRING,
		T_CONFIG_SHORT,
		T_CONFIG_INT,
		T_CONFIG_BOOLEAN,
		T_CONFIG_ARRAY,
		T_CONFIG_LOCAL,
		T_CONFIG_DEPRECATED,
		T_CONFIG_UNSUPPORTED
} config_values_type_t;

typedef enum { T_CONFIG_SCOPE_UNSET,
		T_CONFIG_SCOPE_SERVER,
		T_CONFIG_SCOPE_CONNECTION
} config_scope_type_t;

typedef struct {
	const char *key;
	void *destination;

	config_values_type_t type;
	config_scope_type_t scope;
} config_values_t;

typedef enum { 
	DIRECT, 
	EXTERNAL, 
	SMB_BASIC,
	SMB_NTLM
} connection_type;

typedef struct {
	char *key;
	connection_type type;
	char *value;
} request_handler;

typedef struct {
	char *key;
	char *host;
	unsigned short port;
	int used;
	short factor;
} fcgi_connections;


typedef union {
#ifdef HAVE_IPV6
	struct sockaddr_in6 ipv6;
#endif
	struct sockaddr_in ipv4;
#ifdef HAVE_SYS_UN_H
	struct sockaddr_un un;
#endif
	struct sockaddr plain;
} sock_addr;

/* fcgi_response_header contains ... */
#define HTTP_STATUS         BV(0)
#define HTTP_CONNECTION     BV(1)
#define HTTP_CONTENT_LENGTH BV(2)
#define HTTP_DATE           BV(3)
#define HTTP_LOCATION       BV(4)

typedef struct {
	/** HEADER */
	/* the request-line */
	buffer *request;
	buffer *uri;

	buffer *orig_uri;

	http_method_t  http_method;
	http_version_t http_version;

	buffer *request_line;

	/* strings to the header */
	buffer *http_host; /* not alloced */
	const char   *http_range;
	const char   *http_content_type;
	const char   *http_if_modified_since;
	const char   *http_if_none_match;

	array  *headers;

	/* CONTENT */
	size_t content_length; /* returned by strtoul() */

	/* internal representation */
	int     accept_encoding;

	/* internal */
	buffer *pathinfo;
} request;

typedef struct {
	off_t   content_length;
	int     keep_alive;               /* used by  the subrequests in proxy, cgi and fcgi to say the subrequest was keep-alive or not */

	array  *headers;

	enum {
		HTTP_TRANSFER_ENCODING_IDENTITY, HTTP_TRANSFER_ENCODING_CHUNKED
	} transfer_encoding;
} response;

typedef struct {
	buffer *scheme; /* scheme without colon or slashes ( "http" or "https" ) */

	/* authority with optional portnumber ("site.name" or "site.name:8080" ) NOTE: without "username:password@" */
	buffer *authority;

	/* path including leading slash ("/" or "/index.html") - urldecoded, and sanitized  ( buffer_path_simplify() && buffer_urldecode_path() ) */
	buffer *path;
	buffer *path_raw; /* raw path, as sent from client. no urldecoding or path simplifying */
	buffer *query; /* querystring ( everything after "?", ie: in "/index.php?foo=1", query is "foo=1" ) */
} request_uri;

typedef struct {
	buffer *path;
	buffer *basedir; /* path = "(basedir)(.*)" */

	buffer *doc_root; /* path = doc_root + rel_path */
	buffer *rel_path;

	buffer *etag;
} physical;

typedef struct {
	buffer *name;
	buffer *etag;

	struct stat st;

	time_t stat_ts;

#ifdef HAVE_LSTAT
	char is_symlink;
#endif

#ifdef HAVE_FAM_H
	int    dir_version;
#endif

	buffer *content_type;
} stat_cache_entry;

typedef struct {
	splay_tree *files; /* the nodes of the tree are stat_cache_entry's */

	buffer *dir_name; /* for building the dirname from the filename */
#ifdef HAVE_FAM_H
	splay_tree *dirs; /* the nodes of the tree are fam_dir_entry */

	FAMConnection fam;
	int    fam_fcce_ndx;
#endif
	buffer *hash_key;  /* temp-store for the hash-key */
} stat_cache;

typedef struct {
	array *mimetypes;

	/* virtual-servers */
	buffer *document_root;
	buffer *server_name;
	buffer *error_handler;
	buffer *server_tag;
	buffer *dirlist_encoding;
	buffer *errorfile_prefix;

#ifdef HAVE_LIBSMBCLIENT
	//- Sungmin add 20111017
	buffer *auth_ntlm_list;
#endif

	unsigned short max_keep_alive_requests;
	unsigned short max_keep_alive_idle;
	unsigned short max_read_idle;
	unsigned short max_write_idle;
	unsigned short use_xattr;
	unsigned short follow_symlink;
	unsigned short range_requests;

	/* debug */

	unsigned short log_file_not_found;
	unsigned short log_request_header;
	unsigned short log_request_handling;
	unsigned short log_response_header;
	unsigned short log_condition_handling;
	unsigned short log_ssl_noise;
	unsigned short log_timeouts;


	/* server wide */
	buffer *ssl_pemfile;
	buffer *ssl_ca_file;
	buffer *ssl_cipher_list;
	buffer *ssl_dh_file;
	buffer *ssl_ec_curve;
	unsigned short ssl_honor_cipher_order; /* determine SSL cipher in server-preferred order, not client-order */
	unsigned short ssl_empty_fragments; /* whether to not set SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS */
	unsigned short ssl_use_sslv2;
	unsigned short ssl_use_sslv3;
	unsigned short ssl_verifyclient;
	unsigned short ssl_verifyclient_enforce;
	unsigned short ssl_verifyclient_depth;
	buffer *ssl_verifyclient_username;
	unsigned short ssl_verifyclient_export_cert;
	unsigned short ssl_disable_client_renegotiation;

	unsigned short use_ipv6, set_v6only; /* set_v6only is only a temporary option */
	unsigned short defer_accept;
	unsigned short ssl_enabled; /* only interesting for setting up listening sockets. don't use at runtime */
	unsigned short allow_http11;
	unsigned short etag_use_inode;
	unsigned short etag_use_mtime;
	unsigned short etag_use_size;
	unsigned short force_lowercase_filenames; /* if the FS is case-insensitive, force all files to lower-case */
	unsigned int max_request_size;

	unsigned short kbytes_per_second; /* connection kb/s limit */

	/* configside */
	unsigned short global_kbytes_per_second; /*  */

	off_t  global_bytes_per_second_cnt;
	/* server-wide traffic-shaper
	 *
	 * each context has the counter which is inited once
	 * a second by the global_kbytes_per_second config-var
	 *
	 * as soon as global_kbytes_per_second gets below 0
	 * the connected conns are "offline" a little bit
	 *
	 * the problem:
	 * we somehow have to loose our "we are writable" signal
	 * on the way.
	 *
	 */
	off_t *global_bytes_per_second_cnt_ptr; /*  */

#ifdef USE_OPENSSL
	SSL_CTX *ssl_ctx; /* not patched */
	/* SNI per host: with COMP_SERVER_SOCKET, COMP_HTTP_SCHEME, COMP_HTTP_HOST */
	EVP_PKEY *ssl_pemfile_pkey;
	X509 *ssl_pemfile_x509;
	STACK_OF(X509_NAME) *ssl_ca_file_cert_names;
#endif
} specific_config;

/* the order of the items should be the same as they are processed
 * read before write as we use this later */
typedef enum {
	CON_STATE_CONNECT,
	CON_STATE_REQUEST_START,
	CON_STATE_READ,
	CON_STATE_REQUEST_END,
	CON_STATE_READ_POST,
	CON_STATE_HANDLE_REQUEST,
	CON_STATE_RESPONSE_START,
	CON_STATE_WRITE,
	CON_STATE_RESPONSE_END,
	CON_STATE_ERROR,
	CON_STATE_CLOSE
} connection_state_t;

typedef enum { COND_RESULT_UNSET, COND_RESULT_FALSE, COND_RESULT_TRUE } cond_result_t;
typedef struct {
	cond_result_t result;
	int patterncount;
	int matches[3 * 10];
	buffer *comp_value; /* just a pointer */
	
	comp_key_t comp_type;
} cond_cache_t;

//- Sungmin add
#ifdef HAVE_LIBSMBCLIENT
#include <libsmbclient.h>
#define uint32 uint32_t

#define NMB_PORT 137
#define SMB_PORT 445

/* Capabilities.  see ftp.microsoft.com/developr/drg/cifs/cifs/cifs4.txt */
#define CAP_RAW_MODE         0x0001
#define CAP_MPX_MODE         0x0002
#define CAP_UNICODE          0x0004
#define CAP_LARGE_FILES      0x0008
#define CAP_NT_SMBS          0x0010
#define CAP_RPC_REMOTE_APIS  0x0020
#define CAP_STATUS32         0x0040
#define CAP_LEVEL_II_OPLOCKS 0x0080
#define CAP_LOCK_AND_READ    0x0100
#define CAP_NT_FIND          0x0200
#define CAP_DFS              0x1000
#define CAP_W2K_SMBS         0x2000
#define CAP_LARGE_READX      0x4000
#define CAP_LARGE_WRITEX     0x8000
#define CAP_UNIX             0x800000 /* Capabilities for UNIX extensions. Created by HP. */
#define CAP_EXTENDED_SECURITY 0x80000000

/* CreateDisposition field. */
#define FILE_SUPERSEDE 0		/* File exists overwrite/supersede. File not exist create. */
#define FILE_OPEN 1			/* File exists open. File not exist fail. */
#define FILE_CREATE 2			/* File exists fail. File not exist create. */
#define FILE_OPEN_IF 3			/* File exists open. File not exist create. */
#define FILE_OVERWRITE 4		/* File exists overwrite. File not exist fail. */
#define FILE_OVERWRITE_IF 5		/* File exists overwrite. File not exist create. */

/* CreateOptions field. */
#define FILE_DIRECTORY_FILE       0x0001
#define FILE_WRITE_THROUGH        0x0002
#define FILE_SEQUENTIAL_ONLY      0x0004
#define FILE_NO_INTERMEDIATE_BUFFERING 0x0008
#define FILE_SYNCHRONOUS_IO_ALERT      0x0010	/* may be ignored */
#define FILE_SYNCHRONOUS_IO_NONALERT   0x0020	/* may be ignored */
#define FILE_NON_DIRECTORY_FILE   0x0040
#define FILE_CREATE_TREE_CONNECTION    0x0080	/* ignore, should be zero */
#define FILE_COMPLETE_IF_OPLOCKED      0x0100	/* ignore, should be zero */
#define FILE_NO_EA_KNOWLEDGE      0x0200
#define FILE_EIGHT_DOT_THREE_ONLY 0x0400 /* aka OPEN_FOR_RECOVERY: ignore, should be zero */
#define FILE_RANDOM_ACCESS        0x0800
#define FILE_DELETE_ON_CLOSE      0x1000
#define FILE_OPEN_BY_FILE_ID	  0x2000
#define FILE_OPEN_FOR_BACKUP_INTENT    0x4000
#define FILE_NO_COMPRESSION       0x8000
#define FILE_RESERVER_OPFILTER    0x00100000	/* ignore, should be zero */
#define FILE_OPEN_REPARSE_POINT   0x00200000
#define FILE_OPEN_NO_RECALL       0x00400000
#define FILE_OPEN_FOR_FREE_SPACE_QUERY 0x00800000 /* ignore should be zero */

/* File Specific access rights */
#define FILE_READ_DATA        0x00000001
#define FILE_WRITE_DATA       0x00000002
#define FILE_APPEND_DATA      0x00000004
#define FILE_READ_EA          0x00000008 /* File and directory */
#define FILE_WRITE_EA         0x00000010 /* File and directory */
#define FILE_EXECUTE          0x00000020
#define FILE_DELETE_CHILD     0x00000040
#define FILE_READ_ATTRIBUTES  0x00000080
#define FILE_WRITE_ATTRIBUTES 0x00000100

#define FILE_ALL_ACCESS       0x000001FF

typedef enum auth_type {
	SMB_AUTH_UNSET,
	SMB_AUTH_BASIC,
	SMB_AUTH_NTLM
}SMB_AUTH_TYPE;

typedef enum ntlm_message_type
{
	NTLMSSP_INITIAL = 0 /* samba internal state */,
	NTLMSSP_NEGOTIATE = 1,
	NTLMSSP_CHALLENGE = 2,
	NTLMSSP_AUTH      = 3,
	NTLMSSP_UNKNOWN   = 4,
	NTLMSSP_DONE      = 5 /* samba final state */
}NTLM_MESSAGE_TYPE;

typedef enum {
	SMB_FILE_QUERY,
	SMB_SHARE_QUERY,
	SMB_HOST_QUERY
}URI_QUERY_TYPE;

/* protocol types. It assumes that higher protocols include lower protocols
   as subsets */
enum protocol_types {
	PROTOCOL_NONE,
	PROTOCOL_CORE,
	PROTOCOL_COREPLUS,
	PROTOCOL_LANMAN1,
	PROTOCOL_LANMAN2,
	PROTOCOL_NT1,
	PROTOCOL_SMB2
};

typedef struct smb_info_s {

	SMB_AUTH_TYPE auth_type;
	
	//common part	
	buffer *workgroup;
	buffer *server;
	buffer *share;
	buffer *path;
	URI_QUERY_TYPE qflag;
	buffer *user_agent;
	buffer *src_ip;
	int auth_right;

	int login_count;
	time_t login_begin_time;
	
	//for Basic
	buffer *username;
	buffer *password;
	time_t auth_time;
	//unsigned char is_authed;
	
	//for NTLM
	struct cli_state *cli;
	NTLM_MESSAGE_TYPE state;
	void *ntlmssp_state;

	buffer *asus_token;
	
	struct smb_info_s *prev, *next;
	
}smb_info_t;
#endif

typedef struct {
	connection_state_t state;

	/* timestamps */
	time_t read_idle_ts;
	time_t close_timeout_ts;
	time_t write_request_ts;

	time_t connection_start;
	time_t request_start;

	struct timeval start_tv;

	size_t request_count;        /* number of requests handled in this connection */
	size_t loops_per_request;    /* to catch endless loops in a single request
				      *
				      * used by mod_rewrite, mod_fastcgi, ... and others
				      * this is self-protection
				      */

	int fd;                      /* the FD for this connection */
	int fde_ndx;                 /* index for the fdevent-handler */
	int ndx;                     /* reverse mapping to server->connection[ndx] */

	/* fd states */
	int is_readable;
	int is_writable;

	int keep_alive;              /* only request.c can enable it, all other just disable */
	int keep_alive_idle;         /* remember max_keep_alive_idle from config */

	int file_started;
	int file_finished;

	chunkqueue *write_queue;      /* a large queue for low-level write ( HTTP response ) [ file, mem ] */
	chunkqueue *read_queue;       /* a small queue for low-level read ( HTTP request ) [ mem ] */
	chunkqueue *request_content_queue; /* takes request-content into tempfile if necessary [ tempfile, mem ]*/

	int traffic_limit_reached;

	off_t bytes_written;          /* used by mod_accesslog, mod_rrd */
	off_t bytes_written_cur_second; /* used by mod_accesslog, mod_rrd */
	off_t bytes_read;             /* used by mod_accesslog, mod_rrd */
	off_t bytes_header;

	int http_status;

	sock_addr dst_addr;
	buffer *dst_addr_buf;

	buffer *asus_token;
	
	/* request */
	buffer *parse_request;
	unsigned int parsed_response; /* bitfield which contains the important header-fields of the parsed response header */

	request  request;
	request_uri uri;
	physical physical;
	response response;

	size_t header_len;

	array  *environment; /* used to pass lighttpd internal stuff to the FastCGI/CGI apps, setenv does that */

	/* response */
	int    got_response;

	int    in_joblist;

	connection_type mode;

	void **plugin_ctx;           /* plugin connection specific config */

	specific_config conf;        /* global connection specific config */
	cond_cache_t *cond_cache;

	buffer *server_name;

	/* error-handler */
	buffer *error_handler;
	int error_handler_saved_status;
	int in_error_handler;

	struct server_socket *srv_socket;   /* reference to the server-socket */

#ifdef USE_OPENSSL
	SSL *ssl;
# ifndef OPENSSL_NO_TLSEXT
	buffer *tlsext_server_name;
# endif
	unsigned int renegotiations; /* count of SSL_CB_HANDSHAKE_START */
#endif
	/* etag handling */
	etag_flags_t etag_flags;

	int conditional_is_valid[COMP_LAST_ELEMENT]; 
#ifdef HAVE_LIBSMBCLIENT
	buffer* share_link_basic_auth;
	buffer* share_link_shortpath;
	buffer* share_link_realpath;
	buffer* share_link_filename;
	int     share_link_type;
	buffer* physical_auth_url;
	physical url; //- start with smb://	 or http://
	buffer* url_options;
	smb_info_t *smb_info;
	int		cur_fd;// samba r/w fd
	
	buffer *aidisk_username;
	buffer *aidisk_passwd;

	buffer *match_smb_ip;
	buffer *replace_smb_name;
#endif

} connection;

typedef struct {
	connection **ptr;
	size_t size;
	size_t used;
} connections;


#ifdef HAVE_IPV6
typedef struct {
	int family;
	union {
		struct in6_addr ipv6;
		struct in_addr  ipv4;
	} addr;
	char b2[INET6_ADDRSTRLEN + 1];
	time_t ts;
} inet_ntop_cache_type;
#endif


typedef struct {
	buffer *uri;
	time_t mtime;
	int http_status;
} realpath_cache_type;

typedef struct {
	time_t  mtime;  /* the key */
	buffer *str;    /* a buffer for the string represenation */
} mtime_cache_type;

typedef struct {
	void  *ptr;
	size_t used;
	size_t size;
} buffer_plugin;

typedef struct {
	unsigned short port;
	buffer *bindhost;

	buffer *errorlog_file;
	unsigned short errorlog_use_syslog;
	buffer *breakagelog_file;

	unsigned short dont_daemonize;
	buffer *changeroot;
	buffer *username;
	buffer *groupname;

	buffer *pid_file;

	buffer *event_handler;

	buffer *modules_dir;
	buffer *network_backend;
	array *modules;
	array *upload_tempdirs;
	unsigned int upload_temp_file_size;

	unsigned short max_worker;
	unsigned short max_fds;
	unsigned short max_conns;
	unsigned int max_request_size;

	unsigned short log_request_header_on_error;
	unsigned short log_state_handling;

	enum { STAT_CACHE_ENGINE_UNSET,
			STAT_CACHE_ENGINE_NONE,
			STAT_CACHE_ENGINE_SIMPLE
#ifdef HAVE_FAM_H
			, STAT_CACHE_ENGINE_FAM
#endif
	} stat_cache_engine;
	unsigned short enable_cores;
	unsigned short reject_expect_100_with_417;

#ifdef HAVE_LIBSMBCLIENT
	//- Sungmin add 20111018
	buffer *arpping_interface;
	buffer *syslog_file;
	buffer *product_image;
	buffer *aicloud_version;
	buffer *smartsync_version;
	buffer *app_installation_url;	
	unsigned int max_sharelink;
#endif
} server_config;

typedef struct server_socket {
	sock_addr addr;
	int       fd;
	int       fde_ndx;

	unsigned short is_ssl;

	buffer *srv_token;

#ifdef USE_OPENSSL
	SSL_CTX *ssl_ctx;
#endif
} server_socket;

typedef struct {
	server_socket **ptr;

	size_t size;
	size_t used;
} server_socket_array;

typedef struct server {
	server_socket_array srv_sockets;

	/* the errorlog */
	int errorlog_fd;
	enum { ERRORLOG_FILE, ERRORLOG_FD, ERRORLOG_SYSLOG, ERRORLOG_PIPE } errorlog_mode;
	buffer *errorlog_buf;

	fdevents *ev, *ev_ins;

	buffer_plugin plugins;
	void *plugin_slots;

	/* counters */
	int con_opened;
	int con_read;
	int con_written;
	int con_closed;

	int ssl_is_init;

	int max_fds;    /* max possible fds */
	int cur_fds;    /* currently used fds */
	int want_fds;   /* waiting fds */
	int sockets_disabled;

	size_t max_conns;

	/* buffers */
	buffer *parse_full_path;
	buffer *response_header;
	buffer *response_range;
	buffer *tmp_buf;

	buffer *tmp_chunk_len;

	buffer *empty_string; /* is necessary for cond_match */

	buffer *cond_check_buf;

	/* caches */
#ifdef HAVE_IPV6
	inet_ntop_cache_type inet_ntop_cache[INET_NTOP_CACHE_MAX];
#endif
	mtime_cache_type mtime_cache[FILE_CACHE_MAX];

	array *split_vals;

	/* Timestamps */
	time_t cur_ts;
	time_t last_generated_date_ts;
	time_t last_generated_debug_ts;
	time_t startup_ts;

	char entropy[8]; /* from /dev/[u]random if possible, otherwise rand() */
	char is_real_entropy; /* whether entropy is from /dev/[u]random */

	buffer *ts_debug_str;
	buffer *ts_date_str;

	/* config-file */
	array *config;
	array *config_touched;

	array *config_context;
	specific_config **config_storage;

	server_config  srvconf;

	short int config_deprecated;
	short int config_unsupported;

	connections *conns;
	connections *joblist;
	connections *fdwaitqueue;

	stat_cache  *stat_cache;

	/**
	 * The status array can carry all the status information you want
	 * the key to the array is <module-prefix>.<name>
	 * and the values are counters
	 *
	 * example:
	 *   fastcgi.backends        = 10
	 *   fastcgi.active-backends = 6
	 *   fastcgi.backend.<key>.load = 24
	 *   fastcgi.backend.<key>....
	 *
	 *   fastcgi.backend.<key>.disconnects = ...
	 */
	array *status;

	fdevent_handler_t event_handler;

	int (* network_backend_write)(struct server *srv, connection *con, int fd, chunkqueue *cq, off_t max_bytes);
#ifdef USE_OPENSSL
	int (* network_ssl_backend_write)(struct server *srv, connection *con, SSL *ssl, chunkqueue *cq, off_t max_bytes);
#endif

	uid_t uid;
	gid_t gid;

//- Sungmin add
#ifdef HAVE_LIBSMBCLIENT
	smb_info_t *smb_srv_info_list;
	int syslog_fd;
	buffer *syslog_buf;
	buffer *cur_login_info;
	buffer *last_login_info;
	time_t last_no_ssl_connection_ts;
	int is_streaming_port_opend;
#endif
	
} server;

//- Sungmin add
#ifdef HAVE_LIBSMBCLIENT
typedef struct smb_srv_info_s {
	int id;
	buffer *ip;
	buffer *mac;
	buffer *name;	
	int online;
	struct smb_srv_info_s *prev, *next;
}smb_srv_info_t;
smb_srv_info_t *smb_srv_info_list;

typedef struct share_link_info_s {
	buffer *shortpath;
	buffer *realpath;
	buffer *filename;
	buffer *auth;
	unsigned long createtime;
	unsigned long expiretime;
	int toshare;
	struct share_link_info_s *prev, *next;
}share_link_info_t;
share_link_info_t *share_link_info_list;

typedef struct aicloud_acc_info_s {
	buffer *username;
	buffer *password;
	struct aicloud_acc_info_s *prev, *next;
}aicloud_acc_info_t;
aicloud_acc_info_t *aicloud_acc_info_list;

typedef struct aicloud_acc_invite_info_s {
	buffer *productid;
	buffer *deviceid;
	buffer *token;
	buffer *permission;
	unsigned long bytes_in_avail;
	int smart_access;
	buffer *security_code;
	int status;
	int auth_type;
	unsigned long createtime;
	struct aicloud_acc_invite_info_s *prev, *next;
}aicloud_acc_invite_info_t;
aicloud_acc_invite_info_t *aicloud_acc_invite_info_list;

typedef enum { 
	T_ACCOUNT_SAMBA,
	T_ACCOUNT_AICLOUD
} account_type;

typedef enum { 
	T_ADMIN,
	T_USER
} account_permission_type;
#endif

#endif
