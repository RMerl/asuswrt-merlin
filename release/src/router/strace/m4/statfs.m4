dnl ### A macro to determine whether statfs64 is defined.
AC_DEFUN([AC_STATFS64],
[AC_MSG_CHECKING(for statfs64 in sys/vfs.h)
AC_CACHE_VAL(ac_cv_type_statfs64,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#ifdef LINUX
#include <linux/types.h>
#include <sys/vfs.h>
#endif]], [[struct statfs64 st;]])],[ac_cv_type_statfs64=yes],[ac_cv_type_statfs64=no])])
AC_MSG_RESULT($ac_cv_type_statfs64)
if test "$ac_cv_type_statfs64" = yes
then
	AC_DEFINE([HAVE_STATFS64], 1,
[Define if statfs64 is available in sys/vfs.h.])
fi
])
