/*
 * Logging of zebra
 * Copyright (C) 1997, 1998, 1999 Kunihiro Ishiguro
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

#define QUAGGA_DEFINE_DESC_TABLE

#include <zebra.h>

#include "log.h"
#include "memory.h"
#include "command.h"
#ifndef SUNOS_5
#include <sys/un.h>
#endif
/* for printstack on solaris */
#ifdef HAVE_UCONTEXT_H
#include <ucontext.h>
#endif

static int logfile_fd = -1;	/* Used in signal handler. */

struct zlog *zlog_default = NULL;

const char *zlog_proto_names[] = 
{
  "NONE",
  "DEFAULT",
  "ZEBRA",
  "RIP",
  "BGP",
  "OSPF",
  "RIPNG",
  "BABEL",
  "OSPF6",
  "ISIS",
  "PIM",
  "MASC",
  NULL,
};

const char *zlog_priority[] =
{
  "emergencies",
  "alerts",
  "critical",
  "errors",
  "warnings",
  "notifications",
  "informational",
  "debugging",
  NULL,
};
  


/* For time string format. */

size_t
quagga_timestamp(int timestamp_precision, char *buf, size_t buflen)
{
  static struct {
    time_t last;
    size_t len;
    char buf[28];
  } cache;
  struct timeval clock;

  /* would it be sufficient to use global 'recent_time' here?  I fear not... */
  gettimeofday(&clock, NULL);

  /* first, we update the cache if the time has changed */
  if (cache.last != clock.tv_sec)
    {
      struct tm *tm;
      cache.last = clock.tv_sec;
      tm = localtime(&cache.last);
      cache.len = strftime(cache.buf, sizeof(cache.buf),
      			   "%Y/%m/%d %H:%M:%S", tm);
    }
  /* note: it's not worth caching the subsecond part, because
     chances are that back-to-back calls are not sufficiently close together
     for the clock not to have ticked forward */

  if (buflen > cache.len)
    {
      memcpy(buf, cache.buf, cache.len);
      if ((timestamp_precision > 0) &&
	  (buflen > cache.len+1+timestamp_precision))
	{
	  /* should we worry about locale issues? */
	  static const int divisor[] = {0, 100000, 10000, 1000, 100, 10, 1};
	  int prec;
	  char *p = buf+cache.len+1+(prec = timestamp_precision);
	  *p-- = '\0';
	  while (prec > 6)
	    /* this is unlikely to happen, but protect anyway */
	    {
	      *p-- = '0';
	      prec--;
	    }
	  clock.tv_usec /= divisor[prec];
	  do
	    {
	      *p-- = '0'+(clock.tv_usec % 10);
	      clock.tv_usec /= 10;
	    }
	  while (--prec > 0);
	  *p = '.';
	  return cache.len+1+timestamp_precision;
	}
      buf[cache.len] = '\0';
      return cache.len;
    }
  if (buflen > 0)
    buf[0] = '\0';
  return 0;
}

/* Utility routine for current time printing. */
static void
time_print(FILE *fp, struct timestamp_control *ctl)
{
  if (!ctl->already_rendered)
    {
      ctl->len = quagga_timestamp(ctl->precision, ctl->buf, sizeof(ctl->buf));
      ctl->already_rendered = 1;
    }
  fprintf(fp, "%s ", ctl->buf);
}
  

/* va_list version of zlog. */
static void
vzlog (struct zlog *zl, int priority, const char *format, va_list args)
{
  struct timestamp_control tsctl;
  tsctl.already_rendered = 0;

  /* If zlog is not specified, use default one. */
  if (zl == NULL)
    zl = zlog_default;

  /* When zlog_default is also NULL, use stderr for logging. */
  if (zl == NULL)
    {
      tsctl.precision = 0;
      time_print(stderr, &tsctl);
      fprintf (stderr, "%s: ", "unknown");
      vfprintf (stderr, format, args);
      fprintf (stderr, "\n");
      fflush (stderr);

      /* In this case we return at here. */
      return;
    }
  tsctl.precision = zl->timestamp_precision;

  /* Syslog output */
  if (priority <= zl->maxlvl[ZLOG_DEST_SYSLOG])
    {
      va_list ac;
      va_copy(ac, args);
      vsyslog (priority|zlog_default->facility, format, ac);
      va_end(ac);
    }

  /* File output. */
  if ((priority <= zl->maxlvl[ZLOG_DEST_FILE]) && zl->fp)
    {
      va_list ac;
      time_print (zl->fp, &tsctl);
      if (zl->record_priority)
	fprintf (zl->fp, "%s: ", zlog_priority[priority]);
      fprintf (zl->fp, "%s: ", zlog_proto_names[zl->protocol]);
      va_copy(ac, args);
      vfprintf (zl->fp, format, ac);
      va_end(ac);
      fprintf (zl->fp, "\n");
      fflush (zl->fp);
    }

  /* stdout output. */
  if (priority <= zl->maxlvl[ZLOG_DEST_STDOUT])
    {
      va_list ac;
      time_print (stdout, &tsctl);
      if (zl->record_priority)
	fprintf (stdout, "%s: ", zlog_priority[priority]);
      fprintf (stdout, "%s: ", zlog_proto_names[zl->protocol]);
      va_copy(ac, args);
      vfprintf (stdout, format, ac);
      va_end(ac);
      fprintf (stdout, "\n");
      fflush (stdout);
    }

  /* Terminal monitor. */
  if (priority <= zl->maxlvl[ZLOG_DEST_MONITOR])
    vty_log ((zl->record_priority ? zlog_priority[priority] : NULL),
	     zlog_proto_names[zl->protocol], format, &tsctl, args);
}

static char *
str_append(char *dst, int len, const char *src)
{
  while ((len-- > 0) && *src)
    *dst++ = *src++;
  return dst;
}

static char *
num_append(char *s, int len, u_long x)
{
  char buf[30];
  char *t;

  if (!x)
    return str_append(s,len,"0");
  *(t = &buf[sizeof(buf)-1]) = '\0';
  while (x && (t > buf))
    {
      *--t = '0'+(x % 10);
      x /= 10;
    }
  return str_append(s,len,t);
}

#if defined(SA_SIGINFO) || defined(HAVE_STACK_TRACE)
static char *
hex_append(char *s, int len, u_long x)
{
  char buf[30];
  char *t;

  if (!x)
    return str_append(s,len,"0");
  *(t = &buf[sizeof(buf)-1]) = '\0';
  while (x && (t > buf))
    {
      u_int cc = (x % 16);
      *--t = ((cc < 10) ? ('0'+cc) : ('a'+cc-10));
      x /= 16;
    }
  return str_append(s,len,t);
}
#endif

/* Needs to be enhanced to support Solaris. */
static int
syslog_connect(void)
{
#ifdef SUNOS_5
  return -1;
#else
  int fd;
  char *s;
  struct sockaddr_un addr;

  if ((fd = socket(AF_UNIX,SOCK_DGRAM,0)) < 0)
    return -1;
  addr.sun_family = AF_UNIX;
#ifdef _PATH_LOG
#define SYSLOG_SOCKET_PATH _PATH_LOG
#else
#define SYSLOG_SOCKET_PATH "/dev/log"
#endif
  s = str_append(addr.sun_path,sizeof(addr.sun_path),SYSLOG_SOCKET_PATH);
#undef SYSLOG_SOCKET_PATH
  *s = '\0';
  if (connect(fd,(struct sockaddr *)&addr,sizeof(addr)) < 0)
    {
      close(fd);
      return -1;
    }
  return fd;
#endif
}

static void
syslog_sigsafe(int priority, const char *msg, size_t msglen)
{
  static int syslog_fd = -1;
  char buf[sizeof("<1234567890>ripngd[1234567890]: ")+msglen+50];
  char *s;

  if ((syslog_fd < 0) && ((syslog_fd = syslog_connect()) < 0))
    return;

#define LOC s,buf+sizeof(buf)-s
  s = buf;
  s = str_append(LOC,"<");
  s = num_append(LOC,priority);
  s = str_append(LOC,">");
  /* forget about the timestamp, too difficult in a signal handler */
  s = str_append(LOC,zlog_default->ident);
  if (zlog_default->syslog_options & LOG_PID)
    {
      s = str_append(LOC,"[");
      s = num_append(LOC,getpid());
      s = str_append(LOC,"]");
    }
  s = str_append(LOC,": ");
  s = str_append(LOC,msg);
  write(syslog_fd,buf,s-buf);
#undef LOC
}

static int
open_crashlog(void)
{
#define CRASHLOG_PREFIX "/var/tmp/quagga."
#define CRASHLOG_SUFFIX "crashlog"
  if (zlog_default && zlog_default->ident)
    {
      /* Avoid strlen since it is not async-signal-safe. */
      const char *p;
      size_t ilen;

      for (p = zlog_default->ident, ilen = 0; *p; p++)
	ilen++;
      {
	char buf[sizeof(CRASHLOG_PREFIX)+ilen+sizeof(CRASHLOG_SUFFIX)+3];
	char *s = buf;
#define LOC s,buf+sizeof(buf)-s
	s = str_append(LOC, CRASHLOG_PREFIX);
	s = str_append(LOC, zlog_default->ident);
	s = str_append(LOC, ".");
	s = str_append(LOC, CRASHLOG_SUFFIX);
#undef LOC
	*s = '\0';
	return open(buf, O_WRONLY|O_CREAT|O_EXCL, LOGFILE_MASK);
      }
    }
  return open(CRASHLOG_PREFIX CRASHLOG_SUFFIX, O_WRONLY|O_CREAT|O_EXCL,
	      LOGFILE_MASK);
#undef CRASHLOG_SUFFIX
#undef CRASHLOG_PREFIX
}

/* Note: the goal here is to use only async-signal-safe functions. */
void
zlog_signal(int signo, const char *action
#ifdef SA_SIGINFO
	    , siginfo_t *siginfo, void *program_counter
#endif
	   )
{
  time_t now;
  char buf[sizeof("DEFAULT: Received signal S at T (si_addr 0xP, PC 0xP); aborting...")+100];
  char *s = buf;
  char *msgstart = buf;
#define LOC s,buf+sizeof(buf)-s

  time(&now);
  if (zlog_default)
    {
      s = str_append(LOC,zlog_proto_names[zlog_default->protocol]);
      *s++ = ':';
      *s++ = ' ';
      msgstart = s;
    }
  s = str_append(LOC,"Received signal ");
  s = num_append(LOC,signo);
  s = str_append(LOC," at ");
  s = num_append(LOC,now);
#ifdef SA_SIGINFO
  s = str_append(LOC," (si_addr 0x");
  s = hex_append(LOC,(u_long)(siginfo->si_addr));
  if (program_counter)
    {
      s = str_append(LOC,", PC 0x");
      s = hex_append(LOC,(u_long)program_counter);
    }
  s = str_append(LOC,"); ");
#else /* SA_SIGINFO */
  s = str_append(LOC,"; ");
#endif /* SA_SIGINFO */
  s = str_append(LOC,action);
  if (s < buf+sizeof(buf))
    *s++ = '\n';

  /* N.B. implicit priority is most severe */
#define PRI LOG_CRIT

#define DUMP(FD) write(FD, buf, s-buf);
  /* If no file logging configured, try to write to fallback log file. */
  if ((logfile_fd >= 0) || ((logfile_fd = open_crashlog()) >= 0))
    DUMP(logfile_fd)
  if (!zlog_default)
    DUMP(STDERR_FILENO)
  else
    {
      if (PRI <= zlog_default->maxlvl[ZLOG_DEST_STDOUT])
        DUMP(STDOUT_FILENO)
      /* Remove trailing '\n' for monitor and syslog */
      *--s = '\0';
      if (PRI <= zlog_default->maxlvl[ZLOG_DEST_MONITOR])
        vty_log_fixed(buf,s-buf);
      if (PRI <= zlog_default->maxlvl[ZLOG_DEST_SYSLOG])
	syslog_sigsafe(PRI|zlog_default->facility,msgstart,s-msgstart);
    }
#undef DUMP

  zlog_backtrace_sigsafe(PRI,
#ifdef SA_SIGINFO
  			 program_counter
#else
			 NULL
#endif
			);

  s = buf;
  if (!thread_current)
    s = str_append (LOC, "no thread information available\n");
  else
    {
      s = str_append (LOC, "in thread ");
      s = str_append (LOC, thread_current->funcname);
      s = str_append (LOC, " scheduled from ");
      s = str_append (LOC, thread_current->schedfrom);
      s = str_append (LOC, ":");
      s = num_append (LOC, thread_current->schedfrom_line);
      s = str_append (LOC, "\n");
    }

#define DUMP(FD) write(FD, buf, s-buf);
  /* If no file logging configured, try to write to fallback log file. */
  if (logfile_fd >= 0)
    DUMP(logfile_fd)
  if (!zlog_default)
    DUMP(STDERR_FILENO)
  else
    {
      if (PRI <= zlog_default->maxlvl[ZLOG_DEST_STDOUT])
        DUMP(STDOUT_FILENO)
      /* Remove trailing '\n' for monitor and syslog */
      *--s = '\0';
      if (PRI <= zlog_default->maxlvl[ZLOG_DEST_MONITOR])
        vty_log_fixed(buf,s-buf);
      if (PRI <= zlog_default->maxlvl[ZLOG_DEST_SYSLOG])
	syslog_sigsafe(PRI|zlog_default->facility,msgstart,s-msgstart);
    }
#undef DUMP

#undef PRI
#undef LOC
}

/* Log a backtrace using only async-signal-safe functions.
   Needs to be enhanced to support syslog logging. */
void
zlog_backtrace_sigsafe(int priority, void *program_counter)
{
#ifdef HAVE_STACK_TRACE
  static const char pclabel[] = "Program counter: ";
  void *array[64];
  int size;
  char buf[100];
  char *s, **bt = NULL;
#define LOC s,buf+sizeof(buf)-s

#ifdef HAVE_GLIBC_BACKTRACE
  size = backtrace(array, array_size(array));
  if (size <= 0 || (size_t)size > array_size(array))
    return;

#define DUMP(FD) { \
  if (program_counter) \
    { \
      write(FD, pclabel, sizeof(pclabel)-1); \
      backtrace_symbols_fd(&program_counter, 1, FD); \
    } \
  write(FD, buf, s-buf);	\
  backtrace_symbols_fd(array, size, FD); \
}
#elif defined(HAVE_PRINTSTACK)
#define DUMP(FD) { \
  if (program_counter) \
    write((FD), pclabel, sizeof(pclabel)-1); \
  write((FD), buf, s-buf); \
  printstack((FD)); \
}
#endif /* HAVE_GLIBC_BACKTRACE, HAVE_PRINTSTACK */

  s = buf;
  s = str_append(LOC,"Backtrace for ");
  s = num_append(LOC,size);
  s = str_append(LOC," stack frames:\n");

  if ((logfile_fd >= 0) || ((logfile_fd = open_crashlog()) >= 0))
    DUMP(logfile_fd)
  if (!zlog_default)
    DUMP(STDERR_FILENO)
  else
    {
      if (priority <= zlog_default->maxlvl[ZLOG_DEST_STDOUT])
	DUMP(STDOUT_FILENO)
      /* Remove trailing '\n' for monitor and syslog */
      *--s = '\0';
      if (priority <= zlog_default->maxlvl[ZLOG_DEST_MONITOR])
	vty_log_fixed(buf,s-buf);
      if (priority <= zlog_default->maxlvl[ZLOG_DEST_SYSLOG])
	syslog_sigsafe(priority|zlog_default->facility,buf,s-buf);
      {
	int i;
#ifdef HAVE_GLIBC_BACKTRACE
        bt = backtrace_symbols(array, size);
#endif
	/* Just print the function addresses. */
	for (i = 0; i < size; i++)
	  {
	    s = buf;
	    if (bt) 
	      s = str_append(LOC, bt[i]);
	    else {
	      s = str_append(LOC,"[bt ");
	      s = num_append(LOC,i);
	      s = str_append(LOC,"] 0x");
	      s = hex_append(LOC,(u_long)(array[i]));
	    }
	    *s = '\0';
	    if (priority <= zlog_default->maxlvl[ZLOG_DEST_MONITOR])
	      vty_log_fixed(buf,s-buf);
	    if (priority <= zlog_default->maxlvl[ZLOG_DEST_SYSLOG])
	      syslog_sigsafe(priority|zlog_default->facility,buf,s-buf);
	  }
	  if (bt)
	    free(bt);
      }
    }
#undef DUMP
#undef LOC
#endif /* HAVE_STRACK_TRACE */
}

void
zlog_backtrace(int priority)
{
#ifndef HAVE_GLIBC_BACKTRACE
  zlog(NULL, priority, "No backtrace available on this platform.");
#else
  void *array[20];
  int size, i;
  char **strings;

  size = backtrace(array, array_size(array));
  if (size <= 0 || (size_t)size > array_size(array))
    {
      zlog_err("Cannot get backtrace, returned invalid # of frames %d "
	       "(valid range is between 1 and %lu)",
	       size, (unsigned long)(array_size(array)));
      return;
    }
  zlog(NULL, priority, "Backtrace for %d stack frames:", size);
  if (!(strings = backtrace_symbols(array, size)))
    {
      zlog_err("Cannot get backtrace symbols (out of memory?)");
      for (i = 0; i < size; i++)
	zlog(NULL, priority, "[bt %d] %p",i,array[i]);
    }
  else
    {
      for (i = 0; i < size; i++)
	zlog(NULL, priority, "[bt %d] %s",i,strings[i]);
      free(strings);
    }
#endif /* HAVE_GLIBC_BACKTRACE */
}

void
zlog (struct zlog *zl, int priority, const char *format, ...)
{
  va_list args;

  va_start(args, format);
  vzlog (zl, priority, format, args);
  va_end (args);
}

#define ZLOG_FUNC(FUNCNAME,PRIORITY) \
void \
FUNCNAME(const char *format, ...) \
{ \
  va_list args; \
  va_start(args, format); \
  vzlog (NULL, PRIORITY, format, args); \
  va_end(args); \
}

ZLOG_FUNC(zlog_err, LOG_ERR)

ZLOG_FUNC(zlog_warn, LOG_WARNING)

ZLOG_FUNC(zlog_info, LOG_INFO)

ZLOG_FUNC(zlog_notice, LOG_NOTICE)

ZLOG_FUNC(zlog_debug, LOG_DEBUG)

#undef ZLOG_FUNC

#define PLOG_FUNC(FUNCNAME,PRIORITY) \
void \
FUNCNAME(struct zlog *zl, const char *format, ...) \
{ \
  va_list args; \
  va_start(args, format); \
  vzlog (zl, PRIORITY, format, args); \
  va_end(args); \
}

PLOG_FUNC(plog_err, LOG_ERR)

PLOG_FUNC(plog_warn, LOG_WARNING)

PLOG_FUNC(plog_info, LOG_INFO)

PLOG_FUNC(plog_notice, LOG_NOTICE)

PLOG_FUNC(plog_debug, LOG_DEBUG)

#undef PLOG_FUNC

void zlog_thread_info (int log_level)
{
  if (thread_current)
    zlog(NULL, log_level, "Current thread function %s, scheduled from "
	 "file %s, line %u", thread_current->funcname,
	 thread_current->schedfrom, thread_current->schedfrom_line);
  else
    zlog(NULL, log_level, "Current thread not known/applicable");
}

void
_zlog_assert_failed (const char *assertion, const char *file,
		     unsigned int line, const char *function)
{
  /* Force fallback file logging? */
  if (zlog_default && !zlog_default->fp &&
      ((logfile_fd = open_crashlog()) >= 0) &&
      ((zlog_default->fp = fdopen(logfile_fd, "w")) != NULL))
    zlog_default->maxlvl[ZLOG_DEST_FILE] = LOG_ERR;
  zlog(NULL, LOG_CRIT, "Assertion `%s' failed in file %s, line %u, function %s",
       assertion,file,line,(function ? function : "?"));
  zlog_backtrace(LOG_CRIT);
  zlog_thread_info(LOG_CRIT);
  abort();
}


/* Open log stream */
struct zlog *
openzlog (const char *progname, zlog_proto_t protocol,
	  int syslog_flags, int syslog_facility)
{
  struct zlog *zl;
  u_int i;

  zl = XCALLOC(MTYPE_ZLOG, sizeof (struct zlog));

  zl->ident = progname;
  zl->protocol = protocol;
  zl->facility = syslog_facility;
  zl->syslog_options = syslog_flags;

  /* Set default logging levels. */
  for (i = 0; i < array_size(zl->maxlvl); i++)
    zl->maxlvl[i] = ZLOG_DISABLED;
  zl->maxlvl[ZLOG_DEST_MONITOR] = LOG_DEBUG;
  zl->default_lvl = LOG_DEBUG;

  openlog (progname, syslog_flags, zl->facility);
  
  return zl;
}

void
closezlog (struct zlog *zl)
{
  closelog();

  if (zl->fp != NULL)
    fclose (zl->fp);

  if (zl->filename != NULL)
    free (zl->filename);

  XFREE (MTYPE_ZLOG, zl);
}

/* Called from command.c. */
void
zlog_set_level (struct zlog *zl, zlog_dest_t dest, int log_level)
{
  if (zl == NULL)
    zl = zlog_default;

  zl->maxlvl[dest] = log_level;
}

int
zlog_set_file (struct zlog *zl, const char *filename, int log_level)
{
  FILE *fp;
  mode_t oldumask;

  /* There is opend file.  */
  zlog_reset_file (zl);

  /* Set default zl. */
  if (zl == NULL)
    zl = zlog_default;

  /* Open file. */
  oldumask = umask (0777 & ~LOGFILE_MASK);
  fp = fopen (filename, "a");
  umask(oldumask);
  if (fp == NULL)
    return 0;

  /* Set flags. */
  zl->filename = strdup (filename);
  zl->maxlvl[ZLOG_DEST_FILE] = log_level;
  zl->fp = fp;
  logfile_fd = fileno(fp);

  return 1;
}

/* Reset opend file. */
int
zlog_reset_file (struct zlog *zl)
{
  if (zl == NULL)
    zl = zlog_default;

  if (zl->fp)
    fclose (zl->fp);
  zl->fp = NULL;
  logfile_fd = -1;
  zl->maxlvl[ZLOG_DEST_FILE] = ZLOG_DISABLED;

  if (zl->filename)
    free (zl->filename);
  zl->filename = NULL;

  return 1;
}

/* Reopen log file. */
int
zlog_rotate (struct zlog *zl)
{
  int level;

  if (zl == NULL)
    zl = zlog_default;

  if (zl->fp)
    fclose (zl->fp);
  zl->fp = NULL;
  logfile_fd = -1;
  level = zl->maxlvl[ZLOG_DEST_FILE];
  zl->maxlvl[ZLOG_DEST_FILE] = ZLOG_DISABLED;

  if (zl->filename)
    {
      mode_t oldumask;
      int save_errno;

      oldumask = umask (0777 & ~LOGFILE_MASK);
      zl->fp = fopen (zl->filename, "a");
      save_errno = errno;
      umask(oldumask);
      if (zl->fp == NULL)
        {
	  zlog_err("Log rotate failed: cannot open file %s for append: %s",
	  	   zl->filename, safe_strerror(save_errno));
	  return -1;
        }	
      logfile_fd = fileno(zl->fp);
      zl->maxlvl[ZLOG_DEST_FILE] = level;
    }

  return 1;
}

/* Message lookup function. */
const char *
lookup (const struct message *mes, int key)
{
  const struct message *pnt;

  for (pnt = mes; pnt->key != 0; pnt++) 
    if (pnt->key == key) 
      return pnt->str;

  return "";
}

/* Older/faster version of message lookup function, but requires caller to pass
 * in the array size (instead of relying on a 0 key to terminate the search). 
 *
 * The return value is the message string if found, or the 'none' pointer
 * provided otherwise.
 */
const char *
mes_lookup (const struct message *meslist, int max, int index,
  const char *none, const char *mesname)
{
  int pos = index - meslist[0].key;
  
  /* first check for best case: index is in range and matches the key
   * value in that slot.
   * NB: key numbering might be offset from 0. E.g. protocol constants
   * often start at 1.
   */
  if ((pos >= 0) && (pos < max)
      && (meslist[pos].key == index))
    return meslist[pos].str;

  /* fall back to linear search */
  {
    int i;

    for (i = 0; i < max; i++, meslist++)
      {
	if (meslist->key == index)
	  {
	    const char *str = (meslist->str ? meslist->str : none);
	    
	    zlog_debug ("message index %d [%s] found in %s at position %d (max is %d)",
		      index, str, mesname, i, max);
	    return str;
	  }
      }
  }
  zlog_err("message index %d not found in %s (max is %d)", index, mesname, max);
  assert (none);
  return none;
}

/* Wrapper around strerror to handle case where it returns NULL. */
const char *
safe_strerror(int errnum)
{
  const char *s = strerror(errnum);
  return (s != NULL) ? s : "Unknown error";
}

#define DESC_ENTRY(T) [(T)] = { (T), (#T), '\0' }
static const struct zebra_desc_table command_types[] = {
  DESC_ENTRY	(ZEBRA_INTERFACE_ADD),
  DESC_ENTRY	(ZEBRA_INTERFACE_DELETE),
  DESC_ENTRY	(ZEBRA_INTERFACE_ADDRESS_ADD),
  DESC_ENTRY	(ZEBRA_INTERFACE_ADDRESS_DELETE),
  DESC_ENTRY	(ZEBRA_INTERFACE_UP),
  DESC_ENTRY	(ZEBRA_INTERFACE_DOWN),
  DESC_ENTRY	(ZEBRA_IPV4_ROUTE_ADD),
  DESC_ENTRY	(ZEBRA_IPV4_ROUTE_DELETE),
  DESC_ENTRY	(ZEBRA_IPV6_ROUTE_ADD),
  DESC_ENTRY	(ZEBRA_IPV6_ROUTE_DELETE),
  DESC_ENTRY	(ZEBRA_REDISTRIBUTE_ADD),
  DESC_ENTRY	(ZEBRA_REDISTRIBUTE_DELETE),
  DESC_ENTRY	(ZEBRA_REDISTRIBUTE_DEFAULT_ADD),
  DESC_ENTRY	(ZEBRA_REDISTRIBUTE_DEFAULT_DELETE),
  DESC_ENTRY	(ZEBRA_IPV4_NEXTHOP_LOOKUP),
  DESC_ENTRY	(ZEBRA_IPV6_NEXTHOP_LOOKUP),
  DESC_ENTRY	(ZEBRA_IPV4_IMPORT_LOOKUP),
  DESC_ENTRY	(ZEBRA_IPV6_IMPORT_LOOKUP),
  DESC_ENTRY	(ZEBRA_INTERFACE_RENAME),
  DESC_ENTRY	(ZEBRA_ROUTER_ID_ADD),
  DESC_ENTRY	(ZEBRA_ROUTER_ID_DELETE),
  DESC_ENTRY	(ZEBRA_ROUTER_ID_UPDATE),
  DESC_ENTRY	(ZEBRA_HELLO),
};
#undef DESC_ENTRY

static const struct zebra_desc_table unknown = { 0, "unknown", '?' };

static const struct zebra_desc_table *
zroute_lookup(u_int zroute)
{
  u_int i;

  if (zroute >= array_size(route_types))
    {
      zlog_err("unknown zebra route type: %u", zroute);
      return &unknown;
    }
  if (zroute == route_types[zroute].type)
    return &route_types[zroute];
  for (i = 0; i < array_size(route_types); i++)
    {
      if (zroute == route_types[i].type)
        {
	  zlog_warn("internal error: route type table out of order "
		    "while searching for %u, please notify developers", zroute);
	  return &route_types[i];
        }
    }
  zlog_err("internal error: cannot find route type %u in table!", zroute);
  return &unknown;
}

const char *
zebra_route_string(u_int zroute)
{
  return zroute_lookup(zroute)->string;
}

char
zebra_route_char(u_int zroute)
{
  return zroute_lookup(zroute)->chr;
}

const char *
zserv_command_string (unsigned int command)
{
  if (command >= array_size(command_types))
    {
      zlog_err ("unknown zserv command type: %u", command);
      return unknown.string;
    }
  return command_types[command].string;
}

int
proto_name2num(const char *s)
{
   unsigned i;

   for (i=0; i<array_size(route_types); ++i)
     if (strcasecmp(s, route_types[i].string) == 0)
       return route_types[i].type;
   return -1;
}

int
proto_redistnum(int afi, const char *s)
{
  if (! s)
    return -1;

  if (afi == AFI_IP)
    {
      if (strncmp (s, "k", 1) == 0)
	return ZEBRA_ROUTE_KERNEL;
      else if (strncmp (s, "c", 1) == 0)
	return ZEBRA_ROUTE_CONNECT;
      else if (strncmp (s, "s", 1) == 0)
	return ZEBRA_ROUTE_STATIC;
      else if (strncmp (s, "r", 1) == 0)
	return ZEBRA_ROUTE_RIP;
      else if (strncmp (s, "o", 1) == 0)
	return ZEBRA_ROUTE_OSPF;
      else if (strncmp (s, "i", 1) == 0)
	return ZEBRA_ROUTE_ISIS;
      else if (strncmp (s, "bg", 2) == 0)
	return ZEBRA_ROUTE_BGP;
      else if (strncmp (s, "ba", 2) == 0)
	return ZEBRA_ROUTE_BABEL;
    }
  if (afi == AFI_IP6)
    {
      if (strncmp (s, "k", 1) == 0)
	return ZEBRA_ROUTE_KERNEL;
      else if (strncmp (s, "c", 1) == 0)
	return ZEBRA_ROUTE_CONNECT;
      else if (strncmp (s, "s", 1) == 0)
	return ZEBRA_ROUTE_STATIC;
      else if (strncmp (s, "r", 1) == 0)
	return ZEBRA_ROUTE_RIPNG;
      else if (strncmp (s, "o", 1) == 0)
	return ZEBRA_ROUTE_OSPF6;
      else if (strncmp (s, "i", 1) == 0)
	return ZEBRA_ROUTE_ISIS;
      else if (strncmp (s, "bg", 2) == 0)
	return ZEBRA_ROUTE_BGP;
      else if (strncmp (s, "ba", 2) == 0)
	return ZEBRA_ROUTE_BABEL;
    }
  return -1;
}
