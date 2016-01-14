/*
 * zebra string function
 *
 * XXX This version of snprintf does not check bounds!
 */

/*
 The implementations of strlcpy and strlcat are copied from rsync (GPL):
    Copyright (C) Andrew Tridgell 1998
    Copyright (C) 2002 by Martin Pool

 Note that these are not terribly efficient, since they make more than one
 pass over the argument strings.  At some point, they should be optimized.
 
 The implementation of strndup is copied from glibc-2.3.5:
    Copyright (C) 1996, 1997, 1998, 2001, 2002 Free Software Foundation, Inc.
*/

/* 
 * This file is part of Quagga.
 *
 * Quagga is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * Quagga is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Quagga; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include <zebra.h>

#ifndef HAVE_SNPRINTF
/*
 * snprint() is a real basic wrapper around the standard sprintf()
 * without any bounds checking
 */
int
snprintf(char *str, size_t size, const char *format, ...)
{
  va_list args;

  va_start (args, format);

  return vsprintf (str, format, args);
}
#endif

#ifndef HAVE_STRLCPY
/**
 * Like strncpy but does not 0 fill the buffer and always null 
 * terminates.
 *
 * @param bufsize is the size of the destination buffer.
 *
 * @return index of the terminating byte.
 **/
size_t
strlcpy(char *d, const char *s, size_t bufsize)
{
	size_t len = strlen(s);
	size_t ret = len;
	if (bufsize > 0) {
		if (len >= bufsize)
			len = bufsize-1;
		memcpy(d, s, len);
		d[len] = 0;
	}
	return ret;
}
#endif

#ifndef HAVE_STRLCAT
/**
 * Like strncat() but does not 0 fill the buffer and always null 
 * terminates.
 *
 * @param bufsize length of the buffer, which should be one more than
 * the maximum resulting string length.
 **/
size_t
strlcat(char *d, const char *s, size_t bufsize)
{
	size_t len1 = strlen(d);
	size_t len2 = strlen(s);
	size_t ret = len1 + len2;

	if (len1 < bufsize - 1) {
		if (len2 >= bufsize - len1)
			len2 = bufsize - len1 - 1;
		memcpy(d+len1, s, len2);
		d[len1+len2] = 0;
	}
	return ret;
}
#endif

#ifndef HAVE_STRNLEN
size_t
strnlen(const char *s, size_t maxlen)
{
  const char *p;
  return (p = (const char *)memchr(s, '\0', maxlen)) ? (size_t)(p-s) : maxlen;
}
#endif

#ifndef HAVE_STRNDUP
char *
strndup (const char *s, size_t maxlen)
{
    size_t len = strnlen (s, maxlen);
    char *new = (char *) malloc (len + 1);

    if (new == NULL)
      return NULL;

    new[len] = '\0';
    return (char *) memcpy (new, s, len);
}
#endif
