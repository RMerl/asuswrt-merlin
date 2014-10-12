/* 
   Unix SMB/CIFS implementation.
   time handling functions

   Copyright (C) Andrew Tridgell 		1992-2004
   Copyright (C) Stefan (metze) Metzmacher	2002   
   Copyright (C) Jeremy Allison			2007
   Copyright (C) Andrew Bartlett                2011

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/time.h"

/**
 * @file
 * @brief time handling functions
 */

#if (SIZEOF_LONG == 8)
#define TIME_FIXUP_CONSTANT_INT 11644473600L
#elif (SIZEOF_LONG_LONG == 8)
#define TIME_FIXUP_CONSTANT_INT 11644473600LL
#endif



/**
 External access to time_t_min and time_t_max.
**/
_PUBLIC_ time_t get_time_t_max(void)
{
	return TIME_T_MAX;
}

/**
a gettimeofday wrapper
**/
_PUBLIC_ void GetTimeOfDay(struct timeval *tval)
{
#ifdef HAVE_GETTIMEOFDAY_TZ
	gettimeofday(tval,NULL);
#else
	gettimeofday(tval);
#endif
}

/**
a wrapper to preferably get the monotonic time
**/
_PUBLIC_ void clock_gettime_mono(struct timespec *tp)
{
	if (clock_gettime(CUSTOM_CLOCK_MONOTONIC,tp) != 0) {
		clock_gettime(CLOCK_REALTIME,tp);
	}
}

/**
a wrapper to preferably get the monotonic time in seconds
as this is only second resolution we can use the cached
(and much faster) COARSE clock variant
**/
_PUBLIC_ time_t time_mono(time_t *t)
{
	struct timespec tp;
	int rc = -1;
#ifdef CLOCK_MONOTONIC_COARSE
	rc = clock_gettime(CLOCK_MONOTONIC_COARSE,&tp);
#endif
	if (rc != 0) {
		clock_gettime_mono(&tp);
	}
	if (t != NULL) {
		*t = tp.tv_sec;
	}
	return tp.tv_sec;
}


#define TIME_FIXUP_CONSTANT 11644473600LL

time_t convert_timespec_to_time_t(struct timespec ts)
{
	/* Ensure tv_nsec is less than 1sec. */
	while (ts.tv_nsec > 1000000000) {
		ts.tv_sec += 1;
		ts.tv_nsec -= 1000000000;
	}

	/* 1 ns == 1,000,000,000 - one thousand millionths of a second.
	   increment if it's greater than 500 millionth of a second. */

	if (ts.tv_nsec > 500000000) {
		return ts.tv_sec + 1;
	}
	return ts.tv_sec;
}

struct timespec convert_time_t_to_timespec(time_t t)
{
	struct timespec ts;
	ts.tv_sec = t;
	ts.tv_nsec = 0;
	return ts;
}



/**
 Interpret an 8 byte "filetime" structure to a time_t
 It's originally in "100ns units since jan 1st 1601"

 An 8 byte value of 0xffffffffffffffff will be returned as a timespec of

	tv_sec = 0
	tv_nsec = 0;

 Returns GMT.
**/
time_t nt_time_to_unix(NTTIME nt)
{
	return convert_timespec_to_time_t(nt_time_to_unix_timespec(&nt));
}


/**
put a 8 byte filetime from a time_t
This takes GMT as input
**/
_PUBLIC_ void unix_to_nt_time(NTTIME *nt, time_t t)
{
	uint64_t t2; 

	if (t == (time_t)-1) {
		*nt = (NTTIME)-1LL;
		return;
	}	

	if (t == TIME_T_MAX) {
		*nt = 0x7fffffffffffffffLL;
		return;
	}

	if (t == 0) {
		*nt = 0;
		return;
	}		

	t2 = t;
	t2 += TIME_FIXUP_CONSTANT_INT;
	t2 *= 1000*1000*10;

	*nt = t2;
}


/**
check if it's a null unix time
**/
_PUBLIC_ bool null_time(time_t t)
{
	return t == 0 || 
		t == (time_t)0xFFFFFFFF || 
		t == (time_t)-1;
}


/**
check if it's a null NTTIME
**/
_PUBLIC_ bool null_nttime(NTTIME t)
{
	return t == 0 || t == (NTTIME)-1;
}

/*******************************************************************
  create a 16 bit dos packed date
********************************************************************/
static uint16_t make_dos_date1(struct tm *t)
{
	uint16_t ret=0;
	ret = (((unsigned int)(t->tm_mon+1)) >> 3) | ((t->tm_year-80) << 1);
	ret = ((ret&0xFF)<<8) | (t->tm_mday | (((t->tm_mon+1) & 0x7) << 5));
	return ret;
}

/*******************************************************************
  create a 16 bit dos packed time
********************************************************************/
static uint16_t make_dos_time1(struct tm *t)
{
	uint16_t ret=0;
	ret = ((((unsigned int)t->tm_min >> 3)&0x7) | (((unsigned int)t->tm_hour) << 3));
	ret = ((ret&0xFF)<<8) | ((t->tm_sec/2) | ((t->tm_min & 0x7) << 5));
	return ret;
}

/*******************************************************************
  create a 32 bit dos packed date/time from some parameters
  This takes a GMT time and returns a packed localtime structure
********************************************************************/
static uint32_t make_dos_date(time_t unixdate, int zone_offset)
{
	struct tm *t;
	uint32_t ret=0;

	if (unixdate == 0) {
		return 0;
	}

	unixdate -= zone_offset;

	t = gmtime(&unixdate);
	if (!t) {
		return 0xFFFFFFFF;
	}

	ret = make_dos_date1(t);
	ret = ((ret&0xFFFF)<<16) | make_dos_time1(t);

	return ret;
}

/**
put a dos date into a buffer (time/date format)
This takes GMT time and puts local time in the buffer
**/
_PUBLIC_ void push_dos_date(uint8_t *buf, int offset, time_t unixdate, int zone_offset)
{
	uint32_t x = make_dos_date(unixdate, zone_offset);
	SIVAL(buf,offset,x);
}

/**
put a dos date into a buffer (date/time format)
This takes GMT time and puts local time in the buffer
**/
_PUBLIC_ void push_dos_date2(uint8_t *buf,int offset,time_t unixdate, int zone_offset)
{
	uint32_t x;
	x = make_dos_date(unixdate, zone_offset);
	x = ((x&0xFFFF)<<16) | ((x&0xFFFF0000)>>16);
	SIVAL(buf,offset,x);
}

/**
put a dos 32 bit "unix like" date into a buffer. This routine takes
GMT and converts it to LOCAL time before putting it (most SMBs assume
localtime for this sort of date)
**/
_PUBLIC_ void push_dos_date3(uint8_t *buf,int offset,time_t unixdate, int zone_offset)
{
	if (!null_time(unixdate)) {
		unixdate -= zone_offset;
	}
	SIVAL(buf,offset,unixdate);
}

/*******************************************************************
  interpret a 32 bit dos packed date/time to some parameters
********************************************************************/
void interpret_dos_date(uint32_t date,int *year,int *month,int *day,int *hour,int *minute,int *second)
{
	uint32_t p0,p1,p2,p3;

	p0=date&0xFF; p1=((date&0xFF00)>>8)&0xFF; 
	p2=((date&0xFF0000)>>16)&0xFF; p3=((date&0xFF000000)>>24)&0xFF;

	*second = 2*(p0 & 0x1F);
	*minute = ((p0>>5)&0xFF) + ((p1&0x7)<<3);
	*hour = (p1>>3)&0xFF;
	*day = (p2&0x1F);
	*month = ((p2>>5)&0xFF) + ((p3&0x1)<<3) - 1;
	*year = ((p3>>1)&0xFF) + 80;
}

/**
  create a unix date (int GMT) from a dos date (which is actually in
  localtime)
**/
_PUBLIC_ time_t pull_dos_date(const uint8_t *date_ptr, int zone_offset)
{
	uint32_t dos_date=0;
	struct tm t;
	time_t ret;

	dos_date = IVAL(date_ptr,0);

	if (dos_date == 0) return (time_t)0;
  
	interpret_dos_date(dos_date,&t.tm_year,&t.tm_mon,
			   &t.tm_mday,&t.tm_hour,&t.tm_min,&t.tm_sec);
	t.tm_isdst = -1;
  
	ret = timegm(&t);

	ret += zone_offset;

	return ret;
}

/**
like make_unix_date() but the words are reversed
**/
_PUBLIC_ time_t pull_dos_date2(const uint8_t *date_ptr, int zone_offset)
{
	uint32_t x,x2;

	x = IVAL(date_ptr,0);
	x2 = ((x&0xFFFF)<<16) | ((x&0xFFFF0000)>>16);
	SIVAL(&x,0,x2);

	return pull_dos_date((const uint8_t *)&x, zone_offset);
}

/**
  create a unix GMT date from a dos date in 32 bit "unix like" format
  these generally arrive as localtimes, with corresponding DST
**/
_PUBLIC_ time_t pull_dos_date3(const uint8_t *date_ptr, int zone_offset)
{
	time_t t = (time_t)IVAL(date_ptr,0);
	if (!null_time(t)) {
		t += zone_offset;
	}
	return t;
}


/****************************************************************************
 Return the date and time as a string
****************************************************************************/

char *timeval_string(TALLOC_CTX *ctx, const struct timeval *tp, bool hires)
{
	time_t t;
	struct tm *tm;

	t = (time_t)tp->tv_sec;
	tm = localtime(&t);
	if (!tm) {
		if (hires) {
			return talloc_asprintf(ctx,
					       "%ld.%06ld seconds since the Epoch",
					       (long)tp->tv_sec,
					       (long)tp->tv_usec);
		} else {
			return talloc_asprintf(ctx,
					       "%ld seconds since the Epoch",
					       (long)t);
		}
	} else {
#ifdef HAVE_STRFTIME
		char TimeBuf[60];
		if (hires) {
			strftime(TimeBuf,sizeof(TimeBuf)-1,"%Y/%m/%d %H:%M:%S",tm);
			return talloc_asprintf(ctx,
					       "%s.%06ld", TimeBuf,
					       (long)tp->tv_usec);
		} else {
			strftime(TimeBuf,sizeof(TimeBuf)-1,"%Y/%m/%d %H:%M:%S",tm);
			return talloc_strdup(ctx, TimeBuf);
		}
#else
		if (hires) {
			const char *asct = asctime(tm);
			return talloc_asprintf(ctx, "%s.%06ld",
					asct ? asct : "unknown",
					(long)tp->tv_usec);
		} else {
			const char *asct = asctime(tm);
			return talloc_asprintf(ctx, asct ? asct : "unknown");
		}
#endif
	}
}

char *current_timestring(TALLOC_CTX *ctx, bool hires)
{
	struct timeval tv;

	GetTimeOfDay(&tv);
	return timeval_string(ctx, &tv, hires);
}


/**
return a HTTP/1.0 time string
**/
_PUBLIC_ char *http_timestring(TALLOC_CTX *mem_ctx, time_t t)
{
	char *buf;
	char tempTime[60];
	struct tm *tm = localtime(&t);

	if (t == TIME_T_MAX) {
		return talloc_strdup(mem_ctx, "never");
	}

	if (!tm) {
		return talloc_asprintf(mem_ctx,"%ld seconds since the Epoch",(long)t);
	}

#ifndef HAVE_STRFTIME
	buf = talloc_strdup(mem_ctx, asctime(tm));
	if (buf[strlen(buf)-1] == '\n') {
		buf[strlen(buf)-1] = 0;
	}
#else
	strftime(tempTime, sizeof(tempTime)-1, "%a, %d %b %Y %H:%M:%S %Z", tm);
	buf = talloc_strdup(mem_ctx, tempTime);
#endif /* !HAVE_STRFTIME */

	return buf;
}

/**
 Return the date and time as a string
**/
_PUBLIC_ char *timestring(TALLOC_CTX *mem_ctx, time_t t)
{
	char *TimeBuf;
	char tempTime[80];
	struct tm *tm;

	tm = localtime(&t);
	if (!tm) {
		return talloc_asprintf(mem_ctx,
				       "%ld seconds since the Epoch",
				       (long)t);
	}

#ifdef HAVE_STRFTIME
	/* Some versions of gcc complain about using some special format
	 * specifiers. This is a bug in gcc, not a bug in this code. See a
	 * recent strftime() manual page for details. */
	strftime(tempTime,sizeof(tempTime)-1,"%a %b %e %X %Y %Z",tm);
	TimeBuf = talloc_strdup(mem_ctx, tempTime);
#else
	TimeBuf = talloc_strdup(mem_ctx, asctime(tm));
#endif

	return TimeBuf;
}

/**
  return a talloced string representing a NTTIME for human consumption
*/
_PUBLIC_ const char *nt_time_string(TALLOC_CTX *mem_ctx, NTTIME nt)
{
	time_t t;
	if (nt == 0) {
		return "NTTIME(0)";
	}
	t = nt_time_to_unix(nt);
	return timestring(mem_ctx, t);
}


/**
  put a NTTIME into a packet
*/
_PUBLIC_ void push_nttime(uint8_t *base, uint16_t offset, NTTIME t)
{
	SBVAL(base, offset,   t);
}

/**
  pull a NTTIME from a packet
*/
_PUBLIC_ NTTIME pull_nttime(uint8_t *base, uint16_t offset)
{
	NTTIME ret = BVAL(base, offset);
	return ret;
}

/**
  return (tv1 - tv2) in microseconds
*/
_PUBLIC_ int64_t usec_time_diff(const struct timeval *tv1, const struct timeval *tv2)
{
	int64_t sec_diff = tv1->tv_sec - tv2->tv_sec;
	return (sec_diff * 1000000) + (int64_t)(tv1->tv_usec - tv2->tv_usec);
}

/**
  return (tp1 - tp2) in microseconds
*/
_PUBLIC_ int64_t nsec_time_diff(const struct timespec *tp1, const struct timespec *tp2)
{
	int64_t sec_diff = tp1->tv_sec - tp2->tv_sec;
	return (sec_diff * 1000000000) + (int64_t)(tp1->tv_nsec - tp2->tv_nsec);
}


/**
  return a zero timeval
*/
_PUBLIC_ struct timeval timeval_zero(void)
{
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	return tv;
}

/**
  return true if a timeval is zero
*/
_PUBLIC_ bool timeval_is_zero(const struct timeval *tv)
{
	return tv->tv_sec == 0 && tv->tv_usec == 0;
}

/**
  return a timeval for the current time
*/
_PUBLIC_ struct timeval timeval_current(void)
{
	struct timeval tv;
	GetTimeOfDay(&tv);
	return tv;
}

/**
  return a timeval struct with the given elements
*/
_PUBLIC_ struct timeval timeval_set(uint32_t secs, uint32_t usecs)
{
	struct timeval tv;
	tv.tv_sec = secs;
	tv.tv_usec = usecs;
	return tv;
}


/**
  return a timeval ofs microseconds after tv
*/
_PUBLIC_ struct timeval timeval_add(const struct timeval *tv,
			   uint32_t secs, uint32_t usecs)
{
	struct timeval tv2 = *tv;
	const unsigned int million = 1000000;
	tv2.tv_sec += secs;
	tv2.tv_usec += usecs;
	tv2.tv_sec += tv2.tv_usec / million;
	tv2.tv_usec = tv2.tv_usec % million;
	return tv2;
}

/**
  return the sum of two timeval structures
*/
struct timeval timeval_sum(const struct timeval *tv1,
			   const struct timeval *tv2)
{
	return timeval_add(tv1, tv2->tv_sec, tv2->tv_usec);
}

/**
  return a timeval secs/usecs into the future
*/
_PUBLIC_ struct timeval timeval_current_ofs(uint32_t secs, uint32_t usecs)
{
	struct timeval tv = timeval_current();
	return timeval_add(&tv, secs, usecs);
}

/**
  compare two timeval structures. 
  Return -1 if tv1 < tv2
  Return 0 if tv1 == tv2
  Return 1 if tv1 > tv2
*/
_PUBLIC_ int timeval_compare(const struct timeval *tv1, const struct timeval *tv2)
{
	if (tv1->tv_sec  > tv2->tv_sec)  return 1;
	if (tv1->tv_sec  < tv2->tv_sec)  return -1;
	if (tv1->tv_usec > tv2->tv_usec) return 1;
	if (tv1->tv_usec < tv2->tv_usec) return -1;
	return 0;
}

/**
  return true if a timer is in the past
*/
_PUBLIC_ bool timeval_expired(const struct timeval *tv)
{
	struct timeval tv2 = timeval_current();
	if (tv2.tv_sec > tv->tv_sec) return true;
	if (tv2.tv_sec < tv->tv_sec) return false;
	return (tv2.tv_usec >= tv->tv_usec);
}

/**
  return the number of seconds elapsed between two times
*/
_PUBLIC_ double timeval_elapsed2(const struct timeval *tv1, const struct timeval *tv2)
{
	return (tv2->tv_sec - tv1->tv_sec) + 
	       (tv2->tv_usec - tv1->tv_usec)*1.0e-6;
}

/**
  return the number of seconds elapsed since a given time
*/
_PUBLIC_ double timeval_elapsed(const struct timeval *tv)
{
	struct timeval tv2 = timeval_current();
	return timeval_elapsed2(tv, &tv2);
}

/**
  return the lesser of two timevals
*/
_PUBLIC_ struct timeval timeval_min(const struct timeval *tv1,
			   const struct timeval *tv2)
{
	if (tv1->tv_sec < tv2->tv_sec) return *tv1;
	if (tv1->tv_sec > tv2->tv_sec) return *tv2;
	if (tv1->tv_usec < tv2->tv_usec) return *tv1;
	return *tv2;
}

/**
  return the greater of two timevals
*/
_PUBLIC_ struct timeval timeval_max(const struct timeval *tv1,
			   const struct timeval *tv2)
{
	if (tv1->tv_sec > tv2->tv_sec) return *tv1;
	if (tv1->tv_sec < tv2->tv_sec) return *tv2;
	if (tv1->tv_usec > tv2->tv_usec) return *tv1;
	return *tv2;
}

/**
  return the difference between two timevals as a timeval
  if tv1 comes after tv2, then return a zero timeval
  (this is *tv2 - *tv1)
*/
_PUBLIC_ struct timeval timeval_until(const struct timeval *tv1,
			     const struct timeval *tv2)
{
	struct timeval t;
	if (timeval_compare(tv1, tv2) >= 0) {
		return timeval_zero();
	}
	t.tv_sec = tv2->tv_sec - tv1->tv_sec;
	if (tv1->tv_usec > tv2->tv_usec) {
		t.tv_sec--;
		t.tv_usec = 1000000 - (tv1->tv_usec - tv2->tv_usec);
	} else {
		t.tv_usec = tv2->tv_usec - tv1->tv_usec;
	}
	return t;
}


/**
  convert a timeval to a NTTIME
*/
_PUBLIC_ NTTIME timeval_to_nttime(const struct timeval *tv)
{
	return 10*(tv->tv_usec + 
		  ((TIME_FIXUP_CONSTANT + (uint64_t)tv->tv_sec) * 1000000));
}

/**
  convert a NTTIME to a timeval
*/
_PUBLIC_ void nttime_to_timeval(struct timeval *tv, NTTIME t)
{
	if (tv == NULL) return;

	t += 10/2;
	t /= 10;
	t -= TIME_FIXUP_CONSTANT*1000*1000;

	tv->tv_sec  = t / 1000000;

	if (TIME_T_MIN > tv->tv_sec || tv->tv_sec > TIME_T_MAX) {
		tv->tv_sec  = 0;
		tv->tv_usec = 0;
		return;
	}
	
	tv->tv_usec = t - tv->tv_sec*1000000;
}

/*******************************************************************
yield the difference between *A and *B, in seconds, ignoring leap seconds
********************************************************************/
static int tm_diff(struct tm *a, struct tm *b)
{
	int ay = a->tm_year + (1900 - 1);
	int by = b->tm_year + (1900 - 1);
	int intervening_leap_days =
		(ay/4 - by/4) - (ay/100 - by/100) + (ay/400 - by/400);
	int years = ay - by;
	int days = 365*years + intervening_leap_days + (a->tm_yday - b->tm_yday);
	int hours = 24*days + (a->tm_hour - b->tm_hour);
	int minutes = 60*hours + (a->tm_min - b->tm_min);
	int seconds = 60*minutes + (a->tm_sec - b->tm_sec);

	return seconds;
}


int extra_time_offset=0;

/**
  return the UTC offset in seconds west of UTC, or 0 if it cannot be determined
 */
_PUBLIC_ int get_time_zone(time_t t)
{
	struct tm *tm = gmtime(&t);
	struct tm tm_utc;
	if (!tm)
		return 0;
	tm_utc = *tm;
	tm = localtime(&t);
	if (!tm)
		return 0;
	return tm_diff(&tm_utc,tm)+60*extra_time_offset;
}

struct timespec nt_time_to_unix_timespec(NTTIME *nt)
{
	int64_t d;
	struct timespec ret;

	if (*nt == 0 || *nt == (int64_t)-1) {
		ret.tv_sec = 0;
		ret.tv_nsec = 0;
		return ret;
	}

	d = (int64_t)*nt;
	/* d is now in 100ns units, since jan 1st 1601".
	   Save off the ns fraction. */

	/*
	 * Take the last seven decimal digits and multiply by 100.
	 * to convert from 100ns units to 1ns units.
	 */
        ret.tv_nsec = (long) ((d % (1000 * 1000 * 10)) * 100);

	/* Convert to seconds */
	d /= 1000*1000*10;

	/* Now adjust by 369 years to make the secs since 1970 */
	d -= TIME_FIXUP_CONSTANT_INT;

	if (d <= (int64_t)TIME_T_MIN) {
		ret.tv_sec = TIME_T_MIN;
		ret.tv_nsec = 0;
		return ret;
	}

	if (d >= (int64_t)TIME_T_MAX) {
		ret.tv_sec = TIME_T_MAX;
		ret.tv_nsec = 0;
		return ret;
	}

	ret.tv_sec = (time_t)d;
	return ret;
}


/**
  check if 2 NTTIMEs are equal.
*/
bool nt_time_equal(NTTIME *t1, NTTIME *t2)
{
	return *t1 == *t2;
}

/**
 Check if it's a null timespec.
**/

bool null_timespec(struct timespec ts)
{
	return ts.tv_sec == 0 || 
		ts.tv_sec == (time_t)0xFFFFFFFF || 
		ts.tv_sec == (time_t)-1;
}


