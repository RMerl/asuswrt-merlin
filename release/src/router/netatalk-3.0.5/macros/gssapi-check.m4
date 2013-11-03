dnl Autoconf macro to check for kerberos

AC_DEFUN([NETATALK_GSSAPI_CHECK], 
[
    FOUND_GSSAPI=no
    GSSAPI_LIBS=""
    GSSAPI_CFLAGS=""
    GSSAPI_LDFLAGS=""
    save_CFLAGS="$CFLAGS"
    save_LDFLAGS="$LDFLAGS"
    save_LIBS="$LIBS"

    AC_ARG_WITH(gssapi,
        [  --with-gssapi[[=PATH]]    path to GSSAPI for Kerberos V UAM [[auto]]],
        [compilegssapi=$withval],
        [compilegssapi=auto]
    )

    if test x"$compilegssapi" != x"no" ; then
        if test "x$compilegssapi" != "xyes" -a "x$compilegssapi" != "xauto" ; then
            export CFLAGS="-I$withval/include"
            export LDFLAGS="-L$withval/${atalk_libname}"
            AC_MSG_NOTICE([checking for GSSAPI support in $compilegssapi])
        fi

        if test x"$compilegssapi" = x"yes" -o x"$compilegssapi" = x"auto" ; then
            # check for krb5-config from recent MIT and Heimdal kerberos 5
            AC_PATH_PROG(KRB5_CONFIG, krb5-config)
            AC_MSG_CHECKING([for working krb5-config that takes --libs gssapi])

            if test -x "$KRB5_CONFIG" ; then 
                TEMP="`$KRB5_CONFIG --libs gssapi`"
                if test $? -eq 0 ; then 
                    GSSAPI_CFLAGS="`$KRB5_CONFIG --cflags | sed s/@INCLUDE_des@//`"
                    GSSAPI_LIBS="$TEMP"
                    FOUND_GSSAPI=yes
                    AC_MSG_RESULT(yes)
                else
                    AC_MSG_RESULT(no. Fallback to previous krb5 detection strategy)
                fi
            else
                AC_MSG_RESULT(no. Fallback to previous krb5 detection strategy)
            fi
        fi
    fi

    if test x"$compilegssapi" != x"no" -a x"$FOUND_GSSAPI" = x"no" ; then
        # check for gssapi headers
        gss_headers_found=no
        AC_CHECK_HEADERS(gssapi.h gssapi/gssapi_generic.h gssapi/gssapi.h gssapi/gssapi_krb5.h,
            [gss_headers_found=yes])
        if test x"$gss_headers_found" = x"no" ; then
            AC_MSG_ERROR([GSSAPI installation not found, headers missing])
        fi
        # check for libs
        AC_SEARCH_LIBS(gss_display_status, [gss gssapi gssapi_krb5])
        if test x"$ac_cv_search_gss_display_status" = x"no" ; then
            AC_MSG_ERROR([GSSAPI installation not found, library missing])
        fi
        GSSAPI_CFLAGS="$CFLAGS"
        GSSAPI_LIBS="$LIBS"
        FOUND_GSSAPI=yes
    fi

    if test x"$FOUND_GSSAPI" = x"yes" ; then
        # check for functions
        export CFLAGS="$GSSAPI_CFLAGS"
        export LIBS="$GSSAPI_LIBS"
        AC_CHECK_FUNC(gss_acquire_cred, [], [AC_MSG_ERROR([GSSAPI: required function gss_acquire_cred missing])])

        # Heimdal/MIT compatibility fix
        if test "$ac_cv_header_gssapi_h" = "yes" ; then
            AC_EGREP_HEADER(
                GSS_C_NT_HOSTBASED_SERVICE,
                gssapi.h,
                AC_DEFINE(HAVE_GSS_C_NT_HOSTBASED_SERVICE, 1, [Wheter GSS_C_NT_HOSTBASED_SERVICE is in gssapi.h])
            )
        else
            AC_EGREP_HEADER(
                GSS_C_NT_HOSTBASED_SERVICE,
                gssapi/gssapi.h,
                AC_DEFINE(HAVE_GSS_C_NT_HOSTBASED_SERVICE, 1, [Wheter GSS_C_NT_HOSTBASED_SERVICE is in gssapi.h])
            )
        fi

        AC_DEFINE(HAVE_GSSAPI, 1, [Whether to enable GSSAPI support])
        if test x"$ac_cv_func_gss_acquire_cred" = x"yes" ; then
                ifelse([$1], , :, [$1])
        else
                ifelse([$2], , :, [$2])
        fi
    fi

    AC_SUBST(GSSAPI_LIBS)
    AC_SUBST(GSSAPI_CFLAGS)
    AC_SUBST(GSSAPI_LDFLAGS)

    export LIBS="$save_LIBS"
    export CFLAGS="$save_CFLAGS"
    export LDFLAGS="$save_LDFLAGS"
])
