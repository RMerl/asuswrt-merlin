dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)dnl
AC_INIT(Makefile.in)
AC_CONFIG_HEADER(config.h:config.in)

sinclude(../common/aclocal.m4)

# Bugs in autoconf 2.59 break the call to SIM_AC_COMMON, hack around
# it by inlining the macro's contents.
sinclude(../common/common.m4)

AC_CHECK_HEADERS(stdlib.h)

# In the Cygwin environment, we need some additional flags.
AC_CACHE_CHECK([for cygwin], sim_cv_os_cygwin,
[AC_EGREP_CPP(lose, [
#ifdef __CYGWIN__
lose
#endif],[sim_cv_os_cygwin=yes],[sim_cv_os_cygwin=no])])

if test x$sim_cv_os_cygwin = xyes; then
  TERMCAP='`if test -r ../../libtermcap/libtermcap.a; then echo ../../libtermcap/libtermcap.a; else echo -ltermcap; fi` -luser32'
else
  AC_CHECK_LIB(termcap, main, TERMCAP=-ltermcap, TERMCAP="")
fi
AC_SUBST(TERMCAP)

# We prefer the in-tree readline.  Top-level dependencies make sure
# src/readline (if it's there) is configured before src/sim.
if test -r ../../readline/Makefile; then
  READLINE=../../readline/libreadline.a
else
  AC_CHECK_LIB(readline, readline, READLINE=-lreadline,
	       AC_ERROR([the required "readline" library is missing]), $TERMCAP)
fi
AC_SUBST(READLINE)
SIM_AC_OUTPUT
