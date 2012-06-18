AC_ARG_ENABLE(nss-wrapper,
AS_HELP_STRING([--enable-nss-wrapper], [Turn on nss wrapper library (default=no)]))

HAVE_NSS_WRAPPER=no

if eval "test x$developer = xyes"; then
	enable_nss_wrapper=yes
fi

if eval "test x$enable_nss_wrapper = xyes"; then
        AC_DEFINE(NSS_WRAPPER,1,[Use nss wrapper library])
	HAVE_NSS_WRAPPER=yes

	# this is only used for samba3
	NSS_WRAPPER_OBJS="../lib/nss_wrapper/nss_wrapper.o"
fi

AC_SUBST(HAVE_NSS_WRAPPER)
AC_SUBST(NSS_WRAPPER_OBJS)
