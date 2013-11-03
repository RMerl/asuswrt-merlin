dnl Autoconf macro to check for the existence of grep

AC_DEFUN([AC_PROG_GREP], [
AC_REQUIRE([AC_EXEEXT])dnl
test x$GREP = x && AC_PATH_PROG(GREP, grep$EXEEXT, grep$EXEEXT)
test x$GREP = x && AC_MSG_ERROR([no acceptable grep found in \$PATH])
])

AC_SUBST(GREP)
