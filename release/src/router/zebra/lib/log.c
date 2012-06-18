/* Logging of zebra
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

#include <zebra.h>

#include "log.h"
#include "memory.h"
#include "command.h"

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
  "OSPF6",
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
#define TIME_BUF 27

/* Utility routine for current time printing. */
static void
time_print (FILE *fp)
{
  int ret;
  char buf [TIME_BUF];
  time_t clock;
  struct tm *tm;
  
  time (&clock);
  tm = localtime (&clock);

  ret = strftime (buf, TIME_BUF, "%Y/%m/%d %H:%M:%S", tm);
  if (ret == 0) {
    zlog_warn ("strftime error");
  }

  fprintf (fp, "%s ", buf);
}

/* va_list version of zlog. */
void
vzlog (struct zlog *zl, int priority, const char *format, va_list *args)
{
  /* If zlog is not specified, use default one. */
  if (zl == NULL)
    zl = zlog_default;

  /* When zlog_default is also NULL, use stderr for logging. */
  if (zl == NULL)
    {
      time_print (stderr);
      fprintf (stderr, "%s: ", "unknown");
      vfprintf (stderr, format, args[ZLOG_NOLOG_INDEX]);
      fprintf (stderr, "\n");
      fflush (stderr);

      /* In this case we return at here. */
      return;
    }

  /* only log this information if it has not been masked out */
  if ( priority > zl->maskpri )
    return ;
		
  /* Syslog output */
  if (zl->flags & ZLOG_SYSLOG)
    vsyslog (priority|zlog_default->facility, format, args[ZLOG_SYSLOG_INDEX]);

  /* File output. */
  if (zl->flags & ZLOG_FILE)
    {
      time_print (zl->fp);
      if (zl->record_priority) fprintf (zl->fp, "%s: ", zlog_priority[priority]);
      fprintf (zl->fp, "%s: ", zlog_proto_names[zl->protocol]);
      vfprintf (zl->fp, format, args[ZLOG_FILE_INDEX]);
      fprintf (zl->fp, "\n");
      fflush (zl->fp);
    }

  /* stdout output. */
  if (zl->flags & ZLOG_STDOUT)
    {
      time_print (stdout);
      if (zl->record_priority) fprintf (stdout, "%s: ", zlog_priority[priority]);
      fprintf (stdout, "%s: ", zlog_proto_names[zl->protocol]);
      vfprintf (stdout, format, args[ZLOG_STDOUT_INDEX]);
      fprintf (stdout, "\n");
      fflush (stdout);
    }

  /* stderr output. */
  if (zl->flags & ZLOG_STDERR)
    {
      time_print (stderr);
      if (zl->record_priority) fprintf (stderr, "%s: ", zlog_priority[priority]);
      fprintf (stderr, "%s: ", zlog_proto_names[zl->protocol]);
      vfprintf (stderr, format, args[ZLOG_STDERR_INDEX]);
      fprintf (stderr, "\n");
      fflush (stderr);
    }

  /* Terminal monitor. */
  vty_log (zlog_proto_names[zl->protocol], format, args[ZLOG_NOLOG_INDEX]);
}

void
zlog (struct zlog *zl, int priority, const char *format, ...)
{
  va_list args[ZLOG_MAX_INDEX];
  int index;

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_start(args[index], format);

  vzlog (zl, priority, format, args);

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_end (args[index]);
}

void
zlog_err (const char *format, ...)
{
  va_list args[ZLOG_MAX_INDEX];
  int index;

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_start(args[index], format);

  vzlog (NULL, LOG_ERR, format, args);

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_end (args[index]);
}

void
zlog_warn (const char *format, ...)
{
  va_list args[ZLOG_MAX_INDEX];
  int index;

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_start(args[index], format);

  vzlog (NULL, LOG_WARNING, format, args);

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_end (args[index]);
}

void
zlog_info (const char *format, ...)
{
  va_list args[ZLOG_MAX_INDEX];
  int index;

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_start(args[index], format);

  vzlog (NULL, LOG_INFO, format, args);

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_end (args[index]);
}

void
zlog_notice (const char *format, ...)
{
  va_list args[ZLOG_MAX_INDEX];
  int index;

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_start(args[index], format);

  vzlog (NULL, LOG_NOTICE, format, args);

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_end (args[index]);
}

void
zlog_debug (const char *format, ...)
{
  va_list args[ZLOG_MAX_INDEX];
  int index;

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_start(args[index], format);

  vzlog (NULL, LOG_DEBUG, format, args);

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_end (args[index]);
}

void
plog_err (struct zlog *zl, const char *format, ...)
{
  va_list args[ZLOG_MAX_INDEX];
  int index;

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_start(args[index], format);

  vzlog (zl, LOG_ERR, format, args);

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_end (args[index]);
}

void
plog_warn (struct zlog *zl, const char *format, ...)
{
  va_list args[ZLOG_MAX_INDEX];
  int index;

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_start(args[index], format);

  vzlog (zl, LOG_WARNING, format, args);

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_end (args[index]);
}

void
plog_info (struct zlog *zl, const char *format, ...)
{
  va_list args[ZLOG_MAX_INDEX];
  int index;

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_start(args[index], format);

  vzlog (zl, LOG_INFO, format, args);

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_end (args[index]);
}

void
plog_notice (struct zlog *zl, const char *format, ...)
{
  va_list args[ZLOG_MAX_INDEX];
  int index;

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_start(args[index], format);

  vzlog (zl, LOG_NOTICE, format, args);

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_end (args[index]);
}

void
plog_debug (struct zlog *zl, const char *format, ...)
{
  va_list args[ZLOG_MAX_INDEX];
  int index;

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_start(args[index], format);

  vzlog (zl, LOG_DEBUG, format, args);

  for (index = 0; index < ZLOG_MAX_INDEX; index++)
    va_end (args[index]);
}


/* Open log stream */
struct zlog *
openzlog (const char *progname, int flags, zlog_proto_t protocol,
	  int syslog_flags, int syslog_facility)
{
  struct zlog *zl;

  zl = XMALLOC(MTYPE_ZLOG, sizeof (struct zlog));
  memset (zl, 0, sizeof (struct zlog));

  zl->ident = progname;
  zl->flags = flags;
  zl->protocol = protocol;
  zl->facility = syslog_facility;
  zl->maskpri = LOG_DEBUG;
  zl->record_priority = 0;

  openlog (progname, syslog_flags, zl->facility);
  
  return zl;
}

void
closezlog (struct zlog *zl)
{
  closelog();
  fclose (zl->fp);

  XFREE (MTYPE_ZLOG, zl);
}

/* Called from command.c. */
void
zlog_set_flag (struct zlog *zl, int flags)
{
  if (zl == NULL)
    zl = zlog_default;

  zl->flags |= flags;
}

void
zlog_reset_flag (struct zlog *zl, int flags)
{
  if (zl == NULL)
    zl = zlog_default;

  zl->flags &= ~flags;
}

int
zlog_set_file (struct zlog *zl, int flags, char *filename)
{
  FILE *fp;

  /* There is opend file.  */
  zlog_reset_file (zl);

  /* Set default zl. */
  if (zl == NULL)
    zl = zlog_default;

  /* Open file. */
  fp = fopen (filename, "a");
  if (fp == NULL)
    return 0;

  /* Set flags. */
  zl->filename = strdup (filename);
  zl->flags |= ZLOG_FILE;
  zl->fp = fp;

  return 1;
}

/* Reset opend file. */
int
zlog_reset_file (struct zlog *zl)
{
  if (zl == NULL)
    zl = zlog_default;

  zl->flags &= ~ZLOG_FILE;

  if (zl->fp)
    fclose (zl->fp);
  zl->fp = NULL;

  if (zl->filename)
    free (zl->filename);
  zl->filename = NULL;

  return 1;
}

/* Reopen log file. */
int
zlog_rotate (struct zlog *zl)
{
  FILE *fp;

  if (zl == NULL)
    zl = zlog_default;

  if (zl->fp)
    fclose (zl->fp);
  zl->fp = NULL;

  if (zl->filename)
    {
      fp = fopen (zl->filename, "a");
      if (fp == NULL)
	return -1;
      zl->fp = fp;
    }

  return 1;
}

static char *zlog_cwd = NULL;

void
zlog_save_cwd ()
{
  char *cwd;

  cwd = getcwd (NULL, MAXPATHLEN);

  zlog_cwd = XMALLOC (MTYPE_TMP, strlen (cwd) + 1);
  strcpy (zlog_cwd, cwd);
}

char *
zlog_get_cwd ()
{
  return zlog_cwd;
}

void
zlog_free_cwd ()
{
  if (zlog_cwd)
    XFREE (MTYPE_TMP, zlog_cwd);
}

/* Message lookup function. */
char *
lookup (struct message *mes, int key)
{
  struct message *pnt;

  for (pnt = mes; pnt->key != 0; pnt++) 
    if (pnt->key == key) 
      return pnt->str;

  return "";
}

/* Very old hacky version of message lookup function.  Still partly
   used in bgpd and ospfd. */
char *
mes_lookup (struct message *meslist, int max, int index)
{
  if (index < 0 || index >= max) 
    {
      zlog_err ("message index out of bound: %d", max);
      return NULL;
    }
  return meslist[index].str;
}
