/*
   Unix SMB/CIFS implementation.

   Copyright (C) Stefan Metzmacher 2008

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

#include "param/share.h"

struct sys_lease_context;
struct opendb_entry;
struct messaging_context;
struct tevent_context;

typedef NTSTATUS (*sys_lease_send_break_fn)(struct messaging_context *,
					    struct opendb_entry *,
					    uint8_t level);

struct sys_lease_ops {
	const char *name;
	NTSTATUS (*init)(struct sys_lease_context *ctx);
	NTSTATUS (*setup)(struct sys_lease_context *ctx,
			  struct opendb_entry *e);
	NTSTATUS (*update)(struct sys_lease_context *ctx,
			   struct opendb_entry *e);
	NTSTATUS (*remove)(struct sys_lease_context *ctx,
			   struct opendb_entry *e);
};

struct sys_lease_context {
	struct tevent_context *event_ctx;
	struct messaging_context *msg_ctx;
	sys_lease_send_break_fn break_send;
	void *private_data; /* for use of backend */
	const struct sys_lease_ops *ops;
};

NTSTATUS sys_lease_register(const struct sys_lease_ops *ops);
NTSTATUS sys_lease_init(void);

struct sys_lease_context *sys_lease_context_create(struct share_config *scfg,
						   TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct messaging_context *msg_ctx,
						   sys_lease_send_break_fn break_send);

NTSTATUS sys_lease_setup(struct sys_lease_context *ctx,
			 struct opendb_entry *e);

NTSTATUS sys_lease_update(struct sys_lease_context *ctx,
			  struct opendb_entry *e);

NTSTATUS sys_lease_remove(struct sys_lease_context *ctx,
			  struct opendb_entry *e);
