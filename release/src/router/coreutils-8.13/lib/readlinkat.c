/* Read a symlink relative to an open directory.
   Copyright (C) 2009-2011 Free Software Foundation, Inc.

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

/* written by Eric Blake */

#include <config.h>

#include <unistd.h>

/* Gnulib provides a readlink stub for mingw; use it for distinction
   between EINVAL and ENOENT, rather than always failing with ENOSYS.  */

/* POSIX 2008 says that unlike readlink, readlinkat returns 0 for
   success instead of the buffer length.  But this would render
   readlinkat worthless since readlink does not guarantee a
   NUL-terminated buffer.  Assume this was a bug in POSIX.  */

/* Read the contents of symlink FILE into buffer BUF of size LEN, in the
   directory open on descriptor FD.  If possible, do it without changing
   the working directory.  Otherwise, resort to using save_cwd/fchdir,
   then readlink/restore_cwd.  If either the save_cwd or the restore_cwd
   fails, then give a diagnostic and exit nonzero.  */

#define AT_FUNC_NAME readlinkat
#define AT_FUNC_F1 readlink
#define AT_FUNC_POST_FILE_PARAM_DECLS , char *buf, size_t len
#define AT_FUNC_POST_FILE_ARGS        , buf, len
#define AT_FUNC_RESULT ssize_t
#include "at-func.c"
#undef AT_FUNC_NAME
#undef AT_FUNC_F1
#undef AT_FUNC_POST_FILE_PARAM_DECLS
#undef AT_FUNC_POST_FILE_ARGS
#undef AT_FUNC_RESULT
