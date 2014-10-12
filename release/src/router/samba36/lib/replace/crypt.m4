###############################################
# test for where we get crypt() from
AC_CHECK_HEADERS(crypt.h)
AC_SEARCH_LIBS_EXT(crypt, [crypt], CRYPT_LIBS,
  [ AC_DEFINE(HAVE_CRYPT,1,[Whether the system has the crypt() function]) ],
  [ LIBREPLACEOBJ="${LIBREPLACEOBJ} $libreplacedir/crypt.o" ])
