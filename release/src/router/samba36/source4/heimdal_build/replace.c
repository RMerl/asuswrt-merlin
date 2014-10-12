/* 
   Unix SMB/CIFS implementation.

   some replacement functions for parts of roken that don't fit easily into 
   our build system

   Copyright (C) Andrew Tridgell 2005
   
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

#include "config.h"
#include "err.h"
#include "roken.h"
#include "system/filesys.h"

#ifndef HAVE_ERR
 void err(int eval, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	perror("");
	va_end(ap);
	exit(eval);
}
#endif

#ifndef HAVE_ERRX
 void errx(int eval, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	exit(eval);
}
#endif

#ifndef HAVE_WARNX
 void warnx(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}
#endif

#ifndef HAVE_FLOCK
 int flock(int fd, int op)
{
#undef flock
	struct flock lock;
	lock.l_whence = 0;
	lock.l_start = 0;
	lock.l_len = 0;
	lock.l_pid = 0;

	switch (op & (LOCK_UN|LOCK_SH|LOCK_EX)) {
	case LOCK_UN:
		lock.l_type = F_UNLCK;
		return fcntl(fd, F_SETLK, &lock);
	case LOCK_SH:
		lock.l_type = F_RDLCK;
		return fcntl(fd, (op&LOCK_NB)?F_SETLK:F_SETLKW, &lock);
	case LOCK_EX:
		lock.l_type = F_WRLCK;
		return fcntl(fd, (op&LOCK_NB)?F_SETLK:F_SETLKW, &lock);
	}
	errno = EINVAL;
	return -1;
}
#endif
