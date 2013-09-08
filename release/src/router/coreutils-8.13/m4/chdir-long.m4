#serial 12

# Use Gnulib's robust chdir function.
# It can handle arbitrarily long directory names, which means
# that when it is given the name of an existing directory, it
# never fails with ENAMETOOLONG.
# Arrange to compile chdir-long.c only on systems that define PATH_MAX.

dnl Copyright (C) 2004-2007, 2009-2011 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Written by Jim Meyering.

AC_DEFUN([gl_FUNC_CHDIR_LONG],
[
  AC_CACHE_CHECK([whether this system has an arbitrary file name length limit],
    gl_cv_have_arbitrary_file_name_length_limit,
    [AC_EGREP_CPP([have_arbitrary_file_name_length_limit],
                  [#include <unistd.h>
#include <limits.h>
#if defined PATH_MAX || defined MAXPATHLEN
have_arbitrary_file_name_length_limit
#endif],
    gl_cv_have_arbitrary_file_name_length_limit=yes,
    gl_cv_have_arbitrary_file_name_length_limit=no)])
])

AC_DEFUN([gl_PREREQ_CHDIR_LONG],
[
  AC_REQUIRE([AC_C_INLINE])
  :
])
