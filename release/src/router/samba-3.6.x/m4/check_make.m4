dnl SMB Build Environment make Checks
dnl -------------------------------------------------------
dnl  Copyright (C) Stefan (metze) Metzmacher 2004
dnl  Copyright (C) Jelmer Vernooij 2005
dnl  Released under the GNU GPL
dnl -------------------------------------------------------
dnl

AC_DEFUN([AC_SAMBA_GNU_MAKE],
[
AC_PATH_PROGS(MAKE,gmake make)

AC_CACHE_CHECK([whether we have GNU make], samba_cv_gnu_make, [
if ! $ac_cv_path_MAKE --version | head -1 | grep GNU 2>/dev/null >/dev/null
then
	samba_cv_gnu_make=no
else
	samba_cv_gnu_make=yes
fi
])
if test x$samba_cv_gnu_make = xyes; then
	$1
else
	$2
fi
])

AC_DEFUN([AC_SAMBA_GNU_MAKE_VERSION], 
[
AC_CACHE_CHECK([GNU make version], samba_cv_gnu_make_version,[
		samba_cv_gnu_make_version=`$ac_cv_path_MAKE --version | head -1 | cut -d " " -f 3 2>/dev/null`
	])
])
