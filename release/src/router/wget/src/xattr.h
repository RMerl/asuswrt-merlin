/* xattr.h -- POSIX Extended Attribute function mappings.

   Copyright (C) 2016 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.  */

#include <stdio.h>

#ifndef _XATTR_H
#define _XATTR_H

/* Store metadata name/value attributes against fp. */
int set_file_metadata (const char *origin_url, const char *referrer_url, FILE *fp);

#if defined(__linux)
/* libc on Linux has fsetxattr (5 arguments). */
#  include <sys/xattr.h>
#  define USE_XATTR
#elif defined(__APPLE__)
/* libc on OS/X has fsetxattr (6 arguments). */
#  include <sys/xattr.h>
#  define fsetxattr(file, name, buffer, size, flags) \
          fsetxattr ((file), (name), (buffer), (size), 0, (flags))
#  define USE_XATTR
#elif defined(__FreeBSD_version) && (__FreeBSD_version > 500000)
/* FreeBSD */
#  include <sys/types.h>
#  include <sys/extattr.h>
#  define fsetxattr(file, name, buffer, size, flags) \
          extattr_set_fd ((file), EXTATTR_NAMESPACE_USER, (name), (buffer), (size))
#  define USE_XATTR
#endif

#endif /* _XATTR_H */
