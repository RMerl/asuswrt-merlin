dnl Autoconf macros, display configure summary

AC_DEFUN([AC_NETATALK_CONFIG_SUMMARY], [

	AC_MSG_RESULT([Configure summary:])
	AC_MSG_RESULT([    INIT STYLE:])
	if test "x$init_style" != "x"; then
		AC_MSG_RESULT([         $init_style])
	else
		AC_MSG_RESULT([         none])
	fi
	AC_MSG_RESULT([    AFP:])
	AC_MSG_RESULT([         Extended Attributes: $neta_cv_eas])
	AC_MSG_RESULT([         ACL support: $with_acl_support])
	AC_MSG_RESULT([    CNID:])
	AC_MSG_RESULT([         backends: $compiled_backends])
	AC_MSG_RESULT([    UAMS:])
	uams_using_options=""
	if test x"$netatalk_cv_use_pam" != x"no"; then
        	uams_using_options="PAM"
	fi
	if test "x$netatalk_cv_use_shadowpw" = "xyes"; then
        	uams_using_options="$uams_using_options SHADOW"
	fi
	if test "x$neta_cv_compile_dhx" = "xyes"; then
		AC_MSG_RESULT([         DHX     ($uams_using_options)])
	fi
        if test "x$neta_cv_compile_dhx2" = "xyes"; then
                AC_MSG_RESULT([         DHX2    ($uams_using_options)])
        fi
	if test "x$neta_cv_have_openssl" = "xyes"; then
		AC_MSG_RESULT([         RANDNUM (afppasswd)])
	fi
	if test x"$netatalk_cv_build_krb5_uam" = x"yes"; then
		AC_MSG_RESULT([         Kerberos V])
	fi
	if test x"$compile_pgp" = x"yes"; then
		AC_MSG_RESULT([         PGP])
	fi
	AC_MSG_RESULT([         clrtxt  ($uams_using_options)])
	AC_MSG_RESULT([         guest])
	AC_MSG_RESULT([    Options:])
	AC_MSG_RESULT([         Zeroconf support:        $netatalk_cv_zeroconf])
	AC_MSG_RESULT([         tcp wrapper support:     $netatalk_cv_tcpwrap])
dnl	if test x"$netatalk_cv_linux_sendfile" != x; then
dnl		AC_MSG_RESULT([         Linux sendfile support:  $netatalk_cv_linux_sendfile])
dnl	fi
	AC_MSG_RESULT([         quota support:           $netatalk_cv_quotasupport])
	AC_MSG_RESULT([         admin group support:     $netatalk_cv_admin_group])
	AC_MSG_RESULT([         valid shell check:       $netatalk_cv_use_shellcheck])
	AC_MSG_RESULT([         cracklib support:        $netatalk_cv_with_cracklib])
dnl	AC_MSG_RESULT([         Samba sharemode interop: $neta_cv_have_smbshmd])
	AC_MSG_RESULT([         ACL support:             $with_acl_support])
	AC_MSG_RESULT([         Kerberos support:        $with_kerberos])
	AC_MSG_RESULT([         LDAP support:            $netatalk_cv_ldap])
	AC_MSG_RESULT([         dbus support:            $atalk_cv_with_dbus])
	AC_MSG_RESULT([         dtrace probes:           $WDTRACE])
	AC_MSG_RESULT([    Paths:])
	AC_MSG_RESULT([         Netatalk lockfile:       $ac_cv_netatalk_lock])
	if test "x$init_style" != x"none"; then
		AC_MSG_RESULT([         init directory:          $ac_cv_init_dir])
	fi
	if test x"$atalk_cv_with_dbus" = x"yes"; then
		AC_MSG_RESULT([         dbus system directory:   $ac_cv_dbus_sysdir])
	fi
	if test x"$use_pam_so" = x"yes"; then
	   if test x"$netatalk_cv_install_pam" = x"yes"; then
		AC_MSG_RESULT([         pam config directory:    $ac_cv_pamdir])
	   else
		AC_MSG_RESULT([])
		AC_MSG_WARN([ PAM support was configured for your system, but the netatalk PAM configuration file])
		AC_MSG_WARN([ cannot be installed. Please install the config/netatalk.pamd file manually.])
		AC_MSG_WARN([ If you're running Solaris or BSD you'll have to edit /etc/pam.conf to get PAM working.])
		AC_MSG_WARN([ You can also re-run configure and specify --without-pam to disable PAM support.])
	   fi
	fi
	AC_MSG_RESULT([    Documentation:])
	AC_MSG_RESULT([         Docbook:                 $XSLTPROC_WORKS])
])


AC_DEFUN([AC_NETATALK_LIBS_SUMMARY], [
	dnl #################################################
	dnl # Display summary of libraries detected

	AC_MSG_RESULT([Compilation summary:])
	AC_MSG_RESULT([    CPPFLAGS       = $CPPFLAGS])
	AC_MSG_RESULT([    CFLAGS         = $CFLAGS])
	AC_MSG_RESULT([    LIBS           = $LIBS])
	AC_MSG_RESULT([    PTHREADS:])
	AC_MSG_RESULT([        LIBS   = $PTHREAD_LIBS])
	AC_MSG_RESULT([        CFLAGS = $PTHREAD_CFLAGS])
	if test x"$neta_cv_have_openssl" = x"yes"; then
		AC_MSG_RESULT([    SSL:])
		AC_MSG_RESULT([        LIBS   = $SSL_LIBS])
		AC_MSG_RESULT([        CFLAGS = $SSL_CFLAGS])
	fi
        if test x"$neta_cv_have_libgcrypt" = x"yes"; then
                AC_MSG_RESULT([    LIBGCRYPT:])
                AC_MSG_RESULT([        LIBS   = $LIBGCRYPT_LIBS])
                AC_MSG_RESULT([        CFLAGS = $LIBGCRYPT_CFLAGS])
        fi
	if test x"$netatalk_cv_use_pam" = x"yes"; then
		AC_MSG_RESULT([    PAM:])
		AC_MSG_RESULT([        LIBS   = $PAM_LIBS])
		AC_MSG_RESULT([        CFLAGS = $PAM_CFLAGS])
	fi
	if test x"$netatalk_cv_use_pam" = x"yes"; then
		AC_MSG_RESULT([    WRAP:])
		AC_MSG_RESULT([        LIBS   = $WRAP_LIBS])
		AC_MSG_RESULT([        CFLAGS = $WRAP_CFLAGS])
	fi
	if test x"$bdb_required" = x"yes"; then
		AC_MSG_RESULT([    BDB:])
		AC_MSG_RESULT([        LIBS   = $BDB_LIBS])
		AC_MSG_RESULT([        CFLAGS = $BDB_CFLAGS])
	fi
	if test x"$netatalk_cv_build_krb5_uam" = x"yes"; then
		AC_MSG_RESULT([    GSSAPI:])
		AC_MSG_RESULT([        LIBS   = $GSSAPI_LIBS])
		AC_MSG_RESULT([        CFLAGS = $GSSAPI_CFLAGS])
	fi
	if test x"$netatalk_cv_use_cups" = x"yes"; then
		AC_MSG_RESULT([    CUPS:])
		AC_MSG_RESULT([        LIBS   = $CUPS_LIBS])
		AC_MSG_RESULT([        CFLAGS = $CUPS_CFLAGS])
	fi
	if test x"$netatalk_cv_zeroconf" = x"yes"; then
		AC_MSG_RESULT([    ZEROCONF:])
		AC_MSG_RESULT([        LIBS   = $ZEROCONF_LIBS])
		AC_MSG_RESULT([        CFLAGS = $ZEROCONF_CFLAGS])
	fi
	if test x"$netatalk_cv_ldap" = x"yes"; then
		AC_MSG_RESULT([    LDAP:])
		AC_MSG_RESULT([        LIBS   = $LDAP_LDFLAGS $LDAP_LIBS])
		AC_MSG_RESULT([        CFLAGS = $LDAP_CFLAGS])
	fi
    AC_MSG_RESULT([    LIBEVENT:])
    if test x"$use_bundled_libevent" = x"yes"; then
		AC_MSG_RESULT([        bundled])
    else
		AC_MSG_RESULT([        LIBS   = $LIBEVENT_CFLAGS])
		AC_MSG_RESULT([        CFLAGS = $LIBEVENT_LDFLAGS])
    fi
])
