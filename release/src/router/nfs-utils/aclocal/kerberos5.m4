dnl Checks for Kerberos
dnl NOTE: while we intend to do generic gss-api, currently we
dnl have a requirement to get an initial Kerberos machine
dnl credential.  Thus, the requirement for Kerberos.
dnl The Kerberos gssapi library will be dynamically loaded?
AC_DEFUN([AC_KERBEROS_V5],[
  AC_MSG_CHECKING(for Kerberos v5)
  AC_ARG_WITH(krb5,
  [AC_HELP_STRING([--with-krb5=DIR], [use Kerberos v5 installation in DIR])],
  [ case "$withval" in
    yes|no)
       krb5_with=""
       ;;
    *)
       krb5_with="$withval"
       ;;
    esac ]
  )

  for dir in $krb5_with /usr /usr/kerberos /usr/local /usr/local/krb5 \
  	     /usr/krb5 /usr/heimdal /usr/local/heimdal /usr/athena ; do
    dnl This ugly hack brought on by the split installation of
    dnl MIT Kerberos on Fedora Core 1
    K5CONFIG=""
    if test -f $dir/bin/krb5-config; then
      K5CONFIG=$dir/bin/krb5-config
    elif test -f "/usr/kerberos/bin/krb5-config"; then
      K5CONFIG="/usr/kerberos/bin/krb5-config"
    elif test -f "/usr/lib/mit/bin/krb5-config"; then
      K5CONFIG="/usr/lib/mit/bin/krb5-config"
    fi
    if test "$K5CONFIG" != ""; then
      KRBCFLAGS=`$K5CONFIG --cflags`
      KRBLIBS=`$K5CONFIG --libs gssapi`
      K5VERS=`$K5CONFIG --version | head -n 1 | awk '{split($(4),v,"."); if (v@<:@"3"@:>@ == "") v@<:@"3"@:>@ = "0"; print v@<:@"1"@:>@v@<:@"2"@:>@v@<:@"3"@:>@ }'`
      AC_DEFINE_UNQUOTED(KRB5_VERSION, $K5VERS, [Define this as the Kerberos version number])
      if test -f $dir/include/gssapi/gssapi_krb5.h -a \
                \( -f $dir/lib/libgssapi_krb5.a -o \
                   -f $dir/lib64/libgssapi_krb5.a -o \
                   -f $dir/lib64/libgssapi_krb5.so -o \
                   -f $dir/lib/libgssapi_krb5.so \) ; then
         AC_DEFINE(HAVE_KRB5, 1, [Define this if you have MIT Kerberos libraries])
         KRBDIR="$dir"
  dnl If we are using MIT K5 1.3.1 and before, we *MUST* use the
  dnl private function (gss_krb5_ccache_name) to get correct
  dnl behavior of changing the ccache used by gssapi.
  dnl Starting in 1.3.2, we *DO NOT* want to use
  dnl gss_krb5_ccache_name, instead we want to set KRB5CCNAME
  dnl to get gssapi to use a different ccache
         if test $K5VERS -le 131; then
           AC_DEFINE(USE_GSS_KRB5_CCACHE_NAME, 1, [Define this if the private function, gss_krb5_cache_name, must be used to tell the Kerberos library which credentials cache to use. Otherwise, this is done by setting the KRB5CCNAME environment variable])
         fi
         gssapi_lib=gssapi_krb5
         break
      dnl The following ugly hack brought on by the split installation
      dnl of Heimdal Kerberos on SuSe
      elif test \( -f $dir/include/heim_err.h -o\
      		 -f $dir/include/heimdal/heim_err.h \) -a \
                -f $dir/lib/libroken.a; then
         AC_DEFINE(HAVE_HEIMDAL, 1, [Define this if you have Heimdal Kerberos libraries])
         KRBDIR="$dir"
         gssapi_lib=gssapi
        break
      fi
    fi
  done
  dnl We didn't find a usable Kerberos environment
  if test "x$KRBDIR" = "x"; then
    if test "x$krb5_with" = "x"; then
      AC_MSG_ERROR(Kerberos v5 with GSS support not found: consider --disable-gss or --with-krb5=)
    else
      AC_MSG_ERROR(Kerberos v5 with GSS support not found at $krb5_with)
    fi
  fi
  AC_MSG_RESULT($KRBDIR)

  dnl Check if -rpath=$(KRBDIR)/lib is needed
  echo "The current KRBDIR is $KRBDIR"
  if test "$KRBDIR/lib" = "/lib" -o "$KRBDIR/lib" = "/usr/lib" \
       -o "$KRBDIR/lib" = "//lib" -o "$KRBDIR/lib" = "/usr//lib" ; then
    KRBLDFLAGS="";
  elif /sbin/ldconfig -p | grep > /dev/null "=> $KRBDIR/lib/"; then
    KRBLDFLAGS="";
  else
    KRBLDFLAGS="-Wl,-rpath=$KRBDIR/lib"
  fi

  dnl Now check for functions within gssapi library
  AC_CHECK_LIB($gssapi_lib, gss_krb5_export_lucid_sec_context,
    AC_DEFINE(HAVE_LUCID_CONTEXT_SUPPORT, 1, [Define this if the Kerberos GSS library supports gss_krb5_export_lucid_sec_context]), ,$KRBLIBS)
  AC_CHECK_LIB($gssapi_lib, gss_krb5_set_allowable_enctypes,
    AC_DEFINE(HAVE_SET_ALLOWABLE_ENCTYPES, 1, [Define this if the Kerberos GSS library supports gss_krb5_set_allowable_enctypes]), ,$KRBLIBS)
  AC_CHECK_LIB($gssapi_lib, gss_krb5_ccache_name,
    AC_DEFINE(HAVE_GSS_KRB5_CCACHE_NAME, 1, [Define this if the Kerberos GSS library supports gss_krb5_ccache_name]), ,$KRBLIBS)

  dnl Check for newer error message facility
  AC_CHECK_LIB($gssapi_lib, krb5_get_error_message,
    AC_DEFINE(HAVE_KRB5_GET_ERROR_MESSAGE, 1, [Define this if the function krb5_get_error_message is available]), ,$KRBLIBS)

  dnl Check for function to specify addressless tickets
  AC_CHECK_LIB($gssapi_lib, krb5_get_init_creds_opt_set_addressless,
    AC_DEFINE(HAVE_KRB5_GET_INIT_CREDS_OPT_SET_ADDRESSLESS, 1, [Define this if the function krb5_get_init_creds_opt_set_addressless is available]), ,$KRBLIBS)

  dnl If they specified a directory and it didn't work, give them a warning
  if test "x$krb5_with" != "x" -a "$krb5_with" != "$KRBDIR"; then
    AC_MSG_WARN(Using $KRBDIR instead of requested value of $krb5_with for Kerberos!)
  fi

  AC_SUBST([KRBDIR])
  AC_SUBST([KRBLIBS])
  AC_SUBST([KRBCFLAGS])
  AC_SUBST([KRBLDFLAGS])
  AC_SUBST([K5VERS])

])
