# serial 6
# See if we need to provide futimens replacement.

dnl Copyright (C) 2009-2011 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Written by Eric Blake.

AC_DEFUN([gl_FUNC_FUTIMENS],
[
  AC_REQUIRE([gl_SYS_STAT_H_DEFAULTS])
  AC_REQUIRE([gl_USE_SYSTEM_EXTENSIONS])
  AC_CHECK_FUNCS_ONCE([futimens])
  if test $ac_cv_func_futimens = no; then
    HAVE_FUTIMENS=0
  else
    AC_CACHE_CHECK([whether futimens works],
      [gl_cv_func_futimens_works],
      [AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
]], [[struct timespec ts[2] = { { 1, UTIME_OMIT }, { 1, UTIME_NOW } };
      int fd = creat ("conftest.file", 0600);
      struct stat st;
      if (fd < 0) return 1;
      errno = 0;
      if (futimens (AT_FDCWD, NULL) == 0) return 2;
      if (errno != EBADF) return 3;
      if (futimens (fd, ts)) return 4;
      sleep (1);
      ts[0].tv_nsec = UTIME_NOW;
      ts[1].tv_nsec = UTIME_OMIT;
      if (futimens (fd, ts)) return 5;
      if (fstat (fd, &st)) return 6;
      if (st.st_ctime < st.st_atime) return 7;
      ]])],
dnl FIXME: simplify this in 2012, when file system bugs are no longer common
         [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#ifdef __linux__
/* The Linux kernel added futimens in 2.6.22, but has bugs with UTIME_OMIT
   in several file systems as recently as 2.6.32.  Always replace futimens
   to support older kernels.  */
choke me
#endif
      ]])],
           [gl_cv_func_futimens_works=yes],
           [gl_cv_func_futimens_works="needs runtime check"])],
         [gl_cv_func_futimens_works=no],
         [gl_cv_func_futimens_works="guessing no"])
      rm -f conftest.file])
    if test "$gl_cv_func_futimens_works" != yes; then
      REPLACE_FUTIMENS=1
    fi
  fi
])
