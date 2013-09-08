/* fd-relative mkdir
   Copyright (C) 2005-2006, 2009-2011 Free Software Foundation, Inc.

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

/* written by Jim Meyering */

#include <config.h>

#include <unistd.h>

/* Solaris 10 has no function like this.
   Create a subdirectory, FILE, with mode MODE, in the directory
   open on descriptor FD.  If possible, do it without changing the
   working directory.  Otherwise, resort to using save_cwd/fchdir,
   then mkdir/restore_cwd.  If either the save_cwd or the restore_cwd
   fails, then give a diagnostic and exit nonzero.  */

#define AT_FUNC_NAME mkdirat
#define AT_FUNC_F1 mkdir
#define AT_FUNC_POST_FILE_PARAM_DECLS , mode_t mode
#define AT_FUNC_POST_FILE_ARGS        , mode
#include "at-func.c"
