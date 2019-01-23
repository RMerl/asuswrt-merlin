/*
   Unix SMB/CIFS implementation.

   common events code for fd events

   Copyright (C) Stefan Metzmacher 2009

     ** NOTE! The following LGPL license applies to the tevent
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "replace.h"
#include "tevent.h"
#include "tevent_internal.h"
#include "tevent_util.h"

int tevent_common_fd_destructor(struct tevent_fd *fde)
{
	if (fde->event_ctx) {
		DLIST_REMOVE(fde->event_ctx->fd_events, fde);
	}

	if (fde->close_fn) {
		fde->close_fn(fde->event_ctx, fde, fde->fd, fde->private_data);
		fde->fd = -1;
	}

	return 0;
}

struct tevent_fd *tevent_common_add_fd(struct tevent_context *ev, TALLOC_CTX *mem_ctx,
				       int fd, uint16_t flags,
				       tevent_fd_handler_t handler,
				       void *private_data,
				       const char *handler_name,
				       const char *location)
{
	struct tevent_fd *fde;

	/* tevent will crash later on select() if we save
	 * a negative file descriptor. Better to fail here
	 * so that consumers will be able to debug it
	 */
	if (fd < 0) return NULL;

	fde = talloc(mem_ctx?mem_ctx:ev, struct tevent_fd);
	if (!fde) return NULL;

	fde->event_ctx		= ev;
	fde->fd			= fd;
	fde->flags		= flags;
	fde->handler		= handler;
	fde->close_fn		= NULL;
	fde->private_data	= private_data;
	fde->handler_name	= handler_name;
	fde->location		= location;
	fde->additional_flags	= 0;
	fde->additional_data	= NULL;

	DLIST_ADD(ev->fd_events, fde);

	talloc_set_destructor(fde, tevent_common_fd_destructor);

	return fde;
}
uint16_t tevent_common_fd_get_flags(struct tevent_fd *fde)
{
	return fde->flags;
}

void tevent_common_fd_set_flags(struct tevent_fd *fde, uint16_t flags)
{
	if (fde->flags == flags) return;
	fde->flags = flags;
}

void tevent_common_fd_set_close_fn(struct tevent_fd *fde,
				   tevent_fd_close_fn_t close_fn)
{
	fde->close_fn = close_fn;
}
