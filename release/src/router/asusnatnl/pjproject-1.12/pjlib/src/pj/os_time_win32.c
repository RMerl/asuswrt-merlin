/* $Id: os_time_win32.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pj/os.h>
#include <pj/string.h>
#include <pj/log.h>
#include <windows.h>

///////////////////////////////////////////////////////////////////////////////

#define SECS_TO_FT_MULT 10000000

static LARGE_INTEGER base_time;

#if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE
#   define WINCE_TIME
#endif

#ifdef WINCE_TIME
/* Note:
 *  In Windows CE/Windows Mobile platforms, the availability of milliseconds
 *  time resolution in SYSTEMTIME.wMilliseconds depends on the OEM, and most
 *  likely it won't be available. When it's not available, the 
 *  SYSTEMTIME.wMilliseconds will contain a constant arbitrary value.
 *
 *  Because of that, we need to emulate the milliseconds time resolution
 *  using QueryPerformanceCounter() (via pj_get_timestamp() API). However 
 *  there is limitation on using this, i.e. the time returned by 
 *  pj_gettimeofday() may be off by up to plus/minus 999 msec (the second
 *  part will be correct, however the msec part may be off), because we're 
 *  not synchronizing the msec field with the change of value of the "second"
 *  field of the system time.
 *
 *  Also there is other caveat which need to be handled (and they are 
 *  handled by this implementation):
 *   - user may change system time, so pj_gettimeofday() needs to periodically
 *     checks if system time has changed. The period on which system time is
 *     checked is controlled by PJ_WINCE_TIME_CHECK_INTERVAL macro.
 */
static LARGE_INTEGER g_start_time;  /* Time gettimeofday() is first called  */
static pj_timestamp  g_start_tick;  /* TS gettimeofday() is first called  */
static pj_timestamp  g_last_update; /* Last time check_system_time() is 
				       called, to periodically synchronize
				       with up-to-date system time (in case
				       user changes system time).	    */
static pj_uint64_t   g_update_period; /* Period (in TS) check_system_time()
				         should be called.		    */

/* Period on which check_system_time() is called, in seconds		    */
#ifndef PJ_WINCE_TIME_CHECK_INTERVAL
#   define PJ_WINCE_TIME_CHECK_INTERVAL (10)
#endif

#endif

#ifdef WINCE_TIME
static pj_status_t init_start_time(void)
{
    SYSTEMTIME st;
    FILETIME ft;
    pj_timestamp freq;
    pj_status_t status;

    GetLocalTime(&st);
    SystemTimeToFileTime(&st, &ft);

    g_start_time.LowPart = ft.dwLowDateTime;
    g_start_time.HighPart = ft.dwHighDateTime;
    g_start_time.QuadPart /= SECS_TO_FT_MULT;
    g_start_time.QuadPart -= base_time.QuadPart;

    status = pj_get_timestamp(&g_start_tick);
    if (status != PJ_SUCCESS)
	return status;

    g_last_update.u64 = g_start_tick.u64;

    status = pj_get_timestamp_freq(&freq);
    if (status != PJ_SUCCESS)
	return status;

    g_update_period = PJ_WINCE_TIME_CHECK_INTERVAL * freq.u64;

    PJ_LOG(4,("os_time_win32.c", "WinCE time (re)started"));

    return PJ_SUCCESS;
}

static pj_status_t check_system_time(pj_uint64_t ts_elapsed)
{
    enum { MIS = 5 };
    SYSTEMTIME st;
    FILETIME ft;
    LARGE_INTEGER cur, calc;
    DWORD diff;
    pj_timestamp freq;
    pj_status_t status;

    /* Get system's current time */
    GetLocalTime(&st);
    SystemTimeToFileTime(&st, &ft);
    
    cur.LowPart = ft.dwLowDateTime;
    cur.HighPart = ft.dwHighDateTime;
    cur.QuadPart /= SECS_TO_FT_MULT;
    cur.QuadPart -= base_time.QuadPart;

    /* Get our calculated system time */
    status = pj_get_timestamp_freq(&freq);
    if (status != PJ_SUCCESS)
	return status;

    calc.QuadPart = g_start_time.QuadPart + ts_elapsed / freq.u64;

    /* See the difference between calculated and actual system time */
    if (calc.QuadPart >= cur.QuadPart) {
	diff = (DWORD)(calc.QuadPart - cur.QuadPart);
    } else {
	diff = (DWORD)(cur.QuadPart - calc.QuadPart);
    }

    if (diff > MIS) {
	/* System time has changed */
	PJ_LOG(3,("os_time_win32.c", "WinCE system time changed detected "
				      "(diff=%u)", diff));
	status = init_start_time();
    } else {
	status = PJ_SUCCESS;
    }

    return status;
}

#endif

// Find 1st Jan 1970 as a FILETIME 
static pj_status_t get_base_time(void)
{
    SYSTEMTIME st;
    FILETIME ft;
    pj_status_t status = PJ_SUCCESS;

    memset(&st,0,sizeof(st));
    st.wYear=1970;
    st.wMonth=1;
    st.wDay=1;
    SystemTimeToFileTime(&st, &ft);
    
    base_time.LowPart = ft.dwLowDateTime;
    base_time.HighPart = ft.dwHighDateTime;
    base_time.QuadPart /= SECS_TO_FT_MULT;

#ifdef WINCE_TIME
    pj_enter_critical_section();
    status = init_start_time();
    pj_leave_critical_section();
#endif

    return status;
}

#if defined(_MSC_VER) || defined(__BORLANDC__)
#define EPOCHFILETIME (116444736000000000i64)
#else
#define EPOCHFILETIME (116444736000000000LL)
#endif

PJ_DEF(pj_status_t) pj_gettimeofday2(pj_time_val2 *tv)
{
    FILETIME        ft;
    LARGE_INTEGER   li;
    __int64         t;
    static int      tzflag;

    if (tv) {
        GetSystemTimeAsFileTime(&ft);
        li.LowPart  = ft.dwLowDateTime;
        li.HighPart = ft.dwHighDateTime;
        t  = li.QuadPart;       /* In 100-nanosecond intervals */
        t -= EPOCHFILETIME;     /* Offset to the Epoch time */
        t /= 10;                /* In microseconds */
        tv->sec  = (long)(t / 1000000);
        tv->usec = (long)(t % 1000000);
    }

    return 0;
}

PJ_DEF(pj_status_t) pj_gettimeofday(pj_time_val *tv)
{
#ifdef WINCE_TIME
    pj_timestamp tick;
    pj_uint64_t msec_elapsed;
#else
    SYSTEMTIME st;
    FILETIME ft;
    LARGE_INTEGER li;
#endif
    pj_status_t status;

    if (base_time.QuadPart == 0) {
	status = get_base_time();
	if (status != PJ_SUCCESS)
	    return status;
    }

#ifdef WINCE_TIME
    do {
	status = pj_get_timestamp(&tick);
	if (status != PJ_SUCCESS)
	    return status;

	if (tick.u64 - g_last_update.u64 >= g_update_period) {
	    pj_enter_critical_section();
	    if (tick.u64 - g_last_update.u64 >= g_update_period) {
		g_last_update.u64 = tick.u64;
		check_system_time(tick.u64 - g_start_tick.u64);
	    }
	    pj_leave_critical_section();
	} else {
	    break;
	}
    } while (1);

    msec_elapsed = pj_elapsed_msec64(&g_start_tick, &tick);

    tv->sec = (long)(g_start_time.QuadPart + msec_elapsed/1000);
    tv->msec = (long)(msec_elapsed % 1000);
#else
    /* Standard Win32 GetLocalTime */
    GetLocalTime(&st);
    SystemTimeToFileTime(&st, &ft);

    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    li.QuadPart /= SECS_TO_FT_MULT;
    li.QuadPart -= base_time.QuadPart;

    tv->sec = li.LowPart;
    tv->msec = st.wMilliseconds;
#endif	/* WINCE_TIME */

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_time_decode(const pj_time_val *tv, pj_parsed_time *pt)
{
    LARGE_INTEGER li;
    FILETIME ft;
    SYSTEMTIME st;

    li.QuadPart = tv->sec;
    li.QuadPart += base_time.QuadPart;
    li.QuadPart *= SECS_TO_FT_MULT;

    ft.dwLowDateTime = li.LowPart;
    ft.dwHighDateTime = li.HighPart;
    FileTimeToSystemTime(&ft, &st);

    pt->year = st.wYear;
    pt->mon = st.wMonth-1;
    pt->day = st.wDay;
    pt->wday = st.wDayOfWeek;

    pt->hour = st.wHour;
    pt->min = st.wMinute;
    pt->sec = st.wSecond;
    pt->msec = tv->msec;

    return PJ_SUCCESS;
}

/**
 * Encode parsed time to time value.
 */
PJ_DEF(pj_status_t) pj_time_encode(const pj_parsed_time *pt, pj_time_val *tv)
{
    SYSTEMTIME st;
    FILETIME ft;
    LARGE_INTEGER li;

    pj_bzero(&st, sizeof(st));
    st.wYear = (pj_uint16_t) pt->year;
    st.wMonth = (pj_uint16_t) (pt->mon + 1);
    st.wDay = (pj_uint16_t) pt->day;
    st.wHour = (pj_uint16_t) pt->hour;
    st.wMinute = (pj_uint16_t) pt->min;
    st.wSecond = (pj_uint16_t) pt->sec;
    st.wMilliseconds = (pj_uint16_t) pt->msec;
    
    SystemTimeToFileTime(&st, &ft);

    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    li.QuadPart /= SECS_TO_FT_MULT;
    li.QuadPart -= base_time.QuadPart;

    tv->sec = li.LowPart;
    tv->msec = st.wMilliseconds;

    return PJ_SUCCESS;
}

/**
 * Convert local time to GMT.
 */
PJ_DEF(pj_status_t) pj_time_local_to_gmt(pj_time_val *tv);

/**
 * Convert GMT to local time.
 */
PJ_DEF(pj_status_t) pj_time_gmt_to_local(pj_time_val *tv);


