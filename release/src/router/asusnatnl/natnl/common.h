/*
 * Project: udptunnel
 * File: common.h
 *
 * Copyright (C) 2009 Daniel Meekins
 * Contact: dmeekins - gmail
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AA_COMMON_H
#define AA_COMMON_H

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <pj/errno.h>

#define NO_DEBUG     0
#define DEBUG_LEVEL1 1
#define DEBUG_LEVEL2 2
#define DEBUG_LEVEL3 3

#ifdef WIN32
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
#endif

/* cl.exe has a different 'inline' keyword for some dumb reason */
#ifdef WIN32
#define _inline_ __inline
#else
#define _inline_ inline
#endif

#ifdef WIN32
// Set signal handler for crash stack trace use.
void set_signal_handler();
#endif

struct natnl_data {
	pj_sem_t *waiting_sem;
	void *user_data;
	int status;
};

#define my_memcpy(dst, src, dst_len, src_len) {memcpy(dst, src, dst_len < src_len ? dst_len : src_len);}

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#ifdef SOLARIS
/* Copied from sys/time.h on linux system since solaris system that I tried to
 * compile on didn't have timeradd macro. */
#define timeradd(a, b, result)                                                \
    do {                                                                      \
        (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;                         \
        (result)->tv_usec = (a)->tv_usec + (b)->tv_usec;                      \
        if ((result)->tv_usec >= 1000000)                                     \
        {                                                                     \
            ++(result)->tv_sec;                                               \
            (result)->tv_usec -= 1000000;                                     \
        }                                                                     \
    } while (0)
#endif /* SOLARIS */

static _inline_ int isnum(char *s)
{
    for(; *s != '\0'; s++)
    {
        if(!isdigit((int)*s))
            return 0;
    }

    return 1;
}

static _inline_ char* get_id_part_device_id(char *full_device_id)
{
	char *tmp;
	if (!full_device_id)
		return NULL;

	tmp = strtok(full_device_id, "sip:");
	if (tmp)
		tmp = strtok(tmp, "@");
	return tmp == NULL ? NULL : tmp;	
}

#endif /* COMMON_H */

