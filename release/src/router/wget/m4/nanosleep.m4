# serial 36

dnl From Jim Meyering.
dnl Check for the nanosleep function.
dnl If not found, use the supplied replacement.
dnl

# Copyright (C) 1999-2001, 2003-2017 Free Software Foundation, Inc.

# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_NANOSLEEP],
[
 AC_REQUIRE([gl_HEADER_TIME_H_DEFAULTS])
 AC_REQUIRE([AC_CANONICAL_HOST]) dnl for cross-compiles

 dnl Persuade glibc and Solaris <time.h> to declare nanosleep.
 AC_REQUIRE([gl_USE_SYSTEM_EXTENSIONS])

 AC_CHECK_HEADERS_ONCE([sys/time.h])
 AC_REQUIRE([gl_FUNC_SELECT])

 nanosleep_save_libs=$LIBS

 # Solaris 2.5.1 needs -lposix4 to get the nanosleep function.
 # Solaris 7 prefers the library name -lrt to the obsolescent name -lposix4.
 LIB_NANOSLEEP=
 AC_SUBST([LIB_NANOSLEEP])
 AC_SEARCH_LIBS([nanosleep], [rt posix4],
                [test "$ac_cv_search_nanosleep" = "none required" ||
                 LIB_NANOSLEEP=$ac_cv_search_nanosleep])
 if test "x$ac_cv_search_nanosleep" != xno; then
   dnl The system has a nanosleep function.

   AC_REQUIRE([gl_MULTIARCH])
   if test $APPLE_UNIVERSAL_BUILD = 1; then
     # A universal build on Apple Mac OS X platforms.
     # The test result would be 'no (mishandles large arguments)' in 64-bit
     # mode but 'yes' in 32-bit mode. But we need a configuration result that
     # is valid in both modes.
     gl_cv_func_nanosleep='no (mishandles large arguments)'
   fi

   AC_CACHE_CHECK([for working nanosleep],
    [gl_cv_func_nanosleep],
    [
     AC_RUN_IFELSE(
       [AC_LANG_SOURCE([[
          #include <errno.h>
          #include <limits.h>
          #include <signal.h>
          #if HAVE_SYS_TIME_H
           #include <sys/time.h>
          #endif
          #include <time.h>
          #include <unistd.h>
          #define TYPE_SIGNED(t) (! ((t) 0 < (t) -1))
          #define TYPE_MAXIMUM(t) \
            ((t) (! TYPE_SIGNED (t) \
                  ? (t) -1 \
                  : ((((t) 1 << (sizeof (t) * CHAR_BIT - 2)) - 1) * 2 + 1)))

          static void
          check_for_SIGALRM (int sig)
          {
            if (sig != SIGALRM)
              _exit (1);
          }

          int
          main ()
          {
            static struct timespec ts_sleep;
            static struct timespec ts_remaining;
            static struct sigaction act;
            /* Test for major problems first.  */
            if (! nanosleep)
              return 2;
            act.sa_handler = check_for_SIGALRM;
            sigemptyset (&act.sa_mask);
            sigaction (SIGALRM, &act, NULL);
            ts_sleep.tv_sec = 0;
            ts_sleep.tv_nsec = 1;
            alarm (1);
            if (nanosleep (&ts_sleep, NULL) != 0)
              return 3;
            /* Test for a minor problem: the handling of large arguments.  */
            ts_sleep.tv_sec = TYPE_MAXIMUM (time_t);
            ts_sleep.tv_nsec = 999999999;
            alarm (1);
            if (nanosleep (&ts_sleep, &ts_remaining) != -1)
              return 4;
            if (errno != EINTR)
              return 5;
            if (ts_remaining.tv_sec <= TYPE_MAXIMUM (time_t) - 10)
              return 6;
            return 0;
          }]])],
       [gl_cv_func_nanosleep=yes],
       [case $? in dnl (
        4|5|6) gl_cv_func_nanosleep='no (mishandles large arguments)';; dnl (
        *)   gl_cv_func_nanosleep=no;;
        esac],
       [case "$host_os" in dnl ((
          linux*) # Guess it halfway works when the kernel is Linux.
            gl_cv_func_nanosleep='guessing no (mishandles large arguments)' ;;
          *)      # If we don't know, assume the worst.
            gl_cv_func_nanosleep='guessing no' ;;
        esac
       ])
    ])
   case "$gl_cv_func_nanosleep" in
     *yes)
       REPLACE_NANOSLEEP=0
       ;;
     *)
       REPLACE_NANOSLEEP=1
       case "$gl_cv_func_nanosleep" in
         *"mishandles large arguments"*)
           AC_DEFINE([HAVE_BUG_BIG_NANOSLEEP], [1],
             [Define to 1 if nanosleep mishandles large arguments.])
           ;;
         *)
           # The replacement uses select(). Add $LIBSOCKET to $LIB_NANOSLEEP.
           for ac_lib in $LIBSOCKET; do
             case " $LIB_NANOSLEEP " in
               *" $ac_lib "*) ;;
               *) LIB_NANOSLEEP="$LIB_NANOSLEEP $ac_lib";;
             esac
           done
           ;;
       esac
       ;;
   esac
 else
   HAVE_NANOSLEEP=0
 fi
 LIBS=$nanosleep_save_libs
])

# Prerequisites of lib/nanosleep.c.
AC_DEFUN([gl_PREREQ_NANOSLEEP],
[
  AC_CHECK_HEADERS_ONCE([sys/select.h])
  gl_PREREQ_SIG_HANDLER_H
])
