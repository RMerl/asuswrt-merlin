/* 
   Unix SMB/CIFS implementation.

   process model: standard (1 process per client connection)

   Copyright (C) Andrew Tridgell 1992-2005
   Copyright (C) James J Myers 2003 <myersjj@samba.org>
   Copyright (C) Stefan (metze) Metzmacher 2004
   
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

/* we hold a pipe open in the parent, and the any child
   processes wait for EOF on that pipe. This ensures that
   children die when the parent dies */
static int child_pipe[2];

/*
  called when the process model is selected
*/
static void standard_model_init(void)
{
	pipe(child_pipe);
	signal(SIGCHLD, SIG_IGN);
}

/*
  handle EOF on the child pipe
*/
static void standard_pipe_handler(struct tevent_context *event_ctx, struct tevent_fd *fde, 
				  uint16_t flags, void *private_data)
{
	DEBUG(10,("Child %d exiting\n", (int)getpid()));
	exit(0);
}

/*
  called when a listening socket becomes readable. 
*/
static void standard_accept_connection(struct tevent_context *ev, 
				       struct loadparm_context *lp_ctx,
				       struct socket_context *sock, 
				       void (*new_conn)(struct tevent_context *,
							struct loadparm_context *, struct socket_context *, 
							struct server_id , void *), 
				       void *private_data)
{
	NTSTATUS status;
	struct socket_context *sock2;
	pid_t pid;
	struct socket_address *c, *s;

	/* accept an incoming connection. */
	status = socket_accept(sock, &sock2);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("standard_accept_connection: accept: %s\n",
			 nt_errstr(status)));
		/* this looks strange, but is correct. We need to throttle things until
		   the system clears enough resources to handle this new socket */
		sleep(1);
		return;
	}

	pid = fork();

	if (pid != 0) {
		/* parent or error code ... */
		talloc_free(sock2);
		/* go back to the event loop */
		return;
	}

	pid = getpid();

	/* This is now the child code. We need a completely new event_context to work with */

	if (tevent_re_initialise(ev) != 0) {
		smb_panic("Failed to re-initialise tevent after fork");
	}

	/* this will free all the listening sockets and all state that
	   is not associated with this new connection */
	talloc_free(sock);

	/* we don't care if the dup fails, as its only a select()
	   speed optimisation */
	socket_dup(sock2);
			
	/* tdb needs special fork handling */
	ldb_wrap_fork_hook();

	tevent_add_fd(ev, ev, child_pipe[0], TEVENT_FD_READ,
		      standard_pipe_handler, NULL);
	close(child_pipe[1]);

	/* Ensure that the forked children do not expose identical random streams */
	set_need_random_reseed();

	/* setup the process title */
	c = socket_get_peer_addr(sock2, ev);
	s = socket_get_my_addr(sock2, ev);
	if (s && c) {
		setproctitle("conn c[%s:%u] s[%s:%u] server_id[%d]",
			     c->addr, c->port, s->addr, s->port, (int)pid);
	}
	talloc_free(c);
	talloc_free(s);

	/* setup this new connection.  Cluster ID is PID based for this process modal */
	new_conn(ev, lp_ctx, sock2, cluster_id(pid, 0), private_data);

	/* we can't return to the top level here, as that event context is gone,
	   so we now process events in the new event context until there are no
	   more to process */	   
	event_loop_wait(ev);

	talloc_free(ev);
	exit(0);
}

/*
  called to create a new server task
*/
static void standard_new_task(struct tevent_context *ev, 
			      struct loadparm_context *lp_ctx,
			      const char *service_name,
			      void (*new_task)(struct tevent_context *, struct loadparm_context *lp_ctx, struct server_id , void *),
			      void *private_data)
{
	pid_t pid;

	pid = fork();

	if (pid != 0) {
		/* parent or error code ... go back to the event loop */
		return;
	}

	pid = getpid();

	/* this will free all the listening sockets and all state that
	   is not associated with this new connection */
	if (tevent_re_initialise(ev) != 0) {
		smb_panic("Failed to re-initialise tevent after fork");
	}

	/* ldb/tdb need special fork handling */
	ldb_wrap_fork_hook();

	tevent_add_fd(ev, ev, child_pipe[0], TEVENT_FD_READ,
		      standard_pipe_handler, NULL);
	close(child_pipe[1]);

	/* Ensure that the forked children do not expose identical random streams */
	set_need_random_reseed();

	setproctitle("task %s server_id[%d]", service_name, (int)pid);

	/* setup this new task.  Cluster ID is PID based for this process modal */
	new_task(ev, lp_ctx, cluster_id(pid, 0), private_data);

	/* we can't return to the top level here, as that event context is gone,
	   so we now process events in the new event context until there are no
	   more to process */	   
	event_loop_wait(ev);

	talloc_free(ev);
	exit(0);
}


/* called when a task goes down */
_NORETURN_ static void standard_terminate(struct tevent_context *ev, struct loadparm_context *lp_ctx,
					  const char *reason) 
{
	DEBUG(2,("standard_terminate: reason[%s]\n",reason));

	talloc_free(ev);

	/* this reload_charcnv() has the effect of freeing the iconv context memory,
	   which makes leak checking easier */
	reload_charcnv(lp_ctx);

	/* terminate this process */
	exit(0);
}

/* called to set a title of a task or connection */
static void standard_set_title(struct tevent_context *ev, const char *title) 
{
	if (title) {
		setproctitle("%s", title);
	} else {
		setproctitle(NULL);
	}
}

static const struct model_ops standard_ops = {
	.name			= "standard",
	.model_init		= standard_model_init,
	.accept_connection	= standard_accept_connection,
	.new_task               = standard_new_task,
	.terminate              = standard_terminate,
	.set_title              = standard_set_title,
};

/*
  initialise the standard process model, registering ourselves with the process model subsystem
 */
NTSTATUS process_model_standard_init(void)
{
	return register_process_model(&standard_ops);
}
