/* 
   Unix SMB/CIFS implementation.
   SMB parameters and setup, plus a whole lot more.
   
   Copyright (C) Andrew Tridgell              1992-2000
   Copyright (C) John H Terpstra              1996-2002
   Copyright (C) Luke Kenneth Casson Leighton 1996-2000
   Copyright (C) Paul Ashton                  1998-2000
   Copyright (C) Simo Sorce                   2001-2002
   Copyright (C) Martin Pool		      2002
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _SMB_H
#define _SMB_H

/* logged when starting the various Samba daemons */
#define COPYRIGHT_STARTUP_MESSAGE	"Copyright Andrew Tridgell and the Samba Team 1992-2011"


#if defined(LARGE_SMB_OFF_T)
#define BUFFER_SIZE (128*1024)
#else /* no large readwrite possible */
#define BUFFER_SIZE (0xFFFF)
#endif

#define SAFETY_MARGIN 1024
#define LARGE_WRITEX_HDR_SIZE 65

#define NMB_PORT 137
#define DGRAM_PORT 138
#define SMB_PORT1 445
#define SMB_PORT2 139
#define SMB_PORTS "445 139"

#define Undefined (-1)
#define False false
#define True true
#define Auto (2)
#define Required (3)

#define SIZEOFWORD 2

#ifndef DEF_CREATE_MASK
#define DEF_CREATE_MASK (0755)
#endif

/* string manipulation flags - see clistr.c and srvstr.c */
#define STR_TERMINATE 1
#define STR_UPPER 2
#define STR_ASCII 4
#define STR_UNICODE 8
#define STR_NOALIGN 16
#define STR_TERMINATE_ASCII 128

/* how long to wait for secondary SMB packets (milli-seconds) */
#define SMB_SECONDARY_WAIT (60*1000)

/* this defines the error codes that receive_smb can put in smb_read_error */
enum smb_read_errors {
	SMB_READ_OK = 0,
	SMB_READ_TIMEOUT,
	SMB_READ_EOF,
	SMB_READ_ERROR,
	SMB_WRITE_ERROR, /* This error code can go into the client smb_rw_error. */
	SMB_READ_BAD_SIG,
	SMB_NO_MEMORY,
	SMB_DO_NOT_DO_TDIS, /* cli_close_connection() check for this when smbfs wants to keep tree connected */
	SMB_READ_BAD_DECRYPT
};

#define DIR_STRUCT_SIZE 43

/* deny modes */
#define DENY_DOS 0
#define DENY_ALL 1
#define DENY_WRITE 2
#define DENY_READ 3
#define DENY_NONE 4
#define DENY_FCB 7

/* open modes */
#define DOS_OPEN_RDONLY 0
#define DOS_OPEN_WRONLY 1
#define DOS_OPEN_RDWR 2
#define DOS_OPEN_EXEC 3
#define DOS_OPEN_FCB 0xF

/* define shifts and masks for share and open modes. */
#define OPENX_MODE_MASK 0xF
#define DENY_MODE_SHIFT 4
#define DENY_MODE_MASK 0x7
#define GET_OPENX_MODE(x) ((x) & OPENX_MODE_MASK)
#define SET_OPENX_MODE(x) ((x) & OPENX_MODE_MASK)
#define GET_DENY_MODE(x) (((x)>>DENY_MODE_SHIFT) & DENY_MODE_MASK)
#define SET_DENY_MODE(x) (((x) & DENY_MODE_MASK) <<DENY_MODE_SHIFT)

/* Sync on open file (not sure if used anymore... ?) */
#define FILE_SYNC_OPENMODE (1<<14)
#define GET_FILE_SYNC_OPENMODE(x) (((x) & FILE_SYNC_OPENMODE) ? True : False)

/* open disposition values */
#define OPENX_FILE_EXISTS_FAIL 0
#define OPENX_FILE_EXISTS_OPEN 1
#define OPENX_FILE_EXISTS_TRUNCATE 2

/* mask for open disposition. */
#define OPENX_FILE_OPEN_MASK 0x3

#define GET_FILE_OPENX_DISPOSITION(x) ((x) & FILE_OPEN_MASK)
#define SET_FILE_OPENX_DISPOSITION(x) ((x) & FILE_OPEN_MASK)

/* The above can be OR'ed with... */
#define OPENX_FILE_CREATE_IF_NOT_EXIST 0x10
#define OPENX_FILE_FAIL_IF_NOT_EXIST 0

typedef union unid_t {
	uid_t uid;
	gid_t gid;
} unid_t;

/* pipe string names */

#ifndef MAXSUBAUTHS
#define MAXSUBAUTHS 15 /* max sub authorities in a SID */
#endif

#define SID_MAX_SIZE ((size_t)(8+(MAXSUBAUTHS*4)))

#include "librpc/gen_ndr/security.h"

/*
 * The complete list of SIDS belonging to this user.
 * Created when a vuid is registered.
 * The definition of the user_sids array is as follows :
 *
 * token->user_sids[0] = primary user SID.
 * token->user_sids[1] = primary group SID.
 * token->user_sids[2..num_sids] = supplementary group SIDS.
 */

#define PRIMARY_USER_SID_INDEX 0
#define PRIMARY_GROUP_SID_INDEX 1

typedef struct write_cache {
	SMB_OFF_T file_size;
	SMB_OFF_T offset;
	size_t alloc_size;
	size_t data_size;
	char *data;
} write_cache;

struct fd_handle {
	size_t ref_count;
	int fd;
	uint64_t position_information;
	SMB_OFF_T pos;
	uint32 private_options;	/* NT Create options, but we only look at
				 * NTCREATEX_OPTIONS_PRIVATE_DENY_DOS and
				 * NTCREATEX_OPTIONS_PRIVATE_DENY_FCB and
				 * NTCREATEX_OPTIONS_PRIVATE_DELETE_ON_CLOSE
				 * for print files *only*, where
				 * DELETE_ON_CLOSE is not stored in the share
				 * mode database.
				 */
	unsigned long gen_id;
};

struct idle_event;
struct share_mode_entry;
struct uuid;
struct named_mutex;
struct wb_context;
struct rpc_cli_smbd_conn;
struct fncall_context;

struct vfs_fsp_data {
    struct vfs_fsp_data *next;
    struct vfs_handle_struct *owner;
    void (*destroy)(void *p_data);
    void *_dummy_;
    /* NOTE: This structure contains four pointers so that we can guarantee
     * that the end of the structure is always both 4-byte and 8-byte aligned.
     */
};

/* the basic packet size, assuming no words or bytes */
#define smb_size 39

struct notify_change {
	uint32_t action;
	const char *name;
};

struct notify_mid_map;
struct notify_entry;
struct notify_event;
struct notify_change_request;
struct sys_notify_backend;
struct sys_notify_context {
	struct event_context *ev;
	struct connection_struct *conn;
	void *private_data; 	/* For use by the system backend */
};

struct notify_change_buf {
	/*
	 * If no requests are pending, changes are queued here. Simple array,
	 * we only append.
	 */

	/*
	 * num_changes == -1 means that we have got a catch-all change, when
	 * asked we just return NT_STATUS_OK without specific changes.
	 */
	int num_changes;
	struct notify_change *changes;

	/*
	 * If no changes are around requests are queued here. Using a linked
	 * list, because we have to append at the end and delete from the top.
	 */
	struct notify_change_request *requests;
};

struct print_file_data {
	char *svcname;
	char *docname;
	char *filename;
	struct policy_handle handle;
	uint32_t jobid;
	uint16 rap_jobid;
};

typedef struct files_struct {
	struct files_struct *next, *prev;
	int fnum;
	struct connection_struct *conn;
	struct fd_handle *fh;
	unsigned int num_smb_operations;
	struct file_id file_id;
	uint64_t initial_allocation_size; /* Faked up initial allocation on disk. */
	mode_t mode;
	uint16 file_pid;
	uint16 vuid;
	write_cache *wcp;
	struct timeval open_time;
	uint32 access_mask;		/* NTCreateX access bits (FILE_READ_DATA etc.) */
	uint32 share_access;		/* NTCreateX share constants (FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE). */

	bool update_write_time_triggered;
	struct timed_event *update_write_time_event;
	bool update_write_time_on_close;
	struct timespec close_write_time;
	bool write_time_forced;

	int oplock_type;
	int sent_oplock_break;
	struct timed_event *oplock_timeout;
	struct lock_struct last_lock_failure;
	int current_lock_count; /* Count the number of outstanding locks and pending locks. */

	struct share_mode_entry *pending_break_messages;
	int num_pending_break_messages;

	bool can_lock;
	bool can_read;
	bool can_write;
	bool modified;
	bool is_directory;
	bool aio_write_behind;
	bool lockdb_clean;
	bool initial_delete_on_close; /* Only set at NTCreateX if file was created. */
	bool delete_on_close;
	bool posix_open;
	bool is_sparse;
	struct smb_filename *fsp_name;
	uint32_t name_hash;		/* Jenkins hash of full pathname. */

	struct vfs_fsp_data *vfs_extension;
	struct fake_file_handle *fake_file_handle;

	struct notify_change_buf *notify;

	struct files_struct *base_fsp; /* placeholder for delete on close */

	/*
	 * Read-only cached brlock record, thrown away when the
	 * brlock.tdb seqnum changes. This avoids fetching data from
	 * the brlock.tdb on every read/write call.
	 */
	int brlock_seqnum;
	struct byte_range_lock *brlock_rec;

	struct dptr_struct *dptr;

	/* if not NULL, means this is a print file */
	struct print_file_data *print_file;

} files_struct;

#include "ntquotas.h"
#include "sysquotas.h"

struct client_address {
	char addr[INET6_ADDRSTRLEN];
	const char *name;
};

struct vuid_cache_entry {
	struct auth_serversupplied_info *session_info;
	uint16_t vuid;
	bool read_only;
};

struct vuid_cache {
	unsigned int next_entry;
	struct vuid_cache_entry array[VUID_CACHE_SIZE];
};

typedef struct {
	char *name;
	bool is_wild;
} name_compare_entry;

struct trans_state {
	struct trans_state *next, *prev;
	uint16 vuid;
	uint64_t mid;

	uint32 max_param_return;
	uint32 max_data_return;
	uint32 max_setup_return;

	uint8 cmd;		/* SMBtrans or SMBtrans2 */

	char *name;		/* for trans requests */
	uint16 call;		/* for trans2 and nttrans requests */

	bool close_on_completion;
	bool one_way;

	unsigned int setup_count;
	uint16 *setup;

	size_t received_data;
	size_t received_param;

	size_t total_param;
	char *param;

	size_t total_data;
	char *data;
};

/*
 * Info about an alternate data stream
 */

struct stream_struct {
	SMB_OFF_T size;
	SMB_OFF_T alloc_size;
	char *name;
};

/* Include VFS stuff */

#include "smb_acls.h"
#include "vfs.h"

struct dfree_cached_info {
	time_t last_dfree_time;
	uint64_t dfree_ret;
	uint64_t bsize;
	uint64_t dfree;
	uint64_t dsize;
};

struct dptr_struct;

struct share_params {
	int service;
};

struct share_iterator {
	int next_id;
};

typedef struct connection_struct {
	struct connection_struct *next, *prev;
	struct smbd_server_connection *sconn; /* can be NULL */
	unsigned cnum; /* an index passed over the wire */
	struct share_params *params;
	bool force_user;
	struct vuid_cache vuid_cache;
	bool printer;
	bool ipc;
	bool read_only; /* Attributes for the current user of the share. */
	uint32_t share_access;
	/* Does this filesystem honor
	   sub second timestamps on files
	   and directories when setting time ? */
	enum timestamp_set_resolution ts_res;
	char *connectpath;
	char *origpath;

	struct vfs_handle_struct *vfs_handles;		/* for the new plugins */

	/*
	 * This represents the user information on this connection. Depending
	 * on the vuid using this tid, this might change per SMB request.
	 */
	struct auth_serversupplied_info *session_info;

	/*
	 * If the "force group" parameter is set, this is the primary gid that
	 * may be used in the users token, depending on the vuid using this tid.
	 */
	gid_t force_group_gid;

	uint16 vuid; /* vuid of user who *opened* this connection, or UID_FIELD_INVALID */

	time_t lastused;
	time_t lastused_count;
	bool used;
	int num_files_open;
	unsigned int num_smb_operations; /* Count of smb operations on this tree. */
	int encrypt_level;
	bool encrypted_tid;

	/* Semantics requested by the client or forced by the server config. */
	bool case_sensitive;
	bool case_preserve;
	bool short_case_preserve;

	/* Semantics provided by the underlying filesystem. */
	int fs_capabilities;
	/* Device number of the directory of the share mount.
	   Used to ensure unique FileIndex returns. */
	SMB_DEV_T base_share_dev;

	name_compare_entry *hide_list; /* Per-share list of files to return as hidden. */
	name_compare_entry *veto_list; /* Per-share list of files to veto (never show). */
	name_compare_entry *veto_oplock_list; /* Per-share list of files to refuse oplocks on. */       
	name_compare_entry *aio_write_behind_list; /* Per-share list of files to use aio write behind on. */       
	struct dfree_cached_info *dfree_info;
	struct trans_state *pending_trans;
	struct notify_context *notify_ctx;

	struct rpc_pipe_client *spoolss_pipe;

} connection_struct;

struct current_user {
	connection_struct *conn;
	uint16 vuid;
	struct security_unix_token ut;
	struct security_token *nt_user_token;
};

struct smbd_smb2_request;

struct smb_request {
	uint8_t cmd;
	uint16 flags2;
	uint16 smbpid;
	uint64_t mid; /* For compatibility with SMB2. */
	uint32_t seqnum;
	uint16 vuid;
	uint16 tid;
	uint8  wct;
	uint16_t *vwv;
	uint16_t buflen;
	const uint8_t *buf;
	const uint8 *inbuf;

	/*
	 * Async handling in the main smb processing loop is directed by
	 * outbuf: reply_xxx routines indicate sync behaviour by putting their
	 * reply into "outbuf". If they leave it as NULL, they take of it
	 * themselves, possibly later.
	 *
	 * If async handling is wanted, the reply_xxx routine must make sure
	 * that it talloc_move()s the smb_req somewhere else.
	 */
	uint8 *outbuf;

	size_t unread_bytes;
	bool encrypted;
	connection_struct *conn;
	struct smbd_server_connection *sconn;
	struct smb_perfcount_data pcd;

	/*
	 * Chained request handling
	 */
	struct files_struct *chain_fsp;

	/*
	 * Here we collect the outbufs from the chain handlers
	 */
	uint8_t *chain_outbuf;

	/*
	 * state information for async smb handling
	 */
	void *async_priv;

	bool done;

	/*
	 * Back pointer to smb2 request.
	 */
	struct smbd_smb2_request *smb2req;
};

/* Defines for the sent_oplock_break field above. */
#define NO_BREAK_SENT 0
#define BREAK_TO_NONE_SENT 1
#define LEVEL_II_BREAK_SENT 2

typedef struct {
	fstring smb_name; /* user name from the client */
	fstring unix_name; /* unix user name of a validated user */
	fstring domain; /* domain that the client specified */
} userdom_struct;

/* used for server information: client, nameserv and ipc */
struct server_info_struct {
	fstring name;
	uint32 type;
	fstring comment;
	fstring domain; /* used ONLY in ipc.c NOT namework.c */
	bool server_added; /* used ONLY in ipc.c NOT namework.c */
};

/* used for network interfaces */
struct interface {
	struct interface *next, *prev;
	char *name;
	int flags;
	struct sockaddr_storage ip;
	struct sockaddr_storage netmask;
	struct sockaddr_storage bcast;
};

/* Internal message queue for deferred opens. */
struct pending_message_list {
	struct pending_message_list *next, *prev;
	struct timeval request_time; /* When was this first issued? */
	struct timed_event *te;
	struct smb_perfcount_data pcd;
	uint32_t seqnum;
	bool encrypted;
	bool processed;
	DATA_BLOB buf;
	DATA_BLOB private_data;
};

#define SHARE_MODE_FLAG_POSIX_OPEN	0x1

#include "librpc/gen_ndr/server_id.h"

/* struct returned by get_share_modes */
struct share_mode_entry {
	struct server_id pid;
	uint64_t op_mid;	/* For compatibility with SMB2 opens. */
	uint16 op_type;
	uint32 access_mask;		/* NTCreateX access bits (FILE_READ_DATA etc.) */
	uint32 share_access;		/* NTCreateX share constants (FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE). */
	uint32 private_options;	/* NT Create options, but we only look at
				 * NTCREATEX_OPTIONS_PRIVATE_DENY_DOS and
				 * NTCREATEX_OPTIONS_PRIVATE_DENY_FCB for
				 * smbstatus and swat */
	struct timeval time;
	struct file_id id;
	unsigned long share_file_id;
	uint32 uid;		/* uid of file opener. */
	uint16 flags;		/* See SHARE_MODE_XX above. */
	uint32_t name_hash;		/* Jenkins hash of full pathname. */
};

/* oplock break message definition - linearization of share_mode_entry.

Offset  Data			length.
0	struct server_id pid	4
4	uint16 op_mid		8
12	uint16 op_type		2
14	uint32 access_mask	4
18	uint32 share_access	4
22	uint32 private_options	4
26	uint32 time sec		4
30	uint32 time usec	4
34	uint64 dev		8 bytes
42	uint64 inode		8 bytes
50	uint64 extid		8 bytes
58	unsigned long file_id	4 bytes
62	uint32 uid		4 bytes
66	uint16 flags		2 bytes
68	uint32 name_hash	4 bytes
72

*/

#define OP_BREAK_MSG_PID_OFFSET 0
#define OP_BREAK_MSG_MID_OFFSET 4
#define OP_BREAK_MSG_OP_TYPE_OFFSET 12
#define OP_BREAK_MSG_ACCESS_MASK_OFFSET 14
#define OP_BREAK_MSG_SHARE_ACCESS_OFFSET 18
#define OP_BREAK_MSG_PRIV_OFFSET 22
#define OP_BREAK_MSG_TIME_SEC_OFFSET 26
#define OP_BREAK_MSG_TIME_USEC_OFFSET 30
#define OP_BREAK_MSG_DEV_OFFSET 34
#define OP_BREAK_MSG_INO_OFFSET 42
#define OP_BREAK_MSG_EXTID_OFFSET 50
#define OP_BREAK_MSG_FILE_ID_OFFSET 58
#define OP_BREAK_MSG_UID_OFFSET 62
#define OP_BREAK_MSG_FLAGS_OFFSET 66
#define OP_BREAK_MSG_NAME_HASH_OFFSET 68

#define OP_BREAK_MSG_VNN_OFFSET 72
#define MSG_SMB_SHARE_MODE_ENTRY_SIZE 76

struct delete_token_list {
	struct delete_token_list *next, *prev;
	uint32_t name_hash;
	struct security_unix_token *delete_token;
	struct security_token *delete_nt_token;
};

struct share_mode_lock {
	const char *servicepath; /* canonicalized. */
	const char *base_name;
	const char *stream_name;
	struct file_id id;
	int num_share_modes;
	struct share_mode_entry *share_modes;
	struct delete_token_list *delete_tokens;
	struct timespec old_write_time;
	struct timespec changed_write_time;
	bool fresh;
	bool modified;
	struct db_record *record;
};

/*
 * Internal structure of locking.tdb share mode db.
 * Used by locking.c and libsmbsharemodes.c
 */

struct locking_data {
	union {
		struct {
			int num_share_mode_entries;
			struct timespec old_write_time;
			struct timespec changed_write_time;
			uint32 num_delete_token_entries;
		} s;
		struct share_mode_entry dummy; /* Needed for alignment. */
	} u;
	/* The following four entries are implicit

	   (1) struct share_mode_entry modes[num_share_mode_entries];

	   (2) A num_delete_token_entries of structs {
		uint32_t len_delete_token;
		char unix_token[len_delete_token] (divisible by 4).
	   };

	   (3) char share_name[];
	   (4) char file_name[];
        */
};

#define NT_HASH_LEN 16
#define LM_HASH_LEN 16

/* key and data in the connections database - used in smbstatus and smbd */
struct connections_key {
	struct server_id pid;
	int cnum;
	fstring name;
};

struct connections_data {
	int magic;
	struct server_id pid;
	int cnum;
	uid_t uid;
	gid_t gid;
	char servicename[FSTRING_LEN];
	char addr[24];
	char machine[FSTRING_LEN];
	time_t start;

	/*
	 * This field used to hold the msg_flags. For compatibility reasons,
	 * keep the data structure in the tdb file the same.
	 */
	uint32 unused_compatitibility_field;
};

/* the following are used by loadparm for option lists */
typedef enum {
	P_BOOL,P_BOOLREV,P_CHAR,P_INTEGER,P_OCTAL,P_LIST,
	P_STRING,P_USTRING,P_ENUM,P_SEP
} parm_type;

typedef enum {
	P_LOCAL,P_GLOBAL,P_SEPARATOR,P_NONE
} parm_class;

struct enum_list {
	int value;
	const char *name;
};

struct parm_struct {
	const char *label;
	parm_type type;
	parm_class p_class;
	void *ptr;
	bool (*special)(int snum, const char *, char **);
	const struct enum_list *enum_list;
	unsigned flags;
	union {
		bool bvalue;
		int ivalue;
		char *svalue;
		char cvalue;
		char **lvalue;
	} def;
};

/* The following flags are used in SWAT */
#define FLAG_BASIC 	0x0001 /* Display only in BASIC view */
#define FLAG_SHARE 	0x0002 /* file sharing options */
#define FLAG_PRINT 	0x0004 /* printing options */
#define FLAG_GLOBAL 	0x0008 /* local options that should be globally settable in SWAT */
#define FLAG_WIZARD 	0x0010 /* Parameters that the wizard will operate on */
#define FLAG_ADVANCED 	0x0020 /* Parameters that will be visible in advanced view */
#define FLAG_DEVELOPER 	0x0040 /* No longer used */
#define FLAG_DEPRECATED 0x1000 /* options that should no longer be used */
#define FLAG_HIDE  	0x2000 /* options that should be hidden in SWAT */
#define FLAG_DOS_STRING 0x4000 /* convert from UNIX to DOS codepage when reading this string. */
#define FLAG_META	0x8000 /* A meta directive - not a real parameter */
#define FLAG_CMDLINE	0x10000 /* option has been overridden */

/* offsets into message for common items */
#define smb_com 8
#define smb_rcls 9
#define smb_reh 10
#define smb_err 11
#define smb_flg 13
#define smb_flg2 14
#define smb_pidhigh 16
#define smb_ss_field 18
#define smb_tid 28
#define smb_pid 30
#define smb_uid 32
#define smb_mid 34
#define smb_wct 36
#define smb_vwv 37
#define smb_vwv0 37
#define smb_vwv1 39
#define smb_vwv2 41
#define smb_vwv3 43
#define smb_vwv4 45
#define smb_vwv5 47
#define smb_vwv6 49
#define smb_vwv7 51
#define smb_vwv8 53
#define smb_vwv9 55
#define smb_vwv10 57
#define smb_vwv11 59
#define smb_vwv12 61
#define smb_vwv13 63
#define smb_vwv14 65
#define smb_vwv15 67
#define smb_vwv16 69
#define smb_vwv17 71

/* flag defines. CIFS spec 3.1.1 */
#define FLAG_SUPPORT_LOCKREAD       0x01
#define FLAG_CLIENT_BUF_AVAIL       0x02
#define FLAG_RESERVED               0x04
#define FLAG_CASELESS_PATHNAMES     0x08
#define FLAG_CANONICAL_PATHNAMES    0x10
#define FLAG_REQUEST_OPLOCK         0x20
#define FLAG_REQUEST_BATCH_OPLOCK   0x40
#define FLAG_REPLY                  0x80

/* the complete */
#define SMBmkdir      0x00   /* create directory */
#define SMBrmdir      0x01   /* delete directory */
#define SMBopen       0x02   /* open file */
#define SMBcreate     0x03   /* create file */
#define SMBclose      0x04   /* close file */
#define SMBflush      0x05   /* flush file */
#define SMBunlink     0x06   /* delete file */
#define SMBmv         0x07   /* rename file */
#define SMBgetatr     0x08   /* get file attributes */
#define SMBsetatr     0x09   /* set file attributes */
#define SMBread       0x0A   /* read from file */
#define SMBwrite      0x0B   /* write to file */
#define SMBlock       0x0C   /* lock byte range */
#define SMBunlock     0x0D   /* unlock byte range */
#define SMBctemp      0x0E   /* create temporary file */
#define SMBmknew      0x0F   /* make new file */
#define SMBcheckpath  0x10   /* check directory path */
#define SMBexit       0x11   /* process exit */
#define SMBlseek      0x12   /* seek */
#define SMBtcon       0x70   /* tree connect */
#define SMBtconX      0x75   /* tree connect and X*/
#define SMBtdis       0x71   /* tree disconnect */
#define SMBnegprot    0x72   /* negotiate protocol */
#define SMBdskattr    0x80   /* get disk attributes */
#define SMBsearch     0x81   /* search directory */
#define SMBsplopen    0xC0   /* open print spool file */
#define SMBsplwr      0xC1   /* write to print spool file */
#define SMBsplclose   0xC2   /* close print spool file */
#define SMBsplretq    0xC3   /* return print queue */
#define SMBsends      0xD0   /* send single block message */
#define SMBsendb      0xD1   /* send broadcast message */
#define SMBfwdname    0xD2   /* forward user name */
#define SMBcancelf    0xD3   /* cancel forward */
#define SMBgetmac     0xD4   /* get machine name */
#define SMBsendstrt   0xD5   /* send start of multi-block message */
#define SMBsendend    0xD6   /* send end of multi-block message */
#define SMBsendtxt    0xD7   /* send text of multi-block message */

/* Core+ protocol */
#define SMBlockread	  0x13   /* Lock a range and read */
#define SMBwriteunlock 0x14 /* Unlock a range then write */
#define SMBreadbraw   0x1a  /* read a block of data with no smb header */
#define SMBwritebraw  0x1d  /* write a block of data with no smb header */
#define SMBwritec     0x20  /* secondary write request */
#define SMBwriteclose 0x2c  /* write a file then close it */

/* dos extended protocol */
#define SMBreadBraw      0x1A   /* read block raw */
#define SMBreadBmpx      0x1B   /* read block multiplexed */
#define SMBreadBs        0x1C   /* read block (secondary response) */
#define SMBwriteBraw     0x1D   /* write block raw */
#define SMBwriteBmpx     0x1E   /* write block multiplexed */
#define SMBwriteBs       0x1F   /* write block (secondary request) */
#define SMBwriteC        0x20   /* write complete response */
#define SMBsetattrE      0x22   /* set file attributes expanded */
#define SMBgetattrE      0x23   /* get file attributes expanded */
#define SMBlockingX      0x24   /* lock/unlock byte ranges and X */
#define SMBtrans         0x25   /* transaction - name, bytes in/out */
#define SMBtranss        0x26   /* transaction (secondary request/response) */
#define SMBioctl         0x27   /* IOCTL */
#define SMBioctls        0x28   /* IOCTL  (secondary request/response) */
#define SMBcopy          0x29   /* copy */
#define SMBmove          0x2A   /* move */
#define SMBecho          0x2B   /* echo */
#define SMBopenX         0x2D   /* open and X */
#define SMBreadX         0x2E   /* read and X */
#define SMBwriteX        0x2F   /* write and X */
#define SMBsesssetupX    0x73   /* Session Set Up & X (including User Logon) */
#define SMBffirst        0x82   /* find first */
#define SMBfunique       0x83   /* find unique */
#define SMBfclose        0x84   /* find close */
#define SMBkeepalive     0x85   /* keepalive */
#define SMBinvalid       0xFE   /* invalid command */

/* Extended 2.0 protocol */
#define SMBtrans2        0x32   /* TRANS2 protocol set */
#define SMBtranss2       0x33   /* TRANS2 protocol set, secondary command */
#define SMBfindclose     0x34   /* Terminate a TRANSACT2_FINDFIRST */
#define SMBfindnclose    0x35   /* Terminate a TRANSACT2_FINDNOTIFYFIRST */
#define SMBulogoffX      0x74   /* user logoff */

/* NT SMB extensions. */
#define SMBnttrans       0xA0   /* NT transact */
#define SMBnttranss      0xA1   /* NT transact secondary */
#define SMBntcreateX     0xA2   /* NT create and X */
#define SMBntcancel      0xA4   /* NT cancel */
#define SMBntrename      0xA5   /* NT rename */

/* These are the trans subcommands */
#define TRANSACT_SETNAMEDPIPEHANDLESTATE  0x01 
#define TRANSACT_DCERPCCMD                0x26
#define TRANSACT_WAITNAMEDPIPEHANDLESTATE 0x53

/* These are the TRANS2 sub commands */
#define TRANSACT2_OPEN				0x00
#define TRANSACT2_FINDFIRST			0x01
#define TRANSACT2_FINDNEXT			0x02
#define TRANSACT2_QFSINFO			0x03
#define TRANSACT2_SETFSINFO			0x04
#define TRANSACT2_QPATHINFO			0x05
#define TRANSACT2_SETPATHINFO			0x06
#define TRANSACT2_QFILEINFO			0x07
#define TRANSACT2_SETFILEINFO			0x08
#define TRANSACT2_FSCTL				0x09
#define TRANSACT2_IOCTL				0x0A
#define TRANSACT2_FINDNOTIFYFIRST		0x0B
#define TRANSACT2_FINDNOTIFYNEXT		0x0C
#define TRANSACT2_MKDIR				0x0D
#define TRANSACT2_SESSION_SETUP			0x0E
#define TRANSACT2_GET_DFS_REFERRAL		0x10
#define TRANSACT2_REPORT_DFS_INCONSISTANCY	0x11

/* These are the NT transact sub commands. */
#define NT_TRANSACT_CREATE                1
#define NT_TRANSACT_IOCTL                 2
#define NT_TRANSACT_SET_SECURITY_DESC     3
#define NT_TRANSACT_NOTIFY_CHANGE         4
#define NT_TRANSACT_RENAME                5
#define NT_TRANSACT_QUERY_SECURITY_DESC   6
#define NT_TRANSACT_GET_USER_QUOTA	  7
#define NT_TRANSACT_SET_USER_QUOTA	  8

/* These are the NT transact_get_user_quota sub commands */
#define TRANSACT_GET_USER_QUOTA_LIST_CONTINUE	0x0000
#define TRANSACT_GET_USER_QUOTA_LIST_START	0x0100
#define TRANSACT_GET_USER_QUOTA_FOR_SID		0x0101

/* Relevant IOCTL codes */
#define IOCTL_QUERY_JOB_INFO      0x530060

/* these are the trans2 sub fields for primary requests */
#define smb_tpscnt smb_vwv0
#define smb_tdscnt smb_vwv1
#define smb_mprcnt smb_vwv2
#define smb_mdrcnt smb_vwv3
#define smb_msrcnt smb_vwv4
#define smb_flags smb_vwv5
#define smb_timeout smb_vwv6
#define smb_pscnt smb_vwv9
#define smb_psoff smb_vwv10
#define smb_dscnt smb_vwv11
#define smb_dsoff smb_vwv12
#define smb_suwcnt smb_vwv13
#define smb_setup smb_vwv14
#define smb_setup0 smb_setup
#define smb_setup1 (smb_setup+2)
#define smb_setup2 (smb_setup+4)

/* these are for the secondary requests */
#define smb_spscnt smb_vwv2
#define smb_spsoff smb_vwv3
#define smb_spsdisp smb_vwv4
#define smb_sdscnt smb_vwv5
#define smb_sdsoff smb_vwv6
#define smb_sdsdisp smb_vwv7
#define smb_sfid smb_vwv8

/* and these for responses */
#define smb_tprcnt smb_vwv0
#define smb_tdrcnt smb_vwv1
#define smb_prcnt smb_vwv3
#define smb_proff smb_vwv4
#define smb_prdisp smb_vwv5
#define smb_drcnt smb_vwv6
#define smb_droff smb_vwv7
#define smb_drdisp smb_vwv8

/* these are for the NT trans primary request. */
#define smb_nt_MaxSetupCount smb_vwv0
#define smb_nt_Flags (smb_vwv0 + 1)
#define smb_nt_TotalParameterCount (smb_vwv0 + 3)
#define smb_nt_TotalDataCount (smb_vwv0 + 7)
#define smb_nt_MaxParameterCount (smb_vwv0 + 11)
#define smb_nt_MaxDataCount (smb_vwv0 + 15)
#define smb_nt_ParameterCount (smb_vwv0 + 19)
#define smb_nt_ParameterOffset (smb_vwv0 + 23)
#define smb_nt_DataCount (smb_vwv0 + 27)
#define smb_nt_DataOffset (smb_vwv0 + 31)
#define smb_nt_SetupCount (smb_vwv0 + 35)
#define smb_nt_Function (smb_vwv0 + 36)
#define smb_nt_SetupStart (smb_vwv0 + 38)

/* these are for the NT trans secondary request. */
#define smb_nts_TotalParameterCount (smb_vwv0 + 3)
#define smb_nts_TotalDataCount (smb_vwv0 + 7)
#define smb_nts_ParameterCount (smb_vwv0 + 11)
#define smb_nts_ParameterOffset (smb_vwv0 + 15)
#define smb_nts_ParameterDisplacement (smb_vwv0 + 19)
#define smb_nts_DataCount (smb_vwv0 + 23)
#define smb_nts_DataOffset (smb_vwv0 + 27)
#define smb_nts_DataDisplacement (smb_vwv0 + 31)

/* these are for the NT trans reply. */
#define smb_ntr_TotalParameterCount (smb_vwv0 + 3)
#define smb_ntr_TotalDataCount (smb_vwv0 + 7)
#define smb_ntr_ParameterCount (smb_vwv0 + 11)
#define smb_ntr_ParameterOffset (smb_vwv0 + 15)
#define smb_ntr_ParameterDisplacement (smb_vwv0 + 19)
#define smb_ntr_DataCount (smb_vwv0 + 23)
#define smb_ntr_DataOffset (smb_vwv0 + 27)
#define smb_ntr_DataDisplacement (smb_vwv0 + 31)

/* these are for the NT create_and_X */
#define smb_ntcreate_NameLength (smb_vwv0 + 5)
#define smb_ntcreate_Flags (smb_vwv0 + 7)
#define smb_ntcreate_RootDirectoryFid (smb_vwv0 + 11)
#define smb_ntcreate_DesiredAccess (smb_vwv0 + 15)
#define smb_ntcreate_AllocationSize (smb_vwv0 + 19)
#define smb_ntcreate_FileAttributes (smb_vwv0 + 27)
#define smb_ntcreate_ShareAccess (smb_vwv0 + 31)
#define smb_ntcreate_CreateDisposition (smb_vwv0 + 35)
#define smb_ntcreate_CreateOptions (smb_vwv0 + 39)
#define smb_ntcreate_ImpersonationLevel (smb_vwv0 + 43)
#define smb_ntcreate_SecurityFlags (smb_vwv0 + 47)

/* this is used on a TConX. I'm not sure the name is very helpful though */
#define SMB_SUPPORT_SEARCH_BITS        0x0001
#define SMB_SHARE_IN_DFS               0x0002

/* Named pipe write mode flags. Used in writeX calls. */
#define PIPE_RAW_MODE 0x4
#define PIPE_START_MESSAGE 0x8

/* the desired access to use when opening a pipe */
#define DESIRED_ACCESS_PIPE 0x2019f
 
/* Mapping of access rights to UNIX perms. */
#define UNIX_ACCESS_RWX		FILE_GENERIC_ALL
#define UNIX_ACCESS_R 		FILE_GENERIC_READ
#define UNIX_ACCESS_W		FILE_GENERIC_WRITE
#define UNIX_ACCESS_X		FILE_GENERIC_EXECUTE

/* Mapping of access rights to UNIX perms. for a UNIX directory. */
#define UNIX_DIRECTORY_ACCESS_RWX		FILE_GENERIC_ALL
#define UNIX_DIRECTORY_ACCESS_R 		FILE_GENERIC_READ
#define UNIX_DIRECTORY_ACCESS_W			(FILE_GENERIC_WRITE|FILE_DELETE_CHILD)
#define UNIX_DIRECTORY_ACCESS_X			FILE_GENERIC_EXECUTE

#if 0
/*
 * This is the old mapping we used to use. To get W2KSP2 profiles
 * working we need to map to the canonical file perms.
 */
#define UNIX_ACCESS_RWX (UNIX_ACCESS_R|UNIX_ACCESS_W|UNIX_ACCESS_X)
#define UNIX_ACCESS_R (READ_CONTROL_ACCESS|SYNCHRONIZE_ACCESS|\
			FILE_READ_ATTRIBUTES|FILE_READ_EA|FILE_READ_DATA)
#define UNIX_ACCESS_W (READ_CONTROL_ACCESS|SYNCHRONIZE_ACCESS|\
			FILE_WRITE_ATTRIBUTES|FILE_WRITE_EA|\
			FILE_APPEND_DATA|FILE_WRITE_DATA)
#define UNIX_ACCESS_X (READ_CONTROL_ACCESS|SYNCHRONIZE_ACCESS|\
			FILE_EXECUTE|FILE_READ_ATTRIBUTES)
#endif

#define UNIX_ACCESS_NONE (WRITE_OWNER_ACCESS)

/* Flags field. */
#define REQUEST_OPLOCK 2
#define REQUEST_BATCH_OPLOCK 4
#define OPEN_DIRECTORY 8
#define EXTENDED_RESPONSE_REQUIRED 0x10

/* ShareAccess field. */
#define FILE_SHARE_NONE 0 /* Cannot be used in bitmask. */
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define FILE_SHARE_DELETE 4

/* FileAttributesField */
#define FILE_ATTRIBUTE_READONLY		0x001L
#define FILE_ATTRIBUTE_HIDDEN		0x002L
#define FILE_ATTRIBUTE_SYSTEM		0x004L
#define FILE_ATTRIBUTE_VOLUME		0x008L
#define FILE_ATTRIBUTE_DIRECTORY	0x010L
#define FILE_ATTRIBUTE_ARCHIVE		0x020L
#define FILE_ATTRIBUTE_NORMAL		0x080L
#define FILE_ATTRIBUTE_TEMPORARY	0x100L
#define FILE_ATTRIBUTE_SPARSE		0x200L
#define FILE_ATTRIBUTE_REPARSE_POINT    0x400L
#define FILE_ATTRIBUTE_COMPRESSED	0x800L
#define FILE_ATTRIBUTE_OFFLINE          0x1000L
#define FILE_ATTRIBUTE_NONINDEXED	0x2000L
#define FILE_ATTRIBUTE_ENCRYPTED        0x4000L
#define SAMBA_ATTRIBUTES_MASK		(FILE_ATTRIBUTE_READONLY|\
					FILE_ATTRIBUTE_HIDDEN|\
					FILE_ATTRIBUTE_SYSTEM|\
					FILE_ATTRIBUTE_DIRECTORY|\
					FILE_ATTRIBUTE_ARCHIVE)

/* Flags - combined with attributes. */
#define FILE_FLAG_WRITE_THROUGH    0x80000000L
#define FILE_FLAG_NO_BUFFERING     0x20000000L
#define FILE_FLAG_RANDOM_ACCESS    0x10000000L
#define FILE_FLAG_SEQUENTIAL_SCAN  0x08000000L
#define FILE_FLAG_DELETE_ON_CLOSE  0x04000000L
#define FILE_FLAG_BACKUP_SEMANTICS 0x02000000L
#define FILE_FLAG_POSIX_SEMANTICS  0x01000000L

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

#define NTCREATEX_OPTIONS_MUST_IGNORE_MASK      (0x008F0480)

#define NTCREATEX_OPTIONS_INVALID_PARAM_MASK    (0xFF100030)

/*
 * Private create options used by the ntcreatex processing code. From Samba4.
 * We reuse some ignored flags for private use. Passed in the private_flags
 * argument.
 */
#define NTCREATEX_OPTIONS_PRIVATE_DENY_DOS     0x0001
#define NTCREATEX_OPTIONS_PRIVATE_DENY_FCB     0x0002

/* Private options for streams support */
#define NTCREATEX_OPTIONS_PRIVATE_STREAM_DELETE 0x0004

/* Private options for printer support */
#define NTCREATEX_OPTIONS_PRIVATE_DELETE_ON_CLOSE 0x0008

/* Responses when opening a file. */
#define FILE_WAS_SUPERSEDED 0
#define FILE_WAS_OPENED 1
#define FILE_WAS_CREATED 2
#define FILE_WAS_OVERWRITTEN 3

/* File type flags */
#define FILE_TYPE_DISK  0
#define FILE_TYPE_BYTE_MODE_PIPE 1
#define FILE_TYPE_MESSAGE_MODE_PIPE 2
#define FILE_TYPE_PRINTER 3
#define FILE_TYPE_COMM_DEVICE 4
#define FILE_TYPE_UNKNOWN 0xFFFF

/* Flag for NT transact rename call. */
#define RENAME_REPLACE_IF_EXISTS 1

/* flags for SMBntrename call (from Samba4) */
#define RENAME_FLAG_MOVE_CLUSTER_INFORMATION 0x102 /* ???? */
#define RENAME_FLAG_HARD_LINK                0x103
#define RENAME_FLAG_RENAME                   0x104
#define RENAME_FLAG_COPY                     0x105

/* Filesystem Attributes. */
#define FILE_CASE_SENSITIVE_SEARCH      0x00000001
#define FILE_CASE_PRESERVED_NAMES       0x00000002
#define FILE_UNICODE_ON_DISK            0x00000004
/* According to cifs9f, this is 4, not 8 */
/* Acconding to testing, this actually sets the security attribute! */
#define FILE_PERSISTENT_ACLS            0x00000008
#define FILE_FILE_COMPRESSION           0x00000010
#define FILE_VOLUME_QUOTAS              0x00000020
#define FILE_SUPPORTS_SPARSE_FILES      0x00000040
#define FILE_SUPPORTS_REPARSE_POINTS    0x00000080
#define FILE_SUPPORTS_REMOTE_STORAGE    0x00000100
#define FS_LFN_APIS                     0x00004000
#define FILE_VOLUME_IS_COMPRESSED       0x00008000
#define FILE_SUPPORTS_OBJECT_IDS        0x00010000
#define FILE_SUPPORTS_ENCRYPTION        0x00020000
#define FILE_NAMED_STREAMS              0x00040000
#define FILE_READ_ONLY_VOLUME           0x00080000

/* ChangeNotify flags. */
#define FILE_NOTIFY_CHANGE_FILE_NAME   0x001
#define FILE_NOTIFY_CHANGE_DIR_NAME    0x002
#define FILE_NOTIFY_CHANGE_ATTRIBUTES  0x004
#define FILE_NOTIFY_CHANGE_SIZE        0x008
#define FILE_NOTIFY_CHANGE_LAST_WRITE  0x010
#define FILE_NOTIFY_CHANGE_LAST_ACCESS 0x020
#define FILE_NOTIFY_CHANGE_CREATION    0x040
#define FILE_NOTIFY_CHANGE_EA          0x080
#define FILE_NOTIFY_CHANGE_SECURITY    0x100
#define FILE_NOTIFY_CHANGE_STREAM_NAME	0x00000200
#define FILE_NOTIFY_CHANGE_STREAM_SIZE	0x00000400
#define FILE_NOTIFY_CHANGE_STREAM_WRITE	0x00000800

#define FILE_NOTIFY_CHANGE_NAME \
	(FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME)

/* change notify action results */
#define NOTIFY_ACTION_ADDED 1
#define NOTIFY_ACTION_REMOVED 2
#define NOTIFY_ACTION_MODIFIED 3
#define NOTIFY_ACTION_OLD_NAME 4
#define NOTIFY_ACTION_NEW_NAME 5
#define NOTIFY_ACTION_ADDED_STREAM 6
#define NOTIFY_ACTION_REMOVED_STREAM 7
#define NOTIFY_ACTION_MODIFIED_STREAM 8


/* where to find the base of the SMB packet proper */
#define smb_base(buf) (((char *)(buf))+4)

/* we don't allow server strings to be longer than 48 characters as
   otherwise NT will not honour the announce packets */
#define MAX_SERVER_STRING_LENGTH 48


#define SMB_SUCCESS 0  /* The request was successful. */

#ifdef WITH_DFS
void dfs_unlogin(void);
extern int dcelogin_atmost_once;
#endif

#ifdef NOSTRDUP
char *strdup(char *s);
#endif

#ifndef SELECT_CAST
#define SELECT_CAST
#endif

/* This was set by JHT in liaison with Jeremy Allison early 1997
 * History:
 * Version 4.0 - never made public
 * Version 4.10 - New to 1.9.16p2, lost in space 1.9.16p3 to 1.9.16p9
 *              - Reappeared in 1.9.16p11 with fixed smbd services
 * Version 4.20 - To indicate that nmbd and browsing now works better
 * Version 4.50 - Set at release of samba-2.2.0 by JHT
 *
 *  Note: In the presence of NT4.X do not set above 4.9
 *        Setting this above 4.9 can have undesired side-effects.
 *        This may change again in Samba-3.0 after further testing. JHT
 */
 
#define DEFAULT_MAJOR_VERSION 0x04
#define DEFAULT_MINOR_VERSION 0x09

/* Browser Election Values */
#define BROWSER_ELECTION_VERSION	0x010f
#define BROWSER_CONSTANT	0xaa55

/* Sercurity mode bits. */
#define NEGOTIATE_SECURITY_USER_LEVEL		0x01
#define NEGOTIATE_SECURITY_CHALLENGE_RESPONSE	0x02
#define NEGOTIATE_SECURITY_SIGNATURES_ENABLED	0x04
#define NEGOTIATE_SECURITY_SIGNATURES_REQUIRED	0x08

/* NT Flags2 bits - cifs6.txt section 3.1.2 */
   
#define FLAGS2_LONG_PATH_COMPONENTS    0x0001
#define FLAGS2_EXTENDED_ATTRIBUTES     0x0002
#define FLAGS2_SMB_SECURITY_SIGNATURES 0x0004
#define FLAGS2_UNKNOWN_BIT4            0x0010
#define FLAGS2_IS_LONG_NAME            0x0040
#define FLAGS2_EXTENDED_SECURITY       0x0800 
#define FLAGS2_DFS_PATHNAMES           0x1000
#define FLAGS2_READ_PERMIT_EXECUTE     0x2000
#define FLAGS2_32_BIT_ERROR_CODES      0x4000 
#define FLAGS2_UNICODE_STRINGS         0x8000

#define FLAGS2_WIN2K_SIGNATURE         0xC852 /* Hack alert ! For now... JRA. */

/* TCONX Flag (smb_vwv2). */
#define TCONX_FLAG_EXTENDED_RESPONSE	0x8

/* File Status Flags. See:

http://msdn.microsoft.com/en-us/library/cc246334(PROT.13).aspx
*/

#define NO_EAS			0x1
#define NO_SUBSTREAMS		0x2
#define NO_REPARSETAG		0x4

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

/* security levels */
enum security_types {SEC_SHARE,SEC_USER,SEC_SERVER,SEC_DOMAIN,SEC_ADS};

/* server roles */
enum server_types {
	ROLE_STANDALONE,
	ROLE_DOMAIN_MEMBER,
	ROLE_DOMAIN_BDC,
	ROLE_DOMAIN_PDC
};

/* printing types */
enum printing_types {PRINT_BSD,PRINT_SYSV,PRINT_AIX,PRINT_HPUX,
		     PRINT_QNX,PRINT_PLP,PRINT_LPRNG,PRINT_SOFTQ,
		     PRINT_CUPS,PRINT_LPRNT,PRINT_LPROS2,PRINT_IPRINT
#if defined(DEVELOPER) || defined(ENABLE_BUILD_FARM_HACKS)
,PRINT_TEST,PRINT_VLP
#endif /* DEVELOPER */
};

/* LDAP SSL options */
enum ldap_ssl_types {LDAP_SSL_OFF, LDAP_SSL_START_TLS};

/* LDAP PASSWD SYNC methods */
enum ldap_passwd_sync_types {LDAP_PASSWD_SYNC_ON, LDAP_PASSWD_SYNC_OFF, LDAP_PASSWD_SYNC_ONLY};

/*
 * This should be under the HAVE_KRB5 flag but since they're used
 * in lp_kerberos_method(), they ned to be always available
 * If you add any entries to KERBEROS_VERIFY defines, please modify USE.*KEYTAB macros
 * so they remain accurate.
 */

#define KERBEROS_VERIFY_SECRETS 0
#define KERBEROS_VERIFY_SYSTEM_KEYTAB 1
#define KERBEROS_VERIFY_DEDICATED_KEYTAB 2
#define KERBEROS_VERIFY_SECRETS_AND_KEYTAB 3

/* Remote architectures we know about. */
enum remote_arch_types {RA_UNKNOWN, RA_WFWG, RA_OS2, RA_WIN95, RA_WINNT,
			RA_WIN2K, RA_WINXP, RA_WIN2K3, RA_VISTA,
			RA_SAMBA, RA_CIFSFS, RA_WINXP64, RA_OSX};

/* case handling */
enum case_handling {CASE_LOWER,CASE_UPPER};

/* ACL compatibility */
enum acl_compatibility {ACL_COMPAT_AUTO, ACL_COMPAT_WINNT, ACL_COMPAT_WIN2K};
/*
 * Global value meaing that the smb_uid field should be
 * ingored (in share level security and protocol level == CORE)
 */

#define UID_FIELD_INVALID 0
#define VUID_OFFSET 100 /* Amount to bias returned vuid numbers */

/* 
 * Size of buffer to use when moving files across filesystems. 
 */
#define COPYBUF_SIZE (8*1024)

/*
 * Map the Core and Extended Oplock requesst bits down
 * to common bits (EXCLUSIVE_OPLOCK & BATCH_OPLOCK).
 */

/*
 * Core protocol.
 */
#define CORE_OPLOCK_REQUEST(inbuf) \
    ((CVAL(inbuf,smb_flg)&(FLAG_REQUEST_OPLOCK|FLAG_REQUEST_BATCH_OPLOCK))>>5)

/*
 * Extended protocol.
 */
#define EXTENDED_OPLOCK_REQUEST(inbuf) ((SVAL(inbuf,smb_vwv2)&((1<<1)|(1<<2)))>>1)

/* Lock types. */
#define LOCKING_ANDX_SHARED_LOCK 0x1
#define LOCKING_ANDX_OPLOCK_RELEASE 0x2
#define LOCKING_ANDX_CHANGE_LOCKTYPE 0x4
#define LOCKING_ANDX_CANCEL_LOCK 0x8
#define LOCKING_ANDX_LARGE_FILES 0x10

/*
 * Bits we test with.
 * Note these must fit into 16-bits.
 */

#define NO_OPLOCK 			0x0
#define EXCLUSIVE_OPLOCK 		0x1
#define BATCH_OPLOCK 			0x2
#define LEVEL_II_OPLOCK 		0x4

/* The following are Samba-private. */
#define INTERNAL_OPEN_ONLY 		0x8
#define FAKE_LEVEL_II_OPLOCK 		0x10	/* Client requested no_oplock, but we have to
				 * inform potential level2 holders on
				 * write. */
#define DEFERRED_OPEN_ENTRY 		0x20
#define UNUSED_SHARE_MODE_ENTRY 	0x40
#define FORCE_OPLOCK_BREAK_TO_NONE 	0x80

/* None of the following should ever appear in fsp->oplock_request. */
#define SAMBA_PRIVATE_OPLOCK_MASK (INTERNAL_OPEN_ONLY|DEFERRED_OPEN_ENTRY|UNUSED_SHARE_MODE_ENTRY|FORCE_OPLOCK_BREAK_TO_NONE)

#define EXCLUSIVE_OPLOCK_TYPE(lck) ((lck) & ((unsigned int)EXCLUSIVE_OPLOCK|(unsigned int)BATCH_OPLOCK))
#define BATCH_OPLOCK_TYPE(lck) ((lck) & (unsigned int)BATCH_OPLOCK)
#define LEVEL_II_OPLOCK_TYPE(lck) ((lck) & ((unsigned int)LEVEL_II_OPLOCK|(unsigned int)FAKE_LEVEL_II_OPLOCK))

/* kernel_oplock_message definition.

struct kernel_oplock_message {
	uint64_t dev;
	uint64_t inode;
	unit64_t extid;
	unsigned long file_id;
};

Offset  Data                  length.
0     uint64_t dev            8 bytes
8     uint64_t inode          8 bytes
16    uint64_t extid          8 bytes
24    unsigned long file_id   4 bytes
28

*/
#define MSG_SMB_KERNEL_BREAK_SIZE 28

/* file_renamed_message definition.

struct file_renamed_message {
	uint64_t dev;
	uint64_t inode;
	char names[1]; A variable area containing sharepath and filename.
};

Offset  Data			length.
0	uint64_t dev		8 bytes
8	uint64_t inode		8 bytes
16      unit64_t extid          8 bytes
24	char [] name		zero terminated namelen bytes
minimum length == 24.

*/

#define MSG_FILE_RENAMED_MIN_SIZE 24

/*
 * On the wire return values for oplock types.
 */

#define CORE_OPLOCK_GRANTED (1<<5)
#define EXTENDED_OPLOCK_GRANTED (1<<15)

#define NO_OPLOCK_RETURN 0
#define EXCLUSIVE_OPLOCK_RETURN 1
#define BATCH_OPLOCK_RETURN 2
#define LEVEL_II_OPLOCK_RETURN 3

/* Oplock levels */
#define OPLOCKLEVEL_NONE 0
#define OPLOCKLEVEL_II 1

/*
 * Capabilities abstracted for different systems.
 */

enum smbd_capability {
    KERNEL_OPLOCK_CAPABILITY,
    DMAPI_ACCESS_CAPABILITY,
    LEASE_CAPABILITY
};

/*
 * Kernel oplocks capability flags.
 */

/* Level 2 oplocks are supported natively by kernel oplocks. */
#define KOPLOCKS_LEVEL2_SUPPORTED		0x1

/* The kernel notifies deferred openers when they can retry the open. */
#define KOPLOCKS_DEFERRED_OPEN_NOTIFICATION	0x2

/* The kernel notifies smbds when an oplock break times out. */
#define KOPLOCKS_TIMEOUT_NOTIFICATION		0x4

/* The kernel notifies smbds when an oplock is broken. */
#define KOPLOCKS_OPLOCK_BROKEN_NOTIFICATION	0x8

struct kernel_oplocks_ops;
struct kernel_oplocks {
	const struct kernel_oplocks_ops *ops;
	uint32_t flags;
	void *private_data;
};

enum level2_contention_type {
	LEVEL2_CONTEND_ALLOC_SHRINK,
	LEVEL2_CONTEND_ALLOC_GROW,
	LEVEL2_CONTEND_SET_FILE_LEN,
	LEVEL2_CONTEND_FILL_SPARSE,
	LEVEL2_CONTEND_WRITE,
	LEVEL2_CONTEND_WINDOWS_BRL,
	LEVEL2_CONTEND_POSIX_BRL
};

/* if a kernel does support oplocks then a structure of the following
   typee is used to describe how to interact with the kernel */
struct kernel_oplocks_ops {
	bool (*set_oplock)(struct kernel_oplocks *ctx,
			   files_struct *fsp, int oplock_type);
	void (*release_oplock)(struct kernel_oplocks *ctx,
			       files_struct *fsp, int oplock_type);
	void (*contend_level2_oplocks_begin)(files_struct *fsp,
					     enum level2_contention_type type);
	void (*contend_level2_oplocks_end)(files_struct *fsp,
					   enum level2_contention_type type);
};

#include "smb_macros.h"

#define MAX_NETBIOSNAME_LEN 16
/* DOS character, NetBIOS namestring. Type used on the wire. */
typedef char nstring[MAX_NETBIOSNAME_LEN];
/* Unix character, NetBIOS namestring. Type used to manipulate name in nmbd. */
typedef char unstring[MAX_NETBIOSNAME_LEN*4];

/* A netbios name structure. */
struct nmb_name {
	nstring      name;
	char         scope[64];
	unsigned int name_type;
};

/* A netbios node status array element. */
struct node_status {
	nstring name;
	unsigned char type;
	unsigned char flags;
};

/* The extra info from a NetBIOS node status query */
struct node_status_extra {
	unsigned char mac_addr[6];
	/* There really is more here ... */ 
};

typedef struct user_struct {
	struct user_struct *next, *prev;
	uint16 vuid; /* Tag for this entry. */

	char *session_keystr; /* used by utmp and pam session code.  
				 TDB key string */
	int homes_snum;

	struct auth_serversupplied_info *session_info;

	struct auth_ntlmssp_state *auth_ntlmssp_state;
} user_struct;

struct unix_error_map {
	int unix_error;
	int dos_class;
	int dos_code;
	NTSTATUS nt_error;
};

/*
   Do you want session setups at user level security with a invalid
   password to be rejected or allowed in as guest? WinNT rejects them
   but it can be a pain as it means "net view" needs to use a password

   You have 3 choices in the setting of map_to_guest:

   "NEVER_MAP_TO_GUEST" means session setups with an invalid password
   are rejected. This is the default.

   "MAP_TO_GUEST_ON_BAD_USER" means session setups with an invalid password
   are rejected, unless the username does not exist, in which case it
   is treated as a guest login

   "MAP_TO_GUEST_ON_BAD_PASSWORD" means session setups with an invalid password
   are treated as a guest login

   Note that map_to_guest only has an effect in user or server
   level security.
*/

#define NEVER_MAP_TO_GUEST 		0
#define MAP_TO_GUEST_ON_BAD_USER 	1
#define MAP_TO_GUEST_ON_BAD_PASSWORD 	2
#define MAP_TO_GUEST_ON_BAD_UID 	3

#define SAFE_NETBIOS_CHARS ". -_"

/* The maximum length of a trust account password.
   Used when we randomly create it, 15 char passwords
   exceed NT4's max password length */

#define DEFAULT_TRUST_ACCOUNT_PASSWORD_LENGTH 14

#define PORT_NONE	0
#ifndef LDAP_PORT
#define LDAP_PORT	389
#endif
#define LDAP_GC_PORT    3268

/* used by the IP comparison function */
struct ip_service {
	struct sockaddr_storage ss;
	unsigned port;
};

struct ea_struct {
	uint8 flags;
	char *name;
	DATA_BLOB value;
};

struct ea_list {
	struct ea_list *next, *prev;
	struct ea_struct ea;
};

/* EA names used internally in Samba. KEEP UP TO DATE with prohibited_ea_names in trans2.c !. */
#define SAMBA_POSIX_INHERITANCE_EA_NAME "user.SAMBA_PAI"
/* EA to use for DOS attributes */
#define SAMBA_XATTR_DOS_ATTRIB "user.DOSATTRIB"
/* Prefix for DosStreams in the vfs_streams_xattr module */
#define SAMBA_XATTR_DOSSTREAM_PREFIX "user.DosStream."
/* Prefix for xattrs storing streams. */
#define SAMBA_XATTR_MARKER "user.SAMBA_STREAMS"

/* map readonly options */
enum mapreadonly_options {MAP_READONLY_NO, MAP_READONLY_YES, MAP_READONLY_PERMISSIONS};

/* usershare error codes. */
enum usershare_err {
		USERSHARE_OK=0,
		USERSHARE_MALFORMED_FILE,
		USERSHARE_BAD_VERSION,
		USERSHARE_MALFORMED_PATH,
		USERSHARE_MALFORMED_COMMENT_DEF,
		USERSHARE_MALFORMED_ACL_DEF,
		USERSHARE_ACL_ERR,
		USERSHARE_PATH_NOT_ABSOLUTE,
		USERSHARE_PATH_IS_DENIED,
		USERSHARE_PATH_NOT_ALLOWED,
		USERSHARE_PATH_NOT_DIRECTORY,
		USERSHARE_POSIX_ERR,
		USERSHARE_MALFORMED_SHARENAME_DEF,
		USERSHARE_BAD_SHARENAME
};

/* Different reasons for closing a file. */
enum file_close_type {NORMAL_CLOSE=0,SHUTDOWN_CLOSE,ERROR_CLOSE};

/* Used in SMB_FS_OBJECTID_INFORMATION requests.  Must be exactly 48 bytes. */
#define SAMBA_EXTENDED_INFO_MAGIC 0x536d4261 /* "SmBa" */
#define SAMBA_EXTENDED_INFO_VERSION_STRING_LENGTH 28
struct smb_extended_info {
	uint32 samba_magic;		/* Always SAMBA_EXTRA_INFO_MAGIC */
	uint32 samba_version;		/* Major/Minor/Release/Revision */
	uint32 samba_subversion;	/* Prerelease/RC/Vendor patch */
	NTTIME samba_gitcommitdate;
	char   samba_version_string[SAMBA_EXTENDED_INFO_VERSION_STRING_LENGTH];
};

/* time info */
struct smb_file_time {
	struct timespec mtime;
	struct timespec atime;
	struct timespec ctime;
	struct timespec create_time;
};

/*
 * unix_convert_flags
 */
#define UCF_SAVE_LCOMP			0x00000001
#define UCF_ALWAYS_ALLOW_WCARD_LCOMP	0x00000002
#define UCF_COND_ALLOW_WCARD_LCOMP	0x00000004
#define UCF_POSIX_PATHNAMES		0x00000008
#define UCF_UNIX_NAME_LOOKUP		0x00000010
#define UCF_CREATING_FILE		0x00000020

/*
 * smb_filename
 */
struct smb_filename {
	char *base_name;
	char *stream_name;
	char *original_lcomp;
	SMB_STRUCT_STAT st;
};

/* struct for maintaining the child processes that get spawned from smbd */
struct child_pid {
	struct child_pid *prev, *next;
	pid_t pid;
};

/* Used to keep track of deferred opens. */
struct deferred_open_record;

/* Client-side offline caching policy types */
#define CSC_POLICY_MANUAL 0
#define CSC_POLICY_DOCUMENTS 1
#define CSC_POLICY_PROGRAMS 2
#define CSC_POLICY_DISABLE 3

/* Used inside aio code. */
struct aio_extra;

/*
 * Reasons for cache flush.
 */

enum flush_reason_enum {
    SEEK_FLUSH,
    READ_FLUSH,
    WRITE_FLUSH,
    READRAW_FLUSH,
    OPLOCK_RELEASE_FLUSH,
    CLOSE_FLUSH,
    SYNC_FLUSH,
    SIZECHANGE_FLUSH,
    /* NUM_FLUSH_REASONS must remain the last value in the enumeration. */
    NUM_FLUSH_REASONS};

#endif /* _SMB_H */
