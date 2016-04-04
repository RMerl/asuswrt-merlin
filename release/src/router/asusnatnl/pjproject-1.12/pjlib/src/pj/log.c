/* $Id: log.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/types.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/os.h>
#include <pj/compat/stdarg.h>

#if PJ_LOG_MAX_LEVEL >= 1

#if 0
PJ_DEF_DATA(int) pj_log_max_level = PJ_LOG_MAX_LEVEL;
#else
static int pj_log_max_level[PJSUA_MAX_INSTANCES];
static int log_max_level_initialized;
#endif

#if PJ_HAS_THREADS
static long thread_suspended_tls_id[PJSUA_MAX_INSTANCES];
static int thread_tls_id_initialized;
#endif

static pj_log_func *log_writer = NULL;//&pj_log_write;
static unsigned log_decor = PJ_LOG_HAS_TIME | PJ_LOG_HAS_MICRO_SEC |
			    PJ_LOG_HAS_SENDER | PJ_LOG_HAS_NEWLINE |
			    PJ_LOG_HAS_SPACE
#if defined(PJ_WIN32) && PJ_WIN32!=0
			    | PJ_LOG_HAS_COLOR
#endif
			    ;

static pj_color_t PJ_LOG_COLOR_0 = PJ_TERM_COLOR_BRIGHT | PJ_TERM_COLOR_R;
static pj_color_t PJ_LOG_COLOR_1 = PJ_TERM_COLOR_BRIGHT | PJ_TERM_COLOR_R;
static pj_color_t PJ_LOG_COLOR_2 = PJ_TERM_COLOR_BRIGHT | 
				   PJ_TERM_COLOR_R | 
				   PJ_TERM_COLOR_G;
static pj_color_t PJ_LOG_COLOR_3 = PJ_TERM_COLOR_BRIGHT | 
				   PJ_TERM_COLOR_R | 
				   PJ_TERM_COLOR_G | 
				   PJ_TERM_COLOR_B;
static pj_color_t PJ_LOG_COLOR_4 = PJ_TERM_COLOR_R | 
				   PJ_TERM_COLOR_G | 
				   PJ_TERM_COLOR_B;
static pj_color_t PJ_LOG_COLOR_5 = PJ_TERM_COLOR_R | 
				   PJ_TERM_COLOR_G | 
				   PJ_TERM_COLOR_B;
static pj_color_t PJ_LOG_COLOR_6 = PJ_TERM_COLOR_R | 
				   PJ_TERM_COLOR_G | 
				   PJ_TERM_COLOR_B;
/* Default terminal color */
static pj_color_t PJ_LOG_COLOR_77 = PJ_TERM_COLOR_R | 
				    PJ_TERM_COLOR_G | 
				    PJ_TERM_COLOR_B;

#if PJ_LOG_USE_STACK_BUFFER==0
static char log_buffer[PJ_LOG_MAX_SIZE];
#endif

static void log_max_level_initialize()
{
	int i;
	if(log_max_level_initialized)
		return;

	for (i=0; i < PJ_ARRAY_SIZE(pj_log_max_level); i++)
	{
		pj_log_max_level[i] = PJ_LOG_MAX_LEVEL;
	}

	log_max_level_initialized = 1;
}

#if PJ_HAS_THREADS

static void thread_tls_id_initialize()
{
	int i;
	if(thread_tls_id_initialized)
		return;

	for (i=0; i < PJ_ARRAY_SIZE(thread_suspended_tls_id); i++)
	{
		thread_suspended_tls_id[i] = -1;
	}

	thread_tls_id_initialized = 1;
}

static void logging_shutdown(int inst_id)
{
    if (thread_suspended_tls_id[inst_id] != -1) {
		pj_thread_local_free(thread_suspended_tls_id[inst_id]);
		thread_suspended_tls_id[inst_id] = -1;
    }
}
#endif

pj_status_t pj_log_init(int inst_id)
{
	log_max_level_initialize();

#if PJ_HAS_THREADS
	thread_tls_id_initialize();

    if (thread_suspended_tls_id[inst_id] == -1) {
	pj_thread_local_alloc(&thread_suspended_tls_id[inst_id]);
	pj_atexit(inst_id, &logging_shutdown);
    }
#endif
    return PJ_SUCCESS;
}

PJ_DEF(void) pj_log_set_decor(unsigned decor)
{
    log_decor = decor;
}

PJ_DEF(unsigned) pj_log_get_decor(void)
{
    return log_decor;
}

PJ_DEF(void) pj_log_set_color(int level, pj_color_t color)
{
    switch (level) 
    {
	case 0: PJ_LOG_COLOR_0 = color; 
	    break;
	case 1: PJ_LOG_COLOR_1 = color; 
	    break;
	case 2: PJ_LOG_COLOR_2 = color; 
	    break;
	case 3: PJ_LOG_COLOR_3 = color; 
	    break;
	case 4: PJ_LOG_COLOR_4 = color; 
	    break;
	case 5: PJ_LOG_COLOR_5 = color; 
	    break;
	case 6: PJ_LOG_COLOR_6 = color; 
	    break;
	/* Default terminal color */
	case 77: PJ_LOG_COLOR_77 = color; 
	    break;
	default:
	    /* Do nothing */
	    break;
    }
}

PJ_DEF(pj_color_t) pj_log_get_color(int level)
{
    switch (level) {
	case 0:
	    return PJ_LOG_COLOR_0;
	case 1:
	    return PJ_LOG_COLOR_1;
	case 2:
	    return PJ_LOG_COLOR_2;
	case 3:
	    return PJ_LOG_COLOR_3;
	case 4:
	    return PJ_LOG_COLOR_4;
	case 5:
	    return PJ_LOG_COLOR_5;
	case 6:
	    return PJ_LOG_COLOR_6;
	default:
	    /* Return default terminal color */
	    return PJ_LOG_COLOR_77;
    }
}

PJ_DEF(void) pj_log_set_level(int inst_id, int level)
{
    pj_log_max_level[inst_id] = level;
}

#if 1
PJ_DEF(int) pj_log_get_level(int inst_id)
{
    return pj_log_max_level[inst_id];
}
#endif

PJ_DEF(void) pj_log_set_log_func( pj_log_func *func )
{
    log_writer = func;
}

PJ_DEF(pj_log_func*) pj_log_get_log_func(void)
{
    return log_writer;
}

/* Temporarily suspend logging facility for this thread.
 * If thread local storage/variable is not used or not initialized, then
 * we can only suspend the logging globally across all threads. This may
 * happen e.g. when log function is called before PJLIB is fully initialized
 * or after PJLIB is shutdown.
 */
static void suspend_logging(int inst_id, int *saved_level)
{
	/* Save the level regardless, just in case PJLIB is shutdown
	 * between suspend and resume.
	 */
	*saved_level = pj_log_max_level[inst_id];

#if PJ_HAS_THREADS
    if (thread_suspended_tls_id[inst_id] != -1) 
    {
	pj_thread_local_set(thread_suspended_tls_id[inst_id], (void*)PJ_TRUE);
    } 
    else
#endif
    {
	pj_log_max_level[inst_id] = 0;
    }
}

/* Resume logging facility for this thread */
static void resume_logging(int inst_id, int *saved_level)
{
#if PJ_HAS_THREADS
    if (thread_suspended_tls_id[inst_id] != -1) 
    {
	pj_thread_local_set(thread_suspended_tls_id[inst_id], (void*)PJ_FALSE);
    }
    else
#endif
    {
	/* Only revert the level if application doesn't change the
	 * logging level between suspend and resume.
	 */
	if (pj_log_max_level[inst_id]==0 && *saved_level)
	    pj_log_max_level[inst_id] = *saved_level;
    }
}

/* Is logging facility suspended for this thread? */
static pj_bool_t is_logging_suspended(int inst_id)
{
#if PJ_HAS_THREADS
    if (thread_suspended_tls_id[inst_id] != -1) 
    {
	return pj_thread_local_get(thread_suspended_tls_id[inst_id]) != NULL;
    }
    else
#endif
    {
	return pj_log_max_level[inst_id] == 0;
    }
}

PJ_DEF(void) pj_log( const char *sender, int level, //INST_TODO
					 const char *format, va_list marker)
{
	pj_log2(sender, level, format, marker, 0);
}

PJ_DEF(void) pj_log2( const char *sender, int level, //INST_TODO
		     const char *format, va_list marker, int flush)
{
    pj_time_val now;
    pj_parsed_time ptime;
    char *pre;
#if PJ_LOG_USE_STACK_BUFFER
    char log_buffer[PJ_LOG_MAX_SIZE];
#endif
    int saved_level, len, print_len;
	int inst_id = 0;

    PJ_CHECK_STACK();

    if (level > pj_log_max_level[inst_id]) 
	return;

    if (is_logging_suspended(inst_id)) 
	return;

    /* Temporarily disable logging for this thread. Some of PJLIB APIs that
     * this function calls below will recursively call the logging function 
     * back, hence it will cause infinite recursive calls if we allow that.
     */
    suspend_logging(inst_id, &saved_level); 

    /* Get current date/time. */
    pj_gettimeofday(&now);
    pj_time_decode(&now, &ptime);

    pre = log_buffer;
    if (log_decor & PJ_LOG_HAS_LEVEL_TEXT) {
	static const char *ltexts[] = { "FATAL:", "ERROR:", " WARN:", 
		" INFO:", "DEBUG:", "TRACE:", "DETRC:"};
	pj_ansi_strcpy(pre, ltexts[level]);
	pre += 6;
    }
    if (log_decor & PJ_LOG_HAS_DAY_NAME) {
	static const char *wdays[] = { "Sun", "Mon", "Tue", "Wed",
				       "Thu", "Fri", "Sat"};
	pj_ansi_strcpy(pre, wdays[ptime.wday]);
	pre += 3;
    }
    if (log_decor & PJ_LOG_HAS_YEAR) {
	*pre++ = ' ';
	pre += pj_utoa(ptime.year, pre);
    }
    if (log_decor & PJ_LOG_HAS_MONTH) {
	*pre++ = '-';
	pre += pj_utoa_pad(ptime.mon+1, pre, 2, '0');
    }
    if (log_decor & PJ_LOG_HAS_DAY_OF_MON) {
	*pre++ = '-';
	pre += pj_utoa_pad(ptime.day, pre, 2, '0');
    }
    if (log_decor & PJ_LOG_HAS_TIME) {
	*pre++ = ' ';
	pre += pj_utoa_pad(ptime.hour, pre, 2, '0');
	*pre++ = ':';
	pre += pj_utoa_pad(ptime.min, pre, 2, '0');
	*pre++ = ':';
	pre += pj_utoa_pad(ptime.sec, pre, 2, '0');
    }
    if (log_decor & PJ_LOG_HAS_MICRO_SEC) {
	*pre++ = '.';
	pre += pj_utoa_pad(ptime.msec, pre, 3, '0');
    }

	// Process Id
	*pre++ = ' ';
	pre += pj_utoa_pad(pj_getpid(), pre, 10, '0');

	// Thread Id
	*pre++ = '/';
	pre += pj_utoa_pad(pj_gettid(), pre, 10, '0');

    if (log_decor & PJ_LOG_HAS_SENDER) {
	enum { SENDER_WIDTH = 17 };
	int sender_len = strlen(sender);
	*pre++ = ' ';
	if (sender_len <= SENDER_WIDTH) {
	    while (sender_len < SENDER_WIDTH)
		*pre++ = ' ', ++sender_len;
	    while (*sender)
		*pre++ = *sender++;
	} else {
	    int i;
	    for (i=0; i<SENDER_WIDTH; ++i)
		*pre++ = *sender++;
	}
    }
    if (log_decor & PJ_LOG_HAS_THREAD_ID) {
	enum { THREAD_WIDTH = 12 };
	const char *thread_name = pj_thread_get_name(pj_thread_this(inst_id));
	int thread_len = strlen(thread_name);
	*pre++ = ' ';
	if (thread_len <= THREAD_WIDTH) {
	    while (thread_len < THREAD_WIDTH)
		*pre++ = ' ', ++thread_len;
	    while (*thread_name)
		*pre++ = *thread_name++;
	} else {
	    int i;
	    for (i=0; i<THREAD_WIDTH; ++i)
		*pre++ = *thread_name++;
	}
    }

    if (log_decor != 0 && log_decor != PJ_LOG_HAS_NEWLINE)
	*pre++ = ' ';

    if (log_decor & PJ_LOG_HAS_SPACE) {
	*pre++ = ' ';
    }

    len = pre - log_buffer;

    /* Print the whole message to the string log_buffer. */
    print_len = pj_ansi_vsnprintf(pre, sizeof(log_buffer)-len, format, 
				  marker);
    if (print_len < 0) {
	level = 1;
	print_len = pj_ansi_snprintf(pre, sizeof(log_buffer)-len, 
				     "<logging error: msg too long>");
    }
    len = len + print_len;
    if (len > 0 && len < (int)sizeof(log_buffer)-2) {
	if (log_decor & PJ_LOG_HAS_CR) {
	    log_buffer[len++] = '\r';
	}
	if (log_decor & PJ_LOG_HAS_NEWLINE) {
	    log_buffer[len++] = '\n';
	}
	log_buffer[len] = '\0';
    } else {
	len = sizeof(log_buffer)-1;
	if (log_decor & PJ_LOG_HAS_CR) {
	    log_buffer[sizeof(log_buffer)-3] = '\r';
	}
	if (log_decor & PJ_LOG_HAS_NEWLINE) {
	    log_buffer[sizeof(log_buffer)-2] = '\n';
	}
	log_buffer[sizeof(log_buffer)-1] = '\0';
    }

    /* It should be safe to resume logging at this point. Application can
     * recursively call the logging function inside the callback.
     */
    resume_logging(inst_id, &saved_level);

    if (log_writer)
	(*log_writer)(inst_id, level, log_buffer, len, flush);
}

/*
PJ_DEF(void) pj_log_0(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 0, format, arg);
    va_end(arg);
}
*/

PJ_DEF(void) pj_log_1(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 1, format, arg);
    va_end(arg);
}
#endif	/* PJ_LOG_MAX_LEVEL >= 1 */

#if PJ_LOG_MAX_LEVEL >= 2
PJ_DEF(void) pj_log_2(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 2, format, arg);
    va_end(arg);
}
#endif

#if PJ_LOG_MAX_LEVEL >= 3
PJ_DEF(void) pj_log_3(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 3, format, arg);
    va_end(arg);
}
#endif

#if PJ_LOG_MAX_LEVEL >= 4
PJ_DEF(void) pj_log_4(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 4, format, arg);
    va_end(arg);
}
#endif

#if PJ_LOG_MAX_LEVEL >= 5
PJ_DEF(void) pj_log_5(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 5, format, arg);
    va_end(arg);
}
#endif

#if PJ_LOG_MAX_LEVEL >= 6
PJ_DEF(void) pj_log_6(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 6, format, arg);
    va_end(arg);
}
#endif

