dnl Checks librpcsec version
AC_DEFUN([AC_RPCSEC_VERSION], [

  PKG_CHECK_MODULES([GSSGLUE], [libgssglue >= 0.1])

  dnl TI-RPC replaces librpcsecgss
  if test "$enable_tirpc" = no; then
    PKG_CHECK_MODULES([RPCSECGSS], [librpcsecgss >= 0.16])
  fi

])dnl
