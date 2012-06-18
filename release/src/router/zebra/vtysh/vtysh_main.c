/* Virtual terminal interface shell.
 * Copyright (C) 2000 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include <zebra.h>

#include <sys/un.h>
#include <setjmp.h>
#include <sys/wait.h>
#include <pwd.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "version.h"
#include "getopt.h"
#include "command.h"

#include "vtysh/vtysh.h"
#include "vtysh/vtysh_user.h"

/* VTY shell program name. */
char *progname;

/* Configuration file name.  Usually this is configurable, but vtysh
   has static configuration file only.  */
char *config_file = NULL;

/* Configuration file and directory. */
char *config_current = NULL;
char config_default[] = SYSCONFDIR VTYSH_DEFAULT_CONFIG;

/* Integrated configuration file. */
char *integrate_file = NULL;
char *integrate_current = NULL;
#if 0
char integrate_default[] = SYSCONFDIR INTEGRATE_DEFAULT_CONFIG;
#endif

/* Flag for indicate executing child command. */
int execute_flag = 0;

/* For sigsetjmp() & siglongjmp(). */
static sigjmp_buf jmpbuf;

/* Flag for avoid recursive siglongjmp() call. */
static int jmpflag = 0;

/* A static variable for holding the line. */
static char *line_read;

/* Master of threads. */
struct thread_master *master;

/* SIGTSTP handler.  This function care user's ^Z input. */
void
sigtstp (int sig)
{
  /* Execute "end" command. */
  vtysh_execute ("end");
  
  /* Initialize readline. */
  rl_initialize ();
  printf ("\n");

  /* Check jmpflag for duplicate siglongjmp(). */
  if (! jmpflag)
    return;

  jmpflag = 0;

  /* Back to main command loop. */
  siglongjmp (jmpbuf, 1);
}

/* SIGINT handler.  This function care user's ^Z input.  */
void
sigint (int sig)
{
  /* Check this process is not child process. */
  if (! execute_flag)
    {
      rl_initialize ();
      printf ("\n");
      rl_forced_update_display ();
    }
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
  signal_set (SIGINT, sigint);
  signal_set (SIGTSTP, sigtstp);
  signal_set (SIGPIPE, SIG_IGN);
}

/* Help information display. */
static void
usage (int status)
{
  if (status != 0)
    fprintf (stderr, "Try `%s --help' for more information.\n", progname);
  else
    {    
      printf ("Usage : %s [OPTION...]\n\n\
Daemon which manages kernel routing table management and \
redistribution between different routing protocols.\n\n\
-b, --boot               Execute boot startup configuration\n\
-e, --eval               Execute argument as command\n\
-h, --help               Display this help and exit\n\
\n\
Report bugs to %s\n", progname, ZEBRA_BUG_ADDRESS);
    }
  exit (status);
}

/* VTY shell options, we use GNU getopt library. */
struct option longopts[] = 
{
  { "boot",                no_argument,             NULL, 'b'},
  { "eval",                 required_argument,       NULL, 'e'},
  { "help",                 no_argument,             NULL, 'h'},
  { 0 }
};

/* Read a string, and return a pointer to it.  Returns NULL on EOF. */
char *
vtysh_rl_gets ()
{
  /* If the buffer has already been allocated, return the memory
     to the free pool. */
  if (line_read)
    {
      free (line_read);
      line_read = NULL;
    }
     
  /* Get a line from the user.  Change prompt according to node.  XXX. */
  line_read = readline (vtysh_prompt ());
     
  /* If the line has any text in it, save it on the history. */
  if (line_read && *line_read)
    add_history (line_read);
     
  return (line_read);
}

/* VTY shell main routine. */
int
main (int argc, char **argv, char **env)
{
  char *p;
  int opt;
  int eval_flag = 0;
  int boot_flag = 0;
  char *eval_line = NULL;
  char *integrated_file = NULL;

  /* Preserve name of myself. */
  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

  /* Option handling. */
  while (1) 
    {
      opt = getopt_long (argc, argv, "be:h", longopts, 0);
    
      if (opt == EOF)
	break;

      switch (opt) 
	{
	case 0:
	  break;
	case 'b':
	  boot_flag = 1;
	  break;
	case 'e':
	  eval_flag = 1;
	  eval_line = optarg;
	  break;
	case 'h':
	  usage (0);
	  break;
	case 'i':
	  integrated_file = strdup (optarg);
	default:
	  usage (1);
	  break;
	}
    }

  /* Initialize user input buffer. */
  line_read = NULL;

  /* Signal and others. */
  signal_init ();

  /* Make vty structure and register commands. */
  vtysh_init_vty ();
  vtysh_init_cmd ();
  vtysh_user_init ();
  vtysh_config_init ();

  vty_init_vtysh ();

  sort_node ();

  vtysh_connect_all ();

  /* Read vtysh configuration file. */
  vtysh_read_config (config_file, config_current, config_default);

  /* If eval mode */
  if (eval_flag)
    {
      vtysh_execute_no_pager (eval_line);
      exit (0);
    }
  
  /* Boot startup configuration file. */
  if (boot_flag)
    {
      vtysh_read_config (integrate_file, integrate_current, integrate_default);
      exit (0);
    }

  vtysh_pager_init ();

  vtysh_readline_init ();

  vty_hello (vty);

  vtysh_auth ();

  /* Preparation for longjmp() in sigtstp(). */
  sigsetjmp (jmpbuf, 1);
  jmpflag = 1;

  /* Main command loop. */
  while (vtysh_rl_gets ())
    vtysh_execute (line_read);

  printf ("\n");

  /* Rest in peace. */
  exit (0);
}
