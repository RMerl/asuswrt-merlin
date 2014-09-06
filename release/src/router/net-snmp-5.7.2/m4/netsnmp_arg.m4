dnl Reports an invalid option and suggests --enable-<ARGUMENT 1> instead
AC_DEFUN([NETSNMP_INVALID_ENABLE],
  [AC_MSG_ERROR([Invalid option. Use --enable-$1/--disable-$1 instead])])

dnl Reports an invalid option and suggests --with-<ARGUMENT 1> instead
AC_DEFUN([NETSNMP_INVALID_WITH],
  [AC_MSG_ERROR([Invalid option. Use --with-$1/--without-$1 instead])])

dnl Similar to AC_ARG_ENABLE but also defines a matching WITH option that
dnl suggests the use of the ENABLE option if called
AC_DEFUN([NETSNMP_ARG_ENABLE],
  [AC_ARG_ENABLE([$1],[$2]dnl
     m4_if(m4_eval($# < 3),1,[],[,[$3]])dnl
     m4_if(m4_eval($# < 4),1,[],[,[$4]]))
   AC_ARG_WITH(
     [$1],,
     [NETSNMP_INVALID_ENABLE([$1])])])

dnl Similar to AC_ARG_WITH but also defines a matching ENABLE option that
dnl suggests the use of the WITH option if called
AC_DEFUN([NETSNMP_ARG_WITH],
  [AC_ARG_WITH([$1],[$2]dnl
     m4_if(m4_eval($# < 3),1,[],[,[$3]])dnl
     m4_if(m4_eval($# < 4),1,[],[,[$4]]))
   AC_ARG_ENABLE(
     [$1],,
     [NETSNMP_INVALID_WITH([$1])])])
