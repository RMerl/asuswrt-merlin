/* Private details of the DIR type.
   Copyright (C) 2011-2017 Free Software Foundation, Inc.

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

#ifndef _DIRENT_PRIVATE_H
#define _DIRENT_PRIVATE_H 1

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct gl_directory
{
  /* Status, or error code to produce in next readdir() call.
     -2 means the end of the directory is already reached,
     -1 means the entry was already filled by FindFirstFile,
     0 means the entry needs to be filled using FindNextFile.
     A positive value is an error code.  */
  int status;
  /* Handle, reading the directory, at current position.  */
  HANDLE current;
  /* Found directory entry.  */
  WIN32_FIND_DATA entry;
  /* Argument to pass to FindFirstFile.  It consists of the absolutized
     directory name, followed by a directory separator and the wildcards.  */
  char dir_name_mask[1];
};

#endif /* _DIRENT_PRIVATE_H */
