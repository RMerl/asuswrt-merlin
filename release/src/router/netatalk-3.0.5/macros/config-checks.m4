dnl Autoconf macro to set the configuration directories.

AC_DEFUN([AC_NETATALK_CONFIG_DIRS], [
	PKGCONFDIR="${sysconfdir}"

	AC_ARG_WITH(pkgconfdir,
        	[  --with-pkgconfdir=DIR   package specific configuration in DIR
                          [[$sysconfdir]]],
               	[
			if test "x$withval" != "x"; then
				PKGCONFDIR="$withval"
			fi
		]
	)


	SERVERTEXT="${localstatedir}/netatalk/msg"

	AC_ARG_WITH(message-dir,
		[  --with-message-dir=PATH path to server message files [[$localstatedir/netatalk/msg/]]],
		[
			if test x"$withval" = x"no";  then 
				AC_MSG_WARN([message-dir is mandatory and cannot be disabled, using default])
			elif test "x$withval" != "x" && test x"$withval" != x"yes"; then
				SERVERTEXT="$withval"
			fi
		]
	)

	AC_SUBST(PKGCONFDIR)
	AC_SUBST(SERVERTEXT)
])
