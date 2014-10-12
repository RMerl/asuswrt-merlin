AC_DEFUN_ONCE(AC_LIBREPLACE_NETWORK_CHECKS,
[
echo "LIBREPLACE_NETWORK_CHECKS: START"

AC_DEFINE(LIBREPLACE_NETWORK_CHECKS, 1, [LIBREPLACE_NETWORK_CHECKS were used])
LIBREPLACE_NETWORK_OBJS=""
LIBREPLACE_NETWORK_LIBS=""

AC_CHECK_HEADERS(sys/socket.h netinet/in.h netdb.h arpa/inet.h)
AC_CHECK_HEADERS(netinet/in_systm.h)
AC_CHECK_HEADERS([netinet/ip.h], [], [],[
	#include <sys/types.h>
	#ifdef HAVE_NETINET_IN_H
	#include <netinet/in.h>
	#endif
	#ifdef HAVE_NETINET_IN_SYSTM_H
	#include <netinet/in_systm.h>
	#endif
])
AC_CHECK_HEADERS(netinet/tcp.h netinet/in_ip.h)
AC_CHECK_HEADERS(sys/sockio.h sys/un.h)
AC_CHECK_HEADERS(sys/uio.h)

dnl we need to check that net/if.h really can be used, to cope with hpux
dnl where including it always fails
AC_CACHE_CHECK([for usable net/if.h],libreplace_cv_USABLE_NET_IF_H,[
	AC_COMPILE_IFELSE([AC_LANG_SOURCE([
		AC_INCLUDES_DEFAULT
		#if HAVE_SYS_SOCKET_H
		# include <sys/socket.h>
		#endif
		#include <net/if.h>
		int main(void) {return 0;}])],
		[libreplace_cv_USABLE_NET_IF_H=yes],
		[libreplace_cv_USABLE_NET_IF_H=no]
	)
])
if test x"$libreplace_cv_USABLE_NET_IF_H" = x"yes";then
	AC_DEFINE(HAVE_NET_IF_H, 1, usability of net/if.h)
fi

AC_HAVE_TYPE([socklen_t],[#include <sys/socket.h>])
AC_HAVE_TYPE([sa_family_t],[#include <sys/socket.h>])
AC_HAVE_TYPE([struct addrinfo], [#include <netdb.h>])
AC_HAVE_TYPE([struct sockaddr], [#include <sys/socket.h>])
AC_HAVE_TYPE([struct sockaddr_storage], [
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
])
AC_HAVE_TYPE([struct sockaddr_in6], [
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
])

if test x"$ac_cv_type_struct_sockaddr_storage" = x"yes"; then
AC_CHECK_MEMBER(struct sockaddr_storage.ss_family,
                AC_DEFINE(HAVE_SS_FAMILY, 1, [Defined if struct sockaddr_storage has ss_family field]),,
                [
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
		])

if test x"$ac_cv_member_struct_sockaddr_storage_ss_family" != x"yes"; then
AC_CHECK_MEMBER(struct sockaddr_storage.__ss_family,
                AC_DEFINE(HAVE___SS_FAMILY, 1, [Defined if struct sockaddr_storage has __ss_family field]),,
                [
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
		])
fi
fi

AC_CACHE_CHECK([for sin_len in sock],libreplace_cv_HAVE_SOCK_SIN_LEN,[
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
		],[
struct sockaddr_in sock; sock.sin_len = sizeof(sock);
		],[
		libreplace_cv_HAVE_SOCK_SIN_LEN=yes
		],[
		libreplace_cv_HAVE_SOCK_SIN_LEN=no
		])
])
if test x"$libreplace_cv_HAVE_SOCK_SIN_LEN" = x"yes"; then
	AC_DEFINE(HAVE_SOCK_SIN_LEN,1,[Whether the sockaddr_in struct has a sin_len property])
fi

############################################
# check for unix domain sockets
AC_CACHE_CHECK([for unix domain sockets],libreplace_cv_HAVE_UNIXSOCKET,[
	AC_TRY_COMPILE([
#include <sys/types.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
		],[
struct sockaddr_un sunaddr;
sunaddr.sun_family = AF_UNIX;
		],[
		libreplace_cv_HAVE_UNIXSOCKET=yes
		],[
		libreplace_cv_HAVE_UNIXSOCKET=no
		])
])
if test x"$libreplace_cv_HAVE_UNIXSOCKET" = x"yes"; then
	AC_DEFINE(HAVE_UNIXSOCKET,1,[If we need to build with unixscoket support])
fi

dnl The following test is roughly taken from the cvs sources.
dnl
dnl If we can't find connect, try looking in -lsocket, -lnsl, and -linet.
dnl The Irix 5 libc.so has connect and gethostbyname, but Irix 5 also has
dnl libsocket.so which has a bad implementation of gethostbyname (it
dnl only looks in /etc/hosts), so we only look for -lsocket if we need
dnl it.
AC_CHECK_FUNCS(connect)
if test x"$ac_cv_func_connect" = x"no"; then
	AC_CHECK_LIB_EXT(nsl_s, LIBREPLACE_NETWORK_LIBS, connect)
	AC_CHECK_LIB_EXT(nsl, LIBREPLACE_NETWORK_LIBS, connect)
	AC_CHECK_LIB_EXT(socket, LIBREPLACE_NETWORK_LIBS, connect)
	AC_CHECK_LIB_EXT(inet, LIBREPLACE_NETWORK_LIBS, connect)
	dnl We can't just call AC_CHECK_FUNCS(connect) here,
	dnl because the value has been cached.
	if test x"$ac_cv_lib_ext_nsl_s_connect" = x"yes" ||
		test x"$ac_cv_lib_ext_nsl_connect" = x"yes" ||
		test x"$ac_cv_lib_ext_socket_connect" = x"yes" ||
		test x"$ac_cv_lib_ext_inet_connect" = x"yes"
	then
		AC_DEFINE(HAVE_CONNECT,1,[Whether the system has connect()])
	fi
fi

AC_CHECK_FUNCS(gethostbyname)
if test x"$ac_cv_func_gethostbyname" = x"no"; then
	AC_CHECK_LIB_EXT(nsl_s, LIBREPLACE_NETWORK_LIBS, gethostbyname)
	AC_CHECK_LIB_EXT(nsl, LIBREPLACE_NETWORK_LIBS, gethostbyname)
	AC_CHECK_LIB_EXT(socket, LIBREPLACE_NETWORK_LIBS, gethostbyname)
	dnl We can't just call AC_CHECK_FUNCS(gethostbyname) here,
	dnl because the value has been cached.
	if test x"$ac_cv_lib_ext_nsl_s_gethostbyname" = x"yes" ||
		test x"$ac_cv_lib_ext_nsl_gethostbyname" = x"yes" ||
		test x"$ac_cv_lib_ext_socket_gethostbyname" = x"yes"
	then
		AC_DEFINE(HAVE_GETHOSTBYNAME,1,
			  [Whether the system has gethostbyname()])
	fi
fi

dnl HP-UX has if_nametoindex in -lipv6
AC_CHECK_FUNCS(if_nametoindex)
if test x"$ac_cv_func_if_nametoindex" = x"no"; then
	AC_CHECK_LIB_EXT(ipv6, LIBREPLACE_NETWORK_LIBS, if_nametoindex)
	dnl We can't just call AC_CHECK_FUNCS(if_nametoindex) here,
	dnl because the value has been cached.
	if test x"$ac_cv_lib_ext_ipv6_if_nametoindex" = x"yes"
	then
		AC_DEFINE(HAVE_IF_NAMETOINDEX, 1,
			  [Whether the system has if_nametoindex()])
	fi
fi

# The following tests need LIBS="${LIBREPLACE_NETWORK_LIBS}"
old_LIBS=$LIBS
LIBS="${LIBREPLACE_NETWORK_LIBS}"
libreplace_SAVE_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS -I$libreplacedir"

AC_CHECK_FUNCS(socketpair,[],[LIBREPLACE_NETWORK_OBJS="${LIBREPLACE_NETWORK_OBJS} $libreplacedir/socketpair.o"])

AC_CACHE_CHECK([for broken inet_ntoa],libreplace_cv_REPLACE_INET_NTOA,[
AC_TRY_RUN([
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
main() { struct in_addr ip; ip.s_addr = 0x12345678;
if (strcmp(inet_ntoa(ip),"18.52.86.120") &&
    strcmp(inet_ntoa(ip),"120.86.52.18")) { exit(0); }
exit(1);}],
           libreplace_cv_REPLACE_INET_NTOA=yes,libreplace_cv_REPLACE_INET_NTOA=no,libreplace_cv_REPLACE_INET_NTOA=cross)])

AC_CHECK_FUNCS(inet_ntoa,[],[libreplace_cv_REPLACE_INET_NTOA=yes])
if test x"$libreplace_cv_REPLACE_INET_NTOA" = x"yes"; then
    AC_DEFINE(REPLACE_INET_NTOA,1,[Whether inet_ntoa should be replaced])
    LIBREPLACE_NETWORK_OBJS="${LIBREPLACE_NETWORK_OBJS} $libreplacedir/inet_ntoa.o"
fi

AC_CHECK_FUNCS(inet_aton,[],[LIBREPLACE_NETWORK_OBJS="${LIBREPLACE_NETWORK_OBJS} $libreplacedir/inet_aton.o"])

AC_CHECK_FUNCS(inet_ntop,[],[LIBREPLACE_NETWORK_OBJS="${LIBREPLACE_NETWORK_OBJS} $libreplacedir/inet_ntop.o"])

AC_CHECK_FUNCS(inet_pton,[],[LIBREPLACE_NETWORK_OBJS="${LIBREPLACE_NETWORK_OBJS} $libreplacedir/inet_pton.o"])

dnl test for getaddrinfo/getnameinfo
AC_CACHE_CHECK([for getaddrinfo],libreplace_cv_HAVE_GETADDRINFO,[
AC_TRY_LINK([
#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
#include <sys/socket.h>
#include <netdb.h>],
[
struct sockaddr sa;
struct addrinfo *ai = NULL;
int ret = getaddrinfo(NULL, NULL, NULL, &ai);
if (ret != 0) {
	const char *es = gai_strerror(ret);
}
freeaddrinfo(ai);
ret = getnameinfo(&sa, sizeof(sa),
		NULL, 0,
		NULL, 0, 0);

],
libreplace_cv_HAVE_GETADDRINFO=yes,libreplace_cv_HAVE_GETADDRINFO=no)])

if test x"$libreplace_cv_HAVE_GETADDRINFO" = x"yes"; then
	# getaddrinfo is broken on some AIX systems
	# see bug 5910, use our replacements if we detect
	# a broken system.
	AC_TRY_RUN([
		#include <stddef.h>
		#include <sys/types.h>
		#include <sys/socket.h>
		#include <netdb.h>
		int main(int argc, const char *argv[])
		{
			struct addrinfo hints = {0,};
			struct addrinfo *ppres;
			const char hostname1[] = "0.0.0.0";
			const char hostname2[] = "127.0.0.1";
			const char hostname3[] = "::";
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_family = AF_UNSPEC;
			hints.ai_flags =
				AI_NUMERICHOST|AI_PASSIVE|AI_ADDRCONFIG;
			/* Test for broken flag combination on AIX. */
			if (getaddrinfo(hostname1, NULL, &hints, &ppres) == EAI_BADFLAGS) {
				/* This fails on an IPv6-only box, but not with
				   the EAI_BADFLAGS error. */
				return 1;
			}
			if (getaddrinfo(hostname2, NULL, &hints, &ppres) == 0) {
				/* IPv4 lookup works - good enough. */
				return 0;
			}
			/* Uh-oh, no IPv4. Are we IPv6-only ? */
			return getaddrinfo(hostname3, NULL, &hints, &ppres) != 0 ? 1 : 0;
		}],
		libreplace_cv_HAVE_GETADDRINFO=yes,
		libreplace_cv_HAVE_GETADDRINFO=no)
fi

if test x"$libreplace_cv_HAVE_GETADDRINFO" = x"yes"; then
	AC_DEFINE(HAVE_GETADDRINFO,1,[Whether the system has getaddrinfo])
	AC_DEFINE(HAVE_GETNAMEINFO,1,[Whether the system has getnameinfo])
	AC_DEFINE(HAVE_FREEADDRINFO,1,[Whether the system has freeaddrinfo])
	AC_DEFINE(HAVE_GAI_STRERROR,1,[Whether the system has gai_strerror])
else
	LIBREPLACE_NETWORK_OBJS="${LIBREPLACE_NETWORK_OBJS} $libreplacedir/getaddrinfo.o"
fi

AC_CHECK_HEADERS([ifaddrs.h])

dnl Used when getifaddrs is not available
AC_CHECK_MEMBERS([struct sockaddr.sa_len], 
	 [AC_DEFINE(HAVE_SOCKADDR_SA_LEN, 1, [Whether struct sockaddr has a sa_len member])],
	 [],
	 [#include <sys/socket.h>])

dnl test for getifaddrs and freeifaddrs
AC_CACHE_CHECK([for getifaddrs and freeifaddrs],libreplace_cv_HAVE_GETIFADDRS,[
AC_TRY_LINK([
#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>],
[
struct ifaddrs *ifp = NULL;
int ret = getifaddrs (&ifp);
freeifaddrs(ifp);
],
libreplace_cv_HAVE_GETIFADDRS=yes,libreplace_cv_HAVE_GETIFADDRS=no)])
if test x"$libreplace_cv_HAVE_GETIFADDRS" = x"yes"; then
    AC_DEFINE(HAVE_GETIFADDRS,1,[Whether the system has getifaddrs])
    AC_DEFINE(HAVE_FREEIFADDRS,1,[Whether the system has freeifaddrs])
	AC_DEFINE(HAVE_STRUCT_IFADDRS,1,[Whether struct ifaddrs is available])
fi

##################
# look for a method of finding the list of network interfaces
iface=no;
AC_CACHE_CHECK([for iface getifaddrs],libreplace_cv_HAVE_IFACE_GETIFADDRS,[
AC_TRY_RUN([
#define HAVE_IFACE_GETIFADDRS 1
#define NO_CONFIG_H 1
#define AUTOCONF_TEST 1
#define SOCKET_WRAPPER_NOT_REPLACE
#include "$libreplacedir/replace.c"
#include "$libreplacedir/inet_ntop.c"
#include "$libreplacedir/snprintf.c"
#include "$libreplacedir/getifaddrs.c"
#define getifaddrs_test main
#include "$libreplacedir/test/getifaddrs.c"],
           libreplace_cv_HAVE_IFACE_GETIFADDRS=yes,libreplace_cv_HAVE_IFACE_GETIFADDRS=no,libreplace_cv_HAVE_IFACE_GETIFADDRS=cross)])
if test x"$libreplace_cv_HAVE_IFACE_GETIFADDRS" = x"yes"; then
    iface=yes;AC_DEFINE(HAVE_IFACE_GETIFADDRS,1,[Whether iface getifaddrs is available])
else
	LIBREPLACE_NETWORK_OBJS="${LIBREPLACE_NETWORK_OBJS} $libreplacedir/getifaddrs.o"
fi


if test $iface = no; then
AC_CACHE_CHECK([for iface AIX],libreplace_cv_HAVE_IFACE_AIX,[
AC_TRY_RUN([
#define HAVE_IFACE_AIX 1
#define NO_CONFIG_H 1
#define AUTOCONF_TEST 1
#undef _XOPEN_SOURCE_EXTENDED
#define SOCKET_WRAPPER_NOT_REPLACE
#include "$libreplacedir/replace.c"
#include "$libreplacedir/inet_ntop.c"
#include "$libreplacedir/snprintf.c"
#include "$libreplacedir/getifaddrs.c"
#define getifaddrs_test main
#include "$libreplacedir/test/getifaddrs.c"],
           libreplace_cv_HAVE_IFACE_AIX=yes,libreplace_cv_HAVE_IFACE_AIX=no,libreplace_cv_HAVE_IFACE_AIX=cross)])
if test x"$libreplace_cv_HAVE_IFACE_AIX" = x"yes"; then
    iface=yes;AC_DEFINE(HAVE_IFACE_AIX,1,[Whether iface AIX is available])
fi
fi


if test $iface = no; then
AC_CACHE_CHECK([for iface ifconf],libreplace_cv_HAVE_IFACE_IFCONF,[
AC_TRY_RUN([
#define HAVE_IFACE_IFCONF 1
#define NO_CONFIG_H 1
#define AUTOCONF_TEST 1
#define SOCKET_WRAPPER_NOT_REPLACE
#include "$libreplacedir/replace.c"
#include "$libreplacedir/inet_ntop.c"
#include "$libreplacedir/snprintf.c"
#include "$libreplacedir/getifaddrs.c"
#define getifaddrs_test main
#include "$libreplacedir/test/getifaddrs.c"],
           libreplace_cv_HAVE_IFACE_IFCONF=yes,libreplace_cv_HAVE_IFACE_IFCONF=no,libreplace_cv_HAVE_IFACE_IFCONF=cross)])
if test x"$libreplace_cv_HAVE_IFACE_IFCONF" = x"yes"; then
    iface=yes;AC_DEFINE(HAVE_IFACE_IFCONF,1,[Whether iface ifconf is available])
fi
fi

if test $iface = no; then
AC_CACHE_CHECK([for iface ifreq],libreplace_cv_HAVE_IFACE_IFREQ,[
AC_TRY_RUN([
#define HAVE_IFACE_IFREQ 1
#define NO_CONFIG_H 1
#define AUTOCONF_TEST 1
#define SOCKET_WRAPPER_NOT_REPLACE
#include "$libreplacedir/replace.c"
#include "$libreplacedir/inet_ntop.c"
#include "$libreplacedir/snprintf.c"
#include "$libreplacedir/getifaddrs.c"
#define getifaddrs_test main
#include "$libreplacedir/test/getifaddrs.c"],
           libreplace_cv_HAVE_IFACE_IFREQ=yes,libreplace_cv_HAVE_IFACE_IFREQ=no,libreplace_cv_HAVE_IFACE_IFREQ=cross)])
if test x"$libreplace_cv_HAVE_IFACE_IFREQ" = x"yes"; then
    iface=yes;AC_DEFINE(HAVE_IFACE_IFREQ,1,[Whether iface ifreq is available])
fi
fi

dnl Some old Linux systems have broken header files and
dnl miss the IPV6_V6ONLY define in netinet/in.h,
dnl but have it in linux/in6.h.
dnl We can't include both files so we just check if the value
dnl if defined and do the replacement in system/network.h
AC_CACHE_CHECK([for IPV6_V6ONLY support],libreplace_cv_HAVE_IPV6_V6ONLY,[
	AC_TRY_COMPILE([
#include <stdlib.h> /* for NULL */
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
		],
		[
#ifndef IPV6_V6ONLY
#error no IPV6_V6ONLY
#endif
		],[
		libreplace_cv_HAVE_IPV6_V6ONLY=yes
		],[
		libreplace_cv_HAVE_IPV6_V6ONLY=no
		])
])
if test x"$libreplace_cv_HAVE_IPV6_V6ONLY" != x"yes"; then
   dnl test for IPV6_V6ONLY
   AC_CACHE_CHECK([for IPV6_V6ONLY in linux/in6.h],libreplace_cv_HAVE_LINUX_IPV6_V6ONLY_26,[
	AC_TRY_COMPILE([
	#include <linux/in6.h>
		],
		[
	#if (IPV6_V6ONLY != 26)
	#error no linux IPV6_V6ONLY
	#endif
		],[
		libreplace_cv_HAVE_LINUX_IPV6_V6ONLY_26=yes
		],[
		libreplace_cv_HAVE_LINUX_IPV6_V6ONLY_26=no
		])
	])
	if test x"$libreplace_cv_HAVE_LINUX_IPV6_V6ONLY_26" = x"yes"; then
		AC_DEFINE(HAVE_LINUX_IPV6_V6ONLY_26,1,[Whether the system has IPV6_V6ONLY in linux/in6.h])
	fi
fi

dnl test for ipv6
AC_CACHE_CHECK([for ipv6 support],libreplace_cv_HAVE_IPV6,[
	AC_TRY_LINK([
#include <stdlib.h> /* for NULL */
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
		],
		[
struct sockaddr_storage sa_store;
struct addrinfo *ai = NULL;
struct in6_addr in6addr;
int idx = if_nametoindex("iface1");
int s = socket(AF_INET6, SOCK_STREAM, 0);
int ret = getaddrinfo(NULL, NULL, NULL, &ai);
if (ret != 0) {
	const char *es = gai_strerror(ret);
}
freeaddrinfo(ai);
{
	int val = 1;
	#ifdef HAVE_LINUX_IPV6_V6ONLY_26
	#define IPV6_V6ONLY 26
	#endif
	ret = setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY,
			 (const void *)&val, sizeof(val));
}
		],[
		libreplace_cv_HAVE_IPV6=yes
		],[
		libreplace_cv_HAVE_IPV6=no
		])
])
if test x"$libreplace_cv_HAVE_IPV6" = x"yes"; then
    AC_DEFINE(HAVE_IPV6,1,[Whether the system has IPv6 support])
fi

LIBS=$old_LIBS
CPPFLAGS="$libreplace_SAVE_CPPFLAGS"

LIBREPLACEOBJ="${LIBREPLACEOBJ} ${LIBREPLACE_NETWORK_OBJS}"

echo "LIBREPLACE_NETWORK_CHECKS: END"
]) dnl end AC_LIBREPLACE_NETWORK_CHECKS
