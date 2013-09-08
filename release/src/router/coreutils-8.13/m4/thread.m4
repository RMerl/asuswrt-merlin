# thread.m4 serial 2
dnl Copyright (C) 2008-2011 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_THREAD],
[
  AC_REQUIRE([gl_THREADLIB])
  AC_REQUIRE([AC_C_INLINE])

  if test $gl_threads_api = posix; then
    gl_save_LIBS="$LIBS"
    LIBS="$LIBS $LIBMULTITHREAD"
    AC_CHECK_FUNCS([pthread_atfork])
    LIBS="$gl_save_LIBS"
  fi
])
