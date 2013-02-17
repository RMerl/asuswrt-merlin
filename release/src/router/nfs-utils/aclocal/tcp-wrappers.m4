# Check whether user wants TCP wrappers support
AC_DEFUN([AC_TCP_WRAPPERS],[
  TCPW_MSG="no"
  AC_ARG_WITH(tcp-wrappers,
    [  --with-tcp-wrappers[[=PATH]]      Enable tcpwrappers support
                 (optionally in PATH)],
    with_tcpw=$withval, with_tcpw=yes)
        if test "x$with_tcpw" != "xno" ; then
            saved_LIBS="$LIBS"
            saved_LDFLAGS="$LDFLAGS"
            saved_CPPFLAGS="$CPPFLAGS"
            if test -n "${with_tcpw}" -a "${with_tcpw}" != "yes"; then
                if test -d "${with_tcpw}/lib"; then
                    if test -n "${need_dash_r}"; then
                        LDFLAGS="-L${with_tcpw}/lib -R${with_tcpw}/lib ${LDFLAGS}"
                    else
                        LDFLAGS="-L${with_tcpw}/lib ${LDFLAGS}"
                    fi
                else
                    if test -n "${need_dash_r}"; then
                        LDFLAGS="-L${with_tcpw} -R${with_tcpw} ${LDFLAGS}"
                    else
                        LDFLAGS="-L${with_tcpw} ${LDFLAGS}"
                    fi
                fi
                if test -d "${with_tcpw}/include"; then
                    CPPFLAGS="-I${with_tcpw}/include ${CPPFLAGS}"
                else
                    CPPFLAGS="-I${with_tcpw} ${CPPFLAGS}"
                fi
            fi
            LIBWRAP="-lwrap"
            LIBS="$LIBWRAP $LIBS"
            AC_MSG_CHECKING(for libwrap)
            AC_LINK_IFELSE([AC_LANG_PROGRAM([[
                #include <tcpd.h>
                int deny_severity = 0, allow_severity = 0;
                ]], [[hosts_access(0);]])],[
                AC_MSG_RESULT(yes)
                AC_SUBST(LIBWRAP)
                AC_DEFINE([LIBWRAP], [1], [tcp-wrapper])
                AC_DEFINE([HAVE_LIBWRAP], [1], [tcp-wrapper])
                AC_DEFINE([HAVE_TCP_WRAPPER], [1], [tcp-wrapper])
                TCPW_MSG="yes"
                ],[
                AC_MSG_ERROR([*** libwrap missing])
                
            ])
            LIBS="$saved_LIBS"
        fi
  AC_SUBST(LIBWRAP)
  AC_SUBST(HAVE_LIBWRAP)
  AC_SUBST(HAVE_TCP_WRAPPER)
])
