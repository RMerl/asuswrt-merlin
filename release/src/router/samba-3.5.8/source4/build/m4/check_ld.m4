dnl SMB Build Environment LD Checks
dnl -------------------------------------------------------
dnl  Copyright (C) Stefan (metze) Metzmacher 2004
dnl  Copyright (C) Jelmer Vernooij 2006
dnl  Released under the GNU GPL
dnl -------------------------------------------------------
dnl

AC_PATH_PROG(PROG_LD,ld)
LD=${PROG_LD}
AC_PROG_LD_GNU
LD=""

AC_SUBST(BLDSHARED)
AC_SUBST(LD)
AC_SUBST(SYS_LDFLAGS)
AC_SUBST(LDFLAGS)

# Assume non-shared by default and override below
# these are the defaults, good for lots of systems
BLDSHARED="false"
LD="${CC}"
PICFLAG=""

# allow for --with-hostld=gcc
AC_ARG_WITH(hostld,[  --with-hostld=linker    choose host linker],
[HOSTLD=$withval],
[HOSTLD=$HOSTCC])

AC_MSG_CHECKING([whether to try to build shared libraries on $host_os])

# and these are for particular systems
case "$host_os" in
	*linux*)
		BLDSHARED="true"
		SYS_LDFLAGS="-Wl,--export-dynamic"
		;;
	*solaris*)
		BLDSHARED="true"
		if test "${GCC}" = "yes"; then
			if test "${ac_cv_prog_gnu_ld}" = "yes"; then
				SYS_LDFLAGS="-Wl,-E"
			fi
		fi
		;;
	*sunos*)
		BLDSHARED="true"
		;;
	*netbsd* | *freebsd* | *dragonfly* )  
		BLDSHARED="true"
		SYS_LDFLAGS="-Wl,--export-dynamic"
		;;
	*openbsd*)
		BLDSHARED="true"
		SYS_LDFLAGS="-Wl,-Bdynamic"
		;;
	*irix*)
		BLDSHARED="true"
		;;
	*aix*)
		BLDSHARED="true"
		SYS_LDFLAGS="-Wl,-brtl,-bexpall,-bbigtoc"
		;;
	*hpux*)
		# Use special PIC flags for the native HP-UX compiler.
		BLDSHARED="true" # I hope this is correct
		if test "$host_cpu" = "ia64"; then
			SYS_LDFLAGS="-Wl,-E,+b/usr/local/lib/hpux32:/usr/lib/hpux32"
		else
			SYS_LDFLAGS="-Wl,-E,+b/usr/local/lib:/usr/lib"
		fi
		;;
	*osf*)
		BLDSHARED="true"
		;;
	*unixware*)
		BLDSHARED="true"
		;;
	*darwin*)
		BLDSHARED="true"
		;;
esac

AC_MSG_RESULT($BLDSHARED)

AC_MSG_CHECKING([LD])
AC_MSG_RESULT([$LD])
AC_MSG_CHECKING([LDFLAGS])
AC_MSG_RESULT([$LDFLAGS])
AC_MSG_CHECKING([SYS_LDFLAGS])
AC_MSG_RESULT([$SYS_LDFLAGS])

AC_SUBST(HOSTLD)

AC_LIBREPLACE_STLD
AC_LIBREPLACE_STLD_FLAGS

AC_MSG_CHECKING([STLD])
AC_MSG_RESULT([$STLD])
AC_MSG_CHECKING([STLD_FLAGS])
AC_MSG_RESULT([$STLD_FLAGS])

AC_LD_PICFLAG
AC_LD_EXPORT_DYNAMIC
AC_LD_SHLIBEXT
AC_LD_SONAMEFLAG
AC_LIBREPLACE_SHLD
AC_LIBREPLACE_SHLD_FLAGS
AC_LIBREPLACE_MDLD
AC_LIBREPLACE_MDLD_FLAGS
AC_LIBREPLACE_RUNTIME_LIB_PATH_VAR

#######################################################
# test whether building a shared library actually works
if test $BLDSHARED = true; then

	AC_MSG_CHECKING([SHLD])
	AC_MSG_RESULT([$SHLD])
	AC_MSG_CHECKING([SHLD_FLAGS])
	AC_MSG_RESULT([$SHLD_FLAGS])

	AC_MSG_CHECKING([MDLD])
	AC_MSG_RESULT([$MDLD])
	AC_MSG_CHECKING([MDLD_FLAGS])
	AC_MSG_RESULT([$MDLD_FLAGS])

	AC_MSG_CHECKING([SHLIBEXT])
	AC_MSG_RESULT([$SHLIBEXT])
	AC_MSG_CHECKING([SONAMEFLAG])
	AC_MSG_RESULT([$SONAMEFLAG])

	AC_MSG_CHECKING([PICFLAG])
	AC_MSG_RESULT([$PICFLAG])

	AC_CACHE_CHECK([whether building shared libraries actually works], 
	               [ac_cv_shlib_works],[
			ac_cv_shlib_works=no
			# try building a trivial shared library
			# TODO: also test SONAMEFLAG
			${CC} ${CFLAGS} ${PICFLAG} -c ${srcdir-.}/../tests/shlib.c -o shlib.o &&
				${SHLD} ${SHLD_FLAGS} -o shlib.${SHLIBEXT} shlib.o && 
				ac_cv_shlib_works=yes
			rm -f shlib.${SHLIBEXT} shlib.o
	])
	AC_CACHE_CHECK([whether building shared modules actually works], 
	               [ac_cv_shmod_works],[
			ac_cv_shmod_works=no
			# try building a trivial shared library
			${CC} ${CFLAGS} ${PICFLAG} -c ${srcdir-.}/../tests/shlib.c -o shlib.o &&
				${MDLD} ${MDLD_FLAGS} -o shlib.${SHLIBEXT} shlib.o && 
				ac_cv_shmod_works=yes
			rm -f shlib.${SHLIBEXT} shlib.o
	])
	if test $ac_cv_shlib_works = no; then
		AC_MSG_ERROR(unable to build shared libraries)
	fi
	if test $ac_cv_shmod_works = no; then
		AC_MSG_ERROR(unable to build shared modules)
	fi
fi

AC_DEFINE_UNQUOTED(SHLIBEXT, "$SHLIBEXT", [Shared library extension])

AC_MSG_CHECKING([if we can link using the selected flags])
AC_TRY_RUN([#include "${srcdir-.}/../tests/trivial.c"],
	    AC_MSG_RESULT(yes),
	    AC_MSG_ERROR([we cannot link with the selected cc and ld flags. Aborting configure]),
	    AC_MSG_WARN([cannot run when cross-compiling]))


USESHARED=false
AC_SUBST(USESHARED)

AC_ARG_ENABLE(dso,
[  --enable-dso 	          Enable using shared libraries internally (experimental)],
[],[enable_dso=no])

if test x"$enable_dso" = x"yes" -a x"$BLDSHARED" != x"true"; then
	AC_MSG_ERROR([--enable-dso: no support for shared libraries])
fi

if test x"$enable_dso" != x"no"; then
	USESHARED=$BLDSHARED
fi

AC_MSG_CHECKING([if binaries will use shared libraries])
AC_MSG_RESULT([$USESHARED])
