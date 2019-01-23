/* 
   Unix SMB/CIFS implementation.

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

/*
  notify implementation using inotify
*/

#include "includes.h"
#include "system/filesys.h"
#include <tevent.h>
#include "ntvfs/sysdep/sys_notify.h"
#include "../lib/util/dlinklist.h"
#include "libcli/raw/smb.h"
#include "param/param.h"

#if HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
#else
/* for older glibc varients - we can remove this eventually */
#include <linux/inotify.h>
#include <asm/unistd.h>

#ifndef HAVE_INOTIFY_INIT
/*
  glibc doesn't define these functions yet (as of March 2006)
*/
static int inotify_init(void)
{
	return syscall(__NR_inotify_init);
}

static int inotify_add_watch(int fd, const char *path, __u32 mask)
{
	return syscall(__NR_inotify_add_watch, fd, path, mask);
}

static int inotify_rm_watch(int fd, int wd)
{
	return syscall(__NR_inotify_rm_watch, fd, wd);
}
#endif
#endif


/* older glibc headers don't have these defines either */
#ifndef IN_ONLYDIR
#define IN_ONLYDIR 0x01000000
#endif
#ifndef IN_MASK_ADD
#define IN_MASK_ADD 0x20000000
#endif

struct inotify_private {
	struct sys_notify_context *ctx;
	int fd;
	struct inotify_watch_context *watches;
};

struct inotify_watch_context {
	struct inotify_watch_context *next, *prev;
	struct inotify_private *in;
	int wd;
	sys_notify_callback_t callback;
	void *private_data;
	uint32_t mask; /* the inotify mask */
	uint32_t filter; /* the windows completion filter */
	const char *path;
};


/*
  see if a particular event from inotify really does match a requested
  notify event in SMB
*/
static bool filter_match(struct inotify_watch_context *w,
			 struct inotify_event *e)
{
	if ((e->mask & w->mask) == 0) {
		/* this happens because inotify_add_watch() coalesces watches on the same
		   path, oring their masks together */
		return false;
	}

	/* SMB separates the filters for files and directories */
	if (e->mask & IN_ISDIR) {
		if ((w->filter & FILE_NOTIFY_CHANGE_DIR_NAME) == 0) {
			return false;
		}
	} else {
		if ((e->mask & IN_ATTRIB) &&
		    (w->filter & (FILE_NOTIFY_CHANGE_ATTRIBUTES|
				  FILE_NOTIFY_CHANGE_LAST_WRITE|
				  FILE_NOTIFY_CHANGE_LAST_ACCESS|
				  FILE_NOTIFY_CHANGE_EA|
				  FILE_NOTIFY_CHANGE_SECURITY))) {
			return true;
		}
		if ((e->mask & IN_MODIFY) && 
		    (w->filter & FILE_NOTIFY_CHANGE_ATTRIBUTES)) {
			return true;
		}
		if ((w->filter & FILE_NOTIFY_CHANGE_FILE_NAME) == 0) {
			return false;
		}
	}

	return true;
}
	


/*
  dispatch one inotify event
  
  the cookies are used to correctly handle renames
*/
static void inotify_dispatch(struct inotify_private *in, 
			     struct inotify_event *e, 
			     uint32_t prev_cookie,
			     struct inotify_event *e2)
{
	struct inotify_watch_context *w, *next;
	struct notify_event ne;

	/* ignore extraneous events, such as unmount and IN_IGNORED events */
	if ((e->mask & (IN_ATTRIB|IN_MODIFY|IN_CREATE|IN_DELETE|
			IN_MOVED_FROM|IN_MOVED_TO)) == 0) {
		return;
	}

	/* map the inotify mask to a action. This gets complicated for
	   renames */
	if (e->mask & IN_CREATE) {
		ne.action = NOTIFY_ACTION_ADDED;
	} else if (e->mask & IN_DELETE) {
		ne.action = NOTIFY_ACTION_REMOVED;
	} else if (e->mask & IN_MOVED_FROM) {
		if (e2 != NULL && e2->cookie == e->cookie) {
			ne.action = NOTIFY_ACTION_OLD_NAME;
		} else {
			ne.action = NOTIFY_ACTION_REMOVED;
		}
	} else if (e->mask & IN_MOVED_TO) {
		if (e->cookie == prev_cookie) {
			ne.action = NOTIFY_ACTION_NEW_NAME;
		} else {
			ne.action = NOTIFY_ACTION_ADDED;
		}
	} else {
		ne.action = NOTIFY_ACTION_MODIFIED;
	}
	ne.path = e->name;

	/* find any watches that have this watch descriptor */
	for (w=in->watches;w;w=next) {
		next = w->next;
		if (w->wd == e->wd && filter_match(w, e)) {
			w->callback(in->ctx, w->private_data, &ne);
		}
	}

	/* SMB expects a file rename to generate three events, two for
	   the rename and the other for a modify of the
	   destination. Strange! */
	if (ne.action != NOTIFY_ACTION_NEW_NAME ||
	    (e->mask & IN_ISDIR) != 0) {
		return;
	}

	ne.action = NOTIFY_ACTION_MODIFIED;
	e->mask = IN_ATTRIB;
	
	for (w=in->watches;w;w=next) {
		next = w->next;
		if (w->wd == e->wd && filter_match(w, e) &&
		    !(w->filter & FILE_NOTIFY_CHANGE_CREATION)) {
			w->callback(in->ctx, w->private_data, &ne);
		}
	}
}

/*
  called when the kernel has some events for us
*/
static void inotify_handler(struct tevent_context *ev, struct tevent_fd *fde,
			    uint16_t flags, void *private_data)
{
	struct inotify_private *in = talloc_get_type(private_data,
						     struct inotify_private);
	int bufsize = 0;
	struct inotify_event *e0, *e;
	uint32_t prev_cookie=0;

	/*
	  we must use FIONREAD as we cannot predict the length of the
	  filenames, and thus can't know how much to allocate
	  otherwise
	*/
	if (ioctl(in->fd, FIONREAD, &bufsize) != 0 || 
	    bufsize == 0) {
		DEBUG(0,("No data on inotify fd?!\n"));
		return;
	}

	e0 = e = talloc_size(in, bufsize);
	if (e == NULL) return;

	if (read(in->fd, e0, bufsize) != bufsize) {
		DEBUG(0,("Failed to read all inotify data\n"));
		talloc_free(e0);
		return;
	}

	/* we can get more than one event in the buffer */
	while (bufsize >= sizeof(*e)) {
		struct inotify_event *e2 = NULL;
		bufsize -= e->len + sizeof(*e);
		if (bufsize >= sizeof(*e)) {
			e2 = (struct inotify_event *)(e->len + sizeof(*e) + (char *)e);
		}
		inotify_dispatch(in, e, prev_cookie, e2);
		prev_cookie = e->cookie;
		e = e2;
	}

	talloc_free(e0);
}

/*
  setup the inotify handle - called the first time a watch is added on
  this context
*/
static NTSTATUS inotify_setup(struct sys_notify_context *ctx)
{
	struct inotify_private *in;
	struct tevent_fd *fde;

	in = talloc(ctx, struct inotify_private);
	NT_STATUS_HAVE_NO_MEMORY(in);

	in->fd = inotify_init();
	if (in->fd == -1) {
		DEBUG(0,("Failed to init inotify - %s\n", strerror(errno)));
		talloc_free(in);
		return map_nt_error_from_unix(errno);
	}
	in->ctx = ctx;
	in->watches = NULL;

	ctx->private_data = in;

	/* add a event waiting for the inotify fd to be readable */
	fde = tevent_add_fd(ctx->ev, in, in->fd,
			   TEVENT_FD_READ, inotify_handler, in);
	if (!fde) {
		if (errno == 0) {
			errno = ENOMEM;
		}
		DEBUG(0,("Failed to tevent_add_fd() - %s\n", strerror(errno)));
		talloc_free(in);
		return map_nt_error_from_unix(errno);
	}

	tevent_fd_set_auto_close(fde);

	return NT_STATUS_OK;
}


/*
  map from a change notify mask to a inotify mask. Remove any bits
  which we can handle
*/
static const struct {
	uint32_t notify_mask;
	uint32_t inotify_mask;
} inotify_mapping[] = {
	{FILE_NOTIFY_CHANGE_FILE_NAME,   IN_CREATE|IN_DELETE|IN_MOVED_FROM|IN_MOVED_TO},
	{FILE_NOTIFY_CHANGE_DIR_NAME,    IN_CREATE|IN_DELETE|IN_MOVED_FROM|IN_MOVED_TO},
	{FILE_NOTIFY_CHANGE_ATTRIBUTES,  IN_ATTRIB|IN_MOVED_TO|IN_MOVED_FROM|IN_MODIFY},
	{FILE_NOTIFY_CHANGE_LAST_WRITE,  IN_ATTRIB},
	{FILE_NOTIFY_CHANGE_LAST_ACCESS, IN_ATTRIB},
	{FILE_NOTIFY_CHANGE_EA,          IN_ATTRIB},
	{FILE_NOTIFY_CHANGE_SECURITY,    IN_ATTRIB}
};

static uint32_t inotify_map(struct notify_entry *e)
{
	int i;
	uint32_t out=0;
	for (i=0;i<ARRAY_SIZE(inotify_mapping);i++) {
		if (inotify_mapping[i].notify_mask & e->filter) {
			out |= inotify_mapping[i].inotify_mask;
			e->filter &= ~inotify_mapping[i].notify_mask;
		}
	}
	return out;
}

/*
  destroy a watch
*/
static int watch_destructor(struct inotify_watch_context *w)
{
	struct inotify_private *in = w->in;
	int wd = w->wd;
	DLIST_REMOVE(w->in->watches, w);

	/* only rm the watch if its the last one with this wd */
	for (w=in->watches;w;w=w->next) {
		if (w->wd == wd) break;
	}
	if (w == NULL) {
		inotify_rm_watch(in->fd, wd);
	}
	return 0;
}


/*
  add a watch. The watch is removed when the caller calls
  talloc_free() on *handle
*/
static NTSTATUS inotify_watch(struct sys_notify_context *ctx,
			      struct notify_entry *e,
			      sys_notify_callback_t callback,
			      void *private_data, 
			      void *handle_p)
{
	struct inotify_private *in;
	int wd;
	uint32_t mask;
	struct inotify_watch_context *w;
	uint32_t filter = e->filter;
	void **handle = (void **)handle_p;

	/* maybe setup the inotify fd */
	if (ctx->private_data == NULL) {
		NTSTATUS status;
		status = inotify_setup(ctx);
		NT_STATUS_NOT_OK_RETURN(status);
	}

	in = talloc_get_type(ctx->private_data, struct inotify_private);

	mask = inotify_map(e);
	if (mask == 0) {
		/* this filter can't be handled by inotify */
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* using IN_MASK_ADD allows us to cope with inotify() returning the same
	   watch descriptor for muliple watches on the same path */
	mask |= (IN_MASK_ADD | IN_ONLYDIR);

	/* get a new watch descriptor for this path */
	wd = inotify_add_watch(in->fd, e->path, mask);
	if (wd == -1) {
		e->filter = filter;
		return map_nt_error_from_unix(errno);
	}

	w = talloc(in, struct inotify_watch_context);
	if (w == NULL) {
		inotify_rm_watch(in->fd, wd);
		e->filter = filter;
		return NT_STATUS_NO_MEMORY;
	}

	w->in = in;
	w->wd = wd;
	w->callback = callback;
	w->private_data = private_data;
	w->mask = mask;
	w->filter = filter;
	w->path = talloc_strdup(w, e->path);
	if (w->path == NULL) {
		inotify_rm_watch(in->fd, wd);
		e->filter = filter;
		return NT_STATUS_NO_MEMORY;
	}

	(*handle) = w;

	DLIST_ADD(in->watches, w);

	/* the caller frees the handle to stop watching */
	talloc_set_destructor(w, watch_destructor);
	
	return NT_STATUS_OK;
}


static struct sys_notify_backend inotify = {
	.name = "inotify",
	.notify_watch = inotify_watch
};

/*
  initialialise the inotify module
 */
NTSTATUS sys_notify_inotify_init(void)
{
	/* register ourselves as a system inotify module */
	return sys_notify_register(&inotify);
}
