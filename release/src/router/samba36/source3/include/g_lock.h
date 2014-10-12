/*
   Unix SMB/CIFS implementation.
   global locks based on dbwrap and messaging
   Copyright (C) 2009 by Volker Lendecke

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

#ifndef _G_LOCK_H_
#define _G_LOCK_H_

#include "dbwrap.h"

struct g_lock_ctx;

enum g_lock_type {
	G_LOCK_READ = 0,
	G_LOCK_WRITE = 1,
};

/*
 * Or'ed with g_lock_type
 */
#define G_LOCK_PENDING (2)

struct g_lock_ctx *g_lock_ctx_init(TALLOC_CTX *mem_ctx,
				   struct messaging_context *msg);

NTSTATUS g_lock_lock(struct g_lock_ctx *ctx, const char *name,
		     enum g_lock_type lock_type, struct timeval timeout);
NTSTATUS g_lock_unlock(struct g_lock_ctx *ctx, const char *name);
NTSTATUS g_lock_get(struct g_lock_ctx *ctx, const char *name,
		struct server_id *pid);

NTSTATUS g_lock_do(const char *name, enum g_lock_type lock_type,
		   struct timeval timeout, struct server_id self,
		   void (*fn)(void *private_data), void *private_data);

int g_lock_locks(struct g_lock_ctx *ctx,
		 int (*fn)(const char *name, void *private_data),
		 void *private_data);
NTSTATUS g_lock_dump(struct g_lock_ctx *ctx, const char *name,
		     int (*fn)(struct server_id pid,
			       enum g_lock_type lock_type,
			       void *private_data),
		     void *private_data);

#endif
