dnl Process this file with autoconf to produce a configure script.

dnl we need at least autoconf 2.52
AC_PREREQ(2.52)

AC_INIT(README)
AM_CONFIG_HEADER(config.h)
AC_CANONICAL_HOST

PACKAGE=wol
MAJOR=0
MINOR=7
MICRO=1
PATCH=
VERSION=$MAJOR.$MINOR.$MICRO$PATCH
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE")
AC_DEFINE_UNQUOTED(VERSION, "$VERSION")
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)

dnl ready for automake
AM_INIT_AUTOMAKE($PACKAGE, $VERSION)


dnl Enable extensions in standard headers
dnl gl_USE_SYSTEM_EXTENSIONS must be added quite early to configure.ac.
gl_USE_SYSTEM_EXTENSIONS


dnl check for programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_AWK
AC_PATH_PROG(SED, sed, "")

do_pod2texi="no"
AC_PATH_PROG(PERL, perl, "")
if test x$PERL = x ; then
  AC_MSG_WARN([*** Unable to find perl. Disabling manpage creation ***])
else
  AC_PATH_PROG(P2M, pod2man, "")
  if test x$P2M = x ; then
     AC_MSG_WARN([*** Unable to find pod2man. Disabling manpage creation ***])
  else
     do_pod2texi="yes"
  fi
fi
AM_CONDITIONAL([POD2TEXI], [test x$do_pod2texi = xyes])



dnl check compiler characteristic
AC_C_CONST
AC_C_BIGENDIAN


dnl check data types
AC_CHECK_SIZEOF(unsigned char, 1)

ether_includes=""
AC_CHECK_TYPE([struct ether_addr], , , [#include <netinet/ether.h>]) dnl Linux
if test "$ac_cv_type_struct_ether_addr" = "yes"; then
	AC_DEFINE_UNQUOTED(HAVE_STRUCT_ETHER_ADDR, 1)
	ether_includes="#include <netinet/ether.h>"
else dnl BSD
	AC_CHECK_TYPE([struct ether_addr], , , [#include <net/ethernet.h>])
	if test "$ac_cv_type_struct_ether_addr" = "yes"; then
		AC_DEFINE_UNQUOTED(HAVE_STRUCT_ETHER_ADDR, 1)
		ether_includes="#include <net/ethernet.h>"
	else dnl Solaris
		ac_cv_type_struct_ether_addr="no"
		AC_MSG_CHECKING([for struct ether_addr])
		AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>],
				[struct ether_addr ea;],
				[ac_cv_type_struct_ether_addr="yes"], [])
		AC_MSG_RESULT($ac_cv_type_struct_ether_addr)
		if test "$ac_cv_type_struct_ether_addr" = "yes"; then
			AC_DEFINE_UNQUOTED(HAVE_STRUCT_ETHER_ADDR, 1)
			ether_includes="#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>"
		else
			AC_MSG_WARN([*** Unable to find struct ether_addr. Disabling ethernet name mapping.***])
		fi
	fi
fi

dnl check struct members
if test "$ac_cv_type_struct_ether_addr" = "yes"; then
	AC_CHECK_MEMBER([struct ether_addr.octet], , , [$ether_includes])
	if test "$ac_cv_member_struct_ether_addr_octet" = "yes"; then
		AC_DEFINE_UNQUOTED(HAVE_STRUCT_ETHER_ADDR_OCTET, 1)
	else
		AC_CHECK_MEMBER([struct ether_addr.ether_addr_octet], , , [$ether_includes])
		if test "$ac_cv_member_struct_ether_addr_ether_addr_octet" = "yes"; then
			AC_DEFINE_UNQUOTED(HAVE_STRUCT_ETHER_ADDR_ETHER_ADDR_OCTET, 1)
		else
			AC_MSG_ERROR([*** Found struct ether_addr, but no usable octet member... ***])
		fi
	fi
fi



dnl i18n
ALL_LINGUAS="de fr it es sv"
dnl don't pass symbol [external] since it will disable local intl directory
AM_GNU_GETTEXT


dnl
dnl check header files
dnl

AC_HEADER_STDC
AC_HEADER_TIME
AC_CHECK_HEADERS([stdio.h unistd.h getopt.h locale.h libintl.h])


dnl
dnl check ethernet definitions
dnl

AC_CHECK_HEADERS([netinet/ether.h net/ethernet.h netinet/in.h])


dnl `net/if.h'
dnl     On Darwin, this file requires that `stdio.h' and `sys/socket.h' be
dnl     included beforehand.  One should run:

AC_CHECK_HEADERS([sys/socket.h])
AC_CHECK_HEADERS([net/if.h], [], [],
                 [#include <stdio.h>
                  #if STDC_HEADERS
                  # include <stdlib.h>
                  # include <stddef.h>
                  #else
                  # if HAVE_STDLIB_H
                  #  include <stdlib.h>
                  # endif
                  #endif
                  #if HAVE_SYS_SOCKET_H
                  # include <sys/socket.h>
                  #endif
                 ])

dnl `netinet/if_ether.h'
dnl     On Darwin, this file requires that `stdio.h' and `sys/socket.h' be
dnl     included beforehand.  One should run:

AC_CHECK_HEADERS([sys/socket.h])
AC_CHECK_HEADERS([netinet/if_ether.h], [], [],
                 [#include <stdio.h>
                  #if STDC_HEADERS
                  # include <stdlib.h>
                  # include <stddef.h>
                  #else
                  # if HAVE_STDLIB_H
                  #  include <stdlib.h>
                  # endif
                  #endif
                  #if HAVE_SYS_SOCKET_H
                  # include <sys/socket.h>
                  #endif
                  #if HAVE_NET_IF_H
                  # include <net/if.h>
                  #endif
                  #if HAVE_NETINET_IN_H
                  # include <netinet/in.h>
                  #endif
                 ])


dnl check system functions
gl_GETOPT dnl for GNU getopt
jm_PREREQ_ERROR dnl for GNU error
jm_FUNC_MALLOC	dnl for GNU xmalloc
jm_FUNC_REALLOC	dnl for GNU xmalloc
jm_FUNC_GLIBC_UNLOCKED_IO dnl for GNU unlocked-io
gl_ERROR dnl for GNU error
gl_MD5 dnl for GNU md5

AM_FUNC_GETLINE dnl for GNU getline
AC_CHECK_FUNCS(usleep)
AC_CHECK_FUNCS(getopt_long) dnl for GNU getopt
AC_CHECK_FUNCS(getdelim)
AC_CHECK_FUNC(inet_aton, [], [
	dnl check libresolv for inet_aton() as seen on solaris
	AC_CHECK_LIB(resolv, inet_aton,
								EXTRA_LIBS="$EXTRA_LIBS -lresolv", EXTRA_LIBS="$EXTRA_LIBS")
])
AC_CHECK_FUNC(socket, [], [
	dnl check libsocket for socket() as seen on solaris
	AC_CHECK_LIB(socket, socket,
								EXTRA_LIBS="$EXTRA_LIBS -lsocket", EXTRA_LIBS="$EXTRA_LIBS")
])
AC_CHECK_FUNC(gethostbyname, [], [
  dnl check libnsl for gethostbyname() as seen on solaris
	AC_CHECK_LIB(nsl, gethostbyname,
	              EXTRA_LIBS="$EXTRA_LIBS -lnsl", EXTRA_LIBS="$EXTRA_LIBS")
])

ether_hostton="no"
AC_CHECK_FUNC(ether_hostton, [ether_hostton="yes"], [
	dnl check libsocket for ether_hostton() as seen on solaris
	AC_CHECK_LIB(socket, ether_hostton,
								EXTRA_LIBS="$(echo $EXTRA_LIBS | $SED s/-lsocket//g) -lsocket"
								ether_hostton="yes",
								EXTRA_LIBS="$EXTRA_LIBS")
])
AC_SUBST(EXTRA_LIBS)
if test x$ether_hostton = xyes; then
	AC_DEFINE_UNQUOTED(HAVE_ETHER_HOSTTON, 1)
fi


dnl arguments
AC_MSG_CHECKING(wheter you want ANSI emulation)
AC_ARG_ENABLE(ansi,
[  --enable-ansi           enables strict ANSI checking [default=no]],
with_ansi=$enableval, with_ansi=no)
test "$with_ansi" = "yes" && CFLAGS="$CFLAGS -ansi -pedantic"
AC_MSG_RESULT($with_ansi)

dnl check for python
dnl AC_PATH_PROG(PY, python, "")
dnl if test x$PY = x ; then
dnl 	AC_MSG_WARN([
dnl *** Unable to find python.
dnl *** Because of that, you won't be able to run certain tests via 'make check'.])
dnl fi


dnl config.h.in defines
AC_DEFINE([HAVE_STRUCT_ETHER_ADDR], 0, [struct ether_addr])
AC_DEFINE([HAVE_STRUCT_ETHER_ADDR_ETHER_ADDR_OCTET], 0, [struct ether_addr.ether_addr_octet])
AC_DEFINE([HAVE_STRUCT_ETHER_ADDR_OCTET], 0, [struct ether_addr.octet])
AC_DEFINE([HAVE_ETHER_HOSTTON], 0, [ether_hostton])


dnl full featured warnings
CFLAGS="$CFLAGS -W -Wall -Wpointer-arith -Wimplicit-function-declaration"


dnl create the Makefiles
AC_OUTPUT([intl/Makefile po/Makefile.in
Makefile
demo/Makefile
doc/Makefile
include/Makefile
lib/Makefile
m4/Makefile
src/Makefile
src/wol-dhcpdconf
src/wol-bootptab], [chmod +x src/wol-dhcpdconf src/wol-bootptab])


dnl output configuration
echo "\

$PACKAGE $VERSION configuration:

	prefix: $prefix
	exec_prefix: $exec_prefix

	bindir: $bindir
	libdir: $libdir
	datadir: $datadir
	mandir: $mandir
	infodir: $infodir

	ether_hostton support: $ether_hostton
	generate wol manpage: $do_pod2texi

	EXTRA_LIBS: $EXTRA_LIBS
	ANSI emulation: $with_ansi
"
