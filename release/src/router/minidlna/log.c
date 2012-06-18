/* MiniDLNA media server
 * Copyright (C) 2008-2010 NETGEAR, Inc. All Rights Reserved.
 *
 * This file is part of MiniDLNA.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "log.h"

static FILE *log_fp = NULL;
static int default_log_level = E_WARN;
int log_level[L_MAX];

char *facility_name[] = {
	"general",
	"artwork",
	"database",
	"inotify",
	"scanner",
	"metadata",
	"http",
	"ssdp",
	"tivo",
	0
};

char *level_name[] = {
	"off",					// E_OFF
	"fatal",				// E_FATAL
	"error",				// E_ERROR
	"warn",					// E_WARN
	"info",					// E_INFO
	"debug",				// E_DEBUG
	0
};

int
log_init(const char *fname, const char *debug)
{
	int i;
	FILE *fp;
	short int log_level_set[L_MAX];

	if (debug)
	{
		const char *rhs, *lhs, *nlhs, *p;
		int n;
		int level, facility;
		memset(&log_level_set, 0, sizeof(log_level_set));
		rhs = nlhs = debug;
		while (rhs && (rhs = strchr(rhs, '='))) {
			rhs++;
			p = strchr(rhs, ',');
			n = p ? p - rhs : strlen(rhs);
			for (level=0; level_name[level]; level++) {
				if (!(strncasecmp(level_name[level], rhs, n)))
					break;
			}
			lhs = nlhs;
			rhs = nlhs = p;
			if (!(level_name[level])) {
				// unknown level
				continue;
			}
			do {
				if (*lhs==',') lhs++;
				p = strpbrk(lhs, ",=");
				n = p ? p - lhs : strlen(lhs);
				for (facility=0; facility_name[facility]; facility++) {
					if (!(strncasecmp(facility_name[facility], lhs, n)))
						break;
				}
				if ((facility_name[facility])) {
					log_level[facility] = level;
					log_level_set[facility] = 1;
				}
				lhs = p;
			} while (*lhs && *lhs==',');
		}
		for (i=0; i<L_MAX; i++)
		{
			if( !log_level_set[i] )
			{
				log_level[i] = default_log_level;
			}
		}
	}
	else {
		for (i=0; i<L_MAX; i++)
			log_level[i] = default_log_level;
	}

	if (!fname)					// use default i.e. stdout
		return 0;

	if (!(fp = fopen(fname, "a")))
		return 1;
	log_fp = fp;
	return 0;
}

void
log_err(int level, enum _log_facility facility, char *fname, int lineno, char *fmt, ...)
{
	//char errbuf[1024];
	char * errbuf;
	va_list ap;
	time_t t;
	struct tm *tm;

	if (level && level>log_level[facility] && level>E_FATAL)
		return;

	if (!log_fp)
		log_fp = stdout;

	// user log
	va_start(ap, fmt);
	//vsnprintf(errbuf, sizeof(errbuf), fmt, ap);
	if (vasprintf(&errbuf, fmt, ap) == -1)
	{
		va_end(ap);
		return;
	}
	va_end(ap);

	// timestamp
	t = time(NULL);
	tm = localtime(&t);
	fprintf(log_fp, "[%04d/%02d/%02d %02d:%02d:%02d] ",
	        tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
	        tm->tm_hour, tm->tm_min, tm->tm_sec);

	if (level)
		fprintf(log_fp, "%s:%d: %s: %s", fname, lineno, level_name[level], errbuf);
	else
		fprintf(log_fp, "%s:%d: %s", fname, lineno, errbuf);
	fflush(log_fp);
	free(errbuf);

	if (level==E_FATAL)
		exit(-1);

	return;
}
