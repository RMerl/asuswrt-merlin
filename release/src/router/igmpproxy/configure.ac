AC_PREREQ([2.63])
AC_INIT([igmpproxy], [0.1.2])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/igmpproxy.c])
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC_C99

AC_CANONICAL_HOST
case $host_os in
	linux*) os=linux;;
	freebsd*) os=freebsd;;
	netbsd*) os=netbsd;;
	openbsd*) os=openbsd;;
	dragonfly*) os=dragonfly;;
	*) AC_MSG_ERROR([OS $host_os is not supported]);;
esac
AC_CONFIG_LINKS([src/os.h:src/os-${os}.h])

AC_CHECK_MEMBERS([struct sockaddr.sa_len], [], [], [[
#include <sys/types.h>
#include <sys/socket.h>
]])
AC_CHECK_MEMBERS([struct sockaddr_in.sin_len], [], [], [[
#include <sys/types.h>
#include <netinet/in.h>
]])

AC_CONFIG_FILES([
	Makefile
	doc/Makefile
	src/Makefile
	doc/igmpproxy.8
	doc/igmpproxy.conf.5
])
AC_OUTPUT
