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
  lease (oplock) implementation using fcntl F_SETLEASE on linux
*/

#include "includes.h"
#include <tevent.h>
#include "system/filesys.h"
#include "ntvfs/sysdep/sys_lease.h"
#include "ntvfs/ntvfs.h"
#include "librpc/gen_ndr/ndr_opendb.h"
#include "../lib/util/dlinklist.h"
#include "cluster/cluster.h"

#define LINUX_LEASE_RT_SIGNAL (SIGRTMIN+1)

struct linux_lease_pending {
	struct linux_lease_pending *prev, *next;
	struct sys_lease_context *ctx;
	struct opendb_entry e;
};

/* the global linked list of pending leases */
static struct linux_lease_pending *leases;

static void linux_lease_signal_handler(struct tevent_context *ev_ctx,
				       struct tevent_signal *se,
				       int signum, int count,
				       void *_info, void *private_data)
{
	struct sys_lease_context *ctx = talloc_get_type(private_data,
					struct sys_lease_context);
	siginfo_t *info = (siginfo_t *)_info;
	struct linux_lease_pending *c;
	int got_fd = info->si_fd;

	for (c = leases; c; c = c->next) {
		int *fd = (int *)c->e.fd;

		if (got_fd == *fd) {
			break;
		}
	}

	if (!c) {
		return;
	}

	ctx->break_send(ctx->msg_ctx, &c->e, OPLOCK_BREAK_TO_NONE);
}

static int linux_lease_pending_destructor(struct linux_lease_pending *p)
{
	int ret;
	int *fd = (int *)p->e.fd;

	DLIST_REMOVE(leases, p);

	if (*fd == -1) {
		return 0;
	}

	ret = fcntl(*fd, F_SETLEASE, F_UNLCK);
	if (ret == -1) {
		DEBUG(0,("%s: failed to remove oplock: %s\n",
			__FUNCTION__, strerror(errno)));
	}

	return 0;
}

static NTSTATUS linux_lease_init(struct sys_lease_context *ctx)
{
	struct tevent_signal *se;

	se = tevent_add_signal(ctx->event_ctx, ctx,
			       LINUX_LEASE_RT_SIGNAL, SA_SIGINFO,
			       linux_lease_signal_handler, ctx);
	NT_STATUS_HAVE_NO_MEMORY(se);

	return NT_STATUS_OK;
}

static NTSTATUS linux_lease_setup(struct sys_lease_context *ctx,
				  struct opendb_entry *e)
{
	int ret;
	int *fd = (int *)e->fd;
	struct linux_lease_pending *p;

	if (e->oplock_level == OPLOCK_NONE) {
		e->fd = NULL;
		return NT_STATUS_OK;
	} else if (e->oplock_level == OPLOCK_LEVEL_II) {
		/*
		 * the linux kernel doesn't support level2 oplocks
		 * so fix up the granted oplock level
		 */
		e->oplock_level = OPLOCK_NONE;
		e->allow_level_II_oplock = false;
		e->fd = NULL;
		return NT_STATUS_OK;
	}

	p = talloc(ctx, struct linux_lease_pending);
	NT_STATUS_HAVE_NO_MEMORY(p);

	p->ctx = ctx;
	p->e = *e;

	ret = fcntl(*fd, F_SETSIG, LINUX_LEASE_RT_SIGNAL);
	if (ret == -1) {
		talloc_free(p);
		return map_nt_error_from_unix(errno);
	}

	ret = fcntl(*fd, F_SETLEASE, F_WRLCK);
	if (ret == -1) {
		talloc_free(p);
		return map_nt_error_from_unix(errno);
	}

	DLIST_ADD(leases, p);

	talloc_set_destructor(p, linux_lease_pending_destructor);

	return NT_STATUS_OK;
}

static NTSTATUS linux_lease_remove(struct sys_lease_context *ctx,
				   struct opendb_entry *e);

static NTSTATUS linux_lease_update(struct sys_lease_context *ctx,
				   struct opendb_entry *e)
{
	struct linux_lease_pending *c;

	for (c = leases; c; c = c->next) {
		if (c->e.fd == e->fd) {
			break;
		}
	}

	if (!c) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	/*
	 * set the fd pointer to NULL so that the caller
	 * will not call the remove function as the oplock
	 * is already removed
	 */
	e->fd = NULL;

	talloc_free(c);

	return NT_STATUS_OK;
}

static NTSTATUS linux_lease_remove(struct sys_lease_context *ctx,
				   struct opendb_entry *e)
{
	struct linux_lease_pending *c;

	for (c = leases; c; c = c->next) {
		if (c->e.fd == e->fd) {
			break;
		}
	}

	if (!c) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	talloc_free(c);

	return NT_STATUS_OK;
}

static struct sys_lease_ops linux_lease_ops = {
	.name	= "linux",
	.init	= linux_lease_init,
	.setup	= linux_lease_setup,
	.update	= linux_lease_update,
	.remove	= linux_lease_remove
};

/*
  initialialise the linux lease module
 */
NTSTATUS sys_lease_linux_init(void)
{
	/* register ourselves as a system lease module */
	return sys_lease_register(&linux_lease_ops);
}
