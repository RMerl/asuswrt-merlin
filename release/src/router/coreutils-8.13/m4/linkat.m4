# serial 6
# See if we need to provide linkat replacement.

dnl Copyright (C) 2009-2011 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Written by Eric Blake.

AC_DEFUN([gl_FUNC_LINKAT],
[
  AC_REQUIRE([gl_FUNC_OPENAT])
  AC_REQUIRE([gl_FUNC_LINK_FOLLOWS_SYMLINK])
  AC_REQUIRE([gl_UNISTD_H_DEFAULTS])
  AC_REQUIRE([gl_USE_SYSTEM_EXTENSIONS])
  AC_REQUIRE([AC_CANONICAL_HOST]) dnl for cross-compiles
  AC_CHECK_FUNCS_ONCE([linkat symlink])
  AC_CHECK_HEADERS_ONCE([sys/param.h])
  if test $ac_cv_func_linkat = no; then
    HAVE_LINKAT=0
  else
    AC_CACHE_CHECK([whether linkat(,AT_SYMLINK_FOLLOW) works],
      [gl_cv_func_linkat_follow],
      [rm -rf conftest.f1 conftest.f2
       touch conftest.f1
       AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <fcntl.h>
#include <unistd.h>
#ifdef __linux__
/* Linux added linkat in 2.6.16, but did not add AT_SYMLINK_FOLLOW
   until 2.6.18.  Always replace linkat to support older kernels.  */
choke me
#endif
]], [return linkat (AT_FDCWD, "conftest.f1", AT_FDCWD, "conftest.f2",
                    AT_SYMLINK_FOLLOW);])],
         [gl_cv_func_linkat_follow=yes],
         [gl_cv_func_linkat_follow="need runtime check"])
       rm -rf conftest.f1 conftest.f2])
    AC_CACHE_CHECK([whether linkat handles trailing slash correctly],
      [gl_cv_func_linkat_slash],
      [rm -rf conftest.a conftest.b conftest.c conftest.d
       AC_RUN_IFELSE(
         [AC_LANG_PROGRAM(
            [[#include <unistd.h>
              #include <fcntl.h>
              #include <errno.h>
              #include <stdio.h>
            ]],
            [[int result;
              int fd;
              /* Create a regular file.  */
              fd = open ("conftest.a", O_CREAT | O_EXCL | O_WRONLY, 0600);
              if (fd < 0)
                return 1;
              if (write (fd, "hello", 5) < 5)
                return 2;
              if (close (fd) < 0)
                return 3;
              /* Test whether hard links are supported on the current
                 device.  */
              if (linkat (AT_FDCWD, "conftest.a", AT_FDCWD, "conftest.b",
                          AT_SYMLINK_FOLLOW) < 0)
                return 0;
              result = 0;
              /* Test whether a trailing "/" is treated like "/.".  */
              if (linkat (AT_FDCWD, "conftest.a/", AT_FDCWD, "conftest.c",
                          AT_SYMLINK_FOLLOW) == 0)
                result |= 4;
              if (linkat (AT_FDCWD, "conftest.a", AT_FDCWD, "conftest.d/",
                          AT_SYMLINK_FOLLOW) == 0)
                result |= 8;
              return result;
            ]])],
         [gl_cv_func_linkat_slash=yes],
         [gl_cv_func_linkat_slash=no],
         [# Guess yes on glibc systems, no otherwise.
          case "$host_os" in
            *-gnu*) gl_cv_func_linkat_slash="guessing yes";;
            *)      gl_cv_func_linkat_slash="guessing no";;
          esac
         ])
       rm -rf conftest.a conftest.b conftest.c conftest.d])
    case "$gl_cv_func_linkat_slash" in
      *yes) gl_linkat_slash_bug=0 ;;
      *)    gl_linkat_slash_bug=1 ;;
    esac
    if test "$gl_cv_func_linkat_follow" != yes \
       || test $gl_linkat_slash_bug = 1; then
      REPLACE_LINKAT=1
      AC_DEFINE_UNQUOTED([LINKAT_TRAILING_SLASH_BUG], [$gl_linkat_slash_bug],
        [Define to 1 if linkat fails to recognize a trailing slash.])
    fi
  fi
])
