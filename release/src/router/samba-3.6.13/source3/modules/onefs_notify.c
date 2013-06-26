/*
 * Unix SMB/CIFS implementation.
 *
 * Support for change notify using OneFS's file event notification system
 *
 * Copyright (C) Andrew Tridgell, 2006
 * Copyright (C) Steven Danneman, 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* Implement handling of change notify requests on files and directories using
 * Isilon OneFS's "ifs event" file notification system.
 *
 * The structure of this file is based off the implementation of change notify
 * using the inotify system calls in smbd/notify_inotify.c */

/* TODO: We could reduce the number of file descriptors used by merging
 * multiple watch requests on the same directory into the same
 * onefs_notify_watch_context.  To do this we'd need mux/demux routines that
 * when receiving an event on that watch context would check it against the
 * CompletionFilter and WatchTree of open SMB requests, and return notify
 * events back to the proper SMB requests */

#include "includes.h"
#include "smbd/smbd.h"
#include "onefs.h"

#include <ifs/ifs_types.h>
#include <ifs/ifs_syscalls.h>
#include <isi_util/syscalls.h>

#include <sys/event.h>

#define ONEFS_IFS_EVENT_MAX_NUM 8
#define ONEFS_IFS_EVENT_MAX_BYTES (ONEFS_IFS_EVENT_MAX_NUM * \
				   sizeof(struct ifs_event))

struct onefs_notify_watch_context {
	struct sys_notify_context *ctx;
	int watch_fd;
	ino_t watch_lin;
	const char *path;
	int ifs_event_fd;
	uint32_t ifs_filter; /* the ifs event mask */
	uint32_t smb_filter; /* the windows completion filter */
	void (*callback)(struct sys_notify_context *ctx,
			 void *private_data,
			 struct notify_event *ev);
	void *private_data;
};

/**
 * Conversion map from a SMB completion filter to an IFS event mask.
 */
static const struct {
	uint32_t smb_filter;
	uint32_t ifs_filter;
} onefs_notify_conv[] = {
	{FILE_NOTIFY_CHANGE_FILE_NAME,
	    NOTE_CREATE | NOTE_DELETE | NOTE_RENAME_FROM | NOTE_RENAME_TO},
	{FILE_NOTIFY_CHANGE_DIR_NAME,
	    NOTE_CREATE | NOTE_DELETE | NOTE_RENAME_FROM | NOTE_RENAME_TO},
	{FILE_NOTIFY_CHANGE_ATTRIBUTES,
	    NOTE_CREATE | NOTE_DELETE | NOTE_RENAME_FROM | NOTE_RENAME_TO |
	    NOTE_ATTRIB},
	{FILE_NOTIFY_CHANGE_SIZE,
	    NOTE_SIZE | NOTE_EXTEND},
	{FILE_NOTIFY_CHANGE_LAST_WRITE,
	    NOTE_WRITE | NOTE_ATTRIB},
	/* OneFS doesn't set atime by default, but we can somewhat fake it by
	 * notifying for other events that imply ACCESS */
	{FILE_NOTIFY_CHANGE_LAST_ACCESS,
	    NOTE_WRITE | NOTE_ATTRIB},
	/* We don't have an ifs_event for the setting of create time, but we
	 * can fake by notifying when a "new" file is created via rename */
	{FILE_NOTIFY_CHANGE_CREATION,
	    NOTE_RENAME_TO},
	{FILE_NOTIFY_CHANGE_SECURITY,
	    NOTE_SECURITY},
	/* Unsupported bits
	FILE_NOTIFY_CHANGE_EA		 (no EAs in OneFS)
	FILE_NOTIFY_CHANGE_STREAM_NAME	 (no ifs_event equivalent)
	FILE_NOTIFY_CHANGE_STREAM_SIZE	 (no ifs_event equivalent)
	FILE_NOTIFY_CHANGE_STREAM_WRITE	 (no ifs_event equivalent) */
};

#define ONEFS_NOTIFY_UNSUPPORTED  (FILE_NOTIFY_CHANGE_LAST_ACCESS | \
				   FILE_NOTIFY_CHANGE_CREATION    | \
				   FILE_NOTIFY_CHANGE_EA          | \
				   FILE_NOTIFY_CHANGE_STREAM_NAME | \
				   FILE_NOTIFY_CHANGE_STREAM_SIZE | \
				   FILE_NOTIFY_CHANGE_STREAM_WRITE)

/**
 * Convert Windows/SMB filter/flags to IFS event filter.
 *
 * @param[in] smb_filter Windows Completion Filter sent in the SMB
 *
 * @return ifs event filter mask
 */
static uint32_t
onefs_notify_smb_filter_to_ifs_filter(uint32_t smb_filter)
{
	int i;
	uint32_t ifs_filter = 0x0;

	for (i=0;i<ARRAY_SIZE(onefs_notify_conv);i++) {
		if (onefs_notify_conv[i].smb_filter & smb_filter) {
			ifs_filter |= onefs_notify_conv[i].ifs_filter;
		}
	}

	return ifs_filter;
}

/**
 * Convert IFS filter/flags to a Windows notify action.
 *
 * Returns Win notification actions, types (1-5).
 *
 * @param[in] smb_filter Windows Completion Filter sent in the SMB
 * @param[in] ifs_filter Returned from the kernel in the ifs_event
 *
 * @return 0 if there are no more relevant flags.
 */
static int
onefs_notify_ifs_filter_to_smb_action(uint32_t smb_filter, uint32_t ifs_filter)
{
	/* Handle Windows special cases, before modifying events bitmask */

	/* Special case 1: win32api->MoveFile needs to send a modified
	 * notification on the new file, if smb_filter == ATTRIBUTES.
	 * Here we handle the case where two separate ATTR & NAME notifications
	 * have been registered.  We handle the case where both bits are set in
	 * the same registration in onefs_notify_dispatch() */
	if ((smb_filter & FILE_NOTIFY_CHANGE_ATTRIBUTES) &&
		!(smb_filter & FILE_NOTIFY_CHANGE_FILE_NAME) &&
		(ifs_filter & NOTE_FILE) && (ifs_filter & NOTE_RENAME_TO))
	{
		return NOTIFY_ACTION_MODIFIED;
	}

	/* Special case 2: Writes need to send a modified
	 * notification on the file, if smb_filter = ATTRIBUTES. */
	if ((smb_filter & FILE_NOTIFY_CHANGE_ATTRIBUTES) &&
		(ifs_filter & NOTE_FILE) && (ifs_filter & NOTE_WRITE))
	{
		return NOTIFY_ACTION_MODIFIED;
	}

	/* Loop because some events may be filtered out. Eventually all
	 * relevant events will be taken care of and returned. */
	while (1) {
		if (ifs_filter & NOTE_CREATE) {
			ifs_filter &= ~NOTE_CREATE;
			if ((smb_filter & FILE_NOTIFY_CHANGE_FILE_NAME) &&
				(ifs_filter & NOTE_FILE))
				return NOTIFY_ACTION_ADDED;
			if ((smb_filter & FILE_NOTIFY_CHANGE_DIR_NAME) &&
				(ifs_filter & NOTE_DIRECTORY))
				return NOTIFY_ACTION_ADDED;
		}
		else if (ifs_filter & NOTE_DELETE) {
			ifs_filter &= ~NOTE_DELETE;
			if ((smb_filter & FILE_NOTIFY_CHANGE_FILE_NAME) &&
				(ifs_filter & NOTE_FILE))
				return NOTIFY_ACTION_REMOVED;
			if ((smb_filter & FILE_NOTIFY_CHANGE_DIR_NAME) &&
				(ifs_filter & NOTE_DIRECTORY))
				return NOTIFY_ACTION_REMOVED;
		}
		else if (ifs_filter & NOTE_WRITE) {
			ifs_filter &= ~NOTE_WRITE;
			if ((smb_filter & FILE_NOTIFY_CHANGE_LAST_WRITE) ||
				(smb_filter & FILE_NOTIFY_CHANGE_LAST_ACCESS))
				return NOTIFY_ACTION_MODIFIED;
		}
		else if ((ifs_filter & NOTE_SIZE) || (ifs_filter & NOTE_EXTEND)) {
			ifs_filter &= ~NOTE_SIZE;
			ifs_filter &= ~NOTE_EXTEND;

			/* TODO: Do something on NOTE_DIR? */
			if ((smb_filter & FILE_NOTIFY_CHANGE_SIZE) &&
			    (ifs_filter & NOTE_FILE))
				return NOTIFY_ACTION_MODIFIED;
		}
		else if (ifs_filter & NOTE_ATTRIB) {
			ifs_filter &= ~NOTE_ATTRIB;
			/* NOTE_ATTRIB needs to be converted to a
			 * LAST_WRITE as well, because we need to send
			 * LAST_WRITE when the mtime changes. Looking into
			 * better alternatives as this causes extra LAST_WRITE
			 * notifications. We also return LAST_ACCESS as a
			 * modification to attribs implies this. */
			if ((smb_filter & FILE_NOTIFY_CHANGE_ATTRIBUTES) ||
				(smb_filter & FILE_NOTIFY_CHANGE_LAST_WRITE) ||
				(smb_filter & FILE_NOTIFY_CHANGE_LAST_ACCESS))
				return NOTIFY_ACTION_MODIFIED;
		}
		else if (ifs_filter & NOTE_LINK) {
			ifs_filter &= ~NOTE_LINK;
			/* NOTE_LINK will send out NO notifications */
		}
		else if (ifs_filter & NOTE_REVOKE) {
			ifs_filter &= ~NOTE_REVOKE;
			/* NOTE_REVOKE will send out NO notifications */
		}
		else if (ifs_filter & NOTE_RENAME_FROM)  {
			ifs_filter &= ~NOTE_RENAME_FROM;

			if (((smb_filter & FILE_NOTIFY_CHANGE_FILE_NAME) &&
				 (ifs_filter & NOTE_FILE)) ||
				((smb_filter & FILE_NOTIFY_CHANGE_DIR_NAME) &&
				 (ifs_filter & NOTE_DIRECTORY))) {
				/* Check if this is a RENAME, not a MOVE */
				if (ifs_filter & NOTE_RENAME_SAMEDIR) {
					/* Remove the NOTE_RENAME_SAMEDIR, IFF
					 * RENAME_TO is not in this event */
					if (!(ifs_filter & NOTE_RENAME_TO))
						ifs_filter &=
						    ~NOTE_RENAME_SAMEDIR;
					return NOTIFY_ACTION_OLD_NAME;
				}
				return NOTIFY_ACTION_REMOVED;
			}
		}
		else if (ifs_filter & NOTE_RENAME_TO) {
			ifs_filter &= ~NOTE_RENAME_TO;

			if (((smb_filter & FILE_NOTIFY_CHANGE_FILE_NAME) &&
				 (ifs_filter & NOTE_FILE)) ||
				((smb_filter & FILE_NOTIFY_CHANGE_DIR_NAME) &&
				 (ifs_filter & NOTE_DIRECTORY))) {
				/* Check if this is a RENAME, not a MOVE */
				if (ifs_filter & NOTE_RENAME_SAMEDIR) {
					/* Remove the NOTE_RENAME_SAMEDIR, IFF
					 * RENAME_FROM is not in this event */
					if (!(ifs_filter & NOTE_RENAME_FROM))
						ifs_filter &=
						    ~NOTE_RENAME_SAMEDIR;
					return NOTIFY_ACTION_NEW_NAME;
				}
				return NOTIFY_ACTION_ADDED;
			}
			/* RAW-NOTIFY shows us that a rename triggers a
			 * creation time change */
			if ((smb_filter & FILE_NOTIFY_CHANGE_CREATION) &&
				(ifs_filter & NOTE_FILE))
				return NOTIFY_ACTION_MODIFIED;
		}
		else if (ifs_filter & NOTE_SECURITY) {
			ifs_filter &= ~NOTE_SECURITY;

			if (smb_filter & FILE_NOTIFY_CHANGE_SECURITY)
				return NOTIFY_ACTION_MODIFIED;
		} else {
			/* No relevant flags found */
			return 0;
		}
	}
}

/**
 * Retrieve a directory path of a changed file, relative to the watched dir
 *
 * @param[in] wc watch context for the returned event
 * @param[in] e ifs_event notification returned from the kernel
 * @param[out] path name relative to the watched dir. This is talloced
 *		    off of wc and needs to be freed by the caller.
 *
 * @return true on success
 *
 * TODO: This function currently doesn't handle path names with multiple
 * encodings.  enc_get_lin_path() should be used in the future to convert
 * each path segment's encoding to UTF-8
 */
static bool
get_ifs_event_path(struct onefs_notify_watch_context *wc, struct ifs_event *e,
		   char **path)
{
	char *path_buf = NULL;
	size_t pathlen = 0;
	int error = 0;

	SMB_ASSERT(path);

	/* Lookup the path from watch_dir through our parent dir */
	if (e->namelen > 0) {
		error = lin_get_path(wc->watch_lin,
				     e->parent_lin,
				     HEAD_SNAPID,
				     e->parent_parent_lin,
				     e->parent_name_hash,
				     &pathlen, &path_buf);
		if (!error) {
			/* Only add slash if a path exists in path_buf from
			 * lin_get_path call. Windows does not expect a
			 * leading '/' */
			if (pathlen > 0)
				*path = talloc_asprintf(wc, "%s/%s",
							path_buf, e->name);
			else
				*path = talloc_asprintf(wc, "%s", e->name);
			SAFE_FREE(path_buf);
		}
	}

	/* If ifs_event didn't return a name, or we errored out of our intial
	 * path lookup, try again using the lin of the changed file */
	if (!(*path)) {
		error = lin_get_path(wc->watch_lin,
				     e->lin,
				     HEAD_SNAPID,
				     e->parent_lin,
				     e->name_hash,
				     &pathlen, &path_buf);
		if (error) {
			/* It's possible that both the lin and the parent lin
			 * are invalid (or not given) -- we will skip these
			 * events. */
			DEBUG(3,("Path lookup failed. LINS are invalid: "
				 "e->lin: 0x%llu, e->parent_lin: 0x%llu, "
				 "e->parent_parent_lin: 0x%llu\n",
				 e->lin, e->parent_lin, e->parent_parent_lin));
			SAFE_FREE(path_buf);
			return false;
		} else {
			*path = talloc_asprintf(wc, "%s", path_buf);
			DEBUG(5, ("Using path from event LIN = %s\n",
			    path_buf));
			SAFE_FREE(path_buf);
		}
	}

	/* Replacement of UNIX slashes with WIN slashes is handled at a
	 * higher layer. */

	return true;
}

/**
 * Dispatch one OneFS notify event to the general Samba code
 *
 * @param[in] wc watch context for the returned event
 * @param[in] e event returned from the kernel
 *
 * @return nothing
 */
static void
onefs_notify_dispatch(struct onefs_notify_watch_context *wc,
		      struct ifs_event *e)
{
	char *path = NULL;
	struct notify_event ne;

	DEBUG(5, ("Retrieved ifs event from kernel: lin=%#llx, ifs_events=%#x, "
		 "parent_lin=%#llx, namelen=%d, name=\"%s\"\n",
		 e->lin, e->events, e->parent_lin, e->namelen, e->name));

	/* Check validity of event returned from kernel */
	if (e->lin == 0) {
		/* The lin == 0 specifies 1 of 2 cases:
		 * 1) We are out of events. The kernel has a limited
		 *    amount (somewhere near 90000)
		 * 2) Split nodes have merged back and had data written
		 *    to them -- thus we've missed some of those events.  */
		DEBUG(3, ("We've missed some kernel ifs events!\n"));

		/* Alert higher level to the problem so it returns catch-all
		 * response to the client */
		ne.path = NULL;
		ne.action = 0;
		wc->callback(wc->ctx, wc->private_data, &ne);
	}

	if (e->lin == wc->watch_lin) {
		/* Windows doesn't report notifications on root
		 * watched directory */
		/* TODO: This should be abstracted out to the general layer
		 * instead of being handled in every notify provider */
		DEBUG(5, ("Skipping notification on root of the watched "
			 "path.\n"));
		return;
	}

	/* Retrieve the full path for the ifs event name */
	if(!get_ifs_event_path(wc, e, &path)) {
		DEBUG(3, ("Failed to convert the ifs_event lins to a path. "
			 "Skipping this event\n"));
		return;
	}

	if (!strncmp(path, ".ifsvar", 7)) {
		/* Skip notifications on file if its in ifs configuration
		 * directory */
		goto clean;
	}

	ne.path = path;

	/* Convert ifs event mask to an smb action mask */
	ne.action = onefs_notify_ifs_filter_to_smb_action(wc->smb_filter,
							  e->events);

	DEBUG(5, ("Converted smb_filter=%#x, ifs_events=%#x, to "
		  "ne.action = %d, for ne.path = %s\n",
		  wc->smb_filter, e->events, ne.action, ne.path));

	if (!ne.action)
		goto clean;

	/* Return notify_event to higher level */
	wc->callback(wc->ctx, wc->private_data, &ne);

        /* SMB expects a file rename/move to generate three actions, a
	 * rename_from/delete on the from file, a rename_to/create on the to
	 * file, and a modify for the rename_to file. If we have two separate
	 * notifications registered for ATTRIBUTES and FILENAME, this case will
	 * be handled by separate ifs_events in
	 * onefs_notify_ifs_filter_to_smb_action(). If both bits are registered
	 * in the same notification, we must send an extra MODIFIED action
	 * here. */
	if ((wc->smb_filter & FILE_NOTIFY_CHANGE_ATTRIBUTES) &&
	    (wc->smb_filter & FILE_NOTIFY_CHANGE_FILE_NAME) &&
	    (e->events & NOTE_FILE) && (e->events & NOTE_RENAME_TO))
	{
		ne.action = NOTIFY_ACTION_MODIFIED;
		wc->callback(wc->ctx, wc->private_data, &ne);
	}

	/* FALLTHROUGH */
clean:
	talloc_free(path);
	return;
}

/**
 * Callback when the kernel has some events for us
 *
 * Read events off ifs event fd and pass them to our dispatch function
 *
 * @param ev context of all tevents registered in the smbd
 * @param fde tevent struct specific to one ifs event channel
 * @param flags tevent flags passed when we added our ifs event channel fd to
 *		the main loop
 * @param private_data onefs_notify_watch_context specific to this ifs event
 *		       channel
 *
 * @return nothing
 */
static void
onefs_notify_handler(struct event_context *ev,
		     struct fd_event *fde,
		     uint16_t flags,
		     void *private_data)
{
	struct onefs_notify_watch_context *wc = NULL;
	struct ifs_event ifs_events[ONEFS_IFS_EVENT_MAX_NUM];
	ssize_t nread = 0;
	int count = 0;
	int i = 0;

	wc = talloc_get_type(private_data, struct onefs_notify_watch_context);

	/* Read as many ifs events from the notify channel as we can */
	nread = sys_read(wc->ifs_event_fd, &ifs_events,
			 ONEFS_IFS_EVENT_MAX_BYTES);
	if (nread == 0) {
		DEBUG(0,("No data found while reading ifs event fd?!\n"));
		return;
	}
	if (nread < 0) {
		DEBUG(0,("Failed to read ifs event data: %s\n",
			strerror(errno)));
		return;
	}

	count = nread / sizeof(struct ifs_event);

	DEBUG(5, ("Got %d notification events in %d bytes.\n", count, nread));

	/* Dispatch ifs_events one-at-a-time to higher level */
	for (i=0; i < count; i++) {
		onefs_notify_dispatch(wc, &ifs_events[i]);
	}
}

/**
 * Destroy the ifs event channel
 *
 * This is called from talloc_free() when the generic Samba notify layer frees
 * the onefs_notify_watch_context.
 *
 * @param[in] wc pointer to watch context which is being destroyed
 *
 * return 0 on success
*/
static int
onefs_watch_destructor(struct onefs_notify_watch_context *wc)
{
	/* The ifs_event_fd will re de-registered from the event loop by
	 * another destructor triggered from the freeing of this wc */
	close(wc->ifs_event_fd);
	return 0;
}

/**
 * Register a single change notify watch request.
 *
 * Open an event listener on a directory to watch for modifications. This
 * channel is closed by a destructor when the caller calls talloc_free()
 * on *handle.
 *
 * @param[in] vfs_handle handle given to most VFS operations
 * @param[in] ctx context (conn and tevent) for this connection
 * @param[in] e filter and path of client's notify request
 * @param[in] callback function to call when file notification event is received
 *		       from the kernel, passing that event up to Samba's
 *		       generalized change notify layer
 * @param[in] private_data opaque data given to us by the general change notify
 *			   layer which must be returned in the callback function
 * @param[out] handle_p handle returned to generalized change notify layer used
 *			to close the event channel when notification is
 *			cancelled
 *
 * @return NT_STATUS_OK unless error
 */
NTSTATUS
onefs_notify_watch(vfs_handle_struct *vfs_handle,
		   struct sys_notify_context *ctx,
		   struct notify_entry *e,
		   void (*callback)(struct sys_notify_context *ctx,
				    void *private_data,
				    struct notify_event *ev),
		   void *private_data,
		   void *handle_p)
{
	int ifs_event_fd = -1;
	uint32_t ifs_filter = 0;
	uint32_t smb_filter = e->filter;
	bool watch_tree = !!e->subdir_filter;
	struct onefs_notify_watch_context *wc = NULL;
	void **handle = (void **)handle_p;
	SMB_STRUCT_STAT sbuf;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;

	/* Fallback to default Samba implementation if kernel CN is disabled */
	if (!lp_kernel_change_notify(vfs_handle->conn->params)) {
		(*handle) = NULL;
		return NT_STATUS_OK;
	}

	/* The OneFS open path should always give us a valid fd on a directory*/
	SMB_ASSERT(e->dir_fd >= 0);

	/* Always set e->filter to 0 so we don't fallback on the default change
	 * notify backend. It's not cluster coherent or cross-protocol so we
	 * can't guarantee correctness using it. */
	e->filter = 0;
	e->subdir_filter = 0;

	/* Snapshots do not currently allow event listeners. See Isilon
	 * bug 33476 for an example of .snapshot debug spew that can occur. */
	if (e->dir_id.extid != HEAD_SNAPID)
		return NT_STATUS_INVALID_PARAMETER;

	/* Convert Completion Filter mask to IFS Event mask */
	ifs_filter = onefs_notify_smb_filter_to_ifs_filter(smb_filter);

	if (smb_filter & ONEFS_NOTIFY_UNSUPPORTED) {
		/* One or more of the filter bits could not be fully handled by
		 * the ifs_event system. To be correct, if we cannot service a
		 * bit in the completion filter we should return
		 * NT_STATUS_NOT_IMPLEMENTED to let the client know that they
		 * won't be receiving all the notify events that they asked for.
		 * Unfortunately, WinXP clients cache this error message, stop
		 * trying to send any notify requests at all, and instead return
		 * NOT_IMPLEMENTED to all requesting apps without ever sending a
		 * message to us. Thus we lie, and say we can service all bits,
		 * but simply don't return actions for the filter bits we can't
		 * detect or fully implement. */
		DEBUG(3,("One or more of the Windows completion filter bits "
			 "for \"%s\" could not be fully handled by the "
			 "ifs_event system. The failed bits are "
			 "smb_filter=%#x\n",
			 e->path, smb_filter & ONEFS_NOTIFY_UNSUPPORTED));
	}

	if (ifs_filter == 0) {
		/* None of the filter bits given are supported by the ifs_event
		 * system. Don't create a kernel notify channel, but mock
		 * up a fake handle for the caller. */
		DEBUG(3,("No bits in the Windows completion filter could be "
		         "translated to ifs_event mask for \"%s\", "
			 "smb_filter=%#x\n", e->path, smb_filter));
		(*handle) = NULL;
		return NT_STATUS_OK;
	}

	/* Register an ifs event channel for this watch request */
	ifs_event_fd = ifs_create_listener(watch_tree ?
					   EVENT_RECURSIVE :
					   EVENT_CHILDREN,
					   ifs_filter,
					   e->dir_fd);
	if (ifs_event_fd < 0) {
		DEBUG(0,("Failed to create listener for %s with \"%s\". "
			"smb_filter=0x%x, ifs_filter=0x%x, watch_tree=%u\n",
			strerror(errno), e->path, smb_filter, ifs_filter,
			watch_tree));
		return map_nt_error_from_unix(errno);
	}

	/* Create a watch context to track this change notify request */
	wc = talloc(ctx, struct onefs_notify_watch_context);
	if (wc == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto err;
	}

	/* Get LIN for directory */
	if (onefs_sys_fstat(e->dir_fd, &sbuf)) {
		DEBUG(0, ("stat on directory fd failed: %s\n",
			  strerror(errno)));
		status = map_nt_error_from_unix(errno);
		goto err;
	}

	if (sbuf.st_ex_ino == 0) {
		DEBUG(0, ("0 LIN found!\n"));
		goto err;
	}

	wc->ctx = ctx;
	wc->watch_fd = e->dir_fd;
	wc->watch_lin = sbuf.st_ex_ino;
	wc->ifs_event_fd = ifs_event_fd;
	wc->ifs_filter = ifs_filter;
	wc->smb_filter = smb_filter;
	wc->callback = callback;
	wc->private_data = private_data;
	wc->path = talloc_strdup(wc, e->path);
	if (wc->path == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto err;
	}

	(*handle) = wc;

	/* The caller frees the handle to stop watching */
	talloc_set_destructor(wc, onefs_watch_destructor);

	/* Add a tevent waiting for the ifs event fd to be readable */
	event_add_fd(ctx->ev, wc, wc->ifs_event_fd, EVENT_FD_READ,
		     onefs_notify_handler, wc);

	DEBUG(10, ("Watching for changes on \"%s\" smb_filter=0x%x, "
		   "ifs_filter=0x%x, watch_tree=%d, ifs_event_fd=%d, "
		   "dir_fd=%d, dir_lin=0x%llx\n",
		   e->path, smb_filter, ifs_filter, watch_tree,
		   ifs_event_fd, e->dir_fd, sbuf.st_ex_ino));

	return NT_STATUS_OK;

err:
	talloc_free(wc);
	SMB_ASSERT(ifs_event_fd >= 0);
	close(ifs_event_fd);
	return status;
}
