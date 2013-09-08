# unlink.m4 serial 8
dnl Copyright (C) 2009-2011 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_UNLINK],
[
  AC_REQUIRE([gl_UNISTD_H_DEFAULTS])
  AC_REQUIRE([AC_CANONICAL_HOST])
  dnl Detect FreeBSD 7.2, AIX 7.1, Solaris 9 bug.
  AC_CACHE_CHECK([whether unlink honors trailing slashes],
    [gl_cv_func_unlink_honors_slashes],
    [touch conftest.file
     # Assume that if we have lstat, we can also check symlinks.
     if test $ac_cv_func_lstat = yes; then
       ln -s conftest.file conftest.lnk
     fi
     AC_RUN_IFELSE(
       [AC_LANG_PROGRAM(
         [[#include <unistd.h>
           #include <errno.h>
         ]],
         [[int result = 0;
           if (!unlink ("conftest.file/"))
             result |= 1;
           else if (errno != ENOTDIR)
             result |= 2;
#if HAVE_LSTAT
           if (!unlink ("conftest.lnk/"))
             result |= 4;
           else if (errno != ENOTDIR)
             result |= 8;
#endif
           return result;
         ]])],
      [gl_cv_func_unlink_honors_slashes=yes],
      [gl_cv_func_unlink_honors_slashes=no],
      [gl_cv_func_unlink_honors_slashes="guessing no"])
     rm -f conftest.file conftest.lnk])
  dnl Detect MacOS X 10.5.6 bug: On read-write HFS mounts, unlink("..") or
  dnl unlink("../..") succeeds without doing anything.
  AC_CACHE_CHECK([whether unlink of a parent directory fails as it should],
    [gl_cv_func_unlink_parent_fails],
    [case "$host_os" in
       darwin*)
         dnl Try to unlink a subdirectory of /tmp, because /tmp is usually on a
         dnl HFS mount on MacOS X. Use a subdirectory, owned by the current
         dnl user, because otherwise unlink() may fail due to permissions
         dnl reasons, and because when running as root we don't want to risk
         dnl destroying the entire /tmp.
         if {
              # Use the mktemp program if available. If not available, hide the error
              # message.
              tmp=`(umask 077 && mktemp -d /tmp/gtXXXXXX) 2>/dev/null` &&
              test -n "$tmp" && test -d "$tmp"
            } ||
            {
              # Use a simple mkdir command. It is guaranteed to fail if the directory
              # already exists.  $RANDOM is bash specific and expands to empty in shells
              # other than bash, ksh and zsh.  Its use does not increase security;
              # rather, it minimizes the probability of failure in a very cluttered /tmp
              # directory.
              tmp=/tmp/gt$$-$RANDOM
              (umask 077 && mkdir "$tmp")
            }; then
           mkdir "$tmp/subdir"
           GL_SUBDIR_FOR_UNLINK="$tmp/subdir"
           export GL_SUBDIR_FOR_UNLINK
           AC_RUN_IFELSE(
             [AC_LANG_SOURCE([[
                #include <stdlib.h>
                #include <unistd.h>
                int main ()
                {
                  int result = 0;
                  if (chdir (getenv ("GL_SUBDIR_FOR_UNLINK")) != 0)
                    result |= 1;
                  else if (unlink ("..") == 0)
                    result |= 2;
                  return result;
                }
              ]])],
             [gl_cv_func_unlink_parent_fails=yes],
             [gl_cv_func_unlink_parent_fails=no],
             [gl_cv_func_unlink_parent_fails="guessing no"])
           unset GL_SUBDIR_FOR_UNLINK
           rm -rf "$tmp"
         else
           gl_cv_func_unlink_parent_fails="guessing no"
         fi
         ;;
       *)
         gl_cv_func_unlink_parent_fails="guessing yes"
         ;;
     esac
    ])
  case "$gl_cv_func_unlink_parent_fails" in
    *no)
      AC_DEFINE([UNLINK_PARENT_BUG], [1],
        [Define to 1 if unlink() on a parent directory may succeed])
      ;;
  esac
  if test "$gl_cv_func_unlink_honors_slashes" != yes \
     || { case "$gl_cv_func_unlink_parent_fails" in
            *yes) false;;
            *no) true;;
          esac
        }; then
    REPLACE_UNLINK=1
  fi
])
