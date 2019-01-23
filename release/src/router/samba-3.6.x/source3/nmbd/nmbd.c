/*
   Unix SMB/CIFS implementation.
   NBT netbios routines and daemon - version 2
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Jeremy Allison 1997-2002
   Copyright (C) Jelmer Vernooij 2002,2003 (Conversion to popt)

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
#include "nmbd/nmbd.h"
#include "serverid.h"
#include "messages.h"

int ClientNMB       = -1;
int ClientDGRAM     = -1;
int global_nmb_port = -1;

extern bool rescan_listen_set;
extern bool global_in_nmbd;

extern bool override_logfile;

/* have we found LanMan clients yet? */
bool found_lm_clients = False;

/* what server type are we currently */

time_t StartupTime = 0;

struct event_context *nmbd_event_context(void)
{
	return server_event_context();
}

struct messaging_context *nmbd_messaging_context(void)
{
	struct messaging_context *msg_ctx = server_messaging_context();
	if (likely(msg_ctx != NULL)) {
		return msg_ctx;
	}
	smb_panic("Could not init nmbd's messaging context.\n");
	return NULL;
}

/**************************************************************************** **
 Handle a SIGTERM in band.
 **************************************************************************** */

static void terminate(void)
{
	DEBUG(0,("Got SIGTERM: going down...\n"));
  
	/* Write out wins.dat file if samba is a WINS server */
	wins_write_database(0,False);
  
	/* Remove all SELF registered names from WINS */
	release_wins_names();
  
	/* Announce all server entries as 0 time-to-live, 0 type. */
	announce_my_servers_removed();

	/* If there was an async dns child - kill it. */
	kill_async_dns_child();

	gencache_stabilize();
	serverid_deregister(procid_self());

	pidfile_unlink();

	exit(0);
}

static void nmbd_sig_term_handler(struct tevent_context *ev,
				  struct tevent_signal *se,
				  int signum,
				  int count,
				  void *siginfo,
				  void *private_data)
{
	terminate();
}

static bool nmbd_setup_sig_term_handler(void)
{
	struct tevent_signal *se;

	se = tevent_add_signal(nmbd_event_context(),
			       nmbd_event_context(),
			       SIGTERM, 0,
			       nmbd_sig_term_handler,
			       NULL);
	if (!se) {
		DEBUG(0,("failed to setup SIGTERM handler"));
		return false;
	}

	return true;
}

static void msg_reload_nmbd_services(struct messaging_context *msg,
				     void *private_data,
				     uint32_t msg_type,
				     struct server_id server_id,
				     DATA_BLOB *data);

static void nmbd_sig_hup_handler(struct tevent_context *ev,
				 struct tevent_signal *se,
				 int signum,
				 int count,
				 void *siginfo,
				 void *private_data)
{
	DEBUG(0,("Got SIGHUP dumping debug info.\n"));
	msg_reload_nmbd_services(nmbd_messaging_context(),
				 NULL, MSG_SMB_CONF_UPDATED,
				 procid_self(), NULL);
}

static bool nmbd_setup_sig_hup_handler(void)
{
	struct tevent_signal *se;

	se = tevent_add_signal(nmbd_event_context(),
			       nmbd_event_context(),
			       SIGHUP, 0,
			       nmbd_sig_hup_handler,
			       NULL);
	if (!se) {
		DEBUG(0,("failed to setup SIGHUP handler"));
		return false;
	}

	return true;
}

/**************************************************************************** **
 Handle a SHUTDOWN message from smbcontrol.
 **************************************************************************** */

static void nmbd_terminate(struct messaging_context *msg,
			   void *private_data,
			   uint32_t msg_type,
			   struct server_id server_id,
			   DATA_BLOB *data)
{
	terminate();
}

/**************************************************************************** **
 Possibly continue after a fault.
 **************************************************************************** */

static void fault_continue(void)
{
	dump_core();
}

/**************************************************************************** **
 Expire old names from the namelist and server list.
 **************************************************************************** */

static void expire_names_and_servers(time_t t)
{
	static time_t lastrun = 0;
  
	if ( !lastrun )
		lastrun = t;
	if ( t < (lastrun + 5) )
		return;
	lastrun = t;

	/*
	 * Expire any timed out names on all the broadcast
	 * subnets and those registered with the WINS server.
	 * (nmbd_namelistdb.c)
	 */

	expire_names(t);

	/*
	 * Go through all the broadcast subnets and for each
	 * workgroup known on that subnet remove any expired
	 * server names. If a workgroup has an empty serverlist
	 * and has itself timed out then remove the workgroup.
	 * (nmbd_workgroupdb.c)
	 */

	expire_workgroups_and_servers(t);
}

/************************************************************************** **
 Reload the list of network interfaces.
 Doesn't return until a network interface is up.
 ************************************************************************** */

static void reload_interfaces(time_t t)
{
	static time_t lastt;
	int n;
	bool print_waiting_msg = true;
	struct subnet_record *subrec;

	if (t && ((t - lastt) < NMBD_INTERFACES_RELOAD)) {
		return;
	}

	lastt = t;

	if (!interfaces_changed()) {
		return;
	}

  try_again:

	/* the list of probed interfaces has changed, we may need to add/remove
	   some subnets */
	load_interfaces();

	/* find any interfaces that need adding */
	for (n=iface_count() - 1; n >= 0; n--) {
		char str[INET6_ADDRSTRLEN];
		const struct interface *iface = get_interface(n);
		struct in_addr ip, nmask;

		if (!iface) {
			DEBUG(2,("reload_interfaces: failed to get interface %d\n", n));
			continue;
		}

		/* Ensure we're only dealing with IPv4 here. */
		if (iface->ip.ss_family != AF_INET) {
			DEBUG(2,("reload_interfaces: "
				"ignoring non IPv4 interface.\n"));
			continue;
		}

		ip = ((struct sockaddr_in *)(void *)&iface->ip)->sin_addr;
		nmask = ((struct sockaddr_in *)(void *)
			 &iface->netmask)->sin_addr;

		/*
		 * We don't want to add a loopback interface, in case
		 * someone has added 127.0.0.1 for smbd, nmbd needs to
		 * ignore it here. JRA.
		 */

		if (is_loopback_addr((struct sockaddr *)(void *)&iface->ip)) {
			DEBUG(2,("reload_interfaces: Ignoring loopback "
				"interface %s\n",
				print_sockaddr(str, sizeof(str), &iface->ip) ));
			continue;
		}

		for (subrec=subnetlist; subrec; subrec=subrec->next) {
			if (ip_equal_v4(ip, subrec->myip) &&
			    ip_equal_v4(nmask, subrec->mask_ip)) {
				break;
			}
		}

		if (!subrec) {
			/* it wasn't found! add it */
			DEBUG(2,("Found new interface %s\n",
				 print_sockaddr(str,
					 sizeof(str), &iface->ip) ));
			subrec = make_normal_subnet(iface);
			if (subrec)
				register_my_workgroup_one_subnet(subrec);
		}
	}

	/* find any interfaces that need deleting */
	for (subrec=subnetlist; subrec; subrec=subrec->next) {
		for (n=iface_count() - 1; n >= 0; n--) {
			struct interface *iface = get_interface(n);
			struct in_addr ip, nmask;
			if (!iface) {
				continue;
			}
			/* Ensure we're only dealing with IPv4 here. */
			if (iface->ip.ss_family != AF_INET) {
				DEBUG(2,("reload_interfaces: "
					"ignoring non IPv4 interface.\n"));
				continue;
			}
			ip = ((struct sockaddr_in *)(void *)
			      &iface->ip)->sin_addr;
			nmask = ((struct sockaddr_in *)(void *)
				 &iface->netmask)->sin_addr;
			if (ip_equal_v4(ip, subrec->myip) &&
			    ip_equal_v4(nmask, subrec->mask_ip)) {
				break;
			}
		}
		if (n == -1) {
			/* oops, an interface has disapeared. This is
			 tricky, we don't dare actually free the
			 interface as it could be being used, so
			 instead we just wear the memory leak and
			 remove it from the list of interfaces without
			 freeing it */
			DEBUG(2,("Deleting dead interface %s\n",
				 inet_ntoa(subrec->myip)));
			close_subnet(subrec);
		}
	}

	rescan_listen_set = True;

	/* We need to wait if there are no subnets... */
	if (FIRST_SUBNET == NULL) {
		void (*saved_handler)(int);

		if (print_waiting_msg) {
			DEBUG(0,("reload_interfaces: "
				"No subnets to listen to. Waiting..\n"));
			print_waiting_msg = false;
		}

		/*
		 * Whilst we're waiting for an interface, allow SIGTERM to
		 * cause us to exit.
		 */
		saved_handler = CatchSignal(SIGTERM, SIG_DFL);

		/* We only count IPv4, non-loopback interfaces here. */
		while (iface_count_v4_nl() == 0) {
			sleep(5);
			load_interfaces();
		}

		CatchSignal(SIGTERM, saved_handler);

		/*
		 * We got an interface, go back to blocking term.
		 */

		goto try_again;
	}
}

/**************************************************************************** **
 Reload the services file.
 **************************************************************************** */

static bool reload_nmbd_services(bool test)
{
	bool ret;

	set_remote_machine_name("nmbd", False);

	if ( lp_loaded() ) {
		char *fname = lp_configfile();
		if (file_exist(fname) && !strcsequal(fname,get_dyn_CONFIGFILE())) {
			set_dyn_CONFIGFILE(fname);
			test = False;
		}
		TALLOC_FREE(fname);
	}

	if ( test && !lp_file_list_changed() )
		return(True);

	ret = lp_load(get_dyn_CONFIGFILE(), True , False, False, True);

	/* perhaps the config filename is now set */
	if ( !test ) {
		DEBUG( 3, ( "services not loaded\n" ) );
		reload_nmbd_services( True );
	}

	return(ret);
}

/**************************************************************************** **
 * React on 'smbcontrol nmbd reload-config' in the same way as to SIGHUP
 **************************************************************************** */

static void msg_reload_nmbd_services(struct messaging_context *msg,
				     void *private_data,
				     uint32_t msg_type,
				     struct server_id server_id,
				     DATA_BLOB *data)
{
	write_browse_list( 0, True );
	dump_all_namelists();
	reload_nmbd_services( True );
	reopen_logs();
	reload_interfaces(0);
}

static void msg_nmbd_send_packet(struct messaging_context *msg,
				 void *private_data,
				 uint32_t msg_type,
				 struct server_id src,
				 DATA_BLOB *data)
{
	struct packet_struct *p = (struct packet_struct *)data->data;
	struct subnet_record *subrec;
	struct sockaddr_storage ss;
	const struct sockaddr_storage *pss;
	const struct in_addr *local_ip;

	DEBUG(10, ("Received send_packet from %u\n", (unsigned int)procid_to_pid(&src)));

	if (data->length != sizeof(struct packet_struct)) {
		DEBUG(2, ("Discarding invalid packet length from %u\n",
			  (unsigned int)procid_to_pid(&src)));
		return;
	}

	if ((p->packet_type != NMB_PACKET) &&
	    (p->packet_type != DGRAM_PACKET)) {
		DEBUG(2, ("Discarding invalid packet type from %u: %d\n",
			  (unsigned int)procid_to_pid(&src), p->packet_type));
		return;
	}

	in_addr_to_sockaddr_storage(&ss, p->ip);
	pss = iface_ip((struct sockaddr *)(void *)&ss);

	if (pss == NULL) {
		DEBUG(2, ("Could not find ip for packet from %u\n",
			  (unsigned int)procid_to_pid(&src)));
		return;
	}

	local_ip = &((const struct sockaddr_in *)pss)->sin_addr;
	subrec = FIRST_SUBNET;

	p->recv_fd = -1;
	p->send_fd = (p->packet_type == NMB_PACKET) ?
		subrec->nmb_sock : subrec->dgram_sock;

	for (subrec = FIRST_SUBNET; subrec != NULL;
	     subrec = NEXT_SUBNET_EXCLUDING_UNICAST(subrec)) {
		if (ip_equal_v4(*local_ip, subrec->myip)) {
			p->send_fd = (p->packet_type == NMB_PACKET) ?
				subrec->nmb_sock : subrec->dgram_sock;
			break;
		}
	}

	if (p->packet_type == DGRAM_PACKET) {
		p->port = 138;
		p->packet.dgram.header.source_ip.s_addr = local_ip->s_addr;
		p->packet.dgram.header.source_port = 138;
	}

	send_packet(p);
}

/**************************************************************************** **
 The main select loop.
 **************************************************************************** */

static void process(void)
{
	bool run_election;

	while( True ) {
		time_t t = time(NULL);
		TALLOC_CTX *frame = talloc_stackframe();

		/*
		 * Check all broadcast subnets to see if
		 * we need to run an election on any of them.
		 * (nmbd_elections.c)
		 */

		run_election = check_elections();

		/*
		 * Read incoming UDP packets.
		 * (nmbd_packets.c)
		 */

		if(listen_for_packets(run_election)) {
			TALLOC_FREE(frame);
			return;
		}

		/*
		 * Process all incoming packets
		 * read above. This calls the success and
		 * failure functions registered when response
		 * packets arrrive, and also deals with request
		 * packets from other sources.
		 * (nmbd_packets.c)
		 */

		run_packet_queue();

		/*
		 * Run any elections - initiate becoming
		 * a local master browser if we have won.
		 * (nmbd_elections.c)
		 */

		run_elections(t);

		/*
		 * Send out any broadcast announcements
		 * of our server names. This also announces
		 * the workgroup name if we are a local
		 * master browser.
		 * (nmbd_sendannounce.c)
		 */

		announce_my_server_names(t);

		/*
		 * Send out any LanMan broadcast announcements
		 * of our server names.
		 * (nmbd_sendannounce.c)
		 */

		announce_my_lm_server_names(t);

		/*
		 * If we are a local master browser, periodically
		 * announce ourselves to the domain master browser.
		 * This also deals with syncronising the domain master
		 * browser server lists with ourselves as a local
		 * master browser.
		 * (nmbd_sendannounce.c)
		 */

		announce_myself_to_domain_master_browser(t);

		/*
		 * Fullfill any remote announce requests.
		 * (nmbd_sendannounce.c)
		 */

		announce_remote(t);

		/*
		 * Fullfill any remote browse sync announce requests.
		 * (nmbd_sendannounce.c)
		 */

		browse_sync_remote(t);

		/*
		 * Scan the broadcast subnets, and WINS client
		 * namelists and refresh any that need refreshing.
		 * (nmbd_mynames.c)
		 */

		refresh_my_names(t);

		/*
		 * Scan the subnet namelists and server lists and
		 * expire thos that have timed out.
		 * (nmbd.c)
		 */

		expire_names_and_servers(t);

		/*
		 * Write out a snapshot of our current browse list into
		 * the browse.dat file. This is used by smbd to service
		 * incoming NetServerEnum calls - used to synchronise
		 * browse lists over subnets.
		 * (nmbd_serverlistdb.c)
		 */

		write_browse_list(t, False);

		/*
		 * If we are a domain master browser, we have a list of
		 * local master browsers we should synchronise browse
		 * lists with (these are added by an incoming local
		 * master browser announcement packet). Expire any of
		 * these that are no longer current, and pull the server
		 * lists from each of these known local master browsers.
		 * (nmbd_browsesync.c)
		 */

		dmb_expire_and_sync_browser_lists(t);

		/*
		 * Check that there is a local master browser for our
		 * workgroup for all our broadcast subnets. If one
		 * is not found, start an election (which we ourselves
		 * may or may not participate in, depending on the
		 * setting of the 'local master' parameter.
		 * (nmbd_elections.c)
		 */

		check_master_browser_exists(t);

		/*
		 * If we are configured as a logon server, attempt to
		 * register the special NetBIOS names to become such
		 * (WORKGROUP<1c> name) on all broadcast subnets and
		 * with the WINS server (if used). If we are configured
		 * to become a domain master browser, attempt to register
		 * the special NetBIOS name (WORKGROUP<1b> name) to
		 * become such.
		 * (nmbd_become_dmb.c)
		 */

		add_domain_names(t);

		/*
		 * If we are a WINS server, do any timer dependent
		 * processing required.
		 * (nmbd_winsserver.c)
		 */

		initiate_wins_processing(t);

		/*
		 * If we are a domain master browser, attempt to contact the
		 * WINS server to get a list of all known WORKGROUPS/DOMAINS.
		 * This will only work to a Samba WINS server.
		 * (nmbd_browsesync.c)
		 */

		if (lp_enhanced_browsing())
			collect_all_workgroup_names_from_wins_server(t);

		/*
		 * Go through the response record queue and time out or re-transmit
		 * and expired entries.
		 * (nmbd_packets.c)
		 */

		retransmit_or_expire_response_records(t);

		/*
		 * check to see if any remote browse sync child processes have completed
		 */

		sync_check_completion();

		/*
		 * regularly sync with any other DMBs we know about 
		 */

		if (lp_enhanced_browsing())
			sync_all_dmbs(t);

		/* check for new network interfaces */

		reload_interfaces(t);

		/* free up temp memory */
		TALLOC_FREE(frame);
	}
}

/**************************************************************************** **
 Open the socket communication.
 **************************************************************************** */

static bool open_sockets(bool isdaemon, int port)
{
	struct sockaddr_storage ss;
	const char *sock_addr = lp_socket_address();

	/*
	 * The sockets opened here will be used to receive broadcast
	 * packets *only*. Interface specific sockets are opened in
	 * make_subnet() in namedbsubnet.c. Thus we bind to the
	 * address "0.0.0.0". The parameter 'socket address' is
	 * now deprecated.
	 */

	if (!interpret_string_addr(&ss, sock_addr,
				AI_NUMERICHOST|AI_PASSIVE)) {
		DEBUG(0,("open_sockets: unable to get socket address "
			"from string %s", sock_addr));
		return false;
	}
	if (ss.ss_family != AF_INET) {
		DEBUG(0,("open_sockets: unable to use IPv6 socket"
			"%s in nmbd\n",
			sock_addr));
		return false;
	}

	if (isdaemon) {
		ClientNMB = open_socket_in(SOCK_DGRAM, port,
					   0, &ss,
					   true);
	} else {
		ClientNMB = 0;
	}

	if (ClientNMB == -1) {
		return false;
	}

	ClientDGRAM = open_socket_in(SOCK_DGRAM, DGRAM_PORT,
					   3, &ss,
					   true);

	if (ClientDGRAM == -1) {
		if (ClientNMB != 0) {
			close(ClientNMB);
		}
		return false;
	}

	/* we are never interested in SIGPIPE */
	BlockSignals(True,SIGPIPE);

	set_socket_options( ClientNMB,   "SO_BROADCAST" );
	set_socket_options( ClientDGRAM, "SO_BROADCAST" );

	/* Ensure we're non-blocking. */
	set_blocking( ClientNMB, False);
	set_blocking( ClientDGRAM, False);

	DEBUG( 3, ( "open_sockets: Broadcast sockets opened.\n" ) );
	return( True );
}

/**************************************************************************** **
 main program
 **************************************************************************** */

 int main(int argc, const char *argv[])
{
	static bool is_daemon;
	static bool opt_interactive;
	static bool Fork = true;
	static bool no_process_group;
	static bool log_stdout;
	poptContext pc;
	char *p_lmhosts = NULL;
	int opt;
	enum {
		OPT_DAEMON = 1000,
		OPT_INTERACTIVE,
		OPT_FORK,
		OPT_NO_PROCESS_GROUP,
		OPT_LOG_STDOUT
	};
	struct poptOption long_options[] = {
	POPT_AUTOHELP
	{"daemon", 'D', POPT_ARG_NONE, NULL, OPT_DAEMON, "Become a daemon(default)" },
	{"interactive", 'i', POPT_ARG_NONE, NULL, OPT_INTERACTIVE, "Run interactive (not a daemon)" },
	{"foreground", 'F', POPT_ARG_NONE, NULL, OPT_FORK, "Run daemon in foreground (for daemontools & etc)" },
	{"no-process-group", 0, POPT_ARG_NONE, NULL, OPT_NO_PROCESS_GROUP, "Don't create a new process group" },
	{"log-stdout", 'S', POPT_ARG_NONE, NULL, OPT_LOG_STDOUT, "Log to stdout" },
	{"hosts", 'H', POPT_ARG_STRING, &p_lmhosts, 0, "Load a netbios hosts file"},
	{"port", 'p', POPT_ARG_INT, &global_nmb_port, 0, "Listen on the specified port" },
	POPT_COMMON_SAMBA
	{ NULL }
	};
	TALLOC_CTX *frame;
	NTSTATUS status;

	/*
	 * Do this before any other talloc operation
	 */
	talloc_enable_null_tracking();
	frame = talloc_stackframe();

	load_case_tables();

	global_nmb_port = NMB_PORT;

	pc = poptGetContext("nmbd", argc, argv, long_options, 0);
	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_DAEMON:
			is_daemon = true;
			break;
		case OPT_INTERACTIVE:
			opt_interactive = true;
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
		default:
			d_fprintf(stderr, "\nInvalid option %s: %s\n\n",
				  poptBadOption(pc, 0), poptStrerror(opt));
			poptPrintUsage(pc, stderr, 0);
			exit(1);
		}
	};
	poptFreeContext(pc);

	global_in_nmbd = true;
	
	StartupTime = time(NULL);
	
	sys_srandom(time(NULL) ^ sys_getpid());
	
	if (!override_logfile) {
		char *lfile = NULL;
		if (asprintf(&lfile, "%s/log.nmbd", get_dyn_LOGFILEBASE()) < 0) {
			exit(1);
		}
		lp_set_logfile(lfile);
		SAFE_FREE(lfile);
	}
	
	fault_setup((void (*)(void *))fault_continue );
	dump_core_setup("nmbd");
	
	/* POSIX demands that signals are inherited. If the invoking process has
	 * these signals masked, we will have problems, as we won't receive them. */
	BlockSignals(False, SIGHUP);
	BlockSignals(False, SIGUSR1);
	BlockSignals(False, SIGTERM);

#if defined(SIGFPE)
	/* we are never interested in SIGFPE */
	BlockSignals(True,SIGFPE);
#endif

	/* We no longer use USR2... */
#if defined(SIGUSR2)
	BlockSignals(True, SIGUSR2);
#endif

	if ( opt_interactive ) {
		Fork = False;
		log_stdout = True;
	}

	if ( log_stdout && Fork ) {
		DEBUG(0,("ERROR: Can't log to stdout (-S) unless daemon is in foreground (-F) or interactive (-i)\n"));
		exit(1);
	}
	if (log_stdout) {
		setup_logging( argv[0], DEBUG_STDOUT);
	} else {
		setup_logging( argv[0], DEBUG_FILE);
	}

	reopen_logs();

	DEBUG(0,("nmbd version %s started.\n", samba_version_string()));
	DEBUGADD(0,("%s\n", COPYRIGHT_STARTUP_MESSAGE));

	if (!lp_load_initial_only(get_dyn_CONFIGFILE())) {
		DEBUG(0, ("error opening config file\n"));
		exit(1);
	}

	if (nmbd_messaging_context() == NULL) {
		return 1;
	}

	if ( !reload_nmbd_services(False) )
		return(-1);

	if(!init_names())
		return -1;

	reload_nmbd_services( True );

	if (strequal(lp_workgroup(),"*")) {
		DEBUG(0,("ERROR: a workgroup name of * is no longer supported\n"));
		exit(1);
	}

	set_samba_nb_type();

	if (!is_daemon && !is_a_socket(0)) {
		DEBUG(0,("standard input is not a socket, assuming -D option\n"));
		is_daemon = True;
	}
  
	if (is_daemon && !opt_interactive) {
		DEBUG( 2, ( "Becoming a daemon.\n" ) );
		become_daemon(Fork, no_process_group, log_stdout);
	}

#if HAVE_SETPGID
	/*
	 * If we're interactive we want to set our own process group for 
	 * signal management.
	 */
	if (opt_interactive && !no_process_group)
		setpgid( (pid_t)0, (pid_t)0 );
#endif

	if (nmbd_messaging_context() == NULL) {
		return 1;
	}

#ifndef SYNC_DNS
	/* Setup the async dns. We do it here so it doesn't have all the other
		stuff initialised and thus chewing memory and sockets */
	if(lp_we_are_a_wins_server() && lp_dns_proxy()) {
		start_async_dns();
	}
#endif

	if (!directory_exist(lp_lockdir())) {
		mkdir(lp_lockdir(), 0755);
	}

	pidfile_create("nmbd");

	status = reinit_after_fork(nmbd_messaging_context(),
				   nmbd_event_context(),
				   procid_self(), false);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("reinit_after_fork() failed\n"));
		exit(1);
	}

	if (!nmbd_setup_sig_term_handler())
		exit(1);
	if (!nmbd_setup_sig_hup_handler())
		exit(1);

	/* get broadcast messages */

	if (!serverid_register(procid_self(),
			       FLAG_MSG_GENERAL|FLAG_MSG_DBWRAP)) {
		DEBUG(1, ("Could not register myself in serverid.tdb\n"));
		exit(1);
	}

	messaging_register(nmbd_messaging_context(), NULL,
			   MSG_FORCE_ELECTION, nmbd_message_election);
#if 0
	/* Until winsrepl is done. */
	messaging_register(nmbd_messaging_context(), NULL,
			   MSG_WINS_NEW_ENTRY, nmbd_wins_new_entry);
#endif
	messaging_register(nmbd_messaging_context(), NULL,
			   MSG_SHUTDOWN, nmbd_terminate);
	messaging_register(nmbd_messaging_context(), NULL,
			   MSG_SMB_CONF_UPDATED, msg_reload_nmbd_services);
	messaging_register(nmbd_messaging_context(), NULL,
			   MSG_SEND_PACKET, msg_nmbd_send_packet);

	TimeInit();

	DEBUG( 3, ( "Opening sockets %d\n", global_nmb_port ) );

	if ( !open_sockets( is_daemon, global_nmb_port ) ) {
		kill_async_dns_child();
		return 1;
	}

	/* Determine all the IP addresses we have. */
	load_interfaces();

	/* Create an nmbd subnet record for each of the above. */
	if( False == create_subnets() ) {
		DEBUG(0,("ERROR: Failed when creating subnet lists. Exiting.\n"));
		kill_async_dns_child();
		exit(1);
	}

	/* Load in any static local names. */ 
	if (p_lmhosts) {
		set_dyn_LMHOSTSFILE(p_lmhosts);
	}
	load_lmhosts_file(get_dyn_LMHOSTSFILE());
	DEBUG(3,("Loaded hosts file %s\n", get_dyn_LMHOSTSFILE()));

	/* If we are acting as a WINS server, initialise data structures. */
	if( !initialise_wins() ) {
		DEBUG( 0, ( "nmbd: Failed when initialising WINS server.\n" ) );
		kill_async_dns_child();
		exit(1);
	}

	/* 
	 * Register nmbd primary workgroup and nmbd names on all
	 * the broadcast subnets, and on the WINS server (if specified).
	 * Also initiate the startup of our primary workgroup (start
	 * elections if we are setup as being able to be a local
	 * master browser.
	 */

	if( False == register_my_workgroup_and_names() ) {
		DEBUG(0,("ERROR: Failed when creating my my workgroup. Exiting.\n"));
		kill_async_dns_child();
		exit(1);
	}

	if (!initialize_nmbd_proxy_logon()) {
		DEBUG(0,("ERROR: Failed setup nmbd_proxy_logon.\n"));
		kill_async_dns_child();
		exit(1);
	}

	if (!nmbd_init_packet_server()) {
		kill_async_dns_child();
                exit(1);
        }

	TALLOC_FREE(frame);
	process();

	kill_async_dns_child();
	return(0);
}
