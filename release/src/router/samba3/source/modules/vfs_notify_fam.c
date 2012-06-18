/*
 * FAM file notification support.
 *
 * Copyright (c) James Peach 2005
 * Copyright (c) Volker Lendecke 2007
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include "includes.h"

#include <fam.h>

#if !defined(HAVE_FAM_H_FAMCODES_TYPEDEF)
/* Gamin provides this typedef which means we can't use 'enum FAMCodes' as per
 * every other FAM implementation. Phooey.
 */
typedef enum FAMCodes FAMCodes;
#endif

/* NOTE: There are multiple versions of FAM floating around the net, each with
 * slight differences from the original SGI FAM implementation. In this file,
 * we rely only on the SGI features and do not assume any extensions. For
 * example, we do not look at FAMErrno, because it is not set by the original
 * implementation.
 *
 * Random FAM links:
 *	http://oss.sgi.com/projects/fam/
 *	http://savannah.nongnu.org/projects/fam/
 *	http://sourceforge.net/projects/bsdfam/
 */

/* ------------------------------------------------------------------------- */

struct fam_watch_context {
	struct fam_watch_context *prev, *next;
	FAMConnection *fam_connection;
	struct FAMRequest fr;
	struct sys_notify_context *sys_ctx;
	void (*callback)(struct sys_notify_context *ctx, 
			 void *private_data,
			 struct notify_event *ev);
	void *private_data;
	uint32_t mask; /* the inotify mask */
	uint32_t filter; /* the windows completion filter */
	const char *path;
};


/*
 * We want one FAM connection per smbd, not one per tcon.
 */
static FAMConnection fam_connection;
static BOOL fam_connection_initialized = False;

static struct fam_watch_context *fam_notify_list;
static void fam_handler(struct event_context *event_ctx,
			struct fd_event *fd_event,
			uint16 flags,
			void *private_data);

static NTSTATUS fam_open_connection(FAMConnection *fam_conn,
				    struct event_context *event_ctx)
{
	int res;
	char *name;

	ZERO_STRUCTP(fam_conn);
	FAMCONNECTION_GETFD(fam_conn) = -1;

	if (asprintf(&name, "smbd (%lu)", (unsigned long)sys_getpid()) == -1) {
		DEBUG(0, ("No memory\n"));
		return NT_STATUS_NO_MEMORY;
	}

	res = FAMOpen2(fam_conn, name);
	SAFE_FREE(name);

	if (res < 0) {
		DEBUG(10, ("FAM file change notifications not available\n"));
		/*
		 * No idea how to get NT_STATUS from a FAM result
		 */
		FAMCONNECTION_GETFD(fam_conn) = -1;
		return NT_STATUS_UNEXPECTED_IO_ERROR;
	}

	if (event_add_fd(event_ctx, event_ctx,
			 FAMCONNECTION_GETFD(fam_conn),
			 EVENT_FD_READ, fam_handler,
			 (void *)fam_conn) == NULL) {
		DEBUG(0, ("event_add_fd failed\n"));
		FAMClose(fam_conn);
		FAMCONNECTION_GETFD(fam_conn) = -1;
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

static void fam_reopen(FAMConnection *fam_conn,
		       struct event_context *event_ctx,
		       struct fam_watch_context *notify_list)
{
	struct fam_watch_context *ctx;

	DEBUG(5, ("Re-opening FAM connection\n"));

	FAMClose(fam_conn);

	if (!NT_STATUS_IS_OK(fam_open_connection(fam_conn, event_ctx))) {
		DEBUG(5, ("Re-opening fam connection failed\n"));
		return;
	}

	for (ctx = notify_list; ctx; ctx = ctx->next) {
		FAMMonitorDirectory(fam_conn, ctx->path, &ctx->fr, NULL);
	}
}

static void fam_handler(struct event_context *event_ctx,
			struct fd_event *fd_event,
			uint16 flags,
			void *private_data)
{
	FAMConnection *fam_conn = (FAMConnection *)private_data;
	FAMEvent fam_event;
	struct fam_watch_context *ctx;
	struct notify_event ne;

	if (FAMPending(fam_conn) == 0) {
		DEBUG(10, ("fam_handler called but nothing pending\n"));
		return;
	}

	if (FAMNextEvent(fam_conn, &fam_event) != 1) {
		DEBUG(5, ("FAMNextEvent returned an error\n"));
		TALLOC_FREE(fd_event);
		fam_reopen(fam_conn, event_ctx, fam_notify_list);
		return;
	}

	DEBUG(10, ("Got FAMCode %d for %s\n", fam_event.code,
		   fam_event.filename));

	switch (fam_event.code) {
	case FAMChanged:
		ne.action = NOTIFY_ACTION_MODIFIED;
		break;
	case FAMCreated:
		ne.action = NOTIFY_ACTION_ADDED;
		break;
	case FAMDeleted:
		ne.action = NOTIFY_ACTION_REMOVED;
		break;
	default:
		DEBUG(10, ("Ignoring code FAMCode %d for file %s\n",
			   (int)fam_event.code, fam_event.filename));
		return;
	}

	for (ctx = fam_notify_list; ctx; ctx = ctx->next) {
		if (memcmp(&fam_event.fr, &ctx->fr, sizeof(FAMRequest)) == 0) {
			break;
		}
	}

	if (ctx == NULL) {
		DEBUG(5, ("Discarding event for file %s\n",
			  fam_event.filename));
		return;
	}

	if ((ne.path = strrchr_m(fam_event.filename, '\\')) == NULL) {
		ne.path = fam_event.filename;
	}

	ctx->callback(ctx->sys_ctx, ctx->private_data, &ne);
}

static int fam_watch_context_destructor(struct fam_watch_context *ctx)
{
	if (FAMCONNECTION_GETFD(ctx->fam_connection) != -1) {
		FAMCancelMonitor(&fam_connection, &ctx->fr);
	}
	DLIST_REMOVE(fam_notify_list, ctx);
	return 0;
}

/*
  add a watch. The watch is removed when the caller calls
  talloc_free() on *handle
*/
static NTSTATUS fam_watch(vfs_handle_struct *vfs_handle,
			  struct sys_notify_context *ctx,
			  struct notify_entry *e,
			  void (*callback)(struct sys_notify_context *ctx, 
					   void *private_data,
					   struct notify_event *ev),
			  void *private_data, 
			  void *handle_p)
{
	const uint32 fam_mask = (FILE_NOTIFY_CHANGE_FILE_NAME|
				 FILE_NOTIFY_CHANGE_DIR_NAME);
	struct fam_watch_context *watch;
	void **handle = (void **)handle_p;

	if ((e->filter & fam_mask) == 0) {
		DEBUG(10, ("filter = %u, ignoring in FAM\n", e->filter));
		return NT_STATUS_OK;
	}

	if (!fam_connection_initialized) {
		if (!NT_STATUS_IS_OK(fam_open_connection(&fam_connection,
							 ctx->ev))) {
			/*
			 * Just let smbd do all the work itself
			 */
			return NT_STATUS_OK;
		}
		fam_connection_initialized = True;
	}

	if (!(watch = TALLOC_P(ctx, struct fam_watch_context))) {
		return NT_STATUS_NO_MEMORY;
	}

	watch->fam_connection = &fam_connection;

	watch->callback = callback;
	watch->private_data = private_data;
	watch->sys_ctx = ctx;

	if (!(watch->path = talloc_strdup(watch, e->path))) {
		DEBUG(0, ("talloc_asprintf failed\n"));
		TALLOC_FREE(watch);
		return NT_STATUS_NO_MEMORY;
	}

	/*
	 * The FAM module in this early state will only take care of
	 * FAMCreated and FAMDeleted events, Leave the rest to
	 * notify_internal.c
	 */

	watch->filter = fam_mask;
	e->filter &= ~fam_mask;

	DLIST_ADD(fam_notify_list, watch);
	talloc_set_destructor(watch, fam_watch_context_destructor);

	/*
	 * Only directories monitored so far
	 */

	if (FAMCONNECTION_GETFD(watch->fam_connection) != -1) {
		FAMMonitorDirectory(watch->fam_connection, watch->path,
				    &watch->fr, NULL);
	}
	else {
		/*
		 * If the re-open is successful, this will establish the
		 * FAMMonitor from the list
		 */
		fam_reopen(watch->fam_connection, ctx->ev, fam_notify_list);
	}

	*handle = (void *)watch;

	return NT_STATUS_OK;
}

/* VFS operations structure */

static vfs_op_tuple notify_fam_op_tuples[] = {

	{SMB_VFS_OP(fam_watch),
	 SMB_VFS_OP_NOTIFY_WATCH,
	 SMB_VFS_LAYER_OPAQUE},

	{SMB_VFS_OP(NULL),
	 SMB_VFS_OP_NOOP,
	 SMB_VFS_LAYER_NOOP}

};


NTSTATUS vfs_notify_fam_init(void);
NTSTATUS vfs_notify_fam_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "notify_fam",
				notify_fam_op_tuples);
}
