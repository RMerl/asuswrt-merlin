/* dirfd.c -- return the file descriptor associated with an open DIR*

   Copyright (C) 2001, 2006, 2008-2011 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Written by Jim Meyering. */

#include <config.h>

#include <dirent.h>
#include <errno.h>

int
dirfd (DIR *dir_p)
{
  int fd = DIR_TO_FD (dir_p);
  if (fd == -1)
    errno = ENOTSUP;
  return fd;
}
