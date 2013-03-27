/* 
   Date manipulation routines
   Copyright (C) 1999-2006, Joe Orton <joe@manyfish.co.uk>
   Copyright (C) 2004 Jiang Lei <tristone@deluxe.ocn.ne.jp>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

*/

#include "config.h"

#include <sys/types.h>

#include <time.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef WIN32
#include <windows.h> /* for TIME_ZONE_INFORMATION */
#endif

#include "ne_alloc.h"
#include "ne_dates.h"
#include "ne_string.h"

/* Generic date manipulation routines. */

/* ISO8601: 2001-01-01T12:30:00Z */
#define ISO8601_FORMAT_Z "%04d-%02d-%02dT%02d:%02d:%lfZ"
#define ISO8601_FORMAT_M "%04d-%02d-%02dT%02d:%02d:%lf-%02d:%02d"
#define ISO8601_FORMAT_P "%04d-%02d-%02dT%02d:%02d:%lf+%02d:%02d"

/* RFC1123: Sun, 06 Nov 1994 08:49:37 GMT */
#define RFC1123_FORMAT "%3s, %02d %3s %4d %02d:%02d:%02d GMT"
/* RFC850:  Sunday, 06-Nov-94 08:49:37 GMT */
#define RFC1036_FORMAT "%10s %2d-%3s-%2d %2d:%2d:%2d GMT"
/* asctime: Wed Jun 30 21:49:08 1993 */
#define ASCTIME_FORMAT "%3s %3s %2d %2d:%2d:%2d %4d"

static const char rfc1123_weekdays[7][4] = { 
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" 
};
static const char short_months[12][4] = { 
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

#if defined(HAVE_STRUCT_TM_TM_GMTOFF)
#define GMTOFF(t) ((t).tm_gmtoff)
#elif defined(HAVE_STRUCT_TM___TM_GMTOFF)
#define GMTOFF(t) ((t).__tm_gmtoff)
#elif defined(WIN32)
#define GMTOFF(t) (gmt_to_local_win32())
#elif defined(HAVE_TIMEZONE)
/* FIXME: the following assumes fixed dst offset of 1 hour */
#define GMTOFF(t) (-timezone + ((t).tm_isdst > 0 ? 3600 : 0))
#else
/* FIXME: work out the offset anyway. */
#define GMTOFF(t) (0)
#endif

#ifdef WIN32
time_t gmt_to_local_win32(void)
{
    TIME_ZONE_INFORMATION tzinfo;
    DWORD dwStandardDaylight;
    long bias;

    dwStandardDaylight = GetTimeZoneInformation(&tzinfo);
    bias = tzinfo.Bias;

    if (dwStandardDaylight == TIME_ZONE_ID_STANDARD)
        bias += tzinfo.StandardBias;
    
    if (dwStandardDaylight == TIME_ZONE_ID_DAYLIGHT)
        bias += tzinfo.DaylightBias;
    
    return (- bias * 60);
}
#endif

/* Returns the time/date GMT, in RFC1123-type format: eg
 *  Sun, 06 Nov 1994 08:49:37 GMT. */
char *ne_rfc1123_date(time_t anytime) {
    struct tm *gmt;
    char *ret;
    gmt = gmtime(&anytime);
    if (gmt == NULL)
	return NULL;
    ret = ne_malloc(29 + 1); /* dates are 29 chars long */
/*  it goes: Sun, 06 Nov 1994 08:49:37 GMT */
    ne_snprintf(ret, 30, RFC1123_FORMAT,
		rfc1123_weekdays[gmt->tm_wday], gmt->tm_mday, 
		short_months[gmt->tm_mon], 1900 + gmt->tm_year, 
		gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
    
    return ret;
}

/* Takes an ISO-8601-formatted date string and returns the time_t.
 * Returns (time_t)-1 if the parse fails. */
time_t ne_iso8601_parse(const char *date) 
{
    struct tm gmt = {0};
    int off_hour, off_min;
    double sec;
    off_t fix;
    int n;
    time_t result;

    /*  it goes: ISO8601: 2001-01-01T12:30:00+03:30 */
    if ((n = sscanf(date, ISO8601_FORMAT_P,
		    &gmt.tm_year, &gmt.tm_mon, &gmt.tm_mday,
		    &gmt.tm_hour, &gmt.tm_min, &sec,
		    &off_hour, &off_min)) == 8) {
      gmt.tm_sec = (int)sec;
      fix = - off_hour * 3600 - off_min * 60;
    }
    /*  it goes: ISO8601: 2001-01-01T12:30:00-03:30 */
    else if ((n = sscanf(date, ISO8601_FORMAT_M,
			 &gmt.tm_year, &gmt.tm_mon, &gmt.tm_mday,
			 &gmt.tm_hour, &gmt.tm_min, &sec,
			 &off_hour, &off_min)) == 8) {
      gmt.tm_sec = (int)sec;
      fix = off_hour * 3600 + off_min * 60;
    }
    /*  it goes: ISO8601: 2001-01-01T12:30:00Z */
    else if ((n = sscanf(date, ISO8601_FORMAT_Z,
			 &gmt.tm_year, &gmt.tm_mon, &gmt.tm_mday,
			 &gmt.tm_hour, &gmt.tm_min, &sec)) == 6) {
      gmt.tm_sec = (int)sec;
      fix = 0;
    }
    else {
      return (time_t)-1;
    }

    gmt.tm_year -= 1900;
    gmt.tm_isdst = -1;
    gmt.tm_mon--;

    result = mktime(&gmt) + fix;
    return result + GMTOFF(gmt);
}

/* Takes an RFC1123-formatted date string and returns the time_t.
 * Returns (time_t)-1 if the parse fails. */
time_t ne_rfc1123_parse(const char *date) 
{
    struct tm gmt = {0};
    char wkday[4], mon[4];
    int n;
    time_t result;
    
/*  it goes: Sun, 06 Nov 1994 08:49:37 GMT */
    n = sscanf(date, RFC1123_FORMAT,
	    wkday, &gmt.tm_mday, mon, &gmt.tm_year, &gmt.tm_hour,
	    &gmt.tm_min, &gmt.tm_sec);
    /* Is it portable to check n==7 here? */
    gmt.tm_year -= 1900;
    for (n=0; n<12; n++)
	if (strcmp(mon, short_months[n]) == 0)
	    break;
    /* tm_mon comes out as 12 if the month is corrupt, which is desired,
     * since the mktime will then fail */
    gmt.tm_mon = n;
    gmt.tm_isdst = -1;
    result = mktime(&gmt);
    return result + GMTOFF(gmt);
}

/* Takes a string containing a RFC1036-style date and returns the time_t */
time_t ne_rfc1036_parse(const char *date) 
{
    struct tm gmt = {0};
    int n;
    char wkday[11], mon[4];
    time_t result;

    /* RFC850/1036 style dates: Sunday, 06-Nov-94 08:49:37 GMT */
    n = sscanf(date, RFC1036_FORMAT,
		wkday, &gmt.tm_mday, mon, &gmt.tm_year,
		&gmt.tm_hour, &gmt.tm_min, &gmt.tm_sec);
    if (n != 7) {
	return (time_t)-1;
    }

    /* portable to check n here? */
    for (n=0; n<12; n++)
	if (strcmp(mon, short_months[n]) == 0)
	    break;
    /* tm_mon comes out as 12 if the month is corrupt, which is desired,
     * since the mktime will then fail */

    /* Defeat Y2K bug. */
    if (gmt.tm_year < 50)
	gmt.tm_year += 100;

    gmt.tm_mon = n;
    gmt.tm_isdst = -1;
    result = mktime(&gmt);
    return result + GMTOFF(gmt);
}


/* (as)ctime dates are like:
 *    Wed Jun 30 21:49:08 1993
 */
time_t ne_asctime_parse(const char *date) 
{
    struct tm gmt = {0};
    int n;
    char wkday[4], mon[4];
    time_t result;

    n = sscanf(date, ASCTIME_FORMAT,
		wkday, mon, &gmt.tm_mday, 
		&gmt.tm_hour, &gmt.tm_min, &gmt.tm_sec,
		&gmt.tm_year);
    /* portable to check n here? */
    for (n=0; n<12; n++)
	if (strcmp(mon, short_months[n]) == 0)
	    break;
    /* tm_mon comes out as 12 if the month is corrupt, which is desired,
     * since the mktime will then fail */
    gmt.tm_mon = n;
    gmt.tm_isdst = -1;
    result = mktime(&gmt);
    return result + GMTOFF(gmt);
}

/* HTTP-date parser */
time_t ne_httpdate_parse(const char *date)
{
    time_t tmp;
    tmp = ne_rfc1123_parse(date);
    if (tmp == -1) {
        tmp = ne_rfc1036_parse(date);
	if (tmp == -1)
	    tmp = ne_asctime_parse(date);
    }
    return tmp;
}
