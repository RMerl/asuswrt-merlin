dnl Check to see if we should use the included popt

INCLUDED_POPT=auto
AC_ARG_WITH(included-popt,
[  --with-included-popt    use bundled popt library, not from system],
[ INCLUDED_POPT=$withval ])

AC_SUBST(POPT_LIBS)
AC_SUBST(POPT_CFLAGS)

if test x"$INCLUDED_POPT" != x"yes"; then
	AC_CHECK_HEADERS(popt.h)
	AC_CHECK_LIB(popt, poptGetContext, [ POPT_LIBS="-lpopt" ])
	if test x"$ac_cv_header_popt_h" = x"no" -o x"$ac_cv_lib_popt_poptGetContext" = x"no"; then
		INCLUDED_POPT=yes
		POPT_CFLAGS=""
	else
		INCLUDED_POPT=no
	fi
fi

AC_MSG_CHECKING(whether to use included popt)
AC_MSG_RESULT($INCLUDED_POPT)
if test x"$INCLUDED_POPT" != x"no"; then
	dnl find the popt sources. This is meant to work both for 
	dnl popt standalone builds, and builds of packages using popt
	poptdir=""
	poptpaths="$srcdir $srcdir/lib/popt $srcdir/popt $srcdir/../popt $srcdir/../lib/popt"
	for d in $poptpaths; do
		if test -f "$d/popt.c"; then
			poptdir="$d"		
			POPT_CFLAGS="-I$d"
			AC_SUBST(poptdir)
			break
		fi
	done
        if test x"$poptdir" = "x"; then
		AC_MSG_ERROR([cannot find popt source in $poptpaths])
	fi
	POPT_OBJ="popt.o findme.o poptconfig.o popthelp.o poptparse.o"
	AC_SUBST(POPT_OBJ)
	AC_CHECK_HEADERS([float.h alloca.h])
fi
