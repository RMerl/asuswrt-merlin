dnl ### A macro to determine whether stat64 is defined.
AC_DEFUN([AC_STAT64],
[AC_MSG_CHECKING(for stat64 in (asm|sys)/stat.h)
AC_CACHE_VAL(ac_cv_type_stat64,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
#ifdef LINUX
#include <linux/types.h>
#include <asm/stat.h>
#else
#include <sys/stat.h>
#endif]], [[struct stat64 st;]])],[ac_cv_type_stat64=yes],[ac_cv_type_stat64=no])])
AC_MSG_RESULT($ac_cv_type_stat64)
if test "$ac_cv_type_stat64" = yes
then
	AC_DEFINE([HAVE_STAT64], 1,
[Define if stat64 is available in asm/stat.h.])
fi
])
