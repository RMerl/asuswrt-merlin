dnl # Server process model subsystem

SMB_ENABLE(process_model_thread,NO)

#################################################
# check for pthread support
AC_MSG_CHECKING(whether to use pthreads)
AC_ARG_WITH(pthreads,
[AS_HELP_STRING([--with-pthreads],[Include pthreads (default=no)])],
[ case "$withval" in
	yes)
		AC_MSG_RESULT(yes)
		if test x"$ac_cv_func_pread" != x"yes" -o x"$ac_cv_func_pwrite" != x"yes";then
			AC_MSG_ERROR([You cannot enable threads when you don't have pread/pwrite!])
		fi
		SMB_ENABLE(process_model_thread,YES)
		SMB_ENABLE(PTHREAD,YES)
	;;
	*)
		AC_MSG_RESULT(no)
	;;
  esac ],
AC_MSG_RESULT(no)
)

SMB_EXT_LIB(PTHREAD,[-lpthread])

AC_MSG_CHECKING(whether to search for setproctitle support)
AC_ARG_WITH(setproctitle,
[AS_HELP_STRING([--with-setproctitle], [Search for setproctitle support (default=no)])],
[ case "$withval" in
	yes)
		AC_MSG_RESULT(yes)
		AC_CHECK_HEADERS(setproctitle.h)
		AC_CHECK_FUNC(setproctitle, [], [
		   AC_CHECK_LIB_EXT(setproctitle, SETPROCTITLE_LIBS, setproctitle)
		])
		AC_MSG_CHECKING(whether to use setproctitle)
		if test x"$ac_cv_func_setproctitle" = x"yes" -o \
		   \( x"$ac_cv_header_setproctitle_h" = x"yes" -a \
		    x"$ac_cv_lib_ext_setproctitle_setproctitle" = x"yes" \) ; then
			AC_MSG_RESULT(yes)
			SMB_ENABLE(SETPROCTITLE, YES)
			AC_DEFINE(HAVE_SETPROCTITLE,1,[Whether setproctitle() is available])
		else
			AC_MSG_RESULT(no)
		fi
	;;
	*)
		AC_MSG_RESULT(no)
	;;
  esac ],
AC_MSG_RESULT(no)
)

SMB_EXT_LIB(SETPROCTITLE,
	[${SETPROCTITLE_LIBS}],
	[${SETPROCTITLE_CFLAGS}],
	[${SETPROCTITLE_CPPFLAGS}],
	[${SETPROCTITLE_LDFLAGS}])
