# getopt.m4
dnl Copyright (C) 2003-2004 Remi Denis-Courmont
dnl From Remi Denis-Courmont

AC_DEFUN([RDC_FUNC_GETOPT_LONG],
[AC_CHECK_HEADERS(getopt.h)
AH_TEMPLATE([HAVE_GETOPT_LONG], [Define to 1 if you have the `getopt_long' function.])
AC_SEARCH_LIBS(getopt_long, [gnugetopt], have_getopt_long=yes,
have_getopt_long=no)
if test $have_getopt_long = yes; then
  AC_DEFINE(HAVE_GETOPT_LONG)
  $1
else
  $2
  false
fi
])

AC_DEFUN([RDC_REPLACE_FUNC_GETOPT_LONG],
[AH_BOTTOM([/* Fallback replacement for GNU `getopt_long' */
#ifndef HAVE_GETOPT_LONG
# define getopt_long( argc, argv, optstring, longopts, longindex ) \
	getopt (argc, argv, optstring)
# if !GETOPT_STRUCT_OPTION && !HAVE_GETOPT_H
 struct option { const char *name; int has_arg; int *flag; int val; };
#  define GETOPT_STRUCT_OPTION 1
# endif
# ifndef required_argument
#  define no_argument 0
#  define required_argument 1
#  define optional_argument 2
# endif
#endif])
RDC_FUNC_GETOPT_LONG
])

