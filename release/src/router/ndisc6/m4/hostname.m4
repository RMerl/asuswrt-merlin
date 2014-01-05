# hostname.m4
dnl Copyright (C) 2003-2004 Remi Denis-Courmont
dnl From Remi Denis-Courmont

AC_DEFUN([RDC_BUILD_HOSTNAME],
[AC_CACHE_CHECK([for build hostname],
rdc_cv_build_hostname,
[rdc_cv_build_hostname=`hostname -f 2>/dev/null || uname -n 2>/dev/null || hostname 2>/dev/null || echo "unknown"`
])
AC_DEFINE_UNQUOTED(PACKAGE_BUILD_HOSTNAME, "$rdc_cv_build_hostname",
		[Define to the hostname of the host who builds the package.])
])

