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
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "upnpglobalvars.h"
#include "log.h"

static FILE *log_fp = NULL;
static const int _default_log_level = E_WARN;
int log_level[L_MAX];

const char *facility_name[] = {
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

const char *level_name[] = {
	"off",					// E_OFF
	"fatal",				// E_FATAL
	"error",				// E_ERROR
	"warn",					// E_WARN
	"info",					// E_INFO
	"debug",				// E_DEBUG
	"maxdebug",				// E_MAXDEBUG
	0
};

void
log_close(void)
{
	if (log_fp)
		fclose(log_fp);
}

int find_matching_name(const char* str, const char* names[]) {
	if (str == NULL) return -1;

	const char* start = strpbrk(str, ",=");
	int level, c = (start != NULL) ? start - str : strlen(str);
	for (level = 0; names[level] != 0; level++) {
		if (!(strncasecmp(names[level], str, c)))
			return level;
	}
	return -1;
}

int
log_init(const char *fname, const char *debug)
{
	int i;
	FILE *fp;

	int level = find_matching_name(debug, level_name);
	int default_log_level = (level == -1) ? _default_log_level : level;

	for (i=0; i<L_MAX; i++)
		log_level[i] = default_log_level;

	if (debug)
	{
		const char *rhs, *lhs, *nlhs;
		int level, facility;

		rhs = nlhs = debug;
		while (rhs && (rhs = strchr(rhs, '='))) {
			rhs++;
			level = find_matching_name(rhs, level_name);
			if (level == -1) {
				DPRINTF(E_WARN, L_GENERAL, "unknown level in debug string: %s", debug);
				continue;
			}

			lhs = nlhs;
			rhs = nlhs = strchr(rhs, ',');
			do {
				if (*lhs==',') lhs++;
				facility = find_matching_name(lhs, facility_name);
				if (facility == -1) {
					DPRINTF(E_WARN, L_GENERAL, "unknown debug facility in debug string: %s", debug);
				} else {
					log_level[facility] = level;
				}

				lhs = strpbrk(lhs, ",=");
			} while (*lhs && *lhs==',');
		}
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
	va_list ap;

	if (level && level>log_level[facility] && level>E_FATAL)
		return;

	if (!log_fp)
		log_fp = stdout;

	// timestamp
	if (!GETFLAG(SYSTEMD_MASK))
	{
		time_t t;
		struct tm *tm;
		t = time(NULL);
		tm = localtime(&t);
		fprintf(log_fp, "[%04d/%02d/%02d %02d:%02d:%02d] ",
		        tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
		        tm->tm_hour, tm->tm_min, tm->tm_sec);
	}

	if (level)
		fprintf(log_fp, "%s:%d: %s: ", fname, lineno, level_name[level]);
	else
		fprintf(log_fp, "%s:%d: ", fname, lineno);

	// user log
	va_start(ap, fmt);
	if (vfprintf(log_fp, fmt, ap) == -1)
	{
		va_end(ap);
		return;
	}
	va_end(ap);

	fflush(log_fp);

	if (level==E_FATAL)
		exit(-1);

	return;
}
