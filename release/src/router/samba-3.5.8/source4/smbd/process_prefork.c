/* 
   Unix SMB/CIFS implementation.

   process model: prefork (n client connections per process)

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
#include "../tdb/include/tdb.h"
#include "lib/socket/socket.h"
#include "smbd/process_model.h"
#include "param/secrets.h"
#include "system/filesys.h"
#include "cluster/cluster.h"
#include "param/param.h"

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
static void prefork_model_init(struct tevent_context *ev)
{
	signal(SIGCHLD, SIG_IGN);
}

static void prefork_reload_after_fork(void)
{
	/* tdb needs special fork handling */
	if (tdb_reopen_all(1) == -1) {
		DEBUG(0,("prefork_reload_after_fork: tdb_reopen_all failed.\n"));
	}

	/* Ensure that the forked children do not expose identical random streams */
	set_need_random_reseed();
}

/*
  called when a listening socket becomes readable. 
*/
static void prefork_accept_connection(struct tevent_context *ev, 
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
static void prefork_new_task(struct tevent_context *ev, 
			     struct loadparm_context *lp_ctx,
			     const char *service_name,
			     void (*new_task_fn)(struct tevent_context *, struct loadparm_context *lp_ctx, struct server_id , void *), 
			     void *private_data)
{
	pid_t pid;
	int i, num_children;

	struct tevent_context *ev2, *ev_parent;

	pid = fork();

	if (pid != 0) {
		/* parent or error code ... go back to the event loop */
		return;
	}

	pid = getpid();

	/* This is now the child code. We need a completely new event_context to work with */
	ev2 = s4_event_context_init(NULL);

	/* the service has given us a private pointer that
	   encapsulates the context it needs for this new connection -
	   everything else will be freed */
	talloc_steal(ev2, private_data);

	/* this will free all the listening sockets and all state that
	   is not associated with this new connection */
	talloc_free(ev);

	setproctitle("task %s server_id[%d]", service_name, pid);

	prefork_reload_after_fork();

	/* setup this new connection: process will bind to it's sockets etc */
	new_task_fn(ev2, lp_ctx, cluster_id(pid, 0), private_data);

	num_children = lp_parm_int(lp_ctx, NULL, "prefork children", service_name, 0);
	if (num_children == 0) {

		/* We don't want any kids hanging around for this one,
		 * let the parent do all the work */
		event_loop_wait(ev2);
		
		talloc_free(ev2);
		exit(0);
	}

	/* We are now free to spawn some child proccesses */

	for (i=0; i < num_children; i++) {

		pid = fork();
		if (pid > 0) {
			continue;
		} else if (pid == -1) {
			return;
		} else {
			pid = getpid();
			setproctitle("task %s server_id[%d]", service_name, pid);

			prefork_reload_after_fork();

			/* we can't return to the top level here, as that event context is gone,
			   so we now process events in the new event context until there are no
			   more to process */	   
			event_loop_wait(ev2);
			
			talloc_free(ev2);
			exit(0);
		}
	}

	/* Don't listen on the sockets we just gave to the children */
	talloc_free(ev2);
	
	/* But we need a events system to handle reaping children */
	ev_parent = s4_event_context_init(NULL);
	
	/* TODO: Handle some events... */
	
	/* we can't return to the top level here, as that event context is gone,
	   so we now process events in the new event context until there are no
	   more to process */	   
	event_loop_wait(ev_parent);
	
	talloc_free(ev_parent);
	exit(0);

}


/* called when a task goes down */
_NORETURN_ static void prefork_terminate(struct tevent_context *ev, struct loadparm_context *lp_ctx, const char *reason) 
{
	DEBUG(2,("prefork_terminate: reason[%s]\n",reason));
}

/* called to set a title of a task or connection */
static void prefork_set_title(struct tevent_context *ev, const char *title) 
{
	if (title) {
		setproctitle("%s", title);
	} else {
		setproctitle(NULL);
	}
}

static const struct model_ops prefork_ops = {
	.name			= "prefork",
	.model_init		= prefork_model_init,
	.accept_connection	= prefork_accept_connection,
	.new_task               = prefork_new_task,
	.terminate              = prefork_terminate,
	.set_title              = prefork_set_title,
};

/*
  initialise the prefork process model, registering ourselves with the process model subsystem
 */
NTSTATUS process_model_prefork_init(void)
{
	return register_process_model(&prefork_ops);
}
