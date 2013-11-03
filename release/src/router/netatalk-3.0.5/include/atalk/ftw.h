/* Copyright (C) 1992,1996-1999,2003,2004 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

/*
 *	X/Open Portability Guide 4.2: ftw.h
 */

#ifndef _ATALK_FTW_H
#define	_ATALK_FTW_H	1

#include <sys/types.h>
#include <sys/stat.h>

/* Values for the FLAG argument to the user function passed to `ftw'
   and 'nftw'.  */
enum
{
  FTW_F,		/* Regular file.  */
#define FTW_F	 FTW_F
  FTW_D,		/* Directory.  */
#define FTW_D	 FTW_D
  FTW_DNR,		/* Unreadable directory.  */
#define FTW_DNR	 FTW_DNR
  FTW_NS,		/* Unstatable file.  */
#define FTW_NS	 FTW_NS
  FTW_SL,		/* Symbolic link.  */
# define FTW_SL	 FTW_SL

/* These flags are only passed from the `nftw' function.  */
  FTW_DP,		/* Directory, all subdirs have been visited. */
# define FTW_DP	 FTW_DP
  FTW_SLN		/* Symbolic link naming non-existing file.  */
# define FTW_SLN FTW_SLN
};


/* Flags for fourth argument of `nftw'.  */
enum
{
  FTW_PHYS = 1,		/* Perform physical walk, ignore symlinks.  */
# define FTW_PHYS	FTW_PHYS
  FTW_MOUNT = 2,	/* Report only files on same file system as the
			   argument.  */
# define FTW_MOUNT	FTW_MOUNT
  FTW_CHDIR = 4,	/* Change to current directory while processing it.  */
# define FTW_CHDIR	FTW_CHDIR
  FTW_DEPTH = 8,	/* Report files in directory before directory itself.*/
# define FTW_DEPTH	FTW_DEPTH
  FTW_ACTIONRETVAL = 16	/* Assume callback to return FTW_* values instead of
			   zero to continue and non-zero to terminate.  */
#  define FTW_ACTIONRETVAL FTW_ACTIONRETVAL
};

/* Return values from callback functions.  */
enum
{
  FTW_CONTINUE = 0,	/* Continue with next sibling or for FTW_D with the
			   first child.  */
# define FTW_CONTINUE	FTW_CONTINUE
  FTW_STOP = 1,		/* Return from `ftw' or `nftw' with FTW_STOP as return
			   value.  */
# define FTW_STOP	FTW_STOP
  FTW_SKIP_SUBTREE = 2,	/* Only meaningful for FTW_D: Don't walk through the
			   subtree, instead just continue with its next
			   sibling. */
# define FTW_SKIP_SUBTREE FTW_SKIP_SUBTREE
  FTW_SKIP_SIBLINGS = 3,/* Continue with FTW_DP callback for current directory
			    (if FTW_DEPTH) and then its siblings.  */
# define FTW_SKIP_SIBLINGS FTW_SKIP_SIBLINGS
};

/* Structure used for fourth argument to callback function for `nftw'.  */
struct FTW
  {
    int base;
    int level;
  };

/* Convenient types for callback functions.  */
typedef int (*nftw_func_t) (const char *filename,
                            const struct stat *status,
                            int flag,
                            struct FTW *info);
#define NFTW_FUNC_T nftw_func_t

typedef void (*dir_notification_func_t) (void);

extern int nftw(const char *dir,
                nftw_func_t func,
                dir_notification_func_t up,
                int descriptors,
                int flag);

#endif	/* ATALK_FTW_H */
