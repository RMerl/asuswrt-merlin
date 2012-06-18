/* 
   Unix SMB/CIFS implementation.

   generic byte range locking code - common include

   Copyright (C) Andrew Tridgell 2006
   
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

#include "libcli/libcli.h"

struct brlock_ops {
	struct brl_context *(*brl_init)(TALLOC_CTX *, struct server_id , 
					struct loadparm_context *lp_ctx,
					struct messaging_context *);
	struct brl_handle *(*brl_create_handle)(TALLOC_CTX *, struct ntvfs_handle *, DATA_BLOB *);
	NTSTATUS (*brl_lock)(struct brl_context *,
			     struct brl_handle *,
			     uint32_t ,
			     uint64_t , uint64_t , 
			     enum brl_type ,
			     void *);
	NTSTATUS (*brl_unlock)(struct brl_context *,
			       struct brl_handle *, 
			       uint32_t ,
			       uint64_t , uint64_t );
	NTSTATUS (*brl_remove_pending)(struct brl_context *,
				       struct brl_handle *, 
				       void *);
	NTSTATUS (*brl_locktest)(struct brl_context *,
				 struct brl_handle *,
				 uint32_t , 
				 uint64_t , uint64_t , 
				 enum brl_type );
	NTSTATUS (*brl_close)(struct brl_context *,
			      struct brl_handle *);
};


void brl_set_ops(const struct brlock_ops *new_ops);
void brl_tdb_init_ops(void);
void brl_ctdb_init_ops(void);

