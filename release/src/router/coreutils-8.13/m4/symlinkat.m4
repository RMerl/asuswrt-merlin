# serial 5
# See if we need to provide symlinkat replacement.

dnl Copyright (C) 2009-2011 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Written by Eric Blake.

AC_DEFUN([gl_FUNC_SYMLINKAT],
[
  AC_REQUIRE([gl_FUNC_OPENAT])
  AC_REQUIRE([gl_UNISTD_H_DEFAULTS])
  AC_REQUIRE([gl_USE_SYSTEM_EXTENSIONS])
  AC_CHECK_FUNCS_ONCE([symlinkat])
  if test $ac_cv_func_symlinkat = no; then
    HAVE_SYMLINKAT=0
  fi
])
