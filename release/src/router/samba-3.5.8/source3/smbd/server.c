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
#include "smbd/globals.h"

static_decl_rpc;

#ifdef WITH_DFS
extern int dcelogin_atmost_once;
#endif /* WITH_DFS */

int smbd_server_fd(void)
{
	return server_fd;
}

static void smbd_set_server_fd(int fd)
{
	server_fd = fd;
}

int get_client_fd(void)
{
	return server_fd;
}

struct event_context *smbd_event_context(void)
{
	if (!smbd_event_ctx) {
		smbd_event_ctx = event_context_init(talloc_autofree_context());
	}
	if (!smbd_event_ctx) {
		smb_panic("Could not init smbd event context");
	}
	return smbd_event_ctx;
}

struct messaging_context *smbd_messaging_context(void)
{
	if (smbd_msg_ctx == NULL) {
		smbd_msg_ctx = messaging_init(talloc_autofree_context(),
					      server_id_self(),
					      smbd_event_context());
	}
	if (smbd_msg_ctx == NULL) {
		DEBUG(0, ("Could not init smbd messaging context.\n"));
	}
	return smbd_msg_ctx;
}

struct memcache *smbd_memcache(void)
{
	if (!smbd_memcache_ctx) {
		smbd_memcache_ctx = memcache_init(talloc_autofree_context(),
						  lp_max_stat_cache_size()*1024);
	}
	if (!smbd_memcache_ctx) {
		smb_panic("Could not init smbd memcache");
	}

	return smbd_memcache_ctx;
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
	DEBUG(10,("smb_conf_updated: Got message saying smb.conf was "
		  "updated. Reloading.\n"));
	change_to_root_user();
	reload_services(False);
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

	for (child = children; child != NULL; child = child->next) {
		if (child->pid == pid) {
			struct child_pid *tmp = child;
			DLIST_REMOVE(children, child);
			SAFE_FREE(tmp);
			num_children -= 1;
			return;
		}
	}

	DEBUG(0, ("Could not find child %d -- ignoring\n", (int)pid));
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
	pid_t pid = 0;

	smbd_set_server_fd(accept(s->fd,(struct sockaddr *)&addr,&in_addrlen));

	if (smbd_server_fd() == -1 && errno == EINTR)
		return;

	if (smbd_server_fd() == -1) {
		DEBUG(0,("open_sockets_smbd: accept: %s\n",
			 strerror(errno)));
		return;
	}

	if (s->parent->interactive) {
		smbd_process();
		exit_server_cleanly("end of interactive mode");
		return;
	}

	if (!allowable_number_of_smbd_processes()) {
		close(smbd_server_fd());
		smbd_set_server_fd(-1);
		return;
	}

	pid = sys_fork();
	if (pid == 0) {
		NTSTATUS status = NT_STATUS_OK;
		/* Child code ... */
		am_parent = 0;

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
					   smbd_event_context(), true);
		if (!NT_STATUS_IS_OK(status)) {
			if (NT_STATUS_EQUAL(status,
					    NT_STATUS_TOO_MANY_OPENED_FILES)) {
				DEBUG(0,("child process cannot initialize "
					 "because too many files are open\n"));
				goto exit;
			}
			DEBUG(0,("reinit_after_fork() failed\n"));
			smb_panic("reinit_after_fork() failed");
		}

		smbd_setup_sig_term_handler();
		smbd_setup_sig_hup_handler();

		smbd_process();
	 exit:
		exit_server_cleanly("end of child");
		return;
	} else if (pid < 0) {
		DEBUG(0,("smbd_accept_connection: sys_fork() failed: %s\n",
			 strerror(errno)));
	}

	/* The parent doesn't need this socket */
	close(smbd_server_fd());

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
			      const char *smb_ports)
{
	int num_interfaces = iface_count();
	int i;
	char *ports;
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
			char *tok;
			const char *ptr;

			if (ifss == NULL) {
				DEBUG(0,("open_sockets_smbd: "
					"interface %d has NULL IP address !\n",
					i));
				continue;
			}

			for (ptr=ports;
			     next_token_talloc(talloc_tos(),&ptr, &tok, " \t,");) {
				unsigned port = atoi(tok);
				if (port == 0 || port > 0xffff) {
					continue;
				}

				if (!smbd_open_one_socket(parent, ifss, port)) {
					return false;
				}
			}
		}
	} else {
		/* Just bind to 0.0.0.0 - accept connections
		   from anywhere. */

		char *tok;
		const char *ptr;
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
				if (port == 0 || port > 0xffff) {
					continue;
				}

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
	claim_connection(NULL,"",
			 FLAG_MSG_GENERAL|FLAG_MSG_SMBD|FLAG_MSG_DBWRAP);

        /* Listen to messages */

	messaging_register(smbd_messaging_context(), NULL,
			   MSG_SMB_SAM_SYNC, msg_sam_sync);
	messaging_register(smbd_messaging_context(), NULL,
			   MSG_SHUTDOWN, msg_exit_server);
	messaging_register(smbd_messaging_context(), NULL,
			   MSG_SMB_FILE_RENAME, msg_file_was_renamed);
	messaging_register(smbd_messaging_context(), NULL,
			   MSG_SMB_CONF_UPDATED, smb_conf_updated);
	messaging_register(smbd_messaging_context(), NULL,
			   MSG_SMB_STAT_CACHE_DELETE, smb_stat_cache_delete);
	messaging_register(smbd_messaging_context(), NULL,
			   MSG_DEBUG, smbd_msg_debug);
	brl_register_msgs(smbd_messaging_context());

#ifdef CLUSTER_SUPPORT
	if (lp_clustering()) {
		ctdbd_register_reconfigure(messaging_ctdbd_connection());
	}
#endif

#ifdef DEVELOPER
	messaging_register(smbd_messaging_context(), NULL,
			   MSG_SMB_INJECT_FAULT, msg_inject_fault);
#endif

	if (dns_port != 0) {
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
 Reload printers
**************************************************************************/
void reload_printers(void)
{
	int snum;
	int n_services = lp_numservices();
	int pnum = lp_servicenumber(PRINTERS_NAME);
	const char *pname;

	pcap_cache_reload();

	/* remove stale printers */
	for (snum = 0; snum < n_services; snum++) {
		/* avoid removing PRINTERS_NAME or non-autoloaded printers */
		if (snum == pnum || !(lp_snum_ok(snum) && lp_print_ok(snum) &&
		                      lp_autoloaded(snum)))
			continue;

		pname = lp_printername(snum);
		if (!pcap_printername_ok(pname)) {
			DEBUG(3, ("removing stale printer %s\n", pname));

			if (is_printer_published(NULL, snum, NULL))
				nt_printer_publish(NULL, snum, DSPRINT_UNPUBLISH);
			del_a_printer(pname);
			lp_killservice(snum);
		}
	}

	load_printers();
}

/****************************************************************************
 Reload the services file.
**************************************************************************/

bool reload_services(bool test)
{
	bool ret;

	if (lp_loaded()) {
		char *fname = lp_configfile();
		if (file_exist(fname) &&
		    !strcsequal(fname, get_dyn_CONFIGFILE())) {
			set_dyn_CONFIGFILE(fname);
			test = False;
		}
	}

	reopen_logs();

	if (test && !lp_file_list_changed())
		return(True);

	lp_killunused(conn_snum_used);

	ret = lp_load(get_dyn_CONFIGFILE(), False, False, True, True);

	reload_printers();

	/* perhaps the config filename is now set */
	if (!test)
		reload_services(True);

	reopen_logs();

	load_interfaces();

	if (smbd_server_fd() != -1) {
		set_socket_options(smbd_server_fd(),"SO_KEEPALIVE");
		set_socket_options(smbd_server_fd(), lp_socket_options());
	}

	mangle_reset_cache();
	reset_stat_cache();

	/* this forces service parameters to be flushed */
	set_current_service(NULL,0,True);

	return(ret);
}

/****************************************************************************
 Exit the server.
****************************************************************************/

/* Reasons for shutting down a server process. */
enum server_exit_reason { SERVER_EXIT_NORMAL, SERVER_EXIT_ABNORMAL };

static void exit_server_common(enum server_exit_reason how,
	const char *const reason) _NORETURN_;

static void exit_server_common(enum server_exit_reason how,
	const char *const reason)
{
	bool had_open_conn = false;
	struct smbd_server_connection *sconn = smbd_server_conn;

	if (!exit_firsttime)
		exit(0);
	exit_firsttime = false;

	change_to_root_user();

	if (sconn && sconn->smb1.negprot.auth_context) {
		struct auth_context *a = sconn->smb1.negprot.auth_context;
		a->free(&sconn->smb1.negprot.auth_context);
	}

	if (sconn) {
		had_open_conn = conn_close_all(sconn);
		invalidate_all_vuids(sconn);
	}

	/* 3 second timeout. */
	print_notify_send_messages(smbd_messaging_context(), 3);

	/* delete our entry in the connections database. */
	yield_connection(NULL,"");

#ifdef WITH_DFS
	if (dcelogin_atmost_once) {
		dfs_unlogin();
	}
#endif

#ifdef USE_DMAPI
	/* Destroy Samba DMAPI session only if we are master smbd process */
	if (am_parent) {
		if (!dmapi_destroy_session()) {
			DEBUG(0,("Unable to close Samba DMAPI session\n"));
		}
	}
#endif

	locking_end();
	printing_end();

	/*
	 * we need to force the order of freeing the following,
	 * because smbd_msg_ctx is not a talloc child of smbd_server_conn.
	 */
	sconn = NULL;
	TALLOC_FREE(smbd_server_conn);
	TALLOC_FREE(smbd_msg_ctx);
	TALLOC_FREE(smbd_event_ctx);

	if (how != SERVER_EXIT_NORMAL) {
		int oldlevel = DEBUGLEVEL;

		DEBUGLEVEL = 10;

		DEBUGSEP(0);
		DEBUG(0,("Abnormal server exit: %s\n",
			reason ? reason : "no explanation provided"));
		DEBUGSEP(0);

		log_stack_trace();

		DEBUGLEVEL = oldlevel;
		dump_core();

	} else {    
		DEBUG(3,("Server exit (%s)\n",
			(reason ? reason : "normal exit")));
		if (am_parent) {
			pidfile_unlink();
		}
		gencache_stabilize();
	}

	/* if we had any open SMB connections when we exited then we
	   need to tell the parent smbd so that it can trigger a retry
	   of any locks we may have been holding or open files we were
	   blocking */
	if (had_open_conn) {
		exit(1);
	} else {
		exit(0);
	}
}

void exit_server(const char *const explanation)
{
	exit_server_common(SERVER_EXIT_ABNORMAL, explanation);
}

void exit_server_cleanly(const char *const explanation)
{
	exit_server_common(SERVER_EXIT_NORMAL, explanation);
}

void exit_server_fault(void)
{
	exit_server("critical server fault");
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

	file_init();

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
	TALLOC_CTX *frame = talloc_stackframe(); /* Setup tos. */

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

	setup_logging(argv[0],log_stdout);

	if (print_build_options) {
		build_options(True); /* Display output to screen as well as debug */
		exit(0);
	}

	load_case_tables();

#ifdef HAVE_SETLUID
	/* needed for SecureWare on SCO */
	setluid(0);
#endif

	sec_init();

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

	init_sec_ctx();

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

	if (smbd_messaging_context() == NULL)
		exit(1);

	if (!reload_services(False))
		return(-1);	

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

	DEBUG(3,( "loaded services\n"));

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
		become_daemon(Fork, no_process_group);
	}

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

	if (!NT_STATUS_IS_OK(reinit_after_fork(smbd_messaging_context(),
			     smbd_event_context(), false))) {
		DEBUG(0,("reinit_after_fork() failed\n"));
		exit(1);
	}

	smbd_setup_sig_term_handler();
	smbd_setup_sig_hup_handler();

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

	if(!get_global_sam_sid()) {
		DEBUG(0,("ERROR: Samba cannot create a SAM SID.\n"));
		exit(1);
	}

	if (!session_init())
		exit(1);

	if (!connections_init(True))
		exit(1);

	if (!locking_init())
		exit(1);

	namecache_enable();

	if (!W_ERROR_IS_OK(registry_init_full()))
		exit(1);

#if 0
	if (!init_svcctl_db())
                exit(1);
#endif

	if (!print_backend_init(smbd_messaging_context()))
		exit(1);

	if (!init_guest_info()) {
		DEBUG(0,("ERROR: failed to setup guest info.\n"));
		return -1;
	}

	/* Open the share_info.tdb here, so we don't have to open
	   after the fork on every single connection.  This is a small
	   performance improvment and reduces the total number of system
	   fds used. */
	if (!share_info_db_init()) {
		DEBUG(0,("ERROR: failed to load share info db.\n"));
		exit(1);
	}

	/* only start the background queue daemon if we are 
	   running as a daemon -- bad things will happen if
	   smbd is launched via inetd and we fork a copy of 
	   ourselves here */

	if (is_daemon && !interactive
	    && lp_parm_bool(-1, "smbd", "backgroundqueue", true)) {
		start_background_queue();
	}

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

		smbd_process();

		exit_server_cleanly(NULL);
		return(0);
	}

	parent = talloc_zero(smbd_event_context(), struct smbd_parent_context);
	if (!parent) {
		exit_server("talloc(struct smbd_parent_context) failed");
	}
	parent->interactive = interactive;

	if (!open_sockets_smbd(parent, ports))
		exit_server("open_sockets_smbd() failed");

	TALLOC_FREE(frame);
	/* make sure we always have a valid stackframe */
	frame = talloc_stackframe();

	smbd_parent_loop(parent);

	exit_server_cleanly(NULL);
	TALLOC_FREE(frame);
	return(0);
}
