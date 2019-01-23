/*
   Unix SMB/CIFS implementation.
   Main SMB server routines
   Copyright (C) Andrew Tridgell		1992-1998
   Copyright (C) Martin Pool			2002
   Copyright (C) Jelmer Vernooij		2002-2003
   Copyright (C) Volker Lendecke		1993-2007
   Copyright (C) Jeremy Allison			1993-2007

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
#include "system/filesys.h"
#include "popt_common.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "registry/reg_init_full.h"
#include "libcli/auth/schannel.h"
#include "secrets.h"
#include "memcache.h"
#include "ctdbd_conn.h"
#include "printing/printer_list.h"
#include "rpc_server/rpc_ep_setup.h"
#include "printing/pcap.h"
#include "printing.h"
#include "serverid.h"
#include "passdb.h"
#include "auth.h"
#include "messages.h"
#include "smbprofile.h"

extern void start_epmd(struct tevent_context *ev_ctx,
		       struct messaging_context *msg_ctx);

extern void start_spoolssd(struct event_context *ev_ctx,
			   struct messaging_context *msg_ctx);

#ifdef WITH_DFS
extern int dcelogin_atmost_once;
#endif /* WITH_DFS */

static void smbd_set_server_fd(int fd)
{
	struct smbd_server_connection *sconn = smbd_server_conn;
	char addr[INET6_ADDRSTRLEN];
	const char *name;

	sconn->sock = fd;

	/*
	 * Initialize sconn->client_id: If we can't find the client's
	 * name, default to its address.
	 */

	if (sconn->client_id.name != NULL &&
	    sconn->client_id.name != sconn->client_id.addr) {
		talloc_free(discard_const_p(char, sconn->client_id.name));
		sconn->client_id.name = NULL;
	}

	client_addr(fd, sconn->client_id.addr, sizeof(sconn->client_id.addr));

	name = client_name(sconn->sock);
	if (strcmp(name, "UNKNOWN") != 0) {
		name = talloc_strdup(sconn, name);
	} else {
		name = NULL;
	}
	sconn->client_id.name =
		(name != NULL) ? name : sconn->client_id.addr;

	sub_set_socket_ids(sconn->client_id.addr, sconn->client_id.name,
			   client_socket_addr(sconn->sock, addr,
					      sizeof(addr)));
}

struct event_context *smbd_event_context(void)
{
	return server_event_context();
}

/*******************************************************************
 What to do when smb.conf is updated.
 ********************************************************************/

static void smb_conf_updated(struct messaging_context *msg,
			     void *private_data,
			     uint32_t msg_type,
			     struct server_id server_id,
			     DATA_BLOB *data)
{
	struct tevent_context *ev_ctx =
		talloc_get_type_abort(private_data, struct tevent_context);

	DEBUG(10,("smb_conf_updated: Got message saying smb.conf was "
		  "updated. Reloading.\n"));
	change_to_root_user();
	reload_services(msg, smbd_server_conn->sock, False);
	/* printer reload triggered by background printing process */
}

/*******************************************************************
 What to do when printcap is updated.
 ********************************************************************/

static void smb_pcap_updated(struct messaging_context *msg,
			     void *private_data,
			     uint32_t msg_type,
			     struct server_id server_id,
			     DATA_BLOB *data)
{
	struct tevent_context *ev_ctx =
		talloc_get_type_abort(private_data, struct tevent_context);
#ifndef PRINTER_SUPPORT
	return;
#endif
	DEBUG(10,("Got message saying pcap was updated. Reloading.\n"));
	change_to_root_user();
	reload_printers(ev_ctx, msg);
}

static void smbd_sig_term_handler(struct tevent_context *ev,
				  struct tevent_signal *se,
				  int signum,
				  int count,
				  void *siginfo,
				  void *private_data)
{
	exit_server_cleanly("termination signal");
}

static void smbd_setup_sig_term_handler(void)
{
	struct tevent_signal *se;

	se = tevent_add_signal(smbd_event_context(),
			       smbd_event_context(),
			       SIGTERM, 0,
			       smbd_sig_term_handler,
			       NULL);
	if (!se) {
		exit_server("failed to setup SIGTERM handler");
	}
}

static void smbd_sig_hup_handler(struct tevent_context *ev,
				 struct tevent_signal *se,
				 int signum,
				 int count,
				 void *siginfo,
				 void *private_data)
{
	struct messaging_context *msg_ctx = talloc_get_type_abort(
		private_data, struct messaging_context);
	change_to_root_user();
	DEBUG(1,("Reloading services after SIGHUP\n"));
	reload_services(msg_ctx, smbd_server_conn->sock, false);
}

static void smbd_setup_sig_hup_handler(struct tevent_context *ev,
				       struct messaging_context *msg_ctx)
{
	struct tevent_signal *se;

	se = tevent_add_signal(ev, ev, SIGHUP, 0, smbd_sig_hup_handler,
			       msg_ctx);
	if (!se) {
		exit_server("failed to setup SIGHUP handler");
	}
}


/*******************************************************************
 Delete a statcache entry.
 ********************************************************************/

static void smb_stat_cache_delete(struct messaging_context *msg,
				  void *private_data,
				  uint32_t msg_tnype,
				  struct server_id server_id,
				  DATA_BLOB *data)
{
	const char *name = (const char *)data->data;
	DEBUG(10,("smb_stat_cache_delete: delete name %s\n", name));
	stat_cache_delete(name);
}

/****************************************************************************
  Send a SIGTERM to our process group.
*****************************************************************************/

static void  killkids(void)
{
	if(am_parent) kill(0,SIGTERM);
}

/****************************************************************************
 Process a sam sync message - not sure whether to do this here or
 somewhere else.
****************************************************************************/

static void msg_sam_sync(struct messaging_context *msg,
			 void *private_data,
			 uint32_t msg_type,
			 struct server_id server_id,
			 DATA_BLOB *data)
{
        DEBUG(10, ("** sam sync message received, ignoring\n"));
}

static void msg_exit_server(struct messaging_context *msg,
			    void *private_data,
			    uint32_t msg_type,
			    struct server_id server_id,
			    DATA_BLOB *data)
{
	DEBUG(3, ("got a SHUTDOWN message\n"));
	exit_server_cleanly(NULL);
}

#ifdef DEVELOPER
static void msg_inject_fault(struct messaging_context *msg,
			     void *private_data,
			     uint32_t msg_type,
			     struct server_id src,
			     DATA_BLOB *data)
{
	int sig;

	if (data->length != sizeof(sig)) {
		DEBUG(0, ("Process %s sent bogus signal injection request\n",
			  procid_str_static(&src)));
		return;
	}

	sig = *(int *)data->data;
	if (sig == -1) {
		exit_server("internal error injected");
		return;
	}

#if HAVE_STRSIGNAL
	DEBUG(0, ("Process %s requested injection of signal %d (%s)\n",
		  procid_str_static(&src), sig, strsignal(sig)));
#else
	DEBUG(0, ("Process %s requested injection of signal %d\n",
		  procid_str_static(&src), sig));
#endif

	kill(sys_getpid(), sig);
}
#endif /* DEVELOPER */

/*
 * Parent smbd process sets its own debug level first and then
 * sends a message to all the smbd children to adjust their debug
 * level to that of the parent.
 */

static void smbd_msg_debug(struct messaging_context *msg_ctx,
			   void *private_data,
			   uint32_t msg_type,
			   struct server_id server_id,
			   DATA_BLOB *data)
{
	struct child_pid *child;

	debug_message(msg_ctx, private_data, MSG_DEBUG, server_id, data);

	for (child = children; child != NULL; child = child->next) {
		messaging_send_buf(msg_ctx, pid_to_procid(child->pid),
				   MSG_DEBUG,
				   data->data,
				   strlen((char *) data->data) + 1);
	}
}

static void add_child_pid(pid_t pid)
{
	struct child_pid *child;

	child = SMB_MALLOC_P(struct child_pid);
	if (child == NULL) {
		DEBUG(0, ("Could not add child struct -- malloc failed\n"));
		return;
	}
	child->pid = pid;
	DLIST_ADD(children, child);
	num_children += 1;
}

/*
  at most every smbd:cleanuptime seconds (default 20), we scan the BRL
  and locking database for entries to cleanup. As a side effect this
  also cleans up dead entries in the connections database (due to the
  traversal in message_send_all()

  Using a timer for this prevents a flood of traversals when a large
  number of clients disconnect at the same time (perhaps due to a
  network outage).  
*/

static void cleanup_timeout_fn(struct event_context *event_ctx,
				struct timed_event *te,
				struct timeval now,
				void *private_data)
{
	struct timed_event **cleanup_te = (struct timed_event **)private_data;

	DEBUG(1,("Cleaning up brl and lock database after unclean shutdown\n"));
	message_send_all(smbd_messaging_context(), MSG_SMB_UNLOCK, NULL, 0, NULL);
	messaging_send_buf(smbd_messaging_context(), procid_self(),
				MSG_SMB_BRL_VALIDATE, NULL, 0);
	/* mark the cleanup as having been done */
	(*cleanup_te) = NULL;
}

static void remove_child_pid(pid_t pid, bool unclean_shutdown)
{
	struct child_pid *child;
	static struct timed_event *cleanup_te;
	struct server_id child_id;

	child_id = procid_self(); /* Just initialize pid and potentially vnn */
	child_id.pid = pid;

	for (child = children; child != NULL; child = child->next) {
		if (child->pid == pid) {
			struct child_pid *tmp = child;
			DLIST_REMOVE(children, child);
			SAFE_FREE(tmp);
			num_children -= 1;
			break;
		}
	}

	if (child == NULL) {
		/* not all forked child processes are added to the children list */
		DEBUG(2, ("Could not find child %d -- ignoring\n", (int)pid));
		return;
	}

	if (unclean_shutdown) {
		/* a child terminated uncleanly so tickle all
		   processes to see if they can grab any of the
		   pending locks
                */
		DEBUG(3,(__location__ " Unclean shutdown of pid %u\n",
			(unsigned int)pid));
		if (!cleanup_te) {
			/* call the cleanup timer, but not too often */
			int cleanup_time = lp_parm_int(-1, "smbd", "cleanuptime", 20);
			cleanup_te = event_add_timed(smbd_event_context(), NULL,
						timeval_current_ofs(cleanup_time, 0),
						cleanup_timeout_fn,
						&cleanup_te);
			DEBUG(1,("Scheduled cleanup of brl and lock database after unclean shutdown\n"));
		}
	}

	if (!serverid_deregister(child_id)) {
		DEBUG(1, ("Could not remove pid %d from serverid.tdb\n",
			  (int)pid));
	}
}

/****************************************************************************
 Have we reached the process limit ?
****************************************************************************/

static bool allowable_number_of_smbd_processes(void)
{
	int max_processes = lp_max_smbd_processes();

	if (!max_processes)
		return True;

	return num_children < max_processes;
}

static void smbd_sig_chld_handler(struct tevent_context *ev,
				  struct tevent_signal *se,
				  int signum,
				  int count,
				  void *siginfo,
				  void *private_data)
{
	pid_t pid;
	int status;

	while ((pid = sys_waitpid(-1, &status, WNOHANG)) > 0) {
		bool unclean_shutdown = False;

		/* If the child terminated normally, assume
		   it was an unclean shutdown unless the
		   status is 0
		*/
		if (WIFEXITED(status)) {
			unclean_shutdown = WEXITSTATUS(status);
		}
		/* If the child terminated due to a signal
		   we always assume it was unclean.
		*/
		if (WIFSIGNALED(status)) {
			unclean_shutdown = True;
		}
		remove_child_pid(pid, unclean_shutdown);
	}
}

static void smbd_setup_sig_chld_handler(void)
{
	struct tevent_signal *se;

	se = tevent_add_signal(smbd_event_context(),
			       smbd_event_context(),
			       SIGCHLD, 0,
			       smbd_sig_chld_handler,
			       NULL);
	if (!se) {
		exit_server("failed to setup SIGCHLD handler");
	}
}

struct smbd_open_socket;

struct smbd_parent_context {
	bool interactive;

	/* the list of listening sockets */
	struct smbd_open_socket *sockets;
};

struct smbd_open_socket {
	struct smbd_open_socket *prev, *next;
	struct smbd_parent_context *parent;
	int fd;
	struct tevent_fd *fde;
};

static void smbd_open_socket_close_fn(struct tevent_context *ev,
				      struct tevent_fd *fde,
				      int fd,
				      void *private_data)
{
	/* this might be the socket_wrapper swrap_close() */
	close(fd);
}

static void smbd_accept_connection(struct tevent_context *ev,
				   struct tevent_fd *fde,
				   uint16_t flags,
				   void *private_data)
{
	struct smbd_open_socket *s = talloc_get_type_abort(private_data,
				     struct smbd_open_socket);
	struct sockaddr_storage addr;
	socklen_t in_addrlen = sizeof(addr);
	int fd;
	pid_t pid = 0;
	uint64_t unique_id;

	fd = accept(s->fd, (struct sockaddr *)(void *)&addr,&in_addrlen);
	smbd_set_server_fd(fd);

	if (fd == -1 && errno == EINTR)
		return;

	if (fd == -1) {
		DEBUG(0,("open_sockets_smbd: accept: %s\n",
			 strerror(errno)));
		return;
	}

	if (s->parent->interactive) {
		smbd_process(smbd_server_conn);
		exit_server_cleanly("end of interactive mode");
		return;
	}

	if (!allowable_number_of_smbd_processes()) {
		close(fd);
		smbd_set_server_fd(-1);
		return;
	}

	/*
	 * Generate a unique id in the parent process so that we use
	 * the global random state in the parent.
	 */
	unique_id = serverid_get_random_unique_id();

	pid = sys_fork();
	if (pid == 0) {
		NTSTATUS status = NT_STATUS_OK;

		/* Child code ... */
		am_parent = 0;

		set_my_unique_id(unique_id);

		/* Stop zombies, the parent explicitly handles
		 * them, counting worker smbds. */
		CatchChild();

		/* close our standard file
		   descriptors */
		close_low_fds(False);

		/*
		 * Can't use TALLOC_FREE here. Nulling out the argument to it
		 * would overwrite memory we've just freed.
		 */
		talloc_free(s->parent);
		s = NULL;

		status = reinit_after_fork(smbd_messaging_context(),
					   smbd_event_context(), procid_self(),
					   true);
		if (!NT_STATUS_IS_OK(status)) {
			if (NT_STATUS_EQUAL(status,
					    NT_STATUS_TOO_MANY_OPENED_FILES)) {
				DEBUG(0,("child process cannot initialize "
					 "because too many files are open\n"));
				goto exit;
			}
			if (lp_clustering() &&
			    NT_STATUS_EQUAL(status,
			    NT_STATUS_INTERNAL_DB_ERROR)) {
				DEBUG(1,("child process cannot initialize "
					 "because connection to CTDB "
					 "has failed\n"));
				goto exit;
			}

			DEBUG(0,("reinit_after_fork() failed\n"));
			smb_panic("reinit_after_fork() failed");
		}

		smbd_setup_sig_term_handler();
		smbd_setup_sig_hup_handler(server_event_context(),
					   server_messaging_context());

		if (!serverid_register(procid_self(),
				       FLAG_MSG_GENERAL|FLAG_MSG_SMBD
				       |FLAG_MSG_DBWRAP
				       |FLAG_MSG_PRINT_GENERAL)) {
			exit_server_cleanly("Could not register myself in "
					    "serverid.tdb");
		}

		smbd_process(smbd_server_conn);
	 exit:
		exit_server_cleanly("end of child");
		return;
	}

	if (pid < 0) {
		DEBUG(0,("smbd_accept_connection: sys_fork() failed: %s\n",
			 strerror(errno)));
	}

	/* The parent doesn't need this socket */
	close(fd);

	/* Sun May 6 18:56:14 2001 ackley@cs.unm.edu:
		Clear the closed fd info out of server_fd --
		and more importantly, out of client_fd in
		util_sock.c, to avoid a possible
		getpeername failure if we reopen the logs
		and use %I in the filename.
	*/

	smbd_set_server_fd(-1);

	if (pid != 0) {
		add_child_pid(pid);
	}

	/* Force parent to check log size after
	 * spawning child.  Fix from
	 * klausr@ITAP.Physik.Uni-Stuttgart.De.  The
	 * parent smbd will log to logserver.smb.  It
	 * writes only two messages for each child
	 * started/finished. But each child writes,
	 * say, 50 messages also in logserver.smb,
	 * begining with the debug_count of the
	 * parent, before the child opens its own log
	 * file logserver.client. In a worst case
	 * scenario the size of logserver.smb would be
	 * checked after about 50*50=2500 messages
	 * (ca. 100kb).
	 * */
	force_check_log_size();
}

static bool smbd_open_one_socket(struct smbd_parent_context *parent,
				 const struct sockaddr_storage *ifss,
				 uint16_t port)
{
	struct smbd_open_socket *s;

	s = talloc(parent, struct smbd_open_socket);
	if (!s) {
		return false;
	}

	s->parent = parent;
	s->fd = open_socket_in(SOCK_STREAM,
			       port,
			       parent->sockets == NULL ? 0 : 2,
			       ifss,
			       true);
	if (s->fd == -1) {
		DEBUG(0,("smbd_open_once_socket: open_socket_in: "
			"%s\n", strerror(errno)));
		TALLOC_FREE(s);
		/*
		 * We ignore an error here, as we've done before
		 */
		return true;
	}

	/* ready to listen */
	set_socket_options(s->fd, "SO_KEEPALIVE");
	set_socket_options(s->fd, lp_socket_options());

	/* Set server socket to
	 * non-blocking for the accept. */
	set_blocking(s->fd, False);

	if (listen(s->fd, SMBD_LISTEN_BACKLOG) == -1) {
		DEBUG(0,("open_sockets_smbd: listen: "
			"%s\n", strerror(errno)));
			close(s->fd);
		TALLOC_FREE(s);
		return false;
	}

	s->fde = tevent_add_fd(smbd_event_context(),
			       s,
			       s->fd, TEVENT_FD_READ,
			       smbd_accept_connection,
			       s);
	if (!s->fde) {
		DEBUG(0,("open_sockets_smbd: "
			 "tevent_add_fd: %s\n",
			 strerror(errno)));
		close(s->fd);
		TALLOC_FREE(s);
		return false;
	}
	tevent_fd_set_close_fn(s->fde, smbd_open_socket_close_fn);

	DLIST_ADD_END(parent->sockets, s, struct smbd_open_socket *);

	return true;
}

/****************************************************************************
 Open the socket communication.
****************************************************************************/

static bool open_sockets_smbd(struct smbd_parent_context *parent,
			      struct messaging_context *msg_ctx,
			      const char *smb_ports)
{
	int num_interfaces = iface_count();
	int i;
	char *ports;
	char *tok;
	const char *ptr;
	unsigned dns_port = 0;

#ifdef HAVE_ATEXIT
	atexit(killkids);
#endif

	/* Stop zombies */
	smbd_setup_sig_chld_handler();

	/* use a reasonable default set of ports - listing on 445 and 139 */
	if (!smb_ports) {
		ports = lp_smb_ports();
		if (!ports || !*ports) {
			ports = talloc_strdup(talloc_tos(), SMB_PORTS);
		} else {
			ports = talloc_strdup(talloc_tos(), ports);
		}
	} else {
		ports = talloc_strdup(talloc_tos(), smb_ports);
	}

	for (ptr = ports;
	     next_token_talloc(talloc_tos(),&ptr, &tok, " \t,");) {
		unsigned port = atoi(tok);

		if (port == 0 || port > 0xffff) {
			exit_server_cleanly("Invalid port in the config or on "
					    "the commandline specified!");
		}
	}

	if (lp_interfaces() && lp_bind_interfaces_only()) {
		/* We have been given an interfaces line, and been
		   told to only bind to those interfaces. Create a
		   socket per interface and bind to only these.
		*/

		/* Now open a listen socket for each of the
		   interfaces. */
		for(i = 0; i < num_interfaces; i++) {
			const struct sockaddr_storage *ifss =
					iface_n_sockaddr_storage(i);

			if (ifss == NULL) {
				DEBUG(0,("open_sockets_smbd: "
					"interface %d has NULL IP address !\n",
					i));
				continue;
			}

			for (ptr=ports;
			     next_token_talloc(talloc_tos(),&ptr, &tok, " \t,");) {
				unsigned port = atoi(tok);

				/* Keep the first port for mDNS service
				 * registration.
				 */
				if (dns_port == 0) {
					dns_port = port;
				}

				if (!smbd_open_one_socket(parent, ifss, port)) {
					return false;
				}
			}
		}
	} else {
		/* Just bind to 0.0.0.0 - accept connections
		   from anywhere. */

		const char *sock_addr = lp_socket_address();
		char *sock_tok;
		const char *sock_ptr;

		if (strequal(sock_addr, "0.0.0.0") ||
		    strequal(sock_addr, "::")) {
#if HAVE_IPV6
			sock_addr = "::,0.0.0.0";
#else
			sock_addr = "0.0.0.0";
#endif
		}

		for (sock_ptr=sock_addr;
		     next_token_talloc(talloc_tos(), &sock_ptr, &sock_tok, " \t,"); ) {
			for (ptr=ports; next_token_talloc(talloc_tos(), &ptr, &tok, " \t,"); ) {
				struct sockaddr_storage ss;
				unsigned port = atoi(tok);

				/* Keep the first port for mDNS service
				 * registration.
				 */
				if (dns_port == 0) {
					dns_port = port;
				}

				/* open an incoming socket */
				if (!interpret_string_addr(&ss, sock_tok,
						AI_NUMERICHOST|AI_PASSIVE)) {
					continue;
				}

				if (!smbd_open_one_socket(parent, &ss, port)) {
					return false;
				}
			}
		}
	}

	if (parent->sockets == NULL) {
		DEBUG(0,("open_sockets_smbd: No "
			"sockets available to bind to.\n"));
		return false;
	}

	/* Setup the main smbd so that we can get messages. Note that
	   do this after starting listening. This is needed as when in
	   clustered mode, ctdb won't allow us to start doing database
	   operations until it has gone thru a full startup, which
	   includes checking to see that smbd is listening. */

	if (!serverid_register(procid_self(),
			       FLAG_MSG_GENERAL|FLAG_MSG_SMBD
			       |FLAG_MSG_DBWRAP)) {
		DEBUG(0, ("open_sockets_smbd: Failed to register "
			  "myself in serverid.tdb\n"));
		return false;
	}

        /* Listen to messages */

	messaging_register(msg_ctx, NULL, MSG_SMB_SAM_SYNC, msg_sam_sync);
	messaging_register(msg_ctx, NULL, MSG_SHUTDOWN, msg_exit_server);
	messaging_register(msg_ctx, NULL, MSG_SMB_FILE_RENAME,
			   msg_file_was_renamed);
	messaging_register(msg_ctx, server_event_context(), MSG_SMB_CONF_UPDATED,
			   smb_conf_updated);
	messaging_register(msg_ctx, NULL, MSG_SMB_STAT_CACHE_DELETE,
			   smb_stat_cache_delete);
	messaging_register(msg_ctx, NULL, MSG_DEBUG, smbd_msg_debug);
	brl_register_msgs(msg_ctx);

	msg_idmap_register_msgs(msg_ctx);

#ifdef CLUSTER_SUPPORT
	if (lp_clustering()) {
		ctdbd_register_reconfigure(messaging_ctdbd_connection());
	}
#endif

#ifdef DEVELOPER
	messaging_register(msg_ctx, NULL, MSG_SMB_INJECT_FAULT,
			   msg_inject_fault);
#endif

	if (lp_multicast_dns_register() && (dns_port != 0)) {
#ifdef WITH_DNSSD_SUPPORT
		smbd_setup_mdns_registration(smbd_event_context(),
					     parent, dns_port);
#endif
#ifdef WITH_AVAHI_SUPPORT
		void *avahi_conn;

		avahi_conn = avahi_start_register(
			smbd_event_context(), smbd_event_context(), dns_port);
		if (avahi_conn == NULL) {
			DEBUG(10, ("avahi_start_register failed\n"));
		}
#endif
	}

	return true;
}

static void smbd_parent_loop(struct smbd_parent_context *parent)
{
	/* now accept incoming connections - forking a new process
	   for each incoming connection */
	DEBUG(2,("waiting for connections\n"));
	while (1) {
		int ret;
		TALLOC_CTX *frame = talloc_stackframe();

		ret = tevent_loop_once(smbd_event_context());
		if (ret != 0) {
			exit_server_cleanly("tevent_loop_once() error");
		}

		TALLOC_FREE(frame);
	} /* end while 1 */

/* NOTREACHED	return True; */
}


/****************************************************************************
 Initialise connect, service and file structs.
****************************************************************************/

static bool init_structs(void )
{
	/*
	 * Set the machine NETBIOS name if not already
	 * set from the config file.
	 */

	if (!init_names())
		return False;

	if (!secrets_init())
		return False;

	return True;
}

/****************************************************************************
 main program.
****************************************************************************/

/* Declare prototype for build_options() to avoid having to run it through
   mkproto.h.  Mixing $(builddir) and $(srcdir) source files in the current
   prototype generation system is too complicated. */

extern void build_options(bool screen);

 int main(int argc,const char *argv[])
{
	/* shall I run as a daemon */
	bool is_daemon = false;
	bool interactive = false;
	bool Fork = true;
	bool no_process_group = false;
	bool log_stdout = false;
	char *ports = NULL;
	char *profile_level = NULL;
	int opt;
	poptContext pc;
	bool print_build_options = False;
        enum {
		OPT_DAEMON = 1000,
		OPT_INTERACTIVE,
		OPT_FORK,
		OPT_NO_PROCESS_GROUP,
		OPT_LOG_STDOUT
	};
	struct poptOption long_options[] = {
	POPT_AUTOHELP
	{"daemon", 'D', POPT_ARG_NONE, NULL, OPT_DAEMON, "Become a daemon (default)" },
	{"interactive", 'i', POPT_ARG_NONE, NULL, OPT_INTERACTIVE, "Run interactive (not a daemon)"},
	{"foreground", 'F', POPT_ARG_NONE, NULL, OPT_FORK, "Run daemon in foreground (for daemontools, etc.)" },
	{"no-process-group", '\0', POPT_ARG_NONE, NULL, OPT_NO_PROCESS_GROUP, "Don't create a new process group" },
	{"log-stdout", 'S', POPT_ARG_NONE, NULL, OPT_LOG_STDOUT, "Log to stdout" },
	{"build-options", 'b', POPT_ARG_NONE, NULL, 'b', "Print build options" },
	{"port", 'p', POPT_ARG_STRING, &ports, 0, "Listen on the specified ports"},
	{"profiling-level", 'P', POPT_ARG_STRING, &profile_level, 0, "Set profiling level","PROFILE_LEVEL"},
	POPT_COMMON_SAMBA
	POPT_COMMON_DYNCONFIG
	POPT_TABLEEND
	};
	struct smbd_parent_context *parent = NULL;
	TALLOC_CTX *frame;
	NTSTATUS status;

	/*
	 * Do this before any other talloc operation
	 */
	talloc_enable_null_tracking();
	frame = talloc_stackframe();

	load_case_tables();

	/* Initialize the event context, it will panic on error */
	smbd_event_context();

	smbd_init_globals();

	TimeInit();

#ifdef HAVE_SET_AUTH_PARAMETERS
	set_auth_parameters(argc,argv);
#endif

	pc = poptGetContext("smbd", argc, argv, long_options, 0);
	while((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt)  {
		case OPT_DAEMON:
			is_daemon = true;
			break;
		case OPT_INTERACTIVE:
			interactive = true;
			break;
		case OPT_FORK:
			Fork = false;
			break;
		case OPT_NO_PROCESS_GROUP:
			no_process_group = true;
			break;
		case OPT_LOG_STDOUT:
			log_stdout = true;
			break;
		case 'b':
			print_build_options = True;
			break;
		default:
			d_fprintf(stderr, "\nInvalid option %s: %s\n\n",
				  poptBadOption(pc, 0), poptStrerror(opt));
			poptPrintUsage(pc, stderr, 0);
			exit(1);
		}
	}
	poptFreeContext(pc);

	if (interactive) {
		Fork = False;
		log_stdout = True;
	}

	if (log_stdout) {
		setup_logging(argv[0], DEBUG_STDOUT);
	} else {
		setup_logging(argv[0], DEBUG_FILE);
	}

	if (print_build_options) {
		build_options(True); /* Display output to screen as well as debug */
		exit(0);
	}

#ifdef HAVE_SETLUID
	/* needed for SecureWare on SCO */
	setluid(0);
#endif

	set_remote_machine_name("smbd", False);

	if (interactive && (DEBUGLEVEL >= 9)) {
		talloc_enable_leak_report();
	}

	if (log_stdout && Fork) {
		DEBUG(0,("ERROR: Can't log to stdout (-S) unless daemon is in foreground (-F) or interactive (-i)\n"));
		exit(1);
	}

	/* we want to re-seed early to prevent time delays causing
           client problems at a later date. (tridge) */
	generate_random_buffer(NULL, 0);

	/* get initial effective uid and gid */
	sec_init();

	/* make absolutely sure we run as root - to handle cases where people
	   are crazy enough to have it setuid */
	gain_root_privilege();
	gain_root_group_privilege();

	fault_setup((void (*)(void *))exit_server_fault);
	dump_core_setup("smbd");

	/* we are never interested in SIGPIPE */
	BlockSignals(True,SIGPIPE);

#if defined(SIGFPE)
	/* we are never interested in SIGFPE */
	BlockSignals(True,SIGFPE);
#endif

#if defined(SIGUSR2)
	/* We are no longer interested in USR2 */
	BlockSignals(True,SIGUSR2);
#endif

	/* POSIX demands that signals are inherited. If the invoking process has
	 * these signals masked, we will have problems, as we won't recieve them. */
	BlockSignals(False, SIGHUP);
	BlockSignals(False, SIGUSR1);
	BlockSignals(False, SIGTERM);

	/* Ensure we leave no zombies until we
	 * correctly set up child handling below. */

	CatchChild();

	/* we want total control over the permissions on created files,
	   so set our umask to 0 */
	umask(0);

	reopen_logs();

	DEBUG(0,("smbd version %s started.\n", samba_version_string()));
	DEBUGADD(0,("%s\n", COPYRIGHT_STARTUP_MESSAGE));

	DEBUG(2,("uid=%d gid=%d euid=%d egid=%d\n",
		 (int)getuid(),(int)getgid(),(int)geteuid(),(int)getegid()));

	/* Output the build options to the debug log */ 
	build_options(False);

	if (sizeof(uint16) < 2 || sizeof(uint32) < 4) {
		DEBUG(0,("ERROR: Samba is not configured correctly for the word size on your machine\n"));
		exit(1);
	}

	if (!lp_load_initial_only(get_dyn_CONFIGFILE())) {
		DEBUG(0, ("error opening config file\n"));
		exit(1);
	}

	/* Init the security context and global current_user */
	init_sec_ctx();

	if (smbd_messaging_context() == NULL)
		exit(1);

	/*
	 * Reloading of the printers will not work here as we don't have a
	 * server info and rpc services set up. It will be called later.
	 */
	if (!reload_services(smbd_messaging_context(), -1, False)) {
		exit(1);
	}

	/* ...NOTE... Log files are working from this point! */

	DEBUG(3,("loaded services\n"));

	init_structs();

#ifdef WITH_PROFILE
	if (!profile_setup(smbd_messaging_context(), False)) {
		DEBUG(0,("ERROR: failed to setup profiling\n"));
		return -1;
	}
	if (profile_level != NULL) {
		int pl = atoi(profile_level);
		struct server_id src;

		DEBUG(1, ("setting profiling level: %s\n",profile_level));
		src.pid = getpid();
		set_profile_level(pl, src);
	}
#endif

	if (!is_daemon && !is_a_socket(0)) {
		if (!interactive)
			DEBUG(0,("standard input is not a socket, assuming -D option\n"));

		/*
		 * Setting is_daemon here prevents us from eventually calling
		 * the open_sockets_inetd()
		 */

		is_daemon = True;
	}

	if (is_daemon && !interactive) {
		DEBUG( 3, ( "Becoming a daemon.\n" ) );
		become_daemon(Fork, no_process_group, log_stdout);
	}

	set_my_unique_id(serverid_get_random_unique_id());

#if HAVE_SETPGID
	/*
	 * If we're interactive we want to set our own process group for
	 * signal management.
	 */
	if (interactive && !no_process_group)
		setpgid( (pid_t)0, (pid_t)0);
#endif

	if (!directory_exist(lp_lockdir()))
		mkdir(lp_lockdir(), 0755);

	if (is_daemon)
		pidfile_create("smbd");

	status = reinit_after_fork(smbd_messaging_context(),
				   smbd_event_context(),
				   procid_self(), false);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("reinit_after_fork() failed\n"));
		exit(1);
	}

	smbd_server_conn->msg_ctx = smbd_messaging_context();

	smbd_setup_sig_term_handler();
	smbd_setup_sig_hup_handler(smbd_event_context(),
				   smbd_server_conn->msg_ctx);

	/* Setup all the TDB's - including CLEAR_IF_FIRST tdb's. */

	if (smbd_memcache() == NULL) {
		exit(1);
	}

	memcache_set_global(smbd_memcache());

	/* Initialise the password backed before the global_sam_sid
	   to ensure that we fetch from ldap before we make a domain sid up */

	if(!initialize_password_db(False, smbd_event_context()))
		exit(1);

	if (!secrets_init()) {
		DEBUG(0, ("ERROR: smbd can not open secrets.tdb\n"));
		exit(1);
	}

	if (lp_server_role() == ROLE_DOMAIN_BDC || lp_server_role() == ROLE_DOMAIN_PDC) {
		if (!open_schannel_session_store(NULL, lp_private_dir())) {
			DEBUG(0,("ERROR: Samba cannot open schannel store for secured NETLOGON operations.\n"));
			exit(1);
		}
	}

	if(!get_global_sam_sid()) {
		DEBUG(0,("ERROR: Samba cannot create a SAM SID.\n"));
		exit(1);
	}

	if (!sessionid_init()) {
		exit(1);
	}

	if (!connections_init(True))
		exit(1);

	if (!locking_init())
		exit(1);

	if (!messaging_tdb_parent_init(smbd_event_context())) {
		exit(1);
	}

	if (!notify_internal_parent_init(smbd_event_context())) {
		exit(1);
	}

	if (!serverid_parent_init(smbd_event_context())) {
		exit(1);
	}

	if (!printer_list_parent_init()) {
		exit(1);
	}

#ifdef REGISTRY_BACKEND
	if (!W_ERROR_IS_OK(registry_init_full()))
		exit(1);
#endif

	/* Open the share_info.tdb here, so we don't have to open
	   after the fork on every single connection.  This is a small
	   performance improvment and reduces the total number of system
	   fds used. */
	if (!share_info_db_init()) {
		DEBUG(0,("ERROR: failed to load share info db.\n"));
		exit(1);
	}

	status = init_system_info();
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("ERROR: failed to setup system user info: %s.\n",
			  nt_errstr(status)));
		return -1;
	}

	if (!init_guest_info()) {
		DEBUG(0,("ERROR: failed to setup guest info.\n"));
		return -1;
	}

	if (!file_init(smbd_server_conn)) {
		DEBUG(0, ("ERROR: file_init failed\n"));
		return -1;
	}

	if (is_daemon && !interactive) {
		const char *rpcsrv_type;

		rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
						   "rpc_server", "epmapper",
						   "none");
		if (StrCaseCmp(rpcsrv_type, "daemon") == 0) {
			start_epmd(smbd_event_context(),
				   smbd_server_conn->msg_ctx);
		}
	}

	if (!dcesrv_ep_setup(smbd_event_context(), smbd_server_conn->msg_ctx)) {
		exit(1);
	}

	/*
	 * The print backend init also migrates the printing tdb's,
	 * this requires a winreg pipe.
	 */
#ifdef PRINTER_SUPPORT
	if (!print_backend_init(smbd_messaging_context()))
		exit(1);

	/* only start the background queue daemon if we are 
	   running as a daemon -- bad things will happen if
	   smbd is launched via inetd and we fork a copy of 
	   ourselves here */

	if (is_daemon && !interactive
	    && lp_parm_bool(-1, "smbd", "backgroundqueue", true)) {
		/* background queue is responsible for printcap cache updates */
		messaging_register(smbd_server_conn->msg_ctx,
				   smbd_event_context(),
				   MSG_PRINTER_PCAP, smb_pcap_updated);
		start_background_queue(server_event_context(),
				       smbd_server_conn->msg_ctx);
	} else {
		DEBUG(3, ("running without background printer process, dynamic "
			  "printer updates disabled\n"));
		/* Publish nt printers, this requires a working winreg pipe */
		pcap_cache_reload(server_event_context(),
				  smbd_messaging_context(),
				  &reload_printers_full);
	}

	if (is_daemon && !_lp_disable_spoolss()) {
		const char *rpcsrv_type;

		/* start spoolss daemon */
		/* start as a separate daemon only if enabled */
		rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
						   "rpc_server", "spoolss",
						   "embedded");
		if (StrCaseCmp(rpcsrv_type, "daemon") == 0) {
			start_spoolssd(smbd_event_context(),
				       smbd_messaging_context());
		}
	}
#endif
	if (!is_daemon) {
		/* inetd mode */
		TALLOC_FREE(frame);

		/* Started from inetd. fd 0 is the socket. */
		/* We will abort gracefully when the client or remote system
		   goes away */
		smbd_set_server_fd(dup(0));

		/* close our standard file descriptors */
		close_low_fds(False); /* Don't close stderr */

#ifdef HAVE_ATEXIT
		atexit(killkids);
#endif

	        /* Stop zombies */
		smbd_setup_sig_chld_handler();

		smbd_process(smbd_server_conn);

		exit_server_cleanly(NULL);
		return(0);
	}

	parent = talloc_zero(smbd_event_context(), struct smbd_parent_context);
	if (!parent) {
		exit_server("talloc(struct smbd_parent_context) failed");
	}
	parent->interactive = interactive;

	if (!open_sockets_smbd(parent, smbd_messaging_context(), ports))
		exit_server("open_sockets_smbd() failed");

	TALLOC_FREE(frame);
	/* make sure we always have a valid stackframe */
	frame = talloc_stackframe();

	smbd_parent_loop(parent);

	exit_server_cleanly(NULL);
	TALLOC_FREE(frame);
	return(0);
}
