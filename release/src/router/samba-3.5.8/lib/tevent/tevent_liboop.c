/* 
   Unix SMB/CIFS implementation.
   main select loop and event handling
   wrapper for http://liboop.org/

   Copyright (C) Stefan Metzmacher 2005

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

#include "events.h"
#include "events_internal.h"

#include <oop.h>

/*
 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 

 NOTE: this code compiles fine, but is completly *UNTESTED*
       and is only commited as example

 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!	 
*/

static int oop_event_context_destructor(struct tevent_context *ev)
{
	oop_source_sys *oop_sys = ev->additional_data;

	oop_sys_delete(oop_sys);

	return 0;
}

/*
  create a oop_event_context structure.
*/
static int oop_event_context_init(struct tevent_context *ev, void *private_data)
{
	oop_source_sys *oop_sys = private_data;

	if (!oop_sys) {
		oop_sys = oop_sys_new();
		if (!oop_sys) {
			return -1;
		}

		talloc_set_destructor(ev, oop_event_context_destructor);
	}

	ev->additional_data = oop_sys;

	return 0;
}

static void *oop_event_fd_handler(oop_source *oop, int fd, oop_event oop_type, void *ptr)
{
	struct tevent_fd *fde = ptr;

	if (fd != fde->fd) return OOP_ERROR;

	switch(oop_type) {
		case OOP_READ:
			fde->handler(fde->event_ctx, fde, EVENT_FD_READ, fde->private_data);
			return OOP_CONTINUE;
		case OOP_WRITE:
			fde->handler(fde->event_ctx, fde, EVENT_FD_WRITE, fde->private_data);
			return OOP_CONTINUE;			
		case OOP_EXCEPTION:
			return OOP_ERROR;
		case OOP_NUM_EVENTS:
			return OOP_ERROR;
	}

	return OOP_ERROR;
}

/*
  destroy an fd_event
*/
static int oop_event_fd_destructor(struct tevent_fd *fde)
{
	struct tevent_context *ev = fde->event_ctx;
	oop_source_sys *oop_sys = ev->additional_data;
	oop_source *oop = oop_sys_source(oop_sys);

	if (fde->flags & EVENT_FD_READ)
		oop->cancel_fd(oop, fde->fd, OOP_READ);
	if (fde->flags & EVENT_FD_WRITE)
		oop->cancel_fd(oop, fde->fd, OOP_WRITE);

	if (fde->flags & EVENT_FD_AUTOCLOSE) {
		close(fde->fd);
		fde->fd = -1;
	}

	return 0;
}

/*
  add a fd based event
  return NULL on failure (memory allocation error)
*/
static struct tevent_fd *oop_event_add_fd(struct tevent_context *ev, TALLOC_CTX *mem_ctx,
					 int fd, uint16_t flags,
					 event_fd_handler_t handler,
					 void *private_data)
{
	struct tevent_fd *fde;
	oop_source_sys *oop_sys = ev->additional_data;
	oop_source *oop = oop_sys_source(oop_sys);
	
	fde = talloc(mem_ctx?mem_ctx:ev, struct tevent_fd);
	if (!fde) return NULL;

	fde->event_ctx		= ev;
	fde->fd			= fd;
	fde->flags		= flags;
	fde->handler		= handler;
	fde->private_data	= private_data;
	fde->additional_flags	= 0;
	fde->additional_data	= NULL;

	if (fde->flags & EVENT_FD_READ)
		oop->on_fd(oop, fde->fd, OOP_READ, oop_event_fd_handler, fde);
	if (fde->flags & EVENT_FD_WRITE)
		oop->on_fd(oop, fde->fd, OOP_WRITE, oop_event_fd_handler, fde);

	talloc_set_destructor(fde, oop_event_fd_destructor);

	return fde;
}

/*
  return the fd event flags
*/
static uint16_t oop_event_get_fd_flags(struct tevent_fd *fde)
{
	return fde->flags;
}

/*
  set the fd event flags
*/
static void oop_event_set_fd_flags(struct tevent_fd *fde, uint16_t flags)
{
	oop_source_sys *oop_sys;
	oop_source *oop;

	oop_sys = fde->event_ctx->additional_data;
	oop = oop_sys_source(oop_sys);

	if ((fde->flags & EVENT_FD_READ)&&(!(flags & EVENT_FD_READ)))
		oop->cancel_fd(oop, fde->fd, OOP_READ);

	if ((!(fde->flags & EVENT_FD_READ))&&(flags & EVENT_FD_READ))
		oop->on_fd(oop, fde->fd, OOP_READ, oop_event_fd_handler, fde);

	if ((fde->flags & EVENT_FD_WRITE)&&(!(flags & EVENT_FD_WRITE)))
		oop->cancel_fd(oop, fde->fd, OOP_WRITE);

	if ((!(fde->flags & EVENT_FD_WRITE))&&(flags & EVENT_FD_WRITE))
		oop->on_fd(oop, fde->fd, OOP_WRITE, oop_event_fd_handler, fde);

	fde->flags = flags;
}

static int oop_event_timed_destructor(struct tevent_timer *te);

static int oop_event_timed_deny_destructor(struct tevent_timer *te)
{
	return -1;
}

static void *oop_event_timed_handler(oop_source *oop, struct timeval t, void *ptr)
{
	struct tevent_timer *te = ptr;

	/* deny the handler to free the event */
	talloc_set_destructor(te, oop_event_timed_deny_destructor);
	te->handler(te->event_ctx, te, t, te->private_data);

	talloc_set_destructor(te, oop_event_timed_destructor);
	talloc_free(te);

	return OOP_CONTINUE;
}

/*
  destroy a timed event
*/
static int oop_event_timed_destructor(struct tevent_timer *te)
{
	struct tevent_context *ev = te->event_ctx;
	oop_source_sys *oop_sys = ev->additional_data;
	oop_source *oop = oop_sys_source(oop_sys);

	oop->cancel_time(oop, te->next_event, oop_event_timed_handler, te);

	return 0;
}

/*
  add a timed event
  return NULL on failure (memory allocation error)
*/
static struct tevent_timer *oop_event_add_timed(struct tevent_context *ev, TALLOC_CTX *mem_ctx,
					       struct timeval next_event, 
					       event_timed_handler_t handler, 
					       void *private_data) 
{
	oop_source_sys *oop_sys = ev->additional_data;
	oop_source *oop = oop_sys_source(oop_sys);
	struct tevent_timer *te;

	te = talloc(mem_ctx?mem_ctx:ev, struct tevent_timer);
	if (te == NULL) return NULL;

	te->event_ctx		= ev;
	te->next_event		= next_event;
	te->handler		= handler;
	te->private_data	= private_data;
	te->additional_data	= NULL;

	oop->on_time(oop, te->next_event, oop_event_timed_handler, te);

	talloc_set_destructor(te, oop_event_timed_destructor);

	return te;
}

/*
  do a single event loop using the events defined in ev 
*/
static int oop_event_loop_once(struct tevent_context *ev)
{
	void *oop_ret;
	oop_source_sys *oop_sys = ev->additional_data;

	oop_ret = oop_sys_run_once(oop_sys);
	if (oop_ret == OOP_CONTINUE) {
		return 0;
	}

	return -1;
}

/*
  return on failure or (with 0) if all fd events are removed
*/
static int oop_event_loop_wait(struct tevent_context *ev)
{
	void *oop_ret;
	oop_source_sys *oop_sys = ev->additional_data;

	oop_ret = oop_sys_run(oop_sys);
	if (oop_ret == OOP_CONTINUE) {
		return 0;
	}

	return -1;
}

static const struct event_ops event_oop_ops = {
	.context_init	= oop_event_context_init,
	.add_fd		= oop_event_add_fd,
	.get_fd_flags	= oop_event_get_fd_flags,
	.set_fd_flags	= oop_event_set_fd_flags,
	.add_timer	= oop_event_add_timed,
	.add_signal	= common_event_add_signal,
	.loop_once	= oop_event_loop_once,
	.loop_wait	= oop_event_loop_wait,
};

const struct event_ops *event_liboop_get_ops(void)
{
	return &event_oop_ops;
}
