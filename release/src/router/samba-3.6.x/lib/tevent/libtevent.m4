dnl find the tevent sources. This is meant to work both for
dnl standalone builds, and builds of packages using libtevent

AC_SUBST(teventdir)

if test x"$teventdir" = "x"; then
	teventdir=""
	teventpaths="$srcdir $srcdir/../lib/tevent $srcdir/tevent $srcdir/../tevent"
	for d in $teventpaths; do
		if test -f "$d/tevent.c"; then
			teventdir="$d"
			break;
		fi
	done
	if test x"$teventdir" = "x"; then
	   AC_MSG_ERROR([cannot find libtevent source in $teventpaths])
	fi
fi

TEVENT_OBJ=""
TEVENT_CFLAGS=""
TEVENT_LIBS=""
AC_SUBST(TEVENT_OBJ)
AC_SUBST(TEVENT_CFLAGS)
AC_SUBST(TEVENT_LIBS)

TEVENT_CFLAGS="-I$teventdir"

TEVENT_OBJ="tevent.o tevent_debug.o tevent_util.o"
TEVENT_OBJ="$TEVENT_OBJ tevent_fd.o tevent_timed.o tevent_immediate.o tevent_signal.o"
TEVENT_OBJ="$TEVENT_OBJ tevent_req.o tevent_wakeup.o tevent_queue.o"
TEVENT_OBJ="$TEVENT_OBJ tevent_standard.o tevent_select.o"
TEVENT_OBJ="$TEVENT_OBJ tevent_poll.o"

AC_CHECK_HEADERS(sys/epoll.h)
AC_CHECK_FUNCS(epoll_create)
if test x"$ac_cv_header_sys_epoll_h" = x"yes" -a x"$ac_cv_func_epoll_create" = x"yes"; then
   TEVENT_OBJ="$TEVENT_OBJ tevent_epoll.o"
   AC_DEFINE(HAVE_EPOLL, 1, [Whether epoll available])
fi

if test x"$VERSIONSCRIPT" != "x"; then
    EXPORTSFILE=tevent.exports
    AC_SUBST(EXPORTSFILE)
fi

