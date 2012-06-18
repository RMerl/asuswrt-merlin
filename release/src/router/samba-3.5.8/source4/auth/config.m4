###############################
# start SMB_EXT_LIB_PAM
# check for security/pam_appl.h and -lpam
AC_CHECK_HEADERS(security/pam_appl.h)
AC_CHECK_LIB_EXT(pam, PAM_LIBS, pam_start)
if test x"$ac_cv_header_security_pam_appl_h" = x"yes" -a x"$ac_cv_lib_ext_pam_pam_start" = x"yes";then
	SMB_ENABLE(PAM,YES)
fi
SMB_EXT_LIB(PAM, $PAM_LIBS)
# end SMB_EXT_LIB_PAM
###############################

################################################
# test for where we get crypt() from
AC_CHECK_LIB_EXT(crypt, CRYPT_LIBS, crypt)
SMB_ENABLE(CRYPT,YES)
SMB_EXT_LIB(CRYPT, $CRYPT_LIBS)

AC_CHECK_FUNCS(crypt16 getauthuid getpwanam)

AC_CHECK_HEADERS(sasl/sasl.h)
AC_CHECK_LIB_EXT(sasl2, SASL_LIBS, sasl_client_init)

if test x"$ac_cv_header_sasl_sasl_h" = x"yes" -a x"$ac_cv_lib_ext_sasl2_sasl_client_init" = x"yes";then
	SMB_ENABLE(SASL,YES)
	SMB_ENABLE(cyrus_sasl,YES)
	SASL_CFLAGS="$CFLAGS"
	SASL_CPPFLAGS="$CPPFLAGS"
	SASL_LDFLAGS="$LDFLAGS"
	LIB_REMOVE_USR_LIB(SASL_LDFLAGS)
	CFLAGS_REMOVE_USR_INCLUDE(SASL_CPPFLAGS)
	CFLAGS_REMOVE_USR_INCLUDE(SASL_CFLAGS)
else
	SMB_ENABLE(cyrus_sasl,NO)
fi

SMB_EXT_LIB(SASL, $SASL_LIBS, [${SASL_CFLAGS}], [${SASL_CPPFLAGS}], [${SASL_LDFLAGS}])
