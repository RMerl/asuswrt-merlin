dnl Copyright (c) 2001-2004, Roger Dingledine
dnl Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson
dnl Copyright (c) 2007-2013, The Tor Project, Inc.
dnl See LICENSE for licensing information

AC_INIT([tor],[0.2.5.10])
AC_CONFIG_SRCDIR([src/or/main.c])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CONFIG_HEADERS([orconfig.h])

AC_CANONICAL_HOST

if test -f /etc/redhat-release ; then
  if test -f /usr/kerberos/include ; then
    CPPFLAGS="$CPPFLAGS -I/usr/kerberos/include"
  fi
fi

# Not a no-op; we want to make sure that CPPFLAGS is set before we use
# the += operator on it in src/or/Makefile.am
CPPFLAGS="$CPPFLAGS -I\${top_srcdir}/src/common"

#XXXX020 We should make these enabled or not, before 0.2.0.x-final
AC_ARG_ENABLE(buf-freelists,
   AS_HELP_STRING(--enable-buf-freelists, enable freelists for buffer RAM))
AC_ARG_ENABLE(mempools,
   AS_HELP_STRING(--enable-mempools, enable mempools for relay cells))
AC_ARG_ENABLE(openbsd-malloc,
   AS_HELP_STRING(--enable-openbsd-malloc, Use malloc code from openbsd.  Linux only))
AC_ARG_ENABLE(instrument-downloads,
   AS_HELP_STRING(--enable-instrument-downloads, Instrument downloads of directory resources etc.))
AC_ARG_ENABLE(static-openssl,
   AS_HELP_STRING(--enable-static-openssl, Link against a static openssl library. Requires --with-openssl-dir))
AC_ARG_ENABLE(static-libevent,
   AS_HELP_STRING(--enable-static-libevent, Link against a static libevent library. Requires --with-libevent-dir))
AC_ARG_ENABLE(static-zlib,
   AS_HELP_STRING(--enable-static-zlib, Link against a static zlib library. Requires --with-zlib-dir))
AC_ARG_ENABLE(static-tor,
   AS_HELP_STRING(--enable-static-tor, Create an entirely static Tor binary. Requires --with-openssl-dir and --with-libevent-dir and --with-zlib-dir))
AC_ARG_ENABLE(curve25519,
   AS_HELP_STRING(--disable-curve25519, Build Tor with no curve25519 elliptic-curve crypto support))
AC_ARG_ENABLE(unittests,
   AS_HELP_STRING(--disable-unittests, [Don't build unit tests for Tor. Risky!]))
AC_ARG_ENABLE(coverage,
   AS_HELP_STRING(--enable-coverage, [Enable coverage support in the unit-test build]))

AM_CONDITIONAL(UNITTESTS_ENABLED, test x$enable_unittests != xno)
AM_CONDITIONAL(COVERAGE_ENABLED, test x$enable_coverage = xyes)

if test "$enable_static_tor" = "yes"; then
  enable_static_libevent="yes";
  enable_static_openssl="yes";
  enable_static_zlib="yes";
  CFLAGS="$CFLAGS -static"
fi

if test x$enable_buf_freelists = xyes; then
  AC_DEFINE(ENABLE_BUF_FREELISTS, 1,
            [Defined if we try to use freelists for buffer RAM chunks])
fi

AM_CONDITIONAL(USE_MEMPOOLS, test x$enable_mempools = xyes)
if test x$enable_mempools = xyes; then
  AC_DEFINE(ENABLE_MEMPOOLS, 1,
            [Defined if we try to use mempools for cells being relayed])
fi

AM_CONDITIONAL(USE_OPENBSD_MALLOC, test x$enable_openbsd_malloc = xyes)
if test x$enable_instrument_downloads = xyes; then
  AC_DEFINE(INSTRUMENT_DOWNLOADS, 1,
            [Defined if we want to keep track of how much of each kind of resource we download.])
fi

AC_ARG_ENABLE(transparent,
     AS_HELP_STRING(--disable-transparent, disable transparent proxy support),
     [case "${enableval}" in
        yes) transparent=true ;;
        no)  transparent=false ;;
        *) AC_MSG_ERROR(bad value for --enable-transparent) ;;
      esac], [transparent=true])

AC_ARG_ENABLE(asciidoc,
     AS_HELP_STRING(--disable-asciidoc, don't use asciidoc (disables building of manpages)),
     [case "${enableval}" in
        yes) asciidoc=true ;;
        no)  asciidoc=false ;;
        *) AC_MSG_ERROR(bad value for --disable-asciidoc) ;;
      esac], [asciidoc=true])

# By default, we're not ready to ship a NAT-PMP aware Tor
AC_ARG_ENABLE(nat-pmp,
     AS_HELP_STRING(--enable-nat-pmp, enable NAT-PMP support),
     [case "${enableval}" in
        yes) natpmp=true ;;
        no)  natpmp=false ;;
        * ) AC_MSG_ERROR(bad value for --enable-nat-pmp) ;;
      esac], [natpmp=false])

# By default, we're not ready to ship a UPnP aware Tor
AC_ARG_ENABLE(upnp,
     AS_HELP_STRING(--enable-upnp, enable UPnP support),
     [case "${enableval}" in
        yes) upnp=true ;;
        no)  upnp=false ;;
        * ) AC_MSG_ERROR(bad value for --enable-upnp) ;;
      esac], [upnp=false])


AC_ARG_ENABLE(threads,
     AS_HELP_STRING(--disable-threads, disable multi-threading support))

if test x$enable_threads = x; then
   case $host in
    *-*-solaris* )
     # Don't try multithreading on solaris -- cpuworkers seem to lock.
     AC_MSG_NOTICE([You are running Solaris; Sometimes threading makes
cpu workers lock up here, so I will disable threads.])
     enable_threads="no";;
    *)
     enable_threads="yes";;
   esac
fi

if test "$enable_threads" = "yes"; then
  AC_DEFINE(ENABLE_THREADS, 1, [Defined if we will try to use multithreading])
fi

case $host in
   *-*-solaris* )
     AC_DEFINE(_REENTRANT, 1, [Define on some platforms to activate x_r() functions in time.h])
     ;;
esac

AC_ARG_ENABLE(gcc-warnings,
     AS_HELP_STRING(--enable-gcc-warnings, enable verbose warnings))
AC_ARG_ENABLE(gcc-warnings-advisory,
     AS_HELP_STRING(--enable-gcc-warnings-advisory, [enable verbose warnings, excluding -Werror]))

dnl Others suggest '/gs /safeseh /nxcompat /dynamicbase' for non-gcc on Windows
AC_ARG_ENABLE(gcc-hardening,
    AS_HELP_STRING(--disable-gcc-hardening, disable compiler security checks))

AC_ARG_ENABLE(expensive-hardening,
    AS_HELP_STRING(--enable-expensive-hardening, enable more expensive compiler hardening; makes Tor slower))

dnl Linker hardening options
dnl Currently these options are ELF specific - you can't use this with MacOSX
AC_ARG_ENABLE(linker-hardening,
    AS_HELP_STRING(--disable-linker-hardening, disable linker security fixups))

AC_ARG_ENABLE(local-appdata,
   AS_HELP_STRING(--enable-local-appdata, default to host local application data paths on Windows))
if test "$enable_local_appdata" = "yes"; then
  AC_DEFINE(ENABLE_LOCAL_APPDATA, 1,
            [Defined if we default to host local appdata paths on Windows])
fi

# Tor2web mode flag
AC_ARG_ENABLE(tor2web-mode,
     AS_HELP_STRING(--enable-tor2web-mode, support tor2web non-anonymous mode),
[if test x$enableval = xyes; then
    CFLAGS="$CFLAGS -D ENABLE_TOR2WEB_MODE=1"
fi])

AC_ARG_ENABLE(bufferevents,
     AS_HELP_STRING(--enable-bufferevents, use Libevent's buffered IO.))

AC_ARG_ENABLE(tool-name-check,
     AS_HELP_STRING(--disable-tool-name-check, check for sanely named toolchain when cross-compiling))

AC_ARG_ENABLE(seccomp,
     AS_HELP_STRING(--disable-seccomp, do not attempt to use libseccomp))

dnl check for the correct "ar" when cross-compiling
AN_MAKEVAR([AR], [AC_PROG_AR])
AN_PROGRAM([ar], [AC_PROG_AR])
AC_DEFUN([AC_PROG_AR], [AC_CHECK_TOOL([AR], [ar], [ar])])
AC_PROG_AR

dnl Check whether the above macro has settled for a simply named tool even
dnl though we're cross compiling. We must do this before running AC_PROG_CC,
dnl because that will find any cc on the system, not only the cross-compiler,
dnl and then verify that a binary built with this compiler runs on the
dnl build system. It will then come to the false conclusion that we're not
dnl cross-compiling.
if test x$enable_tool_name_check != xno; then
    if test x$ac_tool_warned = xyes; then
        AC_MSG_ERROR([We are cross compiling but could not find a properly named toolchain. Do you have your cross-compiling toolchain in PATH? (You can --disable-tool-name-check to ignore this.)])
	elif test "x$ac_ct_AR" != x -a x$cross_compiling = xmaybe; then
		AC_MSG_ERROR([We think we are cross compiling but could not find a properly named toolchain. Do you have your cross-compiling toolchain in PATH? (You can --disable-tool-name-check to ignore this.)])
	fi
fi

AC_PROG_CC
AC_PROG_CPP
AC_PROG_MAKE_SET
AC_PROG_RANLIB

dnl autoconf 2.59 appears not to support AC_PROG_SED
AC_CHECK_PROG([SED],[sed],[sed],[/bin/false])

dnl check for asciidoc and a2x
AC_PATH_PROG([ASCIIDOC], [asciidoc], none)
AC_PATH_PROGS([A2X], [a2x a2x.py], none)

AM_CONDITIONAL(USE_ASCIIDOC, test x$asciidoc = xtrue)

AM_CONDITIONAL(USE_FW_HELPER, test x$natpmp = xtrue || test x$upnp = xtrue)
AM_CONDITIONAL(NAT_PMP, test x$natpmp = xtrue)
AM_CONDITIONAL(MINIUPNPC, test x$upnp = xtrue)
AM_PROG_CC_C_O

AC_ARG_VAR(PYTHON)
AC_CHECK_PROGS(PYTHON, [python python2 python2.7 python3 python3.3])
if test "x$PYTHON" = "x"; then
  AC_MSG_WARN([Python unavailable; some tests will not be run.])
fi
AM_CONDITIONAL(USEPYTHON, [test "x$PYTHON" != "x"])

ifdef([AC_C_FLEXIBLE_ARRAY_MEMBER], [
AC_C_FLEXIBLE_ARRAY_MEMBER
], [
 dnl Maybe we've got an old autoconf...
 AC_CACHE_CHECK([for flexible array members],
     tor_cv_c_flexarray,
     [AC_COMPILE_IFELSE(
       AC_LANG_PROGRAM([
 struct abc { int a; char b[]; };
], [
 struct abc *def = malloc(sizeof(struct abc)+sizeof(char));
 def->b[0] = 33;
]),
  [tor_cv_c_flexarray=yes],
  [tor_cv_c_flexarray=no])])
 if test $tor_cv_flexarray = yes ; then
   AC_DEFINE([FLEXIBLE_ARRAY_MEMBER], [], [Define to nothing if C supports flexible array members, and to 1 if it does not.])
 else
   AC_DEFINE([FLEXIBLE_ARRAY_MEMBER], [1], [Define to nothing if C supports flexible array members, and to 1 if it does not.])
 fi
])

AC_PATH_PROG([SHA1SUM], [sha1sum], none)
AC_PATH_PROG([OPENSSL], [openssl], none)

TORUSER=_tor
AC_ARG_WITH(tor-user,
        [  --with-tor-user=NAME    Specify username for tor daemon ],
        [
           TORUSER=$withval
        ]
)
AC_SUBST(TORUSER)

TORGROUP=_tor
AC_ARG_WITH(tor-group,
        [  --with-tor-group=NAME   Specify group name for tor daemon ],
        [
           TORGROUP=$withval
        ]
)
AC_SUBST(TORGROUP)


dnl If _WIN32 is defined and non-zero, we are building for win32
AC_MSG_CHECKING([for win32])
AC_RUN_IFELSE([AC_LANG_SOURCE([
int main(int c, char **v) {
#ifdef _WIN32
#if _WIN32
  return 0;
#else
  return 1;
#endif
#else
  return 2;
#endif
}])],
bwin32=true; AC_MSG_RESULT([yes]),
bwin32=false; AC_MSG_RESULT([no]),
bwin32=cross; AC_MSG_RESULT([cross])
)

if test "$bwin32" = cross; then
AC_MSG_CHECKING([for win32 (cross)])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#ifdef _WIN32
int main(int c, char **v) {return 0;}
#else
#error
int main(int c, char **v) {return x(y);}
#endif
])],
bwin32=true; AC_MSG_RESULT([yes]),
bwin32=false; AC_MSG_RESULT([no]))
fi

AM_CONDITIONAL(BUILD_NT_SERVICES, test x$bwin32 = xtrue)

dnl Enable C99 when compiling with MIPSpro
AC_MSG_CHECKING([for MIPSpro compiler])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(, [
#if (defined(__sgi) && defined(_COMPILER_VERSION))
#error
  return x(y);
#endif
])],
bmipspro=false; AC_MSG_RESULT(no),
bmipspro=true; AC_MSG_RESULT(yes))

if test "$bmipspro" = true; then
  CFLAGS="$CFLAGS -c99"
fi

AC_C_BIGENDIAN

AC_SEARCH_LIBS(socket, [socket network])
AC_SEARCH_LIBS(gethostbyname, [nsl])
AC_SEARCH_LIBS(dlopen, [dl])
AC_SEARCH_LIBS(inet_aton, [resolv])
saved_LIBS="$LIBS"
AC_SEARCH_LIBS([clock_gettime], [rt])
if test "$LIBS" != "$saved_LIBS"; then
   # Looks like we need -lrt for clock_gettime().
   have_rt=yes
fi

if test "$enable_threads" = "yes"; then
  AC_SEARCH_LIBS(pthread_create, [pthread])
  AC_SEARCH_LIBS(pthread_detach, [pthread])
fi

dnl -------------------------------------------------------------------
dnl Check for functions before libevent, since libevent-1.2 apparently
dnl exports strlcpy without defining it in a header.

AC_CHECK_FUNCS(
        _NSGetEnviron \
        accept4 \
        backtrace \
        backtrace_symbols_fd \
        clock_gettime \
        flock \
        ftime \
        getaddrinfo \
        getifaddrs \
        getrlimit \
        gettimeofday \
        gmtime_r \
        inet_aton \
        ioctl \
        issetugid \
        llround \
        localtime_r \
        lround \
        memmem \
        prctl \
        rint \
        sigaction \
        socketpair \
        strlcat \
        strlcpy \
        strptime \
        strtok_r \
        strtoull \
        sysconf \
	sysctl \
        uname \
        usleep \
        vasprintf \
	_vscprintf
)

if test "$enable_threads" = "yes"; then
  AC_CHECK_HEADERS(pthread.h)
  AC_CHECK_FUNCS(pthread_create)
fi

dnl ------------------------------------------------------
dnl Where do you live, libevent?  And how do we call you?

if test "$bwin32" = true; then
  TOR_LIB_WS32=-lws2_32
  TOR_LIB_IPHLPAPI=-liphlpapi
  # Some of the cargo-cults recommend -lwsock32 as well, but I don't
  # think it's actually necessary.
  TOR_LIB_GDI=-lgdi32
else
  TOR_LIB_WS32=
  TOR_LIB_GDI=
fi
AC_SUBST(TOR_LIB_WS32)
AC_SUBST(TOR_LIB_GDI)
AC_SUBST(TOR_LIB_IPHLPAPI)

dnl We need to do this before we try our disgusting hack below.
AC_CHECK_HEADERS([sys/types.h])

dnl This is a disgusting hack so we safely include older libevent headers.
AC_CHECK_TYPE(u_int64_t, unsigned long long)
AC_CHECK_TYPE(u_int32_t, unsigned long)
AC_CHECK_TYPE(u_int16_t, unsigned short)
AC_CHECK_TYPE(u_int8_t, unsigned char)

tor_libevent_pkg_redhat="libevent"
tor_libevent_pkg_debian="libevent-dev"
tor_libevent_devpkg_redhat="libevent-devel"
tor_libevent_devpkg_debian="libevent-dev"

dnl On Gnu/Linux or any place we require it, we'll add librt to the Libevent
dnl linking for static builds.
STATIC_LIBEVENT_FLAGS=""
if test "$enable_static_libevent" = "yes"; then
    if test "$have_rt" = yes; then
      STATIC_LIBEVENT_FLAGS=" -lrt "
    fi
fi

TOR_SEARCH_LIBRARY(libevent, $trylibeventdir, [-levent $STATIC_LIBEVENT_FLAGS $TOR_LIB_WS32], [
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <event.h>], [
#ifdef _WIN32
#include <winsock2.h>
#endif
void exit(int); void *event_init(void);],
    [
#ifdef _WIN32
{WSADATA d; WSAStartup(0x101,&d); }
#endif
event_init(); exit(0);
], [--with-libevent-dir], [/opt/libevent])

dnl Now check for particular libevent functions.
save_LIBS="$LIBS"
save_LDFLAGS="$LDFLAGS"
save_CPPFLAGS="$CPPFLAGS"
LIBS="-levent $STATIC_LIBEVENT_FLAGS $TOR_LIB_WS32 $LIBS"
LDFLAGS="$TOR_LDFLAGS_libevent $LDFLAGS"
CPPFLAGS="$TOR_CPPFLAGS_libevent $CPPFLAGS"
AC_CHECK_FUNCS([event_get_version \
                event_get_version_number \
                event_get_method \
                event_set_log_callback \
                evutil_secure_rng_set_urandom_device_file \
                evutil_secure_rng_init \
                event_base_loopexit])
AC_CHECK_MEMBERS([struct event.min_heap_idx], , ,
[#include <event.h>
])

AC_CHECK_HEADERS(event2/event.h event2/dns.h event2/bufferevent_ssl.h)

LIBS="$save_LIBS"
LDFLAGS="$save_LDFLAGS"
CPPFLAGS="$save_CPPFLAGS"


AM_CONDITIONAL(USE_EXTERNAL_EVDNS, test x$ac_cv_header_event2_dns_h = xyes)

if test "$enable_static_libevent" = "yes"; then
   if test "$tor_cv_library_libevent_dir" = "(system)"; then
     AC_MSG_ERROR("You must specify an explicit --with-libevent-dir=x option when using --enable-static-libevent")
   else
     TOR_LIBEVENT_LIBS="$TOR_LIBDIR_libevent/libevent.a $STATIC_LIBEVENT_FLAGS"
   fi
else
     TOR_LIBEVENT_LIBS="-levent"
fi

dnl This isn't the best test for Libevent 2.0.3-alpha.  Once it's released,
dnl we can do much better.
if test "$enable_bufferevents" = "yes" ; then
  if test "$ac_cv_header_event2_bufferevent_ssl_h" != "yes" ; then
    AC_MSG_ERROR([You've asked for bufferevent support, but you're using a version of Libevent without SSL support.  This won't work.  We need Libevent 2.0.8-rc or later, and you don't seem to even have Libevent 2.0.3-alpha.])
  else

    CPPFLAGS="$CPPFLAGS $TOR_CPPFLAGS_libevent"

    # Check for the right version.  First see if version detection works.
    AC_MSG_CHECKING([whether we can detect the Libevent version])
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <event2/event.h>
#if !defined(LIBEVENT_VERSION_NUMBER) || LIBEVENT_VERSION_NUMBER < 10
#error
int x = y(zz);
#else
int x = 1;
#endif
  ])], [event_version_number_works=yes; AC_MSG_RESULT([yes]) ],
     [event_version_number_works=no;  AC_MSG_RESULT([no])])
    if test "$event_version_number_works" != 'yes'; then
      AC_MSG_WARN([Version detection on Libevent seems broken.  Your Libevent installation is probably screwed up or very old.])
    else
      AC_MSG_CHECKING([whether Libevent is new enough for bufferevents])
      AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <event2/event.h>
#if !defined(LIBEVENT_VERSION_NUMBER) || LIBEVENT_VERSION_NUMBER < 0x02000d00
#error
int x = y(zz);
#else
int x = 1;
#endif
   ])], [ AC_MSG_RESULT([yes]) ],
      [ AC_MSG_RESULT([no])
        AC_MSG_ERROR([Libevent does not seem new enough to support bufferevents.  We require 2.0.13-stable or later]) ] )
    fi
  fi
fi

LIBS="$save_LIBS"
LDFLAGS="$save_LDFLAGS"
CPPFLAGS="$save_CPPFLAGS"

AM_CONDITIONAL(USE_BUFFEREVENTS, test "$enable_bufferevents" = "yes")
if test "$enable_bufferevents" = "yes"; then
  AC_DEFINE(USE_BUFFEREVENTS, 1, [Defined if we're going to use Libevent's buffered IO API])
  if test "$enable_static_libevent" = "yes"; then
    TOR_LIBEVENT_LIBS="$TOR_LIBDIR_libevent/libevent_openssl.a $TOR_LIBEVENT_LIBS"
  else
    TOR_LIBEVENT_LIBS="-levent_openssl $TOR_LIBEVENT_LIBS"
  fi
fi
AC_SUBST(TOR_LIBEVENT_LIBS)

dnl ------------------------------------------------------
dnl Where do you live, libm?

dnl On some platforms (Haiku/BeOS) the math library is
dnl part of libroot. In which case don't link against lm
TOR_LIB_MATH=""
save_LIBS="$LIBS"
AC_SEARCH_LIBS(pow, [m], , AC_MSG_ERROR([Could not find pow in libm or libc.]))
if test "$ac_cv_search_pow" != "none required"; then
    TOR_LIB_MATH="$ac_cv_search_pow"
fi
LIBS="$save_LIBS"
AC_SUBST(TOR_LIB_MATH)

dnl ------------------------------------------------------
dnl Where do you live, openssl?  And how do we call you?

tor_openssl_pkg_redhat="openssl"
tor_openssl_pkg_debian="libssl-dev"
tor_openssl_devpkg_redhat="openssl-devel"
tor_openssl_devpkg_debian="libssl-dev"

ALT_openssl_WITHVAL=""
AC_ARG_WITH(ssl-dir,
  [  --with-ssl-dir=PATH    Obsolete alias for --with-openssl-dir ],
  [
      if test "x$withval" != xno && test "x$withval" != "x" ; then
         ALT_openssl_WITHVAL="$withval"
      fi
  ])

TOR_SEARCH_LIBRARY(openssl, $tryssldir, [-lssl -lcrypto $TOR_LIB_GDI],
    [#include <openssl/rand.h>],
    [void RAND_add(const void *buf, int num, double entropy);],
    [RAND_add((void*)0,0,0); exit(0);], [],
    [/usr/local/openssl /usr/lib/openssl /usr/local/ssl /usr/lib/ssl /usr/local /usr/athena /opt/openssl])

dnl XXXX check for OPENSSL_VERSION_NUMBER == SSLeay()

if test "$enable_static_openssl" = "yes"; then
   if test "$tor_cv_library_openssl_dir" = "(system)"; then
     AC_MSG_ERROR("You must specify an explicit --with-openssl-dir=x option when using --enable-static-openssl")
   else
     TOR_OPENSSL_LIBS="$TOR_LIBDIR_openssl/libssl.a $TOR_LIBDIR_openssl/libcrypto.a"
   fi
else
     TOR_OPENSSL_LIBS="-lssl -lcrypto"
fi
AC_SUBST(TOR_OPENSSL_LIBS)

AC_CHECK_MEMBERS([struct ssl_method_st.get_cipher_by_char], , ,
[#include <openssl/ssl.h>
])

dnl ------------------------------------------------------
dnl Where do you live, zlib?  And how do we call you?

tor_zlib_pkg_redhat="zlib"
tor_zlib_pkg_debian="zlib1g"
tor_zlib_devpkg_redhat="zlib-devel"
tor_zlib_devpkg_debian="zlib1g-dev"

TOR_SEARCH_LIBRARY(zlib, $tryzlibdir, [-lz],
    [#include <zlib.h>],
    [const char * zlibVersion(void);],
    [zlibVersion(); exit(0);], [--with-zlib-dir],
    [/opt/zlib])

if test "$enable_static_zlib" = "yes"; then
   if test "$tor_cv_library_zlib_dir" = "(system)"; then
     AC_MSG_ERROR("You must specify an explicit --with-zlib-dir=x option when
 using --enable-static-zlib")
   else
     TOR_ZLIB_LIBS="$TOR_LIBDIR_zlib/libz.a"
   fi
else
     TOR_ZLIB_LIBS="-lz"
fi
AC_SUBST(TOR_ZLIB_LIBS)

dnl ---------------------------------------------------------------------
dnl Now that we know about our major libraries, we can check for compiler
dnl and linker hardening options.  We need to do this with the libraries known,
dnl since sometimes the linker will like an option but not be willing to
dnl use it with a build of a library.

all_ldflags_for_check="$TOR_LDFLAGS_zlib $TOR_LDFLAGS_openssl $TOR_LDFLAGS_libevent"
all_libs_for_check="$TOR_ZLIB_LIBS $TOR_LIB_MATH $TOR_LIBEVENT_LIBS $TOR_OPENSSL_LIBS $TOR_LIB_WS32 $TOR_LIB_GDI"

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [
#if !defined(__clang__)
#error
#endif])], have_clang=yes, have_clang=no)

if test x$enable_gcc_hardening != xno; then
    CFLAGS="$CFLAGS -D_FORTIFY_SOURCE=2"
    if test x$have_clang = xyes; then
        TOR_CHECK_CFLAGS(-Qunused-arguments)
    fi
    TOR_CHECK_CFLAGS(-fstack-protector-all, also_link)
    AS_VAR_PUSHDEF([can_compile], [tor_cv_cflags_-fstack-protector-all])
    AS_VAR_PUSHDEF([can_link], [tor_can_link_-fstack-protector-all])
    AS_VAR_IF(can_compile, [yes],
        AS_VAR_IF(can_link, [yes],
                  [],
                  AC_MSG_ERROR([We tried to build with stack protection; it looks like your compiler supports it but your libc does not provide it. Are you missing libssp? (You can --disable-gcc-hardening to ignore this error.)]))
        )
    AS_VAR_POPDEF([can_link])
    AS_VAR_POPDEF([can_compile])
    TOR_CHECK_CFLAGS(-Wstack-protector)
    TOR_CHECK_CFLAGS(-fwrapv)
    TOR_CHECK_CFLAGS(--param ssp-buffer-size=1)
    if test "$bwin32" = "false"; then
       TOR_CHECK_CFLAGS(-fPIE)
       TOR_CHECK_LDFLAGS(-pie, "$all_ldflags_for_check", "$all_libs_for_check")
    fi
fi

if test x$enable_expensive_hardening = xyes ; then
   TOR_CHECK_CFLAGS([-fsanitize=address])
   TOR_CHECK_CFLAGS([-fsanitize=undefined])
   TOR_CHECK_CFLAGS([-fno-omit-frame-pointer])
fi

if test x$enable_linker_hardening != xno; then
    TOR_CHECK_LDFLAGS(-z relro -z now, "$all_ldflags_for_check", "$all_libs_for_check")
fi

# For backtrace support
TOR_CHECK_LDFLAGS(-rdynamic)

dnl ------------------------------------------------------
dnl Now see if we have a -fomit-frame-pointer compiler option.

saved_CFLAGS="$CFLAGS"
TOR_CHECK_CFLAGS(-fomit-frame-pointer)
F_OMIT_FRAME_POINTER=''
if test "$saved_CFLAGS" != "$CFLAGS"; then
  if test x$enable_expensive_hardening != xyes ; then
    F_OMIT_FRAME_POINTER='-fomit-frame-pointer'
  fi
fi
CFLAGS="$saved_CFLAGS"
AC_SUBST(F_OMIT_FRAME_POINTER)

dnl ------------------------------------------------------
dnl If we are adding -fomit-frame-pointer (or if the compiler's doing it
dnl for us, as GCC 4.6 and later do at many optimization levels), then
dnl we should try to add -fasynchronous-unwind-tables so that our backtrace
dnl code will work.
TOR_CHECK_CFLAGS(-fasynchronous-unwind-tables)

dnl ------------------------------------------------------
dnl Where do you live, libnatpmp?  And how do we call you?
dnl There are no packages for Debian or Redhat as of this patch

if test "$natpmp" = "true"; then
    AC_DEFINE(NAT_PMP, 1, [Define to 1 if we are building with nat-pmp.])
    TOR_SEARCH_LIBRARY(libnatpmp, $trylibnatpmpdir, [-lnatpmp $TOR_LIB_WS32 $TOR_LIB_IPHLPAPI],
        [#include <natpmp.h>],
        [#ifdef _WIN32
         #define STATICLIB
         #endif
         #include <natpmp.h>],
        [   int r;
            natpmp_t natpmp;
            natpmpresp_t response;
            r = initnatpmp(&natpmp, 0, 0);],
            [printf("initnatpmp() returned %d (%s)\n", r, r?"FAILED":"SUCCESS");
            exit(0);],
        [--with-libnatpmp-dir],
        [/usr/lib/])
fi


dnl ------------------------------------------------------
dnl Where do you live, libminiupnpc?  And how do we call you?
dnl There are no packages for Debian or Redhat as of this patch

if test "$upnp" = "true"; then
    AC_DEFINE(MINIUPNPC, 1, [Define to 1 if we are building with UPnP.])

    dnl Before we call TOR_SEARCH_LIBRARY we'll do a quick compile test
    dnl to see if we have miniupnpc-1.5 or -1.6
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([#include <miniupnpc/miniupnpc.h>],
        [upnpDiscover(1, 0, 0, 0);exit(0);])],[miniupnpc15="true"],[miniupnpc15="false"])

    if test "$miniupnpc15" = "true" ; then
        AC_DEFINE([MINIUPNPC15],[1],[libminiupnpc version 1.5 found])
        TOR_SEARCH_LIBRARY(libminiupnpc, $trylibminiupnpcdir, [-lminiupnpc $TOR_LIB_WS32 $TOR_LIB_IPHLPAPI],
            [#include <miniupnpc/miniwget.h>
             #include <miniupnpc/miniupnpc.h>
             #include <miniupnpc/upnpcommands.h>],
            [void upnpDiscover(int delay, const char * multicastif,
             const char * minissdpdsock, int sameport);],
            [upnpDiscover(1, 0, 0, 0); exit(0);],
            [--with-libminiupnpc-dir],
            [/usr/lib/])
    else
        TOR_SEARCH_LIBRARY(libminiupnpc, $trylibminiupnpcdir, [-lminiupnpc $TOR_LIB_WS32 $TOR_LIB_IPHLPAPI],
            [#include <miniupnpc/miniwget.h>
             #include <miniupnpc/miniupnpc.h>
             #include <miniupnpc/upnpcommands.h>],
            [void upnpDiscover(int delay, const char * multicastif,
             const char * minissdpdsock, int sameport, int ipv6, int * error);],
            [upnpDiscover(1, 0, 0, 0, 0, 0); exit(0);],
            [--with-libminiupnpc-dir],
            [/usr/lib/])
    fi
fi

dnl ============================================================
dnl Check for libseccomp

if test "x$enable_seccomp" != "xno"; then
  AC_CHECK_HEADERS([seccomp.h])
  AC_SEARCH_LIBS(seccomp_init, [seccomp])
fi

dnl ============================================================
dnl We need an implementation of curve25519.

dnl set these defaults.
have_a_curve25519=no
build_curve25519_donna=no
build_curve25519_donna_c64=no
use_curve25519_donna=no
use_curve25519_nacl=no
CURVE25519_LIBS=

if test x$enable_curve25519 != xno; then

  dnl The best choice is using curve25519-donna-c64, but that requires
  dnl that we
  AC_CACHE_CHECK([whether we can use curve25519-donna-c64],
    tor_cv_can_use_curve25519_donna_c64,
    [AC_RUN_IFELSE(
      [AC_LANG_PROGRAM([dnl
        #include <stdint.h>
        typedef unsigned uint128_t __attribute__((mode(TI)));
	int func(uint64_t a, uint64_t b) {
  	     uint128_t c = ((uint128_t)a) * b;
             int ok = ((uint64_t)(c>>96)) == 522859 &&
	           (((uint64_t)(c>>64))&0xffffffffL) == 3604448702L &&
                   (((uint64_t)(c>>32))&0xffffffffL) == 2351960064L &&
                   (((uint64_t)(c))&0xffffffffL) == 0;
             return ok;
        }
	], [dnl
	  int ok = func( ((uint64_t)2000000000) * 1000000000,
	  	         ((uint64_t)1234567890) << 24);
          return !ok;
        ])],
	[tor_cv_can_use_curve25519_donna_c64=yes],
        [tor_cv_can_use_curve25519_donna_c64=no],
	[AC_LINK_IFELSE(
          [AC_LANG_PROGRAM([dnl
        #include <stdint.h>
        typedef unsigned uint128_t __attribute__((mode(TI)));
	int func(uint64_t a, uint64_t b) {
  	     uint128_t c = ((uint128_t)a) * b;
             int ok = ((uint64_t)(c>>96)) == 522859 &&
	           (((uint64_t)(c>>64))&0xffffffffL) == 3604448702L &&
                   (((uint64_t)(c>>32))&0xffffffffL) == 2351960064L &&
                   (((uint64_t)(c))&0xffffffffL) == 0;
             return ok;
        }
	], [dnl
	  int ok = func( ((uint64_t)2000000000) * 1000000000,
	  	         ((uint64_t)1234567890) << 24);
          return !ok;
        ])],
            [tor_cv_can_use_curve25519_donna_c64=cross],
	    [tor_cv_can_use_curve25519_donna_c64=no])])])

  AC_CHECK_HEADERS([crypto_scalarmult_curve25519.h \
                    nacl/crypto_scalarmult_curve25519.h])

  AC_CACHE_CHECK([for nacl compiled with a fast curve25519 implementation],
    tor_cv_can_use_curve25519_nacl,
    [tor_saved_LIBS="$LIBS"
     LIBS="$LIBS -lnacl"
     AC_LINK_IFELSE(
       [AC_LANG_PROGRAM([dnl
         #ifdef HAVE_CRYPTO_SCALARMULT_CURVE25519_H
         #include <crypto_scalarmult_curve25519.h>
	 #elif defined(HAVE_NACL_CRYPTO_SCALARMULT_CURVE25519_H)
	 #include <nacl/crypto_scalarmult_curve25519.h>
	 #endif
         #ifdef crypto_scalarmult_curve25519_ref_BYTES
	 #error Hey, this is the reference implementation! That's not fast.
	 #endif
       ], [
	 unsigned char *a, *b, *c; crypto_scalarmult_curve25519(a,b,c);
       ])], [tor_cv_can_use_curve25519_nacl=yes],
       [tor_cv_can_use_curve25519_nacl=no])
     LIBS="$tor_saved_LIBS" ])

   dnl Okay, now we need to figure out which one to actually use. Fall back
   dnl to curve25519-donna.c

   if test x$tor_cv_can_use_curve25519_donna_c64 != xno; then
     build_curve25519_donna_c64=yes
     use_curve25519_donna=yes
   elif test x$tor_cv_can_use_curve25519_nacl = xyes; then
     use_curve25519_nacl=yes
     CURVE25519_LIBS=-lnacl
   else
     build_curve25519_donna=yes
     use_curve25519_donna=yes
   fi
   have_a_curve25519=yes
fi

if test x$have_a_curve25519 = xyes; then
  AC_DEFINE(CURVE25519_ENABLED, 1,
            [Defined if we have a curve25519 implementation])
fi
if test x$use_curve25519_donna = xyes; then
  AC_DEFINE(USE_CURVE25519_DONNA, 1,
            [Defined if we should use an internal curve25519_donna{,_c64} implementation])
fi
if test x$use_curve25519_nacl = xyes; then
  AC_DEFINE(USE_CURVE25519_NACL, 1,
            [Defined if we should use a curve25519 from nacl])
fi
AM_CONDITIONAL(BUILD_CURVE25519_DONNA, test x$build_curve25519_donna = xyes)
AM_CONDITIONAL(BUILD_CURVE25519_DONNA_C64, test x$build_curve25519_donna_c64 = xyes)
AM_CONDITIONAL(CURVE25519_ENABLED, test x$have_a_curve25519 = xyes)
AC_SUBST(CURVE25519_LIBS)

dnl Make sure to enable support for large off_t if available.
AC_SYS_LARGEFILE

AC_CHECK_HEADERS(
        assert.h \
        errno.h \
        fcntl.h \
        signal.h \
        string.h \
        sys/fcntl.h \
        sys/stat.h \
        sys/time.h \
        sys/types.h \
        time.h \
        unistd.h
 , , AC_MSG_WARN(Some headers were not found, compilation may fail.  If compilation succeeds, please send your orconfig.h to the developers so we can fix this warning.))

dnl These headers are not essential

AC_CHECK_HEADERS(
        arpa/inet.h \
        crt_externs.h \
        execinfo.h \
        grp.h \
        ifaddrs.h \
        inttypes.h \
        limits.h \
        linux/types.h \
        machine/limits.h \
        malloc.h \
        malloc/malloc.h \
        malloc_np.h \
        netdb.h \
        netinet/in.h \
        netinet/in6.h \
        pwd.h \
        stdint.h \
        sys/file.h \
        sys/ioctl.h \
        sys/limits.h \
        sys/mman.h \
        sys/param.h \
        sys/prctl.h \
        sys/resource.h \
        sys/select.h \
        sys/socket.h \
	sys/sysctl.h \
        sys/syslimits.h \
        sys/time.h \
        sys/types.h \
        sys/un.h \
        sys/utime.h \
        sys/wait.h \
        syslog.h \
        utime.h
)

AC_CHECK_HEADERS(sys/param.h)

AC_CHECK_HEADERS(net/if.h, net_if_found=1, net_if_found=0,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif])
AC_CHECK_HEADERS(net/pfvar.h, net_pfvar_found=1, net_pfvar_found=0,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif])
AC_CHECK_HEADERS(linux/netfilter_ipv4.h,
        linux_netfilter_ipv4=1, linux_netfilter_ipv4=0,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LINUX_TYPES_H
#include <linux/types.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif])

if test x$transparent = xtrue ; then
   transparent_ok=0
   if test x$net_if_found = x1 && test x$net_pfvar_found = x1 ; then
     transparent_ok=1
   fi
   if test x$linux_netfilter_ipv4 = x1 ; then
     transparent_ok=1
   fi
   if test x$transparent_ok = x1 ; then
     AC_DEFINE(USE_TRANSPARENT, 1, "Define to enable transparent proxy support")
     case $host in
       *-*-openbsd* | *-*-bitrig*)
         AC_DEFINE(OPENBSD, 1, "Define to handle pf on OpenBSD properly") ;;
     esac
   else
     AC_MSG_NOTICE([Transparent proxy support enabled, but missing headers.])
   fi
fi

AC_CHECK_MEMBERS([struct timeval.tv_sec], , ,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif])

dnl In case we aren't given a working stdint.h, we'll need to grow our own.
dnl Watch out.

AC_CHECK_SIZEOF(int8_t)
AC_CHECK_SIZEOF(int16_t)
AC_CHECK_SIZEOF(int32_t)
AC_CHECK_SIZEOF(int64_t)
AC_CHECK_SIZEOF(uint8_t)
AC_CHECK_SIZEOF(uint16_t)
AC_CHECK_SIZEOF(uint32_t)
AC_CHECK_SIZEOF(uint64_t)
AC_CHECK_SIZEOF(intptr_t)
AC_CHECK_SIZEOF(uintptr_t)

dnl AC_CHECK_TYPES([int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t, intptr_t, uintptr_t])

AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(__int64)
AC_CHECK_SIZEOF(void *)
AC_CHECK_SIZEOF(time_t)
AC_CHECK_SIZEOF(size_t)
AC_CHECK_SIZEOF(pid_t)

AC_CHECK_TYPES([uint, u_char, ssize_t])

AC_PC_FROM_UCONTEXT([/bin/true])

dnl used to include sockaddr_storage, but everybody has that.
AC_CHECK_TYPES([struct in6_addr, struct sockaddr_in6, sa_family_t], , ,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef _WIN32
#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#if defined(_MSC_VER) && (_MSC_VER < 1300)
#include <winsock.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#endif
])
AC_CHECK_MEMBERS([struct in6_addr.s6_addr32, struct in6_addr.s6_addr16, struct sockaddr_in.sin_len, struct sockaddr_in6.sin6_len], , ,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef _WIN32
#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#if defined(_MSC_VER) && (_MSC_VER < 1300)
#include <winsock.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#endif
])

AC_CHECK_TYPES([rlim_t], , ,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
])

AX_CHECK_SIGN([time_t],
       [ AC_DEFINE(TIME_T_IS_SIGNED, 1, [Define if time_t is signed]) ],
       [ : ], [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
])

if test "$ax_cv_decl_time_t_signed" = no; then
  AC_MSG_WARN([You have an unsigned time_t; some things will probably break. Please tell the Tor developers about your interesting platform.])
fi

AX_CHECK_SIGN([size_t],
       [ tor_cv_size_t_signed=yes ],
       [ tor_cv_size_t_signed=no ], [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
])

if test "$ax_cv_decl_size_t_signed" = yes; then
  AC_MSG_ERROR([You have a signed size_t; that's grossly nonconformant.])
fi

AX_CHECK_SIGN([enum always],
       [ AC_DEFINE(ENUM_VALS_ARE_SIGNED, 1, [Define if enum is always signed]) ],
       [ : ], [
 enum always { AAA, BBB, CCC };
])

AC_CHECK_SIZEOF(socklen_t, , [AC_INCLUDES_DEFAULT()
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
])

# We want to make sure that we _don't_ have a cell_t defined, like IRIX does.

AC_CHECK_SIZEOF(cell_t)

# Now make sure that NULL can be represented as zero bytes.
AC_CACHE_CHECK([whether memset(0) sets pointers to NULL], tor_cv_null_is_zero,
[AC_RUN_IFELSE([AC_LANG_SOURCE(
[[#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif
int main () { char *p1,*p2; p1=NULL; memset(&p2,0,sizeof(p2));
return memcmp(&p1,&p2,sizeof(char*))?1:0; }]])],
       [tor_cv_null_is_zero=yes],
       [tor_cv_null_is_zero=no],
       [tor_cv_null_is_zero=cross])])

if test "$tor_cv_null_is_zero" = cross ; then
  # Cross-compiling; let's hope that the target isn't raving mad.
  AC_MSG_NOTICE([Cross-compiling: we'll assume that NULL is represented as a sequence of 0-valued bytes.])
fi

if test "$tor_cv_null_is_zero" != no; then
  AC_DEFINE([NULL_REP_IS_ZERO_BYTES], 1,
            [Define to 1 iff memset(0) sets pointers to NULL])
fi

AC_CACHE_CHECK([whether memset(0) sets doubles to 0.0], tor_cv_dbl0_is_zero,
[AC_RUN_IFELSE([AC_LANG_SOURCE(
[[#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif
int main () { double d1,d2; d1=0; memset(&d2,0,sizeof(d2));
return memcmp(&d1,&d2,sizeof(d1))?1:0; }]])],
       [tor_cv_dbl0_is_zero=yes],
       [tor_cv_dbl0_is_zero=no],
       [tor_cv_dbl0_is_zero=cross])])

if test "$tor_cv_dbl0_is_zero" = cross ; then
  # Cross-compiling; let's hope that the target isn't raving mad.
  AC_MSG_NOTICE([Cross-compiling: we'll assume that 0.0 can be represented as a sequence of 0-valued bytes.])
fi

if test "$tor_cv_dbl0_is_zero" != no; then
  AC_DEFINE([DOUBLE_0_REP_IS_ZERO_BYTES], 1,
            [Define to 1 iff memset(0) sets doubles to 0.0])
fi

# And what happens when we malloc zero?
AC_CACHE_CHECK([whether we can malloc(0) safely.], tor_cv_malloc_zero_works,
[AC_RUN_IFELSE([AC_LANG_SOURCE(
[[#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif
int main () { return malloc(0)?0:1; }]])],
       [tor_cv_malloc_zero_works=yes],
       [tor_cv_malloc_zero_works=no],
       [tor_cv_malloc_zero_works=cross])])

if test "$tor_cv_malloc_zero_works" = cross; then
  # Cross-compiling; let's hope that the target isn't raving mad.
  AC_MSG_NOTICE([Cross-compiling: we'll assume that we need to check malloc() arguments for 0.])
fi

if test "$tor_cv_malloc_zero_works" = yes; then
  AC_DEFINE([MALLOC_ZERO_WORKS], 1,
            [Define to 1 iff malloc(0) returns a pointer])
fi

# whether we seem to be in a 2s-complement world.
AC_CACHE_CHECK([whether we are using 2s-complement arithmetic], tor_cv_twos_complement,
[AC_RUN_IFELSE([AC_LANG_SOURCE(
[[int main () { int problem = ((-99) != (~99)+1);
return problem ? 1 : 0; }]])],
       [tor_cv_twos_complement=yes],
       [tor_cv_twos_complement=no],
       [tor_cv_twos_complement=cross])])

if test "$tor_cv_twos_complement" = cross ; then
  # Cross-compiling; let's hope that the target isn't raving mad.
  AC_MSG_NOTICE([Cross-compiling: we'll assume that negative integers are represented with two's complement.])
fi

if test "$tor_cv_twos_complement" != no ; then
  AC_DEFINE([USING_TWOS_COMPLEMENT], 1,
            [Define to 1 iff we represent negative integers with two's complement])
fi

# What does shifting a negative value do?
AC_CACHE_CHECK([whether right-shift on negative values does sign-extension], tor_cv_sign_extend,
[AC_RUN_IFELSE([AC_LANG_SOURCE(
[[int main () { int okay = (-60 >> 8) == -1; return okay ? 0 : 1; }]])],
       [tor_cv_sign_extend=yes],
       [tor_cv_sign_extend=no],
       [tor_cv_sign_extend=cross])])

if test "$tor_cv_sign_extend" = cross ; then
  # Cross-compiling; let's hope that the target isn't raving mad.
  AC_MSG_NOTICE([Cross-compiling: we'll assume that right-shifting negative integers causes sign-extension])
fi

if test "$tor_cv_sign_extend" != no ; then
  AC_DEFINE([RSHIFT_DOES_SIGN_EXTEND], 1,
            [Define to 1 iff right-shifting a negative value performs sign-extension])
fi

# Whether we should use the dmalloc memory allocation debugging library.
AC_MSG_CHECKING(whether to use dmalloc (debug memory allocation library))
AC_ARG_WITH(dmalloc,
[  --with-dmalloc          Use debug memory allocation library. ],
[if [[ "$withval" = "yes" ]]; then
  dmalloc=1
  AC_MSG_RESULT(yes)
else
  dmalloc=1
  AC_MSG_RESULT(no)
fi], [ dmalloc=0; AC_MSG_RESULT(no) ]
)

if [[ $dmalloc -eq 1 ]]; then
  AC_CHECK_HEADERS(dmalloc.h, , AC_MSG_ERROR(dmalloc header file not found. Do you have the development files for dmalloc installed?))
  AC_SEARCH_LIBS(dmalloc_malloc, [dmallocth dmalloc], , AC_MSG_ERROR(Libdmalloc library not found. If you enable it you better have it installed.))
  AC_DEFINE(USE_DMALLOC, 1, [Debug memory allocation library])
  AC_CHECK_FUNCS(dmalloc_strdup dmalloc_strndup)
fi

AC_ARG_WITH(tcmalloc,
[  --with-tcmalloc         Use tcmalloc memory allocation library. ],
[ tcmalloc=yes ], [ tcmalloc=no ])

if test x$tcmalloc = xyes ; then
   LDFLAGS="-ltcmalloc $LDFLAGS"
fi

using_custom_malloc=no
if test x$enable_openbsd_malloc = xyes ; then
   using_custom_malloc=yes
fi
if test x$tcmalloc = xyes ; then
   using_custom_malloc=yes
fi
if test $using_custom_malloc = no ; then
   AC_CHECK_FUNCS(mallinfo)
fi

# By default, we're going to assume we don't have mlockall()
# bionic and other platforms have various broken mlockall subsystems.
# Some systems don't have a working mlockall, some aren't linkable,
# and some have it but don't declare it.
AC_CHECK_FUNCS(mlockall)
AC_CHECK_DECLS([mlockall], , , [
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif])

# Allow user to specify an alternate syslog facility
AC_ARG_WITH(syslog-facility,
[  --with-syslog-facility=LOG syslog facility to use (default=LOG_DAEMON)],
syslog_facility="$withval", syslog_facility="LOG_DAEMON")
AC_DEFINE_UNQUOTED(LOGFACILITY,$syslog_facility,[name of the syslog facility])
AC_SUBST(LOGFACILITY)

# Check if we have getresuid and getresgid
AC_CHECK_FUNCS(getresuid getresgid)

# Check for gethostbyname_r in all its glorious incompatible versions.
#   (This logic is based on that in Python's configure.in)
AH_TEMPLATE(HAVE_GETHOSTBYNAME_R,
  [Define this if you have any gethostbyname_r()])

AC_CHECK_FUNC(gethostbyname_r, [
  AC_MSG_CHECKING([how many arguments gethostbyname_r() wants])
  OLD_CFLAGS=$CFLAGS
  CFLAGS="$CFLAGS $MY_CPPFLAGS $MY_THREAD_CPPFLAGS $MY_CFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <netdb.h>
  ], [[
    char *cp1, *cp2;
    struct hostent *h1, *h2;
    int i1, i2;
    (void)gethostbyname_r(cp1,h1,cp2,i1,&h2,&i2);
  ]])],[
    AC_DEFINE(HAVE_GETHOSTBYNAME_R)
    AC_DEFINE(HAVE_GETHOSTBYNAME_R_6_ARG, 1,
     [Define this if gethostbyname_r takes 6 arguments])
    AC_MSG_RESULT(6)
  ], [
    AC_TRY_COMPILE([
#include <netdb.h>
    ], [
      char *cp1, *cp2;
      struct hostent *h1;
      int i1, i2;
      (void)gethostbyname_r(cp1,h1,cp2,i1,&i2);
    ], [
      AC_DEFINE(HAVE_GETHOSTBYNAME_R)
      AC_DEFINE(HAVE_GETHOSTBYNAME_R_5_ARG, 1,
        [Define this if gethostbyname_r takes 5 arguments])
      AC_MSG_RESULT(5)
   ], [
      AC_TRY_COMPILE([
#include <netdb.h>
     ], [
       char *cp1;
       struct hostent *h1;
       struct hostent_data hd;
       (void) gethostbyname_r(cp1,h1,&hd);
     ], [
       AC_DEFINE(HAVE_GETHOSTBYNAME_R)
       AC_DEFINE(HAVE_GETHOSTBYNAME_R_3_ARG, 1,
         [Define this if gethostbyname_r takes 3 arguments])
       AC_MSG_RESULT(3)
     ], [
       AC_MSG_RESULT(0)
     ])
  ])
 ])
 CFLAGS=$OLD_CFLAGS
])

AC_CACHE_CHECK([whether the C compiler supports __func__],
  tor_cv_have_func_macro,
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <stdio.h>
int main(int c, char **v) { puts(__func__); }])],
  tor_cv_have_func_macro=yes,
  tor_cv_have_func_macro=no))

AC_CACHE_CHECK([whether the C compiler supports __FUNC__],
  tor_cv_have_FUNC_macro,
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <stdio.h>
int main(int c, char **v) { puts(__FUNC__); }])],
  tor_cv_have_FUNC_macro=yes,
  tor_cv_have_FUNC_macro=no))

AC_CACHE_CHECK([whether the C compiler supports __FUNCTION__],
  tor_cv_have_FUNCTION_macro,
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <stdio.h>
int main(int c, char **v) { puts(__FUNCTION__); }])],
  tor_cv_have_FUNCTION_macro=yes,
  tor_cv_have_FUNCTION_macro=no))

AC_CACHE_CHECK([whether we have extern char **environ already declared],
  tor_cv_have_environ_declared,
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([
/* We define _GNU_SOURCE here because it is also defined in compat.c.
 * Without it environ doesn't get declared. */
#define _GNU_SOURCE
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
int main(int c, char **v) { char **t = environ; }])],
  tor_cv_have_environ_declared=yes,
  tor_cv_have_environ_declared=no))

if test "$tor_cv_have_func_macro" = 'yes'; then
  AC_DEFINE(HAVE_MACRO__func__, 1, [Defined if the compiler supports __func__])
fi

if test "$tor_cv_have_FUNC_macro" = 'yes'; then
  AC_DEFINE(HAVE_MACRO__FUNC__, 1, [Defined if the compiler supports __FUNC__])
fi

if test "$tor_cv_have_FUNCTION_macro" = 'yes'; then
  AC_DEFINE(HAVE_MACRO__FUNCTION__, 1,
           [Defined if the compiler supports __FUNCTION__])
fi

if test "$tor_cv_have_environ_declared" = 'yes'; then
  AC_DEFINE(HAVE_EXTERN_ENVIRON_DECLARED, 1,
           [Defined if we have extern char **environ already declared])
fi

# $prefix stores the value of the --prefix command line option, or
# NONE if the option wasn't set.  In the case that it wasn't set, make
# it be the default, so that we can use it to expand directories now.
if test "x$prefix" = "xNONE"; then
  prefix=$ac_default_prefix
fi

# and similarly for $exec_prefix
if test "x$exec_prefix" = "xNONE"; then
  exec_prefix=$prefix
fi

if test "x$BUILDDIR" = "x"; then
  BUILDDIR=`pwd`
fi
AC_SUBST(BUILDDIR)
AH_TEMPLATE([BUILDDIR],[tor's build directory])
AC_DEFINE_UNQUOTED(BUILDDIR,"$BUILDDIR")

if test "x$CONFDIR" = "x"; then
  CONFDIR=`eval echo $sysconfdir/tor`
fi
AC_SUBST(CONFDIR)
AH_TEMPLATE([CONFDIR],[tor's configuration directory])
AC_DEFINE_UNQUOTED(CONFDIR,"$CONFDIR")

BINDIR=`eval echo $bindir`
AC_SUBST(BINDIR)
LOCALSTATEDIR=`eval echo $localstatedir`
AC_SUBST(LOCALSTATEDIR)

if test "$bwin32" = true; then
  # Test if the linker supports the --nxcompat and --dynamicbase options
  # for Windows
  save_LDFLAGS="$LDFLAGS"
  LDFLAGS="-Wl,--nxcompat -Wl,--dynamicbase"
  AC_MSG_CHECKING([whether the linker supports DllCharacteristics])
  AC_LINK_IFELSE([AC_LANG_PROGRAM([])],
    [AC_MSG_RESULT([yes])]
    [save_LDFLAGS="$save_LDFLAGS $LDFLAGS"],
    [AC_MSG_RESULT([no])]
  )
  LDFLAGS="$save_LDFLAGS"
fi

# Set CFLAGS _after_ all the above checks, since our warnings are stricter
# than autoconf's macros like.
if test "$GCC" = yes; then
  # Disable GCC's strict aliasing checks.  They are an hours-to-debug
  # accident waiting to happen.
  CFLAGS="$CFLAGS -Wall -fno-strict-aliasing"
else
  # Autoconf sets -g -O2 by default. Override optimization level
  # for non-gcc compilers
  CFLAGS="$CFLAGS -O"
  enable_gcc_warnings=no
  enable_gcc_warnings_advisory=no
fi

# OS X Lion started deprecating the system openssl. Let's just disable
# all deprecation warnings on OS X. Also, to potentially make the binary
# a little smaller, let's enable dead_strip.
case "$host_os" in

 darwin*)
    CFLAGS="$CFLAGS -Wno-deprecated-declarations"
    LDFLAGS="$LDFLAGS -dead_strip" ;;
esac

# Add some more warnings which we use in development but not in the
# released versions.  (Some relevant gcc versions can't handle these.)
if test x$enable_gcc_warnings = xyes || test x$enable_gcc_warnings_advisory = xyes; then

  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [
#if !defined(__GNUC__) || (__GNUC__ < 4)
#error
#endif])], have_gcc4=yes, have_gcc4=no)

  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [
#if !defined(__GNUC__) || (__GNUC__ < 4) || (__GNUC__ == 4 && __GNUC_MINOR__ < 2)
#error
#endif])], have_gcc42=yes, have_gcc42=no)

  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [
#if !defined(__GNUC__) || (__GNUC__ < 4) || (__GNUC__ == 4 && __GNUC_MINOR__ < 3)
#error
#endif])], have_gcc43=yes, have_gcc43=no)

  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [
#if !defined(__GNUC__) || (__GNUC__ < 4) || (__GNUC__ == 4 && __GNUC_MINOR__ < 6)
#error
#endif])], have_gcc46=yes, have_gcc46=no)


  save_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS -Wshorten-64-to-32"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])], have_shorten64_flag=yes,
                    have_shorten64_flag=no)
  CFLAGS="$save_CFLAGS"

  case $host in
    *-*-openbsd* | *-*-bitrig*)
      # Some OpenBSD versions (like 4.8) have -Wsystem-headers by default.
      # That's fine, except that the headers don't pass -Wredundant-decls.
      # Therefore, let's disable -Wsystem-headers when we're building
      # with maximal warnings on OpenBSD.
      CFLAGS="$CFLAGS -Wno-system-headers" ;;
  esac

  CFLAGS="$CFLAGS -W -Wfloat-equal -Wundef -Wpointer-arith"
  CFLAGS="$CFLAGS -Wstrict-prototypes -Wmissing-prototypes -Wwrite-strings"
  CFLAGS="$CFLAGS -Wredundant-decls -Wchar-subscripts -Wcomment -Wformat=2"
  CFLAGS="$CFLAGS -Wwrite-strings -Wmissing-declarations -Wredundant-decls"
  CFLAGS="$CFLAGS -Wnested-externs -Wbad-function-cast -Wswitch-enum"

  if test x$enable_gcc_warnings = xyes; then
    CFLAGS="$CFLAGS -Werror"
  fi

  # Disabled, so we can use mallinfo(): -Waggregate-return

  if test x$have_gcc4 = xyes ; then
    # These warnings break gcc 3.3.5 and work on gcc 4.0.2
    CFLAGS="$CFLAGS -Winit-self -Wmissing-field-initializers -Wdeclaration-after-statement -Wold-style-definition"
  fi

  if test x$have_gcc42 = xyes ; then
    # These warnings break gcc 4.0.2 and work on gcc 4.2
    # XXXX020 See if any of these work with earlier versions.
    CFLAGS="$CFLAGS -Waddress -Wmissing-noreturn -Wstrict-overflow=1"

    # We used to use -Wstrict-overflow=5, but that breaks us heavily under 4.3.
  fi

  if test x$have_gcc42 = xyes && test x$have_clang = xno; then
    # These warnings break gcc 4.0.2 and clang, but work on gcc 4.2
    CFLAGS="$CFLAGS -Wnormalized=id -Woverride-init"
  fi

  if test x$have_gcc43 = xyes ; then
    # These warnings break gcc 4.2 and work on gcc 4.3
    # XXXX020 See if any of these work with earlier versions.
    CFLAGS="$CFLAGS -Wextra -Warray-bounds"
  fi

  if test x$have_gcc46 = xyes ; then
    # This warning was added in gcc 4.3, but it appears to generate
    # spurious warnings in gcc 4.4.  I don't know if it works in 4.5.
    CFLAGS="$CFLAGS -Wlogical-op"
  fi

  if test x$have_shorten64_flag = xyes ; then
    CFLAGS="$CFLAGS -Wshorten-64-to-32"
  fi



##This will break the world on some 64-bit architectures
# CFLAGS="$CFLAGS -Winline"
fi

if test "$enable_coverage" = yes && test "$have_clang" = "no"; then
   case "$host_os" in
    darwin*)
      AC_MSG_WARN([Tried to enable coverage on OSX without using the clang compiler. This might not work! If coverage fails, use CC=clang when configuring with --enable-profiling.])
   esac
fi

CPPFLAGS="$CPPFLAGS $TOR_CPPFLAGS_libevent $TOR_CPPFLAGS_openssl $TOR_CPPFLAGS_zlib"

AC_CONFIG_FILES([
        Doxyfile
        Makefile
        contrib/dist/suse/tor.sh
        contrib/operator-tools/tor.logrotate
        contrib/dist/tor.sh
        contrib/dist/torctl
        contrib/dist/tor.service
        src/config/torrc.sample
])

if test x$asciidoc = xtrue && test "$ASCIIDOC" = "none" ; then
  regular_mans="doc/tor doc/tor-gencert doc/tor-resolve doc/torify"
  for file in $regular_mans ; do
    if ! [[ -f "$srcdir/$file.1.in" ]] || ! [[ -f "$srcdir/$file.html.in" ]] ; then
      echo "==================================";
      echo;
      echo "You need asciidoc installed to be able to build the manpage.";
      echo "To build without manpages, use the --disable-asciidoc argument";
      echo "when calling configure.";
      echo;
      echo "==================================";
      exit 1;
    fi
  done
fi

AC_OUTPUT

if test -x /usr/bin/perl && test -x ./scripts/maint/updateVersions.pl ; then
  ./scripts/maint/updateVersions.pl
fi
