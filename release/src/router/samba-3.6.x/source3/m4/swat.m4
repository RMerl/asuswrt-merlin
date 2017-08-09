dnl
dnl Samba3 build environment SWAT configuration
dnl
dnl Copyright (C) Michael Adam 2008
dnl
dnl Released under the GNU General Public License
dnl http://www.gnu.org/licenses/
dnl


SWAT_SBIN_TARGETS='bin/swat$(EXEEXT)'
SWAT_INSTALL_TARGETS=installswat

AC_ARG_ENABLE(swat,
[AS_HELP_STRING([--enable-swat], [Build the SWAT tool (default=yes)])],
[
    case "$enable_swat" in
	no)
	    SWAT_SBIN_TARGETS=''
	    SWAT_INSTALL_TARGETS=''
	    ;;
    esac
])

AC_SUBST(SWAT_SBIN_TARGETS)
AC_SUBST(SWAT_INSTALL_TARGETS)

