AC_DEFUN_ONCE(AC_LIBREPLACE_LOCATION_CHECKS,
[
echo "LIBREPLACE_LOCATION_CHECKS: START"

dnl find the libreplace sources. This is meant to work both for 
dnl libreplace standalone builds, and builds of packages using libreplace
libreplacedir=""
libreplacepaths="$srcdir $srcdir/lib/replace $srcdir/libreplace $srcdir/../libreplace $srcdir/../replace"
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
LIBREPLACEOBJ="replace.o"
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
for d in "$srcdir" "$srcdir/lib/replace" "$srcdir/libreplace" "$srcdir/../libreplace" "$srcdir/../replace"; do
	if test -f "$d/replace.c"; then
		libreplacedir="$d"		
		AC_SUBST(libreplacedir)
		break;
	fi
done
LIBREPLACEOBJ="replace.o"
AC_SUBST(LIBREPLACEOBJ)

LIBREPLACEOBJ="${LIBREPLACEOBJ} snprintf.o"

AC_TYPE_SIGNAL
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

AC_CHECK_FUNCS(pipe strftime srandom random srand rand usleep setbuffer lstat getpgrp)

AC_CHECK_HEADERS(stdbool.h stdint.h sys/select.h)
AC_CHECK_HEADERS(setjmp.h)

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

AC_CACHE_CHECK([for working mmap],samba_cv_HAVE_MMAP,[
AC_TRY_RUN([#include "$libreplacedir/test/shared_mmap.c"],
           samba_cv_HAVE_MMAP=yes,samba_cv_HAVE_MMAP=no,samba_cv_HAVE_MMAP=cross)])
if test x"$samba_cv_HAVE_MMAP" = x"yes"; then
    AC_DEFINE(HAVE_MMAP,1,[Whether mmap works])
fi


AC_CHECK_HEADERS(sys/syslog.h syslog.h)
AC_CHECK_HEADERS(sys/time.h time.h)
AC_CHECK_HEADERS(stdarg.h vararg.h)
AC_CHECK_HEADERS(sys/socket.h netinet/in.h netdb.h arpa/inet.h)
AC_CHECK_HEADERS(netinet/ip.h netinet/tcp.h netinet/in_systm.h netinet/in_ip.h)
AC_CHECK_HEADERS(sys/sockio.h sys/un.h)
AC_CHECK_HEADERS(stropts.h)

dnl we need to check that net/if.h really can be used, to cope with hpux
dnl where including it always fails
AC_CACHE_CHECK([for usable net/if.h],libreplace_cv_USABLE_NET_IF_H,[
	AC_COMPILE_IFELSE([AC_LANG_SOURCE([
		AC_INCLUDES_DEFAULT
		#if HAVE_SYS_SOCKET_H
		# include <sys/socket.h>
		#endif
		#include <net/if.h>
		int main(void) {return 0;}])],
		[libreplace_cv_USABLE_NET_IF_H=yes],
		[libreplace_cv_USABLE_NET_IF_H=no]
	)
])
if test x"$libreplace_cv_USABLE_NET_IF_H" = x"yes";then
	AC_DEFINE(HAVE_NET_IF_H, 1, usability of net/if.h)
fi

AC_CACHE_CHECK([for broken inet_ntoa],samba_cv_REPLACE_INET_NTOA,[
AC_TRY_RUN([
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
main() { struct in_addr ip; ip.s_addr = 0x12345678;
if (strcmp(inet_ntoa(ip),"18.52.86.120") &&
    strcmp(inet_ntoa(ip),"120.86.52.18")) { exit(0); } 
exit(1);}],
           samba_cv_REPLACE_INET_NTOA=yes,samba_cv_REPLACE_INET_NTOA=no,samba_cv_REPLACE_INET_NTOA=cross)])
if test x"$samba_cv_REPLACE_INET_NTOA" = x"yes"; then
    AC_DEFINE(REPLACE_INET_NTOA,1,[Whether inet_ntoa should be replaced])
fi

dnl Provided by replace.c:
AC_TRY_COMPILE([
#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
#include <sys/socket.h>], 
[socklen_t foo;],,
[AC_DEFINE(socklen_t, int,[Socket length type])])

AC_CHECK_FUNCS(seteuid setresuid setegid setresgid chroot bzero strerror)
AC_CHECK_FUNCS(vsyslog setlinebuf mktime ftruncate chsize rename)
AC_CHECK_FUNCS(waitpid strlcpy strlcat initgroups memmove strdup)
AC_CHECK_FUNCS(pread pwrite strndup strcasestr strtok_r mkdtemp socketpair)
AC_HAVE_DECL(setresuid, [#include <unistd.h>])
AC_HAVE_DECL(setresgid, [#include <unistd.h>])
AC_HAVE_DECL(errno, [#include <errno.h>])

AC_CACHE_CHECK([for secure mkstemp],samba_cv_HAVE_SECURE_MKSTEMP,[
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
samba_cv_HAVE_SECURE_MKSTEMP=yes,
samba_cv_HAVE_SECURE_MKSTEMP=no,
samba_cv_HAVE_SECURE_MKSTEMP=cross)])
if test x"$samba_cv_HAVE_SECURE_MKSTEMP" = x"yes"; then
    AC_DEFINE(HAVE_SECURE_MKSTEMP,1,[Whether mkstemp is secure])
fi

dnl Provided by snprintf.c:
AC_CHECK_HEADERS(stdio.h strings.h)
AC_CHECK_DECLS([snprintf, vsnprintf, asprintf, vasprintf])
AC_CHECK_FUNCS(snprintf vsnprintf asprintf vasprintf)

AC_CACHE_CHECK([for C99 vsnprintf],samba_cv_HAVE_C99_VSNPRINTF,[
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
samba_cv_HAVE_C99_VSNPRINTF=yes,samba_cv_HAVE_C99_VSNPRINTF=no,samba_cv_HAVE_C99_VSNPRINTF=cross)])
if test x"$samba_cv_HAVE_C99_VSNPRINTF" = x"yes"; then
    AC_DEFINE(HAVE_C99_VSNPRINTF,1,[Whether there is a C99 compliant vsnprintf])
fi


dnl VA_COPY
AC_CACHE_CHECK([for va_copy],samba_cv_HAVE_VA_COPY,[
AC_TRY_LINK([#include <stdarg.h>
va_list ap1,ap2;], [va_copy(ap1,ap2);],
samba_cv_HAVE_VA_COPY=yes,samba_cv_HAVE_VA_COPY=no)])
if test x"$samba_cv_HAVE_VA_COPY" = x"yes"; then
    AC_DEFINE(HAVE_VA_COPY,1,[Whether va_copy() is available])
fi

if test x"$samba_cv_HAVE_VA_COPY" != x"yes"; then
AC_CACHE_CHECK([for __va_copy],samba_cv_HAVE___VA_COPY,[
AC_TRY_LINK([#include <stdarg.h>
va_list ap1,ap2;], [__va_copy(ap1,ap2);],
samba_cv_HAVE___VA_COPY=yes,samba_cv_HAVE___VA_COPY=no)])
if test x"$samba_cv_HAVE___VA_COPY" = x"yes"; then
    AC_DEFINE(HAVE___VA_COPY,1,[Whether __va_copy() is available])
fi
fi

dnl __FUNCTION__ macro
AC_CACHE_CHECK([for __FUNCTION__ macro],samba_cv_HAVE_FUNCTION_MACRO,[
AC_TRY_COMPILE([#include <stdio.h>], [printf("%s\n", __FUNCTION__);],
samba_cv_HAVE_FUNCTION_MACRO=yes,samba_cv_HAVE_FUNCTION_MACRO=no)])
if test x"$samba_cv_HAVE_FUNCTION_MACRO" = x"yes"; then
    AC_DEFINE(HAVE_FUNCTION_MACRO,1,[Whether there is a __FUNCTION__ macro])
else
    dnl __func__ macro
    AC_CACHE_CHECK([for __func__ macro],samba_cv_HAVE_func_MACRO,[
    AC_TRY_COMPILE([#include <stdio.h>], [printf("%s\n", __func__);],
    samba_cv_HAVE_func_MACRO=yes,samba_cv_HAVE_func_MACRO=no)])
    if test x"$samba_cv_HAVE_func_MACRO" = x"yes"; then
       AC_DEFINE(HAVE_func_MACRO,1,[Whether there is a __func__ macro])
    fi
fi

AC_CHECK_HEADERS([sys/param.h limits.h])

AC_CHECK_TYPE(comparison_fn_t, 
[AC_DEFINE(HAVE_COMPARISON_FN_T, 1,[Whether or not we have comparison_fn_t])])

AC_HAVE_DECL(setenv, [#include <stdlib.h>])
AC_CHECK_FUNCS(setenv unsetenv)

AC_CHECK_FUNCS(strnlen)
AC_CHECK_FUNCS(strtoull __strtoull strtouq strtoll __strtoll strtoq)

# this test disabled as we don't actually need __VA_ARGS__ yet
AC_TRY_CPP([
#define eprintf(...) fprintf(stderr, __VA_ARGS__)
eprintf("bla", "bar");
], AC_DEFINE(HAVE__VA_ARGS__MACRO, 1, [Whether the __VA_ARGS__ macro is available]))

# Check prerequisites
AC_CHECK_FUNCS([memset printf syslog], [], 
			   [ AC_MSG_ERROR([Required function not found])])

AC_CACHE_CHECK([for sig_atomic_t type],samba_cv_sig_atomic_t, [
    AC_TRY_COMPILE([
#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
#include <signal.h>],[sig_atomic_t i = 0],
	samba_cv_sig_atomic_t=yes,samba_cv_sig_atomic_t=no)])
if test x"$samba_cv_sig_atomic_t" = x"yes"; then
   AC_DEFINE(HAVE_SIG_ATOMIC_T_TYPE,1,[Whether we have the atomic_t variable type])
fi


AC_CACHE_CHECK([for O_DIRECT flag to open(2)],samba_cv_HAVE_OPEN_O_DIRECT,[
AC_TRY_COMPILE([
#include <unistd.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif],
[int fd = open("/dev/null", O_DIRECT);],
samba_cv_HAVE_OPEN_O_DIRECT=yes,samba_cv_HAVE_OPEN_O_DIRECT=no)])
if test x"$samba_cv_HAVE_OPEN_O_DIRECT" = x"yes"; then
    AC_DEFINE(HAVE_OPEN_O_DIRECT,1,[Whether the open(2) accepts O_DIRECT])
fi 


AC_CACHE_CHECK([that the C compiler can precompile header files],samba_cv_precompiled_headers, [
	dnl Check whether the compiler can generate precompiled headers
	touch conftest.h
	if ${CC-cc} conftest.h 2> /dev/null && test -f conftest.h.gch; then
		precompiled_headers=yes
	else
		precompiled_headers=no
	fi])
AC_SUBST(precompiled_headers)


dnl Check if the C compiler understands volatile (it should, being ANSI).
AC_CACHE_CHECK([that the C compiler understands volatile],samba_cv_volatile, [
	AC_TRY_COMPILE([#include <sys/types.h>],[volatile int i = 0],
		samba_cv_volatile=yes,samba_cv_volatile=no)])
if test x"$samba_cv_volatile" = x"yes"; then
	AC_DEFINE(HAVE_VOLATILE, 1, [Whether the C compiler understands volatile])
fi

m4_include(system/config.m4)

m4_include(dlfcn.m4)
m4_include(getpass.m4)
m4_include(strptime.m4)
m4_include(win32.m4)
m4_include(timegm.m4)
m4_include(repdir.m4)

AC_CHECK_FUNCS([syslog memset memcpy],,[AC_MSG_ERROR([Required function not found])])

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
])

m4_include(libreplace_cc.m4)
m4_include(libreplace_macros.m4)
m4_include(autoconf-2.60.m4)
