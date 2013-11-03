dnl Kitchen sink for configuration macros

dnl Check for docbook
AC_DEFUN(AX_CHECK_DOCBOOK, [
  # It's just rude to go over the net to build
  XSLTPROC_FLAGS=--nonet
  DOCBOOK_ROOT=
  XSLTPROC_WORKS=no

  AC_ARG_WITH(docbook,
    AS_HELP_STRING(
      [--with-docbook],
      [Path to Docbook XSL directory]
    ),
    [DOCBOOK_ROOT=$withval]
  )

  if test -n "$DOCBOOK_ROOT" ; then
    AC_CHECK_PROG(XSLTPROC,xsltproc,xsltproc,)
    if test -n "$XSLTPROC"; then
      AC_MSG_CHECKING([whether xsltproc works])
      DB_FILE="$DOCBOOK_ROOT/html/docbook.xsl"
      $XSLTPROC $XSLTPROC_FLAGS $DB_FILE >/dev/null 2>&1 << END
<?xml version="1.0" encoding='ISO-8859-1'?>
<!DOCTYPE book PUBLIC "-//OASIS//DTD DocBook XML V4.1.2//EN" "http://www.oasis-open.org/docbook/xml/4.1.2/docbookx.dtd">
<book id="test">
</book>
END
      if test "$?" = 0; then
        XSLTPROC_WORKS=yes
      fi
      AC_MSG_RESULT($XSLTPROC_WORKS)
    fi
  fi

  AC_MSG_CHECKING([whether to build Docbook documentation])
  AC_MSG_RESULT($XSLTPROC_WORKS)

  AM_CONDITIONAL(HAVE_XSLTPROC, test x"$XSLTPROC_WORKS" = x"yes")
  AC_SUBST(XSLTPROC_FLAGS)
  AC_SUBST(DOCBOOK_ROOT)
  AC_SUBST(XSLTPROC)
])

dnl Check for dtrace
AC_DEFUN([AC_NETATALK_DTRACE], [
  AC_ARG_WITH(dtrace,
    AS_HELP_STRING(
      [--with-dtrace],
      [Enable dtrace probes (default: enabled if dtrace found)]
    ),
    [WDTRACE=$withval],
    [WDTRACE=auto]
  )
  if test "x$WDTRACE" = "xyes" -o "x$WDTRACE" = "xauto" ; then
    AC_CHECK_PROG([atalk_cv_have_dtrace], [dtrace], [yes], [no])
    if test "x$atalk_cv_have_dtrace" = "xno" ; then
      if test "x$WDTRACE" = "xyes" ; then
        AC_MSG_FAILURE([dtrace requested but not found])
      fi
      WDTRACE="no"
    else
      WDTRACE="yes"
    fi
  fi

  if test x"$WDTRACE" = x"yes" ; then
    AC_DEFINE([WITH_DTRACE], [1], [dtrace probes])
    DTRACE_LIBS=""
    if test x"$this_os" = x"freebsd" ; then
      DTRACE_LIBS="-lelf"
    fi
    AC_SUBST(DTRACE_LIBS)
  fi
  AM_CONDITIONAL(WITH_DTRACE, test "x$WDTRACE" = "xyes")
])

dnl Check for dbus-glib, for AFP stats
AC_DEFUN([AC_NETATALK_DBUS_GLIB], [
    atalk_cv_with_dbus=no
    PKG_CHECK_MODULES(DBUS, dbus-1 >= 1.1, have_dbus=yes, have_dbus=no)
    PKG_CHECK_MODULES(DBUS_GLIB, dbus-glib-1, have_dbus_glib=yes, have_dbus_glib=no)
    PKG_CHECK_MODULES(DBUS_GTHREAD, gthread-2.0, have_dbus_gthread=yes, have_dbus_gthread=no)
    AC_SUBST(DBUS_CFLAGS)
    AC_SUBST(DBUS_LIBS)
    AC_SUBST(DBUS_GLIB_CFLAGS)
    AC_SUBST(DBUS_GLIB_LIBS)
    AC_SUBST(DBUS_GTHREAD_CFLAGS)
    AC_SUBST(DBUS_GTHREAD_LIBS)
    if test x$have_dbus_glib = xyes -a x$have_dbus = xyes -a x$have_dbus_gthread = xyes ; then
        saved_CFLAGS=$CFLAGS
        saved_LIBS=$LIBS
        CFLAGS="$CFLAGS $DBUS_GLIB_CFLAGS"
        LIBS="$LIBS $DBUS_GLIB_LIBS"
        AC_CHECK_FUNC([dbus_g_bus_get_private], [atalk_cv_with_dbus=yes], [atalk_cv_with_dbus=no])
        CFLAGS="$saved_CFLAGS"
        LIBS="$saved_LIBS"
    fi
    AM_CONDITIONAL(HAVE_DBUS_GLIB, test x$atalk_cv_with_dbus = xyes)

    AC_ARG_WITH(
        dbus-sysconf-dir,
        [AS_HELP_STRING([--with-dbus-sysconf-dir=PATH],[Path to dbus system bus security configuration directory (default: ${sysconfdir}/dbus-1/system.d/)])],
        ac_cv_dbus_sysdir=$withval,
        ac_cv_dbus_sysdir='${sysconfdir}/dbus-1/system.d'
    )

    if test x$atalk_cv_with_dbus = xyes ; then
        AC_DEFINE(HAVE_DBUS_GLIB, 1, [Define if support for dbus-glib was found])
        DBUS_SYS_DIR="$ac_cv_dbus_sysdir"
        AC_SUBST(DBUS_SYS_DIR)
    fi
])

dnl Whether to enable developer build
AC_DEFUN([AC_DEVELOPER], [
    AC_MSG_CHECKING([whether to enable developer build])
    AC_ARG_ENABLE(
        developer,
        AS_HELP_STRING([--enable-developer], [whether to enable developer build (ABI checking)]),
        enable_dev=$enableval,
        enable_dev=no
    )
    AC_MSG_RESULT([$enable_dev])
    AM_CONDITIONAL(DEVELOPER, test x"$enable_dev" = x"yes")
])

dnl Whether to disable bundled libevent
AC_DEFUN([AC_NETATALK_LIBEVENT], [
    AC_MSG_CHECKING([whether to use bundled libevent])
    AC_ARG_WITH(
        libevent,
        [AS_HELP_STRING([--with-libevent],[whether to use the bundled libevent (default: yes)])],
        use_bundled_libevent=$withval,
        use_bundled_libevent=yes
    )
    AC_ARG_WITH(
        libevent-header,
        [AS_HELP_STRING([--with-libevent-header],[path to libevent header files])],
        [use_bundled_libevent=no; LIBEVENT_CFLAGS=-I$withval]
    )
    AC_ARG_WITH(
        libevent-lib,
        [AS_HELP_STRING([--with-libevent-lib],[path to libevent library])],
        [use_bundled_libevent=no; LIBEVENT_LDFLAGS=-L$withval]
    )
    if test x"$LIBEVENT_CFLAGS" = x"-Iyes" -o x"$LIBEVENT_LDFLAGS" = x"-Lyes" ; then
        AC_MSG_ERROR([--with-libevent requires a path])
    fi
    AC_MSG_RESULT([$use_bundled_libevent])
    if test x"$use_bundled_libevent" = x"yes" ; then
        AC_CONFIG_SUBDIRS([libevent])
    fi
    AC_SUBST(LIBEVENT_CFLAGS)
    AC_SUBST(LIBEVENT_LDFLAGS)
    AM_CONDITIONAL(USE_BUILTIN_LIBEVENT, test x"$use_bundled_libevent" = x"yes")
])

dnl Filesystem Hierarchy Standard (FHS) compatibility
AC_DEFUN([AC_NETATALK_FHS], [
AC_MSG_CHECKING([whether to use Filesystem Hierarchy Standard (FHS) compatibility])
AC_ARG_ENABLE(fhs,
	[  --enable-fhs            use Filesystem Hierarchy Standard (FHS) compatibility],[
	if test "$enableval" = "yes"; then
		bindir="/bin"
		sbindir="/sbin"
		sysconfdir="/etc"
		libdir="/lib"
		localstatedir="/var"
		mandir="/usr/share/man"
		uams_path="${libdir}/netatalk"
		PKGCONFDIR="${sysconfdir}"
		SERVERTEXT="${localstatedir}/netatalk/msg"
		use_pam_so=yes
		AC_DEFINE(FHS_COMPATIBILITY, 1, [Define if you want compatibily with the FHS])
		AC_MSG_RESULT([yes])
        atalk_cv_fhs_compat=yes
	else
		AC_MSG_RESULT([no])
        atalk_cv_fhs_compat=no
	fi
	],[
		AC_MSG_RESULT([no])
        atalk_cv_fhs_compat=no
])])

dnl netatalk lockfile path
AC_DEFUN([AC_NETATALK_LOCKFILE], [
    AC_MSG_CHECKING([netatalk lockfile path])
    AC_ARG_WITH(
        lockfile,
        [AS_HELP_STRING([--with-lockfile=PATH],[Path of netatalk lockfile])],
        ac_cv_netatalk_lock=$withval,
        ac_cv_netatalk_lock=""
    )
    if test -z "$ac_cv_netatalk_lock" ; then
        ac_cv_netatalk_lock=/var/spool/locks/netatalk
        if test x"$atalk_cv_fhs_compat" = x"yes" ; then
            ac_cv_netatalk_lock=/var/run/netatalk.pid
        else
            case "$host_os" in
            *freebsd*)
                ac_cv_netatalk_lock=/var/spool/lock/netatalk
                ;;
            *netbsd*|*openbsd*)
                ac_cv_netatalk_lock=/var/run/netatalk.pid
                ;;
            *linux*)
                ac_cv_netatalk_lock=/var/lock/netatalk
                ;;
            esac
        fi
    fi
    AC_DEFINE_UNQUOTED(PATH_NETATALK_LOCK, ["$ac_cv_netatalk_lock"], [netatalk lockfile path])
    AC_SUBST(PATH_NETATALK_LOCK, ["$ac_cv_netatalk_lock"])
    AC_MSG_RESULT([$ac_cv_netatalk_lock])
])

dnl 64bit platform check
AC_DEFUN([AC_NETATALK_64BIT_LIBS], [
AC_MSG_CHECKING([whether to check for 64bit libraries])
# Test if the compiler is in 64bit mode
echo 'int i;' > conftest.$ac_ext
atalk_cv_cc_64bit_output=no
if AC_TRY_EVAL(ac_compile); then
    case `/usr/bin/file conftest.$ac_objext` in
    *"ELF 64"*)
      atalk_cv_cc_64bit_output=yes
      ;;
    esac
fi
rm -rf conftest*

case $host_cpu:$atalk_cv_cc_64bit_output in
powerpc64:yes | s390x:yes | sparc*:yes | x86_64:yes | i386:yes)
    case $target_os in
    solaris2*)
        AC_MSG_RESULT([yes])
        atalk_libname="lib/64"
        ;;
    *bsd* | dragonfly*)
        AC_MSG_RESULT([no])
        atalk_libname="lib"
        ;;
    *)
        AC_MSG_RESULT([yes])
        atalk_libname="lib64"
        ;;
    esac
    ;;
*:*)
    AC_MSG_RESULT([no])
    atalk_libname="lib"
    ;;
esac
])

dnl Check for optional admin group support
AC_DEFUN([AC_NETATALK_ADMIN_GROUP], [
    netatalk_cv_admin_group=yes
    AC_MSG_CHECKING([for administrative group support])
    AC_ARG_ENABLE(admin-group,
 	    [  --disable-admin-group   disable admin group],[
            if test x"$enableval" = x"no"; then
		         AC_DEFINE(ADMIN_GRP, 0, [Define if the admin group should be enabled])
		         netatalk_cv_admin_group=no
		         AC_MSG_RESULT([no])
	        else
		         AC_DEFINE(ADMIN_GRP, 1, [Define if the admin group should be enabled])
		         AC_MSG_RESULT([yes])
            fi],[
		AC_DEFINE(ADMIN_GRP, 1, [Define if the admin group should be enabled])
		AC_MSG_RESULT([yes])
	])
])

dnl Check for optional cracklib support
AC_DEFUN([AC_NETATALK_CRACKLIB], [
netatalk_cv_with_cracklib=no
AC_ARG_WITH(cracklib,
	[  --with-cracklib[[=DICT]]  enable/set location of cracklib dictionary [[no]]],[
	if test "x$withval" != "xno" ; then
		cracklib="$withval"
		AC_CHECK_LIB(crack, main, [
			AC_DEFINE(USE_CRACKLIB, 1, [Define if cracklib should be used])
			LIBS="$LIBS -lcrack"
			if test "$cracklib" = "yes"; then
				cracklib="/usr/$atalk_libname/cracklib_dict"
			fi
			AC_DEFINE_UNQUOTED(_PATH_CRACKLIB, "$cracklib",
				[path to cracklib dictionary])
			AC_MSG_RESULT([setting cracklib dictionary to $cracklib])
			netatalk_cv_with_cracklib=yes
			],[
			AC_MSG_ERROR([cracklib not found!])
			]
		)
	fi
	]
)
AC_MSG_CHECKING([for cracklib support])
AC_MSG_RESULT([$netatalk_cv_with_cracklib])
])

dnl Check whether to enable debug code
AC_DEFUN([AC_NETATALK_DEBUG], [
AC_MSG_CHECKING([whether to enable verbose debug code])
AC_ARG_ENABLE(debug,
	[  --enable-debug          enable verbose debug code],[
	if test "$enableval" != "no"; then
		if test "$enableval" = "yes"; then
			AC_DEFINE(DEBUG, 1, [Define if verbose debugging information should be included])
		else
			AC_DEFINE_UNQUOTED(DEBUG, $enableval, [Define if verbose debugging information should be included])
		fi 
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
        AC_DEFINE(NDEBUG, 1, [Disable assertions])
	fi
	],[
		AC_MSG_RESULT([no])
        AC_DEFINE(NDEBUG, 1, [Disable assertions])
	]
)
])

dnl Check whethe to disable tickle SIGALARM stuff, which eases debugging
AC_DEFUN([AC_NETATALK_DEBUGGING], [
AC_MSG_CHECKING([whether to enable debugging with debuggers])
AC_ARG_ENABLE(debugging,
	[  --enable-debugging      disable SIGALRM timers and DSI tickles (eg for debugging with gdb/dbx/...)],[
	if test "$enableval" != "no"; then
		if test "$enableval" = "yes"; then
			AC_DEFINE(DEBUGGING, 1, [Define if you want to disable SIGALRM timers and DSI tickles])
		else
			AC_DEFINE_UNQUOTED(DEBUGGING, $enableval, [Define if you want to disable SIGALRM timers and DSI tickles])
		fi 
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
	fi
	],[
		AC_MSG_RESULT([no])
	]
)

])

dnl Check for optional shadow password support
AC_DEFUN([AC_NETATALK_SHADOW], [
netatalk_cv_use_shadowpw=no
AC_ARG_WITH(shadow,
	[  --with-shadow           enable shadow password support [[auto]]],
	[netatalk_cv_use_shadowpw="$withval"],
	[netatalk_cv_use_shadowpw=auto]
)

if test "x$netatalk_cv_use_shadowpw" != "xno"; then
    AC_CHECK_HEADER([shadow.h])
    if test x"$ac_cv_header_shadow_h" = x"yes"; then
	netatalk_cv_use_shadowpw=yes
	AC_DEFINE(SHADOWPW, 1, [Define if shadow passwords should be used])
    else 
      if test "x$shadowpw" = "xyes"; then
        AC_MSG_ERROR([shadow support not available])
      else
       	netatalk_cv_use_shadowpw=no
      fi
    fi 
fi

AC_MSG_CHECKING([whether shadow support should be enabled])
if test "x$netatalk_cv_use_shadowpw" = "xyes"; then
	AC_MSG_RESULT([yes])
else
	AC_MSG_RESULT([no])
fi
])

dnl Check for optional valid-shell-check support
AC_DEFUN([AC_NETATALK_SHELL_CHECK], [
netatalk_cv_use_shellcheck=yes
AC_MSG_CHECKING([whether checking for a valid shell should be enabled])
AC_ARG_ENABLE(shell-check,
	[  --disable-shell-check   disable checking for a valid shell],[
	if test "$enableval" = "no"; then 
		AC_DEFINE(DISABLE_SHELLCHECK, 1, [Define if shell check should be disabled])
		AC_MSG_RESULT([no])
		netatalk_cv_use_shellcheck=no
	else
		AC_MSG_RESULT([yes])
	fi
	],[
		AC_MSG_RESULT([yes])
	]
)
])

dnl Check for optional initscript install
AC_DEFUN([AC_NETATALK_INIT_STYLE], [
    AC_ARG_WITH(init-style,
                [  --with-init-style       use OS specific init config [[redhat-sysv|redhat-systemd|suse-sysv|suse-systemd|gentoo|netbsd|debian|solaris|systemd]]],
                init_style="$withval", init_style=none
    )
    case "$init_style" in 
    "redhat")
	    AC_MSG_ERROR([--with-init-style=redhat is obsoleted. Use redhat-sysv or redhat-systemd.])
        ;;
    "redhat-sysv")
	    AC_MSG_RESULT([enabling redhat-style sysv initscript support])
	    ac_cv_init_dir="/etc/rc.d/init.d"
	    ;;
    "redhat-systemd")
	    AC_MSG_RESULT([enabling redhat-style systemd support])
	    ac_cv_init_dir="/usr/lib/systemd/system"
	    ;;
    "suse")
	    AC_MSG_ERROR([--with-init-style=suse is obsoleted. Use suse-sysv or suse-systemd.])
        ;;
    "suse-sysv")
	    AC_MSG_RESULT([enabling suse-style sysv initscript support])
	    ac_cv_init_dir="/etc/init.d"
	    ;;
    "suse-systemd")
	    AC_MSG_RESULT([enabling suse-style systemd support (>=openSUSE12.1)])
	    ac_cv_init_dir="/usr/lib/systemd/system"
	    ;;
    "gentoo")
	    AC_MSG_RESULT([enabling gentoo-style initscript support])
	    ac_cv_init_dir="/etc/init.d"
        ;;
    "netbsd")
	    AC_MSG_RESULT([enabling netbsd-style initscript support])
	    ac_cv_init_dir="/etc/rc.d"
        ;;
    "debian")
	    AC_MSG_RESULT([enabling debian-style initscript support])
	    ac_cv_init_dir="/etc/init.d"
        ;;
    "solaris")
	    AC_MSG_RESULT([enabling solaris-style SMF support])
	    ac_cv_init_dir="/lib/svc/manifest/network/"
        ;;
    "systemd")
	    AC_MSG_RESULT([enabling general systemd support])
	    ac_cv_init_dir="/usr/lib/systemd/system"
        ;;
    "none")
	    AC_MSG_RESULT([disabling init-style support])
	    ac_cv_init_dir="none"
        ;;
    *)
	    AC_MSG_ERROR([illegal init-style])
        ;;
    esac
    AM_CONDITIONAL(USE_NETBSD, test x$init_style = xnetbsd)
    AM_CONDITIONAL(USE_REDHAT_SYSV, test x$init_style = xredhat-sysv)
    AM_CONDITIONAL(USE_SUSE_SYSV, test x$init_style = xsuse-sysv)
    AM_CONDITIONAL(USE_SOLARIS, test x$init_style = xsolaris)
    AM_CONDITIONAL(USE_GENTOO, test x$init_style = xgentoo)
    AM_CONDITIONAL(USE_DEBIAN, test x$init_style = xdebian)
    AM_CONDITIONAL(USE_SYSTEMD, test x$init_style = xsystemd || test x$init_style = xredhat-systemd || test x$init_style = xsuse-systemd)
    AM_CONDITIONAL(USE_UNDEF, test x$init_style = xnone)

    AC_ARG_WITH(init-dir,
                [  --with-init-dir=PATH    path to OS specific init directory],
                ac_cv_init_dir="$withval", ac_cv_init_dir="$ac_cv_init_dir"
    )
    INIT_DIR="$ac_cv_init_dir"
    AC_SUBST(INIT_DIR, ["$ac_cv_init_dir"])
])

dnl OS specific configuration
AC_DEFUN([AC_NETATALK_OS_SPECIFIC], [
case "$host_os" in
	*aix*)				this_os=aix ;;
	*freebsd*) 			this_os=freebsd ;;
	*hpux11*)			this_os=hpux11 ;;
	*irix*)				this_os=irix ;;
	*linux*)   			this_os=linux ;;
	*osx*)				this_os=macosx ;;
	*darwin*)			this_os=macosx ;;
	*netbsd*) 			this_os=netbsd ;;
	*openbsd*) 			this_os=openbsd ;;
	*osf*) 				this_os=tru64 ;;
	*solaris*) 			this_os=solaris ;;
esac

case "$host_cpu" in
	i386|i486|i586|i686|k7)		this_cpu=x86 ;;
	alpha)						this_cpu=alpha ;;
	mips)						this_cpu=mips ;;
	powerpc|ppc)				this_cpu=ppc ;;
esac

dnl --------------------- GNU source
case "$this_os" in
	linux)	AC_DEFINE(_GNU_SOURCE, 1, [Whether to use GNU libc extensions])
        ;;
     kfreebsd-gnu) AC_DEFINE(_GNU_SOURCE, 1, [Whether to use GNU libc extensions])
        ;;
esac

dnl --------------------- operating system specific flags (port from sys/*)

dnl ----- FreeBSD specific -----
if test x"$this_os" = "xfreebsd"; then 
	AC_MSG_RESULT([ * FreeBSD specific configuration])
	AC_DEFINE(BSD4_4, 1, [BSD compatiblity macro])
	AC_DEFINE(FREEBSD, 1, [Define if OS is FreeBSD])
    AC_DEFINE(OPEN_NOFOLLOW_ERRNO, EMLINK, errno returned by open with O_NOFOLLOW)
fi

dnl ----- GNU/kFreeBSD specific -----
if test x"$this_os" = "xkfreebsd-gnu"; then 
	AC_MSG_RESULT([ * GNU/kFreeBSD specific configuration])
	AC_DEFINE(BSD4_4, 1, [BSD compatiblity macro])
	AC_DEFINE(FREEBSD, 1, [Define if OS is FreeBSD])
    AC_DEFINE(OPEN_NOFOLLOW_ERRNO, EMLINK, errno returned by open with O_NOFOLLOW)
fi

dnl ----- Linux specific -----
if test x"$this_os" = "xlinux"; then 
	AC_MSG_RESULT([ * Linux specific configuration])
    AC_DEFINE(LINUX, 1, [OS is Linux])	
	dnl ----- check if we need the quotactl wrapper
    AC_CHECK_HEADERS(linux/dqblk_xfs.h,,
		[AC_CHECK_HEADERS(linux/xqm.h linux/xfs_fs.h)
        	AC_CHECK_HEADERS(xfs/libxfs.h xfs/xqm.h xfs/xfs_fs.h)]
	)


	dnl ----- as far as I can tell, dbtob always does the wrong thing
	dnl ----- on every single version of linux I've ever played with.
	dnl ----- see etc/afpd/quota.c
	AC_DEFINE(HAVE_BROKEN_DBTOB, 1, [Define if dbtob is broken])

	need_dash_r=no
fi

dnl ----- NetBSD specific -----
if test x"$this_os" = "xnetbsd"; then 
	AC_MSG_RESULT([ * NetBSD specific configuration])
	AC_DEFINE(BSD4_4, 1, [BSD compatiblity macro])
	AC_DEFINE(NETBSD, 1, [Define if OS is NetBSD])
    AC_DEFINE(OPEN_NOFOLLOW_ERRNO, EFTYPE, errno returned by open with O_NOFOLLOW)

	CFLAGS="-I\$(top_srcdir)/sys/netbsd $CFLAGS"
	need_dash_r=yes 

	dnl ----- NetBSD does not have crypt.h, uses unistd.h -----
	AC_DEFINE(UAM_DHX, 1, [Define if the DHX UAM modules should be compiled])
fi

dnl ----- OpenBSD specific -----
if test x"$this_os" = "xopenbsd"; then 
	AC_MSG_RESULT([ * OpenBSD specific configuration])
    AC_DEFINE(BSD4_4, 1, [BSD compatiblity macro])
	dnl ----- OpenBSD does not have crypt.h, uses unistd.h -----
	AC_DEFINE(UAM_DHX, 1, [Define if the DHX UAM modules should be compiled])
fi

dnl ----- Solaris specific -----
if test x"$this_os" = "xsolaris"; then 
	AC_MSG_RESULT([ * Solaris specific configuration])
	AC_DEFINE(__svr4__, 1, [Solaris compatibility macro])
	AC_DEFINE(_ISOC9X_SOURCE, 1, [Compatibility macro])
	AC_DEFINE(NO_STRUCT_TM_GMTOFF, 1, [Define if the gmtoff member of struct tm is not available])
	AC_DEFINE(SOLARIS, 1, [Solaris compatibility macro])
    AC_DEFINE(_XOPEN_SOURCE, 600, [Solaris compilation environment])
    AC_DEFINE(__EXTENSIONS__,  1, [Solaris compilation environment])
	need_dash_r=yes
	init_style=solaris
fi

dnl Whether to run ldconfig after installing libraries
AC_PATH_PROG(NETA_LDCONFIG, ldconfig, , [$PATH$PATH_SEPARATOR/sbin$PATH_SEPARATOR/bin$PATH_SEPARATOR/usr/sbin$PATH_SEPARATOR/usr/bin])
echo NETA_LDCONFIG = $NETA_LDCONFIG
AM_CONDITIONAL(RUN_LDCONFIG, test x"$this_os" = x"linux" -a x"$NETA_LDCONFIG" != x"")
])

dnl Check for building PGP UAM module
AC_DEFUN([AC_NETATALK_PGP_UAM], [
AC_MSG_CHECKING([whether the PGP UAM should be build])
AC_ARG_ENABLE(pgp-uam,
	[  --enable-pgp-uam        enable build of PGP UAM module],[
	if test "$enableval" = "yes"; then 
		if test "x$neta_cv_have_openssl" = "xyes"; then 
			AC_DEFINE(UAM_PGP, 1, [Define if the PGP UAM module should be compiled])
			compile_pgp=yes
			AC_MSG_RESULT([yes])
		else
			AC_MSG_RESULT([no])
		fi
	fi
	],[
		AC_MSG_RESULT([no])
	]
)
])

dnl Check for building Kerberos V UAM module
AC_DEFUN([AC_NETATALK_KRB5_UAM], [
netatalk_cv_build_krb5_uam=no
AC_ARG_ENABLE(krbV-uam,
	[  --enable-krbV-uam       enable build of Kerberos V UAM module],
	[
		if test x"$enableval" = x"yes"; then
			NETATALK_GSSAPI_CHECK([
				netatalk_cv_build_krb5_uam=yes
			],[
				AC_MSG_ERROR([need GSSAPI to build Kerberos V UAM])
			])
		fi
	]
	
)

AC_MSG_CHECKING([whether Kerberos V UAM should be build])
if test x"$netatalk_cv_build_krb5_uam" = x"yes"; then
	AC_MSG_RESULT([yes])
else
	AC_MSG_RESULT([no])
fi
AM_CONDITIONAL(USE_GSSAPI, test x"$netatalk_cv_build_krb5_uam" = x"yes")
])

dnl Check if we can directly use Kerberos 5 API, used for reading keytabs
dnl and automatically construction DirectoryService names from that, instead
dnl of requiring special configuration in afp.conf
AC_DEFUN([AC_NETATALK_KERBEROS], [
AC_MSG_CHECKING([for Kerberos 5 (necessary for GetSrvrInfo:DirectoryNames support)])
AC_ARG_WITH([kerberos],
    [AS_HELP_STRING([--with-kerberos], [Kerberos 5 support (default=auto)])],
    [],
    [with_kerberos=auto])
AC_MSG_RESULT($with_kerberos)

if test x"$with_kerberos" != x"no"; then
   have_krb5_header="no"
   AC_CHECK_HEADERS([krb5/krb5.h krb5.h], [have_krb5_header="yes"; break])
   if test x"$have_krb5_header" = x"no" && test x"$with_kerberos" != x"auto"; then
      AC_MSG_FAILURE([--with-kerberos was given, but no headers found])
   fi

   AC_PATH_PROG([KRB5_CONFIG], [krb5-config])
   AC_MSG_CHECKING([for krb5-config])
   if test -x "$KRB5_CONFIG"; then
      AC_MSG_RESULT([$KRB5_CONFIG])
      KRB5_CFLAGS="`$KRB5_CONFIG --cflags krb5`"
      KRB5_LIBS="`$KRB5_CONFIG --libs krb5`"
      AC_SUBST(KRB5_CFLAGS)
      AC_SUBST(KRB5_LIBS)
      with_kerberos="yes"
   else
      AC_MSG_RESULT([not found])
      if test x"$with_kerberos" != x"auto"; then
         AC_MSG_FAILURE([--with-kerberos was given, but krb5-config could not be found])
      fi
   fi
fi

if test x"$with_kerberos" = x"yes"; then
   AC_DEFINE([HAVE_KERBEROS], [1], [Define if Kerberos 5 is available])
fi

dnl Check for krb5_free_unparsed_name and krb5_free_error_message
save_CFLAGS="$CFLAGS"
save_LIBS="$LIBS"
CFLAGS="$KRB5_CFLAGS"
LIBS="$KRB5_LIBS"
AC_CHECK_FUNCS([krb5_free_unparsed_name krb5_free_error_message krb5_free_keytab_entry_contents krb5_kt_free_entry])
CFLAGS="$save_CFLAGS"
LIBS="$save_LIBS"
])

dnl Check for overwrite the config files or not
AC_DEFUN([AC_NETATALK_OVERWRITE_CONFIG], [
AC_MSG_CHECKING([whether configuration files should be overwritten])
AC_ARG_ENABLE(overwrite,
	[  --enable-overwrite      overwrite configuration files during installation],
	[OVERWRITE_CONFIG="${enable_overwrite}"],
	[OVERWRITE_CONFIG="no"]
)
AC_MSG_RESULT([$OVERWRITE_CONFIG])
AC_SUBST(OVERWRITE_CONFIG)
])

dnl Check for LDAP support, for client-side ACL visibility
AC_DEFUN([AC_NETATALK_LDAP], [
AC_MSG_CHECKING(for LDAP (necessary for client-side ACL visibility))
AC_ARG_WITH(ldap,
    [AS_HELP_STRING([--with-ldap],
        [LDAP support (default=auto)])],
        netatalk_cv_ldap=$withval,
        netatalk_cv_ldap=auto
        )
AC_MSG_RESULT($netatalk_cv_ldap)

save_CFLAGS="$CFLAGS"
save_LDFLAGS="$LDFLAGS"
save_LIBS="$LIBS"
CFLAGS=""
LDFLAGS=""
LIBS=""
LDAP_CFLAGS=""
LDAP_LDFLAGS=""
LDAP_LIBS=""

if test x"$netatalk_cv_ldap" != x"no" ; then
   if test x"$netatalk_cv_ldap" != x"yes" -a x"$netatalk_cv_ldap" != x"auto"; then
       CFLAGS="-I$netatalk_cv_ldap/include"
       LDFLAGS="-L$netatalk_cv_ldap/lib"
   fi
   	AC_CHECK_HEADER([ldap.h], netatalk_cv_ldap=yes,
        [ if test x"$netatalk_cv_ldap" = x"yes" ; then
            AC_MSG_ERROR([Missing LDAP headers])
        fi
		netatalk_cv_ldap=no
        ])
	AC_CHECK_LIB(ldap, ldap_init, netatalk_cv_ldap=yes,
        [ if test x"$netatalk_cv_ldap" = x"yes" ; then
            AC_MSG_ERROR([Missing LDAP library])
        fi
		netatalk_cv_ldap=no
        ])
fi

if test x"$netatalk_cv_ldap" = x"yes"; then
    LDAP_CFLAGS="$CFLAGS"
    LDAP_LDFLAGS="$LDFLAGS"
    LDAP_LIBS="-lldap"
	AC_DEFINE(HAVE_LDAP,1,[Whether LDAP is available])
fi

AC_SUBST(LDAP_CFLAGS)
AC_SUBST(LDAP_LDFLAGS)
AC_SUBST(LDAP_LIBS)
CFLAGS="$save_CFLAGS"
LDFLAGS="$save_LDFLAGS"
LIBS="$save_LIBS"
])

dnl Check for ACL support
AC_DEFUN([AC_NETATALK_ACL], [
AC_MSG_CHECKING(whether to support ACLs)
AC_ARG_WITH(acls,
    [AS_HELP_STRING([--with-acls],
        [Include ACL support (default=auto)])],
    [ case "$withval" in
      yes|no)
          with_acl_support="$withval"
		  ;;
      *)
          with_acl_support=auto
          ;;
      esac ],
    [with_acl_support=auto])
AC_MSG_RESULT($with_acl_support)

if test x"$with_acl_support" = x"no"; then
	AC_MSG_RESULT(Disabling ACL support)
	AC_DEFINE(HAVE_NO_ACLS,1,[Whether no ACLs support should be built in])
else
    with_acl_support=yes
fi

if test x"$with_acl_support" = x"yes" ; then
	AC_MSG_NOTICE(checking whether ACL support is available:)
	case "$host_os" in
	*sysv5*)
		AC_MSG_NOTICE(Using UnixWare ACLs)
		AC_DEFINE(HAVE_UNIXWARE_ACLS,1,[Whether UnixWare ACLs are available])
		;;
	*solaris*)
		AC_MSG_NOTICE(Using solaris ACLs)
		AC_DEFINE(HAVE_SOLARIS_ACLS,1,[Whether solaris ACLs are available])
		ACL_LIBS="$ACL_LIBS -lsec"
		;;
	*hpux*)
		AC_MSG_NOTICE(Using HPUX ACLs)
		AC_DEFINE(HAVE_HPUX_ACLS,1,[Whether HPUX ACLs are available])
		;;
	*irix*)
		AC_MSG_NOTICE(Using IRIX ACLs)
		AC_DEFINE(HAVE_IRIX_ACLS,1,[Whether IRIX ACLs are available])
		;;
	*aix*)
		AC_MSG_NOTICE(Using AIX ACLs)
		AC_DEFINE(HAVE_AIX_ACLS,1,[Whether AIX ACLs are available])
		;;
	*osf*)
		AC_MSG_NOTICE(Using Tru64 ACLs)
		AC_DEFINE(HAVE_TRU64_ACLS,1,[Whether Tru64 ACLs are available])
		ACL_LIBS="$ACL_LIBS -lpacl"
		;;
	*darwin*)
		AC_MSG_NOTICE(ACLs on Darwin currently not supported)
		AC_DEFINE(HAVE_NO_ACLS,1,[Whether no ACLs support is available])
		;;
	*)
		AC_CHECK_LIB(acl,acl_get_file,[ACL_LIBS="$ACL_LIBS -lacl"])
		case "$host_os" in
		*linux*)
			AC_CHECK_LIB(attr,getxattr,[ACL_LIBS="$ACL_LIBS -lattr"])
			;;
		esac
		AC_CACHE_CHECK([for POSIX ACL support],netatalk_cv_HAVE_POSIX_ACLS,[
			acl_LIBS=$LIBS
			LIBS="$LIBS $ACL_LIBS"
			AC_LINK_IFELSE([AC_LANG_PROGRAM([[
				#include <sys/types.h>
				#include <sys/acl.h>
			]], [[
				acl_t acl;
				int entry_id;
				acl_entry_t *entry_p;
				return acl_get_entry(acl, entry_id, entry_p);
			]])],[netatalk_cv_HAVE_POSIX_ACLS=yes],[netatalk_cv_HAVE_POSIX_ACLS=no
                with_acl_support=no])
			LIBS=$acl_LIBS
		])
		if test x"$netatalk_cv_HAVE_POSIX_ACLS" = x"yes"; then
			AC_MSG_NOTICE(Using POSIX ACLs)
			AC_DEFINE(HAVE_POSIX_ACLS,1,[Whether POSIX ACLs are available])
			AC_CACHE_CHECK([for acl_get_perm_np],netatalk_cv_HAVE_ACL_GET_PERM_NP,[
				acl_LIBS=$LIBS
				LIBS="$LIBS $ACL_LIBS"
				AC_LINK_IFELSE([AC_LANG_PROGRAM([[
					#include <sys/types.h>
					#include <sys/acl.h>
				]], [[
					acl_permset_t permset_d;
					acl_perm_t perm;
					return acl_get_perm_np(permset_d, perm);
				]])],[netatalk_cv_HAVE_ACL_GET_PERM_NP=yes],[netatalk_cv_HAVE_ACL_GET_PERM_NP=no])
				LIBS=$acl_LIBS
			])
			if test x"$netatalk_cv_HAVE_ACL_GET_PERM_NP" = x"yes"; then
				AC_DEFINE(HAVE_ACL_GET_PERM_NP,1,[Whether acl_get_perm_np() is available])
			fi


                       AC_CACHE_CHECK([for acl_from_mode], netatalk_cv_HAVE_ACL_FROM_MODE,[
                               acl_LIBS=$LIBS
                               LIBS="$LIBS $ACL_LIBS"
                AC_CHECK_FUNCS(acl_from_mode,
                               [netatalk_cv_HAVE_ACL_FROM_MODE=yes],
                               [netatalk_cv_HAVE_ACL_FROM_MODE=no])
                               LIBS=$acl_LIBS
                       ])
                       if test x"netatalk_cv_HAVE_ACL_FROM_MODE" = x"yes"; then
                               AC_DEFINE(HAVE_ACL_FROM_MODE,1,[Whether acl_from_mode() is available])
                       fi

		else
			AC_MSG_NOTICE(ACL support is not avaliable)
			AC_DEFINE(HAVE_NO_ACLS,1,[Whether no ACLs support is available])
		fi
		;;
    esac
fi

if test x"$with_acl_support" = x"yes" ; then
   AC_CHECK_HEADERS([acl/libacl.h])
    AC_DEFINE(HAVE_ACLS,1,[Whether ACLs support is available])
    AC_SUBST(ACL_LIBS)
fi
])

dnl Check for Extended Attributes support
AC_DEFUN([AC_NETATALK_EXTENDED_ATTRIBUTES], [
neta_cv_eas="ad"
neta_cv_eas_sys_found=no
neta_cv_eas_sys_not_found=no

AC_CHECK_HEADERS(sys/attributes.h attr/xattr.h sys/xattr.h sys/extattr.h sys/uio.h sys/ea.h)

case "$this_os" in

  *osf*)
	AC_SEARCH_LIBS(getproplist, [proplist])
	AC_CHECK_FUNCS([getproplist fgetproplist setproplist fsetproplist],
                   [neta_cv_eas_sys_found=yes],
                   [neta_cv_eas_sys_not_found=yes])
	AC_CHECK_FUNCS([delproplist fdelproplist add_proplist_entry get_proplist_entry],,
                   [neta_cv_eas_sys_not_found=yes])
	AC_CHECK_FUNCS([sizeof_proplist_entry],,
                   [neta_cv_eas_sys_not_found=yes])
  ;;

  *solaris*)
	AC_CHECK_FUNCS([attropen],
                   [neta_cv_eas_sys_found=yes; AC_DEFINE(HAVE_EAFD, 1, [extattr API has full fledged fds for EAs])],
                   [neta_cv_eas_sys_not_found=yes])
  ;;

  'freebsd')
    AC_CHECK_FUNCS([extattr_delete_fd extattr_delete_file extattr_delete_link],
                   [neta_cv_eas_sys_found=yes],
                   [neta_cv_eas_sys_not_found=yes])
    AC_CHECK_FUNCS([extattr_get_fd extattr_get_file extattr_get_link],,
                   [neta_cv_eas_sys_not_found=yes])
    AC_CHECK_FUNCS([extattr_list_fd extattr_list_file extattr_list_link],,
                   [neta_cv_eas_sys_not_found=yes])
    AC_CHECK_FUNCS([extattr_set_fd extattr_set_file extattr_set_link],,
                   [neta_cv_eas_sys_not_found=yes])
  ;;

  *freebsd4* | *dragonfly* )
    AC_DEFINE(BROKEN_EXTATTR, 1, [Does extattr API work])
  ;;

  *)
	AC_SEARCH_LIBS(getxattr, [attr])

    if test "x$neta_cv_eas_sys_found" != "xyes" ; then
       AC_CHECK_FUNCS([getxattr lgetxattr fgetxattr listxattr llistxattr],
                      [neta_cv_eas_sys_found=yes],
                      [neta_cv_eas_sys_not_found=yes])
	   AC_CHECK_FUNCS([flistxattr removexattr lremovexattr fremovexattr],,
                      [neta_cv_eas_sys_not_found=yes])
	   AC_CHECK_FUNCS([setxattr lsetxattr fsetxattr],,
                      [neta_cv_eas_sys_not_found=yes])
    fi

    if test "x$neta_cv_eas_sys_found" != "xyes" ; then
	   AC_CHECK_FUNCS([getea fgetea lgetea listea flistea llistea],
                      [neta_cv_eas_sys_found=yes],
                      [neta_cv_eas_sys_not_found=yes])
	   AC_CHECK_FUNCS([removeea fremoveea lremoveea setea fsetea lsetea],,
                      [neta_cv_eas_sys_not_found=yes])
    fi

    if test "x$neta_cv_eas_sys_found" != "xyes" ; then
	   AC_CHECK_FUNCS([attr_get attr_list attr_set attr_remove],,
                      [neta_cv_eas_sys_not_found=yes])
       AC_CHECK_FUNCS([attr_getf attr_listf attr_setf attr_removef],,
                      [neta_cv_eas_sys_not_found=yes])
    fi
  ;;
esac

# Do xattr functions take additional options like on Darwin?
if test x"$ac_cv_func_getxattr" = x"yes" ; then
	AC_CACHE_CHECK([whether xattr interface takes additional options], smb_attr_cv_xattr_add_opt, [
		old_LIBS=$LIBS
		LIBS="$LIBS $ACL_LIBS"
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
			#include <sys/types.h>
			#if HAVE_ATTR_XATTR_H
			#include <attr/xattr.h>
			#elif HAVE_SYS_XATTR_H
			#include <sys/xattr.h>
			#endif
		]], [[
			getxattr(0, 0, 0, 0, 0, 0);
		]])],[smb_attr_cv_xattr_add_opt=yes],[smb_attr_cv_xattr_add_opt=no;LIBS=$old_LIBS])
	])
	if test x"$smb_attr_cv_xattr_add_opt" = x"yes"; then
		AC_DEFINE(XATTR_ADD_OPT, 1, [xattr functions have additional options])
	fi
fi

if test "x$neta_cv_eas_sys_found" = "xyes" ; then
   if test "x$neta_cv_eas_sys_not_found" != "xyes" ; then
      neta_cv_eas="$neta_cv_eas | sys"
   fi
fi
AC_DEFINE_UNQUOTED(EA_MODULES,["$neta_cv_eas"],[Available Extended Attributes modules])
])

dnl Check for libsmbsharemodes from Samba for Samba/Netatalk access/deny/share modes interop
dnl Defines "neta_cv_have_smbshmd" to "yes" or "no"
dnl AC_SUBST's "SMB_SHAREMODES_CFLAGS" and "SMB_SHAREMODES_LDFLAGS"
dnl AM_CONDITIONAL's "USE_SMB_SHAREMODES"
AC_DEFUN([AC_NETATALK_SMB_SHAREMODES], [
    neta_cv_have_smbshmd=no
    AC_ARG_WITH(smbsharemodes-lib,
                [  --with-smbsharemodes-lib=PATH        PATH to libsmbsharemodes lib from Samba],
                [SMB_SHAREMODES_LDFLAGS="-L$withval -lsmbsharemodes"]
    )
    AC_ARG_WITH(smbsharemodes-include,
                [  --with-smbsharemodes-include=PATH    PATH to libsmbsharemodes header from Samba],
                [SMB_SHAREMODES_CFLAGS="-I$withval"]
    )
    AC_ARG_WITH(smbsharemodes,
                [AS_HELP_STRING([--with-smbsharemodes],[Samba interop (default is yes)])],
                [use_smbsharemodes=$withval],
                [use_smbsharemodes=yes]
    )

    if test x"$use_smbsharemodes" = x"yes" ; then
        AC_MSG_CHECKING([whether to enable Samba/Netatalk access/deny/share-modes interop])

        saved_CFLAGS="$CFLAGS"
        saved_LDFLAGS="$LDFLAGS"
        CFLAGS="$SMB_SHAREMODES_CFLAGS $CFLAGS"
        LDFLAGS="$SMB_SHAREMODES_LDFLAGS $LDFLAGS"

        AC_LINK_IFELSE(
            [#include <unistd.h>
             #include <stdio.h>
             #include <sys/time.h>
             #include <time.h>
             #include <stdint.h>
             /* From messages.h */
             struct server_id {
                 pid_t pid;
             };
             #include "smb_share_modes.h"
             int main(void) { (void)smb_share_mode_db_open(""); return 0;}],
            [neta_cv_have_smbshmd=yes]
        )

        AC_MSG_RESULT($neta_cv_have_smbshmd)
        AC_SUBST(SMB_SHAREMODES_CFLAGS, [$SMB_SHAREMODES_CFLAGS])
        AC_SUBST(SMB_SHAREMODES_LDFLAGS, [$SMB_SHAREMODES_LDFLAGS])
        CFLAGS="$saved_CFLAGS"
        LDFLAGS="$saved_LDFLAGS"
    fi

    AM_CONDITIONAL(USE_SMB_SHAREMODES, test x"$neta_cv_have_smbshmd" = x"yes")
])

dnl ------ Check for sendfile() --------
AC_DEFUN([AC_NETATALK_SENDFILE], [
netatalk_cv_search_sendfile=yes
AC_ARG_ENABLE(sendfile,
    [  --disable-sendfile       disable sendfile syscall],
    [if test x"$enableval" = x"no"; then
            netatalk_cv_search_sendfile=no
        fi]
)

if test x"$netatalk_cv_search_sendfile" = x"yes"; then
   case "$host_os" in
   *linux*)
        AC_DEFINE(SENDFILE_FLAVOR_LINUX,1,[Whether linux sendfile() API is available])
        AC_CHECK_FUNC([sendfile], [netatalk_cv_HAVE_SENDFILE=yes])
        ;;

    *solaris*)
        AC_DEFINE(SENDFILE_FLAVOR_SOLARIS, 1, [Solaris sendfile()])
        AC_SEARCH_LIBS(sendfile, sendfile)
        AC_CHECK_FUNC([sendfile], [netatalk_cv_HAVE_SENDFILE=yes])
        AC_CHECK_FUNCS([sendfilev])
        ;;

    *freebsd*)
        AC_DEFINE(SENDFILE_FLAVOR_BSD, 1, [Define if the sendfile() function uses BSD semantics])
        AC_CHECK_FUNC([sendfile], [netatalk_cv_HAVE_SENDFILE=yes])
        ;;

    *)
        ;;

    esac

    if test x"$netatalk_cv_HAVE_SENDFILE" = x"yes"; then
        AC_DEFINE(WITH_SENDFILE,1,[Whether sendfile() should be used])
    fi
fi
])

dnl --------------------- Check if realpath() takes NULL
AC_DEFUN([AC_NETATALK_REALPATH], [
AC_CACHE_CHECK([if the realpath function allows a NULL argument],
    neta_cv_REALPATH_TAKES_NULL, [
        AC_RUN_IFELSE([AC_LANG_SOURCE([[
            #include <stdio.h>
            #include <limits.h>
            #include <signal.h>

            void exit_on_core(int ignored) {
                 exit(1);
            }

            main() {
                char *newpath;
                signal(SIGSEGV, exit_on_core);
                newpath = realpath("/tmp", NULL);
                exit((newpath != NULL) ? 0 : 1);
            }]])],[neta_cv_REALPATH_TAKES_NULL=yes],[neta_cv_REALPATH_TAKES_NULL=no],[neta_cv_REALPATH_TAKES_NULL=cross
        ])
    ]
)

if test x"$neta_cv_REALPATH_TAKES_NULL" = x"yes"; then
    AC_DEFINE(REALPATH_TAKES_NULL,1,[Whether the realpath function allows NULL])
fi
])
