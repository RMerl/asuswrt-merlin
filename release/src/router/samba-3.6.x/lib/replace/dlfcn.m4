dnl dummies provided by dlfcn.c if not available
save_LIBS="$LIBS"
LIBS=""

libreplace_cv_dlfcn=no
AC_SEARCH_LIBS(dlopen, dl)

AC_CHECK_HEADERS(dlfcn.h)
AC_CHECK_FUNCS([dlopen dlsym dlerror dlclose],[],[libreplace_cv_dlfcn=yes])

libreplace_cv_shl=no
AC_SEARCH_LIBS(shl_load, sl)
AC_CHECK_HEADERS(dl.h)
AC_CHECK_FUNCS([shl_load shl_unload shl_findsym],[],[libreplace_cv_shl=yes])

AC_VERIFY_C_PROTOTYPE([void *dlopen(const char* filename, unsigned int flags)],
	[
	return 0;
	],[
	AC_DEFINE(DLOPEN_TAKES_UNSIGNED_FLAGS, 1, [Whether dlopen takes unsigned int flags])
	],[],[
	#include <dlfcn.h>
	])

if test x"${libreplace_cv_dlfcn}" = x"yes";then
	LIBREPLACEOBJ="${LIBREPLACEOBJ} $libreplacedir/dlfcn.o"
fi

LIBDL="$LIBS"
AC_SUBST(LIBDL)
LIBS="$save_LIBS"
