/*
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - xattr support using filesystem xattrs

   Copyright (C) Andrew Tridgell 2004

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __LIB_UTIL_WRAP_XATTR_H__
#define __LIB_UTIL_WRAP_XATTR_H__

ssize_t wrap_fgetxattr(int fd, const char *name, void *value, size_t size);
ssize_t wrap_getxattr(const char *path, const char *name, void *value, size_t size);
int wrap_fsetxattr(int fd, const char *name, void *value, size_t size, int flags);
int wrap_setxattr(const char *path, const char *name, void *value, size_t size, int flags);
int wrap_fremovexattr(int fd, const char *name);
int wrap_removexattr(const char *path, const char *name);

#endif /* __LIB_UTIL_WRAP_XATTR_H__ */

