AC_CACHE_CHECK([whether strptime is available and works],libreplace_cv_STRPTIME_OK,[
	AC_TRY_RUN([
		#define LIBREPLACE_CONFIGURE_TEST_STRPTIME
		#include "$libreplacedir/test/strptime.c"
		],
		[libreplace_cv_STRPTIME_OK=yes],
		[libreplace_cv_STRPTIME_OK=no],
		[libreplace_cv_STRPTIME_OK="assuming not"])
])
if test x"$libreplace_cv_STRPTIME_OK" != x"yes"; then
        AC_DEFINE(REPLACE_STRPTIME,1,[Whether strptime should be replaced])
        LIBREPLACEOBJ="${LIBREPLACEOBJ} $libreplacedir/strptime.o"
fi
