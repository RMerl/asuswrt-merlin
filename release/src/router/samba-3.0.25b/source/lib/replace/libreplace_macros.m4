#
# This is a collection of useful autoconf macros
#

############################################
# Check if the compiler handles c99 struct initialization, and if not try -AC99 and -c99 flags
# Usage: LIBREPLACE_C99_STRUCT_INIT(success-action,failure-action)
# changes CFLAGS to add -AC99 or -c99 if needed
AC_DEFUN([LIBREPLACE_C99_STRUCT_INIT],
[
saved_CFLAGS="$CFLAGS";
c99_init=no
if test x"$c99_init" = x"no"; then
    AC_MSG_CHECKING(for C99 designated initializers)
    CFLAGS="$saved_CFLAGS";
    AC_TRY_COMPILE([#include <stdio.h>],
     [ struct foo {int x;char y;};
       struct foo bar = { .y = 'X', .x = 1 };	 
     ],
     [AC_MSG_RESULT(yes); c99_init=yes],[AC_MSG_RESULT(no)])
fi
if test x"$c99_init" = x"no"; then
    AC_MSG_CHECKING(for C99 designated initializers with -AC99)
    CFLAGS="$saved_CFLAGS -AC99";
    AC_TRY_COMPILE([#include <stdio.h>],
     [ struct foo {int x;char y;};
       struct foo bar = { .y = 'X', .x = 1 };	 
     ],
     [AC_MSG_RESULT(yes); c99_init=yes],[AC_MSG_RESULT(no)])
fi
if test x"$c99_init" = x"no"; then
    AC_MSG_CHECKING(for C99 designated initializers with -qlanglvl=extc99)
    CFLAGS="$saved_CFLAGS -qlanglvl=extc99";
    AC_TRY_COMPILE([#include <stdio.h>],
     [ struct foo {int x;char y;};
       struct foo bar = { .y = 'X', .x = 1 };	 
     ],
     [AC_MSG_RESULT(yes); c99_init=yes],[AC_MSG_RESULT(no)])
fi
if test x"$c99_init" = x"no"; then
    AC_MSG_CHECKING(for C99 designated initializers with -qlanglvl=stdc99)
    CFLAGS="$saved_CFLAGS -qlanglvl=stdc99";
    AC_TRY_COMPILE([#include <stdio.h>],
     [ struct foo {int x;char y;};
       struct foo bar = { .y = 'X', .x = 1 };	 
     ],
     [AC_MSG_RESULT(yes); c99_init=yes],[AC_MSG_RESULT(no)])
fi
if test x"$c99_init" = x"no"; then
    AC_MSG_CHECKING(for C99 designated initializers with -c99)
    CFLAGS="$saved_CFLAGS -c99"
    AC_TRY_COMPILE([#include <stdio.h>],
     [ struct foo {int x;char y;};
       struct foo bar = { .y = 'X', .x = 1 };	 
     ],
     [AC_MSG_RESULT(yes); c99_init=yes],[AC_MSG_RESULT(no)])
fi

if test "`uname`" = "HP-UX"; then
  if test "$ac_cv_c_compiler_gnu" = no; then
	# special override for broken HP-UX compiler - I can't find a way to test
	# this properly (its a compiler bug)
	CFLAGS="$CFLAGS -AC99";
	c99_init=yes;
  fi
fi

if test x"$c99_init" = x"yes"; then
    saved_CFLAGS=""
    $1
else
    CFLAGS="$saved_CFLAGS"
    saved_CFLAGS=""
    $2
fi
])

dnl AC_PROG_CC_FLAG(flag)
AC_DEFUN(AC_PROG_CC_FLAG,
[AC_CACHE_CHECK(whether ${CC-cc} accepts -$1, ac_cv_prog_cc_$1,
[echo 'void f(){}' > conftest.c
if test -z "`${CC-cc} -$1 -c conftest.c 2>&1`"; then
  ac_cv_prog_cc_$1=yes
else
  ac_cv_prog_cc_$1=no
fi
rm -f conftest*
])])

AC_DEFUN([AC_EXTENSION_FLAG],
[
  cat >>confdefs.h <<\EOF
#ifndef $1
# define $1 1
#endif
EOF
AH_VERBATIM([$1], [#ifndef $1
# define $1 1
#endif])
])


dnl see if a declaration exists for a function or variable
dnl defines HAVE_function_DECL if it exists
dnl AC_HAVE_DECL(var, includes)
AC_DEFUN(AC_HAVE_DECL,
[
 AC_CACHE_CHECK([for $1 declaration],ac_cv_have_$1_decl,[
    AC_TRY_COMPILE([$2],[int i = (int)$1],
        ac_cv_have_$1_decl=yes,ac_cv_have_$1_decl=no)])
 if test x"$ac_cv_have_$1_decl" = x"yes"; then
    AC_DEFINE([HAVE_]translit([$1], [a-z], [A-Z])[_DECL],1,[Whether $1() is available])
 fi
])


# AC_CHECK_LIB_EXT(LIBRARY, [EXT_LIBS], [FUNCTION],
#              [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND],
#              [ADD-ACTION-IF-FOUND],[OTHER-LIBRARIES])
# ------------------------------------------------------
#
# Use a cache variable name containing both the library and function name,
# because the test really is for library $1 defining function $3, not
# just for library $1.  Separate tests with the same $1 and different $3s
# may have different results.
#
# Note that using directly AS_VAR_PUSHDEF([ac_Lib], [ac_cv_lib_$1_$3])
# is asking for trouble, since AC_CHECK_LIB($lib, fun) would give
# ac_cv_lib_$lib_fun, which is definitely not what was meant.  Hence
# the AS_LITERAL_IF indirection.
#
# FIXME: This macro is extremely suspicious.  It DEFINEs unconditionally,
# whatever the FUNCTION, in addition to not being a *S macro.  Note
# that the cache does depend upon the function we are looking for.
#
# It is on purpose we used `ac_check_lib_ext_save_LIBS' and not just
# `ac_save_LIBS': there are many macros which don't want to see `LIBS'
# changed but still want to use AC_CHECK_LIB_EXT, so they save `LIBS'.
# And ``ac_save_LIBS' is too tempting a name, so let's leave them some
# freedom.
AC_DEFUN([AC_CHECK_LIB_EXT],
[
AH_CHECK_LIB_EXT([$1])
ac_check_lib_ext_save_LIBS=$LIBS
LIBS="-l$1 $$2 $7 $LIBS"
AS_LITERAL_IF([$1],
      [AS_VAR_PUSHDEF([ac_Lib_ext], [ac_cv_lib_ext_$1])],
      [AS_VAR_PUSHDEF([ac_Lib_ext], [ac_cv_lib_ext_$1''])])dnl

m4_ifval([$3],
 [
    AH_CHECK_FUNC_EXT([$3])
    AS_LITERAL_IF([$1],
              [AS_VAR_PUSHDEF([ac_Lib_func], [ac_cv_lib_ext_$1_$3])],
              [AS_VAR_PUSHDEF([ac_Lib_func], [ac_cv_lib_ext_$1''_$3])])dnl
    AC_CACHE_CHECK([for $3 in -l$1], ac_Lib_func,
	[AC_TRY_LINK_FUNC($3,
                 [AS_VAR_SET(ac_Lib_func, yes);
		  AS_VAR_SET(ac_Lib_ext, yes)],
                 [AS_VAR_SET(ac_Lib_func, no);
		  AS_VAR_SET(ac_Lib_ext, no)])
	])
    AS_IF([test AS_VAR_GET(ac_Lib_func) = yes],
        [AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_$3))])dnl
    AS_VAR_POPDEF([ac_Lib_func])dnl
 ],[
    AC_CACHE_CHECK([for -l$1], ac_Lib_ext,
	[AC_TRY_LINK_FUNC([main],
                 [AS_VAR_SET(ac_Lib_ext, yes)],
                 [AS_VAR_SET(ac_Lib_ext, no)])
	])
 ])
LIBS=$ac_check_lib_ext_save_LIBS

AS_IF([test AS_VAR_GET(ac_Lib_ext) = yes],
    [m4_default([$4], 
        [AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_LIB$1))
		case "$$2" in
		    *-l$1*)
			;;
		    *)
			$2="-l$1 $$2"
			;;
		esac])
		[$6]
	    ],
	    [$5])dnl
AS_VAR_POPDEF([ac_Lib_ext])dnl
])# AC_CHECK_LIB_EXT

# AH_CHECK_LIB_EXT(LIBNAME)
# ---------------------
m4_define([AH_CHECK_LIB_EXT],
[AH_TEMPLATE(AS_TR_CPP(HAVE_LIB$1),
             [Define to 1 if you have the `]$1[' library (-l]$1[).])])

dnl AC_SEARCH_LIBS_EXT(FUNCTION, SEARCH-LIBS, EXT_LIBS,
dnl                    [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND],
dnl                    [OTHER-LIBRARIES])
dnl --------------------------------------------------------
dnl Search for a library defining FUNC, if it's not already available.
AC_DEFUN([AC_SEARCH_LIBS_EXT],
[AC_CACHE_CHECK([for library containing $1], [ac_cv_search_ext_$1],
[
ac_func_search_ext_save_LIBS=$LIBS
ac_cv_search_ext_$1=no
AC_LINK_IFELSE([AC_LANG_CALL([], [$1])],
	       [ac_cv_search_ext_$1="none required"])
if test "$ac_cv_search_ext_$1" = no; then
  for ac_lib in $2; do
    LIBS="-l$ac_lib $$3 $6 $ac_func_search_save_ext_LIBS"
    AC_LINK_IFELSE([AC_LANG_CALL([], [$1])],
		   [ac_cv_search_ext_$1="-l$ac_lib"
break])
  done
fi
LIBS=$ac_func_search_ext_save_LIBS])
AS_IF([test "$ac_cv_search_ext_$1" != no],
  [test "$ac_cv_search_ext_$1" = "none required" || $3="$ac_cv_search_ext_$1 $$3"
  $4],
      [$5])dnl
])

dnl check for a function in a $LIBS and $OTHER_LIBS libraries variable.
dnl AC_CHECK_FUNC_EXT(func,OTHER_LIBS,IF-TRUE,IF-FALSE)
AC_DEFUN([AC_CHECK_FUNC_EXT],
[
    AH_CHECK_FUNC_EXT($1)	
    ac_check_func_ext_save_LIBS=$LIBS
    LIBS="$2 $LIBS"
    AS_VAR_PUSHDEF([ac_var], [ac_cv_func_ext_$1])dnl
    AC_CACHE_CHECK([for $1], ac_var,
	[AC_LINK_IFELSE([AC_LANG_FUNC_LINK_TRY([$1])],
                [AS_VAR_SET(ac_var, yes)],
                [AS_VAR_SET(ac_var, no)])])
    LIBS=$ac_check_func_ext_save_LIBS
    AS_IF([test AS_VAR_GET(ac_var) = yes], 
	    [AC_DEFINE_UNQUOTED(AS_TR_CPP([HAVE_$1])) $3], 
	    [$4])dnl
AS_VAR_POPDEF([ac_var])dnl
])# AC_CHECK_FUNC

# AH_CHECK_FUNC_EXT(FUNCNAME)
# ---------------------
m4_define([AH_CHECK_FUNC_EXT],
[AH_TEMPLATE(AS_TR_CPP(HAVE_$1),
             [Define to 1 if you have the `]$1[' function.])])

dnl Define an AC_DEFINE with ifndef guard.
dnl AC_N_DEFINE(VARIABLE [, VALUE])
define(AC_N_DEFINE,
[cat >> confdefs.h <<\EOF
[#ifndef] $1
[#define] $1 ifelse($#, 2, [$2], $#, 3, [$2], 1)
[#endif]
EOF
])

dnl Add an #include
dnl AC_ADD_INCLUDE(VARIABLE)
define(AC_ADD_INCLUDE,
[cat >> confdefs.h <<\EOF
[#include] $1
EOF
])

dnl remove an #include
dnl AC_REMOVE_INCLUDE(VARIABLE)
define(AC_REMOVE_INCLUDE,
[
grep -v '[#include] $1' confdefs.h >confdefs.h.tmp
cat confdefs.h.tmp > confdefs.h
rm confdefs.h.tmp
])

dnl remove an #define
dnl AC_REMOVE_DEFINE(VARIABLE)
define(AC_REMOVE_DEFINE,
[
grep -v '[#define] $1 ' confdefs.h |grep -v '[#define] $1[$]'>confdefs.h.tmp
cat confdefs.h.tmp > confdefs.h
rm confdefs.h.tmp
])

dnl AS_HELP_STRING is not available in autoconf 2.57, and AC_HELP_STRING is deprecated
dnl in autoconf 2.59, so define AS_HELP_STRING to be AC_HELP_STRING unless it is already
dnl defined.
m4_ifdef([AS_HELP_STRING], , [m4_define([AS_HELP_STRING], m4_defn([AC_HELP_STRING]))])

dnl check if the prototype in the header matches the given one
dnl AC_VERIFY_C_PROTOTYPE(prototype,functionbody,[IF-TRUE].[IF-FALSE],[extraheaders])
AC_DEFUN(AC_VERIFY_C_PROTOTYPE,
[AC_CACHE_CHECK([for prototype $1], AS_TR_SH([ac_cv_c_prototype_$1]),
	AC_COMPILE_IFELSE([
		AC_INCLUDES_DEFAULT
		$5
		$1
		{
			$2
		}
	],[
		AS_TR_SH([ac_cv_c_prototype_$1])=yes
	],[
		AS_TR_SH([ac_cv_c_prototype_$1])=no
	])
)
AS_IF([test $AS_TR_SH([ac_cv_c_prototype_$1]) = yes],[$3],[$4])
])

AC_DEFUN(LIBREPLACE_PROVIDE_HEADER, 
[AC_CHECK_HEADER([$1], 
		[ AC_CONFIG_COMMANDS(rm-$1, [rm -f $libreplacedir/$1], [libreplacedir=$libreplacedir]) ],
		[ AC_CONFIG_COMMANDS(mk-$1, [echo "#include \"replace.h\"" > $libreplacedir/$1], [libreplacedir=$libreplacedir]) ]
	)
])


