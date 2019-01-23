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

/*
  abstract the various kernel interfaces to leases (oplocks) into a
  single Samba friendly interface
*/

#include "includes.h"
#include "system/filesys.h"
#include "ntvfs/sysdep/sys_lease.h"
#include "../lib/util/dlinklist.h"
#include "param/param.h"

/* list of registered backends */
static struct sys_lease_ops *backends;
static uint32_t num_backends;

#define LEASE_BACKEND	"lease:backend"

/*
  initialise a system change notify backend
*/
_PUBLIC_ struct sys_lease_context *sys_lease_context_create(struct share_config *scfg,
							    TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct messaging_context *msg,
							    sys_lease_send_break_fn break_send)
{
	struct sys_lease_context *ctx;
	const char *bname;
	int i;
	NTSTATUS status;

	if (num_backends == 0) {
		return NULL;
	}

	if (ev == NULL) {
		return NULL;
	}

	ctx = talloc_zero(mem_ctx, struct sys_lease_context);
	if (ctx == NULL) {
		return NULL;
	}

	ctx->event_ctx = ev;
	ctx->msg_ctx = msg;
	ctx->break_send = break_send;

	bname = share_string_option(scfg, LEASE_BACKEND, NULL);
	if (!bname) {
		talloc_free(ctx);
		return NULL;
	}

	for (i=0;i<num_backends;i++) {
		if (strcasecmp(backends[i].name, bname) == 0) {
			ctx->ops = &backends[i];
			break;
		}
	}

	if (!ctx->ops) {
		talloc_free(ctx);
		return NULL;
	}

	status = ctx->ops->init(ctx);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(ctx);
		return NULL;
	}

	return ctx;
}

/*
  register a lease backend
*/
_PUBLIC_ NTSTATUS sys_lease_register(const struct sys_lease_ops *backend)
{
	struct sys_lease_ops *b;
	b = talloc_realloc(talloc_autofree_context(), backends,
			   struct sys_lease_ops, num_backends+1);
	NT_STATUS_HAVE_NO_MEMORY(b);
	backends = b;
	backends[num_backends] = *backend;
	num_backends++;
	return NT_STATUS_OK;
}

_PUBLIC_ NTSTATUS sys_lease_init(void)
{
	static bool initialized = false;
#define _MODULE_PROTO(init) extern NTSTATUS init(void);
	STATIC_sys_lease_MODULES_PROTO;
	init_module_fn static_init[] = { STATIC_sys_lease_MODULES };

	if (initialized) return NT_STATUS_OK;
	initialized = true;

	run_init_functions(static_init);

	return NT_STATUS_OK;
}

NTSTATUS sys_lease_setup(struct sys_lease_context *ctx,
			 struct opendb_entry *e)
{
	return ctx->ops->setup(ctx, e);
}

NTSTATUS sys_lease_update(struct sys_lease_context *ctx,
			  struct opendb_entry *e)
{
	return ctx->ops->update(ctx, e);
}

NTSTATUS sys_lease_remove(struct sys_lease_context *ctx,
			  struct opendb_entry *e)
{
	return ctx->ops->remove(ctx, e);
}
