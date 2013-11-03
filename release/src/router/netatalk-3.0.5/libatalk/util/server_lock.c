/*
 * Copyright (c) 1997 Adrian Sun (asun@zoology.washington.edu)
 * All rights reserved. See COPYRIGHT.
 *
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <sys/time.h>

#include <atalk/compat.h>
#include <atalk/util.h>

static struct itimerval itimer;

/* this creates an open lock file which hangs around until the program
 * dies. it returns the pid. due to problems w/ solaris, this has
 * been changed to do the kill() thing. */
pid_t server_lock(char *program, char *pidfile, int debug)
{
  char buf[10];
  FILE *pf;
  pid_t pid;
  int mask;

  if ( !debug ) {
  mask = umask(022);
  /* check for pid. this can get fooled by stale pid's. */
  if ((pf = fopen(pidfile, "r"))) {
    if (fgets(buf, sizeof(buf), pf) && !kill(pid = atol(buf), 0)) {
      fprintf( stderr, "%s is already running (pid = %d), or the lock file is stale.\n",
	       program, pid);      
      fclose(pf);
      return -1;
    }
    fclose(pf);
  }

  if ((pf = fopen(pidfile, "w")) == NULL) {
    fprintf(stderr, "%s: can't open lock file, \"%s\"\n", program,
	    pidfile);
    return -1;
  }
  umask(mask);

  /*
   * Disassociate from controlling tty.
   */

    int		i;

    getitimer(ITIMER_PROF, &itimer);
    switch (pid = fork()) {
    case 0 :
      setitimer(ITIMER_PROF, &itimer, NULL);
      fclose(stdin);
      fclose(stdout);
      fclose(stderr);
      i = open( "/dev/null", O_RDWR );
      i = open( "/dev/null", O_RDWR );
      i = open( "/dev/null", O_RDWR );

#ifdef TIOCNOTTY
      if (( i = open( "/dev/tty", O_RDWR )) >= 0 ) {
	(void)ioctl( i, TIOCNOTTY, 0 );
	setpgid( 0, getpid());
	(void) close(i);
      }
#else
      setpgid( 0, getpid());
#endif
      break;
    case -1 :  /* error */
      perror( "fork" );
    default :  /* server */
      fclose(pf);
      return pid;
    }

  fprintf(pf, "%d\n", getpid());
  fclose(pf);
  } 

  return 0;
}

/*!
 * Check lockfile
 */
int check_lockfile(const char *program, const char *pidfile)
{
    char buf[10];
    FILE *pf;
    pid_t pid;

    /* check for pid. this can get fooled by stale pid's. */
    if ((pf = fopen(pidfile, "r"))) {
        if (fgets(buf, sizeof(buf), pf) && !kill(pid = atol(buf), 0)) {
            fprintf(stderr, "%s is already running (pid = %d), or the lock file is stale.\n",
                    program, pid);      
            fclose(pf);
            return -1;
        }
        fclose(pf);
    }
    return 0;
}

/*!
 * Check and create lockfile
 */
int create_lockfile(const char *program, const char *pidfile)
{
    FILE *pf;
    int mask;
  
    if (check_lockfile(program, pidfile) != 0)
        return -1;

    /* Write PID to pidfile */
    mask = umask(022);
    if ((pf = fopen(pidfile, "w")) == NULL) {
        fprintf(stderr, "%s: can't open lock file, \"%s\"\n", program,
                pidfile);
        return -1;
    }
    umask(mask);
    fprintf(pf, "%d\n", getpid());
    fclose(pf);

    return 0;
}
