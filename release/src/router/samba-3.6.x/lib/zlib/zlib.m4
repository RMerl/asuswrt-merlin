AC_DEFUN([AC_ZLIB],[
AC_CHECK_HEADERS(zlib.h)

AC_CHECK_LIB_EXT(z, ZLIB_LIBS, zlibVersion)

AC_CACHE_CHECK([for zlib >= 1.2.3], samba_cv_zlib_1_2_3, [
	AC_TRY_COMPILE([
		#include <zlib.h>
		],[
		#if (ZLIB_VERNUM >= 0x1230)
		#else
		#error "ZLIB_VERNUM < 0x1230"
		#endif
		],[
		samba_cv_zlib_1_2_3=yes
		],[
		samba_cv_zlib_1_2_3=no
		])
])

if test x"$ac_cv_header_zlib_h" = x"yes" -a \
	x"$ac_cv_lib_ext_z_zlibVersion" = x"yes" -a \
	x"$samba_cv_zlib_1_2_3" = x"yes"; then
	$1
else
	$2
fi
])

