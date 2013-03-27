dnl ### A macro to determine if off_t is a long long
AC_DEFUN([AC_OFF_T_IS_LONG_LONG],
[AC_MSG_CHECKING(for long long off_t)
AC_CACHE_VAL(ac_cv_have_long_long_off_t,
[AC_COMPILE_IFELSE([AC_LANG_SOURCE([[#include <sys/types.h>
char a[(sizeof (off_t) == sizeof (long long) &&
        sizeof (off_t) > sizeof (long)) - 1];
]])],[ac_cv_have_long_long_off_t=yes],[ac_cv_have_long_long_off_t=no])])
AC_MSG_RESULT($ac_cv_have_long_long_off_t)
if test "$ac_cv_have_long_long_off_t" = yes
then
	AC_DEFINE([HAVE_LONG_LONG_OFF_T], 1, [Define if off_t is a long long.])
fi
])

dnl ### A macro to determine if rlim_t is a long long
AC_DEFUN([AC_RLIM_T_IS_LONG_LONG],
[AC_MSG_CHECKING(for long long rlim_t)
AC_CACHE_VAL(ac_cv_have_long_long_rlim_t,
[AC_COMPILE_IFELSE([AC_LANG_SOURCE([[#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
char a[(sizeof (rlim_t) == sizeof (long long) &&
        sizeof (rlim_t) > sizeof (long)) - 1];
]])],[ac_cv_have_long_long_rlim_t=yes],[ac_cv_have_long_long_rlim_t=no])])
AC_MSG_RESULT($ac_cv_have_long_long_rlim_t)
if test "$ac_cv_have_long_long_rlim_t" = yes
then
	AC_DEFINE([HAVE_LONG_LONG_RLIM_T], 1, [Define if rlim_t is a long long.])
fi
])

dnl ### A macro to determine endianness of long long
AC_DEFUN([AC_LITTLE_ENDIAN_LONG_LONG],
[AC_MSG_CHECKING(for little endian long long)
AC_CACHE_VAL(ac_cv_have_little_endian_long_long,
[AC_RUN_IFELSE([AC_LANG_SOURCE([[
int main () {
	union {
		long long ll;
		int l [2];
	} u;
	u.ll = 0x12345678;
	if (u.l[0] == 0x12345678)
		return 0;
	return 1;
}
]])],[ac_cv_have_little_endian_long_long=yes],[ac_cv_have_little_endian_long_long=no],[
if test "x$ac_cv_c_bigendian" = "xyes"; then
	ac_cv_have_little_endian_long_long=no
else
	ac_cv_have_little_endian_long_long=yes
fi
])])
AC_MSG_RESULT($ac_cv_have_little_endian_long_long)
if test "$ac_cv_have_little_endian_long_long" = yes
then
	AC_DEFINE([HAVE_LITTLE_ENDIAN_LONG_LONG], 1,
[Define if long long is little-endian.])
fi
])
