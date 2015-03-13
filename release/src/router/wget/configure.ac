dnl Template file for GNU Autoconf
dnl Copyright (C) 1995, 1996, 1997, 2001, 2007, 2008, 2009, 2010, 2011, 2012,
dnl 2013, 2014 Free Software Foundation, Inc.

dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 3 of the License, or
dnl (at your option) any later version.

dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.

dnl You should have received a copy of the GNU General Public License
dnl along with this program.  If not, see <http://www.gnu.org/licenses/>.

dnl Additional permission under GNU GPL version 3 section 7

dnl If you modify this program, or any covered work, by linking or
dnl combining it with the OpenSSL project's OpenSSL library (or a
dnl modified version of that library), containing parts covered by the
dnl terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
dnl grants you additional permission to convey the resulting work.
dnl Corresponding Source for a non-source form of such a combination
dnl shall include the source code for the parts of OpenSSL used as well
dnl as that of the covered work.

dnl
dnl Process this file with autoconf to produce a configure script.
dnl

AC_INIT([wget],
        m4_esyscmd([build-aux/git-version-gen .tarball-version]),
	[bug-wget@gnu.org])
AC_PREREQ(2.61)

dnl
dnl What version of Wget are we building?
dnl
AC_MSG_NOTICE([configuring for GNU Wget $PACKAGE_VERSION])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AC_CONFIG_SRCDIR([src/wget.h])

dnl
dnl Automake setup
dnl
AM_INIT_AUTOMAKE([1.9])

dnl
dnl Get cannonical host
dnl
AC_CANONICAL_HOST
AC_DEFINE_UNQUOTED([OS_TYPE], "$host_os",
                   [Define to be the name of the operating system.])

dnl
dnl Process features.
dnl

ENABLE_PSL=no
AC_ARG_WITH(libpsl,
    AS_HELP_STRING([--without-libpsl],
                   [disable support for libpsl cookie checking.]),
    [],
    [AC_SEARCH_LIBS(psl_builtin, psl,
                    [ENABLE_PSL=yes; AC_DEFINE([HAVE_LIBPSL], [1], [PSL Support Enabled])],
                    [AC_MSG_WARN(*** libpsl not found. Falling back to Wget builtin cookie checking.)])
    ])

AC_ARG_WITH(ssl,
[[  --without-ssl           disable SSL autodetection
  --with-ssl={gnutls,openssl} specify the SSL backend.  GNU TLS is the default.]])

AC_ARG_WITH(zlib,
[[  --without-zlib          disable zlib ]])

AC_ARG_ENABLE(opie,
[  --disable-opie          disable support for opie or s/key FTP login],
ENABLE_OPIE=$enableval, ENABLE_OPIE=yes)
test x"${ENABLE_OPIE}" = xyes && AC_DEFINE([ENABLE_OPIE], 1,
   [Define if you want the Opie support for FTP compiled in.])

AC_ARG_ENABLE(digest,
[  --disable-digest        disable support for HTTP digest authorization],
ENABLE_DIGEST=$enableval, ENABLE_DIGEST=yes)
test x"${ENABLE_DIGEST}" = xyes && AC_DEFINE([ENABLE_DIGEST], 1,
   [Define if you want the HTTP Digest Authorization compiled in.])

AC_ARG_ENABLE(ntlm,
[  --disable-ntlm          disable support for NTLM authorization],
[ENABLE_NTLM=$enableval], [ENABLE_NTLM=auto])

AC_ARG_ENABLE(debug,
[  --disable-debug         disable support for debugging output],
ENABLE_DEBUG=$enableval, ENABLE_DEBUG=yes)
test x"${ENABLE_DEBUG}" = xyes && AC_DEFINE([ENABLE_DEBUG], 1,
   [Define if you want the debug output support compiled in.])

dnl
dnl Find the compiler
dnl

dnl We want these before the checks, so the checks can modify their values.
test -z "$CFLAGS"  && CFLAGS= auto_cflags=1
test -z "$CC" && cc_specified=yes

AC_PROG_CC
AM_PROG_CC_C_O
AC_AIX

gl_EARLY

dnl
dnl Gettext
dnl
AM_GNU_GETTEXT([external],[need-ngettext])
AM_GNU_GETTEXT_VERSION([0.17])

AC_PROG_RANLIB

AC_PROG_LEX

dnl Turn on optimization by default.  Specifically:
dnl
dnl if the user hasn't specified CFLAGS, then
dnl   if compiler is gcc, then
dnl     use -O2 and some warning flags
dnl   else
dnl     use os-specific flags or -O
if test -n "$auto_cflags"; then
  if test -n "$GCC"; then
    CFLAGS="$CFLAGS -O2 -Wall"
  else
    case "$host_os" in
      *hpux*)  CFLAGS="$CFLAGS +O3"                      ;;
      *ultrix* | *osf*) CFLAGS="$CFLAGS -O -Olimit 2000" ;;
      *)       CFLAGS="$CFLAGS -O" ;;
    esac
  fi
fi

dnl
dnl Checks for basic compiler characteristics.
dnl
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE

dnl Check for basic headers, even though we expect them to exist and
dnl #include them unconditionally in the code.  Their detection is
dnl still needed because test programs used by Autoconf macros check.
dnl Without the checks they will fail to be included in test programs,
dnl which will subsequently fail.
AC_HEADER_STDC

dnl Check for large file support.  This check needs to come fairly
dnl early because it could (in principle) affect whether functions and
dnl headers are available, whether they work, etc.
AC_SYS_LARGEFILE
AC_CHECK_SIZEOF(off_t)

dnl
dnl Checks for system header files that might be missing.
dnl
AC_HEADER_STDBOOL
AC_CHECK_HEADERS(unistd.h sys/time.h)
AC_CHECK_HEADERS(termios.h sys/ioctl.h sys/select.h utime.h sys/utime.h)
AC_CHECK_HEADERS(stdint.h inttypes.h pwd.h wchar.h)

AC_CHECK_DECLS(h_errno,,,[#include <netdb.h>])

dnl
dnl Check sizes of integer types.  These are used to find n-bit
dnl integral types on older systems that fail to provide intN_t and
dnl uintN_t typedefs.
dnl
AC_CHECK_SIZEOF([short])
AC_CHECK_SIZEOF([int])
AC_CHECK_SIZEOF([long])
AC_CHECK_SIZEOF([long long])
AC_CHECK_SIZEOF([void *])

dnl
dnl Checks for non-universal or system-specific types.
dnl
AC_TYPE_SIZE_T
AC_TYPE_PID_T
AC_CHECK_TYPES([uint32_t, uintptr_t, intptr_t, int64_t])
AC_CHECK_TYPES(sig_atomic_t, [], [], [
#include <stdio.h>
#include <sys/types.h>
#include <inttypes.h>
#include <signal.h>
])

# gnulib
gl_INIT

dnl
dnl Checks for library functions.
dnl
AC_FUNC_MMAP
AC_FUNC_FSEEKO
AC_CHECK_FUNCS(strptime timegm vsnprintf vasprintf drand48 pathconf)
AC_CHECK_FUNCS(strtoll usleep ftello sigblock sigsetjmp memrchr wcwidth mbtowc)
AC_CHECK_FUNCS(sleep symlink utime)

if test x"$ENABLE_OPIE" = xyes; then
  AC_LIBOBJ([ftp-opie])
fi

dnl We expect to have these functions on Unix-like systems configure
dnl runs on.  The defines are provided to get them in config.h.in so
dnl Wget can still be ported to non-Unix systems (such as Windows)
dnl that lack some of these functions.
AC_DEFINE([HAVE_STRCASECMP], 1, [Define to 1 if you have the `strcasecmp' function.])
AC_DEFINE([HAVE_STRNCASECMP], 1, [Define to 1 if you have the `strncasecmp' function.])
AC_DEFINE([HAVE_STRDUP], 1, [Define to 1 if you have the `strdup' function.])
AC_DEFINE([HAVE_ISATTY], 1, [Define to 1 if you have the `isatty' function.])

dnl
dnl Call Wget-specific macros defined in aclocal.
dnl
WGET_STRUCT_UTIMBUF
WGET_FNMATCH
WGET_NANOSLEEP
WGET_POSIX_CLOCK
WGET_NSL_SOCKET

dnl Deal with specific hosts
case $host_os in
  *mingw32* )
    LIBS+=' -lws2_32'
    AC_LIBOBJ([mswindows])
    ;;
esac


dnl
dnl Checks for libraries.
dnl

AS_IF([test x"$with_zlib" != xno], [
  with_zlib=yes
  AC_CHECK_LIB(z, compress)
])

AS_IF([test x"$with_ssl" = xopenssl], [
  dnl As of this writing (OpenSSL 0.9.6), the libcrypto shared library
  dnl doesn't record its dependency on libdl, so we need to make sure
  dnl -ldl ends up in LIBS on systems that have it.  Most OSes use
  dnl dlopen(), but HP-UX uses shl_load().
  AC_CHECK_LIB(dl, dlopen, [], [
    AC_CHECK_LIB(dl, shl_load)
  ])

  ssl_found=no
  case $host_os in
    *mingw32* )
      dnl prefer link to openssl dlls if possible. if not then fallback on static libs. if not then error

      AC_CHECK_LIB(eay32, EVP_MD_CTX_init)
      if test x"$ac_cv_lib_eay32_EVP_MD_CTX_init" != xno
      then
        AC_CHECK_LIB(ssl32, SSL_connect, [
          ssl_found=yes
          AC_MSG_NOTICE([Enabling support for SSL via OpenSSL (shared)])
          AC_LIBOBJ([openssl])
          LIBS="${LIBS} -lssl32"
          AC_DEFINE([HAVE_LIBSSL32], [1], [Define to 1 if you have the `ssl32' library (-lssl32).])
        ],
        AC_MSG_ERROR([openssl not found: shared lib eay32 found but ssl32 not found]))

      else
        LIBS+=' -lgdi32'
        dnl fallback and test static libs
      fi
      dnl add zdll lib as dep for above tests?
    ;;
  esac

  AS_IF([test x$ssl_found != xyes], [
    dnl Now actually check for -lssl if it wasn't already found
    AC_LIB_HAVE_LINKFLAGS([ssl], [crypto], [
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/des.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
    ], [SSL_library_init ()])
    if test x"$LIBSSL" != x
    then
      ssl_found=yes
      AC_MSG_NOTICE([compiling in support for SSL via OpenSSL])
      AC_LIBOBJ([openssl])
      LIBS="$LIBSSL $LIBS"
    elif test x"$with_ssl" != x
    then
      AC_MSG_ERROR([--with-ssl=openssl was given, but SSL is not available.])
    fi
  ])

], [
  # --with-ssl is not openssl: check if it's no
  AS_IF([test x"$with_ssl" != xno], [
    dnl default is -lgnutls
    with_ssl=gnutls

    dnl Now actually check for -lgnutls
    AC_LIB_HAVE_LINKFLAGS([gnutls], [], [
#include <gnutls/gnutls.h>
    ], [gnutls_global_init()])
    if test x"$LIBGNUTLS" != x
    then
      ssl_found=yes
      AC_MSG_NOTICE([compiling in support for SSL via GnuTLS])
      AC_LIBOBJ([gnutls])
      LIBS="$LIBGNUTLS $LIBS"
    else
      AC_MSG_ERROR([--with-ssl=gnutls was given, but GNUTLS is not available.])
    fi

    AC_CHECK_FUNCS(gnutls_priority_set_direct)
  ]) # endif: --with-ssl != no?
]) # endif: --with-ssl == openssl?

dnl Enable NTLM if requested and if SSL is available.
if test x"$LIBSSL" != x || test "$ac_cv_lib_ssl32_SSL_connect" = yes
then
  if test x"$ENABLE_NTLM" != xno
  then
    ENABLE_NTLM=yes
    AC_DEFINE([ENABLE_NTLM], 1,
     [Define if you want the NTLM authorization support compiled in.])
    AC_LIBOBJ([http-ntlm])
  fi
else
  AC_CHECK_LIB(nettle, nettle_md4_init, [HAVE_NETTLE=yes], [HAVE_NETTLE=no; AC_MSG_WARN(*** libnettle was not found. You will not be able to use NTLM)])

  if test x"$HAVE_NETTLE" = xyes
  then
    AC_SUBST(NETTLE_LIBS, "-lnettle")
    AC_DEFINE([HAVE_NETTLE], [1], [Use libnettle])
    if test x"$ENABLE_NTLM" != xno
    then
      ENABLE_NTLM=yes
      AC_DEFINE([ENABLE_NTLM], 1,
       [Define if you want the NTLM authorization support compiled in.])
      AC_LIBOBJ([http-ntlm])
      LIBS="$NETTLE_LIBS $LIBS"
    fi
  else
    dnl If SSL is unavailable and the user explicitly requested NTLM,
    dnl abort.
    if test x"$ENABLE_NTLM" = xyes
    then
      AC_MSG_ERROR([NTLM authorization requested and SSL not enabled; aborting])
    fi
  fi
fi


dnl **********************************************************************
dnl Checks for IPv6
dnl **********************************************************************

dnl
dnl We test for IPv6 by checking, in turn, for availability of
dnl presence of the INET6 address/protocol family and the existence of
dnl struct sockaddr_in6.  If any of them is missing, IPv6 is disabled,
dnl and the code reverts to old-style gethostbyname.
dnl
dnl If --enable-ipv6 is explicitly specified on the configure command
dnl line, we check for IPv6 and abort if not found.  If --disable-ipv6
dnl is specified, we disable IPv6 and don't check for it.  The default
dnl is to autodetect IPv6 and use it where available.
dnl

AC_ARG_ENABLE(ipv6,
  AC_HELP_STRING([--disable-ipv6],[disable IPv6 support]),
  [case "${enable_ipv6}" in
    no)
      AC_MSG_NOTICE([disabling IPv6 at user request])
      dnl Disable IPv6 checking
      ipv6=no
      ;;
    yes)
      dnl IPv6 explicitly enabled: force its use (abort if unavailable).
      ipv6=yes
      force_ipv6=yes
      ;;
    auto)
      dnl Auto-detect IPv6, i.e. check for IPv6, but don't force it.
      ipv6=yes
      ;;
    *)
      AC_MSG_ERROR([Invalid --enable-ipv6 argument \`$enable_ipv6'])
      ;;
    esac
  ], [
    dnl If nothing is specified, assume auto-detection.
    ipv6=yes
  ]
)

if test "X$ipv6" = "Xyes"; then
  PROTO_INET6([], [
    AC_MSG_NOTICE([Disabling IPv6 support: your system does not support the PF_INET6 protocol family])
    ipv6=no
  ])
fi

if test "X$ipv6" = "Xyes"; then
  TYPE_STRUCT_SOCKADDR_IN6([],[
    AC_MSG_NOTICE([Disabling IPv6 support: your system does not support \`struct sockaddr_in6'])
    ipv6=no
  ])
  if test "X$ipv6" = "Xyes"; then
    WGET_STRUCT_SOCKADDR_STORAGE
    MEMBER_SIN6_SCOPE_ID
  fi
fi

if test "X$ipv6" = "Xyes"; then
  AC_DEFINE([ENABLE_IPV6], 1, [Define if IPv6 support is enabled.])
  AC_MSG_NOTICE([Enabling support for IPv6.])
elif test "x$force_ipv6" = "xyes"; then
  AC_MSG_ERROR([IPv6 support requested but not found; aborting])
fi

dnl
dnl Find makeinfo.  We used to provide support for Emacs processing
dnl Texinfo using `emacs -batch -eval ...' where makeinfo is
dnl unavailable, but that broke with the addition of makeinfo-specific
dnl command-line options, such as `-I'.  Now we depend on makeinfo to
dnl build the Info documentation.
dnl

AC_CHECK_PROGS(MAKEINFO, [makeinfo], [true])

dnl
dnl Find perl and pod2man
dnl

AC_PATH_PROGS(PERL, [perl5 perl], no)
AC_PATH_PROG(POD2MAN, pod2man, no)

if test "x${POD2MAN}" = xno; then
  COMMENT_IF_NO_POD2MAN="# "
else
  COMMENT_IF_NO_POD2MAN=
fi
AC_SUBST(COMMENT_IF_NO_POD2MAN)


dnl
dnl Check for IDN/IRIs
dnl

AC_ARG_ENABLE(iri,
  AC_HELP_STRING([--disable-iri],[disable IDN/IRIs support]),
  [case "${enable_iri}" in
    no)
      dnl Disable IRIs checking
      AC_MSG_NOTICE([disabling IRIs at user request])
      iri=no
      ;;
    yes)
      dnl IRIs explicitly enabled
      iri=yes
      force_iri=yes
      ;;
    auto)
      dnl Auto-detect IRI
      iri=yes
      ;;
    *)
      AC_MSG_ERROR([Invalid --enable-iri argument \`$enable_iri'])
      ;;
    esac
  ], [
    dnl If nothing is specified, assume auto-detection
    iri=yes
  ]
)

AC_ARG_WITH(libidn, AC_HELP_STRING([--with-libidn=[DIR]],
                                   [Support IDN/IRIs (needs GNU Libidn)]),
                                   libidn=$withval, libidn="")
AS_IF([test "X$iri" != "Xno"],[
  AM_ICONV

  if test "X$am_cv_func_iconv" != "Xyes"; then
    iri=no
    if test "X$force_iri" = "Xyes"; then
      AC_MSG_ERROR([Libiconv is required for IRIs support])
    else
      AC_MSG_NOTICE([disabling IRIs because libiconv wasn't found])
    fi
  fi
],[  # else
  # For some reason, this seems to be set even when we don't check.
  # Explicitly unset.
  LIBICONV=
])

if test "X$iri" != "Xno"; then
  if test "$libidn" != ""; then
    LDFLAGS="${LDFLAGS} -L$libidn/lib"
    CPPFLAGS="${CPPFLAGS} -I$libidn/include"
  fi

  # If idna.h can't be found, check to see if it was installed under
  # /usr/include/idn (OpenSolaris, at least, places it there).
  # Check for idn-int.h in that case, because idna.h won't find
  # idn-int.h until we've decided to add -I/usr/include/idn.
  AC_CHECK_HEADER(idna.h, ,
    [AC_CHECK_HEADER(idn/idn-int.h,
                    [CPPFLAGS="${CPPFLAGS} -I/usr/include/idn"],
                    [iri=no])]
  )

  if test "X$iri" != "Xno"; then
    AC_CHECK_LIB(idn, stringprep_check_version,
      [iri=yes LIBS="${LIBS} -lidn"], iri=no)
  fi

  if test "X$iri" != "Xno" ; then
    AC_DEFINE([ENABLE_IRI], 1, [Define if IRI support is enabled.])
    AC_MSG_NOTICE([Enabling support for IRI.])
  else
    AC_MSG_WARN([Libidn not found])
  fi
fi

dnl
dnl Check for UUID
dnl
AC_CHECK_HEADER(uuid.h,
                AC_CHECK_FUNC(uuid, uuid_create,
                  [AC_DEFINE([HAVE_UUID_CREATE], 1,
                             [Define if uuid_create is available.])
                  ])
)

AC_CHECK_HEADER(uuid/uuid.h,
                AC_CHECK_LIB(uuid, uuid_generate,
                  [LIBS="${LIBS} -luuid"
                   AC_DEFINE([HAVE_LIBUUID], 1,
                             [Define if libuuid is available.])
                  ])
)

dnl
dnl Check for PCRE
dnl

AC_CHECK_HEADER(pcre.h,
                AC_CHECK_LIB(pcre, pcre_compile,
                  [LIBS="${LIBS} -lpcre"
                   AC_DEFINE([HAVE_LIBPCRE], 1,
                             [Define if libpcre is available.])
                  ])
)


dnl Needed by src/Makefile.am
AM_CONDITIONAL([IRI_IS_ENABLED], [test "X$iri" != "Xno"])


dnl
dnl Create output
dnl
AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile util/Makefile
                 po/Makefile.in tests/Makefile
                 lib/Makefile testenv/Makefile])
AC_CONFIG_HEADERS([src/config.h])
AC_OUTPUT

AC_MSG_NOTICE([Summary of build options:

  Version:           $PACKAGE_VERSION
  Host OS:           $host_os
  Install prefix:    $prefix
  Compiler:          $CC
  CFlags:            $CFLAGS $CPPFLAGS
  LDFlags:           $LDFLAGS
  Libs:              $LIBS
  SSL:               $with_ssl
  Zlib:              $with_zlib
  PSL:               $ENABLE_PSL
  Digest:            $ENABLE_DIGEST
  NTLM:              $ENABLE_NTLM
  OPIE:              $ENABLE_OPIE
  Debugging:         $ENABLE_DEBUG
])
