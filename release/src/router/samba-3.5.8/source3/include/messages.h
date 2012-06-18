/* 
   Unix SMB/CIFS implementation.
   messages.c header
   Copyright (C) Andrew Tridgell 2000
   Copyright (C) 2001, 2002 by Martin Pool
   
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

#ifndef _MESSAGES_H_
#define _MESSAGES_H_

/* change the message version with any incompatible changes in the protocol */
#define MESSAGE_VERSION 2

/*
 * Special flags passed to message_send. Allocated from the top, lets see when
 * it collides with the message types in the lower 16 bits :-)
 */

/*
 * Under high load, this message can be dropped. Use for notify-style
 * messages that are not critical for correct operation.
 */
#define MSG_FLAG_LOWPRIORITY		0x80000000


/* Flags to classify messages - used in message_send_all() */
/* Sender will filter by flag. */

#define FLAG_MSG_GENERAL		0x0001
#define FLAG_MSG_SMBD			0x0002
#define FLAG_MSG_NMBD			0x0004
#define FLAG_MSG_PRINT_NOTIFY		0x0008
#define FLAG_MSG_PRINT_GENERAL		0x0010
/* dbwrap messages 4001-4999 */
#define FLAG_MSG_DBWRAP			0x0020


/*
 * Virtual Node Numbers are identifying a node within a cluster. Ctdbd sets
 * this, we retrieve our vnn from it.
 */

#define NONCLUSTER_VNN (0xFFFFFFFF)

/*
 * ctdb gives us 64-bit server ids for messaging_send. This is done to avoid
 * pid clashes and to be able to register for special messages like "all
 * smbds".
 *
 * Normal individual server id's have the upper 32 bits to 0, I picked "1" for
 * Samba, other subsystems might use something else.
 */

#define MSG_SRVID_SAMBA 0x0000000100000000LL


struct server_id {
	pid_t pid;
#ifdef CLUSTER_SUPPORT
	uint32 vnn;
#endif
};

#ifdef CLUSTER_SUPPORT
#define MSG_BROADCAST_PID_STR	"0:0"
#else
#define MSG_BROADCAST_PID_STR	"0"
#endif

struct messaging_context;
struct messaging_rec;

/*
 * struct messaging_context belongs to messages.c, but because we still have
 * messaging_dispatch, we need it here. Once we get rid of signals for
 * notifying processes, this will go.
 */

struct messaging_context {
	struct server_id id;
	struct event_context *event_ctx;
	struct messaging_callback *callbacks;

	struct messaging_backend *local;
	struct messaging_backend *remote;
};

struct messaging_backend {
	NTSTATUS (*send_fn)(struct messaging_context *msg_ctx,
			    struct server_id pid, int msg_type,
			    const DATA_BLOB *data,
			    struct messaging_backend *backend);
	void *private_data;
};

NTSTATUS messaging_tdb_init(struct messaging_context *msg_ctx,
			    TALLOC_CTX *mem_ctx,
			    struct messaging_backend **presult);

NTSTATUS messaging_ctdbd_init(struct messaging_context *msg_ctx,
			      TALLOC_CTX *mem_ctx,
			      struct messaging_backend **presult);
struct ctdbd_connection *messaging_ctdbd_connection(void);

bool message_send_all(struct messaging_context *msg_ctx,
		      int msg_type,
		      const void *buf, size_t len,
		      int *n_sent);
struct event_context *messaging_event_context(struct messaging_context *msg_ctx);
struct messaging_context *messaging_init(TALLOC_CTX *mem_ctx, 
					 struct server_id server_id, 
					 struct event_context *ev);

/*
 * re-init after a fork
 */
NTSTATUS messaging_reinit(struct messaging_context *msg_ctx);

NTSTATUS messaging_register(struct messaging_context *msg_ctx,
			    void *private_data,
			    uint32_t msg_type,
			    void (*fn)(struct messaging_context *msg,
				       void *private_data, 
				       uint32_t msg_type, 
				       struct server_id server_id,
				       DATA_BLOB *data));
void messaging_deregister(struct messaging_context *ctx, uint32_t msg_type,
			  void *private_data);
NTSTATUS messaging_send(struct messaging_context *msg_ctx,
			struct server_id server, 
			uint32_t msg_type, const DATA_BLOB *data);
NTSTATUS messaging_send_buf(struct messaging_context *msg_ctx,
			    struct server_id server, uint32_t msg_type,
			    const uint8 *buf, size_t len);
void messaging_dispatch_rec(struct messaging_context *msg_ctx,
			    struct messaging_rec *rec);

#endif
