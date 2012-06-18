dnl @synopsis GP_CHECK_POPT(FLAG)
dnl
dnl Check whether libpopt is available.
dnl FLAG must be one of
dnl    "mandatory"
dnl    "default-enabled"
dnl    "default-disabled"
dnl
AC_DEFUN([GP_CHECK_POPT],[
#
# [GP_CHECK_POPT]
#
AC_REQUIRE([GP_CONFIG_MSG])dnl
m4_if([$1],[mandatory],        [_GP_CHECK_POPT([mandatory])],
      [$1],[default-enabled],  [_GP_CHECK_POPT([disable])],
      [$1],[default-disabled], [_GP_CHECK_POPT([enable])],
      [m4_errprint(__file__:__line__:[ Error:
Illegal argument to $0: `$1'
Valid values are: mandatory, default-enabled, default-disabled
])m4_exit(1)])dnl
])dnl
dnl
AC_DEFUN([_GP_CHECK_POPT],[
m4_if([$1],[mandatory],[
try_popt=yes
require_popt=yes
],[
try_popt=auto
require_popt=no
AC_ARG_ENABLE([popt],
[AS_HELP_STRING([--$1-popt],[Do not use popt])],
[ if   test "x$withval" = no \
    || test "x$withval" = off \
    || test "x$withval" = false; 
  then
    try_popt=no
    require_popt=no
  elif test "x$withval" = yes \
    || test "x$withval" = on \
    || test "x$withval" = true
  then
    try_popt=yes
    require_popt=yes
  fi
])dnl
])dnl

AC_MSG_CHECKING([whether popt is required])
AC_MSG_RESULT([${require_popt}])

if test "$require_popt" != yes; then
	AC_MSG_CHECKING([whether popt is requested])
	AC_MSG_RESULT([${try_popt}])
fi

dnl Implicit AC_SUBST
AC_ARG_VAR([POPT_CFLAGS],[CPPFLAGS to compile with libpopt])dnl
AC_ARG_VAR([POPT_LIBS],[LDFLAGS to link with libpopt])dnl

have_popt=no

if test "x$POPT_CFLAGS" = "x" && test "x$POPT_LIBS" = "x"; then

	# try to find options to compile popt.h
	CPPFLAGS_save="$CPPFLAGS"
	popth_found=no
	for popt_prefix in "" /usr /usr/local
	do
		if test -n "${popt_prefix}"; then
			:
		elif test -d "${popt_prefix}/include"; then
			CPPFLAGS="-I${popt_prefix}/include ${CPPFLAGS}"
		else
			continue
		fi
		ac_cv_header_popt_h=""
		unset ac_cv_header_popt_h
		AC_CHECK_HEADER([popt.h], [popth_found=yes])
		if test "$popth_found" = yes; then break; fi
	done
	CPPFLAGS="$CPPFLAGS_save"
	if test "$popth_found" = "yes"; then
		if test "$popt_prefix" = ""; then
			POPT_CFLAGS=""
		else
			POPT_CFLAGS="-I${popt_prefix}/include"
		fi
	else
		AC_MSG_ERROR([
* Cannot autodetect popt.h
*
* Set POPT_CFLAGS and POPT_LIBS correctly.
])
	fi

	# try to find options to link against popt
	LDFLAGS_save="$LDFLAGS"
	popt_links=no
	for popt_prefix in /usr "" /usr/local; do
		# We could have "/usr" and "lib64" at the beginning of the
		# lists. Then the first tested location would
		# incidentally be the right one on 64bit systems, and
		# thus work around a bug in libtool on 32bit systems:
		#
		# 32bit libtool doesn't know about 64bit systems, and so the
		# compilation will fail when linking a 32bit library from
		# /usr/lib to a 64bit binary.
		#
		# This hack has been confirmed to workwith a
		# 32bit Debian Sarge and 64bit Fedora Core 3 system.
		for ldir in lib64 "" lib; do
			popt_libdir="${popt_prefix}/${ldir}"
			if test "${popt_libdir}" = "/"; then
				popt_libdir=""
			elif test -d "${popt_libdir}"; then
				LDFLAGS="-L${popt_libdir} ${LDFLAGS}"
			else
				continue
			fi
			# Avoid caching of results
			ac_cv_lib_popt_poptStuffArgs=""
			unset ac_cv_lib_popt_poptStuffArgs
			AC_CHECK_LIB([popt], [poptStuffArgs], [popt_links=yes])
			if test "$popt_links" = yes; then break; fi
		done
		if test "$popt_links" = yes; then break; fi
	done
	LDFLAGS="$LDFLAGS_save"
	if test "$popt_links" = "yes"; then
		if test "$popt_libdir" = ""; then
			POPT_LIBS="-lpopt"
		else
			POPT_LIBS="-L${popt_libdir} -lpopt"
		fi
	else
		AC_MSG_ERROR([
* Cannot autodetect library directory containing popt
*
* Set POPT_CFLAGS and POPT_LIBS correctly.
])
	fi
	have_popt=yes
elif test "x$POPT_CFLAGS" != "x" && test "x$POPT_LIBS" != "x"; then
    # just use the user specivied option
    popt_msg="yes (user specified)"
    have_popt=yes
else
	AC_MSG_ERROR([
* Fatal: Either set both POPT_CFLAGS and POPT_LIBS or neither.
])
fi

AC_MSG_CHECKING([if popt is functional])
if test "$require_popt$have_popt" = "yesno"; then
	AC_MSG_RESULT([no, but required])
	AC_MSG_ERROR([
* popt library not found
* Fatal: ${PACKAGE_NAME} (${PACKAGE_TARNAME}) requires popt
* Please install it and/or set POPT_CFLAGS and POPT_LIBS.
])
fi
AC_MSG_RESULT([${have_popt}])

GP_CONFIG_MSG([use popt library], [${have_popt}])
if test "$have_popt" = "yes"; then
	AC_DEFINE([HAVE_POPT],[1],[whether the popt library is available])
	GP_CONFIG_MSG([popt libs],[${POPT_LIBS}])
	GP_CONFIG_MSG([popt cppflags],[${POPT_CFLAGS}])
fi
AM_CONDITIONAL([HAVE_POPT],[test "$have_popt" = "yes"])
])dnl
dnl
dnl Please do not remove this:
dnl filetype: 7595380e-eff3-49e5-90ab-e40f1d544639
dnl I use this to find all the different instances of this file which 
dnl are supposed to be synchronized.
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
