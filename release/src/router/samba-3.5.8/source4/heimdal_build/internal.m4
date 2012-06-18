m4_define([upcase],`echo $1 | tr abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ`)dnl

m4_ifndef([AC_WARNING_ENABLE],[AC_DEFUN([AC_WARNING_ENABLE],[])])

dnl love_FIND_FUNC(func, includes, arguments)
dnl kind of like AC_CHECK_FUNC, but with headerfiles
AC_DEFUN([love_FIND_FUNC], [

AC_MSG_CHECKING([for $1])
AC_CACHE_VAL(ac_cv_love_func_$1,
[
AC_LINK_IFELSE([AC_LANG_PROGRAM([[$2]],[[$1($3)]])],
[eval "ac_cv_love_func_$1=yes"],[eval "ac_cv_love_func_$1=no"])])

eval "ac_res=\$ac_cv_love_func_$1"

if false; then
	AC_CHECK_FUNCS($1)
fi
# $1
eval "ac_tr_func=HAVE_[]upcase($1)"

case "$ac_res" in
	yes)
	AC_DEFINE_UNQUOTED($ac_tr_func)
	AC_MSG_RESULT([yes])
	;;
	no)
	AC_MSG_RESULT([no])
	;;
esac


])

AC_CHECK_TYPE(u_char, uint8_t)
AC_CHECK_TYPE(u_int32_t, uint32_t)

dnl Not all systems have err.h, so we provide a replacement. Heimdal
dnl unconditionally #includes <err.h>, so we need to create an err.h,
dnl but we can't just have a static one because we don't want to use
dnl it on systems that have a real err.h. If the system has a real
dnl err.h, we should use that (eg. on Darwin, the declarations get
dnl linker attributes added, so we can't guarantee that our local
dnl declarations will be correct). Phew!
AC_CHECK_HEADERS([err.h], [],
	[ cp heimdal/lib/roken/err.hin heimdal_build/err.h ])

dnl Not all systems have ifaddrs.h, so we provide a replacement. Heimdal
dnl unconditionally #includes <ifaddrs.h>, so we need to create an ifaddrs.h,
dnl but we can't just have a static one because we don't want to use
dnl it on systems that have a real ifaddrs.h. If the system has a real
dnl ifaddrs.h. We don't use heimdal's lib/roken/ifaddrs.hin because
dnl our libreplace would conflict with it.
AC_CHECK_HEADERS([ifaddrs.h], [],
	[ cp heimdal_build/ifaddrs.hin heimdal_build/ifaddrs.h ])

AC_CHECK_HEADERS([				\
	crypt.h					\
	curses.h				\
	errno.h					\
	inttypes.h				\
	netdb.h					\
	signal.h				\
	sys/bswap.h				\
	sys/file.h				\
	sys/stropts.h				\
	sys/timeb.h				\
	sys/times.h				\
	sys/uio.h				\
	sys/un.h				\
	sys/utsname.h				\
	term.h					\
	termcap.h				\
	time.h					\
	timezone.h				\
	ttyname.h				\
	netinet/in.h				\
	netinet/in6.h				\
	netinet6/in6.h				\
	libintl.h
])

AC_CHECK_FUNCS([				\
	atexit					\
	cgetent					\
	getprogname				\
	setprogname				\
	inet_aton				\
	gethostname				\
	getnameinfo				\
	iruserok				\
	putenv					\
	rcmd					\
	readv					\
	sendmsg					\
	setitimer				\
	socket					\
	strlwr					\
	strncasecmp				\
	strptime				\
	strsep					\
	strsep_copy				\
	strtok_r				\
	strupr					\
	swab					\
	umask					\
	uname					\
	unsetenv				\
	closefrom				\
	hstrerror				\
	err					\
	warn					\
	errx					\
	warnx					\
	flock					\
	getipnodebyname				\
	getipnodebyaddr				\
	freehostent				\
	writev
])

love_FIND_FUNC(bswap16, [#ifdef HAVE_SYS_BSWAP_H
#include <sys/bswap.h>
#endif], 0)

love_FIND_FUNC(bswap32, [#ifdef HAVE_SYS_BSWAP_H
#include <sys/bswap.h>
#endif], 0)

AC_DEFUN([AC_KRB_STRUCT_WINSIZE], [
AC_MSG_CHECKING(for struct winsize)
AC_CACHE_VAL(ac_cv_struct_winsize, [
ac_cv_struct_winsize=no
for i in sys/termios.h sys/ioctl.h; do
AC_EGREP_HEADER(
struct[[ 	]]*winsize,dnl
$i, ac_cv_struct_winsize=yes; break)dnl
done
])
if test "$ac_cv_struct_winsize" = "yes"; then
  AC_DEFINE(HAVE_STRUCT_WINSIZE, 1, [define if struct winsize is declared in sys/termios.h])
fi
AC_MSG_RESULT($ac_cv_struct_winsize)
AC_EGREP_HEADER(ws_xpixel, termios.h, 
	AC_DEFINE(HAVE_WS_XPIXEL, 1, [define if struct winsize has ws_xpixel]))
AC_EGREP_HEADER(ws_ypixel, termios.h, 
	AC_DEFINE(HAVE_WS_YPIXEL, 1, [define if struct winsize has ws_ypixel]))
])

AC_KRB_STRUCT_WINSIZE

AC_TYPE_SIGNAL
if test "$ac_cv_type_signal" = "void" ; then
	AC_DEFINE(VOID_RETSIGTYPE, 1, [Define if signal handlers return void.])
fi
AC_SUBST(VOID_RETSIGTYPE)


m4_include(heimdal/cf/check-var.m4)

rk_CHECK_VAR(h_errno, 
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif])

m4_include(heimdal/cf/find-func.m4)
m4_include(heimdal/cf/find-func-no-libs.m4)
m4_include(heimdal/cf/find-func-no-libs2.m4)
m4_include(heimdal/cf/resolv.m4)

AC_CHECK_HEADERS([pty.h util.h libutil.h])

AC_CHECK_LIB_EXT(util, OPENPTY_LIBS, openpty)

SMB_ENABLE(OPENPTY,YES)

SMB_EXT_LIB(OPENPTY,[${OPENPTY_LIBS}],[${OPENPTY_CFLAGS}],[${OPENPTY_CPPFLAGS}],[${OPENPTY_LDFLAGS}])

AC_CHECK_LIB_EXT(intl, INTL_LIBS, gettext)

SMB_ENABLE(INTL,YES)

SMB_EXT_LIB(INTL, $INTL_LIBS)

smb_save_LIBS=$LIBS
RESOLV_LIBS=""
LIBS=""

dnl  This fills in the global LIBS...
rk_RESOLV

dnl AC_CHECK_LIB_EXT(resolv, RESOLV_LIBS, res_search)
	SMB_ENABLE(RESOLV,YES)

if test x"$LIBS" != "x"; then
	RESOLV_LIBS=$LIBS
fi

LIBS=$smb_save_LIBS

SMB_EXT_LIB(RESOLV,[${RESOLV_LIBS}],[${RESOLV_CFLAGS}],[${RESOLV_CPPFLAGS}],[${RESOLV_LDFLAGS}])


# these are disabled unless heimdal is found below
SMB_ENABLE(KERBEROS_LIB, NO)
SMB_ENABLE(asn1_compile, NO)
SMB_ENABLE(compile_et, NO)

#
# We need bison -y and flex in new versions
# Otherwise we get random runtime failures
#
LEX_YACC_COMBINATIONS=""
LEX_YACC_COMBINATIONS="$LEX_YACC_COMBINATIONS flex-2.5.33:bison-2.3"
LEX_YACC_COMBINATIONS="$LEX_YACC_COMBINATIONS flex-2.5.34:bison-2.3"

AC_PROG_LEX
LEX_BASENAME=`basename "$LEX"`
if test x"$LEX_BASENAME" = x"flex"; then
	# "flex 2.5.33"
	FLEX_VERSION=`$LEX --version | cut -d ' ' -f2`
	AC_MSG_CHECKING(flex version)
	AC_MSG_RESULT($FLEX_VERSION)
	FLEX_MAJOR=`echo $FLEX_VERSION | cut -d '.' -f1`
	FLEX_MINOR=`echo $FLEX_VERSION | cut -d '.' -f2`
	FLEX_RELEASE=`echo $FLEX_VERSION | cut -d '.' -f3`

	LEX_VERSION="flex-$FLEX_MAJOR.$FLEX_MINOR.$FLEX_RELEASE"
fi

AC_PROG_YACC
YACC_BASENAME=`basename "$YACC"`
if test x"$YACC_BASENAME" = x"bison -y"; then
	# bison (GNU Bison) 2.3
	BISON_VERSION=`$YACC --version | head -1 | cut -d ' ' -f4`
	AC_MSG_CHECKING(bison version)
	AC_MSG_RESULT($BISON_VERSION)
	BISON_MAJOR=`echo $BISON_VERSION | cut -d '.' -f1`
	BISON_MINOR=`echo $BISON_VERSION | cut -d '.' -f2`

	YACC_VERSION="bison-$BISON_MAJOR.$BISON_MINOR"
fi

AC_MSG_CHECKING(working LEX YACC combination)
LEX_YACC="no"
if test x"$LEX_VERSION" != x"" -a x"$YACC_VERSION" != x""; then
	V="$LEX_VERSION:$YACC_VERSION"
	for C in $LEX_YACC_COMBINATIONS; do
		if test x"$V" = x"$C"; then
			LEX_YACC=$V
			break;
		fi
	done
fi
if test x"$LEX_YACC" = x"no"; then
	LEX=false
	YACC=false
fi
AC_MSG_RESULT($LEX_YACC)

# Portions of heimdal kerberos are unpacked into source/heimdal
# of the samba source tree.  

# if we ever get to using a host kerberos, we might add conditionals here
AC_DEFINE(HAVE_COM_ERR,1,[Whether com_err is available])
HAVE_COM_ERR=YES
AC_DEFINE(HAVE_KRB5,1,[Whether kerberos is available])
HAVE_KRB5=YES
AC_DEFINE(HAVE_GSSAPI,1,[Whether GSSAPI is available])
HAVE_GSSAPI=YES
SMB_ENABLE(KERBEROS_LIB, YES)
SMB_ENABLE(asn1_compile, YES)
SMB_ENABLE(compile_et, YES)

# only add closefrom if needed
SMB_ENABLE(HEIMDAL_ROKEN_CLOSEFROM, NO)
SMB_ENABLE(HEIMDAL_ROKEN_CLOSEFROM_H, NO)
if test t$ac_cv_func_closefrom != tyes; then
	SMB_ENABLE(HEIMDAL_ROKEN_CLOSEFROM, YES)
	SMB_ENABLE(HEIMDAL_ROKEN_CLOSEFROM_H, YES)
fi

# only add getprogname if needed
SMB_ENABLE(HEIMDAL_ROKEN_PROGNAME, NO)
SMB_ENABLE(HEIMDAL_ROKEN_PROGNAME_H, NO)
if test t$ac_cv_func_getprogname != tyes; then
	SMB_ENABLE(HEIMDAL_ROKEN_PROGNAME, YES)
	SMB_ENABLE(HEIMDAL_ROKEN_PROGNAME_H, YES)
fi

VPATH="$VPATH:\$(HEIMDAL_VPATH)"

AC_DEFINE(SAMBA4_INTERNAL_HEIMDAL,1,[Whether we use in internal heimdal build])

SMB_INCLUDE_MK(heimdal_build/internal.mk)
