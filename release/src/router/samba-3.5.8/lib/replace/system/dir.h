#ifndef _system_dir_h
#define _system_dir_h
/* 
   Unix SMB/CIFS implementation.

   directory system include wrappers

   Copyright (C) Andrew Tridgell 2004
   
     ** NOTE! The following LGPL license applies to the replace
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#ifndef HAVE_MKDIR_MODE
#define mkdir(dir, mode) mkdir(dir)
#endif

/* Test whether a file name is the "." or ".." directory entries.
 * These really should be inline functions.
 */
#ifndef ISDOT
#define ISDOT(path) ( \
			*((const char *)(path)) == '.' && \
			*(((const char *)(path)) + 1) == '\0' \
		    )
#endif

#ifndef ISDOTDOT
#define ISDOTDOT(path)	( \
			    *((const char *)(path)) == '.' && \
			    *(((const char *)(path)) + 1) == '.' && \
			    *(((const char *)(path)) + 2) == '\0' \
			)
#endif

#endif
