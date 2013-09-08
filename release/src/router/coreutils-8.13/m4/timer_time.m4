# timer_time.m4 serial 1
dnl Copyright (C) 2011 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Check for timer_settime, and set LIB_TIMER_TIME.

AC_DEFUN([gl_TIMER_TIME],
[
  dnl Based on clock_time.m4. See details there.

  AC_REQUIRE([gl_USE_SYSTEM_EXTENSIONS])

  LIB_TIMER_TIME=
  AC_SUBST([LIB_TIMER_TIME])
  gl_saved_libs=$LIBS
    AC_SEARCH_LIBS([timer_settime], [rt posix4],
                   [test "$ac_cv_search_timer_settime" = "none required" ||
                    LIB_TIMER_TIME=$ac_cv_search_timer_settime])
    AC_CHECK_FUNCS([timer_settime])
  LIBS=$gl_saved_libs
])
