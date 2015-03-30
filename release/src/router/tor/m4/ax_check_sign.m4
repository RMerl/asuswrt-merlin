# ===========================================================================
#       http://www.gnu.org/software/autoconf-archive/ax_check_sign.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_CHECK_SIGN (TYPE, [ACTION-IF-SIGNED], [ACTION-IF-UNSIGNED], [INCLUDES])
#
# DESCRIPTION
#
#   Checks whether TYPE is signed or not. If no INCLUDES are specified, the
#   default includes are used. If ACTION-IF-SIGNED is given, it is
#   additional shell code to execute when the type is signed. If
#   ACTION-IF-UNSIGNED is given, it is executed when the type is unsigned.
#
#   This macro assumes that the type exists. Therefore the existence of the
#   type should be checked before calling this macro. For example:
#
#     AC_CHECK_HEADERS([wchar.h])
#     AC_CHECK_TYPE([wchar_t],,[ AC_MSG_ERROR([Type wchar_t not found.]) ])
#     AX_CHECK_SIGN([wchar_t],
#       [ AC_DEFINE(WCHAR_T_SIGNED, 1, [Define if wchar_t is signed]) ],
#       [ AC_DEFINE(WCHAR_T_UNSIGNED, 1, [Define if wchar_t is unsigned]) ], [
#     #ifdef HAVE_WCHAR_H
#     #include <wchar.h>
#     #endif
#     ])
#
# LICENSE
#
#   Copyright (c) 2008 Ville Laurikari <vl@iki.fi>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 6

AU_ALIAS([VL_CHECK_SIGN], [AX_CHECK_SIGN])
AC_DEFUN([AX_CHECK_SIGN], [
 typename=`echo $1 | sed "s/@<:@^a-zA-Z0-9_@:>@/_/g"`
 AC_CACHE_CHECK([whether $1 is signed], ax_cv_decl_${typename}_signed, [
   AC_TRY_COMPILE([$4],
     [ int foo @<:@ 1 - 2 * !((($1) -1) < 0) @:>@ ],
     [ eval "ax_cv_decl_${typename}_signed=\"yes\"" ],
     [ eval "ax_cv_decl_${typename}_signed=\"no\"" ])])
 symbolname=`echo $1 | sed "s/@<:@^a-zA-Z0-9_@:>@/_/g" | tr "a-z" "A-Z"`
 if eval "test \"\${ax_cv_decl_${typename}_signed}\" = \"yes\""; then
   $2
 elif eval "test \"\${ax_cv_decl_${typename}_signed}\" = \"no\""; then
   $3
 fi
])dnl
