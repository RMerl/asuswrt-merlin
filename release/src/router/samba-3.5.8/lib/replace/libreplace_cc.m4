
AC_DEFUN_ONCE(AC__LIBREPLACE_ONLY_CC_CHECKS_START,
[
echo "LIBREPLACE_CC_CHECKS: START"
])

AC_DEFUN_ONCE(AC__LIBREPLACE_ONLY_CC_CHECKS_END,
[
echo "LIBREPLACE_CC_CHECKS: END"
])

dnl
dnl
dnl AC_LIBREPLACE_CC_CHECKS
dnl
dnl Note: we need to use m4_define instead of AC_DEFUN because
dnl       of the ordering of tests
dnl       
dnl 
m4_define(AC_LIBREPLACE_CC_CHECKS,
[
AC__LIBREPLACE_ONLY_CC_CHECKS_START

dnl stop the C89 attempt by autoconf - if autoconf detects -Ae it will enable it
dnl which conflicts with C99 on HPUX
ac_cv_prog_cc_Ae=no

savedCFLAGS=$CFLAGS
AC_PROG_CC
CFLAGS=$savedCFLAGS

dnl don't try for C99 if we are using gcc, as otherwise we 
dnl lose immediate structure constants
if test x"$GCC" != x"yes" ; then
AC_PROG_CC_C99
fi

if test x"$GCC" = x"yes" ; then
	AC_MSG_CHECKING([for version of gcc])
	GCC_VERSION=`$CC -dumpversion`
	AC_MSG_RESULT(${GCC_VERSION})
fi
AC_USE_SYSTEM_EXTENSIONS
AC_C_BIGENDIAN
AC_C_INLINE
LIBREPLACE_C99_STRUCT_INIT([],[AC_MSG_WARN([c99 structure initializer are not supported])])

AC_PROG_INSTALL

AC_ISC_POSIX
AC_N_DEFINE(_XOPEN_SOURCE_EXTENDED)

AC_SYS_LARGEFILE

dnl Add #include for broken IRIX header files
case "$host_os" in
	*irix6*) AC_ADD_INCLUDE(<standards.h>)
		;;
	*hpux*)
		# mmap on HPUX is completely broken...
		AC_DEFINE(MMAP_BLACKLIST, 1, [Whether MMAP is broken])
		if test "`uname -r`" = "B.11.00" -o "`uname -r`" = "B.11.11"; then
			AC_MSG_WARN([Enabling HPUX 11.00/11.11 header bug workaround])
			CFLAGS="$CFLAGS -Dpread=pread64 -Dpwrite=pwrite64"
		fi
		if test "`uname -r`" = "B.11.23"; then
			AC_MSG_WARN([Enabling HPUX 11.23 machine/sys/getppdp.h bug workaround])
			CFLAGS="$CFLAGS -D_MACHINE_SYS_GETPPDP_INCLUDED"
		fi
		;;
	*aix*)
		AC_DEFINE(BROKEN_STRNDUP, 1, [Whether strndup is broken])
		AC_DEFINE(BROKEN_STRNLEN, 1, [Whether strnlen is broken])
		if test "${GCC}" != "yes"; then
			## for funky AIX compiler using strncpy()
			CFLAGS="$CFLAGS -D_LINUX_SOURCE_COMPAT -qmaxmem=32000"
		fi
		;;
	*osf*)
		# this brings in socklen_t
		AC_N_DEFINE(_XOPEN_SOURCE,600)
		AC_N_DEFINE(_OSF_SOURCE)
		;;
	#
	# VOS may need to have POSIX support and System V compatibility enabled.
	#
	*vos*)
		case "$CFLAGS" in
			*-D_POSIX_C_SOURCE*);;
			*)
				CFLAGS="$CFLAGS -D_POSIX_C_SOURCE=200112L"
				AC_DEFINE(_POSIX_C_SOURCE, 200112L, [Whether to enable POSIX support])
				;;
		esac
		case "$CFLAGS" in
			*-D_SYSV*|*-D_SVID_SOURCE*);;
			*)
				CFLAGS="$CFLAGS -D_SYSV"
				AC_DEFINE(_SYSV, 1, [Whether to enable System V compatibility])
				;;
		esac
		;;
esac



AC_CHECK_HEADERS([standards.h])

# Solaris needs HAVE_LONG_LONG defined
AC_CHECK_TYPES(long long)

AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)

AC_CHECK_TYPE(uint_t, unsigned int)
AC_CHECK_TYPE(int8_t, char)
AC_CHECK_TYPE(uint8_t, unsigned char)
AC_CHECK_TYPE(int16_t, short)
AC_CHECK_TYPE(uint16_t, unsigned short)

if test $ac_cv_sizeof_int -eq 4 ; then
AC_CHECK_TYPE(int32_t, int)
AC_CHECK_TYPE(uint32_t, unsigned int)
elif test $ac_cv_size_long -eq 4 ; then
AC_CHECK_TYPE(int32_t, long)
AC_CHECK_TYPE(uint32_t, unsigned long)
else
AC_MSG_ERROR([LIBREPLACE no 32-bit type found])
fi

AC_CHECK_TYPE(int64_t, long long)
AC_CHECK_TYPE(uint64_t, unsigned long long)

AC_CHECK_TYPE(size_t, unsigned int)
AC_CHECK_TYPE(ssize_t, int)

AC_CHECK_SIZEOF(off_t)
AC_CHECK_SIZEOF(size_t)
AC_CHECK_SIZEOF(ssize_t)

AC_CHECK_TYPES([intptr_t, uintptr_t, ptrdiff_t])

if test x"$ac_cv_type_long_long" != x"yes";then
	AC_MSG_ERROR([LIBREPLACE needs type 'long long'])
fi
if test $ac_cv_sizeof_long_long -lt 8;then
	AC_MSG_ERROR([LIBREPLACE needs sizeof(long long) >= 8])
fi

############################################
# check if the compiler can do immediate structures
AC_SUBST(libreplace_cv_immediate_structures)
AC_CACHE_CHECK([for immediate structures],libreplace_cv_immediate_structures,[
	AC_TRY_COMPILE([
		#include <stdio.h>
	],[
		typedef struct {unsigned x;} FOOBAR;
		#define X_FOOBAR(x) ((FOOBAR) { x })
		#define FOO_ONE X_FOOBAR(1)
		FOOBAR f = FOO_ONE;   
		static const struct {
			FOOBAR y; 
		} f2[] = {
			{FOO_ONE}
		};
		static const FOOBAR f3[] = {FOO_ONE};
	],
	libreplace_cv_immediate_structures=yes,
	libreplace_cv_immediate_structures=no,
	libreplace_cv_immediate_structures=cross)
])
if test x"$libreplace_cv_immediate_structures" = x"yes"; then
	AC_DEFINE(HAVE_IMMEDIATE_STRUCTURES,1,[Whether the compiler supports immediate structures])
fi

AC__LIBREPLACE_ONLY_CC_CHECKS_END
]) dnl end AC_LIBREPLACE_CC_CHECKS
