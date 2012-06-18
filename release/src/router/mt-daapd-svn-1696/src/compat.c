#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include "daapd.h"

#if !HAVE_STRCASESTR
/* case-independent string matching, similar to strstr but
 * matching */
char * strcasestr(char* haystack, char* needle) {
  int i;
  int nlength = (int) strlen (needle);
  int hlength = (int) strlen (haystack);

  if (nlength > hlength) return NULL;
  if (hlength <= 0) return NULL;
  if (nlength <= 0) return haystack;
  /* hlength and nlength > 0, nlength <= hlength */
  for (i = 0; i <= (hlength - nlength); i++) {
    if (strncasecmp (haystack + i, needle, nlength) == 0) {
      return haystack + i;
    }
  }
  /* substring not found */
  return NULL;
}

#endif /* !HAVE_STRCASESTR */

/*
 * Copyright (c) 1994 Powerdog Industries.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgement:
 *      This product includes software developed by Powerdog Industries.
 * 4. The name of Powerdog Industries may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY POWERDOG INDUSTRIES ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE POWERDOG INDUSTRIES BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef HAVE_STRPTIME

#define asizeof(a)      (sizeof (a) / sizeof ((a)[0]))


#ifndef sun
struct dtconv {
        char    *abbrev_month_names[12];
        char    *month_names[12];
        char    *abbrev_weekday_names[7];
        char    *weekday_names[7];
        char    *time_format;
        char    *sdate_format;
        char    *dtime_format;
        char    *am_string;
        char    *pm_string;
        char    *ldate_format;
};
#endif

static struct dtconv    En_US = {
        { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" },
        { "January", "February", "March", "April",
          "May", "June", "July", "August",
          "September", "October", "November", "December" },
        { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" },
        { "Sunday", "Monday", "Tuesday", "Wednesday",
          "Thursday", "Friday", "Saturday" },
        "%H:%M:%S",
        "%m/%d/%y",
        "%a %b %e %T %Z %Y",
        "AM",
        "PM",
        "%A, %B, %e, %Y"
};

#ifdef SUNOS4
extern int      strncasecmp();
#endif

void lowercase_string(char *buffer) {
    while(*buffer) {
        *buffer = tolower(*buffer);
        buffer++;
    }
}

char    *strptime(char *buf, char *fmt, struct tm *tm) {
        char    c,
                *ptr;
        int     i, j,
                len;
        ptr = fmt;
        while (*ptr != 0) {
                if (*buf == 0)
                        break;

                c = *ptr++;

                if (c != '%') {
                        if (isspace(c))
                                while (*buf != 0 && isspace(*buf))
                                        buf++;
                        else if (c != *buf++)
                                return 0;
                        continue;
                }

                c = *ptr++;
                switch (c) {
                case 0:
                case '%':
                        if (*buf++ != '%')
                                return 0;
                        break;

                case 'C':
                        buf = strptime(buf, En_US.ldate_format, tm);
                        if (buf == 0)
                                return 0;
                        break;

                case 'c':
                        buf = strptime(buf, "%x %X", tm);
                        if (buf == 0)
                                return 0;
                        break;

                case 'D':
                        buf = strptime(buf, "%m/%d/%y", tm);
                        if (buf == 0)
                                return 0;
                        break;

                case 'R':
                        buf = strptime(buf, "%H:%M", tm);
                        if (buf == 0)
                                return 0;
                        break;

                case 'r':
                        buf = strptime(buf, "%I:%M:%S %p", tm);
                        if (buf == 0)
                                return 0;
                        break;

                case 'T':
                        buf = strptime(buf, "%H:%M:%S", tm);
                        if (buf == 0)
                                return 0;
                        break;

                case 'X':
                        buf = strptime(buf, En_US.time_format, tm);
                        if (buf == 0)
                                return 0;
                        break;

                case 'x':
                        buf = strptime(buf, En_US.sdate_format, tm);
                        if (buf == 0)
                                return 0;
                        break;

                case 'j':
                        if (!isdigit(*buf))
                                return 0;

                        for (i = 0; *buf != 0 && isdigit(*buf); buf++) {
                                i *= 10;
                                i += *buf - '0';
                        }
                        if (i > 365)
                                return 0;

                        tm->tm_yday = i;
                        break;

                case 'M':
                case 'S':
                        if (*buf == 0 || isspace(*buf))
                                break;

                        if (!isdigit(*buf))
                                return 0;

                        for (j=0,i = 0; *buf != 0 && isdigit(*buf) && j<2; j++,buf++) {
                                i *= 10;
                                i += *buf - '0';
                        }
                        if (i > 59)
                                return 0;

                        if (c == 'M')
                                tm->tm_min = i;
                        else
                                tm->tm_sec = i;

                        if (*buf != 0 && isspace(*buf))
                                while (*ptr != 0 && !isspace(*ptr))
                                        ptr++;
                        break;

                case 'H':
                case 'I':
                case 'k':
                case 'l':
                        if (!isdigit(*buf))
                                return 0;

                        for (j=0,i = 0; *buf != 0 && isdigit(*buf) && j<2; j++,buf++) {
                                i *= 10;
                                i += *buf - '0';
                        }
                        if (c == 'H' || c == 'k') {
                                if (i > 23)
                                        return 0;
                        } else if (i > 11)
                                return 0;

                        tm->tm_hour = i;

                        if (*buf != 0 && isspace(*buf))
                                while (*ptr != 0 && !isspace(*ptr))
                                        ptr++;
                        break;

                case 'p':
                        len = (int) strlen(En_US.am_string);
                        lowercase_string( buf );

                        if (strncmp(buf, En_US.am_string, len) == 0) {
                                if (tm->tm_hour > 12)
                                        return 0;
                                if (tm->tm_hour == 12)
                                        tm->tm_hour = 0;
                                buf += len;
                                break;
                        }

                        len = (int) strlen(En_US.pm_string);

                        if (strncmp(buf, En_US.pm_string, len) == 0) {
                                if (tm->tm_hour > 12)
                                        return 0;
                                if (tm->tm_hour != 12)
                                        tm->tm_hour += 12;
                                buf += len;
                                break;
                        }

                        return 0;

                case 'A':
                case 'a':
                        for (i = 0; i < asizeof(En_US.weekday_names); i++) {
                                len = (int) strlen(En_US.weekday_names[i]);

                                lowercase_string( buf );

                                if (strncmp(buf,
                                                En_US.weekday_names[i],
                                                len) == 0)
                                        break;

                                len = (int) strlen(En_US.abbrev_weekday_names[i]);
                                if (strncmp(buf,
                                                En_US.abbrev_weekday_names[i],
                                                len) == 0)
                                        break;
                        }
                        if (i == asizeof(En_US.weekday_names))
                                return 0;

                        tm->tm_wday = i;
                        buf += len;
                        break;

                case 'd':
                case 'e':
                        if (!isdigit(*buf))
                                return 0;

                        for (j=0,i = 0; *buf != 0 && isdigit(*buf) && j<2; j++,buf++) {
                                i *= 10;
                                i += *buf - '0';
                        }
                        if (i > 31)
                                return 0;

                        tm->tm_mday = i;

                        if (*buf != 0 && isspace(*buf))
                                while (*ptr != 0 && !isspace(*ptr))
                                        ptr++;
                        break;

                case 'B':
                case 'b':
                case 'h':
                        for (i = 0; i < asizeof(En_US.month_names); i++) {
                                len = (int) strlen(En_US.month_names[i]);

                                lowercase_string( buf );
                                if (strncmp(buf, En_US.month_names[i],len) == 0)
                                        break;

                                len = (int) strlen(En_US.abbrev_month_names[i]);
                                if (strncmp(buf,
                                                En_US.abbrev_month_names[i],
                                                len) == 0)
                                        break;
                        }
                        if (i == asizeof(En_US.month_names))
                                return 0;

                        tm->tm_mon = i;
                        buf += len;
                        break;

                case 'm':
                        if (!isdigit(*buf))
                                return 0;

                        for (j=0,i = 0; *buf != 0 && isdigit(*buf) && j<2; j++,buf++) {
                                i *= 10;
                                i += *buf - '0';
                        }
                        if (i < 1 || i > 12)
                                return 0;

                        tm->tm_mon = i - 1;

                        if (*buf != 0 && isspace(*buf))
                                while (*ptr != 0 && !isspace(*ptr))
                                        ptr++;
                        break;

                case 'Y':
                case 'y':
                        if (*buf == 0 || isspace(*buf))
                                break;

                        if (!isdigit(*buf))
                                return 0;

                        for (j=0,i = 0; *buf != 0 && isdigit(*buf) && j<((c=='Y')?4:2); j++,buf++) {
                                i *= 10;
                                i += *buf - '0';
                        }

                        if (c == 'Y')
                                i -= 1900;
                        else if (i < 69) /*c=='y', 00-68 is for 20xx, the rest is for 19xx*/
                                i += 100;

                        if (i < 0)
                                return 0;

                        tm->tm_year = i;

                        if (*buf != 0 && isspace(*buf))
                                while (*ptr != 0 && !isspace(*ptr))
                                        ptr++;
                        break;
                }
        }

        return buf;
}

#endif   /* ndef HAVE_STRPTIME */

/* Compliments of Jay Freeman <saurik@saurik.com> */

#if !HAVE_STRSEP
char *strsep(char **stringp, const char *delim) {
        char *ret = *stringp;
        if (ret == NULL) return(NULL); /* grrr */
        if ((*stringp = strpbrk(*stringp, delim)) != NULL) {
                *((*stringp)++) = '\0';
        }
        return(ret);
}
#endif /* !HAVE_STRSEP */

/* Reentrant string tokenizer.  Generic version.
   Copyright (C) 1991,1996-1999,2001,2004 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef HAVE_STRTOK_R

#include <stdio.h>

/* Parse S into tokens separated by characters in DELIM.
   If S is NULL, the saved pointer in SAVE_PTR is used as
   the next starting point.  For example:
        char s[] = "-abc-=-def";
        char *sp;
        x = strtok_r(s, "-", &sp);      // x = "abc", sp = "=-def"
        x = strtok_r(NULL, "-=", &sp);  // x = "def", sp = NULL
        x = strtok_r(NULL, "=", &sp);   // x = NULL
                // s = "abc\0-def\0"
*/
char *strtok_r(char *s, const char *delim, char **last)
{
  char *spanp;
  int c, sc;
  char *tok;

  if (s == NULL && (s = *last) == NULL) {
    return NULL;
  }

  /*
   * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
   */
 cont:
  c = *s++;
  for (spanp = (char *)delim; (sc = *spanp++) != 0; ) {
    if (c == sc) {
      goto cont;
    }
  }

  if (c == 0) {         /* no non-delimiter characters */
    *last = NULL;
    return NULL;
  }
  tok = s - 1;

  /*
   * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
   * Note that delim must have one NUL; we stop if we see that, too.
   */
  for (;;) {
    c = *s++;
    spanp = (char *)delim;
    do {
      if ((sc = *spanp++) == c) {
        if (c == 0) {
          s = NULL;
        }
        else {
          char *w = s - 1;
          *w = '\0';
        }
        *last = s;
        return tok;
      }
    }
    while (sc != 0);
  }
  /* NOTREACHED */
}
#endif

#ifndef HAVE_TIMEGM
time_t timegm(struct tm *tm) {
    time_t ret;
    char *tz;
    char buffer[255];

    tz = getenv("TZ");
    putenv("TZ=UTC0");
    tzset();
    ret = mktime(tm);

    if(tz)
        sprintf(buffer,"TZ=%s",tz);
    else
        strcpy(buffer,"TZ=");
    putenv(buffer);
    tzset();
    return ret;
}
#endif
