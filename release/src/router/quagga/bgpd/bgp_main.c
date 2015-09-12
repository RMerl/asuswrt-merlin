/* Main routine of bgpd.
   Copyright (C) 1996, 97, 98, 1999 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include <zebra.h>

#include "vector.h"
#include "vty.h"
#include "command.h"
#include "getopt.h"
#include "thread.h"
#include <lib/version.h>
#include "memory.h"
#include "prefix.h"
#include "log.h"
#include "privs.h"
#include "sigevent.h"
#include "zclient.h"
#include "routemap.h"
#include "filter.h"
#include "plist.h"
#include "stream.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_mplsvpn.h"
#include "bgpd/bgp_aspath.h"
#include "bgpd/bgp_dump.h"
#include "bgpd/bgp_route.h"
#include "bgpd/bgp_nexthop.h"
#include "bgpd/bgp_regex.h"
#include "bgpd/bgp_clist.h"
#include "bgpd/bgp_debug.h"
#include "bgpd/bgp_filter.h"
#include "bgpd/bgp_zebra.h"

/* bgpd options, we use GNU getopt library. */
static const struct option longopts[] = 
{
  { "daemon",      no_argument,       NULL, 'd'},
  { "config_file", required_argument, NULL, 'f'},
  { "pid_file",    required_argument, NULL, 'i'},
  { "socket",      required_argument, NULL, 'z'},
  { "bgp_port",    required_argument, NULL, 'p'},
  { "listenon",    required_argument, NULL, 'l'},
  { "vty_addr",    required_argument, NULL, 'A'},
  { "vty_port",    required_argument, NULL, 'P'},
  { "retain",      no_argument,       NULL, 'r'},
  { "no_kernel",   no_argument,       NULL, 'n'},
  { "user",        required_argument, NULL, 'u'},
  { "group",       required_argument, NULL, 'g'},
  { "version",     no_argument,       NULL, 'v'},
  { "dryrun",      no_argument,       NULL, 'C'},
  { "help",        no_argument,       NULL, 'h'},
  { 0 }
};

/* signal definitions */
void sighup (void);
void sigint (void);
void sigusr1 (void);

static void bgp_exit (int);

static struct quagga_signal_t bgp_signals[] = 
{
  { 
    .signal = SIGHUP, 
    .handler = &sighup,
  },
  {
    .signal = SIGUSR1,
    .handler = &sigusr1,
  },
  {
    .signal = SIGINT,
    .handler = &sigint,
  },
  {
    .signal = SIGTERM,
    .handler = &sigint,
  },
};

/* Configuration file and directory. */
char config_default[] = SYSCONFDIR BGP_DEFAULT_CONFIG;

/* Route retain mode flag. */
static int retain_mode = 0;

/* Master of threads. */
struct thread_master *master;

/* Manually specified configuration file name.  */
char *config_file = NULL;

/* Process ID saved for use by init system */
static const char *pid_file = PATH_BGPD_PID;

/* VTY port number and address.  */
int vty_port = BGP_VTY_PORT;
char *vty_addr = NULL;

/* privileges */
static zebra_capabilities_t _caps_p [] =  
{
    ZCAP_BIND, 
    ZCAP_NET_RAW,
    ZCAP_NET_ADMIN,
};

struct zebra_privs_t bgpd_privs =
{
#if defined(QUAGGA_USER) && defined(QUAGGA_GROUP)
  .user = QUAGGA_USER,
  .group = QUAGGA_GROUP,
#endif
#ifdef VTY_GROUP
  .vty_group = VTY_GROUP,
#endif
  .caps_p = _caps_p,
  .cap_num_p = array_size(_caps_p),
  .cap_num_i = 0,
};

/* Help information display. */
static void
usage (char *progname, int status)
{
  if (status != 0)
    fprintf (stderr, "Try `%s --help' for more information.\n", progname);
  else
    {    
      printf ("Usage : %s [OPTION...]\n\n\
Daemon which manages kernel routing table management and \
redistribution between different routing protocols.\n\n\
-d, --daemon       Runs in daemon mode\n\
-f, --config_file  Set configuration file name\n\
-i, --pid_file     Set process identifier file name\n\
-z, --socket       Set path of zebra socket\n\
-p, --bgp_port     Set bgp protocol's port number\n\
-l, --listenon     Listen on specified address (implies -n)\n\
-A, --vty_addr     Set vty's bind address\n\
-P, --vty_port     Set vty's port number\n\
-r, --retain       When program terminates, retain added route by bgpd.\n\
-n, --no_kernel    Do not install route to kernel.\n\
-u, --user         User to run as\n\
-g, --group        Group to run as\n\
-v, --version      Print program version\n\
-C, --dryrun       Check configuration for validity and exit\n\
-h, --help         Display this help and exit\n\
\n\
Report bugs to %s\n", progname, ZEBRA_BUG_ADDRESS);
    }

  exit (status);
}

/* SIGHUP handler. */
void 
sighup (void)
{
  zlog (NULL, LOG_INFO, "SIGHUP received");

  /* Terminate all thread. */
  bgp_terminate ();
  bgp_reset ();
  zlog_info ("bgpd restarting!");

  /* Reload config file. */
  vty_read_config (config_file, config_default);

  /* Create VTY's socket */
  vty_serv_sock (vty_addr, vty_port, BGP_VTYSH_PATH);

  /* Try to return to normal operation. */
}

/* SIGINT handler. */
void
sigint (void)
{
  zlog_notice ("Terminating on signal");

  if (! retain_mode)
    bgp_terminate ();

  zprivs_terminate (&bgpd_privs);
  bgp_exit (0);
}

/* SIGUSR1 handler. */
void
sigusr1 (void)
{
  zlog_rotate (NULL);
}

/*
  Try to free up allocations we know about so that diagnostic tools such as
  valgrind are able to better illuminate leaks.

  Zebra route removal and protocol teardown are not meant to be done here.
  For example, "retain_mode" may be set.
*/
static void
bgp_exit (int status)
{
  struct bgp *bgp;
  struct listnode *node, *nnode;
  int *socket;
  struct interface *ifp;
  extern struct zclient *zclient;
  extern struct zclient *zlookup;

  /* it only makes sense for this to be called on a clean exit */
  assert (status == 0);

  /* reverse bgp_master_init */
  for (ALL_LIST_ELEMENTS (bm->bgp, node, nnode, bgp))
    bgp_delete (bgp);
  list_free (bm->bgp);

  /* reverse bgp_master_init */
  for (ALL_LIST_ELEMENTS_RO(bm->listen_sockets, node, socket))
    {
      if (close ((int)(long)socket) == -1)
        zlog_err ("close (%d): %s", (int)(long)socket, safe_strerror (errno));
    }
  list_delete (bm->listen_sockets);

  /* reverse bgp_zebra_init/if_init */
  if (retain_mode)
    if_add_hook (IF_DELETE_HOOK, NULL);
  for (ALL_LIST_ELEMENTS (iflist, node, nnode, ifp))
    {
      struct listnode *c_node, *c_nnode;
      struct connected *c;

      for (ALL_LIST_ELEMENTS (ifp->connected, c_node, c_nnode, c))
        bgp_connected_delete (c);

      if_delete (ifp);
    }
  list_free (iflist);

  /* reverse bgp_attr_init */
  bgp_attr_finish ();

  /* reverse bgp_dump_init */
  bgp_dump_finish ();

  /* reverse bgp_route_init */
  bgp_route_finish ();

  /* reverse bgp_route_map_init/route_map_init */
  route_map_finish ();

  /* reverse bgp_scan_init */
  bgp_scan_finish ();

  /* reverse access_list_init */
  access_list_add_hook (NULL);
  access_list_delete_hook (NULL);
  access_list_reset ();

  /* reverse bgp_filter_init */
  as_list_add_hook (NULL);
  as_list_delete_hook (NULL);
  bgp_filter_reset ();

  /* reverse prefix_list_init */
  prefix_list_add_hook (NULL);
  prefix_list_delete_hook (NULL);
  prefix_list_reset ();

  /* reverse community_list_init */
  community_list_terminate (bgp_clist);

  cmd_terminate ();
  vty_terminate ();
  if (zclient)
    zclient_free (zclient);
  if (zlookup)
    zclient_free (zlookup);
  if (bgp_nexthop_buf)
    stream_free (bgp_nexthop_buf);

  /* reverse bgp_master_init */
  if (master)
    thread_master_free (master);

  if (zlog_default)
    closezlog (zlog_default);

  if (CONF_BGP_DEBUG (normal, NORMAL))
    log_memstats_stderr ("bgpd");

  exit (status);
}

/* Main routine of bgpd. Treatment of argument and start bgp finite
   state machine is handled at here. */
int
main (int argc, char **argv)
{
  char *p;
  int opt;
  int daemon_mode = 0;
  int dryrun = 0;
  char *progname;
  struct thread thread;
  int tmp_port;

  /* Set umask before anything for security */
  umask (0027);

  /* Preserve name of myself. */
  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

  zlog_default = openzlog (progname, ZLOG_BGP,
			   LOG_CONS|LOG_NDELAY|LOG_PID, LOG_DAEMON);

  /* BGP master init. */
  bgp_master_init ();

  /* Command line argument treatment. */
  while (1) 
    {
      opt = getopt_long (argc, argv, "df:i:z:hp:l:A:P:rnu:g:vC", longopts, 0);
    
      if (opt == EOF)
	break;

      switch (opt) 
	{
	case 0:
	  break;
	case 'd':
	  daemon_mode = 1;
	  break;
	case 'f':
	  config_file = optarg;
	  break;
        case 'i':
          pid_file = optarg;
          break;
	case 'z':
	  zclient_serv_path_set (optarg);
	  break;
	case 'p':
	  tmp_port = atoi (optarg);
	  if (tmp_port <= 0 || tmp_port > 0xffff)
	    bm->port = BGP_PORT_DEFAULT;
	  else
	    bm->port = tmp_port;
	  break;
	case 'A':
	  vty_addr = optarg;
	  break;
	case 'P':
          /* Deal with atoi() returning 0 on failure, and bgpd not
             listening on bgp port... */
          if (strcmp(optarg, "0") == 0) 
            {
              vty_port = 0;
              break;
            } 
          vty_port = atoi (optarg);
	  if (vty_port <= 0 || vty_port > 0xffff)
	    vty_port = BGP_VTY_PORT;
	  break;
	case 'r':
	  retain_mode = 1;
	  break;
	case 'l':
	  bm->address = optarg;
	  /* listenon implies -n */
	case 'n':
	  bgp_option_set (BGP_OPT_NO_FIB);
	  break;
	case 'u':
	  bgpd_privs.user = optarg;
	  break;
	case 'g':
	  bgpd_privs.group = optarg;
	  break;
	case 'v':
	  print_version (progname);
	  exit (0);
	  break;
	case 'C':
	  dryrun = 1;
	  break;
	case 'h':
	  usage (progname, 0);
	  break;
	default:
	  usage (progname, 1);
	  break;
	}
    }

  /* Make thread master. */
  master = bm->master;

  /* Initializations. */
  srand (time (NULL));
  signal_init (master, array_size(bgp_signals), bgp_signals);
  zprivs_init (&bgpd_privs);
  cmd_init (1);
  vty_init (master);
  memory_init ();

  /* BGP related initialization.  */
  bgp_init ();

  /* Parse config file. */
  vty_read_config (config_file, config_default);

  /* Start execution only if not in dry-run mode */
  if(dryrun)
    return(0);
  
  /* Turn into daemon if daemon_mode is set. */
  if (daemon_mode && daemon (0, 0) < 0)
    {
      zlog_err("BGPd daemon failed: %s", strerror(errno));
      return (1);
    }


  /* Process ID file creation. */
  pid_output (pid_file);

  /* Make bgp vty socket. */
  vty_serv_sock (vty_addr, vty_port, BGP_VTYSH_PATH);

  /* Print banner. */
  zlog_notice ("BGPd %s starting: vty@%d, bgp@%s:%d", QUAGGA_VERSION,
	       vty_port, 
	       (bm->address ? bm->address : "<all>"),
	       bm->port);

  /* Start finite state machine, here we go! */
  while (thread_fetch (master, &thread))
    thread_call (&thread);

  /* Not reached. */
  return (0);
}
