/* 
   Unix SMB/CIFS implementation.

   generic byte range locking code

   Copyright (C) Andrew Tridgell 1992-2004
   Copyright (C) Jeremy Allison 1992-2000
   
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

/* This module implements a tdb based byte range locking service,
   replacing the fcntl() based byte range locking previously
   used. This allows us to provide the same semantics as NT */

#include "includes.h"
#include "system/filesys.h"
#include "../tdb/include/tdb.h"
#include "messaging/messaging.h"
#include "lib/messaging/irpc.h"
#include "libcli/libcli.h"
#include "cluster/cluster.h"
#include "ntvfs/common/ntvfs_common.h"

static const struct brlock_ops *ops;

/*
  set the brl backend ops
*/
void brl_set_ops(const struct brlock_ops *new_ops)
{
	ops = new_ops;
}

/*
  Open up the brlock database. Close it down using talloc_free(). We
  need the messaging_ctx to allow for pending lock notifications.
*/
struct brl_context *brl_init(TALLOC_CTX *mem_ctx, struct server_id server, 
			     struct loadparm_context *lp_ctx,
			     struct messaging_context *messaging_ctx)
{
	if (ops == NULL) {
		brl_tdb_init_ops();
	}
	return ops->brl_init(mem_ctx, server, lp_ctx, messaging_ctx);
}

struct brl_handle *brl_create_handle(TALLOC_CTX *mem_ctx, struct ntvfs_handle *ntvfs, DATA_BLOB *file_key)
{
	return ops->brl_create_handle(mem_ctx, ntvfs, file_key);
}

/*
  Lock a range of bytes.  The lock_type can be a PENDING_*_LOCK, in
  which case a real lock is first tried, and if that fails then a
  pending lock is created. When the pending lock is triggered (by
  someone else closing an overlapping lock range) a messaging
  notification is sent, identified by the notify_ptr
*/
NTSTATUS brl_lock(struct brl_context *brl,
		  struct brl_handle *brlh,
		  uint32_t smbpid,
		  uint64_t start, uint64_t size, 
		  enum brl_type lock_type,
		  void *notify_ptr)
{
	return ops->brl_lock(brl, brlh, smbpid, start, size, lock_type, notify_ptr);
}


/*
 Unlock a range of bytes.
*/
NTSTATUS brl_unlock(struct brl_context *brl,
		    struct brl_handle *brlh, 
		    uint32_t smbpid,
		    uint64_t start, uint64_t size)
{
	return ops->brl_unlock(brl, brlh, smbpid, start, size);
}

/*
  remove a pending lock. This is called when the caller has either
  given up trying to establish a lock or when they have succeeded in
  getting it. In either case they no longer need to be notified.
*/
NTSTATUS brl_remove_pending(struct brl_context *brl,
			    struct brl_handle *brlh, 
			    void *notify_ptr)
{
	return ops->brl_remove_pending(brl, brlh, notify_ptr);
}


/*
  Test if we are allowed to perform IO on a region of an open file
*/
NTSTATUS brl_locktest(struct brl_context *brl,
		      struct brl_handle *brlh,
		      uint32_t smbpid, 
		      uint64_t start, uint64_t size, 
		      enum brl_type lock_type)
{
	return ops->brl_locktest(brl, brlh, smbpid, start, size, lock_type);
}


/*
 Remove any locks associated with a open file.
*/
NTSTATUS brl_close(struct brl_context *brl,
		   struct brl_handle *brlh)
{
	return ops->brl_close(brl, brlh);
}
