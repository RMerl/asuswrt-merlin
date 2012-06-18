
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#if HAVE_ERRNO_H
#  include <errno.h>
#endif
#if HAVE_SIGNAL_H
#  include <signal.h>
#endif

#include <error.h>
#include <dprintf.h>

int pid_file_create(char *pid_file)
{
#if HAVE_GETPID
  char buf[64];
  FILE* fp = NULL;
  pid_t mypid;
  pid_t otherpid = -1;

#if HAVE_SETEUID && HAVE_SETEGID
  gid_t oldegid = -1;
  uid_t oldeuid = -1;
#endif

#if HAVE_SETEUID && HAVE_SETEGID
  oldegid = getegid();
  oldeuid = geteuid();

  setegid(getgid());
  seteuid(getuid());
#endif

  // check if the pid file exists
  if((fp=fopen(pid_file, "r")) != NULL)
  {
    // if the pid file exists what does it say?
    if(fgets(buf, sizeof(buf), fp) == NULL)
    {
      show_message("error reading pid file: %s (%s)\n", pid_file, error_string);
      goto ERR;
    }
    fclose(fp);
    otherpid = atoi(buf);

    // check to see if the pid is valid
    if(kill(otherpid, 0) == 0)
    {
      // if it is alive then we quit
      show_message("there is another program already running with pid %d.\n", (int)otherpid);
      goto ERR;
    }
  }

  // create the pid file
  if((fp=fopen(pid_file, "w")) == NULL)
  {
    show_message("could not create pid file: %s (%s)\n", pid_file, error_string);
    goto ERR;
  }

  mypid = getpid();
  fprintf(fp, "%d\n", (int)mypid);
  fclose(fp);

  dprintf((stderr, "pid file %s successfully created with value %d.\n",
      pid_file, (int)mypid));

#if HAVE_SETEUID && HAVE_SETEGID
  setegid(oldegid);
  seteuid(oldeuid);
#endif

  return 0;

ERR:
  if(fp) { fclose(fp); fp = NULL; }
#if HAVE_SETEUID && HAVE_SETEGID
  setegid(oldegid);
  seteuid(oldeuid);
#endif
  return(-1);

#else
  return(-1);
#endif
}

int pid_file_delete(char *pid_file)
{
  int ret;

#if HAVE_SETEUID && HAVE_SETEGID
  gid_t oldegid = -1;
  uid_t oldeuid = -1;
#endif

#if HAVE_SETEUID && HAVE_SETEGID
  oldegid = getegid();
  oldeuid = geteuid();

  setegid(getgid());
  seteuid(getuid());
#endif

  ret = unlink(pid_file);

#if HAVE_SETEUID && HAVE_SETEGID
  setegid(oldegid);
  seteuid(oldeuid);
#endif

  return ret;
}

