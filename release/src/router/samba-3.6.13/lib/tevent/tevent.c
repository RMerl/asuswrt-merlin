/* 
   Unix SMB/CIFS implementation.
   main select loop and event handling
   Copyright (C) Andrew Tridgell 2003
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

/*
  PLEASE READ THIS BEFORE MODIFYING!

  This module is a general abstraction for the main select loop and
  event handling. Do not ever put any localised hacks in here, instead
  register one of the possible event types and implement that event
  somewhere else.

  There are 2 types of event handling that are handled in this module:

  1) a file descriptor becoming readable or writeable. This is mostly
     used for network sockets, but can be used for any type of file
     descriptor. You may only register one handler for each file
     descriptor/io combination or you will get unpredictable results
     (this means that you can have a handler for read events, and a
     separate handler for write events, but not two handlers that are
     both handling read events)

  2) a timed event. You can register an event that happens at a
     specific time.  You can register as many of these as you
     like. They are single shot - add a new timed event in the event
     handler to get another event.

  To setup a set of events you first need to create a event_context
  structure using the function tevent_context_init(); This returns a
  'struct tevent_context' that you use in all subsequent calls.

  After that you can add/remove events that you are interested in
  using tevent_add_*() and talloc_free()

  Finally, you call tevent_loop_wait_once() to block waiting for one of the
  events to occor or tevent_loop_wait() which will loop
  forever.

*/
#include "replace.h"
#include "system/filesys.h"
#define TEVENT_DEPRECATED 1
#include "tevent.h"
#include "tevent_internal.h"
#include "tevent_util.h"

struct tevent_ops_list {
	struct tevent_ops_list *next, *prev;
	const char *name;
	const struct tevent_ops *ops;
};

/* list of registered event backends */
static struct tevent_ops_list *tevent_backends = NULL;
static char *tevent_default_backend = NULL;

/*
  register an events backend
*/
bool tevent_register_backend(const char *name, const struct tevent_ops *ops)
{
	struct tevent_ops_list *e;

	for (e = tevent_backends; e != NULL; e = e->next) {
		if (0 == strcmp(e->name, name)) {
			/* already registered, skip it */
			return true;
		}
	}

	e = talloc(NULL, struct tevent_ops_list);
	if (e == NULL) return false;

	e->name = name;
	e->ops = ops;
	DLIST_ADD(tevent_backends, e);

	return true;
}

/*
  set the default event backend
 */
void tevent_set_default_backend(const char *backend)
{
	talloc_free(tevent_default_backend);
	tevent_default_backend = talloc_strdup(NULL, backend);
}

/*
  initialise backends if not already done
*/
static void tevent_backend_init(void)
{
	tevent_select_init();
	tevent_poll_init();
	tevent_standard_init();
#ifdef HAVE_EPOLL
	tevent_epoll_init();
#endif
}

/*
  list available backends
*/
const char **tevent_backend_list(TALLOC_CTX *mem_ctx)
{
	const char **list = NULL;
	struct tevent_ops_list *e;

	tevent_backend_init();

	for (e=tevent_backends;e;e=e->next) {
		list = ev_str_list_add(list, e->name);
	}

	talloc_steal(mem_ctx, list);

	return list;
}

int tevent_common_context_destructor(struct tevent_context *ev)
{
	struct tevent_fd *fd, *fn;
	struct tevent_timer *te, *tn;
	struct tevent_immediate *ie, *in;
	struct tevent_signal *se, *sn;

	if (ev->pipe_fde) {
		talloc_free(ev->pipe_fde);
		close(ev->pipe_fds[0]);
		close(ev->pipe_fds[1]);
		ev->pipe_fde = NULL;
	}

	for (fd = ev->fd_events; fd; fd = fn) {
		fn = fd->next;
		fd->event_ctx = NULL;
		DLIST_REMOVE(ev->fd_events, fd);
	}

	for (te = ev->timer_events; te; te = tn) {
		tn = te->next;
		te->event_ctx = NULL;
		DLIST_REMOVE(ev->timer_events, te);
	}

	for (ie = ev->immediate_events; ie; ie = in) {
		in = ie->next;
		ie->event_ctx = NULL;
		ie->cancel_fn = NULL;
		DLIST_REMOVE(ev->immediate_events, ie);
	}

	for (se = ev->signal_events; se; se = sn) {
		sn = se->next;
		se->event_ctx = NULL;
		DLIST_REMOVE(ev->signal_events, se);
		/*
		 * This is important, Otherwise signals
		 * are handled twice in child. eg, SIGHUP.
		 * one added in parent, and another one in
		 * the child. -- BoYang
		 */
		tevent_cleanup_pending_signal_handlers(se);
	}

	return 0;
}

/*
  create a event_context structure for a specific implemementation.
  This must be the first events call, and all subsequent calls pass
  this event_context as the first element. Event handlers also
  receive this as their first argument.

  This function is for allowing third-party-applications to hook in gluecode
  to their own event loop code, so that they can make async usage of our client libs

  NOTE: use tevent_context_init() inside of samba!
*/
static struct tevent_context *tevent_context_init_ops(TALLOC_CTX *mem_ctx,
						      const struct tevent_ops *ops)
{
	struct tevent_context *ev;
	int ret;

	ev = talloc_zero(mem_ctx, struct tevent_context);
	if (!ev) return NULL;

	talloc_set_destructor(ev, tevent_common_context_destructor);

	ev->ops = ops;

	ret = ev->ops->context_init(ev);
	if (ret != 0) {
		talloc_free(ev);
		return NULL;
	}

	return ev;
}

/*
  create a event_context structure. This must be the first events
  call, and all subsequent calls pass this event_context as the first
  element. Event handlers also receive this as their first argument.
*/
struct tevent_context *tevent_context_init_byname(TALLOC_CTX *mem_ctx,
						  const char *name)
{
	struct tevent_ops_list *e;

	tevent_backend_init();

	if (name == NULL) {
		name = tevent_default_backend;
	}
	if (name == NULL) {
		name = "standard";
	}

	for (e=tevent_backends;e;e=e->next) {
		if (strcmp(name, e->name) == 0) {
			return tevent_context_init_ops(mem_ctx, e->ops);
		}
	}
	return NULL;
}


/*
  create a event_context structure. This must be the first events
  call, and all subsequent calls pass this event_context as the first
  element. Event handlers also receive this as their first argument.
*/
struct tevent_context *tevent_context_init(TALLOC_CTX *mem_ctx)
{
	return tevent_context_init_byname(mem_ctx, NULL);
}

/*
  add a fd based event
  return NULL on failure (memory allocation error)
*/
struct tevent_fd *_tevent_add_fd(struct tevent_context *ev,
				 TALLOC_CTX *mem_ctx,
				 int fd,
				 uint16_t flags,
				 tevent_fd_handler_t handler,
				 void *private_data,
				 const char *handler_name,
				 const char *location)
{
	return ev->ops->add_fd(ev, mem_ctx, fd, flags, handler, private_data,
			       handler_name, location);
}

/*
  set a close function on the fd event
*/
void tevent_fd_set_close_fn(struct tevent_fd *fde,
			    tevent_fd_close_fn_t close_fn)
{
	if (!fde) return;
	if (!fde->event_ctx) return;
	fde->event_ctx->ops->set_fd_close_fn(fde, close_fn);
}

static void tevent_fd_auto_close_fn(struct tevent_context *ev,
				    struct tevent_fd *fde,
				    int fd,
				    void *private_data)
{
	close(fd);
}

void tevent_fd_set_auto_close(struct tevent_fd *fde)
{
	tevent_fd_set_close_fn(fde, tevent_fd_auto_close_fn);
}

/*
  return the fd event flags
*/
uint16_t tevent_fd_get_flags(struct tevent_fd *fde)
{
	if (!fde) return 0;
	if (!fde->event_ctx) return 0;
	return fde->event_ctx->ops->get_fd_flags(fde);
}

/*
  set the fd event flags
*/
void tevent_fd_set_flags(struct tevent_fd *fde, uint16_t flags)
{
	if (!fde) return;
	if (!fde->event_ctx) return;
	fde->event_ctx->ops->set_fd_flags(fde, flags);
}

bool tevent_signal_support(struct tevent_context *ev)
{
	if (ev->ops->add_signal) {
		return true;
	}
	return false;
}

static void (*tevent_abort_fn)(const char *reason);

void tevent_set_abort_fn(void (*abort_fn)(const char *reason))
{
	tevent_abort_fn = abort_fn;
}

static void tevent_abort(struct tevent_context *ev, const char *reason)
{
	tevent_debug(ev, TEVENT_DEBUG_FATAL,
		     "abort: %s\n", reason);

	if (!tevent_abort_fn) {
		abort();
	}

	tevent_abort_fn(reason);
}

/*
  add a timer event
  return NULL on failure
*/
struct tevent_timer *_tevent_add_timer(struct tevent_context *ev,
				       TALLOC_CTX *mem_ctx,
				       struct timeval next_event,
				       tevent_timer_handler_t handler,
				       void *private_data,
				       const char *handler_name,
				       const char *location)
{
	return ev->ops->add_timer(ev, mem_ctx, next_event, handler, private_data,
				  handler_name, location);
}

/*
  allocate an immediate event
  return NULL on failure (memory allocation error)
*/
struct tevent_immediate *_tevent_create_immediate(TALLOC_CTX *mem_ctx,
						  const char *location)
{
	struct tevent_immediate *im;

	im = talloc(mem_ctx, struct tevent_immediate);
	if (im == NULL) return NULL;

	im->prev		= NULL;
	im->next		= NULL;
	im->event_ctx		= NULL;
	im->create_location	= location;
	im->handler		= NULL;
	im->private_data	= NULL;
	im->handler_name	= NULL;
	im->schedule_location	= NULL;
	im->cancel_fn		= NULL;
	im->additional_data	= NULL;

	return im;
}

/*
  schedule an immediate event
  return NULL on failure
*/
void _tevent_schedule_immediate(struct tevent_immediate *im,
				struct tevent_context *ev,
				tevent_immediate_handler_t handler,
				void *private_data,
				const char *handler_name,
				const char *location)
{
	ev->ops->schedule_immediate(im, ev, handler, private_data,
				    handler_name, location);
}

/*
  add a signal event

  sa_flags are flags to sigaction(2)

  return NULL on failure
*/
struct tevent_signal *_tevent_add_signal(struct tevent_context *ev,
					 TALLOC_CTX *mem_ctx,
					 int signum,
					 int sa_flags,
					 tevent_signal_handler_t handler,
					 void *private_data,
					 const char *handler_name,
					 const char *location)
{
	return ev->ops->add_signal(ev, mem_ctx, signum, sa_flags, handler, private_data,
				   handler_name, location);
}

void tevent_loop_allow_nesting(struct tevent_context *ev)
{
	ev->nesting.allowed = true;
}

void tevent_loop_set_nesting_hook(struct tevent_context *ev,
				  tevent_nesting_hook hook,
				  void *private_data)
{
	if (ev->nesting.hook_fn && 
	    (ev->nesting.hook_fn != hook ||
	     ev->nesting.hook_private != private_data)) {
		/* the way the nesting hook code is currently written
		   we cannot support two different nesting hooks at the
		   same time. */
		tevent_abort(ev, "tevent: Violation of nesting hook rules\n");
	}
	ev->nesting.hook_fn = hook;
	ev->nesting.hook_private = private_data;
}

static void tevent_abort_nesting(struct tevent_context *ev, const char *location)
{
	const char *reason;

	reason = talloc_asprintf(NULL, "tevent_loop_once() nesting at %s",
				 location);
	if (!reason) {
		reason = "tevent_loop_once() nesting";
	}

	tevent_abort(ev, reason);
}

/*
  do a single event loop using the events defined in ev 
*/
int _tevent_loop_once(struct tevent_context *ev, const char *location)
{
	int ret;
	void *nesting_stack_ptr = NULL;

	ev->nesting.level++;

	if (ev->nesting.level > 1) {
		if (!ev->nesting.allowed) {
			tevent_abort_nesting(ev, location);
			errno = ELOOP;
			return -1;
		}
	}
	if (ev->nesting.level > 0) {
		if (ev->nesting.hook_fn) {
			int ret2;
			ret2 = ev->nesting.hook_fn(ev,
						   ev->nesting.hook_private,
						   ev->nesting.level,
						   true,
						   (void *)&nesting_stack_ptr,
						   location);
			if (ret2 != 0) {
				ret = ret2;
				goto done;
			}
		}
	}

	ret = ev->ops->loop_once(ev, location);

	if (ev->nesting.level > 0) {
		if (ev->nesting.hook_fn) {
			int ret2;
			ret2 = ev->nesting.hook_fn(ev,
						   ev->nesting.hook_private,
						   ev->nesting.level,
						   false,
						   (void *)&nesting_stack_ptr,
						   location);
			if (ret2 != 0) {
				ret = ret2;
				goto done;
			}
		}
	}

done:
	ev->nesting.level--;
	return ret;
}

/*
  this is a performance optimization for the samba4 nested event loop problems
*/
int _tevent_loop_until(struct tevent_context *ev,
		       bool (*finished)(void *private_data),
		       void *private_data,
		       const char *location)
{
	int ret = 0;
	void *nesting_stack_ptr = NULL;

	ev->nesting.level++;

	if (ev->nesting.level > 1) {
		if (!ev->nesting.allowed) {
			tevent_abort_nesting(ev, location);
			errno = ELOOP;
			return -1;
		}
	}
	if (ev->nesting.level > 0) {
		if (ev->nesting.hook_fn) {
			int ret2;
			ret2 = ev->nesting.hook_fn(ev,
						   ev->nesting.hook_private,
						   ev->nesting.level,
						   true,
						   (void *)&nesting_stack_ptr,
						   location);
			if (ret2 != 0) {
				ret = ret2;
				goto done;
			}
		}
	}

	while (!finished(private_data)) {
		ret = ev->ops->loop_once(ev, location);
		if (ret != 0) {
			break;
		}
	}

	if (ev->nesting.level > 0) {
		if (ev->nesting.hook_fn) {
			int ret2;
			ret2 = ev->nesting.hook_fn(ev,
						   ev->nesting.hook_private,
						   ev->nesting.level,
						   false,
						   (void *)&nesting_stack_ptr,
						   location);
			if (ret2 != 0) {
				ret = ret2;
				goto done;
			}
		}
	}

done:
	ev->nesting.level--;
	return ret;
}

/*
  return on failure or (with 0) if all fd events are removed
*/
int tevent_common_loop_wait(struct tevent_context *ev,
			    const char *location)
{
	/*
	 * loop as long as we have events pending
	 */
	while (ev->fd_events ||
	       ev->timer_events ||
	       ev->immediate_events ||
	       ev->signal_events) {
		int ret;
		ret = _tevent_loop_once(ev, location);
		if (ret != 0) {
			tevent_debug(ev, TEVENT_DEBUG_FATAL,
				     "_tevent_loop_once() failed: %d - %s\n",
				     ret, strerror(errno));
			return ret;
		}
	}

	tevent_debug(ev, TEVENT_DEBUG_WARNING,
		     "tevent_common_loop_wait() out of events\n");
	return 0;
}

/*
  return on failure or (with 0) if all fd events are removed
*/
int _tevent_loop_wait(struct tevent_context *ev, const char *location)
{
	return ev->ops->loop_wait(ev, location);
}


/*
  re-initialise a tevent context. This leaves you with the same
  event context, but all events are wiped and the structure is
  re-initialised. This is most useful after a fork()  

  zero is returned on success, non-zero on failure
*/
int tevent_re_initialise(struct tevent_context *ev)
{
	tevent_common_context_destructor(ev);

	return ev->ops->context_init(ev);
}
