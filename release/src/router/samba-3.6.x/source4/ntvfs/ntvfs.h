/* 
   Unix SMB/CIFS implementation.
   NTVFS structures and defines
   Copyright (C) Andrew Tridgell			2003
   Copyright (C) Stefan Metzmacher			2004
   
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

#ifndef _NTVFS_H_
#define _NTVFS_H_

#include "libcli/raw/interfaces.h"
#include "param/share.h"
#include "librpc/gen_ndr/security.h"
#include "librpc/gen_ndr/server_id4.h"

/* modules can use the following to determine if the interface has changed */
/* version 1 -> 0 - make module stacking easier -- metze */
#define NTVFS_INTERFACE_VERSION 0

struct ntvfs_module_context;
struct ntvfs_request;

/* each backend has to be one one of the following 3 basic types. In
   earlier versions of Samba backends needed to handle all types, now
   we implement them separately.
   The values 1..3 match the SMB2 SMB2_SHARE_TYPE_* values
 */
enum ntvfs_type {NTVFS_DISK=1, NTVFS_IPC=2, NTVFS_PRINT=3};

/* the ntvfs operations structure - contains function pointers to 
   the backend implementations of each operation */
struct ntvfs_ops {
	const char *name;
	enum ntvfs_type type;

	/* initial setup */
	NTSTATUS (*connect)(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req,
			    union smb_tcon *tcon);
	NTSTATUS (*disconnect)(struct ntvfs_module_context *ntvfs);

	/* async_setup - called when a backend is processing a async request */
	NTSTATUS (*async_setup)(struct ntvfs_module_context *ntvfs,
				struct ntvfs_request *req,
				void *private_data);

	/* filesystem operations */
	NTSTATUS (*fsinfo)(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req,
			   union smb_fsinfo *fs);

	/* path operations */
	NTSTATUS (*unlink)(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req,
			   union smb_unlink *unl);
	NTSTATUS (*chkpath)(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req,
			    union smb_chkpath *cp);
	NTSTATUS (*qpathinfo)(struct ntvfs_module_context *ntvfs,
			      struct ntvfs_request *req,
			      union smb_fileinfo *st);
	NTSTATUS (*setpathinfo)(struct ntvfs_module_context *ntvfs,
				struct ntvfs_request *req,
				union smb_setfileinfo *st);
	NTSTATUS (*mkdir)(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req,
			  union smb_mkdir *md);
	NTSTATUS (*rmdir)(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req,
			  struct smb_rmdir *rd);
	NTSTATUS (*rename)(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req,
			   union smb_rename *ren);
	NTSTATUS (*copy)(struct ntvfs_module_context *ntvfs,
			 struct ntvfs_request *req,
			 struct smb_copy *cp);
	NTSTATUS (*open)(struct ntvfs_module_context *ntvfs,
			 struct ntvfs_request *req,
			 union smb_open *oi);

	/* directory search */
	NTSTATUS (*search_first)(struct ntvfs_module_context *ntvfs,
				 struct ntvfs_request *req,
				 union smb_search_first *io, void *private_data,
				 bool (*callback)(void *private_data, const union smb_search_data *file));
	NTSTATUS (*search_next)(struct ntvfs_module_context *ntvfs,
				struct ntvfs_request *req,
				union smb_search_next *io, void *private_data,
				bool (*callback)(void *private_data, const union smb_search_data *file));
	NTSTATUS (*search_close)(struct ntvfs_module_context *ntvfs,
				 struct ntvfs_request *req,
				 union smb_search_close *io);

	/* operations on open files */
	NTSTATUS (*ioctl)(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req,
			  union smb_ioctl *io);
	NTSTATUS (*read)(struct ntvfs_module_context *ntvfs,
			 struct ntvfs_request *req,
			 union smb_read *io);
	NTSTATUS (*write)(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req,
			  union smb_write *io);
	NTSTATUS (*seek)(struct ntvfs_module_context *ntvfs,
			 struct ntvfs_request *req,
			 union smb_seek *io);
	NTSTATUS (*flush)(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req,
			  union smb_flush *flush);
	NTSTATUS (*lock)(struct ntvfs_module_context *ntvfs,
			 struct ntvfs_request *req,
			 union smb_lock *lck);
	NTSTATUS (*qfileinfo)(struct ntvfs_module_context *ntvfs,
			      struct ntvfs_request *req,
			      union smb_fileinfo *info);
	NTSTATUS (*setfileinfo)(struct ntvfs_module_context *ntvfs,
				struct ntvfs_request *req,
				union smb_setfileinfo *info);
	NTSTATUS (*close)(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req,
			  union smb_close *io);

	/* trans interface - used by IPC backend for pipes and RAP calls */
	NTSTATUS (*trans)(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req,
			  struct smb_trans2 *trans);

	/* trans2 interface - only used by CIFS backend to prover complete passthru for testing */
	NTSTATUS (*trans2)(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req,
			   struct smb_trans2 *trans2);

	/* change notify request */
	NTSTATUS (*notify)(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req,
			   union smb_notify *info);

	/* cancel - cancels any pending async request */
	NTSTATUS (*cancel)(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req);

	/* printing specific operations */
	NTSTATUS (*lpq)(struct ntvfs_module_context *ntvfs, 
			struct ntvfs_request *req,
			union smb_lpq *lpq);

	/* logoff - called when a vuid is closed */
	NTSTATUS (*logoff)(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req);
	NTSTATUS (*exit)(struct ntvfs_module_context *ntvfs,
			 struct ntvfs_request *req);
};

struct ntvfs_module_context {
	struct ntvfs_module_context *prev, *next;
	struct ntvfs_context *ctx;
	int depth;
	const struct ntvfs_ops *ops;
	void *private_data;
};

struct ntvfs_context {
	enum ntvfs_type type;

	/* the reported filesystem type */
	char *fs_type;

	/* the reported device type */
	char *dev_type;

	enum protocol_types protocol;

	/*
	 * client capabilities
	 * this field doesn't use protocol specific
	 * values!
	 */
#define NTVFS_CLIENT_CAP_LEVEL_II_OPLOCKS	0x0000000000000001LLU
	uint64_t client_caps;

	/* 
	 * linked list of module contexts
	 */
	struct ntvfs_module_context *modules;

	struct share_config *config;

	struct server_id server_id;
	struct loadparm_context *lp_ctx;
	struct tevent_context *event_ctx;
	struct messaging_context *msg_ctx;

	struct {
		void *private_data;
		NTSTATUS (*handler)(void *private_data, struct ntvfs_handle *handle, uint8_t level);
	} oplock;

	struct {
		const struct tsocket_address *local_address;
		const struct tsocket_address *remote_address;
	} client;

	struct {
		void *private_data;
		NTSTATUS (*create_new)(void *private_data, struct ntvfs_request *req, struct ntvfs_handle **h);
		NTSTATUS (*make_valid)(void *private_data, struct ntvfs_handle *h);
		void (*destroy)(void *private_data, struct ntvfs_handle *h);
		struct ntvfs_handle *(*search_by_wire_key)(void *private_data,  struct ntvfs_request *req, const DATA_BLOB *key);
		DATA_BLOB (*get_wire_key)(void *private_data, struct ntvfs_handle *handle, TALLOC_CTX *mem_ctx);
	} handles;
};

/* a set of flags to control handling of request structures */
#define NTVFS_ASYNC_STATE_ASYNC     (1<<1) /* the backend will answer this one later */
#define NTVFS_ASYNC_STATE_MAY_ASYNC (1<<2) /* the backend is allowed to answer async */
#define NTVFS_ASYNC_STATE_CLOSE     (1<<3) /* the backend session should be closed */

/* the ntvfs_async_state structure allows backend functions to 
   delay replying to requests. To use this, the front end must
   set send_fn to a function to be called by the backend
   when the reply is finally ready to be sent. The backend
   must set status to the status it wants in the
   reply. The backend must set the NTVFS_ASYNC_STATE_ASYNC
   control_flag on the request to indicate that it wishes to
   delay the reply

   If NTVFS_ASYNC_STATE_MAY_ASYNC is not set then the backend cannot
   ask for a delayed reply for this request

   note that the private_data pointer is private to the layer which alloced this struct
*/
struct ntvfs_async_state {
	struct ntvfs_async_state *prev, *next;
	/* the async handling infos */
	unsigned int state;
	void *private_data;
	void (*send_fn)(struct ntvfs_request *);
	NTSTATUS status;

	/* the passthru module's per session private data */
	struct ntvfs_module_context *ntvfs;
};

struct ntvfs_request {
	/* the ntvfs_context this requests belongs to */
	struct ntvfs_context *ctx;

	/* ntvfs per request async states */
	struct ntvfs_async_state *async_states;

	/* the session_info, with security_token and maybe delegated credentials */
	struct auth_session_info *session_info;

	/* the smb pid is needed for locking contexts */
	uint32_t smbpid;

	/*
	 * client capabilities
	 * this field doesn't use protocol specific
	 * values!
	 * see NTVFS_CLIENT_CAP_*
	 */
	uint64_t client_caps;

	/* some statictics for the management tools */
	struct {
		/* the system time when the request arrived */
		struct timeval request_time;
	} statistics;

	struct {
		void *private_data;
	} frontend_data;
};

struct ntvfs_handle {
	struct ntvfs_context *ctx;

	struct auth_session_info *session_info;

	uint16_t smbpid;

	struct ntvfs_handle_data {
		struct ntvfs_handle_data *prev, *next;
		struct ntvfs_module_context *owner;
		void *private_data;/* this must be a valid talloc pointer */
	} *backend_data;

	struct {
		void *private_data;
	} frontend_data;
};

/* this structure is used by backends to determine the size of some critical types */
struct ntvfs_critical_sizes {
	int interface_version;
	int sizeof_ntvfs_critical_sizes;
	int sizeof_ntvfs_context;
	int sizeof_ntvfs_module_context;
	int sizeof_ntvfs_ops;
	int sizeof_ntvfs_async_state;
	int sizeof_ntvfs_request;
	int sizeof_ntvfs_handle;
	int sizeof_ntvfs_handle_data;
};

#define NTVFS_CURRENT_CRITICAL_SIZES(c) \
    struct ntvfs_critical_sizes c = { \
	.interface_version		= NTVFS_INTERFACE_VERSION, \
	.sizeof_ntvfs_critical_sizes	= sizeof(struct ntvfs_critical_sizes), \
	.sizeof_ntvfs_context		= sizeof(struct ntvfs_context), \
	.sizeof_ntvfs_module_context	= sizeof(struct ntvfs_module_context), \
	.sizeof_ntvfs_ops		= sizeof(struct ntvfs_ops), \
	.sizeof_ntvfs_async_state	= sizeof(struct ntvfs_async_state), \
	.sizeof_ntvfs_request		= sizeof(struct ntvfs_request), \
	.sizeof_ntvfs_handle		= sizeof(struct ntvfs_handle), \
	.sizeof_ntvfs_handle_data	= sizeof(struct ntvfs_handle_data), \
    }

struct messaging_context;
#include "librpc/gen_ndr/security.h"
#include "librpc/gen_ndr/s4_notify.h"
#include "ntvfs/ntvfs_proto.h"

#endif /* _NTVFS_H_ */
