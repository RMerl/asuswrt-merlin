/* 
   Unix SMB/CIFS implementation.
   time handling functions

   Copyright (C) Andrew Tridgell 		1992-2004
   Copyright (C) Stefan (metze) Metzmacher	2002   
   Copyright (C) Jeremy Allison			2007

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

/**
 * @file
 * @brief time handling functions
 */


#define NTTIME_INFINITY (NTTIME)0x8000000000000000LL

#if (SIZEOF_LONG == 8)
#define TIME_FIXUP_CONSTANT_INT 11644473600L
#elif (SIZEOF_LONG_LONG == 8)
#define TIME_FIXUP_CONSTANT_INT 11644473600LL
#endif


/**
  parse a nttime as a large integer in a string and return a NTTIME
*/
NTTIME nttime_from_string(const char *s)
{
	return strtoull(s, NULL, 0);
}

/**************************************************************
 Handle conversions between time_t and uint32, taking care to
 preserve the "special" values.
**************************************************************/

uint32_t convert_time_t_to_uint32_t(time_t t)
{
#if (defined(SIZEOF_TIME_T) && (SIZEOF_TIME_T == 8))
	/* time_t is 64-bit. */
	if (t == 0x8000000000000000LL) {
		return 0x80000000;
	} else if (t == 0x7FFFFFFFFFFFFFFFLL) {
		return 0x7FFFFFFF;
	}
#endif
	return (uint32_t)t;
}

time_t convert_uint32_t_to_time_t(uint32_t u)
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

/****************************************************************************
 Check if NTTIME is 0.
****************************************************************************/

bool nt_time_is_zero(const NTTIME *nt)
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

/***************************************************************************
 Server versions of the above functions.
***************************************************************************/

void srv_put_dos_date(char *buf,int offset,time_t unixdate)
{
	push_dos_date((uint8_t *)buf, offset, unixdate, server_zone_offset);
}

void srv_put_dos_date2(char *buf,int offset, time_t unixdate)
{
	push_dos_date2((uint8_t *)buf, offset, unixdate, server_zone_offset);
}

void srv_put_dos_date3(char *buf,int offset,time_t unixdate)
{
	push_dos_date3((uint8_t *)buf, offset, unixdate, server_zone_offset);
}

void round_timespec(enum timestamp_set_resolution res, struct timespec *ts)
{
	switch (res) {
		case TIMESTAMP_SET_SECONDS:
			round_timespec_to_sec(ts);
			break;
		case TIMESTAMP_SET_MSEC:
			round_timespec_to_usec(ts);
			break;
		case TIMESTAMP_SET_NT_OR_BETTER:
			/* No rounding needed. */
			break;
        }
}

/****************************************************************************
 Take a Unix time and convert to an NTTIME structure and place in buffer 
 pointed to by p, rounded to the correct resolution.
****************************************************************************/

void put_long_date_timespec(enum timestamp_set_resolution res, char *p, struct timespec ts)
{
	NTTIME nt;
	round_timespec(res, &ts);
	unix_timespec_to_nt_time(&nt, ts);
	SIVAL(p, 0, nt & 0xFFFFFFFF);
	SIVAL(p, 4, nt >> 32);
}

void put_long_date(char *p, time_t t)
{
	struct timespec ts;
	ts.tv_sec = t;
	ts.tv_nsec = 0;
	put_long_date_timespec(TIMESTAMP_SET_SECONDS, p, ts);
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

time_t make_unix_date(const void *date_ptr, int zone_offset)
{
	uint32_t dos_date=0;
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

time_t make_unix_date2(const void *date_ptr, int zone_offset)
{
	uint32_t x,x2;

	x = IVAL(date_ptr,0);
	x2 = ((x&0xFFFF)<<16) | ((x&0xFFFF0000)>>16);
	SIVAL(&x,0,x2);

	return(make_unix_date((const void *)&x, zone_offset));
}

/*******************************************************************
 Create a unix GMT date from a dos date in 32 bit "unix like" format
 these generally arrive as localtimes, with corresponding DST.
******************************************************************/

time_t make_unix_date3(const void *date_ptr, int zone_offset)
{
	time_t t = (time_t)IVAL(date_ptr,0);
	if (!null_time(t)) {
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
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
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
 Round up a timespec if nsec > 500000000, round down if lower,
 then zero nsec.
****************************************************************************/

void round_timespec_to_sec(struct timespec *ts)
{
	ts->tv_sec = convert_timespec_to_time_t(*ts);
	ts->tv_nsec = 0;
}

/****************************************************************************
 Round a timespec to usec value.
****************************************************************************/

void round_timespec_to_usec(struct timespec *ts)
{
	struct timeval tv = convert_timespec_to_timeval(*ts);
	*ts = convert_timeval_to_timespec(tv);
	while (ts->tv_nsec > 1000000000) {
		ts->tv_sec += 1;
		ts->tv_nsec -= 1000000000;
	}
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

/**
 * @brief Get the startup time of the server.
 *
 * @param[out] ret_time A pointer to a timveal structure to set the startup
 *                      time.
 */
void get_startup_time(struct timeval *ret_time)
{
	ret_time->tv_sec = start_time_hires.tv_sec;
	ret_time->tv_usec = start_time_hires.tv_usec;
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
	uint64_t d;

	if (*nt == 0) {
		return (time_t)0;
	}

	if (*nt == (uint64_t)-1) {
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

time_t uint64s_nt_time_to_unix_abs(const uint64_t *src)
{
	NTTIME nttime;
	nttime = *src;
	return nt_time_to_unix_abs(&nttime);
}

/****************************************************************************
 Put a 8 byte filetime from a struct timespec. Uses GMT.
****************************************************************************/

void unix_timespec_to_nt_time(NTTIME *nt, struct timespec ts)
{
	uint64_t d;

	if (ts.tv_sec ==0 && ts.tv_nsec == 0) {
		*nt = 0;
		return;
	}
	if (ts.tv_sec == TIME_T_MAX) {
		*nt = 0x7fffffffffffffffLL;
		return;
	}		
	if (ts.tv_sec == (time_t)-1) {
		*nt = (uint64_t)-1;
		return;
	}		

	d = ts.tv_sec;
	d += TIME_FIXUP_CONSTANT_INT;
	d *= 1000*1000*10;
	/* d is now in 100ns units. */
	d += (ts.tv_nsec / 100);

	*nt = d;
}

#if 0
void nt_time_to_unix_timespec(struct timespec *ts, NTTIME t)
{
	if (ts == NULL) {
		return;
	}

	/* t starts in 100 nsec units since 1601-01-01. */

	t *= 100;
	/* t is now in nsec units since 1601-01-01. */

	t -= TIME_FIXUP_CONSTANT*1000*1000*100;
	/* t is now in nsec units since the UNIX epoch 1970-01-01. */

	ts->tv_sec  = t / 1000000000LL;

	if (TIME_T_MIN > ts->tv_sec || ts->tv_sec > TIME_T_MAX) {
		ts->tv_sec  = 0;
		ts->tv_nsec = 0;
		return;
	}

	ts->tv_nsec = t - ts->tv_sec*1000000000LL;
}
#endif

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

	*nt = (NTTIME)d;

	/* convert to a negative value */
	*nt=~*nt;
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

	sec=(int)(high+low);

	days=sec/(60*60*24);
	hours=(sec - (days*60*60*24)) / (60*60);
	mins=(sec - (days*60*60*24) - (hours*60*60) ) / 60;
	secs=sec - (days*60*60*24) - (hours*60*60) - (mins*60);

	return talloc_asprintf(talloc_tos(), "%u days, %u hours, %u minutes, "
			       "%u seconds", days, hours, mins, secs);
}

bool nt_time_is_set(const NTTIME *nt)
{
	if (*nt == 0x7FFFFFFFFFFFFFFFLL) {
		return false;
	}

	if (*nt == NTTIME_INFINITY) {
		return false;
	}

	return true;
}
