/* 
   Unix SMB/CIFS implementation.
   time utility functions

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

#ifndef _SAMBA_TIME_H_
#define _SAMBA_TIME_H_

#ifndef _PUBLIC_
#define _PUBLIC_
#endif

#ifndef TIME_T_MIN
/* we use 0 here, because (time_t)-1 means error */
#define TIME_T_MIN 0
#endif

/*
 * we use the INT32_MAX here as on 64 bit systems,
 * gmtime() fails with INT64_MAX
 */
#ifndef TIME_T_MAX
#define TIME_T_MAX MIN(INT32_MAX,_TYPE_MAXIMUM(time_t))
#endif


/* 64 bit time (100 nanosec) 1601 - cifs6.txt, section 3.5, page 30, 4 byte aligned */
typedef uint64_t NTTIME;

/**
 External access to time_t_min and time_t_max.
**/
_PUBLIC_ time_t get_time_t_max(void);

/**
a gettimeofday wrapper
**/
_PUBLIC_ void GetTimeOfDay(struct timeval *tval);

/**
a wrapper to preferably get the monotonic time
**/
_PUBLIC_ void clock_gettime_mono(struct timespec *tp);

/**
a wrapper to preferably get the monotonic time in s
**/
_PUBLIC_ time_t time_mono(time_t *t);

/**
interpret an 8 byte "filetime" structure to a time_t
It's originally in "100ns units since jan 1st 1601"
**/
_PUBLIC_ time_t nt_time_to_unix(NTTIME nt);

/**
put a 8 byte filetime from a time_t
This takes GMT as input
**/
_PUBLIC_ void unix_to_nt_time(NTTIME *nt, time_t t);

/**
check if it's a null unix time
**/
_PUBLIC_ bool null_time(time_t t);

/**
check if it's a null NTTIME
**/
_PUBLIC_ bool null_nttime(NTTIME t);

/**
put a dos date into a buffer (time/date format)
This takes GMT time and puts local time in the buffer
**/
_PUBLIC_ void push_dos_date(uint8_t *buf, int offset, time_t unixdate, int zone_offset);

/**
put a dos date into a buffer (date/time format)
This takes GMT time and puts local time in the buffer
**/
_PUBLIC_ void push_dos_date2(uint8_t *buf,int offset,time_t unixdate, int zone_offset);

/**
put a dos 32 bit "unix like" date into a buffer. This routine takes
GMT and converts it to LOCAL time before putting it (most SMBs assume
localtime for this sort of date)
**/
_PUBLIC_ void push_dos_date3(uint8_t *buf,int offset,time_t unixdate, int zone_offset);

/**
  create a unix date (int GMT) from a dos date (which is actually in
  localtime)
**/
_PUBLIC_ time_t pull_dos_date(const uint8_t *date_ptr, int zone_offset);

/**
like make_unix_date() but the words are reversed
**/
_PUBLIC_ time_t pull_dos_date2(const uint8_t *date_ptr, int zone_offset);

/**
  create a unix GMT date from a dos date in 32 bit "unix like" format
  these generally arrive as localtimes, with corresponding DST
**/
_PUBLIC_ time_t pull_dos_date3(const uint8_t *date_ptr, int zone_offset);

/**
 Return a date and time as a string (optionally with microseconds)

 format is %Y/%m/%d %H:%M:%S if strftime is available
**/

char *timeval_string(TALLOC_CTX *ctx, const struct timeval *tp, bool hires);

/**
 Return the current date and time as a string (optionally with microseconds)

 format is %Y/%m/%d %H:%M:%S if strftime is available
**/
char *current_timestring(TALLOC_CTX *ctx, bool hires);

/**
return a HTTP/1.0 time string
**/
_PUBLIC_ char *http_timestring(TALLOC_CTX *mem_ctx, time_t t);

/**
 Return the date and time as a string

 format is %a %b %e %X %Y %Z
**/
_PUBLIC_ char *timestring(TALLOC_CTX *mem_ctx, time_t t);

/**
  return a talloced string representing a NTTIME for human consumption
*/
_PUBLIC_ const char *nt_time_string(TALLOC_CTX *mem_ctx, NTTIME nt);

/**
  put a NTTIME into a packet
*/
_PUBLIC_ void push_nttime(uint8_t *base, uint16_t offset, NTTIME t);

/**
  pull a NTTIME from a packet
*/
_PUBLIC_ NTTIME pull_nttime(uint8_t *base, uint16_t offset);

/**
  parse a nttime as a large integer in a string and return a NTTIME
*/
_PUBLIC_ NTTIME nttime_from_string(const char *s);

/**
  return (tv1 - tv2) in microseconds
*/
_PUBLIC_ int64_t usec_time_diff(const struct timeval *tv1, const struct timeval *tv2);

/**
  return (tp1 - tp2) in nanoseconds
*/
_PUBLIC_ int64_t nsec_time_diff(const struct timespec *tp1, const struct timespec *tp2);

/**
  return a zero timeval
*/
_PUBLIC_ struct timeval timeval_zero(void);

/**
  return true if a timeval is zero
*/
_PUBLIC_ bool timeval_is_zero(const struct timeval *tv);

/**
  return a timeval for the current time
*/
_PUBLIC_ struct timeval timeval_current(void);

/**
  return a timeval struct with the given elements
*/
_PUBLIC_ struct timeval timeval_set(uint32_t secs, uint32_t usecs);

/**
  return a timeval ofs microseconds after tv
*/
_PUBLIC_ struct timeval timeval_add(const struct timeval *tv,
			   uint32_t secs, uint32_t usecs);

/**
  return the sum of two timeval structures
*/
struct timeval timeval_sum(const struct timeval *tv1,
			   const struct timeval *tv2);

/**
  return a timeval secs/usecs into the future
*/
_PUBLIC_ struct timeval timeval_current_ofs(uint32_t secs, uint32_t usecs);

/**
  compare two timeval structures. 
  Return -1 if tv1 < tv2
  Return 0 if tv1 == tv2
  Return 1 if tv1 > tv2
*/
_PUBLIC_ int timeval_compare(const struct timeval *tv1, const struct timeval *tv2);

/**
  return true if a timer is in the past
*/
_PUBLIC_ bool timeval_expired(const struct timeval *tv);

/**
  return the number of seconds elapsed between two times
*/
_PUBLIC_ double timeval_elapsed2(const struct timeval *tv1, const struct timeval *tv2);

/**
  return the number of seconds elapsed since a given time
*/
_PUBLIC_ double timeval_elapsed(const struct timeval *tv);

/**
  return the lesser of two timevals
*/
_PUBLIC_ struct timeval timeval_min(const struct timeval *tv1,
			   const struct timeval *tv2);

/**
  return the greater of two timevals
*/
_PUBLIC_ struct timeval timeval_max(const struct timeval *tv1,
			   const struct timeval *tv2);

/**
  return the difference between two timevals as a timeval
  if tv1 comes after tv2, then return a zero timeval
  (this is *tv2 - *tv1)
*/
_PUBLIC_ struct timeval timeval_until(const struct timeval *tv1,
			     const struct timeval *tv2);

/**
  convert a timeval to a NTTIME
*/
_PUBLIC_ NTTIME timeval_to_nttime(const struct timeval *tv);

/**
  convert a NTTIME to a timeval
*/
_PUBLIC_ void nttime_to_timeval(struct timeval *tv, NTTIME t);

/**
  return the UTC offset in seconds west of UTC, or 0 if it cannot be determined
 */
_PUBLIC_ int get_time_zone(time_t t);

/**
  check if 2 NTTIMEs are equal.
*/
bool nt_time_equal(NTTIME *t1, NTTIME *t2);

void interpret_dos_date(uint32_t date,int *year,int *month,int *day,int *hour,int *minute,int *second);

struct timespec nt_time_to_unix_timespec(NTTIME *nt);

time_t convert_timespec_to_time_t(struct timespec ts);

struct timespec convert_time_t_to_timespec(time_t t);

bool null_timespec(struct timespec ts);

/** Extra minutes to add to the normal GMT to local time conversion. */
extern int extra_time_offset;

#endif /* _SAMBA_TIME_H_ */
