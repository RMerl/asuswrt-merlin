/*
 * $Id: test-privs.c,v 1.1 2005/10/11 03:48:28 paul Exp $
 *
 * This file is part of Quagga.
 *
 * Quagga is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * Quagga is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Quagga; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <zebra.h>

#include <lib/version.h>
#include "getopt.h"
#include "privs.h"
#include "memory.h"

zebra_capabilities_t _caps_p [] = 
{
  ZCAP_NET_RAW,
  ZCAP_BIND,
  ZCAP_NET_ADMIN,
  ZCAP_DAC_OVERRIDE,
};

struct zebra_privs_t test_privs =
{
#if defined(QUAGGA_USER) && defined(QUAGGA_GROUP)
  .user = QUAGGA_USER,
  .group = QUAGGA_GROUP,
#endif
#if defined(VTY_GROUP)
  .vty_group = VTY_GROUP,
#endif
  .caps_p = _caps_p,
  .cap_num_p = sizeof(_caps_p)/sizeof(_caps_p[0]),
  .cap_num_i = 0
};

struct option longopts[] = 
{
  { "help",        no_argument,       NULL, 'h'},
  { "user",        required_argument, NULL, 'u'},
  { "group",       required_argument, NULL, 'g'},
  { 0 }
};

/* Help information display. */
static void
usage (char *progname, int status)
{
  if (status != 0)
    fprintf (stderr, "Try `%s --help' for more information.\n", progname);
  else
    {    
      printf ("Usage : %s [OPTION...]\n\
Daemon which does 'slow' things.\n\n\
-u, --user         User to run as\n\
-g, --group        Group to run as\n\
-h, --help         Display this help and exit\n\
\n\
Report bugs to %s\n", progname, ZEBRA_BUG_ADDRESS);
    }
  exit (status);
}

struct thread_master *master;
/* main routine. */
int
main (int argc, char **argv)
{
  char *p;
  char *progname;
  struct zprivs_ids_t ids;
  
  /* Set umask before anything for security */
  umask (0027);

  /* get program name */
  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

  while (1) 
    {
      int opt;

      opt = getopt_long (argc, argv, "hu:g:", longopts, 0);
    
      if (opt == EOF)
	break;

      switch (opt) 
	{
	case 0:
	  break;
        case 'u':
          test_privs.user = optarg;
          break;
        case 'g':
          test_privs.group = optarg;
          break;
	case 'h':
	  usage (progname, 0);
	  break;
	default:
	  usage (progname, 1);
	  break;
	}
    }

  /* Library inits. */
  memory_init ();
  zprivs_init (&test_privs);

#define PRIV_STATE() \
  ((test_privs.current_state() == ZPRIVS_RAISED) ? "Raised" : "Lowered")
  
  printf ("%s\n", PRIV_STATE());
  test_privs.change(ZPRIVS_RAISE);
  
  printf ("%s\n", PRIV_STATE());
  test_privs.change(ZPRIVS_LOWER);
  
  printf ("%s\n", PRIV_STATE());
  zprivs_get_ids (&ids);  
  
  /* terminate privileges */
  zprivs_terminate(&test_privs);
  
  /* but these should continue to work... */
  printf ("%s\n", PRIV_STATE());
  test_privs.change(ZPRIVS_RAISE);
  
  printf ("%s\n", PRIV_STATE());
  test_privs.change(ZPRIVS_LOWER);
  
  printf ("%s\n", PRIV_STATE());
  zprivs_get_ids (&ids);  
  
  printf ("terminating\n");
  return 0;
}
