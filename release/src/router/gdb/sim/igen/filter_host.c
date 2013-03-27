/* The IGEN simulator generator for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "filter_host.h"

/* Shorten traces by eliminating the directory component to filenames.  */
const char *
filter_filename (const char *filename)
{
  const char *p = filename;
  const char *last = filename;
  int ch;

  while ((ch = *p++) != '\0' && ch != ':')
    if (ch == '/')
      last = p;

  return last;
}
