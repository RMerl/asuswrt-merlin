dnl @synopsis AX_CFLAGS_IRIX_OPTION (optionflag [,[shellvar][,[A][,[NA]]])
dnl
dnl AX_CFLAGS_IRIX_OPTION(-go_for_it) would show a message as like
dnl "checking CFLAGS for irix/cc -go_for_it ... yes" and adds the
dnl optionflag to CFLAGS if it is understood. You can override the
dnl shellvar-default of CFLAGS of course. The order of arguments stems
dnl from the explicit macros like AX_CFLAGS_WARN_ALL.
dnl
dnl The cousin AX_CXXFLAGS_IRIX_OPTION would check for an option to add
dnl to CXXFLAGS - and it uses the autoconf setup for C++ instead of C
dnl (since it is possible to use different compilers for C and C++).
dnl
dnl The macro is a lot simpler than any special AX_CFLAGS_* macro (or
dnl ac_cxx_rtti.m4 macro) but allows to check for arbitrary options.
dnl However, if you use this macro in a few places, it would be great
dnl if you would make up a new function-macro and submit it to the
dnl ac-archive.
dnl
dnl  - $1 option-to-check-for : required ("-option" as non-value)
dnl  - $2 shell-variable-to-add-to : CFLAGS (or CXXFLAGS in the other case)
dnl  - $3 action-if-found : add value to shellvariable
dnl  - $4 action-if-not-found : nothing
dnl
dnl note: in earlier versions, $1-$2 were swapped. We try to detect the
dnl situation and accept a $2=~/-/ as being the old
dnl option-to-check-for.
dnl
dnl see also: AX_CFLAGS_GCC_OPTION for the widely used original
dnl variant.
dnl
dnl @category C
dnl @author Guido Draheim <guidod@gmx.de>
dnl @version 2005-01-21
dnl @license GPLWithACException

AC_DEFUN([AX_CFLAGS_IRIX_OPTION_OLD], [dnl
AS_VAR_PUSHDEF([FLAGS],[CFLAGS])dnl
AS_VAR_PUSHDEF([VAR],[ac_cv_cflags_irix_option_$2])dnl
AC_CACHE_CHECK([m4_ifval($1,$1,FLAGS) for irix/cc m4_ifval($2,$2,-option)],
VAR,[VAR="no, unknown"
 AC_LANG_SAVE
 AC_LANG_C
 ac_save_[]FLAGS="$[]FLAGS"
for ac_arg dnl
in "-fullwarn -use_readonly_const % m4_ifval($2,$2,-option)"     dnl IRIX C
   #
do FLAGS="$ac_save_[]FLAGS "`echo $ac_arg | sed -e 's,%%.*,,' -e 's,%,,'`
   AC_TRY_COMPILE([],[return 0;],
   [VAR=`echo $ac_arg | sed -e 's,.*% *,,'` ; break])
done
 FLAGS="$ac_save_[]FLAGS"
 AC_LANG_RESTORE
])
case ".$VAR" in
     .ok|.ok,*) m4_ifvaln($3,$3) ;;
   .|.no|.no,*) m4_ifvaln($4,$4) ;;
   *) m4_ifvaln($3,$3,[
   if echo " $[]m4_ifval($1,$1,FLAGS) " | grep " $VAR " 2>&1 >/dev/null
   then AC_RUN_LOG([: m4_ifval($1,$1,FLAGS) does contain $VAR])
   else AC_RUN_LOG([: m4_ifval($1,$1,FLAGS)="$m4_ifval($1,$1,FLAGS) $VAR"])
                      m4_ifval($1,$1,FLAGS)="$m4_ifval($1,$1,FLAGS) $VAR"
   fi ]) ;;
esac
AS_VAR_POPDEF([VAR])dnl
AS_VAR_POPDEF([FLAGS])dnl
])

dnl the only difference - the LANG selection... and the default FLAGS

AC_DEFUN([AX_CXXFLAGS_IRIX_OPTION_OLD], [dnl
AS_VAR_PUSHDEF([FLAGS],[CXXFLAGS])dnl
AS_VAR_PUSHDEF([VAR],[ac_cv_cxxflags_irix_option_$2])dnl
AC_CACHE_CHECK([m4_ifval($1,$1,FLAGS) for irix/cc m4_ifval($2,$2,-option)],
VAR,[VAR="no, unknown"
 AC_LANG_SAVE
 AC_LANG_CXX
 ac_save_[]FLAGS="$[]FLAGS"
for ac_arg dnl
in "-fullwarn -use_readonly_const % m4_ifval($2,$2,-option)"     dnl IRIX C
   #
do FLAGS="$ac_save_[]FLAGS "`echo $ac_arg | sed -e 's,%%.*,,' -e 's,%,,'`
   AC_TRY_COMPILE([],[return 0;],
   [VAR=`echo $ac_arg | sed -e 's,.*% *,,'` ; break])
done
 FLAGS="$ac_save_[]FLAGS"
 AC_LANG_RESTORE
])
case ".$VAR" in
     .ok|.ok,*) m4_ifvaln($3,$3) ;;
   .|.no|.no,*) m4_ifvaln($4,$4) ;;
   *) m4_ifvaln($3,$3,[
   if echo " $[]m4_ifval($1,$1,FLAGS) " | grep " $VAR " 2>&1 >/dev/null
   then AC_RUN_LOG([: m4_ifval($1,$1,FLAGS) does contain $VAR])
   else AC_RUN_LOG([: m4_ifval($1,$1,FLAGS)="$m4_ifval($1,$1,FLAGS) $VAR"])
                      m4_ifval($1,$1,FLAGS)="$m4_ifval($1,$1,FLAGS) $VAR"
   fi ]) ;;
esac
AS_VAR_POPDEF([VAR])dnl
AS_VAR_POPDEF([FLAGS])dnl
])

dnl --------------------------------------------------------------------------

AC_DEFUN([AX_CFLAGS_IRIX_OPTION_NEW], [dnl
AS_VAR_PUSHDEF([FLAGS],[CFLAGS])dnl
AS_VAR_PUSHDEF([VAR],[ac_cv_cflags_irix_option_$1])dnl
AC_CACHE_CHECK([m4_ifval($2,$2,FLAGS) for irix/cc m4_ifval($1,$1,-option)],
VAR,[VAR="no, unknown"
 AC_LANG_SAVE
 AC_LANG_C
 ac_save_[]FLAGS="$[]FLAGS"
for ac_arg dnl
in "-fullwarn -use_readonly_const % m4_ifval($1,$1,-option)"     dnl IRIX C
   #
do FLAGS="$ac_save_[]FLAGS "`echo $ac_arg | sed -e 's,%%.*,,' -e 's,%,,'`
   AC_TRY_COMPILE([],[return 0;],
   [VAR=`echo $ac_arg | sed -e 's,.*% *,,'` ; break])
done
 FLAGS="$ac_save_[]FLAGS"
 AC_LANG_RESTORE
])
case ".$VAR" in
     .ok|.ok,*) m4_ifvaln($3,$3) ;;
   .|.no|.no,*) m4_ifvaln($4,$4) ;;
   *) m4_ifvaln($3,$3,[
   if echo " $[]m4_ifval($2,$2,FLAGS) " | grep " $VAR " 2>&1 >/dev/null
   then AC_RUN_LOG([: m4_ifval($2,$2,FLAGS) does contain $VAR])
   else AC_RUN_LOG([: m4_ifval($2,$2,FLAGS)="$m4_ifval($2,$2,FLAGS) $VAR"])
                      m4_ifval($2,$2,FLAGS)="$m4_ifval($2,$2,FLAGS) $VAR"
   fi ]) ;;
esac
AS_VAR_POPDEF([VAR])dnl
AS_VAR_POPDEF([FLAGS])dnl
])

dnl the only difference - the LANG selection... and the default FLAGS

AC_DEFUN([AX_CXXFLAGS_IRIX_OPTION_NEW], [dnl
AS_VAR_PUSHDEF([FLAGS],[CXXFLAGS])dnl
AS_VAR_PUSHDEF([VAR],[ac_cv_cxxflags_irix_option_$1])dnl
AC_CACHE_CHECK([m4_ifval($2,$2,FLAGS) for irix/cc m4_ifval($1,$1,-option)],
VAR,[VAR="no, unknown"
 AC_LANG_SAVE
 AC_LANG_CXX
 ac_save_[]FLAGS="$[]FLAGS"
for ac_arg dnl
in "-fullwarn -use_readonly_const % m4_ifval($1,$1,-option)"     dnl IRIX C
   #
do FLAGS="$ac_save_[]FLAGS "`echo $ac_arg | sed -e 's,%%.*,,' -e 's,%,,'`
   AC_TRY_COMPILE([],[return 0;],
   [VAR=`echo $ac_arg | sed -e 's,.*% *,,'` ; break])
done
 FLAGS="$ac_save_[]FLAGS"
 AC_LANG_RESTORE
])
case ".$VAR" in
     .ok|.ok,*) m4_ifvaln($3,$3) ;;
   .|.no|.no,*) m4_ifvaln($4,$4) ;;
   *) m4_ifvaln($3,$3,[
   if echo " $[]m4_ifval($2,$2,FLAGS) " | grep " $VAR " 2>&1 >/dev/null
   then AC_RUN_LOG([: m4_ifval($2,$2,FLAGS) does contain $VAR])
   else AC_RUN_LOG([: m4_ifval($2,$2,FLAGS)="$m4_ifval($2,$2,FLAGS) $VAR"])
                      m4_ifval($2,$2,FLAGS)="$m4_ifval($2,$2,FLAGS) $VAR"
   fi ]) ;;
esac
AS_VAR_POPDEF([VAR])dnl
AS_VAR_POPDEF([FLAGS])dnl
])

AC_DEFUN([AX_CFLAGS_IRIX_OPTION],[ifelse(m4_bregexp([$2],[-]),-1,
[AX_CFLAGS_IRIX_OPTION_NEW($@)],[AX_CFLAGS_IRIX_OPTION_OLD($@)])])

AC_DEFUN([AX_CXXFLAGS_IRIX_OPTION],[ifelse(m4_bregexp([$2],[-]),-1,
[AX_CXXFLAGS_IRIX_OPTION_NEW($@)],[AX_CXXFLAGS_IRIX_OPTION_OLD($@)])])
