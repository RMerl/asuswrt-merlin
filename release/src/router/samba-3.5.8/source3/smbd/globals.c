/*
   Unix SMB/Netbios implementation.
   smbd globals
   Copyright (C) Stefan Metzmacher 2009

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

#include "includes.h"
#include "smbd/globals.h"

#if defined(WITH_AIO)
struct aio_extra *aio_list_head = NULL;
struct tevent_signal *aio_signal_event = NULL;
int aio_pending_size = 0;
int outstanding_aio_calls = 0;
#endif

/* dlink list we store pending lock records on. */
struct blocking_lock_record *blocking_lock_queue = NULL;

/* dlink list we move cancelled lock records onto. */
struct blocking_lock_record *blocking_lock_cancelled_queue = NULL;

/* The event that makes us process our blocking lock queue */
struct timed_event *brl_timeout = NULL;

bool blocking_lock_unlock_state = false;
bool blocking_lock_cancel_state = false;

#ifdef USE_DMAPI
struct smbd_dmapi_context *dmapi_ctx = NULL;
#endif


bool dfree_broken = false;

/* how many write cache buffers have been allocated */
unsigned int allocated_write_caches = 0;

int real_max_open_files = 0;
struct bitmap *file_bmap = NULL;
files_struct *Files = NULL;
int files_used = 0;
struct fsp_singleton_cache fsp_fi_cache = {
	.fsp = NULL,
	.id = {
		.devid = 0,
		.inode = 0,
		.extid = 0
	}
};
unsigned long file_gen_counter = 0;
int first_file = 0;

const struct mangle_fns *mangle_fns = NULL;

unsigned char *chartest = NULL;
TDB_CONTEXT *tdb_mangled_cache = NULL;

/* these tables are used to provide fast tests for characters */
unsigned char char_flags[256];
/*
  this determines how many characters are used from the original filename
  in the 8.3 mangled name. A larger value leads to a weaker hash and more collisions.
  The largest possible value is 6.
*/
unsigned mangle_prefix = 0;
unsigned char base_reverse[256];

char *last_from = NULL;
char *last_to = NULL;

struct msg_state *smbd_msg_state = NULL;

bool logged_ioctl_message = false;

int trans_num = 0;
pid_t mypid = 0;
time_t last_smb_conf_reload_time = 0;
time_t last_printer_reload_time = 0;
/****************************************************************************
 structure to hold a linked list of queued messages.
 for processing.
****************************************************************************/
struct pending_message_list *deferred_open_queue = NULL;
uint32_t common_flags2 = FLAGS2_LONG_PATH_COMPONENTS|FLAGS2_32_BIT_ERROR_CODES|FLAGS2_EXTENDED_ATTRIBUTES;

struct smb_srv_trans_enc_ctx *partial_srv_trans_enc_ctx = NULL;
struct smb_srv_trans_enc_ctx *srv_trans_enc_ctx = NULL;

/* A stack of security contexts.  We include the current context as being
   the first one, so there is room for another MAX_SEC_CTX_DEPTH more. */
struct sec_ctx sec_ctx_stack[MAX_SEC_CTX_DEPTH + 1];
int sec_ctx_stack_ndx = 0;
bool become_uid_done = false;
bool become_gid_done = false;

connection_struct *last_conn = NULL;
uint16_t last_flags = 0;

struct db_context *session_db_ctx_ptr = NULL;

uint32_t global_client_caps = 0;

uint16_t fnf_handle = 257;

/* A stack of current_user connection contexts. */
struct conn_ctx conn_ctx_stack[MAX_SEC_CTX_DEPTH];
int conn_ctx_stack_ndx = 0;

struct vfs_init_function_entry *backends = NULL;
char *sparse_buf = NULL;
char *LastDir = NULL;

/* Current number of oplocks we have outstanding. */
int32_t exclusive_oplocks_open = 0;
int32_t level_II_oplocks_open = 0;
bool global_client_failed_oplock_break = false;
struct kernel_oplocks *koplocks = NULL;

int am_parent = 1;
int server_fd = -1;
struct event_context *smbd_event_ctx = NULL;
struct messaging_context *smbd_msg_ctx = NULL;
struct memcache *smbd_memcache_ctx = NULL;
bool exit_firsttime = true;
struct child_pid *children = 0;
int num_children = 0;

struct smbd_server_connection *smbd_server_conn = NULL;

void smbd_init_globals(void)
{
	ZERO_STRUCT(char_flags);
	ZERO_STRUCT(base_reverse);

	ZERO_STRUCT(conn_ctx_stack);

	ZERO_STRUCT(sec_ctx_stack);

	smbd_server_conn = talloc_zero(smbd_event_context(), struct smbd_server_connection);
	if (!smbd_server_conn) {
		exit_server("failed to create smbd_server_connection");
	}
}
