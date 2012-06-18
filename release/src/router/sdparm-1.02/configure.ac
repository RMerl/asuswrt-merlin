AC_INIT(sdparm, 1.02, dgilbert@interlog.com)

AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

AC_PROG_CC
# AC_PROG_CXX
AC_PROG_INSTALL

# check for headers
AC_HEADER_STDC
AC_CHECK_FUNCS(getopt_long,
	       GETOPT_O_FILES='',
	       GETOPT_O_FILES='getopt_long.o')
AC_SUBST(GETOPT_O_FILES)

AC_CANONICAL_HOST

AC_DEFINE_UNQUOTED(SDPARM_BUILD_HOST, "${host}", [sdparm Build Host])

case "${host}" in
        *-*-linux-gnu*)
                AC_SUBST([os_deps], ['sg_pt_linux.o'])
		AC_DEFINE_UNQUOTED(SDPARM_LINUX, 1, [sdparm on linux])
                AC_SUBST([os_libs], ['']) ;;
        *-*-linux*)
                AC_SUBST([os_deps], ['sg_pt_linux.o'])
		AC_DEFINE_UNQUOTED(SDPARM_LINUX, 1, [sdparm on linux])
                AC_SUBST([os_libs], ['']) ;;
        *-*-freebsd*)
                AC_SUBST([os_deps], ['sg_pt_freebsd.o'])
		AC_DEFINE_UNQUOTED(SDPARM_FREEBSD, 1, [sdparm on FreeBSD])
                AC_SUBST([os_libs], ['-lcam']);;
        *-*-solaris*)
                AC_SUBST([os_deps], ['sg_pt_solaris.o'])
		AC_DEFINE_UNQUOTED(SDPARM_SOLARIS, 1, [sdparm on Solaris])
                AC_SUBST([os_libs], ['']);;
        *-*-osf*)
                AC_SUBST([os_deps], ['sg_pt_osf1.o'])
		AC_DEFINE_UNQUOTED(SDPARM_OSF1, 1, [sdparm on Tru64 UNIX])
                AC_SUBST([os_libs], ['']) ;;
        *-*-cygwin*)
                AC_SUBST([os_deps], ['sdparm_wscan.o sg_pt_win32.o'])
		AC_DEFINE_UNQUOTED(SDPARM_WIN32, 1, [sdparm on Win32])
                AC_SUBST([os_libs], ['']) ;;
        *-*-mingw*)
                AC_SUBST([os_deps], ['sdparm_wscan.o sg_pt_win32.o'])
		AC_DEFINE_UNQUOTED(SDPARM_WIN32, 1, [sdparm on Win32])
                AC_SUBST([os_libs], ['']) ;;
        *)
                AC_SUBST([os_deps], ['sg_pt_linux.o'])
		AC_DEFINE_UNQUOTED(SDPARM_LINUX, 1, [assume sdparm on linux])
                AC_SUBST([os_libs], ['']) ;;
esac

# AC_PROG_LIBTOOL
AC_OUTPUT(Makefile src/Makefile doc/Makefile)
