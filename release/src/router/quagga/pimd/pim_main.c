/*
  PIM for Quagga
  Copyright (C) 2008  Everton da Silva Marques

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING; if not, write to the
  Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
  MA 02110-1301 USA
  
  $QuaggaId: $Format:%an, %ai, %h$ $
*/

#include <zebra.h>

#include "log.h"
#include "privs.h" 
#include "version.h"
#include <getopt.h>
#include "command.h"
#include "thread.h"
#include <signal.h>

#include "memory.h"
#include "filter.h"
#include "vty.h"
#include "sigevent.h"
#include "version.h"

#include "pimd.h"
#include "pim_version.h"
#include "pim_signals.h"
#include "pim_zebra.h"

#ifdef PIM_ZCLIENT_DEBUG
extern int zclient_debug;
#endif

extern struct host host;

char config_default[] = SYSCONFDIR PIMD_DEFAULT_CONFIG;

struct option longopts[] = {
  { "daemon",        no_argument,       NULL, 'd'},
  { "config_file",   required_argument, NULL, 'f'},
  { "pid_file",      required_argument, NULL, 'i'},
  { "vty_addr",      required_argument, NULL, 'A'},
  { "vty_port",      required_argument, NULL, 'P'},
  { "version",       no_argument,       NULL, 'v'},
  { "debug_zclient", no_argument,       NULL, 'Z'},
  { "help",          no_argument,       NULL, 'h'},
  { 0 }
};

/* pimd privileges */
zebra_capabilities_t _caps_p [] = 
{
  ZCAP_NET_ADMIN,
  ZCAP_SYS_ADMIN,
  ZCAP_NET_RAW,
};

/* pimd privileges to run with */
struct zebra_privs_t pimd_privs =
{
#if defined(QUAGGA_USER) && defined(QUAGGA_GROUP)
  .user = QUAGGA_USER,
  .group = QUAGGA_GROUP,
#endif
#ifdef VTY_GROUP
  .vty_group = VTY_GROUP,
#endif
  .caps_p = _caps_p,
  .cap_num_p = sizeof(_caps_p)/sizeof(_caps_p[0]),
  .cap_num_i = 0
};

char* progname;
const char *pid_file = PATH_PIMD_PID;

static void usage(int status)
{
  if (status != 0)
    fprintf (stderr, "Try `%s --help' for more information.\n", progname);
  else {    
    printf ("Usage : %s [OPTION...]\n\
Daemon which manages PIM.\n\n\
-d, --daemon         Run in daemon mode\n\
-f, --config_file    Set configuration file name\n\
-i, --pid_file       Set process identifier file name\n\
-z, --socket         Set path of zebra socket\n\
-A, --vty_addr       Set vty's bind address\n\
-P, --vty_port       Set vty's port number\n\
-v, --version        Print program version\n\
"

#ifdef PIM_ZCLIENT_DEBUG
"\
-Z, --debug_zclient  Enable zclient debugging\n\
"
#endif

"\
-h, --help           Display this help and exit\n\
\n\
Report bugs to %s\n", progname, PIMD_BUG_ADDRESS);
  }

  exit (status);
}


int main(int argc, char** argv, char** envp) {
  char *p;
  char *vty_addr = NULL;
  int vty_port = -1;
  int daemon_mode = 0;
  char *config_file = NULL;
  char *zebra_sock_path = NULL;
  struct thread thread;
          
  umask(0027);
 
  progname = ((p = strrchr(argv[0], '/')) ? ++p : argv[0]);
 
  zlog_default = openzlog(progname, ZLOG_PIM,
			  LOG_CONS|LOG_NDELAY|LOG_PID, LOG_DAEMON);
     
  /* this while just reads the options */                       
  while (1) {
    int opt;
            
    opt = getopt_long (argc, argv, "df:i:z:A:P:vZh", longopts, 0);
                      
    if (opt == EOF)
      break;
    
    switch (opt) {
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
      zebra_sock_path = optarg;
      break;
    case 'A':
      vty_addr = optarg;
      break;
    case 'P':
      vty_port = atoi (optarg);
      break;
    case 'v':
      printf(PIMD_PROGNAME " version %s\n", PIMD_VERSION);
      print_version(QUAGGA_PROGNAME);
      exit (0);
      break;
#ifdef PIM_ZCLIENT_DEBUG
    case 'Z':
      zclient_debug = 1;
      break;
#endif
    case 'h':
      usage (0);
      break;
    default:
      usage (1);
      break;
    }
  }

  master = thread_master_create();

  /*
   * Temporarily send zlog to stdout
   */
  zlog_default->maxlvl[ZLOG_DEST_STDOUT] = zlog_default->default_lvl;
  zlog_notice("Boot logging temporarily directed to stdout - begin");

  zlog_notice("Quagga %s " PIMD_PROGNAME " %s starting",
	      QUAGGA_VERSION, PIMD_VERSION);

  /* 
   * Initializations
   */
  zprivs_init (&pimd_privs);
  pim_signals_init();
  cmd_init(1);
  vty_init(master);
  memory_init();
  access_list_init();
  pim_init();

  /*
   * reset zlog default, then will obey configuration file
   */
  zlog_notice("Boot logging temporarily directed to stdout - end");
#if 0
  /* this would disable logging to stdout, but config has not been
     loaded yet to reconfig the logging output */
  zlog_default->maxlvl[ZLOG_DEST_STDOUT] = ZLOG_DISABLED;
#endif

  /*
    Initialize zclient "update" and "lookup" sockets
   */
  pim_zebra_init(zebra_sock_path);

  zlog_notice("Loading configuration - begin");

  /* Get configuration file. */
  vty_read_config(config_file, config_default);

  /*
    Starting from here zlog_* functions will log according configuration
   */

  zlog_notice("Loading configuration - end");

  /* Change to the daemon program. */
  if (daemon_mode) {
    if (daemon(0, 0)) {
      zlog_warn("failed to daemonize");
    }
  }

  /* Process ID file creation. */
  pid_output(pid_file);

  /* Create pimd VTY socket */
  if (vty_port < 0)
    vty_port = PIMD_VTY_PORT;
  vty_serv_sock(vty_addr, vty_port, PIM_VTYSH_PATH);

  zlog_notice("Quagga %s " PIMD_PROGNAME " %s starting, VTY interface at port TCP %d",
	      QUAGGA_VERSION, PIMD_VERSION, vty_port);

#ifdef PIM_DEBUG_BYDEFAULT
  zlog_notice("PIM_DEBUG_BYDEFAULT: Enabling all debug commands");
  PIM_DO_DEBUG_PIM_EVENTS;
  PIM_DO_DEBUG_PIM_PACKETS;
  PIM_DO_DEBUG_PIM_TRACE;
  PIM_DO_DEBUG_IGMP_EVENTS;
  PIM_DO_DEBUG_IGMP_PACKETS;
  PIM_DO_DEBUG_IGMP_TRACE;
  PIM_DO_DEBUG_ZEBRA;
#endif

#ifdef PIM_ZCLIENT_DEBUG
  zlog_notice("PIM_ZCLIENT_DEBUG: zclient debugging is supported, mode is %s (see option -Z)",
	      zclient_debug ? "ON" : "OFF");
#endif

#ifdef PIM_CHECK_RECV_IFINDEX_SANITY
  zlog_notice("PIM_CHECK_RECV_IFINDEX_SANITY: will match sock/recv ifindex");
#ifdef PIM_REPORT_RECV_IFINDEX_MISMATCH
  zlog_notice("PIM_REPORT_RECV_IFINDEX_MISMATCH: will report sock/recv ifindex mismatch");
#endif
#endif

#ifdef PIM_UNEXPECTED_KERNEL_UPCALL
  zlog_notice("PIM_UNEXPECTED_KERNEL_UPCALL: report unexpected kernel upcall");
#endif

#ifdef HAVE_CLOCK_MONOTONIC
  zlog_notice("HAVE_CLOCK_MONOTONIC");
#else
  zlog_notice("!HAVE_CLOCK_MONOTONIC");
#endif

  while (thread_fetch(master, &thread))
    thread_call(&thread);

  zlog_err("%s %s: thread_fetch() returned NULL, exiting",
	   __FILE__, __PRETTY_FUNCTION__);

  /* never reached */
  return 0;
}
