/* Utility functions
 *
 * Project : minidlna
 * Website : http://sourceforge.net/projects/minidlna/
 * Author  : Justin Maggard
 *
 * MiniDLNA media server
 * Copyright (C) 2008-2009  Justin Maggard
 *
 * This file is part of MiniDLNA.
 *
 * MiniDLNA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MiniDLNA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MiniDLNA. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdarg.h>
#include <dirent.h>
#include <sys/param.h>

#include "minidlnatypes.h"

/* String functions */
/* We really want this one inlined, since it has a major performance impact */
static inline int
__attribute__((__format__ (__printf__, 2, 3)))
strcatf(struct string_s *str, const char *fmt, ...)
{
	int ret;
	int size;
	va_list ap;

	if (str->off >= str->size)
		return 0;

	va_start(ap, fmt);
	size = str->size - str->off;
	ret = vsnprintf(str->data + str->off, size, fmt, ap);
	str->off += MIN(ret, size);
	va_end(ap);

	return ret;
}
static inline void strncpyt(char *dst, const char *src, size_t len)
{
	strncpy(dst, src, len);
	dst[len-1] = '\0';
}
static inline int is_reg(const struct dirent *d)
{
#if HAVE_STRUCT_DIRENT_D_TYPE
	return (d->d_type == DT_REG);
#else
	return -1;
#endif
}
static inline int is_dir(const struct dirent *d)
{
#if HAVE_STRUCT_DIRENT_D_TYPE
	return (d->d_type == DT_DIR);
#else
	return -1;
#endif
}
int xasprintf(char **strp, char *fmt, ...) __attribute__((__format__ (__printf__, 2, 3)));
int ends_with(const char * haystack, const char * needle);
char *trim(char *str);
char *strstrc(const char *s, const char *p, const char t);
char *strcasestrc(const char *s, const char *p, const char t);
char *modifyString(char *string, const char *before, const char *after, int noalloc);
char *escape_tag(const char *tag, int force_alloc);
char *unescape_tag(const char *tag, int force_alloc);
char *strip_ext(char *name);

/* Metadata functions */
int is_video(const char * file);
int is_audio(const char * file);
int is_image(const char * file);
int is_playlist(const char * file);
int is_caption(const char * file);
int is_album_art(const char * name);
int resolve_unknown_type(const char * path, media_types dir_type);
const char *mime_to_ext(const char * mime);

/* Others */
int make_dir(char * path, mode_t mode);
unsigned int DJBHash(uint8_t *data, int len);

#endif
