AC_DEFUN_ONCE(AC_LIBREPLACE_LOCATION_CHECKS,
[
echo "LIBREPLACE_LOCATION_CHECKS: START"

dnl find the libreplace sources. This is meant to work both for 
dnl libreplace standalone builds, and builds of packages using libreplace
libreplacedir=""
libreplacepaths="$srcdir $srcdir/lib/replace $srcdir/libreplace $srcdir/../libreplace $srcdir/../replace $srcdir/../lib/replace $srcdir/../../../lib/replace"
for d in $libreplacepaths; do
	if test -f "$d/replace.c"; then
		libreplacedir="$d"		
		AC_SUBST(libreplacedir)
		break;
	fi
done
if test x"$libreplacedir" = "x"; then
	AC_MSG_ERROR([cannot find libreplace in $libreplacepaths])
fi
LIBREPLACEOBJ="$libreplacedir/replace.o"
AC_SUBST(LIBREPLACEOBJ)

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

echo "LIBREPLACE_LOCATION_CHECKS: END"
]) dnl end AC_LIBREPLACE_LOCATION_CHECKS


AC_DEFUN_ONCE(AC_LIBREPLACE_BROKEN_CHECKS,
[
echo "LIBREPLACE_BROKEN_CHECKS: START"

dnl find the libreplace sources. This is meant to work both for 
dnl libreplace standalone builds, and builds of packages using libreplace
libreplacedir=""
libreplacepaths="$srcdir $srcdir/lib/replace $srcdir/libreplace $srcdir/../libreplace $srcdir/../replace $srcdir/../lib/replace $srcdir/../../../lib/replace"
for d in $libreplacepaths; do
	if test -f "$d/replace.c"; then
		libreplacedir="$d"		
		AC_SUBST(libreplacedir)
		break;
	fi
done
if test x"$libreplacedir" = "x"; then
	AC_MSG_ERROR([cannot find libreplace in $libreplacepaths])
fi

LIBREPLACEOBJ="$libreplacedir/replace.o"
AC_SUBST(LIBREPLACEOBJ)

LIBREPLACEOBJ="${LIBREPLACEOBJ} $libreplacedir/snprintf.o"

AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_PID_T
AC_STRUCT_ST_RDEV
AC_CHECK_TYPE(ino_t,unsigned)
AC_CHECK_TYPE(loff_t,off_t)
AC_CHECK_TYPE(offset_t,loff_t)

AC_FUNC_MEMCMP

AC_CHECK_FUNCS([pipe strftime srandom random srand rand usleep setbuffer lstat getpgrp utime utimes])

AC_CHECK_HEADERS(stdbool.h stdint.h sys/select.h)
AC_CHECK_HEADERS(setjmp.h utime.h sys/wait.h)

LIBREPLACE_PROVIDE_HEADER([stdint.h])
LIBREPLACE_PROVIDE_HEADER([stdbool.h])

AC_CHECK_TYPE(bool, 
[AC_DEFINE(HAVE_BOOL, 1, [Whether the bool type is available])],,
[
AC_INCLUDES_DEFAULT
#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#endif]
)

AC_CHECK_TYPE(_Bool, 
[AC_DEFINE(HAVE__Bool, 1, [Whether the _Bool type is available])],,
[
AC_INCLUDES_DEFAULT
#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#endif]
)

AC_CHECK_HEADERS(linux/types.h)

AC_CACHE_CHECK([for working mmap],libreplace_cv_HAVE_MMAP,[
AC_TRY_RUN([#include "$libreplacedir/test/shared_mmap.c"],
           libreplace_cv_HAVE_MMAP=yes,libreplace_cv_HAVE_MMAP=no,libreplace_cv_HAVE_MMAP=cross)])
if test x"$libreplace_cv_HAVE_MMAP" = x"yes"; then
    AC_DEFINE(HAVE_MMAP,1,[Whether mmap works])
fi


AC_CHECK_HEADERS(sys/syslog.h syslog.h)
AC_CHECK_HEADERS(sys/time.h time.h)
AC_CHECK_HEADERS(stdarg.h vararg.h)
AC_CHECK_HEADERS(sys/mount.h mntent.h)
AC_CHECK_HEADERS(stropts.h)
AC_CHECK_HEADERS(unix.h)
AC_CHECK_HEADERS(sys/ucontext.h)

AC_CHECK_FUNCS(seteuid setresuid setegid setresgid chroot bzero strerror strerror_r)
AC_CHECK_FUNCS(vsyslog setlinebuf mktime ftruncate chsize rename)
AC_CHECK_FUNCS(waitpid wait4 strlcpy strlcat initgroups memmove strdup)
AC_CHECK_FUNCS(pread pwrite strndup strcasestr strtok_r mkdtemp dup2 dprintf vdprintf)
AC_CHECK_FUNCS(isatty chown lchown link readlink symlink realpath)
AC_CHECK_FUNCS(fdatasync,,[
	# if we didn't find it, look in librt (Solaris hides it there...)
	AC_CHECK_LIB(rt, fdatasync,
		[libreplace_cv_HAVE_FDATASYNC_IN_LIBRT=yes
		AC_DEFINE(HAVE_FDATASYNC, 1, Define to 1 if there is support for fdatasync)])
])
AC_HAVE_DECL(fdatasync, [#include <unistd.h>])
AC_CHECK_FUNCS(clock_gettime,libreplace_cv_have_clock_gettime=yes,[
	AC_CHECK_LIB(rt, clock_gettime,
		[libreplace_cv_HAVE_CLOCK_GETTIME_IN_LIBRT=yes
		libreplace_cv_have_clock_gettime=yes
		AC_DEFINE(HAVE_CLOCK_GETTIME, 1, Define to 1 if there is support for clock_gettime)])
])
AC_CHECK_FUNCS(get_current_dir_name)
AC_HAVE_DECL(setresuid, [#include <unistd.h>])
AC_HAVE_DECL(setresgid, [#include <unistd.h>])
AC_HAVE_DECL(errno, [#include <errno.h>])

AC_CACHE_CHECK([for secure mkstemp],libreplace_cv_HAVE_SECURE_MKSTEMP,[
AC_TRY_RUN([#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
main() { 
  struct stat st;
  char tpl[20]="/tmp/test.XXXXXX"; 
  int fd = mkstemp(tpl); 
  if (fd == -1) exit(1);
  unlink(tpl);
  if (fstat(fd, &st) != 0) exit(1);
  if ((st.st_mode & 0777) != 0600) exit(1);
  exit(0);
}],
libreplace_cv_HAVE_SECURE_MKSTEMP=yes,
libreplace_cv_HAVE_SECURE_MKSTEMP=no,
libreplace_cv_HAVE_SECURE_MKSTEMP=cross)])
if test x"$libreplace_cv_HAVE_SECURE_MKSTEMP" = x"yes"; then
    AC_DEFINE(HAVE_SECURE_MKSTEMP,1,[Whether mkstemp is secure])
fi

dnl Provided by snprintf.c:
AC_CHECK_HEADERS(stdio.h strings.h)
AC_CHECK_DECLS([snprintf, vsnprintf, asprintf, vasprintf])
AC_CHECK_FUNCS(snprintf vsnprintf asprintf vasprintf)

AC_CACHE_CHECK([for C99 vsnprintf],libreplace_cv_HAVE_C99_VSNPRINTF,[
AC_TRY_RUN([
#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
void foo(const char *format, ...) { 
       va_list ap;
       int len;
       char buf[20];
       long long l = 1234567890;
       l *= 100;

       va_start(ap, format);
       len = vsnprintf(buf, 0, format, ap);
       va_end(ap);
       if (len != 5) exit(1);

       va_start(ap, format);
       len = vsnprintf(0, 0, format, ap);
       va_end(ap);
       if (len != 5) exit(2);

       if (snprintf(buf, 3, "hello") != 5 || strcmp(buf, "he") != 0) exit(3);

       if (snprintf(buf, 20, "%lld", l) != 12 || strcmp(buf, "123456789000") != 0) exit(4);
       if (snprintf(buf, 20, "%zu", 123456789) != 9 || strcmp(buf, "123456789") != 0) exit(5);
       if (snprintf(buf, 20, "%2\$d %1\$d", 3, 4) != 3 || strcmp(buf, "4 3") != 0) exit(6);
       if (snprintf(buf, 20, "%s", 0) < 3) exit(7);

       exit(0);
}
main() { foo("hello"); }
],
libreplace_cv_HAVE_C99_VSNPRINTF=yes,libreplace_cv_HAVE_C99_VSNPRINTF=no,libreplace_cv_HAVE_C99_VSNPRINTF=cross)])
if test x"$libreplace_cv_HAVE_C99_VSNPRINTF" = x"yes"; then
    AC_DEFINE(HAVE_C99_VSNPRINTF,1,[Whether there is a C99 compliant vsnprintf])
fi


dnl VA_COPY
AC_CACHE_CHECK([for va_copy],libreplace_cv_HAVE_VA_COPY,[
AC_TRY_LINK([#include <stdarg.h>
va_list ap1,ap2;], [va_copy(ap1,ap2);],
libreplace_cv_HAVE_VA_COPY=yes,libreplace_cv_HAVE_VA_COPY=no)])
if test x"$libreplace_cv_HAVE_VA_COPY" = x"yes"; then
    AC_DEFINE(HAVE_VA_COPY,1,[Whether va_copy() is available])
fi

if test x"$libreplace_cv_HAVE_VA_COPY" != x"yes"; then
AC_CACHE_CHECK([for __va_copy],libreplace_cv_HAVE___VA_COPY,[
AC_TRY_LINK([#include <stdarg.h>
va_list ap1,ap2;], [__va_copy(ap1,ap2);],
libreplace_cv_HAVE___VA_COPY=yes,libreplace_cv_HAVE___VA_COPY=no)])
if test x"$libreplace_cv_HAVE___VA_COPY" = x"yes"; then
    AC_DEFINE(HAVE___VA_COPY,1,[Whether __va_copy() is available])
fi
fi

dnl __FUNCTION__ macro
AC_CACHE_CHECK([for __FUNCTION__ macro],libreplace_cv_HAVE_FUNCTION_MACRO,[
AC_TRY_COMPILE([#include <stdio.h>], [printf("%s\n", __FUNCTION__);],
libreplace_cv_HAVE_FUNCTION_MACRO=yes,libreplace_cv_HAVE_FUNCTION_MACRO=no)])
if test x"$libreplace_cv_HAVE_FUNCTION_MACRO" = x"yes"; then
    AC_DEFINE(HAVE_FUNCTION_MACRO,1,[Whether there is a __FUNCTION__ macro])
else
    dnl __func__ macro
    AC_CACHE_CHECK([for __func__ macro],libreplace_cv_HAVE_func_MACRO,[
    AC_TRY_COMPILE([#include <stdio.h>], [printf("%s\n", __func__);],
    libreplace_cv_HAVE_func_MACRO=yes,libreplace_cv_HAVE_func_MACRO=no)])
    if test x"$libreplace_cv_HAVE_func_MACRO" = x"yes"; then
       AC_DEFINE(HAVE_func_MACRO,1,[Whether there is a __func__ macro])
    fi
fi

AC_CHECK_HEADERS([sys/param.h limits.h])

AC_CHECK_TYPE(comparison_fn_t, 
[AC_DEFINE(HAVE_COMPARISON_FN_T, 1,[Whether or not we have comparison_fn_t])])

AC_HAVE_DECL(setenv, [#include <stdlib.h>])
AC_CHECK_FUNCS(setenv unsetenv)
AC_HAVE_DECL(environ, [#include <unistd.h>])

AC_CHECK_FUNCS(strnlen)
AC_CHECK_FUNCS(strtoull __strtoull strtouq strtoll __strtoll strtoq)

AC_CHECK_FUNCS(memmem)

# this test disabled as we don't actually need __VA_ARGS__ yet
AC_TRY_CPP([
#define eprintf(...) fprintf(stderr, __VA_ARGS__)
eprintf("bla", "bar");
], AC_DEFINE(HAVE__VA_ARGS__MACRO, 1, [Whether the __VA_ARGS__ macro is available]))


AC_CACHE_CHECK([for sig_atomic_t type],libreplace_cv_sig_atomic_t, [
    AC_TRY_COMPILE([
#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
#include <signal.h>],[sig_atomic_t i = 0],
	libreplace_cv_sig_atomic_t=yes,libreplace_cv_sig_atomic_t=no)])
if test x"$libreplace_cv_sig_atomic_t" = x"yes"; then
   AC_DEFINE(HAVE_SIG_ATOMIC_T_TYPE,1,[Whether we have the atomic_t variable type])
fi


dnl Check if the C compiler understands volatile (it should, being ANSI).
AC_CACHE_CHECK([that the C compiler understands volatile],libreplace_cv_volatile, [
	AC_TRY_COMPILE([#include <sys/types.h>],[volatile int i = 0],
		libreplace_cv_volatile=yes,libreplace_cv_volatile=no)])
if test x"$libreplace_cv_volatile" = x"yes"; then
	AC_DEFINE(HAVE_VOLATILE, 1, [Whether the C compiler understands volatile])
fi

m4_include(system/config.m4)

AC_CACHE_CHECK([for O_DIRECT flag to open(2)],libreplace_cv_HAVE_OPEN_O_DIRECT,[
AC_TRY_COMPILE([
#include <unistd.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif],
[int fd = open("/dev/null", O_DIRECT);],
libreplace_cv_HAVE_OPEN_O_DIRECT=yes,libreplace_cv_HAVE_OPEN_O_DIRECT=no)])
if test x"$libreplace_cv_HAVE_OPEN_O_DIRECT" = x"yes"; then
    AC_DEFINE(HAVE_OPEN_O_DIRECT,1,[Whether the open(2) accepts O_DIRECT])
fi

m4_include(dlfcn.m4)
m4_include(getpass.m4)
m4_include(strptime.m4)
m4_include(win32.m4)
m4_include(timegm.m4)
m4_include(repdir.m4)
m4_include(crypt.m4)

if test x$libreplace_cv_have_clock_gettime = xyes ; then
	SMB_CHECK_CLOCK_ID(CLOCK_MONOTONIC)
	SMB_CHECK_CLOCK_ID(CLOCK_PROCESS_CPUTIME_ID)
	SMB_CHECK_CLOCK_ID(CLOCK_REALTIME)
fi

AC_CACHE_CHECK([for struct timespec type],libreplace_cv_struct_timespec, [
    AC_TRY_COMPILE([
#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
],[struct timespec ts;],
	libreplace_cv_struct_timespec=yes,libreplace_cv_struct_timespec=no)])
if test x"$libreplace_cv_struct_timespec" = x"yes"; then
   AC_DEFINE(HAVE_STRUCT_TIMESPEC,1,[Whether we have struct timespec])
fi

AC_CACHE_CHECK([for ucontext_t type],libreplace_cv_ucontext_t, [
    AC_TRY_COMPILE([
#include <signal.h>
#if HAVE_SYS_UCONTEXT_H
#include <sys/ucontext.h>
# endif
],[ucontext_t uc; sigaddset(&uc.uc_sigmask, SIGUSR1);],
    libreplace_cv_ucontext_t=yes,libreplace_cv_ucontext_t=no)])
if test x"$libreplace_cv_ucontext_t" = x"yes"; then
    AC_DEFINE(HAVE_UCONTEXT_T,1,[Whether we have ucontext_t])
fi

AC_CHECK_FUNCS([printf memset memcpy],,[AC_MSG_ERROR([Required function not found])])

echo "LIBREPLACE_BROKEN_CHECKS: END"
]) dnl end AC_LIBREPLACE_BROKEN_CHECKS

AC_DEFUN_ONCE(AC__LIBREPLACE_ALL_CHECKS_START,
[
#LIBREPLACE_ALL_CHECKS: START"
])
AC_DEFUN_ONCE(AC__LIBREPLACE_ALL_CHECKS_END,
[
#LIBREPLACE_ALL_CHECKS: END"
])
m4_define(AC_LIBREPLACE_ALL_CHECKS,
[
AC__LIBREPLACE_ALL_CHECKS_START
AC_LIBREPLACE_LOCATION_CHECKS
AC_LIBREPLACE_CC_CHECKS
AC_LIBREPLACE_BROKEN_CHECKS
AC__LIBREPLACE_ALL_CHECKS_END
CFLAGS="$CFLAGS -I$libreplacedir"
])

m4_include(libreplace_cc.m4)
m4_include(libreplace_ld.m4)
m4_include(libreplace_network.m4)
m4_include(libreplace_macros.m4)


dnl SMB_CHECK_CLOCK_ID(clockid)
dnl Test whether the specified clock_gettime clock ID is available. If it
dnl is, we define HAVE_clockid
AC_DEFUN([SMB_CHECK_CLOCK_ID],
[
    AC_MSG_CHECKING(for $1)
    AC_TRY_LINK([
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
    ],
    [
clockid_t clk = $1;
    ],
    [
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_$1, 1,
	    [Whether the clock_gettime clock ID $1 is available])
    ],
    [
	AC_MSG_RESULT(no)
    ])
])
m4_ifndef([AC_USE_SYSTEM_EXTENSIONS],[m4_include(autoconf-2.60.m4)])
