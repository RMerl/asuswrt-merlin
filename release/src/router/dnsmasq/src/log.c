/* dnsmasq is Copyright (c) 2000-2017 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
     
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "dnsmasq.h"

#ifdef __ANDROID__
#  include <android/log.h>
#endif

/* Implement logging to /dev/log asynchronously. If syslogd is 
   making DNS lookups through dnsmasq, and dnsmasq blocks awaiting
   syslogd, then the two daemons can deadlock. We get around this
   by not blocking when talking to syslog, instead we queue up to 
   MAX_LOGS messages. If more are queued, they will be dropped,
   and the drop event itself logged. */

/* The "wire" protocol for logging is defined in RFC 3164 */

/* From RFC 3164 */
#define MAX_MESSAGE 1024

/* defaults in case we die() before we log_start() */
static int log_fac = LOG_DAEMON;
static int log_stderr = 0;
static int echo_stderr = 0;
static int log_fd = -1;
static int log_to_file = 0;
static int entries_alloced = 0;
static int entries_lost = 0;
static int connection_good = 1;
static int max_logs = 0;
static int connection_type = SOCK_DGRAM;

struct log_entry {
  int offset, length;
  pid_t pid; /* to avoid duplicates over a fork */
  struct log_entry *next;
  char payload[MAX_MESSAGE];
};

static struct log_entry *entries = NULL;
static struct log_entry *free_entries = NULL;


int log_start(struct passwd *ent_pw, int errfd)
{
  int ret = 0;

  echo_stderr = option_bool(OPT_DEBUG);

  if (daemon->log_fac != -1)
    log_fac = daemon->log_fac;
#ifdef LOG_LOCAL0
  else if (option_bool(OPT_DEBUG))
    log_fac = LOG_LOCAL0;
#endif

  if (daemon->log_file)
    { 
      log_to_file = 1;
      daemon->max_logs = 0;
      if (strcmp(daemon->log_file, "-") == 0)
	{
	  log_stderr = 1;
	  echo_stderr = 0;
	  log_fd = dup(STDERR_FILENO);
	}
    }
  
  max_logs = daemon->max_logs;

  if (!log_reopen(daemon->log_file))
    {
      send_event(errfd, EVENT_LOG_ERR, errno, daemon->log_file ? daemon->log_file : "");
      _exit(0);
    }

  /* if queuing is inhibited, make sure we allocate
     the one required buffer now. */
  if (max_logs == 0)
    {  
      free_entries = safe_malloc(sizeof(struct log_entry));
      free_entries->next = NULL;
      entries_alloced = 1;
    }

  /* If we're running as root and going to change uid later,
     change the ownership here so that the file is always owned by
     the dnsmasq user. Then logrotate can just copy the owner.
     Failure of the chown call is OK, (for instance when started as non-root) */
  if (log_to_file && !log_stderr && ent_pw && ent_pw->pw_uid != 0 && 
      fchown(log_fd, ent_pw->pw_uid, -1) != 0)
    ret = errno;

  return ret;
}

int log_reopen(char *log_file)
{
  if (!log_stderr)
    {      
      if (log_fd != -1)
	close(log_fd);
      
      /* NOTE: umask is set to 022 by the time this gets called */
      
      if (log_file)
	log_fd = open(log_file, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);      
      else
	{
#if defined(HAVE_SOLARIS_NETWORK) || defined(__ANDROID__)
	  /* Solaris logging is "different", /dev/log is not unix-domain socket.
	     Just leave log_fd == -1 and use the vsyslog call for everything.... */
#   define _PATH_LOG ""  /* dummy */
	  return 1;
#else
	  int flags;
	  log_fd = socket(AF_UNIX, connection_type, 0);
	  
	  /* if max_logs is zero, leave the socket blocking */
	  if (log_fd != -1 && max_logs != 0 && (flags = fcntl(log_fd, F_GETFL)) != -1)
	    fcntl(log_fd, F_SETFL, flags | O_NONBLOCK);
#endif
	}
    }
  
  return log_fd != -1;
}

static void free_entry(void)
{
  struct log_entry *tmp = entries;
  entries = tmp->next;
  tmp->next = free_entries;
  free_entries = tmp;
}      

static void log_write(void)
{
  ssize_t rc;
   
  while (entries)
    {
      /* The data in the payload is written with a terminating zero character 
	 and the length reflects this. For a stream connection we need to 
	 send the zero as a record terminator, but this isn't done for a 
	 datagram connection, so treat the length as one less than reality 
	 to elide the zero. If we're logging to a file, turn the zero into 
	 a newline, and leave the length alone. */
      int len_adjust = 0;

      if (log_to_file)
	entries->payload[entries->offset + entries->length - 1] = '\n';
      else if (connection_type == SOCK_DGRAM)
	len_adjust = 1;

      /* Avoid duplicates over a fork() */
      if (entries->pid != getpid())
	{
	  free_entry();
	  continue;
	}

      connection_good = 1;

      if ((rc = write(log_fd, entries->payload + entries->offset, entries->length - len_adjust)) != -1)
	{
	  entries->length -= rc;
	  entries->offset += rc;
	  if (entries->length == len_adjust)
	    {
	      free_entry();
	      if (entries_lost != 0)
		{
		  int e = entries_lost;
		  entries_lost = 0; /* avoid wild recursion */
		  my_syslog(LOG_WARNING, _("overflow: %d log entries lost"), e);
		}	  
	    }
	  continue;
	}
      
      if (errno == EINTR)
	continue;

      if (errno == EAGAIN || errno == EWOULDBLOCK)
	return; /* syslogd busy, go again when select() or poll() says so */
      
      if (errno == ENOBUFS)
	{
	  connection_good = 0;
	  return;
	}

      /* errors handling after this assumes sockets */ 
      if (!log_to_file)
	{
	  /* Once a stream socket hits EPIPE, we have to close and re-open
	     (we ignore SIGPIPE) */
	  if (errno == EPIPE)
	    {
	      if (log_reopen(NULL))
		continue;
	    }
	  else if (errno == ECONNREFUSED || 
		   errno == ENOTCONN || 
		   errno == EDESTADDRREQ || 
		   errno == ECONNRESET)
	    {
	      /* socket went (syslogd down?), try and reconnect. If we fail,
		 stop trying until the next call to my_syslog() 
		 ECONNREFUSED -> connection went down
		 ENOTCONN -> nobody listening
		 (ECONNRESET, EDESTADDRREQ are *BSD equivalents) */
	      
	      struct sockaddr_un logaddr;
	      
#ifdef HAVE_SOCKADDR_SA_LEN
	      logaddr.sun_len = sizeof(logaddr) - sizeof(logaddr.sun_path) + strlen(_PATH_LOG) + 1; 
#endif
	      logaddr.sun_family = AF_UNIX;
	      strncpy(logaddr.sun_path, _PATH_LOG, sizeof(logaddr.sun_path));
	      
	      /* Got connection back? try again. */
	      if (connect(log_fd, (struct sockaddr *)&logaddr, sizeof(logaddr)) != -1)
		continue;
	      
	      /* errors from connect which mean we should keep trying */
	      if (errno == ENOENT || 
		  errno == EALREADY || 
		  errno == ECONNREFUSED ||
		  errno == EISCONN || 
		  errno == EINTR ||
		  errno == EAGAIN || 
		  errno == EWOULDBLOCK)
		{
		  /* try again on next syslog() call */
		  connection_good = 0;
		  return;
		}
	      
	      /* try the other sort of socket... */
	      if (errno == EPROTOTYPE)
		{
		  connection_type = connection_type == SOCK_DGRAM ? SOCK_STREAM : SOCK_DGRAM;
		  if (log_reopen(NULL))
		    continue;
		}
	    }
	}

      /* give up - fall back to syslog() - this handles out-of-space
	 when logging to a file, for instance. */
      log_fd = -1;
      my_syslog(LOG_CRIT, _("log failed: %s"), strerror(errno));
      return;
    }
}

/* priority is one of LOG_DEBUG, LOG_INFO, LOG_NOTICE, etc. See sys/syslog.h.
   OR'd to priority can be MS_TFTP, MS_DHCP, ... to be able to do log separation between
   DNS, DHCP and TFTP services.
*/
void my_syslog(int priority, const char *format, ...)
{
  va_list ap;
  struct log_entry *entry;
  time_t time_now;
  char *p;
  size_t len;
  pid_t pid = getpid();
  char *func = "";

  if ((LOG_FACMASK & priority) == MS_TFTP)
    func = "-tftp";
  else if ((LOG_FACMASK & priority) == MS_DHCP)
    func = "-dhcp";
  else if ((LOG_FACMASK & priority) == MS_SCRIPT)
    func = "-script";
	    
#ifdef LOG_PRI
  priority = LOG_PRI(priority);
#else
  /* Solaris doesn't have LOG_PRI */
  priority &= LOG_PRIMASK;
#endif

  if (echo_stderr) 
    {
      fprintf(stderr, "dnsmasq%s: ", func);
      va_start(ap, format);
      vfprintf(stderr, format, ap);
      va_end(ap);
      fputc('\n', stderr);
    }

  if (log_fd == -1)
    {
#ifdef __ANDROID__
      /* do android-specific logging. 
	 log_fd is always -1 on Android except when logging to a file. */
      int alog_lvl;
      
      if (priority <= LOG_ERR)
	alog_lvl = ANDROID_LOG_ERROR;
      else if (priority == LOG_WARNING)
	alog_lvl = ANDROID_LOG_WARN;
      else if (priority <= LOG_INFO)
	alog_lvl = ANDROID_LOG_INFO;
      else
	alog_lvl = ANDROID_LOG_DEBUG;

      va_start(ap, format);
      __android_log_vprint(alog_lvl, "dnsmasq", format, ap);
      va_end(ap);
#else
      /* fall-back to syslog if we die during startup or 
	 fail during running (always on Solaris). */
      static int isopen = 0;

      if (!isopen)
	{
	  openlog("dnsmasq", LOG_PID, log_fac);
	  isopen = 1;
	}
      va_start(ap, format);  
      vsyslog(priority, format, ap);
      va_end(ap);
#endif

      return;
    }
  
  if ((entry = free_entries))
    free_entries = entry->next;
  else if (entries_alloced < max_logs && (entry = malloc(sizeof(struct log_entry))))
    entries_alloced++;
  
  if (!entry)
    entries_lost++;
  else
    {
      /* add to end of list, consumed from the start */
      entry->next = NULL;
      if (!entries)
	entries = entry;
      else
	{
	  struct log_entry *tmp;
	  for (tmp = entries; tmp->next; tmp = tmp->next);
	  tmp->next = entry;
	}
      
      time(&time_now);
      p = entry->payload;
      if (!log_to_file)
	p += sprintf(p, "<%d>", priority | log_fac);

      /* Omit timestamp for default daemontools situation */
      if (!log_stderr || !option_bool(OPT_NO_FORK)) 
	p += sprintf(p, "%.15s ", ctime(&time_now) + 4);
      
      p += sprintf(p, "dnsmasq%s[%d]: ", func, (int)pid);
        
      len = p - entry->payload;
      va_start(ap, format);  
      len += vsnprintf(p, MAX_MESSAGE - len, format, ap) + 1; /* include zero-terminator */
      va_end(ap);
      entry->length = len > MAX_MESSAGE ? MAX_MESSAGE : len;
      entry->offset = 0;
      entry->pid = pid;
    }
  
  /* almost always, logging won't block, so try and write this now,
     to save collecting too many log messages during a select loop. */
  log_write();
  
  /* Since we're doing things asynchronously, a cache-dump, for instance,
     can now generate log lines very fast. With a small buffer (desirable),
     that means it can overflow the log-buffer very quickly,
     so that the cache dump becomes mainly a count of how many lines 
     overflowed. To avoid this, we delay here, the delay is controlled 
     by queue-occupancy, and grows exponentially. The delay is limited to (2^8)ms.
     The scaling stuff ensures that when the queue is bigger than 8, the delay
     only occurs for the last 8 entries. Once the queue is full, we stop delaying
     to preserve performance.
  */

  if (entries && max_logs != 0)
    {
      int d;
      
      for (d = 0,entry = entries; entry; entry = entry->next, d++);
      
      if (d == max_logs)
	d = 0;
      else if (max_logs > 8)
	d -= max_logs - 8;

      if (d > 0)
	{
	  struct timespec waiter;
	  waiter.tv_sec = 0;
	  waiter.tv_nsec = 1000000 << (d - 1); /* 1 ms */
	  nanosleep(&waiter, NULL);
      
	  /* Have another go now */
	  log_write();
	}
    } 
}

void set_log_writer(void)
{
  if (entries && log_fd != -1 && connection_good)
    poll_listen(log_fd, POLLOUT);
}

void check_log_writer(int force)
{
  if (log_fd != -1 && (force || poll_check(log_fd, POLLOUT)))
    log_write();
}

void flush_log(void)
{
  /* write until queue empty, but don't loop forever if there's
   no connection to the syslog in existence */
  while (log_fd != -1)
    {
      struct timespec waiter;
      log_write();
      if (!entries || !connection_good)
	{
	  close(log_fd);	
	  break;
	}
      waiter.tv_sec = 0;
      waiter.tv_nsec = 1000000; /* 1 ms */
      nanosleep(&waiter, NULL);
    }
}

void die(char *message, char *arg1, int exit_code)
{
  char *errmess = strerror(errno);
  
  if (!arg1)
    arg1 = errmess;

  if (!log_stderr)
    {
      echo_stderr = 1; /* print as well as log when we die.... */
      fputc('\n', stderr); /* prettyfy  startup-script message */
    }
  my_syslog(LOG_CRIT, message, arg1, errmess);
  echo_stderr = 0;
  my_syslog(LOG_CRIT, _("FAILED to start up"));
  flush_log();
  
  exit(exit_code);
}
