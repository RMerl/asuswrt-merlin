##
## Configure template file for Quagga.
## autoconf will generate configure script.
##
##  Copyright (c) 1996, 97, 98, 99, 2000 Kunihiro Ishiguro <kunihiro@zebra.org>
##  Portions Copyright (c) 2003 Paul Jakma <paul@dishone.st>
##
AC_PREREQ(2.53)

AC_INIT(Quagga, 0.99.24, [https://bugzilla.quagga.net])
CONFIG_ARGS="$*"
AC_SUBST(CONFIG_ARGS)
AC_CONFIG_SRCDIR(lib/zebra.h)
AC_CONFIG_MACRO_DIR([m4])

dnl -----------------------------------
dnl Get hostname and other information.
dnl -----------------------------------
AC_CANONICAL_BUILD()
AC_CANONICAL_HOST()
AC_CANONICAL_TARGET()

AM_INIT_AUTOMAKE(1.6)
AM_SILENT_RULES([yes])
AM_MAINTAINER_MODE
AC_CONFIG_HEADERS(config.h)

AC_PATH_PROG(PERL, perl)
AC_CHECK_PROG([GAWK],[gawk],[gawk],[not-in-PATH])
if test "x$GAWK" = "xnot-in-PATH" ; then
	AC_MSG_ERROR([GNU awk is required for lib/memtype.h made by memtypes.awk.
BSD awk complains: awk: gensub doesn't support backreferences (subst "\1") ])
fi
AC_ARG_VAR([GAWK],[GNU AWK])

dnl default is to match previous behavior
exampledir=${sysconfdir}
AC_ARG_ENABLE([exampledir],
	  AS_HELP_STRING([--enable-exampledir],
			 [specify alternate directory for examples]),
			 exampledir="$enableval",)
dnl XXX add --exampledir to autoconf standard directory list somehow
AC_SUBST(exampledir)

dnl default is to match previous behavior
pkgsrcrcdir=""
pkgsrcdir=""
AC_ARG_ENABLE([pkgsrcrcdir],
	  AS_HELP_STRING([--enable-pkgsrcrcdir],
			 [specify directory for rc.d scripts]),
			 pkgsrcrcdir="$enableval"; pkgsrcdir="pkgsrc",)
dnl XXX add --pkgsrcrcdir to autoconf standard directory list somehow
AC_SUBST(pkgsrcdir)
AC_SUBST(pkgsrcrcdir)

dnl ------------
dnl Check CFLAGS
dnl ------------
AC_ARG_WITH(cflags,
[  --with-cflags           Set CFLAGS for use in compilation.])
if test "x$with_cflags" != "x" ; then
  CFLAGS="$with_cflags" ; cflags_specified=yes ;
elif test -n "$CFLAGS" ; then
  cflags_specified=yes ;
fi

dnl --------------------
dnl Check CC and friends
dnl --------------------
AC_LANG([C])
AC_PROG_CC
AC_PROG_CPP
AM_PROG_CC_C_O
AC_PROG_EGREP

dnl autoconf 2.59 appears not to support AC_PROG_SED
dnl AC_PROG_SED
AC_CHECK_PROG([SED],[sed],[sed],[/bin/false])

dnl pdflatex and latexmk are needed to build HACKING.pdf
AC_CHECK_PROG([PDFLATEX],[pdflatex],[pdflatex],[/bin/false])
AC_CHECK_PROG([LATEXMK],[latexmk],[latexmk],[/bin/false])
if test "x$PDFLATEX" = "x/bin/false" -o "x$LATEXMK" = "x/bin/false"; then
	AC_MSG_WARN([Will not be able to make PDF versions of TeX documents])
else
	HAVE_LATEX=true
fi
AM_CONDITIONAL([HAVE_LATEX], [test "x$HAVE_LATEX" = "xtrue"])

dnl ------------------------------------------------------------------
dnl Intel compiler check. Although Intel tries really hard to make icc
dnl look like gcc, there are some differences. It's very verbose with
dnl -Wall and it doesn't support the individual -W options.
dnl ------------------------------------------------------------------
if test "x${GCC}" = "xyes" ; then
  COMPILER="GCC"
  AC_MSG_CHECKING([whether we are using the Intel compiler])
  AC_EGREP_CPP([^__INTEL_COMPILER], [__INTEL_COMPILER],
      [AC_MSG_RESULT([no])],
      [COMPILER="ICC"
       AC_MSG_RESULT([yes])]
  )
else
  AC_MSG_CHECKING([whether we are using SunPro compiler])
  AC_EGREP_CPP([^__SUNPRO_C.*0x5(7|8|9)], ["__SUNPRO_C" __SUNPRO_C],
      [AC_MSG_RESULT([no])],
      [COMPILER="SUNPRO"
       AC_MSG_RESULT([yes])]
  )
fi

dnl ---------------------------------------------
dnl If CLFAGS doesn\'t exist set default value
dnl AC_PROG_CC will have set minimal default
dnl already, eg "-O2 -g" for gcc, "-g" for others
dnl (Wall is gcc specific... have to make sure
dnl  gcc is being used before setting it)
dnl
dnl Intel icc 8.0 also sets __GNUC__, 
dnl but doesn't support all these fancy -W options.
dnl Intel compiler warnings we ignore:
dnl 279: controlling expression is constant.
dnl 869: parameter "xxx" was never referenced - to avoid massive warnings
dnl      about "self", "vty", "argc" and "argv" never referenced in DEFUN
dnl      macro.
dnl 981: operands are evaluated in unspecified order.
dnl
dnl Sun Studio 10 / SunPro 5.7 is also supported,
dnl so lets set some sane CFLAGS for it.
dnl ---------------------------------------------

AC_MSG_CHECKING([whether to set a default CFLAGS])
if test "x${cflags_specified}" = "x" ; then
  case ${COMPILER} in
    "ICC")
        CFLAGS="-Os -g -Wall"
        AC_MSG_RESULT([Intel default])
        ;;
    "GCC")
  	CFLAGS="-Os -fno-omit-frame-pointer -g -std=gnu99 -Wall"
  	CFLAGS="${CFLAGS} -Wsign-compare -Wpointer-arith"
  	CFLAGS="${CFLAGS} -Wbad-function-cast -Wwrite-strings"
  	CFLAGS="${CFLAGS} -Wmissing-prototypes -Wmissing-declarations"
  	CFLAGS="${CFLAGS} -Wchar-subscripts -Wcast-qual"
  	# TODO: conditionally addd -Wpacked if handled
  	AC_MSG_RESULT([gcc default])
  	;;
    "SUNPRO")
  	CFLAGS="-xO4 -v -g -xspace -xcode=pic32 -xstrconst -xc99"
  	AC_MSG_RESULT([SunPro default])
  	;;
    *)
        AC_MSG_RESULT([unknown compiler])
        ;;
  esac
else
  AC_MSG_RESULT([CFLAGS supplied by user])
fi

dnl --------------
dnl Check programs
dnl --------------
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_CHECK_TOOL(AR, ar)

dnl ---------------------------
dnl We, perhaps unfortunately,
dnl depend on GNU Make specific
dnl constructs.
dnl Give the user a warning if
dnl not GNU Make.
dnl ---------------------------
AC_CACHE_CHECK([if ${MAKE-make} is GNU make], [quagga_cv_gnu_make],
	[quagga_cv_gnu_make=no
	 if ${MAKE-make} --version 2>/dev/null | \
	 	grep '^GNU Make ' >/dev/null ; then
	 	quagga_cv_gnu_make=yes;
	 fi
	]
)

dnl -----------------
dnl System extensions
dnl -----------------
AC_GNU_SOURCE

dnl -------
dnl libtool
dnl -------
AC_PROG_LIBTOOL

dnl ----------------------
dnl Packages configuration
dnl ----------------------
AC_ARG_WITH(pkg-extra-version,
	AS_HELP_STRING([--with-pkg-extra-version=VER], [add extra version field, for packagers/distributions]),
	[EXTRAVERSION=$withval],)
AC_ARG_WITH(pkg-git-version,
	AS_HELP_STRING([--with-pkg-git-version], [add git information to MOTD and build version string]),
	[ test "x$withval" != "xno" && with_pkg_git_version="yes" ])
AC_ARG_ENABLE(vtysh,
  AS_HELP_STRING([--enable-vtysh], [include integrated vty shell for Quagga]))
AC_ARG_ENABLE(doc,
  AS_HELP_STRING([--disable-doc], [do not build docs]))
AC_ARG_ENABLE(zebra,
  AS_HELP_STRING([--disable-zebra], [do not build zebra daemon]))
AC_ARG_ENABLE(bgpd,
  AS_HELP_STRING([--disable-bgpd], [do not build bgpd]))
AC_ARG_ENABLE(ripd,
  AS_HELP_STRING([--disable-ripd], [do not build ripd]))
AC_ARG_ENABLE(ripngd,
  AS_HELP_STRING([--disable-ripngd], [do not build ripngd]))
AC_ARG_ENABLE(ospfd,
  AS_HELP_STRING([--disable-ospfd], [do not build ospfd]))
AC_ARG_ENABLE(ospf6d,
  AS_HELP_STRING([--disable-ospf6d], [do not build ospf6d]))
AC_ARG_ENABLE(babeld,
  AS_HELP_STRING([--disable-babeld], [do not build babeld]))
AC_ARG_ENABLE(watchquagga,
  AS_HELP_STRING([--disable-watchquagga], [do not build watchquagga]))
AC_ARG_ENABLE(isisd,
  AS_HELP_STRING([--disable-isisd], [do not build isisd]))
AC_ARG_ENABLE(pimd,
  AS_HELP_STRING([--enable-pimd], [build pimd]))
AC_ARG_ENABLE(bgp-announce,
  AS_HELP_STRING([--disable-bgp-announce,], [turn off BGP route announcement]))
AC_ARG_ENABLE(snmp,
  AS_HELP_STRING([--enable-snmp=ARG], [enable SNMP support (smux or agentx)]))
AC_ARG_WITH(libpam,
  AS_HELP_STRING([--with-libpam], [use libpam for PAM support in vtysh]))
AC_ARG_ENABLE(tcp-zebra,
  AS_HELP_STRING([--enable-tcp-zebra], [enable TCP/IP socket connection between zebra and protocol daemon]))
AC_ARG_ENABLE(opaque-lsa,
  AS_HELP_STRING([--disable-opaque-lsa],[do not build OSPF Opaque-LSA with OSPFAPI support (RFC2370)]))
AC_ARG_ENABLE(ospfapi,
  AS_HELP_STRING([--disable-ospfapi], [do not build OSPFAPI to access the OSPF LSA Database]))
AC_ARG_ENABLE(ospfclient,
  AS_HELP_STRING([--disable-ospfclient], [do not build OSPFAPI client for OSPFAPI,
                          (this is the default if --disable-ospfapi is set)]))
AC_ARG_ENABLE(ospf-te,
  AS_HELP_STRING([--disable-ospf-te],[disable Traffic Engineering Extension to OSPF]))
AC_ARG_ENABLE(multipath,
  AS_HELP_STRING([--enable-multipath=ARG], [enable multipath function, ARG must be digit]))
AC_ARG_ENABLE(user,
  AS_HELP_STRING([--enable-user=USER], [user to run Quagga suite as (default quagga)]))
AC_ARG_ENABLE(group,
  AS_HELP_STRING([--enable-group=GROUP], [group to run Quagga suite as (default quagga)]))
AC_ARG_ENABLE(vty_group,
  AS_HELP_STRING([--enable-vty-group=ARG], [set vty sockets to have specified group as owner]))
AC_ARG_ENABLE(configfile_mask,
  AS_HELP_STRING([--enable-configfile-mask=ARG], [set mask for config files]))
AC_ARG_ENABLE(logfile_mask,
  AS_HELP_STRING([--enable-logfile-mask=ARG], [set mask for log files]))

AC_ARG_ENABLE(rtadv,
  AS_HELP_STRING([--disable-rtadv], [disable IPV6 router advertisement feature]))
AC_ARG_ENABLE(irdp,
  AS_HELP_STRING([--enable-irdp], [enable IRDP server support in zebra]))
AC_ARG_ENABLE(isis_topology,
  AS_HELP_STRING([--enable-isis-topology], [enable IS-IS topology generator]))
AC_ARG_ENABLE(capabilities,
  AS_HELP_STRING([--disable-capabilities], [disable using POSIX capabilities]))
AC_ARG_ENABLE(rusage,
  AS_HELP_STRING([--disable-rusage], [disable using getrusage]))
AC_ARG_ENABLE(gcc_ultra_verbose,
  AS_HELP_STRING([--enable-gcc-ultra-verbose], [enable ultra verbose GCC warnings]))
AC_ARG_ENABLE(linux24_tcp_md5,
  AS_HELP_STRING([--enable-linux24-tcp-md5], [enable support for old, Linux-2.4 RFC2385 patch]))
AC_ARG_ENABLE(gcc-rdynamic,
  AS_HELP_STRING([--enable-gcc-rdynamic], [enable linking with -rdynamic for better backtraces (default if gcc)]))
AC_ARG_ENABLE(backtrace,
  AS_HELP_STRING([--disable-backtrace,], [disable crash backtraces (default autodetect)]))
AC_ARG_ENABLE(time-check,
  AS_HELP_STRING([--disable-time-check], [disable slow thread warning messages]))
AC_ARG_ENABLE(pcreposix,
  AS_HELP_STRING([--enable-pcreposix], [enable using PCRE Posix libs for regex functions]))
AC_ARG_ENABLE(fpm,
  AS_HELP_STRING([--enable-fpm], [enable Forwarding Plane Manager support]))

if test x"${enable_gcc_ultra_verbose}" = x"yes" ; then
  CFLAGS="${CFLAGS} -W -Wcast-qual -Wstrict-prototypes"
  CFLAGS="${CFLAGS} -Wmissing-declarations -Wmissing-noreturn"
  CFLAGS="${CFLAGS} -Wmissing-format-attribute -Wunreachable-code"
  CFLAGS="${CFLAGS} -Wpacked -Wpadded"
fi

if test x"${enable_gcc_rdynamic}" != x"no" ; then
  if test x"${enable_gcc_rdynamic}" = x"yes" -o x"$COMPILER" = x"GCC"; then
    LDFLAGS="${LDFLAGS} -rdynamic"
  fi
fi

if test x"${enable_time_check}" != x"no" ; then
  if test x"${enable_time_check}" = x"yes" -o x"${enable_time_check}" = x ; then
    AC_DEFINE(CONSUMED_TIME_CHECK,5000000,Consumed Time Check)
  else
    AC_DEFINE_UNQUOTED(CONSUMED_TIME_CHECK,$enable_time_check,Consumed Time Check)
  fi
fi

if test "${enable_fpm}" = "yes"; then
   AC_DEFINE(HAVE_FPM,,Forwarding Plane Manager support)
fi

if test "${enable_tcp_zebra}" = "yes"; then
  AC_DEFINE(HAVE_TCP_ZEBRA,,Use TCP for zebra communication)
fi

if test "${enable_opaque_lsa}" != "no"; then
  AC_DEFINE(HAVE_OPAQUE_LSA,,OSPF Opaque LSA)
fi

if test "${enable_ospf_te}" != "no"; then
  AC_DEFINE(HAVE_OPAQUE_LSA,,OSPF Opaque LSA)
  AC_DEFINE(HAVE_OSPF_TE,,OSPF TE)
fi

if test "${enable_linux24_tcp_md5}" = "yes"; then
  AC_DEFINE(HAVE_TCP_MD5_LINUX24,,Old Linux 2.4 TCP MD5 Signature Patch)
fi

AC_MSG_CHECKING(if zebra should be configurable to send Route Advertisements)
if test "${enable_rtadv}" != "no"; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(HAVE_RTADV,,Enable IPv6 Routing Advertisement support)
else
  AC_MSG_RESULT(no)
fi

if test "${enable_irdp}" = "yes"; then
  AC_DEFINE(HAVE_IRDP,, IRDP )
fi

if test "${enable_isisd}" != "no" && test "${enable_isis_topology}" = yes; then
  AC_DEFINE(TOPOLOGY_GENERATE,,Enable IS-IS topology generator code)
  ISIS_TOPOLOGY_INCLUDES="-I\$(srcdir)/topology"
  ISIS_TOPOLOGY_DIR="topology"
  ISIS_TOPOLOGY_LIB="./topology/libtopology.a"
fi

AC_SUBST(ISIS_TOPOLOGY_INCLUDES)
AC_SUBST(ISIS_TOPOLOGY_DIR)
AC_SUBST(ISIS_TOPOLOGY_LIB)

if test "${enable_user}" = "yes" || test x"${enable_user}" = x""; then
  enable_user="quagga"
elif test "${enable_user}" = "no"; then
  enable_user="root"
fi

if test "${enable_group}" = "yes" || test x"${enable_group}" = x""; then
  enable_group="quagga"
elif test "${enable_group}" = "no"; then
  enable_group="root"
fi

if test x"${enable_vty_group}" = x"yes" ; then
  AC_MSG_ERROR([--enable-vty-group requires a group as argument, not yes])
elif test x"${enable_vty_group}" != x""; then
  if test x"${enable_vty_group}" != x"no"; then
    AC_DEFINE_UNQUOTED(VTY_GROUP, "${enable_vty_group}", VTY Sockets Group)
  fi
fi
AC_SUBST([enable_user])
AC_SUBST([enable_group])
AC_SUBST([enable_vty_group])
AC_DEFINE_UNQUOTED(QUAGGA_USER, "${enable_user}", Quagga User)
AC_DEFINE_UNQUOTED(QUAGGA_GROUP, "${enable_group}", Quagga Group)

enable_configfile_mask=${enable_configfile_mask:-0600}
AC_DEFINE_UNQUOTED(CONFIGFILE_MASK, ${enable_configfile_mask}, Mask for config files)

enable_logfile_mask=${enable_logfile_mask:-0600}
AC_DEFINE_UNQUOTED(LOGFILE_MASK, ${enable_logfile_mask}, Mask for log files)

MULTIPATH_NUM=1

case "${enable_multipath}" in
  [[0-9]|[1-9][0-9]])
    MULTIPATH_NUM="${enable_multipath}"
    ;;
  "")
    ;;
  *)           
    AC_MSG_FAILURE([Please specify digit to enable multipath ARG])
    ;;
esac

AC_SUBST(MULTIPATH_NUM)

dnl -----------------------------------
dnl Add extra version string to package
dnl name, string and version fields.
dnl -----------------------------------
if test "x${EXTRAVERSION}" != "x" ; then
  VERSION="${VERSION}${EXTRAVERSION}"
  PACKAGE_VERSION="${PACKAGE_VERSION}${EXTRAVERSION}"
  PACKAGE_STRING="${PACKAGE_STRING}${EXTRAVERSION}"
fi

if test "x$with_pkg_git_version" = "xyes"; then
	if test -d "${srcdir}/.git"; then
		AC_DEFINE(GIT_VERSION, [1], [include git version info])
	else	with_pkg_git_version="no"
		AC_MSG_WARN([--with-pkg-git-version given, but this is not a git checkout])
	fi
fi
AM_CONDITIONAL([GIT_VERSION], [test "x$with_pkg_git_version" = "xyes"])

dnl ------------------------------------
dnl Check C keywords and standard  types
dnl ------------------------------------
AC_C_CONST
AC_C_INLINE
AC_C_RESTRICT
AC_C_VOLATILE
AC_HEADER_STDC
AC_HEADER_TIME
AC_HEADER_SYS_WAIT
dnl AC_TYPE_PID_T
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_SIZE_T
AC_TYPE_SIGNAL
AC_STRUCT_TM

dnl -------------------------
dnl Check other header files.
dnl -------------------------
AC_CHECK_HEADERS([stropts.h sys/ksym.h sys/times.h sys/select.h \
	sys/types.h linux/version.h netdb.h asm/types.h \
	sys/cdefs.h sys/param.h limits.h signal.h \
	sys/socket.h netinet/in.h time.h sys/time.h])

dnl Utility macro to avoid retyping includes all the time
m4_define([QUAGGA_INCLUDES],
[#ifdef SUNOS_5
#define _XPG4_2
#define __EXTENSIONS__
#endif
#include <stdio.h>
#if STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
/* sys/conf.h depends on param.h on FBSD at least */
#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
/* Required for MAXSIG */
#if HAVE_SIGNAL_H
# include <signal.h>
#endif
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef __APPLE__
# define __APPLE_USE_RFC_3542
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif /* TIME_WITH_SYS_TIME */
])dnl

dnl HAVE_NET_IF_H must be discovered by the time the longer AC_CHECK_HEADERS
dnl round below execution begins, otherwise it doesn't properly detect
dnl HAVE_NETINET6_IN6_VAR_H, HAVE_NET_IF_VAR_H and HAVE_STRUCT_IN6_ALIASREQ
dnl on FreeBSD (BZ#408).

AC_CHECK_HEADERS([net/if.h], [], [], QUAGGA_INCLUDES)

m4_define([QUAGGA_INCLUDES],
QUAGGA_INCLUDES
[#if HAVE_NET_IF_H
# include <net/if.h>
#endif
])dnl

dnl Same applies for HAVE_NET_IF_VAR_H, which HAVE_NETINET6_ND6_H and
dnl HAVE_NETINET_IN_VAR_H depend upon. But if_var.h depends on if.h, hence
dnl an additional round for it.

AC_CHECK_HEADERS([net/if_var.h], [], [], QUAGGA_INCLUDES)

m4_define([QUAGGA_INCLUDES],
QUAGGA_INCLUDES
[#if HAVE_NET_IF_VAR_H
# include <net/if_var.h>
#endif
])dnl

AC_CHECK_HEADERS([sys/un.h netinet/in_systm.h netinet/in_var.h \
	net/if_dl.h net/netopt.h net/route.h \
	inet/nd.h arpa/inet.h netinet/ip_icmp.h \
	fcntl.h stddef.h sys/ioctl.h syslog.h wchar.h wctype.h \
	sys/sysctl.h sys/sockio.h kvm.h sys/conf.h],
	[], [], QUAGGA_INCLUDES)

AC_CHECK_HEADERS([ucontext.h], [], [],
[#ifndef __USE_GNU
#define __USE_GNU
#endif /* __USE_GNU */
QUAGGA_INCLUDES
])

m4_define([UCONTEXT_INCLUDES],
[#include <ucontext.h>])dnl

AC_CHECK_MEMBERS([ucontext_t.uc_mcontext.uc_regs],
  [], [], [UCONTEXT_INCLUDES])
AC_CHECK_MEMBERS([ucontext_t.uc_mcontext.regs],
  [AC_CHECK_MEMBERS([ucontext_t.uc_mcontext.regs.nip],
   [], [], [UCONTEXT_INCLUDES])],
  [], [UCONTEXT_INCLUDES])
AC_CHECK_MEMBERS([ucontext_t.uc_mcontext.gregs],
  [], [], [UCONTEXT_INCLUDES])

m4_define([QUAGGA_INCLUDES],
QUAGGA_INCLUDES
[#if HAVE_SYS_UN_H
# include <sys/un.h>
#endif
#if HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#if HAVE_NETINET_IN_VAR_H
# include <netinet/in_var.h>
#endif
#if HAVE_NET_IF_DL_H
# include <net/if_dl.h>
#endif
#if HAVE_NET_NETOPT_H
# include <net/netopt.h>
#endif
#if HAVE_NET_ROUTE_H
# include <net/route.h>
#endif
#if HAVE_INET_ND_H
# include <inet/nd.h>
#endif
#if HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
/* Required for IDRP */
#if HAVE_NETINET_IP_ICMP_H
# include <netinet/ip_icmp.h>
#endif
])dnl

dnl V6 headers are checked below, after we check for v6

dnl Some systems (Solaris 2.x) require libnsl (Network Services Library)
case "$host" in
  [*-sunos5.[6-7]*] | [*-solaris2.[6-7]*])
      opsys=sol2-6
      AC_DEFINE(SUNOS_56, 1, SunOS 5.6 to 5.7)
      AC_DEFINE(SUNOS_5, 1, SunOS 5)
      AC_CHECK_LIB(xnet, main)
      CURSES=-lcurses
      SOLARIS="solaris"
  ;;
  [*-sunos5.[8-9]] \
  | [*-sunos5.1[0-9]] \
  | [*-sunos5.1[0-9].[0-9]] \
  | [*-solaris2.[8-9]] \
  | [*-solaris2.1[0-9]] \
  | [*-solaris2.1[0-9].[0-9]])
      opsys=sol8
      AC_DEFINE(SUNOS_59, 1, [SunOS 5.8 up])
      AC_DEFINE(SUNOS_5, 1, [SunOS 5])
      AC_CHECK_LIB(socket, main)
      AC_CHECK_LIB(nsl, main)
      AC_CHECK_LIB(umem, main)
      AC_CHECK_FUNCS([printstack],
      	[AC_DEFINE([HAVE_PRINTSTACK],1,[Solaris printstack])
      	 AC_DEFINE([HAVE_STACK_TRACE],1,[Stack symbols decode functionality])
      	])
      CURSES=-lcurses
      SOLARIS="solaris"
  ;;
  *-sunos5* | *-solaris2*)
      AC_DEFINE(SUNOS_5,,SunOS 5, Unknown SunOS)
      AC_CHECK_LIB(socket, main)
      AC_CHECK_LIB(nsl, main)
      CURSES=-lcurses
      SOLARIS="solaris"
  ;;
  *-linux*)
      opsys=gnu-linux
      AC_DEFINE(GNU_LINUX,,GNU Linux)
  ;;
  *-openbsd*)
      opsys=openbsd
      AC_DEFINE(OPEN_BSD,,OpenBSD)
  ;;
esac

AC_SYS_LARGEFILE

dnl ---------------------
dnl Integrated VTY option
dnl ---------------------
case "${enable_vtysh}" in
  "yes") VTYSH="vtysh";
         AC_DEFINE(VTYSH,,VTY shell)
dnl	 Vtysh uses libreadline, which looks for termcap functions at
dnl	 configure time.  We follow readlines search order.
dnl	 The required procedures are in libtermcap on NetBSD, in
dnl	 [TODO] on Linux, and in [TODO] on Solaris.
	 AC_CHECK_LIB(termcap, tputs, LIBREADLINE="$LIBREADLINE -ltermcap",
	   [AC_CHECK_LIB(tinfo, tputs, LIBREADLINE="$LIBREADLINE -ltinfo",
	     [AC_CHECK_LIB(curses, tputs, LIBREADLINE="$LIBREADLINE -lcurses",
	       [AC_CHECK_LIB(ncurses, tputs, 
	                     LIBREADLINE="$LIBREADLINE -lncurses")]
	      )]
	    )]
	  )
         AC_CHECK_LIB(readline, main, LIBREADLINE="$LIBREADLINE -lreadline",,
                      "$LIBREADLINE")
         if test $ac_cv_lib_readline_main = no; then
           AC_MSG_ERROR([vtysh needs libreadline but was not found and usable on your system.])
         fi
	 AC_CHECK_HEADER(readline/history.h)
	 if test $ac_cv_header_readline_history_h = no;then
           AC_MSG_ERROR([readline is too old to have readline/history.h, please update to the latest readline library.])
	 fi
	 AC_CHECK_LIB(readline, rl_completion_matches, 
	              LIBREADLINE="$LIBREADLINE",, "$LIBREADLINE")
         if test $ac_cv_lib_readline_rl_completion_matches = no; then
           AC_DEFINE(rl_completion_matches,completion_matches,Old readline)
	 fi
	 ;;
  "no" ) VTYSH="";;
  *    ) ;;
esac
AC_SUBST(LIBREADLINE)
AM_CONDITIONAL(VTYSH, test "x$VTYSH" = "xvtysh")

dnl ----------
dnl PAM module
dnl
dnl Quagga detects the PAM library it is built against by checking for a
dnl functional pam_misc.h (Linux-PAM) or openpam.h (OpenPAM) header. pam_misc.h
dnl is known to #include pam_appl.h, the standard header of a PAM library, and
dnl openpam.h doesn't do that, although depends on the header too. Hence a
dnl little assistance to AC_CHECK_HEADER is necessary for the proper detection
dnl of OpenPAM.
dnl ----------
if test "$with_libpam" = "yes"; then
  AC_CHECK_HEADER([security/pam_misc.h],
    [AC_DEFINE(HAVE_PAM_MISC_H,,Have pam_misc.h)
     AC_DEFINE(PAM_CONV_FUNC,misc_conv,Have misc_conv)
     pam_conv_func="misc_conv"
    ],
    [], QUAGGA_INCLUDES)
  AC_CHECK_HEADER([security/openpam.h],
    [AC_DEFINE(HAVE_OPENPAM_H,,Have openpam.h)
     AC_DEFINE(PAM_CONV_FUNC,openpam_ttyconv,Have openpam_ttyconv)
     pam_conv_func="openpam_ttyconv"
    ],
    [], QUAGGA_INCLUDES[#include <security/pam_appl.h>])
  if test -z "$ac_cv_header_security_pam_misc_h$ac_cv_header_security_openpam_h" ; then
    AC_MSG_WARN([*** pam support will not be built ***])
    with_libpam="no"
  fi
fi

if test "$with_libpam" = "yes"; then
dnl took this test from proftpds configure.in and suited to our needs
dnl -------------------------------------------------------------------------
dnl
dnl This next check looks funky due to a linker problem with some versions
dnl of the PAM library.  Prior to 0.72 release, the Linux PAM shared library
dnl omitted requiring libdl linking information. PAM-0.72 or better ships
dnl with RedHat 6.2 and Debian 2.2 or better.
AC_CHECK_LIB(pam, pam_start,
  [AC_CHECK_LIB(pam, $pam_conv_func,
    [AC_DEFINE(USE_PAM,,Use PAM for authentication)
     LIBPAM="-lpam"],
    [AC_DEFINE(USE_PAM,,Use PAM for authentication)
     LIBPAM="-lpam -lpam_misc"]
    )
  ],

  [AC_CHECK_LIB(pam, pam_end,
    [AC_CHECK_LIB(pam, $pam_conv_func,
      [AC_DEFINE(USE_PAM,,Use PAM for authentication)
       LIBPAM="-lpam -ldl"],
      [AC_DEFINE(USE_PAM,,Use PAM for authentication)
       LIBPAM="-lpam -ldl -lpam_misc"]
     )
  ],AC_MSG_WARN([*** pam support will not be built ***]),
  [-ldl])
  ]
)
fi
AC_SUBST(LIBPAM)

dnl -------------------------------
dnl Endian-ness check
dnl -------------------------------
AC_WORDS_BIGENDIAN

dnl -------------------------------
dnl check the size in byte of the C
dnl -------------------------------
dnl AC_CHECK_SIZEOF(char)
dnl AC_CHECK_SIZEOF(int)
dnl AC_CHECK_SIZEOF(short)
dnl AC_CHECK_SIZEOF(long)

dnl ----------------------------
dnl check existance of functions
dnl ----------------------------
AC_FUNC_CHOWN
AC_FUNC_FNMATCH
AC_FUNC_FORK
AC_FUNC_MEMCMP
AC_FUNC_MKTIME
AC_FUNC_STRFTIME
AC_FUNC_STAT
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STRFTIME
dnl Avoid AC_FUNC_STRNLEN because it pulls in AC_SYSTEM_EXTENSIONS which
dnl can lead to strange side effects.  So we just check for strnlen
dnl directly, see below.
dnl AC_FUNC_STRNLENdnl
AC_FUNC_VPRINTF

dnl -------------------------------
dnl bgpd needs pow() and hence libm
dnl -------------------------------
TMPLIBS="$LIBS"
AC_CHECK_HEADER([math.h],
  [AC_CHECK_LIB([m], [pow],
    [LIBM="-lm"
     LIBS="$LIBS $LIBM"
     AC_DEFINE(HAVE_LIBM,, Have libm)
     AC_CHECK_FUNCS(pow,[],[LIBM=""])
    ])
])
if test x"$LIBM" = x ; then
  AC_MSG_WARN([Unable to find working pow function - bgpd may not link])
fi
LIBS="$TMPLIBS"
AC_SUBST(LIBM)

dnl ---------------
dnl other functions
dnl ---------------
AC_CHECK_FUNCS([dup2 ftruncate getcwd gethostbyname getpagesize gettimeofday \
	inet_ntoa inet_aton strnlen \
	memchr memmove memset select socket \
	strcasecmp strchr strcspn strdup strerror \
	strncasecmp strndup strrchr strspn strstr \
	strtol strtoul strlcat strlcpy \
	daemon snprintf vsnprintf \
	if_nametoindex if_indextoname getifaddrs \
	uname fcntl])

AC_CHECK_FUNCS(setproctitle, ,
  [AC_CHECK_LIB(util, setproctitle, 
     [LIBS="$LIBS -lutil"
      AC_DEFINE(HAVE_SETPROCTITLE,, Have setproctitle)
     ]
   )
  ]
)

dnl ------------------------------------
dnl Determine routing get and set method
dnl ------------------------------------
AC_MSG_CHECKING(zebra between kernel interface method)
if test x"$opsys" = x"gnu-linux"; then
  AC_MSG_RESULT(netlink)
  RT_METHOD=rt_netlink.o
  AC_DEFINE(HAVE_NETLINK,,netlink)
  netlink=yes
else
  AC_MSG_RESULT(Route socket)
  KERNEL_METHOD="kernel_socket.o"
  RT_METHOD="rt_socket.o"
fi
AC_SUBST(RT_METHOD)
AC_SUBST(KERNEL_METHOD)
AM_CONDITIONAL([HAVE_NETLINK], [test "x$netlink" = "xyes"])

dnl --------------------------
dnl Determine IS-IS I/O method
dnl --------------------------
AC_DEFINE(ISIS_METHOD_PFPACKET,	1, [ constant value for isis method pfpacket ])
AC_DEFINE(ISIS_METHOD_DLPI,	2, [ constant value for isis method dlpi ])
AC_DEFINE(ISIS_METHOD_BPF,	3, [ constant value for isis method bpf ])
AC_CHECK_HEADER(net/bpf.h)
AC_CHECK_HEADER(sys/dlpi.h)
AC_MSG_CHECKING(zebra IS-IS I/O method)
if test x"$opsys" = x"gnu-linux"; then
  AC_MSG_RESULT(pfpacket)
  ISIS_METHOD_MACRO="ISIS_METHOD_PFPACKET"
elif test x"$opsys" = x"sol2-6" -o x"$opsys" = x"sol8"; then
  AC_MSG_RESULT(DLPI)
  ISIS_METHOD_MACRO="ISIS_METHOD_DLPI"
else
  if test $ac_cv_header_net_bpf_h = no; then
    if test $ac_cv_header_sys_dlpi_h = no; then
      AC_MSG_RESULT(none)
      AC_MSG_WARN([*** IS-IS support will not be built ***])
      ISISD=""
    else
      AC_MSG_RESULT(DLPI)
    fi
    ISIS_METHOD_MACRO="ISIS_METHOD_DLPI"
  else
    AC_MSG_RESULT(BPF)
    ISIS_METHOD_MACRO="ISIS_METHOD_BPF"
  fi
fi
AC_DEFINE_UNQUOTED(ISIS_METHOD, $ISIS_METHOD_MACRO, [ selected method for isis, == one of the constants ])

dnl ------------------------------------
dnl check for broken CMSG_FIRSTHDR macro
dnl ------------------------------------
AC_MSG_CHECKING(for broken CMSG_FIRSTHDR)
AC_RUN_IFELSE([AC_LANG_SOURCE([[
#ifdef SUNOS_5
#define _XPG4_2
#define __EXTENSIONS__
#endif
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

main()
{
  struct msghdr msg;
  char buf[4];

  msg.msg_control = buf;
  msg.msg_controllen = 0;

  if (CMSG_FIRSTHDR(&msg) != NULL)
    exit(0);
  exit (1);
}]])],[AC_MSG_RESULT(yes - using workaround) AC_DEFINE(HAVE_BROKEN_CMSG_FIRSTHDR,,Broken CMSG_FIRSTHDR)],
[AC_MSG_RESULT(no)],[AC_MSG_RESULT(no)])

dnl ------------------------------
dnl check kernel route read method
dnl ------------------------------
AC_CACHE_CHECK([route read method], [quagga_cv_rtread_method],
[if test "x$netlink" = xyes; then
  quagga_cv_rtread_method="netlink"
else
for quagga_cv_rtread_method in /dev/ip /dev/null;
do
  test x`ls $quagga_cv_rtread_method 2>/dev/null` = x"$quagga_cv_rtread_method" && break
done
case $quagga_cv_rtread_method in
  "/dev/ip")
                     case "$host" in
                       *-freebsd*)    quagga_cv_rtread_method="sysctl";;
                       *)             quagga_cv_rtread_method="getmsg";;
                     esac;;
	*)
                     quagga_cv_rtread_method="sysctl";;
esac
fi])
RTREAD_METHOD=rtread_${quagga_cv_rtread_method}.o
AC_SUBST(RTREAD_METHOD)

dnl -----------------------------
dnl check interface lookup method
dnl -----------------------------
IOCTL_METHOD=ioctl.o
AC_MSG_CHECKING(interface looking up method)
if test "$netlink" = yes; then
  AC_MSG_RESULT(netlink)
  IF_METHOD=if_netlink.o
elif test "$opsys" = "sol2-6";then
  AC_MSG_RESULT(Solaris GIF)
  IF_METHOD=if_ioctl.o
elif test "$opsys" = "sol8";then
  AC_MSG_RESULT(Solaris GLIF)
  IF_METHOD=if_ioctl_solaris.o
  IOCTL_METHOD=ioctl_solaris.o
elif test "$opsys" = "openbsd";then
  AC_MSG_RESULT(openbsd)
  IF_METHOD=if_ioctl.o
elif grep NET_RT_IFLIST /usr/include/sys/socket.h >/dev/null 2>&1; then
  AC_MSG_RESULT(sysctl)
    IF_METHOD=if_sysctl.o
    AC_DEFINE(HAVE_NET_RT_IFLIST,,NET_RT_IFLIST)
else
    AC_MSG_RESULT(ioctl)
    IF_METHOD=if_ioctl.o
fi
AC_SUBST(IF_METHOD)
AC_SUBST(IOCTL_METHOD)

dnl ---------------------------------------------------------------
dnl figure out how to specify an interface in multicast sockets API
dnl ---------------------------------------------------------------
AC_CHECK_MEMBERS([struct ip_mreqn.imr_ifindex], [], [], QUAGGA_INCLUDES)

AC_MSG_CHECKING([for BSD struct ip_mreq hack])
AC_TRY_COMPILE([#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif],[#if (defined(__FreeBSD__) && ((__FreeBSD_version >= 500022 && __FreeBSD_version < 700000) || (__FreeBSD_version < 500000 && __FreeBSD_version >= 440000))) || (defined(__NetBSD__) && defined(__NetBSD_Version__) && __NetBSD_Version__ >= 106010000) || defined(__OpenBSD__) || defined(__APPLE__) || defined(__DragonFly__) || defined(__sun)
  return (0);
#else
  #error No support for BSD struct ip_mreq hack detected
#endif],[AC_MSG_RESULT(yes)
AC_DEFINE(HAVE_BSD_STRUCT_IP_MREQ_HACK,,[Can pass ifindex in struct ip_mreq])],
AC_MSG_RESULT(no))

AC_MSG_CHECKING([for RFC3678 protocol-independed API])
AC_TRY_COMPILE([
#include <sys/types.h>
#include <netinet/in.h>
], [struct group_req gr; int sock; setsockopt(sock, IPPROTO_IP, MCAST_JOIN_GROUP, (void*)&gr, sizeof(gr));
], [AC_MSG_RESULT(yes)
AC_DEFINE(HAVE_RFC3678,1,[Have RFC3678 protocol-independed API])],
AC_MSG_RESULT(no))

dnl ---------------------------------------------------------------
dnl figure out how to check link-state
dnl ---------------------------------------------------------------
AC_CHECK_HEADER([net/if.h],
  [AC_CHECK_HEADER( [net/if_media.h],
    [m4_define([LINK_DETECT_INCLUDES],
      QUAGGA_INCLUDES
      [#include <net/if_media.h>
    ])
    AC_CHECK_MEMBERS( [struct ifmediareq.ifm_status],
      AC_DEFINE(HAVE_BSD_LINK_DETECT,,[BSD link-detect]),
      [], LINK_DETECT_INCLUDES)], 
    [],
    QUAGGA_INCLUDES)], 
  [], QUAGGA_INCLUDES )

dnl ---------------------------------------------------------------
dnl Additional, newer way to check link-state using ifi_link_state.
dnl Not available in all BSD's when ifmediareq available
dnl ---------------------------------------------------------------
AC_CHECK_HEADER([net/if.h],
    AC_CHECK_MEMBERS([struct if_data.ifi_link_state],
      AC_DEFINE(HAVE_BSD_IFI_LINK_STATE,,[BSD ifi_link_state available]),
      [], QUAGGA_INCLUDES),
    ,)

dnl ------------------------
dnl TCP_MD5SIG socket option
dnl ------------------------

AC_CHECK_HEADER([netinet/tcp.h], 
   [m4_define([MD5_INCLUDES],
      QUAGGA_INCLUDES
      [#include <netinet/tcp.h>
    ])
    AC_CHECK_DECLS([TCP_MD5SIG], [], [], MD5_INCLUDES)],
   [],
   QUAGGA_INCLUDES)
if test $ac_cv_have_decl_TCP_MD5SIG = no; then
  AC_CHECK_HEADER([linux/tcp.h],
       [m4_define([MD5_INCLUDES],
          QUAGGA_INCLUDES
          [#include <linux/tcp.h>
        ])
       AC_CHECK_DECLS([TCP_MD5SIG], [], [], MD5_INCLUDES)])
fi

dnl -----------------------------
dnl check ipforward detect method
dnl -----------------------------
AC_CACHE_CHECK([ipforward method], [quagga_cv_ipforward_method],
[if test x$cross_compiling = xyes; then
  if test x"$opsys" = x"gnu-linux"; then
    quagga_cv_ipforward_method=/proc/net/snmp
  else
    quagga_cv_ipforward_method=/dev/ip
  fi
else
  for quagga_cv_ipforward_method in /proc/net/snmp /dev/ip /dev/null;
  do
    test x`ls $quagga_cv_ipforward_method 2>/dev/null` = x"$quagga_cv_ipforward_method" && break
  done
fi
case $quagga_cv_ipforward_method in
  "/proc/net/snmp")  quagga_cv_ipforward_method="proc";;
  "/dev/ip")         
                     case "$host" in
                       *-freebsd*)    quagga_cv_ipforward_method="sysctl";;
                       *)             quagga_cv_ipforward_method="solaris";;
                     esac;;
  *)                 quagga_cv_ipforward_method="sysctl";;
esac])
IPFORWARD=ipforward_${quagga_cv_ipforward_method}.o
AC_SUBST(IPFORWARD)

AC_CHECK_FUNCS(getaddrinfo, [have_getaddrinfo=yes], [have_getaddrinfo=no])

dnl ----------
dnl IPv6 check
dnl ----------
AC_MSG_CHECKING(whether does this OS have IPv6 stack)
dnl ---------
dnl KAME IPv6
dnl ---------
  if grep WIDE /usr/include/netinet6/in6.h >/dev/null 2>&1; then
    AC_DEFINE(KAME,1,KAME IPv6)
    AC_MSG_RESULT(KAME)
dnl ------------------------------------
dnl Solaris 9, 10 and potentially higher
dnl ------------------------------------
  elif test x"$opsys" = x"sol8"; then
    AC_DEFINE(SOLARIS_IPV6, 1, Solaris IPv6)
    AC_MSG_RESULT(Solaris IPv6)
dnl ----------
dnl Linux IPv6
dnl ----------
  elif test x"$opsys" = x"gnu-linux"; then
    AC_DEFINE(LINUX_IPV6,1,Linux IPv6 stack)
    AC_MSG_RESULT(Linux IPv6)
  else
    AC_MSG_ERROR([Failed to detect IPv6 stack])
  fi

dnl this is unconditial, for compatibility
AC_DEFINE(HAVE_IPV6,1,IPv6)

dnl ------------------
dnl IPv6 header checks
dnl ------------------
AC_CHECK_HEADERS([netinet6/in6.h netinet/in6_var.h netinet/icmp6.h \
	netinet6/in6_var.h netinet6/nd6.h], [], [],
	QUAGGA_INCLUDES)

m4_define([QUAGGA_INCLUDES],dnl
QUAGGA_INCLUDES
[#if HAVE_NETINET6_IN6_H
#include <netinet6/in6.h>
#endif
#if HAVE_NETINET_IN6_VAR_H
#include <netinet/in6_var.h>
#endif
#if HAVE_NETINET_ICMP6_H
# include <netinet/icmp6.h>
#endif
#if HAVE_NETINET6_IN6_VAR_H
# include <netinet6/in6_var.h>
#endif
#if HAVE_NETINET6_ND6_H
# include <netinet6/nd6.h>
#endif
])dnl

dnl disable doc check
if test "${enable_doc}" = "no";then
  DOC=""
else
  DOC="doc"
fi

dnl --------------------
dnl Daemon disable check
dnl --------------------
if test "${enable_zebra}" = "no";then
  ZEBRA=""
else
  ZEBRA="zebra"
fi
AM_CONDITIONAL(ZEBRA, test "x$ZEBRA" = "xzebra")

if test "${enable_bgpd}" = "no";then
  BGPD=""
else
  BGPD="bgpd"
fi
AM_CONDITIONAL(BGPD, test "x$BGPD" = "xbgpd")

if test "${enable_ripd}" = "no";then
  RIPD=""
else
  RIPD="ripd"
fi
AM_CONDITIONAL(RIPD, test "x$RIPD" = "xripd")

if test "${enable_ospfd}" = "no";then
  OSPFD=""
else
  OSPFD="ospfd"
fi
AM_CONDITIONAL(OSPFD, test "x$OSPFD" = "xospfd")

if test "${enable_babeld}" = "no";then
  BABELD=""
else
  BABELD="babeld"
fi
AM_CONDITIONAL(BABELD, test "x$BABELD" = "xbabeld")

if test "${enable_watchquagga}" = "no";then
  WATCHQUAGGA=""
else
  WATCHQUAGGA="watchquagga"
fi
AM_CONDITIONAL(WATCHQUAGGA, test "x$WATCHQUAGGA" = "xwatchquagga")

OSPFCLIENT=""
if test "${enable_opaque_lsa}" != "no"; then
  if test "${enable_ospfapi}" != "no";then
    AC_DEFINE(SUPPORT_OSPF_API,,OSPFAPI)

    if test "${enable_ospfclient}" != "no";then
      OSPFCLIENT="ospfclient"
    fi
  fi

fi
AM_CONDITIONAL(OSPFCLIENT, test "x$OSPFCLIENT" = "xospfclient")

case "${enable_ripngd}" in
  "no" ) RIPNGD="";;
  *    ) RIPNGD="ripngd";;
esac
AM_CONDITIONAL(RIPNGD, test "x$RIPNGD" = "xripngd")

case "${enable_ospf6d}" in
  "no" ) OSPF6D="";;
  *    ) OSPF6D="ospf6d";;
esac
AM_CONDITIONAL(OSPF6D, test "x$OSPF6D" = "xospf6d")

case "${enable_isisd}" in
  "no" ) ISISD="";;
  *    ) ISISD="isisd";;
esac
AM_CONDITIONAL(ISISD, test "x$ISISD" = "xisisd")

case "${enable_pimd}" in
  "yes") PIMD="pimd";;
  "no" ) PIMD="";;
  *    ) ;;
esac
AM_CONDITIONAL(PIMD, test "x$PIMD" = "xpimd")

if test "${enable_bgp_announce}" = "no";then
  AC_DEFINE(DISABLE_BGP_ANNOUNCE,1,Disable BGP installation to zebra)
else
  AC_DEFINE(DISABLE_BGP_ANNOUNCE,0,Disable BGP installation to zebra)
fi

AC_SUBST(DOC)
AC_SUBST(ZEBRA)
AC_SUBST(BGPD)
AC_SUBST(RIPD)
AC_SUBST(RIPNGD)
AC_SUBST(OSPFD)
AC_SUBST(OSPF6D)
AC_SUBST(BABELD)
AC_SUBST(WATCHQUAGGA)
AC_SUBST(ISISD)
AC_SUBST(PIMD)
AC_SUBST(SOLARIS)
AC_SUBST(VTYSH)
AC_SUBST(CURSES)
AC_SUBST(OSPFCLIENT)
AC_SUBST(OSPFAPI)
AC_CHECK_LIB(c, inet_ntop, [AC_DEFINE(HAVE_INET_NTOP,,inet_ntop)])
AC_CHECK_LIB(c, inet_pton, [AC_DEFINE(HAVE_INET_PTON,,inet_pton)])
AC_CHECK_LIB(crypt, crypt)
AC_CHECK_LIB(resolv, res_init)

dnl ---------------------------
dnl check system has PCRE regexp
dnl ---------------------------
if test "x$enable_pcreposix" = "xyes"; then
  AC_CHECK_LIB(pcreposix, pcreposix_regexec, ,[enable_pcreposix=no
  AC_MSG_WARN([*** falling back to other regex library ***]) ])
fi

if test "x$enable_pcreposix" != "xyes"; then
dnl ---------------------------
dnl check system has GNU regexp
dnl ---------------------------
AC_MSG_CHECKING(whether system has GNU regex)
AC_CHECK_LIB(c, regexec,
[AC_DEFINE(HAVE_GNU_REGEX,,GNU regexp library)
 LIB_REGEX=""],
[LIB_REGEX="regex.o"])
fi
AC_SUBST(HAVE_LIBPCREPOSIX)
AC_SUBST(LIB_REGEX)

dnl ------------------
dnl check Net-SNMP library
dnl ------------------
if test "${enable_snmp}" != ""; then
   AC_PATH_TOOL([NETSNMP_CONFIG], [net-snmp-config], [no])
   if test x"$NETSNMP_CONFIG" = x"no"; then
      AC_MSG_ERROR([--enable-snmp given but unable to find net-snmp-config])
   fi
   LIBS="$LIBS `${NETSNMP_CONFIG} --agent-libs`"
   CFLAGS="`${NETSNMP_CONFIG} --base-cflags` $CFLAGS"
   AC_MSG_CHECKING([whether we can link to Net-SNMP])
   AC_LINK_IFELSE([AC_LANG_PROGRAM([
int main(void);
],
[
{
  return 0;
}
])],[AC_MSG_RESULT(yes)],[
     AC_MSG_RESULT(no)
     AC_MSG_ERROR([--enable-snmp given but not usable])])
   AC_DEFINE(HAVE_SNMP,,SNMP)
   case "${enable_snmp}" in
     yes)
      SNMP_METHOD=agentx
      ;;
     smux|agentx)
      SNMP_METHOD="${enable_snmp}"
      ;;
     *)
      AC_MSG_ERROR([--enable-snmp given with an unknown method (${enable_snmp}). Use smux or agentx])
      ;;
   esac
   AH_TEMPLATE([SNMP_SMUX], [Use SNMP SMUX to interface with snmpd])
   AH_TEMPLATE([SNMP_AGENTX], [Use SNMP AgentX to interface with snmpd])
   AC_DEFINE_UNQUOTED(AS_TR_CPP(SNMP_${SNMP_METHOD}),,SNMP method to interface with snmpd)
fi

dnl ---------------------------
dnl sockaddr and netinet checks
dnl ---------------------------
AC_CHECK_TYPES([struct sockaddr, struct sockaddr_in,
	struct sockaddr_in6, struct sockaddr_un, struct sockaddr_dl,
	socklen_t, struct vifctl, struct mfcctl, struct sioc_sg_req,
	vifi_t, struct sioc_vif_req, struct igmpmsg,
	struct ifaliasreq, struct if6_aliasreq, struct in6_aliasreq,
	struct nd_opt_adv_interval, struct rt_addrinfo,
	struct nd_opt_homeagent_info, struct nd_opt_adv_interval],
	[], [], QUAGGA_INCLUDES)

AC_CHECK_MEMBERS([struct sockaddr.sa_len,
	struct sockaddr_in.sin_len, struct sockaddr_un.sun_len,
	struct sockaddr_in6.sin6_scope_id,
	struct sockaddr_dl.sdl_len,
	struct if6_aliasreq.ifra_lifetime,
	struct nd_opt_adv_interval.nd_opt_ai_type],
	[], [], QUAGGA_INCLUDES)

dnl ---------------------------
dnl IRDP/pktinfo/icmphdr checks
dnl ---------------------------
AC_CHECK_TYPES([struct in_pktinfo], 
 [AC_CHECK_TYPES([struct icmphdr],
   [if test "${enable_irdp}" != "no"; then
      AC_DEFINE(HAVE_IRDP,, IRDP)
    fi],
   [if test "${enable_irdp}" = "yes"; then
      AC_MSG_ERROR(['IRDP requires in_pktinfo at the moment!'])
    fi], [QUAGGA_INCLUDES])],
 [if test "${enable_irdp}" = "yes"; then
    AC_MSG_ERROR(['IRDP requires in_pktinfo at the moment!'])
  fi], [QUAGGA_INCLUDES])

dnl -----------------------
dnl checking for IP_PKTINFO
dnl -----------------------
AC_MSG_CHECKING(for IP_PKTINFO)
AC_TRY_COMPILE([#include <netdb.h>], [
  int opt = IP_PKTINFO;
], [
  AC_MSG_RESULT(yes)
  AC_DEFINE(HAVE_IP_PKTINFO, 1, [Have IP_PKTINFO])
], [
  AC_MSG_RESULT(no)
])

dnl ---------------------------
dnl checking for IP_RECVDSTADDR
dnl ---------------------------
AC_MSG_CHECKING(for IP_RECVDSTADDR)
AC_TRY_COMPILE([#include <netinet/in.h>], [
  int opt = IP_RECVDSTADDR;
], [
  AC_MSG_RESULT(yes)
  AC_DEFINE(HAVE_IP_RECVDSTADDR, 1, [Have IP_RECVDSTADDR])
], [
  AC_MSG_RESULT(no)
])

dnl ----------------------
dnl checking for IP_RECVIF
dnl ----------------------
AC_MSG_CHECKING(for IP_RECVIF)
AC_TRY_COMPILE([#include <netinet/in.h>], [
  int opt = IP_RECVIF;
], [
  AC_MSG_RESULT(yes)
  AC_DEFINE(HAVE_IP_RECVIF, 1, [Have IP_RECVIF])
], [
  AC_MSG_RESULT(no)
])

dnl --------------------------------------
dnl checking for getrusage struct and call
dnl --------------------------------------
if test "${enable_rusage}" != "no"; then
  AC_MSG_CHECKING(whether getrusage is available)
  AC_TRY_COMPILE([#include <sys/resource.h>],[struct rusage ac_x; getrusage (RUSAGE_SELF, &ac_x);],
    [AC_MSG_RESULT(yes)
     AC_DEFINE(HAVE_RUSAGE,,rusage)],
      AC_MSG_RESULT(no))
fi

dnl --------------------------------------
dnl checking for clock_time monotonic struct and call
dnl --------------------------------------
AC_CHECK_DECL(CLOCK_MONOTONIC,
	[AC_CHECK_LIB(rt, clock_gettime, [LIBS="$LIBS -lrt"])
	 AC_DEFINE(HAVE_CLOCK_MONOTONIC,, Have monotonic clock)
], [AC_MSG_RESULT(no)], [QUAGGA_INCLUDES])

dnl -------------------
dnl capabilities checks
dnl -------------------
if test "${enable_capabilities}" != "no"; then
  AC_MSG_CHECKING(whether prctl PR_SET_KEEPCAPS is available)
  AC_TRY_COMPILE([#include <sys/prctl.h>],[prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);],
    [AC_MSG_RESULT(yes)
     AC_DEFINE(HAVE_PR_SET_KEEPCAPS,,prctl)
     quagga_ac_keepcaps="yes"],
     AC_MSG_RESULT(no)
  )
  if test x"${quagga_ac_keepcaps}" = x"yes"; then
    AC_CHECK_HEADERS(sys/capability.h)
  fi
  if test x"${ac_cv_header_sys_capability_h}" = x"yes"; then
    AC_CHECK_LIB(cap, cap_init, 
      [AC_DEFINE(HAVE_LCAPS,1,Capabilities)
       LIBCAP="-lcap"
       quagga_ac_lcaps="yes"]
    )
  else
    AC_CHECK_HEADERS(priv.h,
     [AC_MSG_CHECKING(Solaris style privileges are available)
      AC_TRY_COMPILE([#include <priv.h>],[getpflags(PRIV_AWARE);],
    	  [AC_MSG_RESULT(yes)
    	   AC_DEFINE(HAVE_SOLARIS_CAPABILITIES,1,getpflags)
    	   quagga_ac_scaps="yes"],
    	   AC_MSG_RESULT(no)
      )
     ]
   )
  fi
  if test x"${quagga_ac_scaps}" = x"yes" \
       -o x"${quagga_ac_lcaps}" = x"yes"; then
    AC_DEFINE(HAVE_CAPABILITIES,1,capabilities)
  fi
fi
AC_SUBST(LIBCAP)

dnl ---------------------------------------------------------------------------
dnl http://www.gnu.org/software/autoconf-archive/ax_sys_weak_alias.html
dnl Check for and set one of the following = 1
dnl   HAVE_SYS_WEAK_ALIAS_ATTRIBUTE
dnl   HAVE_SYS_WEAK_ALIAS_PRAGMA
dnl   HAVE_SYS_WEAK_ALIAS_HPSECONDARY
dnl   HAVE_SYS_WEAK_ALIAS_CRIDUPLICATE
dnl If any scheme is found, set
dnl   HAVE_SYS_WEAK_ALIAS=1
dnl The following variable is set to text value
dnl   WEAK_ALIAS = "attribute" || "pragma" || "hpsecondary" || "criduplicate" || "no"
dnl If weak alias can cross object file boundaries
dnl   WEAK_ALIAS_CROSSFILE = "yes" || "no"
dnl ---------------------------------------------------------------------------
AX_SYS_WEAK_ALIAS

dnl ---------------------------
dnl check for glibc 'backtrace'
dnl --------------------------- 
if test x"${enable_backtrace}" != x"no" ; then
  backtrace_ok=no
  AC_CHECK_HEADER([execinfo.h], [
    AC_SEARCH_LIBS([backtrace], [execinfo], [
      AC_DEFINE(HAVE_GLIBC_BACKTRACE,,[Glibc backtrace])
      AC_DEFINE(HAVE_STACK_TRACE,,[Stack symbol decoding])
      backtrace_ok=yes
    ],, [-lm])
  ])

  if test x"${enable_backtrace}" = x"yes" -a x"${backtrace_ok}" = x"no"; then
    dnl user explicitly requested backtrace but we failed to find support
    AC_MSG_FAILURE([failed to find backtrace support])
  fi
fi

dnl -----------------------------------------
dnl check for malloc mallinfo struct and call
dnl this must try and link using LIBS, in
dnl order to check no alternative allocator
dnl has been specified, which might not provide
dnl mallinfo, e.g. such as Umem on Solaris.
dnl -----------------------------------------
AC_CHECK_HEADER([malloc.h],
 [AC_MSG_CHECKING(whether mallinfo is available)
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <malloc.h>]],
                        [[struct mallinfo ac_x; ac_x = mallinfo ();]])],
      [AC_MSG_RESULT(yes)
       AC_DEFINE(HAVE_MALLINFO,,mallinfo)],
       AC_MSG_RESULT(no)
  )
 ], [], QUAGGA_INCLUDES)

dnl ----------
dnl configure date
dnl ----------
CONFDATE=`date '+%Y%m%d'`
AC_SUBST(CONFDATE)

dnl Conditionally enable PIE support for GNU toolchains.
AC_ARG_ENABLE(pie, AS_HELP_STRING([--disable-pie], [Do not build tools as a Position Independent Executables]))
if test "$enable_pie" != "no"; then
  AC_CACHE_CHECK([whether $CC accepts PIE flags], [ap_cv_cc_pie], [
    save_CFLAGS=$CFLAGS
    save_LDFLAGS=$LDFLAGS
    CFLAGS="$CFLAGS -fPIE"
    LDFLAGS="$LDFLAGS -pie"
    AC_LINK_IFELSE([AC_LANG_SOURCE([[static int foo[30000]; int main () { return 0; }]])],
      [ap_cv_cc_pie=yes], [ap_cv_cc_pie=no]
    )
    CFLAGS=$save_CFLAGS
    LDFLAGS=$save_LDFLAGS
  ])
  if test "$ap_cv_cc_pie" = "yes"; then
    PICFLAGS="-fPIE"
    PILDFLAGS="-pie"
  fi
fi

AC_SUBST(PICFLAGS)
AC_SUBST(PILDFLAGS)

dnl -------
dnl DejaGNU
dnl -------
if test x"$DEJAGNU" = x
then
  DEJAGNU="\$(top_srcdir)/tests/global-conf.exp"
fi
RUNTESTDEFAULTFLAGS="-x --tool \$\$tool"

AC_SUBST(DEJAGNU)
AC_SUBST(RUNTESTDEFAULTFLAGS)

dnl ------------------------------
dnl set paths for state directory
dnl ------------------------------
AC_MSG_CHECKING(directory to use for state file)
if test "${prefix}" = "NONE"; then
  quagga_statedir_prefix="";
else
  quagga_statedir_prefix=${prefix}
fi
if test "${localstatedir}" = '${prefix}/var'; then
  for QUAGGA_STATE_DIR in ${quagga_statedir_prefix}/var/run dnl
			${quagga_statedir_prefix}/var/adm dnl
			${quagga_statedir_prefix}/etc dnl
			/var/run	dnl
                        /var/adm	dnl
                        /etc		dnl
                        /dev/null;
  do
    test -d $QUAGGA_STATE_DIR && break
  done
  quagga_statedir=$QUAGGA_STATE_DIR
else
  quagga_statedir=${localstatedir}
fi
if test $quagga_statedir = "/dev/null"; then
    AC_MSG_ERROR('STATE DIRECTORY NOT FOUND! FIX OR SPECIFY --localstatedir!')
fi
AC_MSG_RESULT(${quagga_statedir})
AC_SUBST(quagga_statedir)

AC_DEFINE_UNQUOTED(PATH_ZEBRA_PID, "$quagga_statedir/zebra.pid",zebra PID)
AC_DEFINE_UNQUOTED(PATH_RIPD_PID, "$quagga_statedir/ripd.pid",ripd PID)
AC_DEFINE_UNQUOTED(PATH_RIPNGD_PID, "$quagga_statedir/ripngd.pid",ripngd PID)
AC_DEFINE_UNQUOTED(PATH_BGPD_PID, "$quagga_statedir/bgpd.pid",bgpd PID)
AC_DEFINE_UNQUOTED(PATH_OSPFD_PID, "$quagga_statedir/ospfd.pid",ospfd PID)
AC_DEFINE_UNQUOTED(PATH_OSPF6D_PID, "$quagga_statedir/ospf6d.pid",ospf6d PID)
AC_DEFINE_UNQUOTED(PATH_BABELD_PID, "$quagga_statedir/babeld.pid",babeld PID)
AC_DEFINE_UNQUOTED(PATH_ISISD_PID, "$quagga_statedir/isisd.pid",isisd PID)
AC_DEFINE_UNQUOTED(PATH_PIMD_PID, "$quagga_statedir/pimd.pid",pimd PID)
AC_DEFINE_UNQUOTED(PATH_WATCHQUAGGA_PID, "$quagga_statedir/watchquagga.pid",watchquagga PID)
AC_DEFINE_UNQUOTED(ZEBRA_SERV_PATH, "$quagga_statedir/zserv.api",zebra api socket)
AC_DEFINE_UNQUOTED(ZEBRA_VTYSH_PATH, "$quagga_statedir/zebra.vty",zebra vty socket)
AC_DEFINE_UNQUOTED(RIP_VTYSH_PATH, "$quagga_statedir/ripd.vty",rip vty socket)
AC_DEFINE_UNQUOTED(RIPNG_VTYSH_PATH, "$quagga_statedir/ripngd.vty",ripng vty socket)
AC_DEFINE_UNQUOTED(BGP_VTYSH_PATH, "$quagga_statedir/bgpd.vty",bgpd vty socket)
AC_DEFINE_UNQUOTED(OSPF_VTYSH_PATH, "$quagga_statedir/ospfd.vty",ospfd vty socket)
AC_DEFINE_UNQUOTED(OSPF6_VTYSH_PATH, "$quagga_statedir/ospf6d.vty",ospf6d vty socket)
AC_DEFINE_UNQUOTED(BABEL_VTYSH_PATH, "$quagga_statedir/babeld.vty",babeld vty socket)
AC_DEFINE_UNQUOTED(ISIS_VTYSH_PATH, "$quagga_statedir/isisd.vty",isisd vty socket)
AC_DEFINE_UNQUOTED(PIM_VTYSH_PATH, "$quagga_statedir/pimd.vty",pimd vty socket)
AC_DEFINE_UNQUOTED(DAEMON_VTY_DIR, "$quagga_statedir",daemon vty directory)

dnl -------------------------------
dnl Quagga sources should always be 
dnl current wrt interfaces. Dont
dnl allow deprecated interfaces to
dnl be exposed.
dnl -------------------------------
AC_DEFINE(QUAGGA_NO_DEPRECATED_INTERFACES, 1, Hide deprecated interfaces)

dnl ---------------------------
dnl Check htonl works correctly
dnl ---------------------------
AC_MSG_CHECKING(for working htonl)
AC_CACHE_VAL(ac_cv_htonl_works,
 [AC_LINK_IFELSE([AC_LANG_PROGRAM([QUAGGA_INCLUDES],[htonl (0);])],
 		[ac_cv_htonl_works=yes], [ac_cv_htonl_works=no])
 ]
)
AC_MSG_RESULT($ac_cv_htonl_works)

AC_CONFIG_FILES([Makefile lib/Makefile zebra/Makefile ripd/Makefile 
	  ripngd/Makefile bgpd/Makefile ospfd/Makefile watchquagga/Makefile
	  ospf6d/Makefile isisd/Makefile babeld/Makefile vtysh/Makefile
	  doc/Makefile ospfclient/Makefile tests/Makefile m4/Makefile
	  pimd/Makefile
	  tests/bgpd.tests/Makefile
	  tests/libzebra.tests/Makefile
	  redhat/Makefile
	  pkgsrc/Makefile
	  redhat/quagga.spec 
	  lib/version.h
	  doc/defines.texi
	  isisd/topology/Makefile
	  pkgsrc/bgpd.sh pkgsrc/ospf6d.sh pkgsrc/ospfd.sh
	  pkgsrc/ripd.sh pkgsrc/ripngd.sh pkgsrc/zebra.sh])
AC_CONFIG_FILES([solaris/Makefile])

AC_CONFIG_FILES([vtysh/extract.pl],[chmod +x vtysh/extract.pl])
## Hack, but working solution to avoid rebuilding of quagga.info.
## It's already in CVS until texinfo 4.7 is more common.
AC_OUTPUT

echo "
Quagga configuration
--------------------
quagga version          : ${PACKAGE_VERSION}
host operating system   : ${host_os}
source code location    : ${srcdir}
compiler                : ${CC}
compiler flags          : ${CFLAGS}
make                    : ${MAKE-make}
linker flags            : ${LDFLAGS} ${LIBS} ${LIBCAP} ${LIBREADLINE} ${LIBM}
state file directory    : ${quagga_statedir}
config file directory   : `eval echo \`echo ${sysconfdir}\``
example directory       : `eval echo \`echo ${exampledir}\``
user to run as		: ${enable_user}
group to run as		: ${enable_group}
group for vty sockets	: ${enable_vty_group}
config file mask        : ${enable_configfile_mask}
log file mask           : ${enable_logfile_mask}

The above user and group must have read/write access to the state file
directory and to the config files in the config file directory."

if test x"$quagga_cv_gnu_make" = x"no"; then echo "
Warning: The ${MAKE-make} programme detected, either in your path or
via the MAKE variable, is not GNU Make. GNU make may be installed as
gmake on some systems. and is required to complete a build of Quagga
" > /dev/stderr 
fi
