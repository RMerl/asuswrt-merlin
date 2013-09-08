# Compare numeric strings.

dnl Copyright (C) 2005, 2009-2011 Free Software Foundation, Inc.

dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl Written by Paul Eggert.

AC_DEFUN([gl_STRINTCMP],
[
  AC_LIBSOURCES([strintcmp.c, strnumcmp.h, strnumcmp-in.h])
  AC_LIBOBJ([strintcmp])

  dnl Prerequisites of lib/strintcmp.c.
  AC_REQUIRE([AC_INLINE])
])

AC_DEFUN([gl_STRNUMCMP],
[
  AC_LIBSOURCES([strnumcmp.c, strnumcmp.h, strnumcmp-in.h])
  AC_LIBOBJ([strnumcmp])

  dnl Prerequisites of lib/strnumcmp.c.
  AC_REQUIRE([AC_INLINE])
])
