/*
 * Unix SMB/CIFS implementation.
 * SMB parameters and setup
 * Copyright (C) Andrew Tridgell 1992-1998 Modified by Jeremy Allison 1995.
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"

/**
 Load from a pipe into memory.
**/

static char *file_pload(const char *syscmd, size_t *size)
{
	int fd, n;
	char *p;
	char buf[1024];
	size_t total;

	fd = sys_popen(syscmd);
	if (fd == -1) {
		return NULL;
	}

	p = NULL;
	total = 0;

	while ((n = sys_read(fd, buf, sizeof(buf))) > 0) {
		p = talloc_realloc(NULL, p, char, total + n + 1);
		if (!p) {
		        DEBUG(0,("file_pload: failed to expand buffer!\n"));
			close(fd);
			return NULL;
		}
		memcpy(p+total, buf, n);
		total += n;
	}

	if (p) {
		p[total] = 0;
	}

	/* FIXME: Perhaps ought to check that the command completed
	 * successfully (returned 0); if not the data may be
	 * truncated. */
	sys_pclose(fd);

	if (size) {
		*size = total;
	}

	return p;
}



/**
 Load a pipe into memory and return an array of pointers to lines in the data
 must be freed with file_lines_free(). 
**/

char **file_lines_pload(const char *syscmd, int *numlines)
{
	char *p;
	size_t size;

	p = file_pload(syscmd, &size);
	if (!p) {
		return NULL;
	}

	return file_lines_parse(p, size, numlines, NULL);
}
