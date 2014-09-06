dnl @synopsis NETSNMP_SEARCH_LIBS(FUNCTION, SEARCH-LIBS, [ACTION-IF-FOUND],
dnl             [ACTION-IF-NOT-FOUND], [OTHER-LIBRARIES], [TARGET-VARIABLE])
dnl Similar to AC_SEARCH_LIBS but changes TARGET-VARIABLE instead of LIBS
dnl If TARGET-VARIABLE is unset then LIBS is used
AC_DEFUN([NETSNMP_SEARCH_LIBS],
[m4_pushdef([netsnmp_target],m4_ifval([$6],[$6],[LIBS]))
 AC_CACHE_CHECK([for library containing $1],
    [netsnmp_cv_func_$1_]netsnmp_target,
    [netsnmp_func_search_save_LIBS="$LIBS"
     m4_if(netsnmp_target, [LIBS],
         [netsnmp_target_val="$LIBS"
          netsnmp_temp_LIBS="$5 ${LIBS}"],
         [netsnmp_target_val="$netsnmp_target"
          netsnmp_temp_LIBS="${netsnmp_target_val} $5 ${LIBS}"])
     netsnmp_result=no
     LIBS="${netsnmp_temp_LIBS}"
     AC_LINK_IFELSE([AC_LANG_CALL([],[$1])],
         [netsnmp_result="none required"],
         [for netsnmp_cur_lib in $2 ; do
              LIBS="-l${netsnmp_cur_lib} ${netsnmp_temp_LIBS}"
              AC_LINK_IFELSE([AC_LANG_CALL([],[$1])],
                  [netsnmp_result=-l${netsnmp_cur_lib}
                   break])
          done])
     LIBS="${netsnmp_func_search_save_LIBS}"
     [netsnmp_cv_func_$1_]netsnmp_target="${netsnmp_result}"])
 if test "${[netsnmp_cv_func_$1_]netsnmp_target}" != "no" ; then
    if test "${[netsnmp_cv_func_$1_]netsnmp_target}" != "none required" ; then
       netsnmp_target="${netsnmp_result} ${netsnmp_target_val}"
    fi
    $3
 m4_ifval([$4], [else
    $4])
 fi
 m4_popdef([netsnmp_target])])
