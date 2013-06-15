dnl dummies provided by dlfcn.c if not available
save_LIBS="$LIBS"
LIBS=""

libreplace_cv_dlfcn=no
AC_SEARCH_LIBS(dlopen, dl)

if test x"${ac_cv_search_dlopen}" = x"no"; then
	libreplace_cv_dlfcn=yes
else
	AC_CHECK_HEADERS(dlfcn.h)
	AC_CHECK_FUNCS([dlopen dlsym dlerror dlclose],[],[libreplace_cv_dlfcn=yes])
fi

if test x"${libreplace_cv_dlfcn}" = x"yes";then
	LIBREPLACEOBJ="${LIBREPLACEOBJ} dlfcn.o"
fi

LIBDL="$LIBS"
AC_SUBST(LIBDL)
LIBS="$save_LIBS"
