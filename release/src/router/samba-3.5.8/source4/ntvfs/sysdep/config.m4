AC_CHECK_HEADERS(linux/inotify.h asm/unistd.h sys/inotify.h)
AC_CHECK_FUNC(inotify_init)
AC_HAVE_DECL(__NR_inotify_init, [#include <asm/unistd.h>])

SMB_ENABLE(sys_notify_inotify, NO)

if test x"$ac_cv_func_inotify_init" = x"yes" -a x"$ac_cv_header_sys_inotify_h" = x"yes"; then
    SMB_ENABLE(sys_notify_inotify, YES)
fi

if test x"$ac_cv_func_inotify_init" = x"yes" -a x"$ac_cv_header_linux_inotify_h" = x"yes"; then
    SMB_ENABLE(sys_notify_inotify, YES)
fi

if test x"$ac_cv_header_linux_inotify_h" = x"yes" -a x"$ac_cv_have___NR_inotify_init_decl" = x"yes"; then
    SMB_ENABLE(sys_notify_inotify, YES)
fi

AC_HAVE_DECL(F_SETLEASE, [#include <fcntl.h>])
AC_HAVE_DECL(SA_SIGINFO, [#include <signal.h>])

SMB_ENABLE(sys_lease_linux, NO)

if test x"$ac_cv_have_F_SETLEASE_decl" = x"yes" \
	-a x"$ac_cv_have_SA_SIGINFO_decl" = x"yes"; then
    SMB_ENABLE(sys_lease_linux, YES)
fi
