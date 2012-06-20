#serial 9

AC_DEFUN([gl_ERROR],
[
  AC_FUNC_ERROR_AT_LINE
  dnl Note: AC_FUNC_ERROR_AT_LINE does AC_LIBSOURCES([error.h, error.c]).
  jm_PREREQ_ERROR
])

# Prerequisites of lib/error.c.
AC_DEFUN([jm_PREREQ_ERROR],
[
  AC_REQUIRE([AC_FUNC_STRERROR_R])
  :
])
