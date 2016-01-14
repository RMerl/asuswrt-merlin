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

#include <lib/version.h>
#include "getopt.h"
#include "command.h"
#include "memory.h"

#include "vtysh/vtysh.h"
#include "vtysh/vtysh_user.h"

/* VTY shell program name. */
char *progname;

/* Configuration file name and directory. */
char config_default[] = SYSCONFDIR VTYSH_DEFAULT_CONFIG;
char history_file[MAXPATHLEN];

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

/* Command logging */
FILE *logfile;

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

/* Signale wrapper for vtysh. We don't use sigevent because
 * vtysh doesn't use threads. TODO */
RETSIGTYPE *
vtysh_signal_set (int signo, void (*func)(int))
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
vtysh_signal_init ()
{
  vtysh_signal_set (SIGINT, sigint);
  vtysh_signal_set (SIGTSTP, sigtstp);
  vtysh_signal_set (SIGPIPE, SIG_IGN);
}

/* Help information display. */
static void
usage (int status)
{
  if (status != 0)
    fprintf (stderr, "Try `%s --help' for more information.\n", progname);
  else
    printf ("Usage : %s [OPTION...]\n\n" \
	    "Integrated shell for Quagga routing software suite. \n\n" \
	    "-b, --boot               Execute boot startup configuration\n" \
	    "-c, --command            Execute argument as command\n" \
	    "-d, --daemon             Connect only to the specified daemon\n" \
	    "-E, --echo               Echo prompt and command in -c mode\n" \
	    "-C, --dryrun             Check configuration for validity and exit\n" \
	    "-h, --help               Display this help and exit\n\n" \
	    "Note that multiple commands may be executed from the command\n" \
	    "line by passing multiple -c args, or by embedding linefeed\n" \
	    "characters in one or more of the commands.\n\n" \
	    "Report bugs to %s\n", progname, ZEBRA_BUG_ADDRESS);

  exit (status);
}

/* VTY shell options, we use GNU getopt library. */
struct option longopts[] = 
{
  { "boot",                 no_argument,             NULL, 'b'},
  /* For compatibility with older zebra/quagga versions */
  { "eval",                 required_argument,       NULL, 'e'},
  { "command",              required_argument,       NULL, 'c'},
  { "daemon",               required_argument,       NULL, 'd'},
  { "echo",                 no_argument,             NULL, 'E'},
  { "dryrun",		    no_argument,	     NULL, 'C'},
  { "help",                 no_argument,             NULL, 'h'},
  { "noerror",		    no_argument,	     NULL, 'n'},
  { 0 }
};

/* Read a string, and return a pointer to it.  Returns NULL on EOF. */
char *
vtysh_rl_gets ()
{
  HIST_ENTRY *last;
  /* If the buffer has already been allocated, return the memory
   * to the free pool. */
  if (line_read)
    {
      free (line_read);
      line_read = NULL;
    }
     
  /* Get a line from the user.  Change prompt according to node.  XXX. */
  line_read = readline (vtysh_prompt ());
     
  /* If the line has any text in it, save it on the history. But only if
   * last command in history isn't the same one. */
  if (line_read && *line_read)
    {
      using_history();
      last = previous_history();
      if (!last || strcmp (last->line, line_read) != 0) {
	add_history (line_read);
	append_history(1,history_file);
      }
    }
     
  return (line_read);
}

static void log_it(const char *line)
{
  time_t t = time(NULL);
  struct tm *tmp = localtime(&t);
  char *user = getenv("USER") ? : "boot";
  char tod[64];

  strftime(tod, sizeof tod, "%Y%m%d-%H:%M.%S", tmp);
  
  fprintf(logfile, "%s:%s %s\n", tod, user, line);
}

/* VTY shell main routine. */
int
main (int argc, char **argv, char **env)
{
  char *p;
  int opt;
  int dryrun = 0;
  int boot_flag = 0;
  const char *daemon_name = NULL;
  struct cmd_rec {
    const char *line;
    struct cmd_rec *next;
  } *cmd = NULL;
  struct cmd_rec *tail = NULL;
  int echo_command = 0;
  int no_error = 0;

  /* Preserve name of myself. */
  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

  /* if logging open now */
  if ((p = getenv("VTYSH_LOG")) != NULL)
      logfile = fopen(p, "a");

  /* Option handling. */
  while (1) 
    {
      opt = getopt_long (argc, argv, "be:c:d:nEhC", longopts, 0);
    
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
	case 'c':
	  {
	    struct cmd_rec *cr;
	    cr = XMALLOC(0, sizeof(*cr));
	    cr->line = optarg;
	    cr->next = NULL;
	    if (tail)
	      tail->next = cr;
	    else
	      cmd = cr;
	    tail = cr;
	  }
	  break;
	case 'd':
	  daemon_name = optarg;
	  break;
	case 'n':
	  no_error = 1;
	  break;
	case 'E':
	  echo_command = 1;
	  break;
	case 'C':
	  dryrun = 1;
	  break;
	case 'h':
	  usage (0);
	  break;
	default:
	  usage (1);
	  break;
	}
    }

  /* Initialize user input buffer. */
  line_read = NULL;
  setlinebuf(stdout);

  /* Signal and others. */
  vtysh_signal_init ();

  /* Make vty structure and register commands. */
  vtysh_init_vty ();
  vtysh_init_cmd ();
  vtysh_user_init ();
  vtysh_config_init ();

  vty_init_vtysh ();

  /* Read vtysh configuration file before connecting to daemons. */
  vtysh_read_config (config_default);

  /* Start execution only if not in dry-run mode */
  if(dryrun)
    return(0);
  
  /* Ignore error messages */
  if (no_error)
    freopen("/dev/null", "w", stdout);

  /* Make sure we pass authentication before proceeding. */
  vtysh_auth ();

  /* Do not connect until we have passed authentication. */
  if (vtysh_connect_all (daemon_name) <= 0)
    {
      fprintf(stderr, "Exiting: failed to connect to any daemons.\n");
      exit(1);
    }

  /* If eval mode. */
  if (cmd)
    {
      /* Enter into enable node. */
      vtysh_execute ("enable");

      while (cmd != NULL)
        {
	  int ret;
	  char *eol;

	  while ((eol = strchr(cmd->line, '\n')) != NULL)
	    {
	      *eol = '\0';

	      if (echo_command)
		printf("%s%s\n", vtysh_prompt(), cmd->line);
	      
	      if (logfile)
		log_it(cmd->line);

	      ret = vtysh_execute_no_pager(cmd->line);
	      if (!no_error &&
		  ! (ret == CMD_SUCCESS ||
		     ret == CMD_SUCCESS_DAEMON ||
		     ret == CMD_WARNING))
		exit(1);

	      cmd->line = eol+1;
	    }

	  if (echo_command)
	    printf("%s%s\n", vtysh_prompt(), cmd->line);

	  if (logfile)
	    log_it(cmd->line);

	  ret = vtysh_execute_no_pager(cmd->line);
	  if (!no_error &&
	      ! (ret == CMD_SUCCESS ||
		 ret == CMD_SUCCESS_DAEMON ||
		 ret == CMD_WARNING))
	    exit(1);

	  {
	    struct cmd_rec *cr;
	    cr = cmd;
	    cmd = cmd->next;
	    XFREE(0, cr);
	  }
        }
      exit (0);
    }
  
  /* Boot startup configuration file. */
  if (boot_flag)
    {
      if (vtysh_read_config (integrate_default))
	{
	  fprintf (stderr, "Can't open configuration file [%s]\n",
		   integrate_default);
	  exit (1);
	}
      else
	exit (0);
    }

  vtysh_pager_init ();

  vtysh_readline_init ();

  vty_hello (vty);

  /* Enter into enable node. */
  vtysh_execute ("enable");

  /* Preparation for longjmp() in sigtstp(). */
  sigsetjmp (jmpbuf, 1);
  jmpflag = 1;

  snprintf(history_file, sizeof(history_file), "%s/.history_quagga", getenv("HOME"));
  read_history(history_file);
  /* Main command loop. */
  while (vtysh_rl_gets ())
    vtysh_execute (line_read);

  history_truncate_file(history_file,1000);
  printf ("\n");

  /* Rest in peace. */
  exit (0);
}
