AC_CHECK_FUNCS(writev)
AC_CHECK_FUNCS(readv)

############################################
# check for unix domain sockets
# done by AC_LIBREPLACE_NETWORK_CHECKS
SMB_ENABLE(socket_unix, NO)
if test x"$libreplace_cv_HAVE_UNIXSOCKET" = x"yes"; then
	SMB_ENABLE(socket_unix, YES)
fi

############################################
# check for ipv6
# done by AC_LIBREPLACE_NETWORK_CHECKS
SMB_ENABLE(socket_ipv6, NO)
if test x"$libreplace_cv_HAVE_IPV6" = x"yes"; then
    SMB_ENABLE(socket_ipv6, YES)
fi
