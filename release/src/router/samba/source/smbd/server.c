/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Main SMB server routines
   Copyright (C) Andrew Tridgell 1992-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
#include "trans2.h"

pstring servicesf = CONFIGFILE;
extern pstring debugf;
extern fstring global_myworkgroup;
extern pstring global_myname;

int am_parent = 1;

/* the last message the was processed */
int last_message = -1;

/* a useful macro to debug the last message processed */
#define LAST_MESSAGE() smb_fn_name(last_message)

extern int DEBUGLEVEL;

extern pstring user_socket_options;

#ifdef WITH_DFS
extern int dcelogin_atmost_once;
#endif /* WITH_DFS */


extern fstring remote_machine;
extern pstring OriginalDir;

/****************************************************************************
  when exiting, take the whole family
****************************************************************************/
static void *dflt_sig(void)
{
	exit_server("caught signal");
	return NULL;
}

/****************************************************************************
  Send a SIGTERM to our process group.
*****************************************************************************/
static void  killkids(void)
{
	if(am_parent) kill(0,SIGTERM);
}


/****************************************************************************
  open the socket communication
****************************************************************************/
static BOOL open_sockets_inetd(void)
{
	extern int Client;

	/* Started from inetd. fd 0 is the socket. */
	/* We will abort gracefully when the client or remote system 
	   goes away */
	Client = dup(0);
	
	/* close our standard file descriptors */
	close_low_fds();
	
	set_socket_options(Client,"SO_KEEPALIVE");
	set_socket_options(Client,user_socket_options);

	return True;
}


/****************************************************************************
  open the socket communication
****************************************************************************/
static BOOL open_sockets(BOOL is_daemon,int port)
{
	extern int Client;
	int num_interfaces = iface_count();
	int fd_listenset[FD_SETSIZE];
	fd_set listen_set;
	int s;
	int i;

	if (!is_daemon) {
		return open_sockets_inetd();
	}

		
#ifdef HAVE_ATEXIT
	{
		static int atexit_set;
		if(atexit_set == 0) {
			atexit_set=1;
			atexit(killkids);
		}
	}
#endif

	/* Stop zombies */
	CatchChild();
		
		
	FD_ZERO(&listen_set);

	if(lp_interfaces() && lp_bind_interfaces_only()) {
		/* We have been given an interfaces line, and been 
		   told to only bind to those interfaces. Create a
		   socket per interface and bind to only these.
		*/
		
		if(num_interfaces > FD_SETSIZE) {
			DEBUG(0,("open_sockets: Too many interfaces specified to bind to. Number was %d \
max can be %d\n", 
				 num_interfaces, FD_SETSIZE));
			return False;
		}
		
		/* Now open a listen socket for each of the
		   interfaces. */
		for(i = 0; i < num_interfaces; i++) {
			struct in_addr *ifip = iface_n_ip(i);
			
			if(ifip == NULL) {
				DEBUG(0,("open_sockets: interface %d has NULL IP address !\n", i));
				continue;
			}
			s = fd_listenset[i] = open_socket_in(SOCK_STREAM, port, 0, ifip->s_addr, True);
			if(s == -1)
				return False;
				/* ready to listen */
			if (listen(s, 5) == -1) {
				DEBUG(0,("listen: %s\n",strerror(errno)));
				close(s);
				return False;
			}
			FD_SET(s,&listen_set);
		}
	} else {
		/* Just bind to 0.0.0.0 - accept connections
		   from anywhere. */
		num_interfaces = 1;
		
		/* open an incoming socket */
		s = open_socket_in(SOCK_STREAM, port, 0,
				   interpret_addr(lp_socket_address()),True);
		if (s == -1)
			return(False);
		
		/* ready to listen */
		if (listen(s, 5) == -1) {
			DEBUG(0,("open_sockets: listen: %s\n",
				 strerror(errno)));
			close(s);
			return False;
		}
		
		fd_listenset[0] = s;
		FD_SET(s,&listen_set);
	} 

	/* now accept incoming connections - forking a new process
	   for each incoming connection */
	DEBUG(2,("waiting for a connection\n"));
	while (1) {
		fd_set lfds;
		int num;
		
		memcpy((char *)&lfds, (char *)&listen_set, 
		       sizeof(listen_set));
		
		num = sys_select(FD_SETSIZE,&lfds,NULL);
		
		if (num == -1 && errno == EINTR)
			continue;
		
		/* check if we need to reload services */
		check_reload(time(NULL));

		/* Find the sockets that are read-ready -
		   accept on these. */
		for( ; num > 0; num--) {
			struct sockaddr addr;
			int in_addrlen = sizeof(addr);
			
			s = -1;
			for(i = 0; i < num_interfaces; i++) {
				if(FD_ISSET(fd_listenset[i],&lfds)) {
					s = fd_listenset[i];
					/* Clear this so we don't look
					   at it again. */
					FD_CLR(fd_listenset[i],&lfds);
					break;
				}
			}

			Client = accept(s,&addr,&in_addrlen);
			
			if (Client == -1 && errno == EINTR)
				continue;
			
			if (Client == -1) {
				DEBUG(0,("open_sockets: accept: %s\n",
					 strerror(errno)));
				continue;
			}
			
			if (Client != -1 && fork()==0) {
				/* Child code ... */
				
				/* close the listening socket(s) */
				for(i = 0; i < num_interfaces; i++)
					close(fd_listenset[i]);
				
				/* close our standard file
				   descriptors */
				close_low_fds();
				am_parent = 0;
				
				set_socket_options(Client,"SO_KEEPALIVE");
				set_socket_options(Client,user_socket_options);
				
				/* Reset global variables in util.c so
				   that client substitutions will be
				   done correctly in the process.  */
				reset_globals_after_fork();

                /*
                 * Ensure this child has kernel oplock
                 * capabilities, but not it's children.
                 */
                set_process_capability(KERNEL_OPLOCK_CAPABILITY, True);
                set_inherited_process_capability(KERNEL_OPLOCK_CAPABILITY, False);

				return True; 
			}
			/* The parent doesn't need this socket */
			close(Client); 

			/* Sun May 6 18:56:14 2001 ackley@cs.unm.edu:
				Clear the closed fd info out of Client in
				util_sock.c to avoid a possible
				getpeername failure if we reopen the logs
				and use %I in the filename.
			*/
			Client = -1;

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
 
		} /* end for num */
	} /* end while 1 */

/* NOTREACHED	return True; */
}

/****************************************************************************
  reload the services file
  **************************************************************************/
BOOL reload_services(BOOL test)
{
	BOOL ret;

	if (lp_loaded()) {
		pstring fname;
		pstrcpy(fname,lp_configfile());
		if (file_exist(fname,NULL) && !strcsequal(fname,servicesf)) {
			pstrcpy(servicesf,fname);
			test = False;
		}
	}

	reopen_logs();

	if (test && !lp_file_list_changed())
		return(True);

	lp_killunused(conn_snum_used);

	ret = lp_load(servicesf,False,False,True);
#ifdef PRINTING
	load_printers();
#endif
	/* perhaps the config filename is now set */
	if (!test)
		reload_services(True);

	reopen_logs();

	load_interfaces();

	{
		extern int Client;
		if (Client != -1) {      
			set_socket_options(Client,"SO_KEEPALIVE");
			set_socket_options(Client,user_socket_options);
		}
	}

	reset_mangled_cache();
    reset_stat_cache();

	/* this forces service parameters to be flushed */
	become_service(NULL,True);

	return(ret);
}



/****************************************************************************
 Catch a sighup.
****************************************************************************/

VOLATILE SIG_ATOMIC_T reload_after_sighup = False;

static void sig_hup(int sig)
{
	BlockSignals(True,SIGHUP);
	DEBUG(1,("Got SIGHUP\n"));

	/*
	 * Fix from <branko.cibej@hermes.si> here.
	 * We used to reload in the signal handler - this
	 * is a *BIG* no-no.
	 */

	reload_after_sighup = True;
	BlockSignals(False,SIGHUP);
}



#if DUMP_CORE
/*******************************************************************
prepare to dump a core file - carefully!
********************************************************************/
static BOOL dump_core(void)
{
	char *p;
	pstring dname;
	pstrcpy(dname,debugf);
	if ((p=strrchr(dname,'/'))) *p=0;
	pstrcat(dname,"/corefiles");
	mkdir(dname,0700);
	sys_chown(dname,getuid(),getgid());
	chmod(dname,0700);
	if (chdir(dname)) return(False);
	umask(~(0700));

#ifdef HAVE_GETRLIMIT
#ifdef RLIMIT_CORE
	{
		struct rlimit rlp;
		getrlimit(RLIMIT_CORE, &rlp);
		rlp.rlim_cur = MAX(4*1024*1024,rlp.rlim_cur);
		setrlimit(RLIMIT_CORE, &rlp);
		getrlimit(RLIMIT_CORE, &rlp);
		DEBUG(3,("Core limits now %d %d\n",
			 (int)rlp.rlim_cur,(int)rlp.rlim_max));
	}
#endif
#endif


	DEBUG(0,("Dumping core in %s\n",dname));
	abort();
	return(True);
}
#endif


/****************************************************************************
exit the server
****************************************************************************/
void exit_server(char *reason)
{
	static int firsttime=1;
	extern char *last_inbuf;


	if (!firsttime) exit(0);
	firsttime = 0;

	unbecome_user();
	DEBUG(2,("Closing connections\n"));

	conn_close_all();

    respond_to_all_remaining_local_messages();

#ifdef WITH_DFS
	if (dcelogin_atmost_once) {
		dfs_unlogin();
	}
#endif

	if (!reason) {   
		int oldlevel = DEBUGLEVEL;
		DEBUGLEVEL = 10;
		DEBUG(0,("Last message was %s\n",smb_fn_name(last_message)));
		if (last_inbuf)
			show_msg(last_inbuf);
		DEBUGLEVEL = oldlevel;
		DEBUG(0,("===============================================================\n"));
#if DUMP_CORE
		if (dump_core()) return;
#endif
	}    

	locking_end();

	DEBUG(3,("Server exit (%s)\n", (reason ? reason : "")));
	exit(0);
}



/****************************************************************************
  initialise connect, service and file structs
****************************************************************************/
static void init_structs(void )
{
	/*
	 * Set the machine NETBIOS name if not already
	 * set from the config file.
	 */

	if (!*global_myname) {
		char *p;
		fstrcpy( global_myname, myhostname() );
		p = strchr( global_myname, '.' );
		if (p) 
			*p = 0;
	}

	strupper( global_myname );

	conn_init();

	file_init();

	/* for RPC pipes */
	init_rpc_pipe_hnd();

	/* for LSA handles */
	init_lsa_policy_hnd();

	init_dptrs();
}

/****************************************************************************
usage on the program
****************************************************************************/
static void usage(char *pname)
{

	printf("Usage: %s [-DaoPh?V] [-d debuglevel] [-l log basename] [-p port]\n", pname);
	printf("       [-O socket options] [-s services file]\n");
	printf("\t-D                    Become a daemon\n");
	printf("\t-a                    Append to log file (default)\n");
	printf("\t-o                    Overwrite log file, don't append\n");
	printf("\t-P                    Passive only\n");
	printf("\t-h                    Print usage\n");
	printf("\t-?                    Print usage\n");
	printf("\t-V                    Print version\n");
	printf("\t-d debuglevel         Set the debuglevel\n");
	printf("\t-l log basename.      Basename for log/debug files\n");
	printf("\t-p port               Listen on the specified port\n");
	printf("\t-O socket options     Socket options\n");
	printf("\t-s services file.     Filename of services file\n");
	printf("\n");
}


/****************************************************************************
  main program
****************************************************************************/
 int main(int argc,char *argv[])
{
	extern BOOL append_log;
	/* shall I run as a daemon */
	BOOL is_daemon = False;
	BOOL specified_logfile = False;
	int port = SMB_PORT;
	int opt;
	extern char *optarg;
	
#ifdef HAVE_SET_AUTH_PARAMETERS
	set_auth_parameters(argc,argv);
#endif

	/* this is for people who can't start the program correctly */
	while (argc > 1 && (*argv[1] != '-')) {
		argv++;
		argc--;
	}

	while ( EOF != (opt = getopt(argc, argv, "O:i:l:s:d:Dp:h?VPaof:")) )
		switch (opt)  {
		case 'O':
			pstrcpy(user_socket_options,optarg);
			break;

		case 'P':
			{
				extern BOOL passive;
				passive = True;
			}
			break;	

		case 's':
			pstrcpy(servicesf,optarg);
			break;

		case 'l':
			specified_logfile = True;
			pstrcpy(debugf,optarg);
			break;

		case 'a':
			append_log = True;
			break;

		case 'o':
			append_log = False;
			break;

		case 'D':
			is_daemon = True;
			break;

		case 'd':
			if (*optarg == 'A')
				DEBUGLEVEL = 10000;
			else
				DEBUGLEVEL = atoi(optarg);
			break;

		case 'p':
			port = atoi(optarg);
			break;

		case 'h':
		case '?':
			usage(argv[0]);
			exit(0);
			break;

		case 'V':
			printf("Version %s\n",VERSION);
			exit(0);
			break;
		default:
			DEBUG(0,("Incorrect program usage - are you sure the command line is correct?\n"));
			usage(argv[0]);
			exit(1);
		}

#ifdef HAVE_SETLUID
	/* needed for SecureWare on SCO */
	setluid(0);
#endif

	/*
	 * gain_root_privilege uses an assert than will cause a core
	 * dump if euid != 0. Ensure this is the case.
	 */

	if(geteuid() != (uid_t)0) {
		fprintf(stderr, "%s: Version %s : Must have effective user id of zero to run.\n", argv[0], VERSION);
		exit(1);
	}

	append_log = True;

	TimeInit();

	if(!specified_logfile)
		pstrcpy(debugf,SMBLOGFILE);  

	pstrcpy(remote_machine, "smb");

	setup_logging(argv[0],False);

	charset_initialise();

	/* we want to re-seed early to prevent time delays causing
           client problems at a later date. (tridge) */
	generate_random_buffer(NULL, 0, False);

	/* make absolutely sure we run as root - to handle cases where people
	   are crazy enough to have it setuid */

	gain_root_privilege();
	gain_root_group_privilege();

	fault_setup((void (*)(void *))exit_server);
	CatchSignal(SIGTERM , SIGNAL_CAST dflt_sig);

	/* we are never interested in SIGPIPE */
	BlockSignals(True,SIGPIPE);

#if defined(SIGFPE)
	/* we are never interested in SIGFPE */
	BlockSignals(True,SIGFPE);
#endif

	/* we want total control over the permissions on created files,
	   so set our umask to 0 */
	umask(0);

	dos_GetWd(OriginalDir);

	init_uid();

	reopen_logs();

	DEBUG(1,( "smbd version %s started.\n", VERSION));
	DEBUGADD(1,( "Copyright Andrew Tridgell 1992-1998\n"));

	DEBUG(2,("uid=%d gid=%d euid=%d egid=%d\n",
		 (int)getuid(),(int)getgid(),(int)geteuid(),(int)getegid()));

	if (sizeof(uint16) < 2 || sizeof(uint32) < 4) {
		DEBUG(0,("ERROR: Samba is not configured correctly for the word size on your machine\n"));
		exit(1);
	}

	/*
	 * Do this before reload_services.
	 */

	if (!reload_services(False))
		return(-1);	

	init_structs();
	
#ifdef WITH_PROFILE
	if (!profile_setup(False)) {
		DEBUG(0,("ERROR: failed to setup profiling\n"));
		return -1;
	}
#endif

#ifdef WITH_SSL
	{
		extern BOOL sslEnabled;
		sslEnabled = lp_ssl_enabled();
		if(sslEnabled)
			sslutil_init(True);
	}
#endif        /* WITH_SSL */

	codepage_initialise(lp_client_code_page());

	fstrcpy(global_myworkgroup, lp_workgroup());

	if(!pdb_generate_sam_sid()) {
		DEBUG(0,("ERROR: Samba cannot create a SAM SID.\n"));
		exit(1);
	}

	CatchSignal(SIGHUP,SIGNAL_CAST sig_hup);
	
	/* Setup the signals that allow the debug log level
	   to by dynamically changed. */
 
	/* If we are using the malloc debug code we can't use
	   SIGUSR1 and SIGUSR2 to do debug level changes. */
	
#ifndef MEM_MAN
#if defined(SIGUSR1)
	CatchSignal( SIGUSR1, SIGNAL_CAST sig_usr1 );
#endif /* SIGUSR1 */
   
#if defined(SIGUSR2)
	CatchSignal( SIGUSR2, SIGNAL_CAST sig_usr2 );
#endif /* SIGUSR2 */
#endif /* MEM_MAN */

	DEBUG(3,( "loaded services\n"));

	if (!is_daemon && !is_a_socket(0)) {
		DEBUG(0,("standard input is not a socket, assuming -D option\n"));
		is_daemon = True;
	}

	if (is_daemon) {
		DEBUG( 3, ( "Becoming a daemon.\n" ) );
		become_daemon();
	}

    check_kernel_oplocks();

	if (!directory_exist(lp_lockdir(), NULL)) {
		mkdir(lp_lockdir(), 0755);
	}

	if (is_daemon) {
		pidfile_create("smbd");
	}

	if (!open_sockets(is_daemon,port))
		exit(1);

	/*
	 * Note that this call should be done after the fork() call
	 * in open_sockets(), as some versions of the locking shared
	 * memory code register openers in a flat file.
	 */

	if (!locking_init(0))
		exit(1);

	if(!initialize_password_db())
		exit(1);

	/* possibly reload the services file. */
	reload_services(True);
	
	if (*lp_rootdir()) {
		if (sys_chroot(lp_rootdir()) == 0)
			DEBUG(2,("Changed root to %s\n", lp_rootdir()));
	}

	/* Setup the oplock IPC socket. */
	if( !open_oplock_ipc() )
		exit(1);

	smbd_process();
	close_sockets();
	
	exit_server("normal exit");
	return(0);
}
