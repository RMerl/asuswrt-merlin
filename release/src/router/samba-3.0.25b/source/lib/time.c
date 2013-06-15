/* 
   Unix SMB/CIFS implementation.
   time handling functions

   Copyright (C) Andrew Tridgell 		1992-2004
   Copyright (C) Stefan (metze) Metzmacher	2002   
   Copyright (C) Jeremy Allison			2007

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"

/**
 * @file
 * @brief time handling functions
 */


#ifndef TIME_T_MIN
#define TIME_T_MIN ((time_t)0 < (time_t) -1 ? (time_t) 0 \
		    : ~ (time_t) 0 << (sizeof (time_t) * CHAR_BIT - 1))
#endif
#ifndef TIME_T_MAX
#define TIME_T_MAX (~ (time_t) 0 - TIME_T_MIN)
#endif

#define NTTIME_INFINITY (NTTIME)0x8000000000000000LL

/***************************************************************************
 External access to time_t_min and time_t_max.
****************************************************************************/

time_t get_time_t_max(void)
{
	return TIME_T_MAX;
}

/***************************************************************************
 A gettimeofday wrapper.
****************************************************************************/

void GetTimeOfDay(struct timeval *tval)
{
#ifdef HAVE_GETTIMEOFDAY_TZ
	gettimeofday(tval,NULL);
#else
	gettimeofday(tval);
#endif
}

#if (SIZEOF_LONG == 8)
#define TIME_FIXUP_CONSTANT_INT 11644473600L
#elif (SIZEOF_LONG_LONG == 8)
#define TIME_FIXUP_CONSTANT_INT 11644473600LL
#endif

/****************************************************************************
 Interpret an 8 byte "filetime" structure to a time_t
 It's originally in "100ns units since jan 1st 1601"

 An 8 byte value of 0xffffffffffffffff will be returned as a timespec of

	tv_sec = 0
	tv_nsec = 0;

 Returns GMT.
****************************************************************************/

time_t nt_time_to_unix(NTTIME nt)
{
	return convert_timespec_to_time_t(nt_time_to_unix_timespec(&nt));
}

/****************************************************************************
 Put a 8 byte filetime from a time_t. Uses GMT.
****************************************************************************/

void unix_to_nt_time(NTTIME *nt, time_t t)
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

/****************************************************************************
 Check if it's a null unix time.
****************************************************************************/

BOOL null_time(time_t t)
{
	return t == 0 || 
		t == (time_t)0xFFFFFFFF || 
		t == (time_t)-1;
}

/****************************************************************************
 Check if it's a null NTTIME.
****************************************************************************/

BOOL null_nttime(NTTIME t)
{
	return t == 0 || t == (NTTIME)-1;
}

/****************************************************************************
 Check if it's a null timespec.
****************************************************************************/

BOOL null_timespec(struct timespec ts)
{
	return ts.tv_sec == 0 || 
		ts.tv_sec == (time_t)0xFFFFFFFF || 
		ts.tv_sec == (time_t)-1;
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
void push_dos_date(uint8_t *buf, int offset, time_t unixdate, int zone_offset)
{
	uint32_t x = make_dos_date(unixdate, zone_offset);
	SIVAL(buf,offset,x);
}

/**
put a dos date into a buffer (date/time format)
This takes GMT time and puts local time in the buffer
**/
void push_dos_date2(uint8_t *buf,int offset,time_t unixdate, int zone_offset)
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
void push_dos_date3(uint8_t *buf,int offset,time_t unixdate, int zone_offset)
{
	if (!null_time(unixdate)) {
		unixdate -= zone_offset;
	}
	SIVAL(buf,offset,unixdate);
}

/*******************************************************************
  interpret a 32 bit dos packed date/time to some parameters
********************************************************************/
static void interpret_dos_date(uint32_t date,int *year,int *month,int *day,int *hour,int *minute,int *second)
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
time_t pull_dos_date(const uint8_t *date_ptr, int zone_offset)
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
time_t pull_dos_date2(const uint8_t *date_ptr, int zone_offset)
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
time_t pull_dos_date3(const uint8_t *date_ptr, int zone_offset)
{
	time_t t = (time_t)IVAL(date_ptr,0);
	if (!null_time(t)) {
		t += zone_offset;
	}
	return t;
}

/***************************************************************************
 Return a HTTP/1.0 time string.
***************************************************************************/

char *http_timestring(time_t t)
{
	static fstring buf;
	struct tm *tm = localtime(&t);

	if (t == TIME_T_MAX) {
		slprintf(buf,sizeof(buf)-1,"never");
	} else if (!tm) {
		slprintf(buf,sizeof(buf)-1,"%ld seconds since the Epoch",(long)t);
	} else {
#ifndef HAVE_STRFTIME
		const char *asct = asctime(tm);
		fstrcpy(buf, asct ? asct : "unknown");
	}
	if(buf[strlen(buf)-1] == '\n') {
		buf[strlen(buf)-1] = 0;
#else /* !HAVE_STRFTIME */
		strftime(buf, sizeof(buf)-1, "%a, %d %b %Y %H:%M:%S %Z", tm);
#endif /* !HAVE_STRFTIME */
	}
	return buf;
}


/**
 Return the date and time as a string
**/
char *timestring(TALLOC_CTX *mem_ctx, time_t t)
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
	/* some versions of gcc complain about using %c. This is a bug
	   in the gcc warning, not a bug in this code. See a recent
	   strftime() manual page for details.
	 */
	strftime(tempTime,sizeof(tempTime)-1,"%c %Z",tm);
	TimeBuf = talloc_strdup(mem_ctx, tempTime);
#else
	TimeBuf = talloc_strdup(mem_ctx, asctime(tm));
#endif

	return TimeBuf;
}

/**
  return a talloced string representing a NTTIME for human consumption
*/
const char *nt_time_string(TALLOC_CTX *mem_ctx, NTTIME nt)
{
	time_t t;
	if (nt == 0) {
		return "NTTIME(0)";
	}
	t = nt_time_to_unix(nt);
	return timestring(mem_ctx, t);
}


/**
  parse a nttime as a large integer in a string and return a NTTIME
*/
NTTIME nttime_from_string(const char *s)
{
	return strtoull(s, NULL, 0);
}

/**
  return (tv1 - tv2) in microseconds
*/
int64_t usec_time_diff(struct timeval *tv1, struct timeval *tv2)
{
	int64_t sec_diff = tv1->tv_sec - tv2->tv_sec;
	return (sec_diff * 1000000) + (int64_t)(tv1->tv_usec - tv2->tv_usec);
}


/**
  return a zero timeval
*/
struct timeval timeval_zero(void)
{
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	return tv;
}

/**
  return True if a timeval is zero
*/
BOOL timeval_is_zero(const struct timeval *tv)
{
	return tv->tv_sec == 0 && tv->tv_usec == 0;
}

/**
  return a timeval for the current time
*/
struct timeval timeval_current(void)
{
	struct timeval tv;
	GetTimeOfDay(&tv);
	return tv;
}

/**
  return a timeval struct with the given elements
*/
struct timeval timeval_set(uint32_t secs, uint32_t usecs)
{
	struct timeval tv;
	tv.tv_sec = secs;
	tv.tv_usec = usecs;
	return tv;
}


/**
  return a timeval ofs microseconds after tv
*/
struct timeval timeval_add(const struct timeval *tv,
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
struct timeval timeval_current_ofs(uint32_t secs, uint32_t usecs)
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
int timeval_compare(const struct timeval *tv1, const struct timeval *tv2)
{
	if (tv1->tv_sec  > tv2->tv_sec)  return 1;
	if (tv1->tv_sec  < tv2->tv_sec)  return -1;
	if (tv1->tv_usec > tv2->tv_usec) return 1;
	if (tv1->tv_usec < tv2->tv_usec) return -1;
	return 0;
}

/**
  return True if a timer is in the past
*/
BOOL timeval_expired(const struct timeval *tv)
{
	struct timeval tv2 = timeval_current();
	if (tv2.tv_sec > tv->tv_sec) return True;
	if (tv2.tv_sec < tv->tv_sec) return False;
	return (tv2.tv_usec >= tv->tv_usec);
}

/**
  return the number of seconds elapsed between two times
*/
double timeval_elapsed2(const struct timeval *tv1, const struct timeval *tv2)
{
	return (tv2->tv_sec - tv1->tv_sec) + 
	       (tv2->tv_usec - tv1->tv_usec)*1.0e-6;
}

/**
  return the number of seconds elapsed since a given time
*/
double timeval_elapsed(const struct timeval *tv)
{
	struct timeval tv2 = timeval_current();
	return timeval_elapsed2(tv, &tv2);
}

/**
  return the lesser of two timevals
*/
struct timeval timeval_min(const struct timeval *tv1,
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
struct timeval timeval_max(const struct timeval *tv1,
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
struct timeval timeval_until(const struct timeval *tv1,
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
NTTIME timeval_to_nttime(const struct timeval *tv)
{
	return 10*(tv->tv_usec + 
		  ((TIME_FIXUP_CONSTANT_INT + (uint64_t)tv->tv_sec) * 1000000));
}

/**************************************************************
 Handle conversions between time_t and uint32, taking care to
 preserve the "special" values.
**************************************************************/

uint32 convert_time_t_to_uint32(time_t t)
{
#if (defined(SIZEOF_TIME_T) && (SIZEOF_TIME_T == 8))
	/* time_t is 64-bit. */
	if (t == 0x8000000000000000LL) {
		return 0x80000000;
	} else if (t == 0x7FFFFFFFFFFFFFFFLL) {
		return 0x7FFFFFFF;
	}
#endif
	return (uint32)t;
}

time_t convert_uint32_to_time_t(uint32 u)
{
#if (defined(SIZEOF_TIME_T) && (SIZEOF_TIME_T == 8))
	/* time_t is 64-bit. */
	if (u == 0x80000000) {
		return (time_t)0x8000000000000000LL;
	} else if (u == 0x7FFFFFFF) {
		return (time_t)0x7FFFFFFFFFFFFFFFLL;
	}
#endif
	return (time_t)u;
}

/*******************************************************************
 Yield the difference between *A and *B, in seconds, ignoring leap seconds.
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

/*******************************************************************
 Return the UTC offset in seconds west of UTC, or 0 if it cannot be determined.
********************************************************************/

int get_time_zone(time_t t)
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

/****************************************************************************
 Check if NTTIME is 0.
****************************************************************************/

BOOL nt_time_is_zero(const NTTIME *nt)
{
	return (*nt == 0);
}

/****************************************************************************
 Convert ASN.1 GeneralizedTime string to unix-time.
 Returns 0 on failure; Currently ignores timezone. 
****************************************************************************/

time_t generalized_to_unix_time(const char *str)
{ 
	struct tm tm;

	ZERO_STRUCT(tm);

	if (sscanf(str, "%4d%2d%2d%2d%2d%2d", 
		   &tm.tm_year, &tm.tm_mon, &tm.tm_mday, 
		   &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
		return 0;
	}
	tm.tm_year -= 1900;
	tm.tm_mon -= 1;

	return timegm(&tm);
}

/*******************************************************************
 Accessor function for the server time zone offset.
 set_server_zone_offset() must have been called first.
******************************************************************/

static int server_zone_offset;

int get_server_zone_offset(void)
{
	return server_zone_offset;
}

/*******************************************************************
 Initialize the server time zone offset. Called when a client connects.
******************************************************************/

int set_server_zone_offset(time_t t)
{
	server_zone_offset = get_time_zone(t);
	return server_zone_offset;
}

/****************************************************************************
 Return the date and time as a string
****************************************************************************/

char *current_timestring(BOOL hires)
{
	static fstring TimeBuf;
	struct timeval tp;
	time_t t;
	struct tm *tm;

	if (hires) {
		GetTimeOfDay(&tp);
		t = (time_t)tp.tv_sec;
	} else {
		t = time(NULL);
	}
	tm = localtime(&t);
	if (!tm) {
		if (hires) {
			slprintf(TimeBuf,
				 sizeof(TimeBuf)-1,
				 "%ld.%06ld seconds since the Epoch",
				 (long)tp.tv_sec, 
				 (long)tp.tv_usec);
		} else {
			slprintf(TimeBuf,
				 sizeof(TimeBuf)-1,
				 "%ld seconds since the Epoch",
				 (long)t);
		}
	} else {
#ifdef HAVE_STRFTIME
		if (hires) {
			strftime(TimeBuf,sizeof(TimeBuf)-1,"%Y/%m/%d %H:%M:%S",tm);
			slprintf(TimeBuf+strlen(TimeBuf),
				 sizeof(TimeBuf)-1 - strlen(TimeBuf), 
				 ".%06ld", 
				 (long)tp.tv_usec);
		} else {
			strftime(TimeBuf,sizeof(TimeBuf)-1,"%Y/%m/%d %H:%M:%S",tm);
		}
#else
		if (hires) {
			const char *asct = asctime(tm);
			slprintf(TimeBuf, 
				 sizeof(TimeBuf)-1, 
				 "%s.%06ld", 
				 asct ? asct : "unknown", 
				 (long)tp.tv_usec);
		} else {
			const char *asct = asctime(tm);
			fstrcpy(TimeBuf, asct ? asct : "unknown");
		}
#endif
	}
	return(TimeBuf);
}


/*******************************************************************
 Put a dos date into a buffer (time/date format).
 This takes GMT time and puts local time in the buffer.
********************************************************************/

static void put_dos_date(char *buf,int offset,time_t unixdate, int zone_offset)
{
	uint32 x = make_dos_date(unixdate, zone_offset);
	SIVAL(buf,offset,x);
}

/*******************************************************************
 Put a dos date into a buffer (date/time format).
 This takes GMT time and puts local time in the buffer.
********************************************************************/

static void put_dos_date2(char *buf,int offset,time_t unixdate, int zone_offset)
{
	uint32 x = make_dos_date(unixdate, zone_offset);
	x = ((x&0xFFFF)<<16) | ((x&0xFFFF0000)>>16);
	SIVAL(buf,offset,x);
}

/*******************************************************************
 Put a dos 32 bit "unix like" date into a buffer. This routine takes
 GMT and converts it to LOCAL time before putting it (most SMBs assume
 localtime for this sort of date)
********************************************************************/

static void put_dos_date3(char *buf,int offset,time_t unixdate, int zone_offset)
{
	if (!null_mtime(unixdate)) {
		unixdate -= zone_offset;
	}
	SIVAL(buf,offset,unixdate);
}


/***************************************************************************
 Server versions of the above functions.
***************************************************************************/

void srv_put_dos_date(char *buf,int offset,time_t unixdate)
{
	put_dos_date(buf, offset, unixdate, server_zone_offset);
}

void srv_put_dos_date2(char *buf,int offset, time_t unixdate)
{
	put_dos_date2(buf, offset, unixdate, server_zone_offset);
}

void srv_put_dos_date3(char *buf,int offset,time_t unixdate)
{
	put_dos_date3(buf, offset, unixdate, server_zone_offset);
}

/****************************************************************************
 Take a Unix time and convert to an NTTIME structure and place in buffer 
 pointed to by p.
****************************************************************************/

void put_long_date_timespec(char *p, struct timespec ts)
{
	NTTIME nt;
	unix_timespec_to_nt_time(&nt, ts);
	SIVAL(p, 0, nt & 0xFFFFFFFF);
	SIVAL(p, 4, nt >> 32);
}

void put_long_date(char *p, time_t t)
{
	struct timespec ts;
	ts.tv_sec = t;
	ts.tv_nsec = 0;
	put_long_date_timespec(p, ts);
}

/****************************************************************************
 Return the best approximation to a 'create time' under UNIX from a stat
 structure.
****************************************************************************/

time_t get_create_time(const SMB_STRUCT_STAT *st,BOOL fake_dirs)
{
	time_t ret, ret1;

	if(S_ISDIR(st->st_mode) && fake_dirs) {
		return (time_t)315493200L;          /* 1/1/1980 */
	}
    
	ret = MIN(st->st_ctime, st->st_mtime);
	ret1 = MIN(ret, st->st_atime);

	if(ret1 != (time_t)0) {
		return ret1;
	}

	/*
	 * One of ctime, mtime or atime was zero (probably atime).
	 * Just return MIN(ctime, mtime).
	 */
	return ret;
}

struct timespec get_create_timespec(const SMB_STRUCT_STAT *st,BOOL fake_dirs)
{
	struct timespec ts;
	ts.tv_sec = get_create_time(st, fake_dirs);
	ts.tv_nsec = 0;
	return ts;
}

/****************************************************************************
 Get/Set all the possible time fields from a stat struct as a timespec.
****************************************************************************/

struct timespec get_atimespec(const SMB_STRUCT_STAT *pst)
{
#if !defined(HAVE_STAT_HIRES_TIMESTAMPS)
	struct timespec ret;

	/* Old system - no ns timestamp. */
	ret.tv_sec = pst->st_atime;
	ret.tv_nsec = 0;
	return ret;
#else
#if defined(HAVE_STAT_ST_ATIM)
	return pst->st_atim;
#elif defined(HAVE_STAT_ST_ATIMENSEC)
	struct timespec ret;
	ret.tv_sec = pst->st_atime;
	ret.tv_nsec = pst->st_atimensec;
	return ret;
#else
#error	CONFIGURE_ERROR_IN_DETECTING_TIMESPEC_IN_STAT 
#endif
#endif
}

void set_atimespec(SMB_STRUCT_STAT *pst, struct timespec ts)
{
#if !defined(HAVE_STAT_HIRES_TIMESTAMPS)
	/* Old system - no ns timestamp. */
	pst->st_atime = ts.tv_sec;
#else
#if defined(HAVE_STAT_ST_ATIM)
	pst->st_atim = ts;
#elif defined(HAVE_STAT_ST_ATIMENSEC)
	pst->st_atime = ts.tv_sec;
	pst->st_atimensec = ts.tv_nsec
#else
#error	CONFIGURE_ERROR_IN_DETECTING_TIMESPEC_IN_STAT 
#endif
#endif
}

struct timespec get_mtimespec(const SMB_STRUCT_STAT *pst)
{
#if !defined(HAVE_STAT_HIRES_TIMESTAMPS)
	struct timespec ret;

	/* Old system - no ns timestamp. */
	ret.tv_sec = pst->st_mtime;
	ret.tv_nsec = 0;
	return ret;
#else
#if defined(HAVE_STAT_ST_MTIM)
	return pst->st_mtim;
#elif defined(HAVE_STAT_ST_MTIMENSEC)
	struct timespec ret;
	ret.tv_sec = pst->st_mtime;
	ret.tv_nsec = pst->st_mtimensec;
	return ret;
#else
#error	CONFIGURE_ERROR_IN_DETECTING_TIMESPEC_IN_STAT 
#endif
#endif
}

void set_mtimespec(SMB_STRUCT_STAT *pst, struct timespec ts)
{
#if !defined(HAVE_STAT_HIRES_TIMESTAMPS)
	/* Old system - no ns timestamp. */
	pst->st_mtime = ts.tv_sec;
#else
#if defined(HAVE_STAT_ST_MTIM)
	pst->st_mtim = ts;
#elif defined(HAVE_STAT_ST_MTIMENSEC)
	pst->st_mtime = ts.tv_sec;
	pst->st_mtimensec = ts.tv_nsec
#else
#error	CONFIGURE_ERROR_IN_DETECTING_TIMESPEC_IN_STAT 
#endif
#endif
}

struct timespec get_ctimespec(const SMB_STRUCT_STAT *pst)
{
#if !defined(HAVE_STAT_HIRES_TIMESTAMPS)
	struct timespec ret;

	/* Old system - no ns timestamp. */
	ret.tv_sec = pst->st_ctime;
	ret.tv_nsec = 0;
	return ret;
#else
#if defined(HAVE_STAT_ST_CTIM)
	return pst->st_ctim;
#elif defined(HAVE_STAT_ST_CTIMENSEC)
	struct timespec ret;
	ret.tv_sec = pst->st_ctime;
	ret.tv_nsec = pst->st_ctimensec;
	return ret;
#else
#error	CONFIGURE_ERROR_IN_DETECTING_TIMESPEC_IN_STAT 
#endif
#endif
}

void set_ctimespec(SMB_STRUCT_STAT *pst, struct timespec ts)
{
#if !defined(HAVE_STAT_HIRES_TIMESTAMPS)
	/* Old system - no ns timestamp. */
	pst->st_ctime = ts.tv_sec;
#else
#if defined(HAVE_STAT_ST_CTIM)
	pst->st_ctim = ts;
#elif defined(HAVE_STAT_ST_CTIMENSEC)
	pst->st_ctime = ts.tv_sec;
	pst->st_ctimensec = ts.tv_nsec
#else
#error	CONFIGURE_ERROR_IN_DETECTING_TIMESPEC_IN_STAT 
#endif
#endif
}

void dos_filetime_timespec(struct timespec *tsp)
{
	tsp->tv_sec &= ~1;
	tsp->tv_nsec = 0;
}

/*******************************************************************
 Create a unix date (int GMT) from a dos date (which is actually in
 localtime).
********************************************************************/

static time_t make_unix_date(const void *date_ptr, int zone_offset)
{
	uint32 dos_date=0;
	struct tm t;
	time_t ret;

	dos_date = IVAL(date_ptr,0);

	if (dos_date == 0) {
		return 0;
	}
  
	interpret_dos_date(dos_date,&t.tm_year,&t.tm_mon,
			&t.tm_mday,&t.tm_hour,&t.tm_min,&t.tm_sec);
	t.tm_isdst = -1;
  
	ret = timegm(&t);

	ret += zone_offset;

	return(ret);
}

/*******************************************************************
 Like make_unix_date() but the words are reversed.
********************************************************************/

static time_t make_unix_date2(const void *date_ptr, int zone_offset)
{
	uint32 x,x2;

	x = IVAL(date_ptr,0);
	x2 = ((x&0xFFFF)<<16) | ((x&0xFFFF0000)>>16);
	SIVAL(&x,0,x2);

	return(make_unix_date((const void *)&x, zone_offset));
}

/*******************************************************************
 Create a unix GMT date from a dos date in 32 bit "unix like" format
 these generally arrive as localtimes, with corresponding DST.
******************************************************************/

static time_t make_unix_date3(const void *date_ptr, int zone_offset)
{
	time_t t = (time_t)IVAL(date_ptr,0);
	if (!null_mtime(t)) {
		t += zone_offset;
	}
	return(t);
}

time_t srv_make_unix_date(const void *date_ptr)
{
	return make_unix_date(date_ptr, server_zone_offset);
}

time_t srv_make_unix_date2(const void *date_ptr)
{
	return make_unix_date2(date_ptr, server_zone_offset);
}

time_t srv_make_unix_date3(const void *date_ptr)
{
	return make_unix_date3(date_ptr, server_zone_offset);
}

time_t convert_timespec_to_time_t(struct timespec ts)
{
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

/****************************************************************************
 Convert a normalized timeval to a timespec.
****************************************************************************/

struct timespec convert_timeval_to_timespec(const struct timeval tv)
{
	struct timespec ts;
	ts.tv_sec = tv.tv_sec;
	ts.tv_nsec = tv.tv_usec * 1000;
	return ts;
}

/****************************************************************************
 Convert a normalized timespec to a timeval.
****************************************************************************/

struct timeval convert_timespec_to_timeval(const struct timespec ts)
{
	struct timeval tv;
	tv.tv_sec = ts.tv_sec;
	tv.tv_usec = ts.tv_nsec / 1000;
	return tv;
}

/****************************************************************************
 Return a timespec for the current time
****************************************************************************/

struct timespec timespec_current(void)
{
	struct timeval tv;
	struct timespec ts;
	GetTimeOfDay(&tv);
	ts.tv_sec = tv.tv_sec;
	ts.tv_nsec = tv.tv_usec * 1000;
	return ts;
}

/****************************************************************************
 Return the lesser of two timespecs.
****************************************************************************/

struct timespec timespec_min(const struct timespec *ts1,
			   const struct timespec *ts2)
{
	if (ts1->tv_sec < ts2->tv_sec) return *ts1;
	if (ts1->tv_sec > ts2->tv_sec) return *ts2;
	if (ts1->tv_nsec < ts2->tv_nsec) return *ts1;
	return *ts2;
}

/****************************************************************************
  compare two timespec structures. 
  Return -1 if ts1 < ts2
  Return 0 if ts1 == ts2
  Return 1 if ts1 > ts2
****************************************************************************/

int timespec_compare(const struct timespec *ts1, const struct timespec *ts2)
{
	if (ts1->tv_sec  > ts2->tv_sec)  return 1;
	if (ts1->tv_sec  < ts2->tv_sec)  return -1;
	if (ts1->tv_nsec > ts2->tv_nsec) return 1;
	if (ts1->tv_nsec < ts2->tv_nsec) return -1;
	return 0;
}

/****************************************************************************
 Interprets an nt time into a unix struct timespec.
 Differs from nt_time_to_unix in that an 8 byte value of 0xffffffffffffffff
 will be returned as (time_t)-1, whereas nt_time_to_unix returns 0 in this case.
****************************************************************************/

struct timespec interpret_long_date(const char *p)
{
	NTTIME nt;
	nt = IVAL(p,0) + ((uint64_t)IVAL(p,4) << 32);
	if (nt == (uint64_t)-1) {
		struct timespec ret;
		ret.tv_sec = (time_t)-1;
		ret.tv_nsec = 0;
		return ret;
	}
	return nt_time_to_unix_timespec(&nt);
}

/***************************************************************************
 Client versions of the above functions.
***************************************************************************/

void cli_put_dos_date(struct cli_state *cli, char *buf, int offset, time_t unixdate)
{
	put_dos_date(buf, offset, unixdate, cli->serverzone);
}

void cli_put_dos_date2(struct cli_state *cli, char *buf, int offset, time_t unixdate)
{
	put_dos_date2(buf, offset, unixdate, cli->serverzone);
}

void cli_put_dos_date3(struct cli_state *cli, char *buf, int offset, time_t unixdate)
{
	put_dos_date3(buf, offset, unixdate, cli->serverzone);
}

time_t cli_make_unix_date(struct cli_state *cli, void *date_ptr)
{
	return make_unix_date(date_ptr, cli->serverzone);
}

time_t cli_make_unix_date2(struct cli_state *cli, void *date_ptr)
{
	return make_unix_date2(date_ptr, cli->serverzone);
}

time_t cli_make_unix_date3(struct cli_state *cli, void *date_ptr)
{
	return make_unix_date3(date_ptr, cli->serverzone);
}

/* Large integer version. */
struct timespec nt_time_to_unix_timespec(NTTIME *nt)
{
	int64 d;
	struct timespec ret;

	if (*nt == 0 || *nt == (int64)-1) {
		ret.tv_sec = 0;
		ret.tv_nsec = 0;
		return ret;
	}

	d = (int64)*nt;
	/* d is now in 100ns units, since jan 1st 1601".
	   Save off the ns fraction. */
	
	ret.tv_nsec = (long) ((d % 100) * 100);

	/* Convert to seconds */
	d /= 1000*1000*10;

	/* Now adjust by 369 years to make the secs since 1970 */
	d -= TIME_FIXUP_CONSTANT_INT;

	if (d <= (int64)TIME_T_MIN) {
		ret.tv_sec = TIME_T_MIN;
		ret.tv_nsec = 0;
		return ret;
	}

	if (d >= (int64)TIME_T_MAX) {
		ret.tv_sec = TIME_T_MAX;
		ret.tv_nsec = 0;
		return ret;
	}

	ret.tv_sec = (time_t)d;
	return ret;
}
/****************************************************************************
 Check if two NTTIMEs are the same.
****************************************************************************/

BOOL nt_time_equals(const NTTIME *nt1, const NTTIME *nt2)
{
	return (*nt1 == *nt2);
}

/*******************************************************************
 Re-read the smb serverzone value.
******************************************************************/

static struct timeval start_time_hires;

void TimeInit(void)
{
	set_server_zone_offset(time(NULL));

	DEBUG(4,("TimeInit: Serverzone is %d\n", server_zone_offset));

	/* Save the start time of this process. */
	if (start_time_hires.tv_sec == 0 && start_time_hires.tv_usec == 0) {
		GetTimeOfDay(&start_time_hires);
	}
}

/**********************************************************************
 Return a timeval struct of the uptime of this process. As TimeInit is
 done before a daemon fork then this is the start time from the parent
 daemon start. JRA.
***********************************************************************/

void get_process_uptime(struct timeval *ret_time)
{
	struct timeval time_now_hires;

	GetTimeOfDay(&time_now_hires);
	ret_time->tv_sec = time_now_hires.tv_sec - start_time_hires.tv_sec;
	if (time_now_hires.tv_usec < start_time_hires.tv_usec) {
		ret_time->tv_sec -= 1;
		ret_time->tv_usec = 1000000 + (time_now_hires.tv_usec - start_time_hires.tv_usec);
	} else {
		ret_time->tv_usec = time_now_hires.tv_usec - start_time_hires.tv_usec;
	}
}

/****************************************************************************
 Convert a NTTIME structure to a time_t.
 It's originally in "100ns units".

 This is an absolute version of the one above.
 By absolute I mean, it doesn't adjust from 1/1/1601 to 1/1/1970
 if the NTTIME was 5 seconds, the time_t is 5 seconds. JFM
****************************************************************************/

time_t nt_time_to_unix_abs(const NTTIME *nt)
{
	uint64 d;

	if (*nt == 0) {
		return (time_t)0;
	}

	if (*nt == (uint64)-1) {
		return (time_t)-1;
	}

	if (*nt == NTTIME_INFINITY) {
		return (time_t)-1;
	}

	/* reverse the time */
	/* it's a negative value, turn it to positive */
	d=~*nt;

	d += 1000*1000*10/2;
	d /= 1000*1000*10;

	if (!(TIME_T_MIN <= ((time_t)d) && ((time_t)d) <= TIME_T_MAX)) {
		return (time_t)0;
	}

	return (time_t)d;
}

/****************************************************************************
 Put a 8 byte filetime from a struct timespec. Uses GMT.
****************************************************************************/

void unix_timespec_to_nt_time(NTTIME *nt, struct timespec ts)
{
	uint64 d;

	if (ts.tv_sec ==0 && ts.tv_nsec == 0) {
		*nt = 0;
		return;
	}
	if (ts.tv_sec == TIME_T_MAX) {
		*nt = 0x7fffffffffffffffLL;
		return;
	}		
	if (ts.tv_sec == (time_t)-1) {
		*nt = (uint64)-1;
		return;
	}		

	d = ts.tv_sec;
	d += TIME_FIXUP_CONSTANT_INT;
	d *= 1000*1000*10;
	/* d is now in 100ns units. */
	d += (ts.tv_nsec / 100);

	*nt = d;
}

/****************************************************************************
 Convert a time_t to a NTTIME structure

 This is an absolute version of the one above.
 By absolute I mean, it doesn't adjust from 1/1/1970 to 1/1/1601
 If the time_t was 5 seconds, the NTTIME is 5 seconds. JFM
****************************************************************************/

void unix_to_nt_time_abs(NTTIME *nt, time_t t)
{
	double d;

	if (t==0) {
		*nt = 0;
		return;
	}

	if (t == TIME_T_MAX) {
		*nt = 0x7fffffffffffffffLL;
		return;
	}
		
	if (t == (time_t)-1) {
		/* that's what NT uses for infinite */
		*nt = NTTIME_INFINITY;
		return;
	}		

	d = (double)(t);
	d *= 1.0e7;

	*nt = d;

	/* convert to a negative value */
	*nt=~*nt;
}


/****************************************************************************
 Check if it's a null mtime.
****************************************************************************/

BOOL null_mtime(time_t mtime)
{
	if (mtime == 0 || mtime == (time_t)0xFFFFFFFF || mtime == (time_t)-1)
		return(True);
	return(False);
}

/****************************************************************************
 Utility function that always returns a const string even if localtime
 and asctime fail.
****************************************************************************/

const char *time_to_asc(const time_t t)
{
	const char *asct;
	struct tm *lt = localtime(&t);

	if (!lt) {
		return "unknown time";
	}

	asct = asctime(lt);
	if (!asct) {
		return "unknown time";
	}
	return asct;
}

const char *display_time(NTTIME nttime)
{
	static fstring string;

	float high;
	float low;
	int sec;
	int days, hours, mins, secs;

	if (nttime==0)
		return "Now";

	if (nttime==NTTIME_INFINITY)
		return "Never";

	high = 65536;	
	high = high/10000;
	high = high*65536;
	high = high/1000;
	high = high * (~(nttime >> 32));

	low = ~(nttime & 0xFFFFFFFF);
	low = low/(1000*1000*10);

	sec=high+low;

	days=sec/(60*60*24);
	hours=(sec - (days*60*60*24)) / (60*60);
	mins=(sec - (days*60*60*24) - (hours*60*60) ) / 60;
	secs=sec - (days*60*60*24) - (hours*60*60) - (mins*60);

	fstr_sprintf(string, "%u days, %u hours, %u minutes, %u seconds", days, hours, mins, secs);
	return (string);
}

BOOL nt_time_is_set(const NTTIME *nt)
{
	if (*nt == 0x7FFFFFFFFFFFFFFFLL) {
		return False;
	}

	if (*nt == NTTIME_INFINITY) {
		return False;
	}

	return True;
}
