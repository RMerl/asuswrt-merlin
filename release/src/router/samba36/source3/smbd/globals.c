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
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "memcache.h"
#include "messages.h"
#include <tdb.h>

#if defined(WITH_AIO)
struct aio_extra *aio_list_head = NULL;
struct tevent_signal *aio_signal_event = NULL;
int aio_pending_size = 0;
int outstanding_aio_calls = 0;
#endif

#ifdef USE_DMAPI
struct smbd_dmapi_context *dmapi_ctx = NULL;
#endif


bool dfree_broken = false;

/* how many write cache buffers have been allocated */
unsigned int allocated_write_caches = 0;

const struct mangle_fns *mangle_fns = NULL;

unsigned char *chartest = NULL;
TDB_CONTEXT *tdb_mangled_cache = NULL;

/*
  this determines how many characters are used from the original filename
  in the 8.3 mangled name. A larger value leads to a weaker hash and more collisions.
  The largest possible value is 6.
*/
unsigned mangle_prefix = 0;

struct msg_state *smbd_msg_state = NULL;

bool logged_ioctl_message = false;

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
struct kernel_oplocks *koplocks = NULL;

int am_parent = 1;
struct memcache *smbd_memcache_ctx = NULL;
bool exit_firsttime = true;
struct child_pid *children = 0;
int num_children = 0;

struct smbd_server_connection *smbd_server_conn = NULL;

struct smbd_server_connection *msg_ctx_to_sconn(struct messaging_context *msg_ctx)
{
	struct server_id my_id, msg_id;

	my_id = messaging_server_id(smbd_server_conn->msg_ctx);
	msg_id = messaging_server_id(msg_ctx);

	if (!procid_equal(&my_id, &msg_id)) {
		return NULL;
	}
	return smbd_server_conn;
}

struct messaging_context *smbd_messaging_context(void)
{
	struct messaging_context *msg_ctx = server_messaging_context();
	if (likely(msg_ctx != NULL)) {
		return msg_ctx;
	}
	smb_panic("Could not init smbd's messaging context.\n");
	return NULL;
}

struct memcache *smbd_memcache(void)
{
	if (!smbd_memcache_ctx) {
		/*
		 * Note we MUST use the NULL context here, not the
		 * autofree context, to avoid side effects in forked
		 * children exiting.
		 */
		smbd_memcache_ctx = memcache_init(NULL,
						  lp_max_stat_cache_size()*1024);
	}
	if (!smbd_memcache_ctx) {
		smb_panic("Could not init smbd memcache");
	}

	return smbd_memcache_ctx;
}


void smbd_init_globals(void)
{
	ZERO_STRUCT(conn_ctx_stack);

	ZERO_STRUCT(sec_ctx_stack);

	smbd_server_conn = talloc_zero(smbd_event_context(), struct smbd_server_connection);
	if (!smbd_server_conn) {
		exit_server("failed to create smbd_server_connection");
	}

	smbd_server_conn->smb1.echo_handler.trusted_fd = -1;
	smbd_server_conn->smb1.echo_handler.socket_lock_fd = -1;
}
