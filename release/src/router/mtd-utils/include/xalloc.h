/*
 * memory wrappers
 *
 * Copyright (c) Artem Bityutskiy, 2007, 2008
 * Copyright 2001, 2002 Red Hat, Inc.
 *           2001 David A. Schleef <ds@lineo.com>
 *           2002 Axis Communications AB
 *           2001, 2002 Erik Andersen <andersen@codepoet.org>
 *           2004 University of Szeged, Hungary
 *           2006 KaiGai Kohei <kaigai@ak.jp.nec.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __MTD_UTILS_XALLOC_H__
#define __MTD_UTILS_XALLOC_H__

#include <stdlib.h>
#include <string.h>

/*
 * Mark these functions as unused so that gcc does not emit warnings
 * when people include this header but don't use every function.
 */

__attribute__((unused))
static void *xmalloc(size_t size)
{
	void *ptr = malloc(size);

	if (ptr == NULL && size != 0)
		sys_errmsg_die("out of memory");
	return ptr;
}

__attribute__((unused))
static void *xcalloc(size_t nmemb, size_t size)
{
	void *ptr = calloc(nmemb, size);

	if (ptr == NULL && nmemb != 0 && size != 0)
		sys_errmsg_die("out of memory");
	return ptr;
}

__attribute__((unused))
static void *xzalloc(size_t size)
{
	return xcalloc(1, size);
}

__attribute__((unused))
static void *xrealloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (ptr == NULL && size != 0)
		sys_errmsg_die("out of memory");
	return ptr;
}

__attribute__((unused))
static char *xstrdup(const char *s)
{
	char *t;

	if (s == NULL)
		return NULL;
	t = strdup(s);
	if (t == NULL)
		sys_errmsg_die("out of memory");
	return t;
}

#ifdef _GNU_SOURCE
#include <stdarg.h>

__attribute__((unused))
static int xasprintf(char **strp, const char *fmt, ...)
{
	int cnt;
	va_list ap;

	va_start(ap, fmt);
	cnt = vasprintf(strp, fmt, ap);
	va_end(ap);

	if (cnt == -1)
		sys_errmsg_die("out of memory");

	return cnt;
}
#endif

#endif /* !__MTD_UTILS_XALLOC_H__ */
