/* ============================================================================
 * Copyright (C) 1999-2000 Angus Mackay. All rights reserved; 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * ============================================================================
 */

/*
 * cache_file.c
 *
 */


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#if HAVE_ERRNO_H
#  include <errno.h>
#endif
#ifdef EMBED
#include <fcntl.h>
#include <signal.h>
#include <config/autoconf.h>
#endif

#include <cache_file.h>
#include <dprintf.h>

#if HAVE_STRERROR
//extern int errno;
#  define error_string strerror(errno)
#elif HAVE_SYS_ERRLIST
extern const char *const sys_errlist[];
extern int errno;
#  define error_string (sys_errlist[errno])
#else
#  define error_string "error message not found"
#endif

int read_cache_file(char *file, time_t *date, char **ipaddr)
{
  FILE *fp = NULL;
  char buf[BUFSIZ+1];
  char *p;
  char *datestr;
  char *ipstr;
#if HAVE_STAT
  struct stat st;
#endif

  // safety first
  buf[BUFSIZ] = '\0';

  // indicate failure
  *date = 0;
  *ipaddr = NULL;

#if HAVE_STAT
  if(stat(file, &st) != 0)
  {
    if(errno == ENOENT)
    {
      return(0);
    }
    return(-1);
  }
#endif

  if((fp=fopen(file, "r")) == NULL)
  {
    return(-1);
  }

  if(fgets(buf, BUFSIZ, fp) != NULL)
  {

    /* chomp new line */
    p = buf;
    while(*p != '\0' && *p != '\r' && *p != '\n') { p++; }
    *p = '\0';

    /* find the first comma */
    p = buf;
    while(*p != '\0' && *p != ',') { p++; }
    if(*p == '\0')
    {
      goto ERR;
    }

    // slap in a null
    *p++ = '\0';

    datestr = buf;
    ipstr = p;

    *date = strtoul(datestr, NULL, 10);
    *ipaddr = strdup(ipstr);
  }
  else
  {
    *date = 0;
    *ipaddr = NULL;
  }

  fclose(fp);

  return 0;

ERR:

  if(fp) { fclose(fp); }
  return(-1);
}

int write_cache_file(char *file, time_t date, char *ipaddr)
{
  FILE *fp = NULL;

  if((fp=fopen(file, "w")) == NULL)
  {
    return(-1);
  }

  fprintf(fp, "%ld,%s\n", date, ipaddr);

  fclose(fp);

#ifdef CONFIG_USER_FLATFSD_FLATFSD
  {
    char value[16];
    pid_t pid;
    int fd;

    fd = open("/var/run/flatfsd.pid", O_RDONLY);
    if (fd != -1) {
      if (read(fd, value, sizeof(value)) > 0 &&
          (pid = atoi(value)) > 1)
        kill(pid, SIGUSR1);
      close(fd);
    }
  }
#endif

  return 0;
}

