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
#include "version.h"
#include "memory.h"
#include "prefix.h"
#include "log.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_mplsvpn.h"

/* bgpd options, we use GNU getopt library. */
struct option longopts[] = 
{
  { "daemon",      no_argument,       NULL, 'd'},
  { "config_file", required_argument, NULL, 'f'},
  { "pid_file",    required_argument, NULL, 'i'},
  { "bgp_port",    required_argument, NULL, 'p'},
  { "vty_addr",    required_argument, NULL, 'A'},
  { "vty_port",    required_argument, NULL, 'P'},
  { "retain",      no_argument,       NULL, 'r'},
  { "no_kernel",   no_argument,       NULL, 'n'},
  { "version",     no_argument,       NULL, 'v'},
  { "help",        no_argument,       NULL, 'h'},
  { 0 }
};

/* Configuration file and directory. */
char config_current[] = BGP_DEFAULT_CONFIG;
char config_default[] = SYSCONFDIR BGP_DEFAULT_CONFIG;

/* Route retain mode flag. */
int retain_mode = 0;

/* Master of threads. */
struct thread_master *master;

/* Manually specified configuration file name.  */
char *config_file = NULL;

/* Process ID saved for use by init system */
char *pid_file = PATH_BGPD_PID;

/* VTY port number and address.  */
int vty_port = BGP_VTY_PORT;
char *vty_addr = NULL;

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
-p, --bgp_port     Set bgp protocol's port number\n\
-A, --vty_addr     Set vty's bind address\n\
-P, --vty_port     Set vty's port number\n\
-r, --retain       When program terminates, retain added route by bgpd.\n\
-n, --no_kernel    Do not install route to kernel.\n\
-v, --version      Print program version\n\
-h, --help         Display this help and exit\n\
\n\
Report bugs to %s\n", progname, ZEBRA_BUG_ADDRESS);
    }

  exit (status);
}

/* SIGHUP handler. */
void 
sighup (int sig)
{
  zlog (NULL, LOG_INFO, "SIGHUP received");

  /* Terminate all thread. */
  bgp_terminate ();
  bgp_reset ();
  zlog_info ("bgpd restarting!");

  /* Reload config file. */
  vty_read_config (config_file, config_current, config_default);

  /* Create VTY's socket */
  vty_serv_sock (vty_addr, vty_port ? vty_port : BGP_VTY_PORT, BGP_VTYSH_PATH);

  /* Try to return to normal operation. */
}

/* SIGINT handler. */
void
sigint (int sig)
{
  zlog (NULL, LOG_INFO, "Terminating on signal");

  if (! retain_mode)
    bgp_terminate ();

  exit (0);
}

/* SIGUSR1 handler. */
void
sigusr1 (int sig)
{
  zlog_rotate (NULL);
}

/* Signale wrapper. */
RETSIGTYPE *
signal_set (int signo, void (*func)(int))
{
  int ret;
  struct sigaction sig;
  struct sigaction osig;

  sig.sa_handler = func;
  sigemptyset (&sig.sa_mask);
  sig.sa_flags = 0;
#ifdef SA_RESTART
  sig.sa_flags |= SA_RESTART;
#endif /* SA_RESTART */

  ret = sigaction (signo, &sig, &osig);

  if (ret < 0) 
    return (SIG_ERR);
  else
    return (osig.sa_handler);
}

/* Initialization of signal handles. */
void
signal_init ()
{
  signal_set (SIGHUP, sighup);
  signal_set (SIGINT, sigint);
  signal_set (SIGTERM, sigint);
  signal_set (SIGPIPE, SIG_IGN);
  signal_set (SIGUSR1, sigusr1);
}

/* Main routine of bgpd. Treatment of argument and start bgp finite
   state machine is handled at here. */
int
main (int argc, char **argv)
{
  char *p;
  int opt;
  int daemon_mode = 0;
  char *progname;
  struct thread thread;

  /* Set umask before anything for security */
  umask (0027);

  /* Preserve name of myself. */
  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

  zlog_default = openzlog (progname, ZLOG_NOLOG, ZLOG_BGP,
			   LOG_CONS|LOG_NDELAY|LOG_PID, LOG_DAEMON);

  /* BGP master init. */
  bgp_master_init ();

  /* Command line argument treatment. */
  while (1) 
    {
      opt = getopt_long (argc, argv, "df:hp:A:P:rnv", longopts, 0);
    
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
	case 'p':
	  bm->port = atoi (optarg);
	  break;
	case 'A':
	  vty_addr = optarg;
	  break;
	case 'P':
	  vty_port = atoi (optarg);
	  break;
	case 'r':
	  retain_mode = 1;
	  break;
	case 'n':
	  bgp_option_set (BGP_OPT_NO_FIB);
	  break;
	case 'v':
	  print_version (progname);
	  exit (0);
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
  signal_init ();
  cmd_init (1);
  vty_init ();
  memory_init ();

  /* BGP related initialization.  */
  bgp_init ();

  /* Sort CLI commands. */
  sort_node ();

  /* Parse config file. */
  vty_read_config (config_file, config_current, config_default);

  /* Turn into daemon if daemon_mode is set. */
  if (daemon_mode)
    daemon (0, 0);

  /* Process ID file creation. */
  pid_output (pid_file);

  /* Make bgp vty socket. */
  vty_serv_sock (vty_addr, vty_port, BGP_VTYSH_PATH);

  /* Print banner. */
  zlog_info ("BGPd %s starting: vty@%d, bgp@%d", ZEBRA_BGPD_VERSION,
	     vty_port, bm->port);

  /* Start finite state machine, here we go! */
  while (thread_fetch (master, &thread))
    thread_call (&thread);

  /* Not reached. */
  exit (0);
}
