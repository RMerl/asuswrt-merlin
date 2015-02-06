/* dnsmasq is Copyright (c) 2000-2014 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
     
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* Declare static char *compiler_opts  in config.h */
#define DNSMASQ_COMPILE_OPTS

#include "dnsmasq.h"

struct daemon *daemon;

static volatile pid_t pid = 0;
static volatile int pipewrite;

static int set_dns_listeners(time_t now, fd_set *set, int *maxfdp);
static void check_dns_listeners(fd_set *set, time_t now);
static void sig_handler(int sig);
static void async_event(int pipe, time_t now);
static void fatal_event(struct event_desc *ev, char *msg);
static int read_event(int fd, struct event_desc *evp, char **msg);
static void poll_resolv(int force, int do_reload, time_t now);

int main (int argc, char **argv)
{
  int bind_fallback = 0;
  time_t now;
  struct sigaction sigact;
  struct iname *if_tmp;
  int piperead, pipefd[2], err_pipe[2];
  struct passwd *ent_pw = NULL;
#if defined(HAVE_SCRIPT)
  uid_t script_uid = 0;
  gid_t script_gid = 0;
#endif
  struct group *gp = NULL;
  long i, max_fd = sysconf(_SC_OPEN_MAX);
  char *baduser = NULL;
  int log_err;
#if defined(HAVE_LINUX_NETWORK)
  cap_user_header_t hdr = NULL;
  cap_user_data_t data = NULL;
  char *bound_device = NULL;
  int did_bind = 0;
#endif 
#if defined(HAVE_DHCP) || defined(HAVE_DHCP6)
  struct dhcp_context *context;
  struct dhcp_relay *relay;
#endif

#ifdef LOCALEDIR
  setlocale(LC_ALL, "");
  bindtextdomain("dnsmasq", LOCALEDIR); 
  textdomain("dnsmasq");
#endif

  sigact.sa_handler = sig_handler;
  sigact.sa_flags = 0;
  sigemptyset(&sigact.sa_mask);
  sigaction(SIGUSR1, &sigact, NULL);
  sigaction(SIGUSR2, &sigact, NULL);
  sigaction(SIGHUP, &sigact, NULL);
  sigaction(SIGTERM, &sigact, NULL);
  sigaction(SIGALRM, &sigact, NULL);
  sigaction(SIGCHLD, &sigact, NULL);

  /* ignore SIGPIPE */
  sigact.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sigact, NULL);

  umask(022); /* known umask, create leases and pid files as 0644 */
 
  rand_init(); /* Must precede read_opts() */
  
  read_opts(argc, argv, compile_opts);
 
  if (daemon->edns_pktsz < PACKETSZ)
    daemon->edns_pktsz = PACKETSZ;

  daemon->packet_buff_sz = daemon->edns_pktsz > DNSMASQ_PACKETSZ ? 
    daemon->edns_pktsz : DNSMASQ_PACKETSZ;
  daemon->packet = safe_malloc(daemon->packet_buff_sz);
  
  daemon->addrbuff = safe_malloc(ADDRSTRLEN);
  if (option_bool(OPT_EXTRALOG))
    daemon->addrbuff2 = safe_malloc(ADDRSTRLEN);
  
#ifdef HAVE_DNSSEC
  if (option_bool(OPT_DNSSEC_VALID))
    {
      daemon->keyname = safe_malloc(MAXDNAME);
      daemon->workspacename = safe_malloc(MAXDNAME);
    }
#endif

#ifdef HAVE_DHCP
  if (!daemon->lease_file)
    {
      if (daemon->dhcp || daemon->dhcp6)
	daemon->lease_file = LEASEFILE;
    }
#endif
  
  /* Close any file descriptors we inherited apart from std{in|out|err} 
     
     Ensure that at least stdin, stdout and stderr (fd 0, 1, 2) exist,
     otherwise file descriptors we create can end up being 0, 1, or 2 
     and then get accidentally closed later when we make 0, 1, and 2 
     open to /dev/null. Normally we'll be started with 0, 1 and 2 open, 
     but it's not guaranteed. By opening /dev/null three times, we 
     ensure that we're not using those fds for real stuff. */
  for (i = 0; i < max_fd; i++)
    if (i != STDOUT_FILENO && i != STDERR_FILENO && i != STDIN_FILENO)
      close(i);
    else
      open("/dev/null", O_RDWR); 

#ifndef HAVE_LINUX_NETWORK
#  if !(defined(IP_RECVDSTADDR) && defined(IP_RECVIF) && defined(IP_SENDSRCADDR))
  if (!option_bool(OPT_NOWILD))
    {
      bind_fallback = 1;
      set_option_bool(OPT_NOWILD);
    }
#  endif
  
  /* -- bind-dynamic not supported on !Linux, fall back to --bind-interfaces */
  if (option_bool(OPT_CLEVERBIND))
    {
      bind_fallback = 1;
      set_option_bool(OPT_NOWILD);
      reset_option_bool(OPT_CLEVERBIND);
    }
#endif

#ifndef HAVE_INOTIFY
  if (daemon->inotify_hosts)
    die(_("dhcp-hostsdir not supported on this platform"), NULL, EC_BADCONF);
#endif
  
  if (option_bool(OPT_DNSSEC_VALID))
    {
#ifdef HAVE_DNSSEC
      if (!daemon->ds)
	die(_("No trust anchors provided for DNSSEC"), NULL, EC_BADCONF);
      
      if (daemon->cachesize < CACHESIZ)
	die(_("Cannot reduce cache size from default when DNSSEC enabled"), NULL, EC_BADCONF);
#else 
      die(_("DNSSEC not available: set HAVE_DNSSEC in src/config.h"), NULL, EC_BADCONF);
#endif
    }

#ifndef HAVE_TFTP
  if (option_bool(OPT_TFTP))
    die(_("TFTP server not available: set HAVE_TFTP in src/config.h"), NULL, EC_BADCONF);
#endif

#ifdef HAVE_CONNTRACK
  if (option_bool(OPT_CONNTRACK) && (daemon->query_port != 0 || daemon->osport))
    die (_("Cannot use --conntrack AND --query-port"), NULL, EC_BADCONF); 
#else
  if (option_bool(OPT_CONNTRACK))
    die(_("Conntrack support not available: set HAVE_CONNTRACK in src/config.h"), NULL, EC_BADCONF);
#endif

#ifdef HAVE_SOLARIS_NETWORK
  if (daemon->max_logs != 0)
    die(_("asychronous logging is not available under Solaris"), NULL, EC_BADCONF);
#endif
  
#ifdef __ANDROID__
  if (daemon->max_logs != 0)
    die(_("asychronous logging is not available under Android"), NULL, EC_BADCONF);
#endif

#ifndef HAVE_AUTH
  if (daemon->authserver)
    die(_("authoritative DNS not available: set HAVE_AUTH in src/config.h"), NULL, EC_BADCONF);
#endif

#ifndef HAVE_LOOP
  if (option_bool(OPT_LOOP_DETECT))
    die(_("Loop detection not available: set HAVE_LOOP in src/config.h"), NULL, EC_BADCONF);
#endif
  
  now = dnsmasq_time();

  /* Create a serial at startup if not configured. */
  if (daemon->authinterface && daemon->soa_sn == 0)
#ifdef HAVE_BROKEN_RTC
    die(_("zone serial must be configured in --auth-soa"), NULL, EC_BADCONF);
#else
  daemon->soa_sn = now;
#endif
  
#ifdef HAVE_DHCP6
  if (daemon->dhcp6)
    {
      daemon->doing_ra = option_bool(OPT_RA);
      
      for (context = daemon->dhcp6; context; context = context->next)
	{
	  if (context->flags & CONTEXT_DHCP)
	    daemon->doing_dhcp6 = 1;
	  if (context->flags & CONTEXT_RA)
	    daemon->doing_ra = 1;
#if !defined(HAVE_LINUX_NETWORK) && !defined(HAVE_BSD_NETWORK)
	  if (context->flags & CONTEXT_TEMPLATE)
	    die (_("dhcp-range constructor not available on this platform"), NULL, EC_BADCONF);
#endif 
	}
    }
#endif
  
#ifdef HAVE_DHCP
  /* Note that order matters here, we must call lease_init before
     creating any file descriptors which shouldn't be leaked
     to the lease-script init process. We need to call common_init
     before lease_init to allocate buffers it uses.*/
  if (daemon->dhcp || daemon->doing_dhcp6 || daemon->relay4 || daemon->relay6)
    {
      dhcp_common_init();
      if (daemon->dhcp || daemon->doing_dhcp6)
	lease_init(now);
    }
  
  if (daemon->dhcp || daemon->relay4)
    dhcp_init();
  
#  ifdef HAVE_DHCP6
  if (daemon->doing_ra || daemon->doing_dhcp6 || daemon->relay6)
    ra_init(now);
  
  if (daemon->doing_dhcp6 || daemon->relay6)
    dhcp6_init();
#  endif

#endif

#ifdef HAVE_IPSET
  if (daemon->ipsets)
    ipset_init();
#endif

#if  defined(HAVE_LINUX_NETWORK)
  netlink_init();
#elif defined(HAVE_BSD_NETWORK)
  route_init();
#endif

  if (option_bool(OPT_NOWILD) && option_bool(OPT_CLEVERBIND))
    die(_("cannot set --bind-interfaces and --bind-dynamic"), NULL, EC_BADCONF);
  
  if (!enumerate_interfaces(1) || !enumerate_interfaces(0))
    die(_("failed to find list of interfaces: %s"), NULL, EC_MISC);
  
  if (option_bool(OPT_NOWILD) || option_bool(OPT_CLEVERBIND)) 
    {
      create_bound_listeners(1);
      
      if (!option_bool(OPT_CLEVERBIND))
	for (if_tmp = daemon->if_names; if_tmp; if_tmp = if_tmp->next)
	  if (if_tmp->name && !if_tmp->used)
	    die(_("unknown interface %s"), if_tmp->name, EC_BADNET);

#if defined(HAVE_LINUX_NETWORK) && defined(HAVE_DHCP)
      /* after enumerate_interfaces()  */
      bound_device = whichdevice();
      
      if (daemon->dhcp)
	{
	  if (!daemon->relay4 && bound_device)
	    {
	      bindtodevice(bound_device, daemon->dhcpfd);
	      did_bind = 1;
	    }
	  if (daemon->enable_pxe && bound_device)
	    {
	      bindtodevice(bound_device, daemon->pxefd);
	      did_bind = 1;
	    }
	}
#endif

#if defined(HAVE_LINUX_NETWORK) && defined(HAVE_DHCP6)
      if (daemon->doing_dhcp6 && !daemon->relay6 && bound_device)
	{
	  bindtodevice(bound_device, daemon->dhcp6fd);
	  did_bind = 1;
	}
#endif
    }
  else 
    create_wildcard_listeners();
 
#ifdef HAVE_DHCP6
  /* after enumerate_interfaces() */
  if (daemon->doing_dhcp6 || daemon->relay6 || daemon->doing_ra)
    join_multicast(1);

  /* After netlink_init() and before create_helper() */
  lease_make_duid(now);
#endif
  
  if (daemon->port != 0)
    {
      cache_init();

#ifdef HAVE_DNSSEC
      blockdata_init();
#endif
    }

#ifdef HAVE_INOTIFY
  if ((!option_bool(OPT_NO_POLL) && daemon->port != 0) ||
      daemon->dhcp || daemon->doing_dhcp6)
    inotify_dnsmasq_init();
  else
    daemon->inotifyfd = -1;
#endif
       
  if (option_bool(OPT_DBUS))
#ifdef HAVE_DBUS
    {
      char *err;
      daemon->dbus = NULL;
      daemon->watches = NULL;
      if ((err = dbus_init()))
	die(_("DBus error: %s"), err, EC_MISC);
    }
#else
  die(_("DBus not available: set HAVE_DBUS in src/config.h"), NULL, EC_BADCONF);
#endif
  
  if (daemon->port != 0)
    pre_allocate_sfds();

#if defined(HAVE_SCRIPT)
  /* Note getpwnam returns static storage */
  if ((daemon->dhcp || daemon->dhcp6) && 
      daemon->scriptuser && 
      (daemon->lease_change_command || daemon->luascript))
    {
      if ((ent_pw = getpwnam(daemon->scriptuser)))
	{
	  script_uid = ent_pw->pw_uid;
	  script_gid = ent_pw->pw_gid;
	 }
      else
	baduser = daemon->scriptuser;
    }
#endif
  
  if (daemon->username && !(ent_pw = getpwnam(daemon->username)))
    baduser = daemon->username;
  else if (daemon->groupname && !(gp = getgrnam(daemon->groupname)))
    baduser = daemon->groupname;

  if (baduser)
    die(_("unknown user or group: %s"), baduser, EC_BADCONF);
   
  /* implement group defaults, "dip" if available, or group associated with uid */
  if (!daemon->group_set && !gp)
    {
      if (!(gp = getgrnam(CHGRP)) && ent_pw)
	gp = getgrgid(ent_pw->pw_gid);
      
      /* for error message */
      if (gp)
	daemon->groupname = gp->gr_name; 
    }

#if defined(HAVE_LINUX_NETWORK)
  /* determine capability API version here, while we can still
     call safe_malloc */
  if (ent_pw && ent_pw->pw_uid != 0)
    {
      int capsize = 1; /* for header version 1 */
      hdr = safe_malloc(sizeof(*hdr));

      /* find version supported by kernel */
      memset(hdr, 0, sizeof(*hdr));
      capget(hdr, NULL);
      
      if (hdr->version != LINUX_CAPABILITY_VERSION_1)
	{
	  /* if unknown version, use largest supported version (3) */
	  if (hdr->version != LINUX_CAPABILITY_VERSION_2)
	    hdr->version = LINUX_CAPABILITY_VERSION_3;
	  capsize = 2;
	}
      
      data = safe_malloc(sizeof(*data) * capsize);
      memset(data, 0, sizeof(*data) * capsize);
    }
#endif

  /* Use a pipe to carry signals and other events back to the event loop 
     in a race-free manner and another to carry errors to daemon-invoking process */
  safe_pipe(pipefd, 1);
  
  piperead = pipefd[0];
  pipewrite = pipefd[1];
  /* prime the pipe to load stuff first time. */
  send_event(pipewrite, EVENT_INIT, 0, NULL); 

  err_pipe[1] = -1;
  
  if (!option_bool(OPT_DEBUG))   
    {
      /* The following code "daemonizes" the process. 
	 See Stevens section 12.4 */
      
      if (chdir("/") != 0)
	die(_("cannot chdir to filesystem root: %s"), NULL, EC_MISC); 

#ifndef NO_FORK      
      if (!option_bool(OPT_NO_FORK))
	{
	  pid_t pid;
	  
	  /* pipe to carry errors back to original process.
	     When startup is complete we close this and the process terminates. */
	  safe_pipe(err_pipe, 0);
	  
	  if ((pid = fork()) == -1)
	    /* fd == -1 since we've not forked, never returns. */
	    send_event(-1, EVENT_FORK_ERR, errno, NULL);
	   
	  if (pid != 0)
	    {
	      struct event_desc ev;
	      char *msg;

	      /* close our copy of write-end */
	      close(err_pipe[1]);
	      
	      /* check for errors after the fork */
	      if (read_event(err_pipe[0], &ev, &msg))
		fatal_event(&ev, msg);
	      
	      _exit(EC_GOOD);
	    } 
	  
	  close(err_pipe[0]);

	  /* NO calls to die() from here on. */
	  
	  setsid();
	 
	  if ((pid = fork()) == -1)
	    send_event(err_pipe[1], EVENT_FORK_ERR, errno, NULL);
	 
	  if (pid != 0)
	    _exit(0);
	}
#endif
            
      /* write pidfile _after_ forking ! */
      if (daemon->runfile)
	{
	  int fd, err = 0;

	  sprintf(daemon->namebuff, "%d\n", (int) getpid());

	  /* Explanation: Some installations of dnsmasq (eg Debian/Ubuntu) locate the pid-file
	     in a directory which is writable by the non-privileged user that dnsmasq runs as. This
	     allows the daemon to delete the file as part of its shutdown. This is a security hole to the 
	     extent that an attacker running as the unprivileged  user could replace the pidfile with a 
	     symlink, and have the target of that symlink overwritten as root next time dnsmasq starts. 

	     The folowing code first deletes any existing file, and then opens it with the O_EXCL flag,
	     ensuring that the open() fails should there be any existing file (because the unlink() failed, 
	     or an attacker exploited the race between unlink() and open()). This ensures that no symlink
	     attack can succeed. 

	     Any compromise of the non-privileged user still theoretically allows the pid-file to be
	     replaced whilst dnsmasq is running. The worst that could allow is that the usual 
	     "shutdown dnsmasq" shell command could be tricked into stopping any other process.

	     Note that if dnsmasq is started as non-root (eg for testing) it silently ignores 
	     failure to write the pid-file.
	  */

	  unlink(daemon->runfile); 
	  
	  if ((fd = open(daemon->runfile, O_WRONLY|O_CREAT|O_TRUNC|O_EXCL, S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH)) == -1)
	    {
	      /* only complain if started as root */
	      if (getuid() == 0)
		err = 1;
	    }
	  else
	    {
	      if (!read_write(fd, (unsigned char *)daemon->namebuff, strlen(daemon->namebuff), 0))
		err = 1;
	      
	      while (!err && close(fd) == -1)
		if (!retry_send())
		  err = 1;
	    }

	  if (err)
	    {
	      send_event(err_pipe[1], EVENT_PIDFILE, errno, daemon->runfile);
	      _exit(0);
	    }
	}
    }
  
   log_err = log_start(ent_pw, err_pipe[1]);

   if (!option_bool(OPT_DEBUG)) 
     {       
       /* open  stdout etc to /dev/null */
       int nullfd = open("/dev/null", O_RDWR);
       dup2(nullfd, STDOUT_FILENO);
       dup2(nullfd, STDERR_FILENO);
       dup2(nullfd, STDIN_FILENO);
       close(nullfd);
     }
   
   /* if we are to run scripts, we need to fork a helper before dropping root. */
  daemon->helperfd = -1;
#ifdef HAVE_SCRIPT 
  if ((daemon->dhcp || daemon->dhcp6) && (daemon->lease_change_command || daemon->luascript))
    daemon->helperfd = create_helper(pipewrite, err_pipe[1], script_uid, script_gid, max_fd);
#endif

  if (!option_bool(OPT_DEBUG) && getuid() == 0)   
    {
      int bad_capabilities = 0;
      gid_t dummy;
      
      /* remove all supplimentary groups */
      if (gp && 
	  (setgroups(0, &dummy) == -1 ||
	   setgid(gp->gr_gid) == -1))
	{
	  send_event(err_pipe[1], EVENT_GROUP_ERR, errno, daemon->groupname);
	  _exit(0);
	}
  
      if (ent_pw && ent_pw->pw_uid != 0)
	{     
#if defined(HAVE_LINUX_NETWORK)	  
	  /* On linux, we keep CAP_NETADMIN (for ARP-injection) and
	     CAP_NET_RAW (for icmp) if we're doing dhcp. If we have yet to bind 
	     ports because of DAD, or we're doing it dynamically,
	     we need CAP_NET_BIND_SERVICE too. */
	  if (is_dad_listeners() || option_bool(OPT_CLEVERBIND))
	    data->effective = data->permitted = data->inheritable =
	      (1 << CAP_NET_ADMIN) | (1 << CAP_NET_RAW) | 
	      (1 << CAP_SETUID) | (1 << CAP_NET_BIND_SERVICE);
	  else
	    data->effective = data->permitted = data->inheritable =
	      (1 << CAP_NET_ADMIN) | (1 << CAP_NET_RAW) | (1 << CAP_SETUID);
	  
	  /* Tell kernel to not clear capabilities when dropping root */
	  if (capset(hdr, data) == -1 || prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0) == -1)
	    bad_capabilities = errno;
			  
#elif defined(HAVE_SOLARIS_NETWORK)
	  /* http://developers.sun.com/solaris/articles/program_privileges.html */
	  priv_set_t *priv_set;
	  
	  if (!(priv_set = priv_str_to_set("basic", ",", NULL)) ||
	      priv_addset(priv_set, PRIV_NET_ICMPACCESS) == -1 ||
	      priv_addset(priv_set, PRIV_SYS_NET_CONFIG) == -1)
	    bad_capabilities = errno;

	  if (priv_set && bad_capabilities == 0)
	    {
	      priv_inverse(priv_set);
	  
	      if (setppriv(PRIV_OFF, PRIV_LIMIT, priv_set) == -1)
		bad_capabilities = errno;
	    }

	  if (priv_set)
	    priv_freeset(priv_set);

#endif    

	  if (bad_capabilities != 0)
	    {
	      send_event(err_pipe[1], EVENT_CAP_ERR, bad_capabilities, NULL);
	      _exit(0);
	    }
	  
	  /* finally drop root */
	  if (setuid(ent_pw->pw_uid) == -1)
	    {
	      send_event(err_pipe[1], EVENT_USER_ERR, errno, daemon->username);
	      _exit(0);
	    }     

#ifdef HAVE_LINUX_NETWORK
	  if (is_dad_listeners() || option_bool(OPT_CLEVERBIND))
	   data->effective = data->permitted =
	     (1 << CAP_NET_ADMIN) | (1 << CAP_NET_RAW) | (1 << CAP_NET_BIND_SERVICE);
	 else
	   data->effective = data->permitted = 
	     (1 << CAP_NET_ADMIN) | (1 << CAP_NET_RAW);
	  data->inheritable = 0;
	  
	  /* lose the setuid and setgid capbilities */
	  if (capset(hdr, data) == -1)
	    {
	      send_event(err_pipe[1], EVENT_CAP_ERR, errno, NULL);
	      _exit(0);
	    }
#endif
	  
	}
    }
  
#ifdef HAVE_LINUX_NETWORK
  if (option_bool(OPT_DEBUG)) 
    prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);
#endif

#ifdef HAVE_TFTP
      if (option_bool(OPT_TFTP))
    {
      DIR *dir;
      struct tftp_prefix *p;
      
      if (daemon->tftp_prefix)
	{
	  if (!((dir = opendir(daemon->tftp_prefix))))
	    {
	      send_event(err_pipe[1], EVENT_TFTP_ERR, errno, daemon->tftp_prefix);
	      _exit(0);
	    }
	  closedir(dir);
	}

      for (p = daemon->if_prefix; p; p = p->next)
	{
	  if (!((dir = opendir(p->prefix))))
	   {
	     send_event(err_pipe[1], EVENT_TFTP_ERR, errno, p->prefix);
	     _exit(0);
	   } 
	  closedir(dir);
	}
    }
#endif

  if (daemon->port == 0)
    my_syslog(LOG_INFO, _("started, version %s DNS disabled"), VERSION);
  else if (daemon->cachesize != 0)
    my_syslog(LOG_INFO, _("started, version %s cachesize %d"), VERSION, daemon->cachesize);
  else
    my_syslog(LOG_INFO, _("started, version %s cache disabled"), VERSION);
  
  my_syslog(LOG_DEBUG, _("compile time options: %s"), compile_opts);
  
#ifdef HAVE_DBUS
  if (option_bool(OPT_DBUS))
    {
      if (daemon->dbus)
	my_syslog(LOG_INFO, _("DBus support enabled: connected to system bus"));
      else
	my_syslog(LOG_INFO, _("DBus support enabled: bus connection pending"));
    }
#endif

  if (option_bool(OPT_LOCAL_SERVICE))
    my_syslog(LOG_INFO, _("DNS service limited to local subnets"));
  
#ifdef HAVE_DNSSEC
  if (option_bool(OPT_DNSSEC_VALID))
    {
      my_syslog(LOG_INFO, _("DNSSEC validation enabled"));
      if (option_bool(OPT_DNSSEC_TIME))
	my_syslog(LOG_INFO, _("DNSSEC signature timestamps not checked until first cache reload"));
    }
#endif

  if (log_err != 0)
    my_syslog(LOG_WARNING, _("warning: failed to change owner of %s: %s"), 
	      daemon->log_file, strerror(log_err));
  
  if (bind_fallback)
    my_syslog(LOG_WARNING, _("setting --bind-interfaces option because of OS limitations"));

  if (option_bool(OPT_NOWILD))
    warn_bound_listeners();

  warn_int_names();
  
  if (!option_bool(OPT_NOWILD)) 
    for (if_tmp = daemon->if_names; if_tmp; if_tmp = if_tmp->next)
      if (if_tmp->name && !if_tmp->used)
	my_syslog(LOG_WARNING, _("warning: interface %s does not currently exist"), if_tmp->name);
   
  if (daemon->port != 0 && option_bool(OPT_NO_RESOLV))
    {
      if (daemon->resolv_files && !daemon->resolv_files->is_default)
	my_syslog(LOG_WARNING, _("warning: ignoring resolv-file flag because no-resolv is set"));
      daemon->resolv_files = NULL;
      if (!daemon->servers)
	my_syslog(LOG_WARNING, _("warning: no upstream servers configured"));
    } 

  if (daemon->max_logs != 0)
    my_syslog(LOG_INFO, _("asynchronous logging enabled, queue limit is %d messages"), daemon->max_logs);
  

#ifdef HAVE_DHCP
  for (context = daemon->dhcp; context; context = context->next)
    log_context(AF_INET, context);

  for (relay = daemon->relay4; relay; relay = relay->next)
    log_relay(AF_INET, relay);

#  ifdef HAVE_DHCP6
  for (context = daemon->dhcp6; context; context = context->next)
    log_context(AF_INET6, context);

  for (relay = daemon->relay6; relay; relay = relay->next)
    log_relay(AF_INET6, relay);
  
  if (daemon->doing_dhcp6 || daemon->doing_ra)
    dhcp_construct_contexts(now);
  
  if (option_bool(OPT_RA))
    my_syslog(MS_DHCP | LOG_INFO, _("IPv6 router advertisement enabled"));
#  endif

#  ifdef HAVE_LINUX_NETWORK
  if (did_bind)
    my_syslog(MS_DHCP | LOG_INFO, _("DHCP, sockets bound exclusively to interface %s"), bound_device);
#  endif

  /* after dhcp_contruct_contexts */
  if (daemon->dhcp || daemon->doing_dhcp6)
    lease_find_interfaces(now);
#endif

#ifdef HAVE_TFTP
  if (option_bool(OPT_TFTP))
    {
#ifdef FD_SETSIZE
      if (FD_SETSIZE < (unsigned)max_fd)
	max_fd = FD_SETSIZE;
#endif

      my_syslog(MS_TFTP | LOG_INFO, "TFTP %s%s %s", 
		daemon->tftp_prefix ? _("root is ") : _("enabled"),
		daemon->tftp_prefix ? daemon->tftp_prefix: "",
		option_bool(OPT_TFTP_SECURE) ? _("secure mode") : "");
      
      /* This is a guess, it assumes that for small limits, 
	 disjoint files might be served, but for large limits, 
	 a single file will be sent to may clients (the file only needs
	 one fd). */

      max_fd -= 30; /* use other than TFTP */
      
      if (max_fd < 0)
	max_fd = 5;
      else if (max_fd < 100)
	max_fd = max_fd/2;
      else
	max_fd = max_fd - 20;
      
      /* if we have to use a limited range of ports, 
	 that will limit the number of transfers */
      if (daemon->start_tftp_port != 0 &&
	  daemon->end_tftp_port - daemon->start_tftp_port + 1 < max_fd)
	max_fd = daemon->end_tftp_port - daemon->start_tftp_port + 1;

      if (daemon->tftp_max > max_fd)
	{
	  daemon->tftp_max = max_fd;
	  my_syslog(MS_TFTP | LOG_WARNING, 
		    _("restricting maximum simultaneous TFTP transfers to %d"), 
		    daemon->tftp_max);
	}
    }
#endif

  /* finished start-up - release original process */
  if (err_pipe[1] != -1)
    close(err_pipe[1]);
  
  if (daemon->port != 0)
    check_servers();
  
  pid = getpid();
  
#ifdef HAVE_INOTIFY
  /* Using inotify, have to select a resolv file at startup */
  poll_resolv(1, 0, now);
#endif
  
  while (1)
    {
      int maxfd = -1;
      struct timeval t, *tp = NULL;
      fd_set rset, wset, eset;
      
      FD_ZERO(&rset);
      FD_ZERO(&wset);
      FD_ZERO(&eset);
      
      /* if we are out of resources, find how long we have to wait
	 for some to come free, we'll loop around then and restart
	 listening for queries */
      if ((t.tv_sec = set_dns_listeners(now, &rset, &maxfd)) != 0)
	{
	  t.tv_usec = 0;
	  tp = &t;
	}

      /* Whilst polling for the dbus, or doing a tftp transfer, wake every quarter second */
      if (daemon->tftp_trans ||
	  (option_bool(OPT_DBUS) && !daemon->dbus))
	{
	  t.tv_sec = 0;
	  t.tv_usec = 250000;
	  tp = &t;
	}
      /* Wake every second whilst waiting for DAD to complete */
      else if (is_dad_listeners())
	{
	  t.tv_sec = 1;
	  t.tv_usec = 0;
	  tp = &t;
	}

#ifdef HAVE_DBUS
      set_dbus_listeners(&maxfd, &rset, &wset, &eset);
#endif	
  
#ifdef HAVE_DHCP
      if (daemon->dhcp || daemon->relay4)
	{
	  FD_SET(daemon->dhcpfd, &rset);
	  bump_maxfd(daemon->dhcpfd, &maxfd);
	  if (daemon->pxefd != -1)
	    {
	      FD_SET(daemon->pxefd, &rset);
	      bump_maxfd(daemon->pxefd, &maxfd);
	    }
	}
#endif

#ifdef HAVE_DHCP6
      if (daemon->doing_dhcp6 || daemon->relay6)
	{
	  FD_SET(daemon->dhcp6fd, &rset);
	  bump_maxfd(daemon->dhcp6fd, &maxfd);
	}

      if (daemon->doing_ra)
	{
	  FD_SET(daemon->icmp6fd, &rset);
	  bump_maxfd(daemon->icmp6fd, &maxfd); 
	}
#endif
    
#ifdef HAVE_INOTIFY
      if (daemon->inotifyfd != -1)
	{
	  FD_SET(daemon->inotifyfd, &rset);
	  bump_maxfd(daemon->inotifyfd, &maxfd);
	}
#endif

#if defined(HAVE_LINUX_NETWORK)
      FD_SET(daemon->netlinkfd, &rset);
      bump_maxfd(daemon->netlinkfd, &maxfd);
#elif defined(HAVE_BSD_NETWORK)
      FD_SET(daemon->routefd, &rset);
      bump_maxfd(daemon->routefd, &maxfd);
#endif
      
      FD_SET(piperead, &rset);
      bump_maxfd(piperead, &maxfd);

#ifdef HAVE_DHCP
#  ifdef HAVE_SCRIPT
      while (helper_buf_empty() && do_script_run(now));

#    ifdef HAVE_TFTP
      while (helper_buf_empty() && do_tftp_script_run());
#    endif

      if (!helper_buf_empty())
	{
	  FD_SET(daemon->helperfd, &wset);
	  bump_maxfd(daemon->helperfd, &maxfd);
	}
#  else
      /* need this for other side-effects */
      while (do_script_run(now));

#    ifdef HAVE_TFTP 
      while (do_tftp_script_run());
#    endif

#  endif
#endif
   
      /* must do this just before select(), when we know no
	 more calls to my_syslog() can occur */
      set_log_writer(&wset, &maxfd);
      
      if (select(maxfd+1, &rset, &wset, &eset, tp) < 0)
	{
	  /* otherwise undefined after error */
	  FD_ZERO(&rset); FD_ZERO(&wset); FD_ZERO(&eset);
	}

      now = dnsmasq_time();

      check_log_writer(&wset);

      /* prime. */
      enumerate_interfaces(1);

      /* Check the interfaces to see if any have exited DAD state
	 and if so, bind the address. */
      if (is_dad_listeners())
	{
	  enumerate_interfaces(0);
	  /* NB, is_dad_listeners() == 1 --> we're binding interfaces */
	  create_bound_listeners(0);
	  warn_bound_listeners();
	}

#if defined(HAVE_LINUX_NETWORK)
      if (FD_ISSET(daemon->netlinkfd, &rset))
	netlink_multicast();
#elif defined(HAVE_BSD_NETWORK)
      if (FD_ISSET(daemon->routefd, &rset))
	route_sock();
#endif

#ifdef HAVE_INOTIFY
      if  (daemon->inotifyfd != -1 && FD_ISSET(daemon->inotifyfd, &rset) && inotify_check(now))
	{
	  if (daemon->port != 0 && !option_bool(OPT_NO_POLL))
	    poll_resolv(1, 1, now);
	} 	  
#else
      /* Check for changes to resolv files once per second max. */
      /* Don't go silent for long periods if the clock goes backwards. */
      if (daemon->last_resolv == 0 || 
	  difftime(now, daemon->last_resolv) > 1.0 || 
	  difftime(now, daemon->last_resolv) < -1.0)
	{
	  /* poll_resolv doesn't need to reload first time through, since 
	     that's queued anyway. */

	  poll_resolv(0, daemon->last_resolv != 0, now); 	  
	  daemon->last_resolv = now;
	}
#endif

      if (FD_ISSET(piperead, &rset))
	async_event(piperead, now);
      
#ifdef HAVE_DBUS
      /* if we didn't create a DBus connection, retry now. */ 
     if (option_bool(OPT_DBUS) && !daemon->dbus)
	{
	  char *err;
	  if ((err = dbus_init()))
	    my_syslog(LOG_WARNING, _("DBus error: %s"), err);
	  if (daemon->dbus)
	    my_syslog(LOG_INFO, _("connected to system DBus"));
	}
      check_dbus_listeners(&rset, &wset, &eset);
#endif
      
      check_dns_listeners(&rset, now);

#ifdef HAVE_TFTP
      check_tftp_listeners(&rset, now);
#endif      

#ifdef HAVE_DHCP
      if (daemon->dhcp || daemon->relay4)
	{
	  if (FD_ISSET(daemon->dhcpfd, &rset))
	    dhcp_packet(now, 0);
	  if (daemon->pxefd != -1 && FD_ISSET(daemon->pxefd, &rset))
	    dhcp_packet(now, 1);
	}

#ifdef HAVE_DHCP6
      if ((daemon->doing_dhcp6 || daemon->relay6) && FD_ISSET(daemon->dhcp6fd, &rset))
	dhcp6_packet(now);

      if (daemon->doing_ra && FD_ISSET(daemon->icmp6fd, &rset))
	icmp6_packet(now);
#endif

#  ifdef HAVE_SCRIPT
      if (daemon->helperfd != -1 && FD_ISSET(daemon->helperfd, &wset))
	helper_write();
#  endif
#endif

    }
}

static void sig_handler(int sig)
{
  if (pid == 0)
    {
      /* ignore anything other than TERM during startup
	 and in helper proc. (helper ignore TERM too) */
      if (sig == SIGTERM)
	exit(EC_MISC);
    }
  else if (pid != getpid())
    {
      /* alarm is used to kill TCP children after a fixed time. */
      if (sig == SIGALRM)
	_exit(0);
    }
  else
    {
      /* master process */
      int event, errsave = errno;
      
      if (sig == SIGHUP)
	event = EVENT_RELOAD;
      else if (sig == SIGCHLD)
	event = EVENT_CHILD;
      else if (sig == SIGALRM)
	event = EVENT_ALARM;
      else if (sig == SIGTERM)
	event = EVENT_TERM;
      else if (sig == SIGUSR1)
	event = EVENT_DUMP;
      else if (sig == SIGUSR2)
	event = EVENT_REOPEN;
      else
	return;

      send_event(pipewrite, event, 0, NULL); 
      errno = errsave;
    }
}

/* now == 0 -> queue immediate callback */
void send_alarm(time_t event, time_t now)
{
  if (now == 0 || event != 0)
    {
      /* alarm(0) or alarm(-ve) doesn't do what we want.... */
      if ((now == 0 || difftime(event, now) <= 0.0))
	send_event(pipewrite, EVENT_ALARM, 0, NULL);
      else 
	alarm((unsigned)difftime(event, now)); 
    }
}

void queue_event(int event)
{
  send_event(pipewrite, event, 0, NULL);
}

void send_event(int fd, int event, int data, char *msg)
{
  struct event_desc ev;
  struct iovec iov[2];

  ev.event = event;
  ev.data = data;
  ev.msg_sz = msg ? strlen(msg) : 0;
  
  iov[0].iov_base = &ev;
  iov[0].iov_len = sizeof(ev);
  iov[1].iov_base = msg;
  iov[1].iov_len = ev.msg_sz;
  
  /* error pipe, debug mode. */
  if (fd == -1)
    fatal_event(&ev, msg);
  else
    /* pipe is non-blocking and struct event_desc is smaller than
       PIPE_BUF, so this either fails or writes everything */
    while (writev(fd, iov, msg ? 2 : 1) == -1 && errno == EINTR);
}

/* NOTE: the memory used to return msg is leaked: use msgs in events only
   to describe fatal errors. */
static int read_event(int fd, struct event_desc *evp, char **msg)
{
  char *buf;

  if (!read_write(fd, (unsigned char *)evp, sizeof(struct event_desc), 1))
    return 0;
  
  *msg = NULL;
  
  if (evp->msg_sz != 0 && 
      (buf = malloc(evp->msg_sz + 1)) &&
      read_write(fd, (unsigned char *)buf, evp->msg_sz, 1))
    {
      buf[evp->msg_sz] = 0;
      *msg = buf;
    }

  return 1;
}
    
static void fatal_event(struct event_desc *ev, char *msg)
{
  errno = ev->data;
  
  switch (ev->event)
    {
    case EVENT_DIE:
      exit(0);

    case EVENT_FORK_ERR:
      die(_("cannot fork into background: %s"), NULL, EC_MISC);
  
    case EVENT_PIPE_ERR:
      die(_("failed to create helper: %s"), NULL, EC_MISC);
  
    case EVENT_CAP_ERR:
      die(_("setting capabilities failed: %s"), NULL, EC_MISC);

    case EVENT_USER_ERR:
      die(_("failed to change user-id to %s: %s"), msg, EC_MISC);

    case EVENT_GROUP_ERR:
      die(_("failed to change group-id to %s: %s"), msg, EC_MISC);
      
    case EVENT_PIDFILE:
      die(_("failed to open pidfile %s: %s"), msg, EC_FILE);

    case EVENT_LOG_ERR:
      die(_("cannot open log %s: %s"), msg, EC_FILE);
    
    case EVENT_LUA_ERR:
      die(_("failed to load Lua script: %s"), msg, EC_MISC);

    case EVENT_TFTP_ERR:
      die(_("TFTP directory %s inaccessible: %s"), msg, EC_FILE);
    }
}	
      
static void async_event(int pipe, time_t now)
{
  pid_t p;
  struct event_desc ev;
  int i, check = 0;
  char *msg;
  
  /* NOTE: the memory used to return msg is leaked: use msgs in events only
     to describe fatal errors. */
  
  if (read_event(pipe, &ev, &msg))
    switch (ev.event)
      {
      case EVENT_RELOAD:
#ifdef HAVE_DNSSEC
	if (option_bool(OPT_DNSSEC_VALID) && option_bool(OPT_DNSSEC_TIME))
	  {
	    my_syslog(LOG_INFO, _("now checking DNSSEC signature timestamps"));
	    reset_option_bool(OPT_DNSSEC_TIME);
	  } 
#endif
	/* fall through */
	
      case EVENT_INIT:
	clear_cache_and_reload(now);
	
	if (daemon->port != 0)
	  {
	    if (daemon->resolv_files && option_bool(OPT_NO_POLL))
	      {
		reload_servers(daemon->resolv_files->name);
		check = 1;
	      }

	    if (daemon->servers_file)
	      {
		read_servers_file();
		check = 1;
	      }

	    if (check)
	      check_servers();
	  }

#ifdef HAVE_DHCP
	rerun_scripts();
#endif
	break;
	
      case EVENT_DUMP:
	if (daemon->port != 0)
	  dump_cache(now);
	break;
	
      case EVENT_ALARM:
#ifdef HAVE_DHCP
	if (daemon->dhcp || daemon->doing_dhcp6)
	  {
	    lease_prune(NULL, now);
	    lease_update_file(now);
	  }
#ifdef HAVE_DHCP6
	else if (daemon->doing_ra)
	  /* Not doing DHCP, so no lease system, manage alarms for ra only */
	    send_alarm(periodic_ra(now), now);
#endif
#endif
	break;
		
      case EVENT_CHILD:
	/* See Stevens 5.10 */
	while ((p = waitpid(-1, NULL, WNOHANG)) != 0)
	  if (p == -1)
	    {
	      if (errno != EINTR)
		break;
	    }      
	  else 
	    for (i = 0 ; i < MAX_PROCS; i++)
	      if (daemon->tcp_pids[i] == p)
		daemon->tcp_pids[i] = 0;
	break;
	
      case EVENT_KILLED:
	my_syslog(LOG_WARNING, _("script process killed by signal %d"), ev.data);
	break;

      case EVENT_EXITED:
	my_syslog(LOG_WARNING, _("script process exited with status %d"), ev.data);
	break;

      case EVENT_EXEC_ERR:
	my_syslog(LOG_ERR, _("failed to execute %s: %s"), 
		  daemon->lease_change_command, strerror(ev.data));
	break;

	/* necessary for fatal errors in helper */
      case EVENT_USER_ERR:
      case EVENT_DIE:
      case EVENT_LUA_ERR:
	fatal_event(&ev, msg);
	break;

      case EVENT_REOPEN:
	/* Note: this may leave TCP-handling processes with the old file still open.
	   Since any such process will die in CHILD_LIFETIME or probably much sooner,
	   we leave them logging to the old file. */
	if (daemon->log_file != NULL)
	  log_reopen(daemon->log_file);
#if defined(HAVE_DHCP) && defined(HAVE_LEASEFILE_EXPIRE)
	if (daemon->dhcp || daemon->dhcp6)
	  lease_flush_file(now);
#endif
	break;

      case EVENT_NEWADDR:
	newaddress(now);
	break;

      case EVENT_NEWROUTE:
	resend_query();
	/* Force re-reading resolv file right now, for luck. */
	poll_resolv(0, 1, now);
	break;

      case EVENT_TERM:
	/* Knock all our children on the head. */
	for (i = 0; i < MAX_PROCS; i++)
	  if (daemon->tcp_pids[i] != 0)
	    kill(daemon->tcp_pids[i], SIGALRM);
	
#if defined(HAVE_SCRIPT)
	/* handle pending lease transitions */
	if (daemon->helperfd != -1)
	  {
	    /* block in writes until all done */
	    if ((i = fcntl(daemon->helperfd, F_GETFL)) != -1)
	      fcntl(daemon->helperfd, F_SETFL, i & ~O_NONBLOCK); 
	    do {
	      helper_write();
	    } while (!helper_buf_empty() || do_script_run(now));
	    close(daemon->helperfd);
	  }
#endif
	
#if defined(HAVE_DHCP) && defined(HAVE_LEASEFILE_EXPIRE)
	if (daemon->dhcp || daemon->dhcp6)
	  lease_flush_file(now);
#endif
	if (daemon->lease_stream)
	  fclose(daemon->lease_stream);

	if (daemon->runfile)
	  unlink(daemon->runfile);
	
	my_syslog(LOG_INFO, _("exiting on receipt of SIGTERM"));
	flush_log();
	exit(EC_GOOD);
      }
}

static void poll_resolv(int force, int do_reload, time_t now)
{
  struct resolvc *res, *latest;
  struct stat statbuf;
  time_t last_change = 0;
  /* There may be more than one possible file. 
     Go through and find the one which changed _last_.
     Warn of any which can't be read. */

  if (daemon->port == 0 || option_bool(OPT_NO_POLL))
    return;
  
  for (latest = NULL, res = daemon->resolv_files; res; res = res->next)
    if (stat(res->name, &statbuf) == -1)
      {
	if (force)
	  {
	    res->mtime = 0; 
	    continue;
	  }

	if (!res->logged)
	  my_syslog(LOG_WARNING, _("failed to access %s: %s"), res->name, strerror(errno));
	res->logged = 1;
	
	if (res->mtime != 0)
	  { 
	    /* existing file evaporated, force selection of the latest
	       file even if its mtime hasn't changed since we last looked */
	    poll_resolv(1, do_reload, now);
	    return;
	  }
      }
    else
      {
	res->logged = 0;
	if (force || (statbuf.st_mtime != res->mtime))
          {
            res->mtime = statbuf.st_mtime;
	    if (difftime(statbuf.st_mtime, last_change) > 0.0)
	      {
		last_change = statbuf.st_mtime;
		latest = res;
	      }
	  }
      }
  
  if (latest)
    {
      static int warned = 0;
      if (reload_servers(latest->name))
	{
	  my_syslog(LOG_INFO, _("reading %s"), latest->name);
	  warned = 0;
	  check_servers();
	  if (option_bool(OPT_RELOAD) && do_reload)
	    clear_cache_and_reload(now);
	}
      else 
	{
	  latest->mtime = 0;
	  if (!warned)
	    {
	      my_syslog(LOG_WARNING, _("no servers found in %s, will retry"), latest->name);
	      warned = 1;
	    }
	}
    }
}       

void clear_cache_and_reload(time_t now)
{
  (void)now;

  if (daemon->port != 0)
    cache_reload();
  
#ifdef HAVE_DHCP
  if (daemon->dhcp || daemon->doing_dhcp6)
    {
      if (option_bool(OPT_ETHERS))
	dhcp_read_ethers();
      reread_dhcp();
#ifdef HAVE_INOTIFY
      set_dhcp_inotify();
#endif
      dhcp_update_configs(daemon->dhcp_conf);
      lease_update_from_configs(); 
      lease_update_file(now); 
      lease_update_dns(1);
    }
#ifdef HAVE_DHCP6
  else if (daemon->doing_ra)
    /* Not doing DHCP, so no lease system, manage 
       alarms for ra only */
    send_alarm(periodic_ra(now), now);
#endif
#endif
}

static int set_dns_listeners(time_t now, fd_set *set, int *maxfdp)
{
  struct serverfd *serverfdp;
  struct listener *listener;
  int wait = 0, i;
  
#ifdef HAVE_TFTP
  int  tftp = 0;
  struct tftp_transfer *transfer;
  for (transfer = daemon->tftp_trans; transfer; transfer = transfer->next)
    {
      tftp++;
      FD_SET(transfer->sockfd, set);
      bump_maxfd(transfer->sockfd, maxfdp);
    }
#endif
  
  /* will we be able to get memory? */
  if (daemon->port != 0)
    get_new_frec(now, &wait, 0);
  
  for (serverfdp = daemon->sfds; serverfdp; serverfdp = serverfdp->next)
    {
      FD_SET(serverfdp->fd, set);
      bump_maxfd(serverfdp->fd, maxfdp);
    }

  if (daemon->port != 0 && !daemon->osport)
    for (i = 0; i < RANDOM_SOCKS; i++)
      if (daemon->randomsocks[i].refcount != 0)
	{
	  FD_SET(daemon->randomsocks[i].fd, set);
	  bump_maxfd(daemon->randomsocks[i].fd, maxfdp);
	}
  
  for (listener = daemon->listeners; listener; listener = listener->next)
    {
      /* only listen for queries if we have resources */
      if (listener->fd != -1 && wait == 0)
	{
	  FD_SET(listener->fd, set);
	  bump_maxfd(listener->fd, maxfdp);
	}

      /* death of a child goes through the select loop, so
	 we don't need to explicitly arrange to wake up here */
      if  (listener->tcpfd != -1)
	for (i = 0; i < MAX_PROCS; i++)
	  if (daemon->tcp_pids[i] == 0)
	    {
	      FD_SET(listener->tcpfd, set);
	      bump_maxfd(listener->tcpfd, maxfdp);
	      break;
	    }

#ifdef HAVE_TFTP
      if (tftp <= daemon->tftp_max && listener->tftpfd != -1)
	{
	  FD_SET(listener->tftpfd, set);
	  bump_maxfd(listener->tftpfd, maxfdp);
	}
#endif

    }
  
  return wait;
}

static void check_dns_listeners(fd_set *set, time_t now)
{
  struct serverfd *serverfdp;
  struct listener *listener;
  int i;

  for (serverfdp = daemon->sfds; serverfdp; serverfdp = serverfdp->next)
    if (FD_ISSET(serverfdp->fd, set))
      reply_query(serverfdp->fd, serverfdp->source_addr.sa.sa_family, now);
  
  if (daemon->port != 0 && !daemon->osport)
    for (i = 0; i < RANDOM_SOCKS; i++)
      if (daemon->randomsocks[i].refcount != 0 && 
	  FD_ISSET(daemon->randomsocks[i].fd, set))
	reply_query(daemon->randomsocks[i].fd, daemon->randomsocks[i].family, now);
  
  for (listener = daemon->listeners; listener; listener = listener->next)
    {
      if (listener->fd != -1 && FD_ISSET(listener->fd, set))
	receive_query(listener, now); 
      
#ifdef HAVE_TFTP     
      if (listener->tftpfd != -1 && FD_ISSET(listener->tftpfd, set))
	tftp_request(listener, now);
#endif

      if (listener->tcpfd != -1 && FD_ISSET(listener->tcpfd, set))
	{
	  int confd, client_ok = 1;
	  struct irec *iface = NULL;
	  pid_t p;
	  union mysockaddr tcp_addr;
	  socklen_t tcp_len = sizeof(union mysockaddr);

	  while ((confd = accept(listener->tcpfd, NULL, NULL)) == -1 && errno == EINTR);
	  
	  if (confd == -1)
	    continue;
	  
	  if (getsockname(confd, (struct sockaddr *)&tcp_addr, &tcp_len) == -1)
	    {
	      close(confd);
	      continue;
	    }
	  
	  /* Make sure that the interface list is up-to-date.
	     
	     We do this here as we may need the results below, and
	     the DNS code needs them for --interface-name stuff.

	     Multiple calls to enumerate_interfaces() per select loop are
	     inhibited, so calls to it in the child process (which doesn't select())
	     have no effect. This avoids two processes reading from the same
	     netlink fd and screwing the pooch entirely.
	  */
 
	  enumerate_interfaces(0);
	  
	  if (option_bool(OPT_NOWILD))
	    iface = listener->iface; /* May be NULL */
	  else 
	    {
	      int if_index;
	      char intr_name[IF_NAMESIZE];
	      
	      /* if we can find the arrival interface, check it's one that's allowed */
	      if ((if_index = tcp_interface(confd, tcp_addr.sa.sa_family)) != 0 &&
		  indextoname(listener->tcpfd, if_index, intr_name))
		{
		  struct all_addr addr;
		  addr.addr.addr4 = tcp_addr.in.sin_addr;
#ifdef HAVE_IPV6
		  if (tcp_addr.sa.sa_family == AF_INET6)
		    addr.addr.addr6 = tcp_addr.in6.sin6_addr;
#endif
		  
		  for (iface = daemon->interfaces; iface; iface = iface->next)
		    if (iface->index == if_index)
		      break;
		  
		  if (!iface && !loopback_exception(listener->tcpfd, tcp_addr.sa.sa_family, &addr, intr_name))
		    client_ok = 0;
		}
	      
	      if (option_bool(OPT_CLEVERBIND))
		iface = listener->iface; /* May be NULL */
	      else
		{
		  /* Check for allowed interfaces when binding the wildcard address:
		     we do this by looking for an interface with the same address as 
		     the local address of the TCP connection, then looking to see if that's
		     an allowed interface. As a side effect, we get the netmask of the
		     interface too, for localisation. */
		  
		  for (iface = daemon->interfaces; iface; iface = iface->next)
		    if (sockaddr_isequal(&iface->addr, &tcp_addr))
		      break;
		  
		  if (!iface)
		    client_ok = 0;
		}
	    }
	  
	  if (!client_ok)
	    {
	      shutdown(confd, SHUT_RDWR);
	      close(confd);
	    }
#ifndef NO_FORK
	  else if (!option_bool(OPT_DEBUG) && (p = fork()) != 0)
	    {
	      if (p != -1)
		{
		  int i;
		  for (i = 0; i < MAX_PROCS; i++)
		    if (daemon->tcp_pids[i] == 0)
		      {
			daemon->tcp_pids[i] = p;
			break;
		      }
		}
	      close(confd);

	      /* The child can use up to TCP_MAX_QUERIES ids, so skip that many. */
	      daemon->log_id += TCP_MAX_QUERIES;
	    }
#endif
	  else
	    {
	      unsigned char *buff;
	      struct server *s; 
	      int flags;
	      struct in_addr netmask;
	      int auth_dns;

	      if (iface)
		{
		  netmask = iface->netmask;
		  auth_dns = iface->dns_auth;
		}
	      else
		{
		  netmask.s_addr = 0;
		  auth_dns = 0;
		}

#ifndef NO_FORK
	      /* Arrange for SIGALARM after CHILD_LIFETIME seconds to
		 terminate the process. */
	      if (!option_bool(OPT_DEBUG))
		alarm(CHILD_LIFETIME);
#endif

	      /* start with no upstream connections. */
	      for (s = daemon->servers; s; s = s->next)
		 s->tcpfd = -1; 
	      
	      /* The connected socket inherits non-blocking
		 attribute from the listening socket. 
		 Reset that here. */
	      if ((flags = fcntl(confd, F_GETFL, 0)) != -1)
		fcntl(confd, F_SETFL, flags & ~O_NONBLOCK);
	      
	      buff = tcp_request(confd, now, &tcp_addr, netmask, auth_dns);
	       
	      shutdown(confd, SHUT_RDWR);
	      close(confd);
	      
	      if (buff)
		free(buff);
	      
	      for (s = daemon->servers; s; s = s->next)
		if (s->tcpfd != -1)
		  {
		    shutdown(s->tcpfd, SHUT_RDWR);
		    close(s->tcpfd);
		  }
#ifndef NO_FORK		   
	      if (!option_bool(OPT_DEBUG))
		{
		  flush_log();
		  _exit(0);
		}
#endif
	    }
	}
    }
}

#ifdef HAVE_DHCP
int make_icmp_sock(void)
{
  int fd;
  int zeroopt = 0;

  if ((fd = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP)) != -1)
    {
      if (!fix_fd(fd) ||
	  setsockopt(fd, SOL_SOCKET, SO_DONTROUTE, &zeroopt, sizeof(zeroopt)) == -1)
	{
	  close(fd);
	  fd = -1;
	}
    }

  return fd;
}

int icmp_ping(struct in_addr addr)
{
  /* Try and get an ICMP echo from a machine. */

  /* Note that whilst in the three second wait, we check for 
     (and service) events on the DNS and TFTP  sockets, (so doing that
     better not use any resources our caller has in use...)
     but we remain deaf to signals or further DHCP packets. */

  int fd;
  struct sockaddr_in saddr;
  struct { 
    struct ip ip;
    struct icmp icmp;
  } packet;
  unsigned short id = rand16();
  unsigned int i, j;
  int gotreply = 0;
  time_t start, now;

#if defined(HAVE_LINUX_NETWORK) || defined (HAVE_SOLARIS_NETWORK)
  if ((fd = make_icmp_sock()) == -1)
    return 0;
#else
  int opt = 2000;
  fd = daemon->dhcp_icmp_fd;
  setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));
#endif

  saddr.sin_family = AF_INET;
  saddr.sin_port = 0;
  saddr.sin_addr = addr;
#ifdef HAVE_SOCKADDR_SA_LEN
  saddr.sin_len = sizeof(struct sockaddr_in);
#endif
  
  memset(&packet.icmp, 0, sizeof(packet.icmp));
  packet.icmp.icmp_type = ICMP_ECHO;
  packet.icmp.icmp_id = id;
  for (j = 0, i = 0; i < sizeof(struct icmp) / 2; i++)
    j += ((u16 *)&packet.icmp)[i];
  while (j>>16)
    j = (j & 0xffff) + (j >> 16);  
  packet.icmp.icmp_cksum = (j == 0xffff) ? j : ~j;
  
  while (sendto(fd, (char *)&packet.icmp, sizeof(struct icmp), 0, 
		(struct sockaddr *)&saddr, sizeof(saddr)) == -1 &&
	 retry_send());
  
  for (now = start = dnsmasq_time(); 
       difftime(now, start) < (float)PING_WAIT;)
    {
      struct timeval tv;
      fd_set rset, wset;
      struct sockaddr_in faddr;
      int maxfd = fd; 
      socklen_t len = sizeof(faddr);
      
      tv.tv_usec = 250000;
      tv.tv_sec = 0; 
      
      FD_ZERO(&rset);
      FD_ZERO(&wset);
      FD_SET(fd, &rset);
      set_dns_listeners(now, &rset, &maxfd);
      set_log_writer(&wset, &maxfd);
      
#ifdef HAVE_DHCP6
      if (daemon->doing_ra)
	{
	  FD_SET(daemon->icmp6fd, &rset);
	  bump_maxfd(daemon->icmp6fd, &maxfd); 
	}
#endif
      
      if (select(maxfd+1, &rset, &wset, NULL, &tv) < 0)
	{
	  FD_ZERO(&rset);
	  FD_ZERO(&wset);
	}

      now = dnsmasq_time();

      check_log_writer(&wset);
      check_dns_listeners(&rset, now);

#ifdef HAVE_DHCP6
      if (daemon->doing_ra && FD_ISSET(daemon->icmp6fd, &rset))
	icmp6_packet(now);
#endif
      
#ifdef HAVE_TFTP
      check_tftp_listeners(&rset, now);
#endif

      if (FD_ISSET(fd, &rset) &&
	  recvfrom(fd, &packet, sizeof(packet), 0,
		   (struct sockaddr *)&faddr, &len) == sizeof(packet) &&
	  saddr.sin_addr.s_addr == faddr.sin_addr.s_addr &&
	  packet.icmp.icmp_type == ICMP_ECHOREPLY &&
	  packet.icmp.icmp_seq == 0 &&
	  packet.icmp.icmp_id == id)
	{
	  gotreply = 1;
	  break;
	}
    }
  
#if defined(HAVE_LINUX_NETWORK) || defined(HAVE_SOLARIS_NETWORK)
  close(fd);
#else
  opt = 1;
  setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));
#endif

  return gotreply;
}
#endif

 
