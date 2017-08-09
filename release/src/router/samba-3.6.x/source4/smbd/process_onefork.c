/*
   Unix SMB/CIFS implementation.

   process model: onefork (1 child process)

   Copyright (C) Andrew Tridgell 1992-2005
   Copyright (C) James J Myers 2003 <myersjj@samba.org>
   Copyright (C) Stefan (metze) Metzmacher 2004
   Copyright (C) Andrew Bartlett 2008 <abartlet@samba.org>
   Copyright (C) David Disseldorp 2008 <ddiss@sgi.com>

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

#include "includes.h"
#include "lib/events/events.h"
#include "lib/socket/socket.h"
#include "smbd/process_model.h"
#include "system/filesys.h"
#include "cluster/cluster.h"
#include "param/param.h"
#include "ldb_wrap.h"

#ifdef HAVE_SETPROCTITLE
#ifdef HAVE_SETPROCTITLE_H
#include <setproctitle.h>
#endif
#else
#define setproctitle none_setproctitle
static int none_setproctitle(const char *fmt, ...) PRINTF_ATTRIBUTE(1, 2);
static int none_setproctitle(const char *fmt, ...)
{
	return 0;
}
#endif

/*
  called when the process model is selected
*/
static void onefork_model_init(void)
{
	signal(SIGCHLD, SIG_IGN);
}

static void onefork_reload_after_fork(void)
{
	ldb_wrap_fork_hook();

	/* Ensure that the forked children do not expose identical random streams */
	set_need_random_reseed();
}

/*
  called when a listening socket becomes readable.
*/
static void onefork_accept_connection(struct tevent_context *ev,
				      struct loadparm_context *lp_ctx,
				      struct socket_context *listen_socket,
				       void (*new_conn)(struct tevent_context *,
							struct loadparm_context *, struct socket_context *,
							struct server_id , void *),
				       void *private_data)
{
	NTSTATUS status;
	struct socket_context *connected_socket;
	pid_t pid = getpid();

	/* accept an incoming connection. */
	status = socket_accept(listen_socket, &connected_socket);
	if (!NT_STATUS_IS_OK(status)) {
		return;
	}

	talloc_steal(private_data, connected_socket);

	new_conn(ev, lp_ctx, connected_socket, cluster_id(pid, socket_get_fd(connected_socket)), private_data);
}

/*
  called to create a new server task
*/
static void onefork_new_task(struct tevent_context *ev,
			     struct loadparm_context *lp_ctx,
			     const char *service_name,
			     void (*new_task_fn)(struct tevent_context *, struct loadparm_context *lp_ctx, struct server_id , void *),
			     void *private_data)
{
	pid_t pid;

	pid = fork();

	if (pid != 0) {
		/* parent or error code ... go back to the event loop */
		return;
	}

	pid = getpid();

	if (tevent_re_initialise(ev) != 0) {
		smb_panic("Failed to re-initialise tevent after fork");
	}

	setproctitle("task %s server_id[%d]", service_name, (int)pid);

	onefork_reload_after_fork();

	/* setup this new connection: process will bind to it's sockets etc */
	new_task_fn(ev, lp_ctx, cluster_id(pid, 0), private_data);

	event_loop_wait(ev);

	talloc_free(ev);
	exit(0);

}


/* called when a task goes down */
static void onefork_terminate(struct tevent_context *ev, struct loadparm_context *lp_ctx, const char *reason)
{
	DEBUG(2,("onefork_terminate: reason[%s]\n",reason));
}

/* called to set a title of a task or connection */
static void onefork_set_title(struct tevent_context *ev, const char *title)
{
	if (title) {
		setproctitle("%s", title);
	} else {
		setproctitle(NULL);
	}
}

static const struct model_ops onefork_ops = {
	.name			= "onefork",
	.model_init		= onefork_model_init,
	.accept_connection	= onefork_accept_connection,
	.new_task               = onefork_new_task,
	.terminate              = onefork_terminate,
	.set_title              = onefork_set_title,
};

/*
  initialise the onefork process model, registering ourselves with the process model subsystem
 */
NTSTATUS process_model_onefork_init(void)
{
	return register_process_model(&onefork_ops);
}
