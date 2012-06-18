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

#include "includes.h"
#include "system/filesys.h"
#include "../lib/util/wrap_xattr.h"

#if defined(HAVE_XATTR_SUPPORT) && defined(XATTR_ADDITIONAL_OPTIONS)
static ssize_t _wrap_darwin_fgetxattr(int fd, const char *name, void *value, size_t size)
{
	return fgetxattr(fd, name, value, size, 0, 0);
}
static ssize_t _wrap_darwin_getxattr(const char *path, const char *name, void *value, size_t size)
{
	return getxattr(path, name, value, size, 0, 0);
}
static int _wrap_darwin_fsetxattr(int fd, const char *name, void *value, size_t size, int flags)
{
	return fsetxattr(fd, name, value, size, 0, flags);
}
static int _wrap_darwin_setxattr(const char *path, const char *name, void *value, size_t size, int flags)
{
	return setxattr(path, name, value, size, 0, flags);
}
static int _wrap_darwin_fremovexattr(int fd, const char *name)
{
	return fremovexattr(fd, name, 0);
}
static int _wrap_darwin_removexattr(const char *path, const char *name)
{
	return removexattr(path, name, 0);
}
#define fgetxattr	_wrap_darwin_fgetxattr
#define getxattr	_wrap_darwin_getxattr
#define fsetxattr	_wrap_darwin_fsetxattr
#define setxattr	_wrap_darwin_setxattr
#define fremovexattr	_wrap_darwin_fremovexattr
#define removexattr	_wrap_darwin_removexattr
#elif !defined(HAVE_XATTR_SUPPORT)
static ssize_t _none_fgetxattr(int fd, const char *name, void *value, size_t size)
{
	errno = ENOSYS;
	return -1;
}
static ssize_t _none_getxattr(const char *path, const char *name, void *value, size_t size)
{
	errno = ENOSYS;
	return -1;
}
static int _none_fsetxattr(int fd, const char *name, void *value, size_t size, int flags)
{
	errno = ENOSYS;
	return -1;
}
static int _none_setxattr(const char *path, const char *name, void *value, size_t size, int flags)
{
	errno = ENOSYS;
	return -1;
}
static int _none_fremovexattr(int fd, const char *name)
{
	errno = ENOSYS;
	return -1;
}
static int _none_removexattr(const char *path, const char *name)
{
	errno = ENOSYS;
	return -1;
}
#define fgetxattr	_none_fgetxattr
#define getxattr	_none_getxattr
#define fsetxattr	_none_fsetxattr
#define setxattr	_none_setxattr
#define fremovexattr	_none_fremovexattr
#define removexattr	_none_removexattr
#endif

_PUBLIC_ ssize_t wrap_fgetxattr(int fd, const char *name, void *value, size_t size)
{
	return fgetxattr(fd, name, value, size);
}
_PUBLIC_ ssize_t wrap_getxattr(const char *path, const char *name, void *value, size_t size)
{
	return getxattr(path, name, value, size);
}
_PUBLIC_ int wrap_fsetxattr(int fd, const char *name, void *value, size_t size, int flags)
{
	return fsetxattr(fd, name, value, size, flags);
}
_PUBLIC_ int wrap_setxattr(const char *path, const char *name, void *value, size_t size, int flags)
{
	return setxattr(path, name, value, size, flags);
}
_PUBLIC_ int wrap_fremovexattr(int fd, const char *name)
{
	return fremovexattr(fd, name);
}
_PUBLIC_ int wrap_removexattr(const char *path, const char *name)
{
	return removexattr(path, name);
}

