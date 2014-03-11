AC_INIT([OpenLLDP],[0.4alpha],[],[openlldp])
AC_PREREQ(2.59)
AM_INIT_AUTOMAKE([dist-bzip2] [dist-zip] [foreign])
AC_PROG_RANLIB
AC_PROG_CC
AM_MAINTAINER_MODE

# Host environment specific tests
AC_CANONICAL_HOST
AC_C_BIGENDIAN

# Enable all warnings for gcc (what about other compilers?)
if test -n "$GCC"; then
  CFLAGS="-Wall -fno-strict-aliasing $CFLAGS"
fi

GGDBFLAGS=""

########### CONFIGURE OPTIONS #############                                                                                                                    
# dnl Does this belong elsewhere?
# No libpcap support yet. ;)                                                                                                                                
# AC_MSG_CHECKING([user defined path to libpcap])
# AC_ARG_WITH(libpcap,
#   AS_HELP_STRING([--with-libpcap],
#    [location of libpcap/winpcap root development tree]),
#  [LDFLAGS="$LDFLAGS -L${with_libpcap}/lib";CPPFLAGS="$CPPFLAGS -I${with_libpcap}/include"])
# AC_MSG_RESULT([$with_libpcap])

# AC_MSG_CHECKING([user defined path to libpcap libraries])
# AC_ARG_WITH(libpcap-libs,
#   AS_HELP_STRING([--with-libpcap-libs],
#       [location of libpcap libraries]),
#   [LDFLAGS="$LDFLAGS -L${with_libpcap_libs}"])
# AC_MSG_RESULT([$with_libpcap_libs])
# AC_SUBST(with_libpcap_libs)

# AC_MSG_CHECKING([user defined path to libpcap includes])
# AC_ARG_WITH(libpcap-includes,
#   AS_HELP_STRING([--with-libpcap-includes],
#     [location of libpcapc includes]),
#   [CPPFLAGS="$CPPFLAGS -I${with_libpcap_includes}"])
# AC_MSG_RESULT([$with_libpcap_includes])

AC_ARG_ENABLE(experimental,
  AS_HELP_STRING([--enable-experimental],
    [enable the use of experimental features/code.]),
  [],
  [enable_experimental="no"])

AC_ARG_ENABLE(efence,
  AS_HELP_STRING([--enable-efence],
    [enable use of Electric Fence for memory debugging.]),
  [],
  [enable_efence="no"])

# No PCAP support yet. ;)
# AC_ARG_ENABLE(generic-framer,
#   AS_HELP_STRING([--enable-generic-framer],
#     [enable the use of PCAP instead of native handler.]),
#   [],
#   [enable_generic_framer="no"])

AC_ARG_ENABLE(panther-binary,
  AS_HELP_STRING([--enable-panther-binary],
    [build as Panther-compatible binary (Mac OS X Only)]),
  [],
  [enabled_panther_binary="no"])

AC_ARG_ENABLE(universal-binary,
  AS_HELP_STRING([--enable-universal-binary],
    [build as Universal Binary (Mac OS X Only)]),
  [],
  [enabled_universal_binary="no"])

########### CONFIGURE TESTS #############                                                                                                                      

dnl enable experimental code? (not currently used)                                                                                                             
AS_IF([test "$enable_experimental" = yes], AC_DEFINE(EXPERIMENTAL), [])

dnl check for efence                                                                                                                                           
AS_IF([test "$enable_efence" = yes],
  [ AC_CHECK_HEADER(efence.h,
      [MORELIBS="$MORELIBS -lefence -lpthread" ],
      AC_MSG_ERROR([You must have Electric Fence installed to use it!]))
    AC_DEFINE(USE_EFENCE)],
  [])

# Might use this in the future... 
# AC_MSG_CHECKING([for libxml-2.0])
# if `pkg-config libxml-2.0 --exists`; then
#     LIBS=`pkg-config libxml-2.0 --libs $LIBS`
#     AM_CPPFLAGS=`pkg-config libxml-2.0 --cflags $AM_CPPFLAGS`
#     AC_MSG_RESULT([found with pkg-config])
# elif test -e /usr/lib/libxml2.dylib; then
#     dnl Mac OS X doesn't have pkg-config.  So check in the defaut location.                                                                                   
#     LIBS="-L/usr/lib -lxml2 $LIBS"
#     AM_CPPFLAGS="-I/usr/include/libxml2 $AM_CPPFLAGS"
#     AC_MSG_RESULT([found in default location for Mac OS X])
# else
#     AC_MSG_ERROR([Xsupplicant requires libxml.  You need to either install libxml\
# , or set PKG_CONFIG_PATH to point to the location of the libxml-2.0.pc file.])
# fi

dnl Determine the operating system and the framer                                                                                                              
case "$host_os" in
   *linux*)
      framer="linux"
      platform="linux"
      AC_DEFINE(LINUX)
      MORELIBS="${MORELIBS} -lm"

      AC_CHECK_HEADERS(sys/socket.h, [], [AC_MSG_ERROR([header file \
                       <sys/socket.h> is required for OpenLLDP.])],)
   ;;

   *darwin*)
      framer="bpf"
      platform="darwin"
      AC_DEFINE(DARWIN)

      user_defined_sysroot="no"

      AS_IF([test "$enable_panther_binary" = "yes"],[
         user_defined_sysroot="yes"
	 sysroot_sdk="MacOSX10.3.9.sdk"
	 macosx_version_min="10.3"
      ], [])

      AS_IF([test "$enable_universal_binary" = "yes"],[
         user_defined_sysroot="yes"
         sysroot_sdk="MacOSX10.4u.sdk"
         macosx_version_min="10.4"

         CFLAGS="$CFLAGS -arch i386 -arch ppc "
         LDFLAGS="$LDFLAGS -arch i386 -arch ppc"
      ], [])

      AS_IF([test "$user_defined_sysroot" = "yes"],[
         # Check to see if the system has xcode-select (Mac OS X 10.5 "Leopard" or later).
	 # This is necessary if the user has installed their development SDKs somewhere
         # other than the default location (/Developer)
         # if xcode-select doesn't exist, assume the tools are in /Developer           
         AC_CHECK_FILE([/usr/bin/xcode-select],[
            sysroot=`/usr/bin/xcode-select -print-path`
         ],[
            sysroot="/Developer"
         ])

         AC_MSG_CHECKING([For Mac OS X SDK Path])
         AC_MSG_RESULT([$sysroot])

         CFLAGS="$CFLAGS -isysroot $sysroot/SDKs/${sysroot_sdk} -mmacosx-version-min=${macosx_version_min}"
      ], [])
   ;;

   dnl - Can we simplify this to *bsd*?
   *freebsd*)
      framer="bpf"
      AC_DEFINE(__BSD__)
      AC_DEFINE(FREEBSD)
   ;;

   *openbsd*)
      framer="bpf"
      AC_DEFINE(__BSD__)
      AC_DEFINE(OPENBSD)
   ;;
   
   *netbsd*)
      framer="bpf"
      AC_DEFINE(__BSD__)
      AC_DEFINE(NETBSD)
   ;;
   
   *windows* | *cygwin* | *mingw*)
      framer="windows"
      platform="windows"
   ;;

   *)
      framer="generic"
   ;;
esac

AC_MSG_CHECKING([for native frame interface])
AC_MSG_RESULT($framer)

AC_MSG_CHECKING([for platform OS])
AC_MSG_RESULT($platform)

AS_IF([test $framer = "linux"],
  [ MORELIBS="${MORELIBS} -lm"
    AC_MSG_CHECKING([for procfs support])
    if test -r /proc/net/dev; then
      AC_MSG_RESULT(okay)
    else
      AC_MSG_ERROR([You must have ProcFS enabled in your kernel!]);
    fi],
  [])

AS_IF([test $framer = "generic"],
  MORELIBS="${MORELIBS} -lwpcap -lpcap -ldnet",
  [])

AM_CONDITIONAL([FRAMER_GENERIC], [test "$framer" = generic])
AM_CONDITIONAL([FRAMER_LINUX],   [test "$framer" = linux])
AM_CONDITIONAL([FRAMER_BPF], 	 [test "$framer" = bpf])
AM_CONDITIONAL([FRAMER_DARWIN],  [test "$framer" = darwin])
AM_CONDITIONAL([FRAMER_RADIUS],  [test "$framer" = radius])
AM_CONDITIONAL([FRAMER_WINDOWS], [test "$framer" = windows])

dnl what about __BSD__?
AM_CONDITIONAL([LINUX],   [test "$platform" = linux])
AM_CONDITIONAL([DARWIN],  [test "$platform" = darwin])
AM_CONDITIONAL([WINDOWS], [test "$platform" = windows])
AM_CONDITIONAL([OPENBSD], [test "$platform" = openbsd])
AM_CONDITIONAL([NETBSD], [test "$platform" = netbsd])
AM_CONDITIONAL([FREEBSD], [test "$platform" = freebsd])

AC_CHECK_LIB(confuse, cfg_title, 
[AC_CHECK_HEADER(confuse.h, CONFUSE="yes", AC_MSG_WARN([*** libconfuse not found - LLDP MED Location functionality will be missing ***]))],)

if test "$CONFUSE" = "yes"; then
    MORELIBS="$MORELIBS -lconfuse"
    AC_DEFINE(USE_CONFUSE)
fi

CFLAGS="$GGDBFLAGS $CFLAGS"
AM_CPPFLAGS="$AM_CPPFLAGS "'-I${top_srcdir} -I${top_srcdir}/src'

AC_SUBST(MORELIBS)
AC_SUBST(DARWINLIBS)
AC_SUBST(CORELIBS)
AC_SUBST(MOREDEFS)
AC_SUBST(LDFLAGS)
AC_SUBST(AM_CPPFLAGS)
AC_SUBST(DARWININCS)
AC_SUBST(PREFIX)

AC_CONFIG_FILES([Makefile 
                 src/Makefile 
		 src/platform/Makefile])
AC_OUTPUT
