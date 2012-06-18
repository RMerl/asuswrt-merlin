########################################################
# Compile with LDAP support?

LDAP_LIBS=""
with_ldap_support=auto
AC_MSG_CHECKING([for LDAP support])

AC_ARG_WITH(ldap,
AS_HELP_STRING([--with-ldap],[LDAP backend support (default=yes)]),
[ case "$withval" in
    yes|no)
	with_ldap_support=$withval
	;;
  esac ])

AC_MSG_RESULT($with_ldap_support)

if test x"$with_ldap_support" != x"no"; then

  ##################################################################
  # first test for ldap.h and lber.h
  # (ldap.h is required for this test)
  AC_CHECK_HEADERS(ldap.h lber.h)
  
  if test x"$ac_cv_header_ldap_h" != x"yes"; then
	if test x"$with_ldap_support" = x"yes"; then
	 AC_MSG_ERROR(ldap.h is needed for LDAP support)
	else
	 AC_MSG_WARN(ldap.h is needed for LDAP support)
	fi
	
	with_ldap_support=no
  fi
fi

if test x"$with_ldap_support" != x"no"; then
  ac_save_LIBS=$LIBS

  ##################################################################
  # we might need the lber lib on some systems. To avoid link errors
  # this test must be before the libldap test
  AC_CHECK_LIB_EXT(lber, LDAP_LIBS, ber_scanf)

  ########################################################
  # now see if we can find the ldap libs in standard paths
  AC_CHECK_LIB_EXT(ldap, LDAP_LIBS, ldap_init)

  AC_CHECK_FUNC_EXT(ldap_domain2hostlist,$LDAP_LIBS)
  
  ########################################################
  # If we have LDAP, does it's rebind procedure take 2 or 3 arguments?
  # Check found in pam_ldap 145.
  AC_CHECK_FUNC_EXT(ldap_set_rebind_proc,$LDAP_LIBS)

  LIBS="$LIBS $LDAP_LIBS"
  AC_CACHE_CHECK(whether ldap_set_rebind_proc takes 3 arguments, smb_ldap_cv_ldap_set_rebind_proc, [
    AC_TRY_COMPILE([
	#include <lber.h>
	#include <ldap.h>], 
	[ldap_set_rebind_proc(0, 0, 0);], 
	[smb_ldap_cv_ldap_set_rebind_proc=3], 
	[smb_ldap_cv_ldap_set_rebind_proc=2]
    ) 
  ])
  
  AC_DEFINE_UNQUOTED(LDAP_SET_REBIND_PROC_ARGS, $smb_ldap_cv_ldap_set_rebind_proc, [Number of arguments to ldap_set_rebind_proc])

  AC_CHECK_FUNC_EXT(ldap_initialize,$LDAP_LIBS)	
  
  if test x"$ac_cv_lib_ext_ldap_ldap_init" = x"yes" -a x"$ac_cv_func_ext_ldap_domain2hostlist" = x"yes"; then
    AC_DEFINE(HAVE_LDAP,1,[Whether ldap is available])
    AC_DEFINE(HAVE_LDB_LDAP,1,[Whether ldb_ldap is available])
    with_ldap_support=yes
    AC_MSG_CHECKING(whether LDAP support is used)
    AC_MSG_RESULT(yes)
    SMB_ENABLE(LDAP,YES)
  else
    if test x"$with_ldap_support" = x"yes"; then
	AC_MSG_ERROR(libldap is needed for LDAP support)
    else
	AC_MSG_WARN(libldap is needed for LDAP support)
    fi
    
    LDAP_LIBS=""
    with_ldap_support=no
  fi
  LIBS=$ac_save_LIBS
fi

SMB_EXT_LIB(LDAP,[${LDAP_LIBS}],[${LDAP_CFLAGS}],[${LDAP_CPPFLAGS}],[${LDAP_LDFLAGS}])
