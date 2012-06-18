dnl SMB Build Environment Path Checks
dnl -------------------------------------------------------
dnl  Copyright (C) Stefan (metze) Metzmacher 2004
dnl  Released under the GNU GPL
dnl -------------------------------------------------------
dnl

AC_LIBREPLACE_LOCATION_CHECKS

#################################################
# Directory handling stuff to support both the
# legacy SAMBA directories and FHS compliant
# ones...
AC_PREFIX_DEFAULT(/usr/local/samba)

# Defaults and --without-fhs
logfilebase="${localstatedir}"
lockdir="${localstatedir}/locks"
piddir="${localstatedir}/run"
privatedir="\${prefix}/private"
modulesdir="\${prefix}/modules"
winbindd_socket_dir="${localstatedir}/run/winbindd"
winbindd_privileged_socket_dir="${localstatedir}/lib/winbindd_privileged"
ntp_signd_socket_dir="${localstatedir}/run/ntp_signd"

AC_ARG_ENABLE(fhs, 
[AS_HELP_STRING([--enable-fhs],[Use FHS-compliant paths (default=no)])],
[fhs=$enableval],
[fhs=no]
)

if test x$fhs = xyes; then
    lockdir="${localstatedir}/lib/samba"
    piddir="${localstatedir}/run/samba"
    logfilebase="${localstatedir}/log/samba"
    privatedir="${localstatedir}/lib/samba/private"
    sysconfdir="${sysconfdir}/samba"
    modulesdir="${libdir}/samba"
    datadir="${datadir}/samba"
    includedir="${includedir}/samba-4.0"
    ntp_signd_socket_dir="${localstatedir}/run/samba/ntp_signd"
    winbindd_socket_dir="${localstatedir}/run/samba/winbindd"
    winbindd_privileged_socket_dir="${localstatedir}/lib/samba/winbindd_privileged"
else
	# Check to prevent installing directly under /usr without the FHS
	AS_IF([test $prefix = /usr || test $prefix = /usr/local],[
		AC_MSG_ERROR([Don't install directly under "/usr" or "/usr/local" without using the FHS option (--enable-fhs). This could lead to file loss!])
	])
fi

#################################################
# set modules directory location
AC_ARG_WITH(modulesdir,
[AS_HELP_STRING([--with-modulesdir=DIR],[Where to put dynamically loadable modules ($modulesdir)])],
[ case "$withval" in
  yes|no)
  #
  # Just in case anybody calls it without argument
  #
    AC_MSG_WARN([--with-modulesdir called without argument - will use default])
  ;;
  * )
    modulesdir="$withval"
    ;;
  esac])

#################################################
# set private directory location
AC_ARG_WITH(privatedir,
[AS_HELP_STRING([--with-privatedir=DIR],[Where to put sam.ldb and other private files containing key material ($ac_default_prefix/private)])],
[ case "$withval" in
  yes|no)
  #
  # Just in case anybody calls it without argument
  #
    AC_MSG_WARN([--with-privatedir called without argument - will use default])
  ;;
  * )
    privatedir="$withval"
    ;;
  esac])

#################################################
# set where the winbindd socket should be put
AC_ARG_WITH(winbindd-socket-dir,
[AS_HELP_STRING([--with-winbindd-socket-dir=DIR],[Where to put the winbindd socket ($winbindd_socket_dir)])],
[ case "$withval" in
  yes|no)
  #
  # Just in case anybody calls it without argument
  #
    AC_MSG_WARN([--with-winbind-socketdir called without argument - will use default])
  ;;
  * )
    winbindd_socket_dir="$withval"
    ;;
  esac])

#################################################
# set where the winbindd privileged socket should be put
AC_ARG_WITH(winbindd-privileged-socket-dir,
[AS_HELP_STRING([--with-winbindd-privileged-socket-dir=DIR],[Where to put the winbindd socket ($winbindd_privileged_socket_dir)])],
[ case "$withval" in
  yes|no)
  #
  # Just in case anybody calls it without argument
  #
    AC_MSG_WARN([--with-winbind-privileged-socketdir called without argument - will use default])
  ;;
  * )
    winbindd_privileged_socket_dir="$withval"
    ;;
  esac])

#################################################
# set where the NTP signing deamon socket should be put
AC_ARG_WITH(ntp-signd-socket-dir,
[AS_HELP_STRING([--with-ntp-signd-socket-dir=DIR],[Where to put the NTP signing deamon socket ($ac_default_prefix/run/ntp_signd)])],
[ case "$withval" in
  yes|no)
  #
  # Just in case anybody calls it without argument
  #
    AC_MSG_WARN([--with-ntp-signd-socketdir called without argument - will use default])
  ;;
  * )
    ntp_signd_socket_dir="$withval"
    ;;
  esac])

#################################################
# set lock directory location
AC_ARG_WITH(lockdir,
[AS_HELP_STRING([--with-lockdir=DIR],[Where to put lock files ($ac_default_prefix/var/locks)])],
[ case "$withval" in
  yes|no)
  #
  # Just in case anybody calls it without argument
  #
    AC_MSG_WARN([--with-lockdir called without argument - will use default])
  ;;
  * )
    lockdir="$withval"
    ;;
  esac])

#################################################
# set pid directory location
AC_ARG_WITH(piddir,
[AS_HELP_STRING([--with-piddir=DIR],[Where to put pid files ($ac_default_prefix/var/locks)])],
[ case "$withval" in
  yes|no)
  #
  # Just in case anybody calls it without argument
  #
    AC_MSG_WARN([--with-piddir called without argument - will use default])
  ;;
  * )
    piddir="$withval"
    ;;
  esac])

#################################################
# set log directory location
AC_ARG_WITH(logfilebase,
[AS_HELP_STRING([--with-logfilebase=DIR],[Where to put log files (\$(VARDIR))])],
[ case "$withval" in
  yes|no)
  #
  # Just in case anybody does it
  #
    AC_MSG_WARN([--with-logfilebase called without argument - will use default])
  ;;
  * )
    logfilebase="$withval"
    ;;
  esac])


AC_SUBST(lockdir)
AC_SUBST(piddir)
AC_SUBST(logfilebase)
AC_SUBST(privatedir)
AC_SUBST(bindir)
AC_SUBST(sbindir)
AC_SUBST(winbindd_socket_dir)
AC_SUBST(winbindd_privileged_socket_dir)
AC_SUBST(ntp_signd_socket_dir)
AC_SUBST(modulesdir)

#################################################
# set prefix for 'make test'
# this is needed to workarround the 108 char 
# unix socket path limitation!
#
selftest_prefix="./st"
AC_SUBST(selftest_prefix)
AC_ARG_WITH(selftest-prefix,
[AS_HELP_STRING([--with-selftest-prefix=DIR],[The prefix where make test will be run ($selftest_prefix)])],
[ case "$withval" in
  yes|no)
    AC_MSG_WARN([--with-selftest-prefix called without argument - will use default])
  ;;
  * )
    selftest_prefix="$withval"
    ;;
  esac])

debug=no
AC_ARG_ENABLE(debug,
[AS_HELP_STRING([--enable-debug],[Turn on compiler debugging information (default=no)])],
    [if test x$enable_debug = xyes; then
        debug=yes
    fi])

developer=no
AC_SUBST(developer)
AC_ARG_ENABLE(developer,
[AS_HELP_STRING([--enable-developer],[Turn on developer warnings and debugging (default=no)])],
    [if test x$enable_developer = xyes; then
	debug=yes
        developer=yes
    fi])

dnl disable these external libs 
AC_ARG_WITH(disable-ext-lib,
[AS_HELP_STRING([--with-disable-ext-lib=LIB],[Comma-seperated list of external libraries])],
[ if test $withval; then
	for i in `echo $withval | sed -e's/,/ /g'`
	do
		eval SMB_$i=NO
	done
fi ])
