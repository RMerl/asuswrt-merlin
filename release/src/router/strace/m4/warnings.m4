# warnings.m4 serial 2
dnl Copyright (C) 2008, 2009, 2010 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Simon Josefsson

# gl_AS_VAR_APPEND(VAR, VALUE)
# ----------------------------
# Provide the functionality of AS_VAR_APPEND if Autoconf does not have it.
m4_ifdef([AS_VAR_APPEND],
[m4_copy([AS_VAR_APPEND], [gl_AS_VAR_APPEND])],
[m4_define([gl_AS_VAR_APPEND],
[AS_VAR_SET([$1], [AS_VAR_GET([$1])$2])])])

# gl_WARN_ADD(PARAMETER, [VARIABLE = WARN_CFLAGS])
# ------------------------------------------------
# Adds parameter to WARN_CFLAGS if the compiler supports it.  For example,
# gl_WARN_ADD([-Wparentheses]).
AC_DEFUN([gl_WARN_ADD],
[AS_VAR_PUSHDEF([gl_Warn], [gl_cv_warn_$1])dnl
AC_CACHE_CHECK([whether compiler handles $1], [gl_Warn], [
  save_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="${CPPFLAGS} $1"
  AC_PREPROC_IFELSE([AC_LANG_PROGRAM([])],
                    [AS_VAR_SET([gl_Warn], [yes])],
                    [AS_VAR_SET([gl_Warn], [no])])
  CPPFLAGS="$save_CPPFLAGS"
])
AS_VAR_PUSHDEF([gl_Flags], m4_if([$2], [], [[WARN_CFLAGS]], [[$2]]))dnl
AS_VAR_IF([gl_Warn], [yes], [gl_AS_VAR_APPEND([gl_Flags], [" $1"])])
AS_VAR_POPDEF([gl_Flags])dnl
AS_VAR_POPDEF([gl_Warn])dnl
m4_ifval([$2], [AS_LITERAL_IF([$2], [AC_SUBST([$2])], [])])dnl
])
