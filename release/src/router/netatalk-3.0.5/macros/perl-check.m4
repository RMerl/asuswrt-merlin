dnl Autoconf macro to check for the existence of Perl

AC_DEFUN([AC_PROG_PERL], [
	AC_REQUIRE([AC_EXEEXT])dnl
	test "x$PERL" = x && AC_PATH_PROG(PERL, perl$EXEEXT, perl$EXEEXT)
	test "x$PERL" = x && AC_MSG_ERROR([no acceptable Perl found in \$PATH])
])

AC_SUBST(PERL)
