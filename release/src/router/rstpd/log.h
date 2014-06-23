/*****************************************************************************
  Copyright (c) 2006 EMC Corporation.

  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Authors: Srinivas Aji <Aji_Srinivas@emc.com>

******************************************************************************/

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>

#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_INFO  2
#define LOG_LEVEL_DEBUG 3
#define LOG_LEVEL_RSTPLIB 4
#define LOG_LEVEL_MAX 100

#define LOG_LEVEL_DEFAULT LOG_LEVEL_INFO

extern void Dprintf(int level, const char *fmt, ...);
extern void vDprintf(int level, const char *fmt, va_list ap);
extern int log_level;

#define PRINT(_level, _fmt, _args...) Dprintf(_level, _fmt, ##_args)

#define TSTM(x,y, _fmt, _args...)						\
	do if (!(x)) { 								\
			PRINT(LOG_LEVEL_ERROR,					\
				"Error in %s at %s:%d verifying %s. " _fmt,	\
				__PRETTY_FUNCTION__, __FILE__, __LINE__, 	\
				#x, ##_args);					\
			return y;						\
       } while (0)

#define TST(x,y) TSTM(x,y,"")

#define LOG(_fmt, _args...) \
	PRINT(LOG_LEVEL_DEBUG, "%s: " _fmt, __PRETTY_FUNCTION__, ##_args)

#define INFO(_fmt, _args...) \
	PRINT(LOG_LEVEL_INFO, "%s: " _fmt, __PRETTY_FUNCTION__, ##_args)

#define ERROR(_fmt, _args...) \
	PRINT(LOG_LEVEL_ERROR, "error, %s: " _fmt, __PRETTY_FUNCTION__, ##_args)

static inline void dump_hex(void *b, int l)
{
	unsigned char *buf = b;
	char logbuf[80];
	int i, j;
	for (i = 0; i < l; i += 16) {
		for (j = 0; j < 16 && i + j < l; j++)
			sprintf(logbuf + j * 3, " %02x", buf[i + j]);
		PRINT(LOG_LEVEL_INFO, "%s", logbuf);
	}
	PRINT(LOG_LEVEL_INFO, "\n");
}

#endif
