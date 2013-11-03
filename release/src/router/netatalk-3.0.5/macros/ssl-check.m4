dnl Autoconf macro to check for SSL or OpenSSL

AC_DEFUN([AC_NETATALK_CRYPT], [

	saveLIBS=$LIBS
	LIBS=""
	CRYPT_LIBS=""

	AC_CHECK_HEADERS(crypt.h)
	AC_CHECK_LIB(crypt, main)

	CRYPT_LIBS=$LIBS
	LIBS=$saveLIBS

	AC_SUBST(CRYPT_LIBS)
])


AC_DEFUN([AC_NETATALK_PATH_SSL], [
	AC_ARG_WITH(ssl-dir, [  --with-ssl-dir=PATH     specify path to OpenSSL installation (must contain
                          lib and include dirs)],
		[
			if test "x$withval" = "xno"; then
				tryssl=no
			elif test "x$withval" = "xyes"; then
				tryssl=yes
				tryssldir=
			else
				dnl FIXME: should only try in $withval
				tryssl=yes
				tryssldir="$withval"
			fi
		], [tryssl=yes]
	)

	SSL_CFLAGS=""
	SSL_LIBS=""
	saved_LIBS=$LIBS
	saved_CFLAGS=$CFLAGS
	neta_cv_have_openssl=no
dnl	compile_ssl=no

	dnl make sure atalk_libname is defined beforehand
	[[ -n "$atalk_libname" ]] || AC_MSG_ERROR([internal error, atalk_libname undefined])

	if test "$tryssl" = "yes"; then
		AC_MSG_CHECKING([for SSL])
		for ssldir in "" $tryssldir /usr /usr/local/openssl /usr/$atalk_libname/openssl /usr/local/ssl /usr/$atalk_libname/ssl /usr/local /usr/pkg /opt /opt/openssl /usr/local/ssl ; do
			if test -f "$ssldir/include/openssl/cast.h" ; then
				SSL_CFLAGS="$SSL_CFLAGS -I$ssldir/include -I$ssldir/include/openssl"
				SSL_LIBS="$SSL_LIBS -L$ssldir/$atalk_libname -L$ssldir -lcrypto"
				if test "x$need_dash_r" = "xyes"; then
					SSL_LIBS="$SSL_LIBS -R$ssldir/$atalk_libname -R$ssldir"
				fi
				AC_MSG_RESULT([$ssldir (enabling RANDNUM and DHX support)])
				CFLAGS="$CFLAGS $SSL_CFLAGS"
				LIBS="$LIBS $SSL_LIBS"

dnl FIXME: The following looks crude and probably doesn't work properly.
				dnl Check for the crypto library:
				AC_CHECK_LIB(crypto, main)
				dnl Check for "DES" library (for SSLeay, not openssl):
				AC_CHECK_LIB(des, main)

		 		AC_DEFINE(OPENSSL_DHX,	1, [Define if the OpenSSL DHX modules should be built])
				AC_DEFINE(UAM_DHX,	1, [Define if the DHX UAM modules should be compiled])
				neta_cv_have_openssl=yes
				neta_cv_compile_dhx=yes
				CFLAGS=$saved_CFLAGS
				LIBS=$saved_LIBS
				break
			fi
		done
		if test "x$neta_cv_have_openssl" = "xno"; then
			AC_MSG_RESULT([no])
		fi
	fi
	CFLAGS_REMOVE_USR_INCLUDE(SSL_CFLAGS)
	LIB_REMOVE_USR_LIB(SSL_LIBS)
	AC_SUBST(SSL_CFLAGS)
	AC_SUBST(SSL_LIBS)
	LIBS=$saved_LIBS
])
