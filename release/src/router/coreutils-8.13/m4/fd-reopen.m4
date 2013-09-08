# Invoke open, but return either a desired file descriptor or -1.

dnl Copyright (C) 2005, 2009-2011 Free Software Foundation, Inc.

dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl Written by Paul Eggert.

AC_DEFUN([gl_FD_REOPEN],
[
  AC_LIBSOURCES([fd-reopen.c, fd-reopen.h])
  AC_LIBOBJ([fd-reopen])
])
