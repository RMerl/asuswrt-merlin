# +------------------------------------------------------------------+
# | madwimax: configure.ac                                           |
# +------------------------------------------------------------------+

# madwimax version number is defined here and *only* here
AC_INIT(madwimax, 0.1.1, [http://code.google.com/p/madwimax/issues/list])
echo "madWiMAX version ${PACKAGE_VERSION}"
AC_CANONICAL_SYSTEM
AC_CONFIG_SRCDIR([src/wimax.c])
AC_CONFIG_HEADERS([include/config.h])
AC_PREFIX_DEFAULT(/usr/local/madwimax)
AM_INIT_AUTOMAKE([-Wall -Werror])

# Use "./configure --enable-maintainer-mode" to keep Makefile.in and Makefile
# in sync after SVN updates.
AM_MAINTAINER_MODE

# Fix this early so we can expand with eval later
test "${prefix}" = "NONE" && prefix="${ac_default_prefix}"
test "${exec_prefix}" = "NONE" && exec_prefix='${prefix}'

# Set defaults
udevdir='/etc/udev'
if test ! -d "${udevdir}"; then
   udevdir=''
fi

PIDPATH="/var/run"
script="generic"
manpages="yes"

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h syslog.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_HEADER_TIME

# Checks for library functions.
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_VPRINTF
AC_FUNC_WAIT3
AC_CHECK_FUNCS([gettimeofday memset socket strchr strerror strtoul])

# Check for pkg-config
PKG_PROG_PKG_CONFIG
if test -z "${PKG_CONFIG}"; then
	AC_MSG_ERROR(["pkg-config not found"])
fi

# Check for libusb-1.0
PKG_CHECK_MODULES([libusb1], libusb-1.0,,AC_MSG_ERROR([libusb-1.0 not found]))

CFLAGS="$CFLAGS $libusb1_CFLAGS"

# Only use these when compiling with gcc 
if ( test "${GCC}" = "yes" )
then
	CFLAGS="${CFLAGS} -Wall -Wsign-compare"
fi  

# Check whether to install udev rules
AC_MSG_CHECKING(whether to install udev rules)
AC_ARG_WITH(udev-dir,
	AC_HELP_STRING([--with-udev-dir=PATH], [where to install udev rules (/etc/udev)]),
	[
		case "${withval}" in
			yes)
				if test -z "${udevdir}"; then
					AC_MSG_RESULT(no)
					AC_MSG_ERROR(["udev directory requested but not found"])
				fi
			;;
			auto)
			;;
			no)
				udevdir=""
			;;
			*)
				udevdir="${withval}"
			;;
		esac
	])
if test -n "${udevdir}"; then
	AC_MSG_RESULT(using ${udevdir})
else
	AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(WITH_UDEV, test -n "${udevdir}")

# Check which event script to use
AC_MSG_CHECKING(which event script to use)
AC_ARG_WITH(script,
	AC_HELP_STRING([--with-script=NAME], [which event script to use (generic)]),
	[
		case "${withval}" in
			yes)
			;;
			auto)
			;;
			no)
				script=""
			;;
			*)
				script="${withval}"
			;;
		esac
	])
if test -n "${script}"; then
	AC_MSG_RESULT(using ${script})
	scriptfile="scripts/events/event.sh.${script}"
else
	AC_MSG_RESULT(no)
	scriptfile=""
fi
AM_CONDITIONAL(WITH_SCRIPT, test -n "${script}")

# Check whether to generate man pages
AC_MSG_CHECKING(whether to generate man pages)
AC_ARG_WITH(man-pages,
	    AC_HELP_STRING([--without-man-pages], [don't generate man pages (default is to generate them)]),
	[
		case "${withval}" in
			yes)
			;;
			auto)
			;;
			no)
				manpages="no"
			;;
			*)
			;;
		esac
	])
AC_MSG_RESULT(${manpages})
if test "${manpages}" = "yes"; then
	# Check for asciidoc
	AC_MSG_CHECKING(for asciidoc)
	if asciidoc --help >/dev/null 2>&1; then
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR(["asciidoc not found"])
	fi
	
	# Check for docbook2x-man
	db2xman=""
	AC_MSG_CHECKING(for docbook2x-man)
	for name in docbook2x-man db2x_docbook2man; do
		if "$name" --help >/dev/null 2>&1; then
			db2xman="$name"
			break;
		fi
	done
	if test -n "${db2xman}"; then
		AC_MSG_RESULT(${db2xman})
	else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR(["docbook2x-man not found"])
	fi

	AC_SUBST(db2xman)
fi
AM_CONDITIONAL(WITH_MAN_PAGES, test "${manpages}" = "yes")


# Expand ${sbindir} and write it out
conftemp="${sbindir}"
eval conftemp=\"${conftemp}\"
eval conftemp=\"${conftemp}\"

SBINDIR=${conftemp}
AC_DEFINE_UNQUOTED(SBINDIR, "${conftemp}",
			[Default sbin path])

# expand ${sysconfdir} and write it out
sysconfdir="${sysconfdir}"/madwimax
conftemp="${sysconfdir}"
eval conftemp=\"${conftemp}\"
eval conftemp=\"${conftemp}\"

SYSCONFDIR=${conftemp}
AC_DEFINE_UNQUOTED(SYSCONFDIR, "${conftemp}",
			[Default sysconf path])


AC_SUBST(SBINDIR)
AC_SUBST(SYSCONFDIR)
AC_SUBST(udevdir)
AC_SUBST(script)


AC_CONFIG_FILES([
	Makefile
	include/Makefile
	man/Makefile
	src/Makefile
	scripts/events/Makefile
	${scriptfile}
	scripts/udev/Makefile
	scripts/udev/z60_madwimax.rules
	scripts/Makefile
])
AC_OUTPUT
