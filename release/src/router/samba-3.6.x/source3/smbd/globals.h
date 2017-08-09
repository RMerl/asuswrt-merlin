/*
   Unix SMB/Netbios implementation.
   smbd globals
   Copyright (C) Stefan Metzmacher 2009
   Copyright (C) Jeremy Allison 2010

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

#include "system/select.h"

#if defined(WITH_AIO)
struct aio_extra;
extern struct aio_extra *aio_list_head;
extern struct tevent_signal *aio_signal_event;
extern int aio_pending_size;
extern int outstanding_aio_calls;
#endif

#ifdef USE_DMAPI
struct smbd_dmapi_context;
extern struct smbd_dmapi_context *dmapi_ctx;
#endif

extern bool dfree_broken;

/* how many write cache buffers have been allocated */
extern unsigned int allocated_write_caches;

/* A singleton cache to speed up searching by dev/inode. */
struct fsp_singleton_cache {
	files_struct *fsp;
	struct file_id id;
};

extern const struct mangle_fns *mangle_fns;

extern unsigned char *chartest;
struct tdb_context;
extern struct tdb_context *tdb_mangled_cache;

/*
  this determines how many characters are used from the original filename
  in the 8.3 mangled name. A larger value leads to a weaker hash and more collisions.
  The largest possible value is 6.
*/
extern unsigned mangle_prefix;

struct msg_state;
extern struct msg_state *smbd_msg_state;

extern bool logged_ioctl_message;

extern int trans_num;

extern time_t last_smb_conf_reload_time;
extern time_t last_printer_reload_time;
/****************************************************************************
 structure to hold a linked list of queued messages.
 for processing.
****************************************************************************/
struct pending_message_list;
extern struct pending_message_list *deferred_open_queue;
extern uint32_t common_flags2;

struct smb_srv_trans_enc_ctx;
extern struct smb_srv_trans_enc_ctx *partial_srv_trans_enc_ctx;
extern struct smb_srv_trans_enc_ctx *srv_trans_enc_ctx;

struct sec_ctx {
	struct security_unix_token ut;
	struct security_token *token;
};
/* A stack of security contexts.  We include the current context as being
   the first one, so there is room for another MAX_SEC_CTX_DEPTH more. */
extern struct sec_ctx sec_ctx_stack[MAX_SEC_CTX_DEPTH + 1];
extern int sec_ctx_stack_ndx;
extern bool become_uid_done;
extern bool become_gid_done;

extern connection_struct *last_conn;
extern uint16_t last_flags;

extern uint32_t global_client_caps;

extern uint16_t fnf_handle;

struct conn_ctx {
	connection_struct *conn;
	uint16 vuid;
};
/* A stack of current_user connection contexts. */
extern struct conn_ctx conn_ctx_stack[MAX_SEC_CTX_DEPTH];
extern int conn_ctx_stack_ndx;

struct vfs_init_function_entry;
extern struct vfs_init_function_entry *backends;
extern char *sparse_buf;
extern char *LastDir;

/* Current number of oplocks we have outstanding. */
extern int32_t exclusive_oplocks_open;
extern int32_t level_II_oplocks_open;
extern struct kernel_oplocks *koplocks;

extern int am_parent;
extern struct event_context *smbd_event_ctx;
extern struct messaging_context *smbd_msg_ctx;
extern struct memcache *smbd_memcache_ctx;
extern bool exit_firsttime;
struct child_pid;
extern struct child_pid *children;
extern int num_children;

struct tstream_context;
struct smbd_smb2_request;
struct smbd_smb2_session;
struct smbd_smb2_tcon;

DATA_BLOB negprot_spnego(TALLOC_CTX *ctx, struct smbd_server_connection *sconn);

void smbd_lock_socket(struct smbd_server_connection *sconn);
void smbd_unlock_socket(struct smbd_server_connection *sconn);

NTSTATUS smb2_signing_sign_pdu(DATA_BLOB session_key,
			       struct iovec *vector,
			       int count);
NTSTATUS smb2_signing_check_pdu(DATA_BLOB session_key,
				const struct iovec *vector,
				int count);

NTSTATUS smbd_do_locking(struct smb_request *req,
			 files_struct *fsp,
			 uint8_t type,
			 int32_t timeout,
			 uint16_t num_ulocks,
			 struct smbd_lock_element *ulocks,
			 uint16_t num_locks,
			 struct smbd_lock_element *locks,
			 bool *async);

NTSTATUS smbd_do_qfilepathinfo(connection_struct *conn,
			       TALLOC_CTX *mem_ctx,
			       uint16_t info_level,
			       files_struct *fsp,
			       struct smb_filename *smb_fname,
			       bool delete_pending,
			       struct timespec write_time_ts,
			       struct ea_list *ea_list,
			       int lock_data_count,
			       char *lock_data,
			       uint16_t flags2,
			       unsigned int max_data_bytes,
			       size_t *fixed_portion,
			       char **ppdata,
			       unsigned int *pdata_size);

NTSTATUS smbd_do_setfilepathinfo(connection_struct *conn,
				struct smb_request *req,
				TALLOC_CTX *mem_ctx,
				uint16_t info_level,
				files_struct *fsp,
				struct smb_filename *smb_fname,
				char **ppdata, int total_data,
				int *ret_data_size);

NTSTATUS smbd_do_qfsinfo(connection_struct *conn,
			 TALLOC_CTX *mem_ctx,
			 uint16_t info_level,
			 uint16_t flags2,
			 unsigned int max_data_bytes,
			 size_t *fixed_portion,
			 struct smb_filename *smb_fname,
			 char **ppdata,
			 int *ret_data_len);

bool smbd_dirptr_get_entry(TALLOC_CTX *ctx,
			   struct dptr_struct *dirptr,
			   const char *mask,
			   uint32_t dirtype,
			   bool dont_descend,
			   bool ask_sharemode,
			   bool (*match_fn)(TALLOC_CTX *ctx,
					    void *private_data,
					    const char *dname,
					    const char *mask,
					    char **_fname),
			   bool (*mode_fn)(TALLOC_CTX *ctx,
					   void *private_data,
					   struct smb_filename *smb_fname,
					   uint32_t *_mode),
			   void *private_data,
			   char **_fname,
			   struct smb_filename **_smb_fname,
			   uint32_t *_mode,
			   long *_prev_offset);

bool smbd_dirptr_lanman2_entry(TALLOC_CTX *ctx,
			       connection_struct *conn,
			       struct dptr_struct *dirptr,
			       uint16 flags2,
			       const char *path_mask,
			       uint32 dirtype,
			       int info_level,
			       int requires_resume_key,
			       bool dont_descend,
			       bool ask_sharemode,
			       uint8_t align,
			       bool do_pad,
			       char **ppdata,
			       char *base_data,
			       char *end_data,
			       int space_remaining,
			       bool *out_of_space,
			       bool *got_exact_match,
			       int *_last_entry_off,
			       struct ea_list *name_list);

NTSTATUS smbd_calculate_access_mask(connection_struct *conn,
				    const struct smb_filename *smb_fname,
				    bool file_existed,
				    uint32_t access_mask,
				    uint32_t *access_mask_out);
NTSTATUS smbd_check_open_rights(struct connection_struct *conn,
				const struct smb_filename *smb_fname,
				uint32_t access_mask,
				uint32_t *access_granted);

void smbd_notify_cancel_by_smbreq(const struct smb_request *smbreq);

void smbd_server_connection_terminate_ex(struct smbd_server_connection *sconn,
					 const char *reason,
					 const char *location);
#define smbd_server_connection_terminate(sconn, reason) \
	smbd_server_connection_terminate_ex(sconn, reason, __location__)

struct server_id sconn_server_id(const struct smbd_server_connection *sconn);

const char *smb2_opcode_name(uint16_t opcode);
bool smbd_is_smb2_header(const uint8_t *inbuf, size_t size);

void reply_smb2002(struct smb_request *req, uint16_t choice);
void smbd_smb2_first_negprot(struct smbd_server_connection *sconn,
			     const uint8_t *inbuf, size_t size);

NTSTATUS smbd_smb2_request_error_ex(struct smbd_smb2_request *req,
				    NTSTATUS status,
				    DATA_BLOB *info,
				    const char *location);
#define smbd_smb2_request_error(req, status) \
	smbd_smb2_request_error_ex(req, status, NULL, __location__)
NTSTATUS smbd_smb2_request_done_ex(struct smbd_smb2_request *req,
				   NTSTATUS status,
				   DATA_BLOB body, DATA_BLOB *dyn,
				   const char *location);
#define smbd_smb2_request_done(req, body, dyn) \
	smbd_smb2_request_done_ex(req, NT_STATUS_OK, body, dyn, __location__)

NTSTATUS smbd_smb2_send_oplock_break(struct smbd_server_connection *sconn,
				     uint64_t file_id_persistent,
				     uint64_t file_id_volatile,
				     uint8_t oplock_level);

NTSTATUS smbd_smb2_request_pending_queue(struct smbd_smb2_request *req,
					 struct tevent_req *subreq);

NTSTATUS smbd_smb2_request_check_session(struct smbd_smb2_request *req);
NTSTATUS smbd_smb2_request_check_tcon(struct smbd_smb2_request *req);

struct smb_request *smbd_smb2_fake_smb_request(struct smbd_smb2_request *req);
void remove_smb2_chained_fsp(files_struct *fsp);

NTSTATUS smbd_smb2_request_verify_sizes(struct smbd_smb2_request *req,
					size_t expected_body_size);

NTSTATUS smbd_smb2_request_process_negprot(struct smbd_smb2_request *req);
NTSTATUS smbd_smb2_request_process_sesssetup(struct smbd_smb2_request *req);
NTSTATUS smbd_smb2_request_process_logoff(struct smbd_smb2_request *req);
NTSTATUS smbd_smb2_request_process_tcon(struct smbd_smb2_request *req);
NTSTATUS smbd_smb2_request_process_tdis(struct smbd_smb2_request *req);
NTSTATUS smbd_smb2_request_process_create(struct smbd_smb2_request *req);
NTSTATUS smbd_smb2_request_process_close(struct smbd_smb2_request *req);
NTSTATUS smbd_smb2_request_process_flush(struct smbd_smb2_request *req);
NTSTATUS smbd_smb2_request_process_read(struct smbd_smb2_request *req);
NTSTATUS smb2_read_complete(struct tevent_req *req, ssize_t nread, int err);
NTSTATUS smbd_smb2_request_process_write(struct smbd_smb2_request *req);
NTSTATUS smb2_write_complete(struct tevent_req *req, ssize_t nwritten, int err);
NTSTATUS smbd_smb2_request_process_lock(struct smbd_smb2_request *req);
NTSTATUS smbd_smb2_request_process_ioctl(struct smbd_smb2_request *req);
NTSTATUS smbd_smb2_request_process_keepalive(struct smbd_smb2_request *req);
NTSTATUS smbd_smb2_request_process_find(struct smbd_smb2_request *req);
NTSTATUS smbd_smb2_request_process_notify(struct smbd_smb2_request *req);
NTSTATUS smbd_smb2_request_process_getinfo(struct smbd_smb2_request *req);
NTSTATUS smbd_smb2_request_process_setinfo(struct smbd_smb2_request *req);
NTSTATUS smbd_smb2_request_process_break(struct smbd_smb2_request *req);
NTSTATUS smbd_smb2_request_dispatch(struct smbd_smb2_request *req);
void smbd_smb2_request_dispatch_immediate(struct tevent_context *ctx,
				struct tevent_immediate *im,
				void *private_data);

/* SMB1 -> SMB2 glue. */
void send_break_message_smb2(files_struct *fsp, int level);
struct blocking_lock_record *get_pending_smb2req_blr(struct smbd_smb2_request *smb2req);
bool push_blocking_lock_request_smb2( struct byte_range_lock *br_lck,
				struct smb_request *req,
				files_struct *fsp,
				int lock_timeout,
				int lock_num,
				uint64_t smblctx,
				enum brl_type lock_type,
				enum brl_flavour lock_flav,
				uint64_t offset,
				uint64_t count,
				uint64_t blocking_smblctx);
void process_blocking_lock_queue_smb2(
	struct smbd_server_connection *sconn, struct timeval tv_curr);
void cancel_pending_lock_requests_by_fid_smb2(files_struct *fsp,
			struct byte_range_lock *br_lck,
			enum file_close_type close_type);
/* From smbd/smb2_create.c */
int map_smb2_oplock_levels_to_samba(uint8_t in_oplock_level);
bool get_deferred_open_message_state_smb2(struct smbd_smb2_request *smb2req,
			struct timeval *p_request_time,
			void **pp_state);
bool open_was_deferred_smb2(struct smbd_server_connection *sconn,
			    uint64_t mid);
void remove_deferred_open_message_smb2(
	struct smbd_server_connection *sconn, uint64_t mid);
void schedule_deferred_open_message_smb2(
	struct smbd_server_connection *sconn, uint64_t mid);
bool push_deferred_open_message_smb2(struct smbd_smb2_request *smb2req,
			struct timeval request_time,
			struct timeval timeout,
			struct file_id id,
			char *private_data,
			size_t priv_len);

struct smbd_smb2_request {
	struct smbd_smb2_request *prev, *next;

	TALLOC_CTX *mem_pool;
	struct smbd_smb2_request **parent;

	struct smbd_server_connection *sconn;

	/* the session the request operates on, maybe NULL */
	struct smbd_smb2_session *session;
	uint64_t last_session_id;

	/* the tcon the request operates on, maybe NULL */
	struct smbd_smb2_tcon *tcon;
	uint32_t last_tid;

	int current_idx;
	bool do_signing;
	bool async;
	bool cancelled;
	bool compound_related;

	/* fake smb1 request. */
	struct smb_request *smb1req;
	struct files_struct *compat_chain_fsp;

	/*
	 * The sub request for async backend calls.
	 * This is used for SMB2 Cancel.
	 */
	struct tevent_req *subreq;

	struct {
		/* the NBT header is not allocated */
		uint8_t nbt_hdr[4];
		/*
		 * vector[0] NBT
		 * .
		 * vector[1] SMB2
		 * vector[2] fixed body
		 * vector[3] dynamic body
		 * .
		 * .
		 * .
		 * vector[4] SMB2
		 * vector[5] fixed body
		 * vector[6] dynamic body
		 * .
		 * .
		 * .
		 */
		struct iovec *vector;
		int vector_count;
	} in;
	struct {
		/* the NBT header is not allocated */
		uint8_t nbt_hdr[4];
		/*
		 * vector[0] NBT
		 * .
		 * vector[1] SMB2
		 * vector[2] fixed body
		 * vector[3] dynamic body
		 * .
		 * .
		 * .
		 * vector[4] SMB2
		 * vector[5] fixed body
		 * vector[6] dynamic body
		 * .
		 * .
		 * .
		 */
		struct iovec *vector;
		int vector_count;
	} out;
};

struct smbd_server_connection;

struct smbd_smb2_session {
	struct smbd_smb2_session *prev, *next;
	struct smbd_server_connection *sconn;
	NTSTATUS status;
	uint64_t vuid;
	struct auth_ntlmssp_state *auth_ntlmssp_state;
	struct auth_serversupplied_info *session_info;
	DATA_BLOB session_key;
	bool do_signing;

	user_struct *compat_vuser;

	struct {
		/* an id tree used to allocate tids */
		struct idr_context *idtree;

		/* this is the limit of tid values for this connection */
		uint32_t limit;

		struct smbd_smb2_tcon *list;
	} tcons;
};

struct smbd_smb2_tcon {
	struct smbd_smb2_tcon *prev, *next;
	struct smbd_smb2_session *session;
	uint32_t tid;
	int snum;
	connection_struct *compat_conn;
};

struct pending_auth_data;

struct smbd_server_connection {
	int sock;
	struct client_address client_id;
	const struct tsocket_address *local_address;
	const struct tsocket_address *remote_address;
	struct messaging_context *msg_ctx;
	struct {
		bool got_session;
	} nbt;
	bool using_smb2;
	int trans_num;

	/*
	 * Cache for calling poll(2) to avoid allocations in our
	 * central event loop
	 */
	struct pollfd *pfds;

	struct files_struct *files;
	struct bitmap *file_bmap;
	int real_max_open_files;
	int files_used;
	struct fsp_singleton_cache fsp_fi_cache;
	unsigned long file_gen_counter;
	int first_file;

	/* number of open connections (tcons) */
	int num_tcons_open;

	/* open directory handles. */
	struct {
		struct bitmap *dptr_bmap;
		struct dptr_struct *dirptrs;
		int dirhandles_open;
	} searches;

	uint64_t num_requests;

	struct {
		struct fd_event *fde;

		struct {
			/*
			 * fd for the fcntl lock mutexing access to our sock
			 */
			int socket_lock_fd;

			/*
			 * fd for the trusted pipe from
			 * echo handler child
			 */
			int trusted_fd;

			/*
			 * fde for the trusted_fd
			 */
			struct fd_event *trusted_fde;

			/*
			 * Reference count for the fcntl lock to
			 * allow recursive locks.
			 */
			int ref_count;
		} echo_handler;

		struct {
			bool encrypted_passwords;
			bool spnego;
			struct auth_context *auth_context;
			bool done;
			/*
			 * Size of the data we can receive. Set by us.
			 * Can be modified by the max xmit parameter.
			 */
			int max_recv;
		} negprot;

		struct {
			bool done_sesssetup;
			/*
			 * Size of data we can send to client. Set
			 *  by the client for all protocols above CORE.
			 *  Set by us for CORE protocol.
			 */
			int max_send;
			uint16_t last_session_tag;

			/* users from session setup */
			char *session_userlist;
			/* workgroup from session setup. */
			char *session_workgroup;
			/*
			 * this holds info on user ids that are already
			 * validated for this VC
			 */
			user_struct *validated_users;
			uint16_t next_vuid;
			int num_validated_vuids;
		} sessions;
		struct {
			connection_struct *Connections;
			/* number of open connections */
			struct bitmap *bmap;
		} tcons;
		struct smb_signing_state *signing_state;
		/* List to store partial SPNEGO auth fragments. */
		struct pending_auth_data *pd_list;

		struct notify_mid_map *notify_mid_maps;

		struct {
			/* dlink list we store pending lock records on. */
			struct blocking_lock_record *blocking_lock_queue;
			/* dlink list we move cancelled lock records onto. */
			struct blocking_lock_record *blocking_lock_cancelled_queue;

			/* The event that makes us process our blocking lock queue */
			struct timed_event *brl_timeout;

			bool blocking_lock_unlock_state;
			bool blocking_lock_cancel_state;
		} locks;
	} smb1;
	struct {
		struct tevent_context *event_ctx;
		struct tevent_queue *recv_queue;
		struct tevent_queue *send_queue;
		struct tstream_context *stream;
		struct {
			/* an id tree used to allocate vuids */
			/* this holds info on session vuids that are already
			 * validated for this VC */
			struct idr_context *idtree;

			/* this is the limit of vuid values for this connection */
			uint64_t limit;

			struct smbd_smb2_session *list;
		} sessions;
		struct {
			/* The event that makes us process our blocking lock queue */
			struct timed_event *brl_timeout;
			bool blocking_lock_unlock_state;
		} locks;
		struct smbd_smb2_request *requests;
		/*
		 * seqnum_low is the lowest sequence number
		 * we will accept.
		 */
		uint64_t seqnum_low;
		/*
		 * seqnum_range is the range of credits we have
		 * granted from the sequence windows starting
		 * at seqnum_low.
		 *
		 * This gets incremented when new credits are
		 * granted and gets decremented when the
		 * lowest sequence number is consumed
		 * (when seqnum_low gets incremented).
		 */
		uint16_t seqnum_range;
		/*
		 * credits_grantedThe number of credits we have currently granted
		 * to the client.
		 *
		 * This gets incremented when new credits are
		 * granted and gets decremented when any credit
		 * is comsumed.
		 *
		 * Note: the decrementing is different compared
		 *       to seqnum_range.
		 */
		uint16_t credits_granted;
		/*
		 * The maximum number of credits we will ever
		 * grant to the client.
		 *
		 * Typically we will only grant 1/16th of
		 * max_credits.
		 *
		 * This is the "server max credits" parameter.
		 */
		uint16_t max_credits;
		/*
		 * a bitmap of size max_credits
		 */
		struct bitmap *credits_bitmap;
		uint32_t max_trans;
		uint32_t max_read;
		uint32_t max_write;
	} smb2;
};

extern struct smbd_server_connection *smbd_server_conn;

struct smbd_server_connection *msg_ctx_to_sconn(struct messaging_context *msg_ctx);

void smbd_init_globals(void);
