/* $Id: log_writer_stdout.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/log.h>
#include <pj/os.h>
#include <pj/compat/stdfileio.h>
#include <pjlib.h>
#if PJ_ANDROID==1
#include <j_log.h>
#define ANDROID_DUMP_STRING "NAT_DUMP" 
#endif


static void term_set_color(int level)
{
#if defined(PJ_TERM_HAS_COLOR) && PJ_TERM_HAS_COLOR != 0
    pj_term_set_color(pj_log_get_color(level));
#else
    PJ_UNUSED_ARG(level);
#endif
}

static void term_restore_color(void)
{
#if defined(PJ_TERM_HAS_COLOR) && PJ_TERM_HAS_COLOR != 0
    /* Set terminal to its default color */
    pj_term_set_color(pj_log_get_color(77));
#endif
}

void substr(char *dest, const char *src, unsigned int start, unsigned int end)
{
    int i = start;
    char *p = src;

    pj_assert(start <= strlen(src));

    if(end > strlen(src))
       end = strlen(src);

    while(i < end)
    {
       dest[i-start] = src[i];
       i++;
    }

    dest[i-start] = '\0';
}

PJ_DEF(void) pj_log_write(int inst_id, int level, const char *buffer, int len, int flush)
{
	int i;
    int shift;
    int chunk_cnt;
    int limit_len = 1000;
    char temp[1001];

    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(len);
	PJ_UNUSED_ARG(flush);

#if PJ_ANDROID==1
	if(len > limit_len) {
        chunk_cnt = len / limit_len;
        for(i = 0; i <= chunk_cnt; i++) {
            memset(temp, "0", 1001);
            shift = limit_len * (i + 1);
            if( shift >= len) {
                substr(temp, buffer, limit_len * i, shift);
                LOG_E(ANDROID_DUMP_STRING,"%s", temp);
            }else {
                substr(temp, buffer, limit_len * i, shift);
                LOG_E(ANDROID_DUMP_STRING,"%s", temp);
            }
        }
    }
    else
		LOG_E(ANDROID_DUMP_STRING,"%s", buffer);
#else

    /* Copy to terminal/file. */
    if (pj_log_get_decor() & PJ_LOG_HAS_COLOR) {
	term_set_color(level);
	printf("%d %s", inst_id, buffer);
	term_restore_color();
    } else {
	printf("%d %s", inst_id, buffer);	
    }
#endif
}


/* 
PJSIP log level 
"FATAL:", 
"ERROR:", 
" WARN:", 
" INFO:", 
"DEBUG:", 
"TRACE:", 
"DETRC:";
*/
/*
Android log level 
typedef enum android_LogPriority {
    ANDROID_LOG_UNKNOWN = 0,
    ANDROID_LOG_DEFAULT,    // only for SetMinPriority() 
    ANDROID_LOG_VERBOSE,
    ANDROID_LOG_DEBUG,
    ANDROID_LOG_INFO,
    ANDROID_LOG_WARN,
    ANDROID_LOG_ERROR,
    ANDROID_LOG_FATAL,
    ANDROID_LOG_SILENT,     // only for SetMinPriority(); must be last 
} android_LogPriority;
*/
PJ_DEF(void) pj_android_log_write(int level, const char *buffer, int len)
{
	PJ_UNUSED_ARG(level);
	PJ_UNUSED_ARG(buffer);
	PJ_UNUSED_ARG(len);
}

