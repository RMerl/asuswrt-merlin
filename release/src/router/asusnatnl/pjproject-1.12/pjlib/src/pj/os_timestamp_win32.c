/* $Id: os_timestamp_win32.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/log.h>
#include <windows.h>

#define THIS_FILE   "os_timestamp_win32.c"


#if 1
#   define TRACE_(x)	    PJ_LOG(3,x)
#else
#   define TRACE_(x)	    ;
#endif


/////////////////////////////////////////////////////////////////////////////

#if defined(PJ_TIMESTAMP_USE_RDTSC) && PJ_TIMESTAMP_USE_RDTSC!=0 && \
    defined(PJ_M_I386) && PJ_M_I386 != 0 && \
    defined(PJ_HAS_PENTIUM) && PJ_HAS_PENTIUM!=0 && \
    defined(_MSC_VER)

/*
 * Use rdtsc to get the OS timestamp.
 */
static LONG CpuMhz;
static pj_int64_t CpuHz;
 
static pj_status_t GetCpuHz(void)
{
    HKEY key;
    LONG rc;
    DWORD size;

#if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0
    rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		      L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
		      0, 0, &key);
#else
    rc = RegOpenKey( HKEY_LOCAL_MACHINE,
		     "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
		     &key);
#endif

    if (rc != ERROR_SUCCESS)
	return PJ_RETURN_OS_ERROR(rc);

    size = sizeof(CpuMhz);
    rc = RegQueryValueEx(key, "~MHz", NULL, NULL, (BYTE*)&CpuMhz, &size);
    RegCloseKey(key);

    if (rc != ERROR_SUCCESS) {
	return PJ_RETURN_OS_ERROR(rc);
    }

    CpuHz = CpuMhz;
    CpuHz = CpuHz * 1000000;

    return PJ_SUCCESS;
}

/* __int64 is nicely returned in EDX:EAX */
__declspec(naked) __int64 rdtsc() 
{
    __asm 
    {
	RDTSC
	RET
    }
}

PJ_DEF(pj_status_t) pj_get_timestamp(pj_timestamp *ts)
{
    ts->u64 = rdtsc();
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq)
{
    pj_status_t status;

    if (CpuHz == 0) {
	status = GetCpuHz();
	if (status != PJ_SUCCESS)
	    return status;
    }

    freq->u64 = CpuHz;
    return PJ_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////

#elif defined(PJ_TIMESTAMP_WIN32_USE_SAFE_QPC) && \
         PJ_TIMESTAMP_WIN32_USE_SAFE_QPC!=0

/* Use safe QueryPerformanceCounter.
 * This implementation has some protection against bug in KB Q274323:
 *   Performance counter value may unexpectedly leap forward
 *   http://support.microsoft.com/default.aspx?scid=KB;EN-US;Q274323
 *
 * THIS SHOULD NOT BE USED YET AS IT DOESN'T HANDLE SYSTEM TIME
 * CHANGE.
 */

static pj_timestamp g_ts_freq;
static pj_timestamp g_ts_base;
static pj_int64_t   g_time_base;

PJ_DEF(pj_status_t) pj_get_timestamp(pj_timestamp *ts)
{
    enum { MAX_RETRY = 10 };
    unsigned i;


    /* pj_get_timestamp_freq() must have been called before.
     * This is done when application called pj_init().
     */
    pj_assert(g_ts_freq.u64 != 0);

    /* Retry QueryPerformanceCounter() until we're sure that the
     * value returned makes sense.
     */
    i = 0;
    do {
	LARGE_INTEGER val;
	pj_int64_t counter64, time64, diff;
	pj_time_val time_now;

	/* Retrieve the counter */
	if (!QueryPerformanceCounter(&val))
	    return PJ_RETURN_OS_ERROR(GetLastError());

	/* Regardless of the goodness of the value, we should put
	 * the counter here, because normally application wouldn't
	 * check the error result of this function.
	 */
	ts->u64 = val.QuadPart;

	/* Retrieve time */
	pj_gettimeofday(&time_now);

	/* Get the counter elapsed time in miliseconds */
	counter64 = (val.QuadPart - g_ts_base.u64) * 1000 / g_ts_freq.u64;
	
	/* Get the time elapsed in miliseconds. 
	 * We don't want to use PJ_TIME_VAL_MSEC() since it's using
	 * 32bit calculation, which limits the maximum elapsed time
	 * to around 49 days only.
	 */
	time64 = time_now.sec;
	time64 = time64 * 1000 + time_now.msec;
	//time64 = GetTickCount();

	/* It's good if the difference between two clocks are within
	 * some compile time constant (default: 20ms, which to allow
	 * context switch happen between QueryPerformanceCounter and
	 * pj_gettimeofday()).
	 */
	diff = (time64 - g_time_base) - counter64;
	if (diff >= -20 && diff <= 20) {
	    /* It's good */
	    return PJ_SUCCESS;
	}

	++i;

    } while (i < MAX_RETRY);

    TRACE_((THIS_FILE, "QueryPerformanceCounter returned bad value"));
    return PJ_ETIMEDOUT;
}

static pj_status_t init_performance_counter(void)
{
    LARGE_INTEGER val;
    pj_time_val time_base;
    pj_status_t status;

    /* Get the frequency */
    if (!QueryPerformanceFrequency(&val))
	return PJ_RETURN_OS_ERROR(GetLastError());

    g_ts_freq.u64 = val.QuadPart;

    /* Get the base timestamp */
    if (!QueryPerformanceCounter(&val))
	return PJ_RETURN_OS_ERROR(GetLastError());

    g_ts_base.u64 = val.QuadPart;


    /* Get the base time */
    status = pj_gettimeofday(&time_base);
    if (status != PJ_SUCCESS)
	return status;

    /* Convert time base to 64bit value in msec */
    g_time_base = time_base.sec;
    g_time_base  = g_time_base * 1000 + time_base.msec;
    //g_time_base = GetTickCount();

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq)
{
    if (g_ts_freq.u64 == 0) {
	enum { MAX_REPEAT = 10 };
	unsigned i;
	pj_status_t status;

	/* Make unellegant compiler happy */
	status = 0;

	/* Repeat initializing performance counter until we're sure
	 * the base timing is correct. It is possible that the system
	 * returns bad counter during this initialization!
	 */
	for (i=0; i<MAX_REPEAT; ++i) {

	    pj_timestamp dummy;

	    /* Init base time */
	    status = init_performance_counter();
	    if (status != PJ_SUCCESS)
		return status;

	    /* Try the base time */
	    status = pj_get_timestamp(&dummy);
	    if (status == PJ_SUCCESS)
		break;
	}

	if (status != PJ_SUCCESS)
	    return status;
    }

    freq->u64 = g_ts_freq.u64;
    return PJ_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////

#else

/*
 * Use QueryPerformanceCounter and QueryPerformanceFrequency.
 * This should be the default implementation to be used on Windows.
 */
PJ_DEF(pj_status_t) pj_get_timestamp(pj_timestamp *ts)
{
    LARGE_INTEGER val;

    if (!QueryPerformanceCounter(&val))
	return PJ_RETURN_OS_ERROR(GetLastError());

    ts->u64 = val.QuadPart;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq)
{
    LARGE_INTEGER val;

    if (!QueryPerformanceFrequency(&val))
	return PJ_RETURN_OS_ERROR(GetLastError());

    freq->u64 = val.QuadPart;
    return PJ_SUCCESS;
}


#endif	/* PJ_TIMESTAMP_USE_RDTSC */

