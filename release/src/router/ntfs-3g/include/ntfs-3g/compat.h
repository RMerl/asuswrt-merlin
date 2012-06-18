/*
 * compat.h - Tweaks for Windows compatibility.
 *
 * Copyright (c) 2002 Richard Russon
 * Copyright (c) 2002-2004 Anton Altaparmakov
 * Copyright (c) 2008-2009 Szabolcs Szakacsits
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _NTFS_COMPAT_H
#define _NTFS_COMPAT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef HAVE_FFS
extern int ffs(int i);
#endif /* HAVE_FFS */

#ifndef HAVE_DAEMON
extern int daemon(int nochdir, int noclose);
#endif /* HAVE_DAEMON */

#ifndef HAVE_STRSEP
extern char *strsep(char **stringp, const char *delim);
#endif /* HAVE_STRSEP */

#ifdef WINDOWS

#define HAVE_STDIO_H		/* mimic config.h */
#define HAVE_STDARG_H

#define atoll			_atoi64
#define fdatasync		commit
#define __inline__		inline
#define __attribute__(X)	/*nothing*/

#else /* !defined WINDOWS */

#ifndef O_BINARY
#define O_BINARY		0		/* unix is binary by default */
#endif

#endif /* defined WINDOWS */

#endif /* defined _NTFS_COMPAT_H */

