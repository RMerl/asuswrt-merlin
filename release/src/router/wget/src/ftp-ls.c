/* Parsing FTP `ls' output.
   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2008, 2009, 2010, 2011 Free Software Foundation,
   Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget.  If not, see <http://www.gnu.org/licenses/>.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.  */

#include "wget.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "utils.h"
#include "ftp.h"
#include "url.h"
#include "convert.h"            /* for html_quote_string prototype */
#include "retr.h"               /* for output_stream */

/* Converts symbolic permissions to number-style ones, e.g. string
   rwxr-xr-x to 755.  For now, it knows nothing of
   setuid/setgid/sticky.  ACLs are ignored.  */
static int
symperms (const char *s)
{
  int perms = 0, i;

  if (strlen (s) < 9)
    return 0;
  for (i = 0; i < 3; i++, s += 3)
    {
      perms <<= 3;
      perms += (((s[0] == 'r') << 2) + ((s[1] == 'w') << 1) +
                (s[2] == 'x' || s[2] == 's'));
    }
  return perms;
}


/* Cleans a line of text so that it can be consistently parsed. Destroys
   <CR> and <LF> in case that thay occur at the end of the line and
   replaces all <TAB> character with <SPACE>. Returns the length of the
   modified line. */
static int
clean_line (char *line, int len)
{
  if (len <= 0) return 0;

  while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
    line[--len] = '\0';

  if (!len) return 0;

  for ( ; *line ; line++ ) if (*line == '\t') *line = ' ';

  return len;
}

/* Convert the Un*x-ish style directory listing stored in FILE to a
   linked list of fileinfo (system-independent) entries.  The contents
   of FILE are considered to be produced by the standard Unix `ls -la'
   output (whatever that might be).  BSD (no group) and SYSV (with
   group) listings are handled.

   The time stamps are stored in a separate variable, time_t
   compatible (I hope).  The timezones are ignored.  */
static struct fileinfo *
ftp_parse_unix_ls (const char *file, int ignore_perms)
{
  FILE *fp;
  static const char *months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  int next, len, i, error, ignore;
  int year, month, day;         /* for time analysis */
  int hour, min, sec, ptype;
  struct tm timestruct, *tnow;
  time_t timenow;
  size_t bufsize = 0;

  char *line = NULL, *tok, *ptok;      /* tokenizer */
  struct fileinfo *dir, *l, cur;       /* list creation */

  fp = fopen (file, "rb");
  if (!fp)
    {
      logprintf (LOG_NOTQUIET, "%s: %s\n", file, strerror (errno));
      return NULL;
    }
  dir = l = NULL;

  /* Line loop to end of file: */
  while ((len = getline (&line, &bufsize, fp)) > 0)
    {
      len = clean_line (line, len);
      /* Skip if total...  */
      if (!strncasecmp (line, "total", 5))
        continue;
      /* Get the first token (permissions).  */
      tok = strtok (line, " ");
      if (!tok)
        continue;

      cur.name = NULL;
      cur.linkto = NULL;

      /* Decide whether we deal with a file or a directory.  */
      switch (*tok)
        {
        case '-':
          cur.type = FT_PLAINFILE;
          DEBUGP (("PLAINFILE; "));
          break;
        case 'd':
          cur.type = FT_DIRECTORY;
          DEBUGP (("DIRECTORY; "));
          break;
        case 'l':
          cur.type = FT_SYMLINK;
          DEBUGP (("SYMLINK; "));
          break;
        default:
          cur.type = FT_UNKNOWN;
          DEBUGP (("UNKNOWN; "));
          break;
        }

      if (ignore_perms)
        {
          switch (cur.type)
            {
            case FT_PLAINFILE:
              cur.perms = 0644;
              break;
            case FT_DIRECTORY:
              cur.perms = 0755;
              break;
            default:
              /*cur.perms = 1023;*/     /* #### What is this?  --hniksic */
              cur.perms = 0644;
            }
          DEBUGP (("implicit perms %0o; ", cur.perms));
        }
       else
         {
           cur.perms = symperms (tok + 1);
           DEBUGP (("perms %0o; ", cur.perms));
         }

      error = ignore = 0;       /* Erroneous and ignoring entries are
                                   treated equally for now.  */
      year = hour = min = sec = 0; /* Silence the compiler.  */
      month = day = 0;
      ptype = TT_DAY;
      next = -1;
      /* While there are tokens on the line, parse them.  Next is the
         number of tokens left until the filename.

         Use the month-name token as the "anchor" (the place where the
         position wrt the file name is "known").  When a month name is
         encountered, `next' is set to 5.  Also, the preceding
         characters are parsed to get the file size.

         This tactic is quite dubious when it comes to
         internationalization issues (non-English month names), but it
         works for now.  */
      tok = line;
      while (ptok = tok,
             (tok = strtok (NULL, " ")) != NULL)
        {
          --next;
          if (next < 0)         /* a month name was not encountered */
            {
              for (i = 0; i < 12; i++)
                if (!strcasecmp (tok, months[i]))
                  break;
              /* If we got a month, it means the token before it is the
                 size, and the filename is three tokens away.  */
              if (i != 12)
                {
                  wgint size;

                  /* Parse the previous token with str_to_wgint.  */
                  if (ptok == line)
                    {
                      /* Something has gone wrong during parsing. */
                      error = 1;
                      break;
                    }
                  errno = 0;
                  size = str_to_wgint (ptok, NULL, 10);
                  if (size == WGINT_MAX && errno == ERANGE)
                    /* Out of range -- ignore the size.  #### Should
                       we refuse to start the download.  */
                    cur.size = 0;
                  else
                    cur.size = size;
                  DEBUGP (("size: %s; ", number_to_static_string(cur.size)));

                  month = i;
                  next = 5;
                  DEBUGP (("month: %s; ", months[month]));
                }
            }
          else if (next == 4)   /* days */
            {
              if (tok[1])       /* two-digit... */
                day = 10 * (*tok - '0') + tok[1] - '0';
              else              /* ...or one-digit */
                day = *tok - '0';
              DEBUGP (("day: %d; ", day));
            }
          else if (next == 3)
            {
              /* This ought to be either the time, or the year.  Let's
                 be flexible!

                 If we have a number x, it's a year.  If we have x:y,
                 it's hours and minutes.  If we have x:y:z, z are
                 seconds.  */
              year = 0;
              min = hour = sec = 0;
              /* We must deal with digits.  */
              if (c_isdigit (*tok))
                {
                  /* Suppose it's year.  */
                  for (; c_isdigit (*tok); tok++)
                    year = (*tok - '0') + 10 * year;
                  if (*tok == ':')
                    {
                      /* This means these were hours!  */
                      hour = year;
                      year = 0;
                      ptype = TT_HOUR_MIN;
                      ++tok;
                      /* Get the minutes...  */
                      for (; c_isdigit (*tok); tok++)
                        min = (*tok - '0') + 10 * min;
                      if (*tok == ':')
                        {
                          /* ...and the seconds.  */
                          ++tok;
                          for (; c_isdigit (*tok); tok++)
                            sec = (*tok - '0') + 10 * sec;
                        }
                    }
                }
              if (year)
                DEBUGP (("year: %d (no tm); ", year));
              else
                DEBUGP (("time: %02d:%02d:%02d (no yr); ", hour, min, sec));
            }
          else if (next == 2)    /* The file name */
            {
              int fnlen;
              char *p;

              /* Since the file name may contain a SPC, it is possible
                 for strtok to handle it wrong.  */
              fnlen = strlen (tok);
              if (fnlen < len - (tok - line))
                {
                  /* So we have a SPC in the file name.  Restore the
                     original.  */
                  tok[fnlen] = ' ';
                  /* If the file is a symbolic link, it should have a
                     ` -> ' somewhere.  */
                  if (cur.type == FT_SYMLINK)
                    {
                      p = strstr (tok, " -> ");
                      if (!p)
                        {
                          error = 1;
                          break;
                        }
                      cur.linkto = xstrdup (p + 4);
                      DEBUGP (("link to: %s\n", cur.linkto));
                      /* And separate it from the file name.  */
                      *p = '\0';
                    }
                }
              /* If we have the filename, add it to the list of files or
                 directories.  */
              /* "." and ".." are an exception!  */
              if (!strcmp (tok, ".") || !strcmp (tok, ".."))
                {
                  DEBUGP (("\nIgnoring `.' and `..'; "));
                  ignore = 1;
                  break;
                }
              /* Some FTP sites choose to have ls -F as their default
                 LIST output, which marks the symlinks with a trailing
                 `@', directory names with a trailing `/' and
                 executables with a trailing `*'.  This is no problem
                 unless encountering a symbolic link ending with `@',
                 or an executable ending with `*' on a server without
                 default -F output.  I believe these cases are very
                 rare.  */
              fnlen = strlen (tok); /* re-calculate `fnlen' */
              cur.name = xmalloc (fnlen + 1);
              memcpy (cur.name, tok, fnlen + 1);
              if (fnlen)
                {
                  if (cur.type == FT_DIRECTORY && cur.name[fnlen - 1] == '/')
                    {
                      cur.name[fnlen - 1] = '\0';
                      DEBUGP (("trailing `/' on dir.\n"));
                    }
                  else if (cur.type == FT_SYMLINK && cur.name[fnlen - 1] == '@')
                    {
                      cur.name[fnlen - 1] = '\0';
                      DEBUGP (("trailing `@' on link.\n"));
                    }
                  else if (cur.type == FT_PLAINFILE
                           && (cur.perms & 0111)
                           && cur.name[fnlen - 1] == '*')
                    {
                      cur.name[fnlen - 1] = '\0';
                      DEBUGP (("trailing `*' on exec.\n"));
                    }
                } /* if (fnlen) */
              else
                error = 1;
              break;
            }
          else
            abort ();
        } /* while */

      if (!cur.name || (cur.type == FT_SYMLINK && !cur.linkto))
        error = 1;

      DEBUGP (("%s\n", cur.name ? cur.name : ""));

      if (error || ignore)
        {
          DEBUGP (("Skipping.\n"));
          xfree_null (cur.name);
          xfree_null (cur.linkto);
          continue;
        }

      if (!dir)
        {
          l = dir = xnew (struct fileinfo);
          memcpy (l, &cur, sizeof (cur));
          l->prev = l->next = NULL;
        }
      else
        {
          cur.prev = l;
          l->next = xnew (struct fileinfo);
          l = l->next;
          memcpy (l, &cur, sizeof (cur));
          l->next = NULL;
        }
      /* Get the current time.  */
      timenow = time (NULL);
      tnow = localtime (&timenow);
      /* Build the time-stamp (the idea by zaga@fly.cc.fer.hr).  */
      timestruct.tm_sec   = sec;
      timestruct.tm_min   = min;
      timestruct.tm_hour  = hour;
      timestruct.tm_mday  = day;
      timestruct.tm_mon   = month;
      if (year == 0)
        {
          /* Some listings will not specify the year if it is "obvious"
             that the file was from the previous year.  E.g. if today
             is 97-01-12, and you see a file of Dec 15th, its year is
             1996, not 1997.  Thanks to Vladimir Volovich for
             mentioning this!  */
          if (month > tnow->tm_mon)
            timestruct.tm_year = tnow->tm_year - 1;
          else
            timestruct.tm_year = tnow->tm_year;
        }
      else
        timestruct.tm_year = year;
      if (timestruct.tm_year >= 1900)
        timestruct.tm_year -= 1900;
      timestruct.tm_wday  = 0;
      timestruct.tm_yday  = 0;
      timestruct.tm_isdst = -1;
      l->tstamp = mktime (&timestruct); /* store the time-stamp */
      l->ptype = ptype;
    }

  xfree (line);
  fclose (fp);
  return dir;
}

static struct fileinfo *
ftp_parse_winnt_ls (const char *file)
{
  FILE *fp;
  int len;
  int year, month, day;         /* for time analysis */
  int hour, min;
  size_t bufsize = 0;
  struct tm timestruct;

  char *line = NULL, *tok;             /* tokenizer */
  char *filename;
  struct fileinfo *dir, *l, cur; /* list creation */

  fp = fopen (file, "rb");
  if (!fp)
    {
      logprintf (LOG_NOTQUIET, "%s: %s\n", file, strerror (errno));
      return NULL;
    }
  dir = l = NULL;

  /* Line loop to end of file: */
  while ((len = getline (&line, &bufsize, fp)) > 0)
    {
      len = clean_line (line, len);

      /* Name begins at 39 column of the listing if date presented in `mm-dd-yy'
         format or at 41 column if date presented in `mm-dd-yyyy' format. Thus,
         we cannot extract name before we parse date. Using this information we
         also can recognize filenames that begin with a series of space
         characters (but who really wants to use such filenames anyway?). */
      if (len < 40) continue;
      filename = line + 39;

      /* First column: mm-dd-yy or mm-dd-yyyy. Should atoi() on the month fail,
         january will be assumed.  */
      tok = strtok(line, "-");
      if (tok == NULL) continue;
      month = atoi(tok) - 1;
      if (month < 0) month = 0;
      tok = strtok(NULL, "-");
      if (tok == NULL) continue;
      day = atoi(tok);
      tok = strtok(NULL, " ");
      if (tok == NULL) continue;
      year = atoi(tok);
      /* Assuming the epoch starting at 1.1.1970 */
      if (year <= 70)
        {
          year += 100;
        }
      else if (year >= 1900)
        {
          year -= 1900;
          filename += 2;
        }
      /* Now it is possible to determine the position of the first symbol in
         filename. */
      cur.name = xstrdup(filename);
      DEBUGP (("Name: '%s'\n", cur.name));


      /* Second column: hh:mm[AP]M, listing does not contain value for
         seconds */
      tok = strtok(NULL,  ":");
      if (tok == NULL) continue;
      hour = atoi(tok);
      tok = strtok(NULL,  "M");
      if (tok == NULL) continue;
      min = atoi(tok);
      /* Adjust hour from AM/PM. Just for the record, the sequence goes
         11:00AM, 12:00PM, 01:00PM ... 11:00PM, 12:00AM, 01:00AM . */
      tok+=2;
      if (hour == 12)  hour  = 0;
      if (*tok == 'P') hour += 12;

      DEBUGP (("YYYY/MM/DD HH:MM - %d/%02d/%02d %02d:%02d\n",
              year+1900, month, day, hour, min));

      /* Build the time-stamp (copy & paste from above) */
      timestruct.tm_sec   = 0;
      timestruct.tm_min   = min;
      timestruct.tm_hour  = hour;
      timestruct.tm_mday  = day;
      timestruct.tm_mon   = month;
      timestruct.tm_year  = year;
      timestruct.tm_wday  = 0;
      timestruct.tm_yday  = 0;
      timestruct.tm_isdst = -1;
      cur.tstamp = mktime (&timestruct); /* store the time-stamp */
      cur.ptype = TT_HOUR_MIN;

      DEBUGP (("Timestamp: %ld\n", cur.tstamp));

      /* Third column: Either file length, or <DIR>. We also set the
         permissions (guessed as 0644 for plain files and 0755 for
         directories as the listing does not give us a clue) and filetype
         here. */
      tok = strtok(NULL, " ");
      if (tok == NULL) continue;
      while ((tok != NULL) && (*tok == '\0'))  tok = strtok(NULL, " ");
      if (tok == NULL) continue;
      if (*tok == '<')
        {
          cur.type  = FT_DIRECTORY;
          cur.size  = 0;
          cur.perms = 0755;
          DEBUGP (("Directory\n"));
        }
      else
        {
          wgint size;
          cur.type  = FT_PLAINFILE;
          errno = 0;
          size = str_to_wgint (tok, NULL, 10);
          if (size == WGINT_MAX && errno == ERANGE)
            cur.size = 0;       /* overflow */
          else
            cur.size = size;
          cur.perms = 0644;
          DEBUGP (("File, size %s bytes\n", number_to_static_string (cur.size)));
        }

      cur.linkto = NULL;

      /* And put everything into the linked list */
      if (!dir)
        {
          l = dir = xnew (struct fileinfo);
          memcpy (l, &cur, sizeof (cur));
          l->prev = l->next = NULL;
        }
      else
        {
          cur.prev = l;
          l->next = xnew (struct fileinfo);
          l = l->next;
          memcpy (l, &cur, sizeof (cur));
          l->next = NULL;
        }
    }

  xfree (line);
  fclose(fp);
  return dir;
}



/* Convert the VMS-style directory listing stored in "file" to a
   linked list of fileinfo (system-independent) entries.  The contents
   of FILE are considered to be produced by the standard VMS
   "DIRECTORY [/SIZE [= ALL]] /DATE [/OWNER] [/PROTECTION]" command,
   more or less.  (Different VMS FTP servers may have different headers,
   and may not supply the same data, but all should be subsets of this.)

   VMS normally provides local (server) time and date information.
   Define the logical name or environment variable
   "WGET_TIMEZONE_DIFFERENTIAL" (seconds) to adjust the receiving local
   times if different from the remote local times.

   2005-02-23 SMS.
   Added code to eliminate "^" escape characters from ODS5 extended file
   names.  The TCPIP FTP server (V5.4) seems to prefer requests which do
   not use the escaped names which it provides.
*/

#define VMS_DEFAULT_PROT_FILE 0644
#define VMS_DEFAULT_PROT_DIR 0755

/* 2005-02-23 SMS.
   eat_carets().

   Delete ODS5 extended file name escape characters ("^") in the
   original buffer.
   Note that the current scheme does not handle all EFN cases, but it
   could be made more complicated.
*/

static void eat_carets( char *str)
/* char *str;      Source pointer. */
{
  char *strd;   /* Destination pointer. */
  char hdgt;
  unsigned char uchr;
  unsigned char prop;

  /* Skip ahead to the first "^", if any. */
  while ((*str != '\0') && (*str != '^'))
     str++;

  /* If no caret was found, quit early. */
  if (*str != '\0')
  {
    /* Shift characters leftward as carets are found. */
    strd = str;
    while (*str != '\0')
    {
      uchr = *str;
      if (uchr == '^')
      {
        /* Found a caret.  Skip it, and check the next character. */
        uchr = *(++str);
        prop = char_prop[ uchr];
        if (prop& 64)
        {
          /* Hex digit.  Get char code from this and next hex digit. */
          if (uchr <= '9')
          {
            hdgt = uchr- '0';           /* '0' - '9' -> 0 - 9. */
          }
          else
          {
            hdgt = ((uchr- 'A')& 7)+ 10;    /* [Aa] - [Ff] -> 10 - 15. */
          }
          hdgt <<= 4;                   /* X16. */
          uchr = *(++str);              /* Next char must be hex digit. */
          if (uchr <= '9')
          {
            uchr = hdgt+ uchr- '0';
          }
          else
          {
            uchr = hdgt+ ((uchr- 'A')& 15)+ 10;
          }
        }
        else if (uchr == '_')
        {
          /* Convert escaped "_" to " ". */
          uchr = ' ';
        }
        else if (uchr == '/')
        {
          /* Convert escaped "/" (invalid Zip) to "?" (invalid VMS). */
          /* Note that this is a left-over from Info-ZIP code, and is
             probably of little value here, except perhaps to avoid
             directory confusion which an unconverted slash might cause.
          */
          uchr = '?';
        }
        /* Else, not a hex digit.  Must be a simple escaped character
           (or Unicode, which is not yet handled here).
        */
      }
      /* Else, not a caret.  Use as-is. */
      *strd = uchr;

      /* Advance destination and source pointers. */
      strd++;
      str++;
    }
    /* Terminate the destination string. */
    *strd = '\0';
  }
}


static struct fileinfo *
ftp_parse_vms_ls (const char *file)
{
  FILE *fp;
  int dt, i, j, len;
  int perms;
  size_t bufsize = 0;
  time_t timenow;
  struct tm *timestruct;
  char date_str[ 32];

  char *line = NULL, *tok; /* tokenizer */
  struct fileinfo *dir, *l, cur; /* list creation */

  fp = fopen (file, "r");
  if (!fp)
    {
      logprintf (LOG_NOTQUIET, "%s: %s\n", file, strerror (errno));
      return NULL;
    }
  dir = l = NULL;

  /* Skip blank lines, Directory heading, and more blank lines. */

  for (j = 0; (i = getline (&line, &bufsize, fp)) > 0; )
    {
      i = clean_line (line, i);
      if (i <= 0)
        continue; /* Ignore blank line. */

      if ((j == 0) && (line[i - 1] == ']'))
        {
          /* Found Directory heading line.  Next non-blank line
          is significant. */
          j = 1;
        }
      else if (!strncmp (line, "Total of ", 9))
        {
          /* Found "Total of ..." footing line.  No valid data
             will follow (empty directory). */
          i = 0; /* Arrange for early exit. */
          break;
        }
      else
        {
          break; /* Must be significant data. */
        }
    }

  /* Read remainder of file until the next blank line or EOF. */

  while (i > 0)
    {
      char *p;

      /* The first token is the file name.  After a long name, other
         data may be on the following line.  A valid directory name ends
         in ".DIR;1" (any case), although some VMS FTP servers may omit
         the version number (";1").
      */

      tok = strtok(line, " ");
      if (tok == NULL) tok = line;
      DEBUGP (("file name:   '%s'\n", tok));

      /* Stripping the version number on a VMS system would be wrong.
         It may be foolish on a non-VMS system, too, but that's someone
         else's problem.  (Define PRESERVE_VMS_VERSIONS for proper
         operation on other operating systems.)

         2005-02-23 SMS.
         ODS5 extended file names may contain escaped semi-colons, so
         the version number is identified as right-side decimal digits
         led by a non-escaped semi-colon.  It may be absent.
      */

#if (!defined( __VMS) && !defined( PRESERVE_VMS_VERSIONS))
      for (p = tok + strlen (tok); (--p > tok) && c_isdigit( *p); );
      if ((*p == ';') && (*(p- 1) != '^'))
        {
          *p = '\0';
        }
#endif /* (!defined( __VMS) && !defined( PRESERVE_VMS_VERSIONS)) */

      /* 2005-02-23 SMS.
         Eliminate "^" escape characters from ODS5 extended file name.
         (A caret is invalid in an ODS2 name, so this is always safe.)
      */
      eat_carets (tok);
      DEBUGP (("file name-^: '%s'\n", tok));

      /* Differentiate between a directory and any other file.  A VMS
         listing may not include file protections (permissions).  Set a
         default permissions value (according to the file type), which
         may be overwritten later.  Store directory names without the
         ".DIR;1" file type and version number, as the plain name is
         what will work in a CWD command.
      */
      len = strlen (tok);
      if (!strncasecmp((tok + (len - 4)), ".DIR", 4))
        {
          *(tok+ (len - 4)) = '\0'; /* Discard ".DIR". */
          cur.type  = FT_DIRECTORY;
          cur.perms = VMS_DEFAULT_PROT_DIR;
          DEBUGP (("Directory (nv)\n"));
        }
      else if (!strncasecmp ((tok + (len - 6)), ".DIR;1", 6))
        {
          *(tok+ (len - 6)) = '\0'; /* Discard ".DIR;1". */
          cur.type  = FT_DIRECTORY;
          cur.perms = VMS_DEFAULT_PROT_DIR;
          DEBUGP (("Directory (v)\n"));
        }
      else
        {
          cur.type  = FT_PLAINFILE;
          cur.perms = VMS_DEFAULT_PROT_FILE;
          DEBUGP (("File\n"));
        }
      cur.name = xstrdup (tok);
      DEBUGP (("Name: '%s'\n", cur.name));

      /* Null the date and time string. */
      *date_str = '\0';

      /* VMS lacks symbolic links. */
      cur.linkto = NULL;

      /* VMS reports file sizes in (512-byte) disk blocks, not bytes,
         hence useless for an integrity check based on byte-count.
         Set size to unknown.
      */
      cur.size = 0;

      /* Get token 2, if any.  A long name may force all other data onto
         a second line.  If needed, read the second line.
      */

      tok = strtok (NULL, " ");
      if (tok == NULL)
        {
          DEBUGP (("Getting additional line.\n"));
          i = getline (&line, &bufsize, fp);
          if (i <= 0)
            {
              DEBUGP (("EOF.  Leaving listing parser.\n"));
              break;
            }

          /* Second line must begin with " ".  Otherwise, it's a first
             line (and we may be confused).
          */
          i = clean_line (line, i);
          if (i <= 0)
            {
              /* Blank line.  End of significant file listing. */
              DEBUGP (("Blank line.  Leaving listing parser.\n"));
              break;
            }
          else if (line[0] != ' ')
            {
              DEBUGP (("Non-blank in column 1.  Must be a new file name?\n"));
              continue;
            }
          else
            {
              tok = strtok (line, " ");
              if (tok == NULL)
                {
                  /* Unexpected non-empty but apparently blank line. */
                  DEBUGP (("Null token.  Leaving listing parser.\n"));
                  break;
                }
            }
        }

      /* Analyze tokens.  (Order is not significant, except date must
         precede time.)

         Size:       ddd or ddd/ddd (where "ddd" is a decimal number)
         Date:       DD-MMM-YYYY
         Time:       HH:MM or HH:MM:SS or HH:MM:SS.CC
         Owner:      [user] or [user,group]
         Protection: (ppp,ppp,ppp,ppp) (where "ppp" is "RWED" or some
         subset thereof, for System, Owner, Group, World.

         If permission is lacking, info may be replaced by the string:
         "No privilege for attempted operation".
      */
      while (tok != NULL)
        {
          DEBUGP (("Token: >%s<: ", tok));

          if ((strlen (tok) < 12) && (strchr( tok, '-') != NULL))
            {
              /* Date. */
              DEBUGP (("Date.\n"));
              strcpy( date_str, tok);
              strcat( date_str, " ");
            }
          else if ((strlen (tok) < 12) && (strchr( tok, ':') != NULL))
            {
              /* Time. */
              DEBUGP (("Time. "));
              strncat( date_str,
                       tok,
                       (sizeof( date_str)- strlen (date_str) - 1));
              DEBUGP (("Date time: >%s<\n", date_str));
            }
          else if (strchr (tok, '[') != NULL)
            {
              /* Owner.  (Ignore.) */
              DEBUGP (("Owner.\n"));
            }
          else if (strchr (tok, '(') != NULL)
            {
              /* Protections (permissions). */
              perms = 0;
              j = 0;
              /*FIXME: Should not be using the variable like this. */
              for (i = 0; i < (int) strlen(tok); i++)
                {
                  switch (tok[ i])
                    {
                    case '(':
                      break;
                    case ')':
                      break;
                    case ',':
                      if (j == 0)
                        {
                          perms = 0;
                          j = 1;
                        }
                      else
                        {
                          perms <<= 3;
                        }
                      break;
                    case 'R':
                      perms |= 4;
                      break;
                    case 'W':
                      perms |= 2;
                      break;
                    case 'E':
                      perms |= 1;
                      break;
                    case 'D':
                      perms |= 2;
                      break;
                    }
                }
              cur.perms = perms;
              DEBUGP (("Prot.  perms = %0o.\n", cur.perms));
            }
          else
            {
              /* Nondescript.  Probably size(s), probably in blocks.
                 Could be "No privilege ..." message.  (Ignore.)
              */
              DEBUGP (("Ignored (size?).\n"));
            }

          tok = strtok (NULL, " ");
        }

      /* Tokens exhausted.  Interpret the data, and fill in the
         structure.
      */
      /* Fill tm timestruct according to date-time string.  Fractional
         seconds are ignored.  Default to current time, if conversion
         fails.
      */
      timenow = time( NULL);
      timestruct = localtime( &timenow );
      strptime( date_str, "%d-%b-%Y %H:%M:%S", timestruct);

      /* Convert struct tm local time to time_t local time. */
      timenow = mktime (timestruct);
      /* Offset local time according to environment variable (seconds). */
      if ((tok = getenv ( "WGET_TIMEZONE_DIFFERENTIAL")) != NULL)
        {
          dt = atoi (tok);
          DEBUGP (("Time differential = %d.\n", dt));
        }
      else
        dt = 0;

      if (dt >= 0)
        timenow += dt;
      else
        timenow -= (-dt);

      cur.tstamp = timenow; /* Store the time-stamp. */
      DEBUGP (("Timestamp: %ld\n", cur.tstamp));
      cur.ptype = TT_HOUR_MIN;

      /* Add the data for this item to the linked list, */
      if (!dir)
        {
          l = dir = (struct fileinfo *)xmalloc (sizeof (struct fileinfo));
          memcpy (l, &cur, sizeof (cur));
          l->prev = l->next = NULL;
        }
      else
        {
          cur.prev = l;
          l->next = (struct fileinfo *)xmalloc (sizeof (struct fileinfo));
          l = l->next;
          memcpy (l, &cur, sizeof (cur));
          l->next = NULL;
        }

      i = getline (&line, &bufsize, fp);
      if (i > 0)
        {
          i = clean_line (line, i);
          if (i <= 0)
            {
              /* Blank line.  End of significant file listing. */
              break;
            }
        }
    }

  xfree (line);
  fclose (fp);
  return dir;
}


/* This function switches between the correct parsing routine depending on
   the SYSTEM_TYPE. The system type should be based on the result of the
   "SYST" response of the FTP server. According to this repsonse we will
   use on of the three different listing parsers that cover the most of FTP
   servers used nowadays.  */

struct fileinfo *
ftp_parse_ls (const char *file, const enum stype system_type)
{
  switch (system_type)
    {
    case ST_UNIX:
      return ftp_parse_unix_ls (file, 0);
    case ST_WINNT:
      {
        /* Detect whether the listing is simulating the UNIX format */
        FILE *fp;
        int   c;
        fp = fopen (file, "rb");
        if (!fp)
        {
          logprintf (LOG_NOTQUIET, "%s: %s\n", file, strerror (errno));
          return NULL;
        }
        c = fgetc(fp);
        fclose(fp);
        /* If the first character of the file is '0'-'9', it's WINNT
           format. */
        if (c >= '0' && c <='9')
          return ftp_parse_winnt_ls (file);
        else
          return ftp_parse_unix_ls (file, 1);
      }
    case ST_VMS:
      return ftp_parse_vms_ls (file);
    case ST_MACOS:
      return ftp_parse_unix_ls (file, 1);
    default:
      logprintf (LOG_NOTQUIET, _("\
Unsupported listing type, trying Unix listing parser.\n"));
      return ftp_parse_unix_ls (file, 0);
    }
}

/* Stuff for creating FTP index. */

/* The function creates an HTML index containing references to given
   directories and files on the appropriate host.  The references are
   FTP.  */
uerr_t
ftp_index (const char *file, struct url *u, struct fileinfo *f)
{
  FILE *fp;
  char *upwd;
  char *htcldir;                /* HTML-clean dir name */
  char *htclfile;               /* HTML-clean file name */
  char *urlclfile;              /* URL-clean file name */

  if (!output_stream)
    {
      fp = fopen (file, "wb");
      if (!fp)
        {
          logprintf (LOG_NOTQUIET, "%s: %s\n", file, strerror (errno));
          return FOPENERR;
        }
    }
  else
    fp = output_stream;
  if (u->user)
    {
      char *tmpu, *tmpp;        /* temporary, clean user and passwd */

      tmpu = url_escape (u->user);
      tmpp = u->passwd ? url_escape (u->passwd) : NULL;
      if (tmpp)
        upwd = concat_strings (tmpu, ":", tmpp, "@", (char *) 0);
      else
        upwd = concat_strings (tmpu, "@", (char *) 0);
      xfree (tmpu);
      xfree_null (tmpp);
    }
  else
    upwd = xstrdup ("");

  htcldir = html_quote_string (u->dir);

  fprintf (fp, "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n");
  fprintf (fp, "<html>\n<head>\n<title>");
  fprintf (fp, _("Index of /%s on %s:%d"), htcldir, u->host, u->port);
  fprintf (fp, "</title>\n</head>\n<body>\n<h1>");
  fprintf (fp, _("Index of /%s on %s:%d"), htcldir, u->host, u->port);
  fprintf (fp, "</h1>\n<hr>\n<pre>\n");

  while (f)
    {
      fprintf (fp, "  ");
      if (f->tstamp != -1)
        {
          /* #### Should we translate the months?  Or, even better, use
             ISO 8601 dates?  */
          static const char *months[] = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
          };
          time_t tstamp = f->tstamp;
          struct tm *ptm = localtime (&tstamp);

          fprintf (fp, "%d %s %02d ", ptm->tm_year + 1900, months[ptm->tm_mon],
                  ptm->tm_mday);
          if (f->ptype == TT_HOUR_MIN)
            fprintf (fp, "%02d:%02d  ", ptm->tm_hour, ptm->tm_min);
          else
            fprintf (fp, "       ");
        }
      else
        fprintf (fp, _("time unknown       "));
      switch (f->type)
        {
        case FT_PLAINFILE:
          fprintf (fp, _("File        "));
          break;
        case FT_DIRECTORY:
          fprintf (fp, _("Directory   "));
          break;
        case FT_SYMLINK:
          fprintf (fp, _("Link        "));
          break;
        default:
          fprintf (fp, _("Not sure    "));
          break;
        }
      htclfile = html_quote_string (f->name);
      urlclfile = url_escape_unsafe_and_reserved (f->name);
      fprintf (fp, "<a href=\"ftp://%s%s:%d", upwd, u->host, u->port);
      if (*u->dir != '/')
        putc ('/', fp);
      /* XXX: Should probably URL-escape dir components here, rather
       * than just HTML-escape, for consistency with the next bit where
       * we use urlclfile for the file component. Anyway, this is safer
       * than what we had... */
      fprintf (fp, "%s", htcldir);
      if (*u->dir)
        putc ('/', fp);
      fprintf (fp, "%s", urlclfile);
      if (f->type == FT_DIRECTORY)
        putc ('/', fp);
      fprintf (fp, "\">%s", htclfile);
      if (f->type == FT_DIRECTORY)
        putc ('/', fp);
      fprintf (fp, "</a> ");
      if (f->type == FT_PLAINFILE)
        fprintf (fp, _(" (%s bytes)"), number_to_static_string (f->size));
      else if (f->type == FT_SYMLINK)
        fprintf (fp, "-> %s", f->linkto ? f->linkto : "(nil)");
      putc ('\n', fp);
      xfree (htclfile);
      xfree (urlclfile);
      f = f->next;
    }
  fprintf (fp, "</pre>\n</body>\n</html>\n");
  xfree (htcldir);
  xfree (upwd);
  if (!output_stream)
    fclose (fp);
  else
    fflush (fp);
  return FTPOK;
}
