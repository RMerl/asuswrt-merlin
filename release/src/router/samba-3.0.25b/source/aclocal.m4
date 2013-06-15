dnl test whether dirent has a d_off member
AC_DEFUN(AC_DIRENT_D_OFF,
[AC_CACHE_CHECK([for d_off in dirent], ac_cv_dirent_d_off,
[AC_TRY_COMPILE([
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>], [struct dirent d; d.d_off;],
ac_cv_dirent_d_off=yes, ac_cv_dirent_d_off=no)])
if test $ac_cv_dirent_d_off = yes; then
  AC_DEFINE(HAVE_DIRENT_D_OFF,1,[Whether dirent has a d_off member])
fi
])

dnl Mark specified module as shared
dnl SMB_MODULE(name,static_files,shared_files,subsystem,whatif-static,whatif-shared)
AC_DEFUN(SMB_MODULE,
[
	AC_MSG_CHECKING([how to build $1])
	if test "$[MODULE_][$1]"; then
		DEST=$[MODULE_][$1]
	elif test "$[MODULE_]translit([$4], [A-Z], [a-z])" -a "$[MODULE_DEFAULT_][$1]"; then
		DEST=$[MODULE_]translit([$4], [A-Z], [a-z])
	else
		DEST=$[MODULE_DEFAULT_][$1]
	fi
	
	if test x"$DEST" = xSHARED; then
		AC_DEFINE([$1][_init], [init_module], [Whether to build $1 as shared module])
		$4_MODULES="$$4_MODULES $3"
		AC_MSG_RESULT([shared])
		[$6]
		string_shared_modules="$string_shared_modules $1"
	elif test x"$DEST" = xSTATIC; then
		[init_static_modules_]translit([$4], [A-Z], [a-z])="$[init_static_modules_]translit([$4], [A-Z], [a-z])  $1_init();"
 		[decl_static_modules_]translit([$4], [A-Z], [a-z])="$[decl_static_modules_]translit([$4], [A-Z], [a-z]) extern NTSTATUS $1_init(void);"
		string_static_modules="$string_static_modules $1"
		$4_STATIC="$$4_STATIC $2"
		AC_SUBST($4_STATIC)
		[$5]
		AC_MSG_RESULT([static])
	else
	    string_ignored_modules="$string_ignored_modules $1"
		AC_MSG_RESULT([not])
	fi
])

AC_DEFUN(SMB_SUBSYSTEM,
[
	AC_SUBST($1_STATIC)
	AC_SUBST($1_MODULES)
	AC_DEFINE_UNQUOTED([static_init_]translit([$1], [A-Z], [a-z]), [{$init_static_modules_]translit([$1], [A-Z], [a-z])[}], [Static init functions])
	AC_DEFINE_UNQUOTED([static_decl_]translit([$1], [A-Z], [a-z]), [$decl_static_modules_]translit([$1], [A-Z], [a-z]), [Decl of Static init functions])
    	ifelse([$2], , :, [rm -f $2])
])

dnl AC_LIBTESTFUNC(lib, function, [actions if found], [actions if not found])
dnl Check for a function in a library, but don't keep adding the same library
dnl to the LIBS variable.  Check whether the function is available in the
dnl current LIBS before adding the library which prevents us spuriously
dnl adding libraries for symbols that are in libc.
dnl
dnl On success, the default actions ensure that HAVE_FOO is defined. The lib
dnl is always added to $LIBS if it was found to be necessary. The caller
dnl can use SMB_REMOVE_LIB to strp this if necessary.
AC_DEFUN([AC_LIBTESTFUNC],
[
  AC_CHECK_FUNCS($2,
      [
        # $2 was found in libc or existing $LIBS
	m4_ifval([$3],
	    [
		$3
	    ],
	    [
		AC_DEFINE(translit([HAVE_$2], [a-z], [A-Z]), 1,
		    [Whether $2 is available])
	    ])
      ],
      [
        # $2 was not found, try adding lib$1
	case " $LIBS " in
          *\ -l$1\ *)
	    m4_ifval([$4],
		[
		    $4
		],
		[
		    # $2 was not found and we already had lib$1
		    # nothing to do here by default
		    true
		])
	    ;;
          *)
	    # $2 was not found, try adding lib$1
	    AC_CHECK_LIB($1, $2,
	      [
		LIBS="-l$1 $LIBS"
		m4_ifval([$3],
		    [
			$3
		    ],
		    [
			AC_DEFINE(translit([HAVE_$2], [a-z], [A-Z]), 1,
			    [Whether $2 is available])
		    ])
	      ],
	      [
		m4_ifval([$4],
		    [
			$4
		    ],
		    [
			# $2 was not found in lib$1
			# nothing to do here by default
			true
		    ])
	      ])
	  ;;
        esac
      ])
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

# AC_CHECK_FUNCS_EXT(FUNCTION, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# -----------------------------------------------------------------
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

dnl Copied from libtool.m4
AC_DEFUN(AC_PROG_LD_GNU,
[AC_CACHE_CHECK([if the linker ($LD) is GNU ld], ac_cv_prog_gnu_ld,
[# I'd rather use --version here, but apparently some GNU ld's only accept -v.
if $LD -v 2>&1 </dev/null | egrep '(GNU|with BFD)' 1>&5; then
  ac_cv_prog_gnu_ld=yes
else
  ac_cv_prog_gnu_ld=no
fi])
])

dnl Removes -I/usr/include/? from given variable
AC_DEFUN(CFLAGS_REMOVE_USR_INCLUDE,[
  ac_new_flags=""
  for i in [$]$1; do
    case [$]i in
    -I/usr/include|-I/usr/include/) ;;
    *) ac_new_flags="[$]ac_new_flags [$]i" ;;
    esac
  done
  $1=[$]ac_new_flags
])

dnl Removes '-L/usr/lib[/]', '-Wl,-rpath,/usr/lib[/]'
dnl and '-Wl,-rpath -Wl,/usr/lib[/]' from given variable
AC_DEFUN(LIB_REMOVE_USR_LIB,[
  ac_new_flags=""
  l=""
  for i in [$]$1; do
    case [$]l[$]i in
    -L/usr/lib) ;;
    -L/usr/lib/) ;;
    -Wl,-rpath,/usr/lib) ;;
    -Wl,-rpath,/usr/lib/) ;;
    -Wl,-rpath) l=[$]i;;
    -Wl,-rpath-Wl,/usr/lib) l="";;
    -Wl,-rpath-Wl,/usr/lib/) l="";;
    *)
    	s=" "
        if test x"[$]ac_new_flags" = x""; then
            s="";
	fi
        if test x"[$]l" = x""; then
            ac_new_flags="[$]ac_new_flags[$]s[$]i";
        else
            ac_new_flags="[$]ac_new_flags[$]s[$]l [$]i";
        fi
        l=""
        ;;
    esac
  done
  $1=[$]ac_new_flags
])

dnl From Bruno Haible.

AC_DEFUN(jm_ICONV,
[
  dnl Some systems have iconv in libc, some have it in libiconv (OSF/1 and
  dnl those with the standalone portable libiconv installed).
  AC_MSG_CHECKING(for iconv in $1)
    jm_cv_func_iconv="no"
    jm_cv_lib_iconv=""
    jm_cv_giconv=no
    jm_save_LIBS="$LIBS"

    dnl Check for include in funny place but no lib needed
    if test "$jm_cv_func_iconv" != yes; then 
      AC_TRY_LINK([#include <stdlib.h>
#include <giconv.h>],
        [iconv_t cd = iconv_open("","");
         iconv(cd,NULL,NULL,NULL,NULL);
         iconv_close(cd);],
         jm_cv_func_iconv=yes
         jm_cv_include="giconv.h"
         jm_cv_giconv="yes"
         jm_cv_lib_iconv="")

      dnl Standard iconv.h include, lib in glibc or libc ...
      if test "$jm_cv_func_iconv" != yes; then
        AC_TRY_LINK([#include <stdlib.h>
#include <iconv.h>],
          [iconv_t cd = iconv_open("","");
           iconv(cd,NULL,NULL,NULL,NULL);
           iconv_close(cd);],
           jm_cv_include="iconv.h"
           jm_cv_func_iconv=yes
           jm_cv_lib_iconv="")

          if test "$jm_cv_lib_iconv" != yes; then
            jm_save_LIBS="$LIBS"
            LIBS="$LIBS -lgiconv"
            AC_TRY_LINK([#include <stdlib.h>
#include <giconv.h>],
              [iconv_t cd = iconv_open("","");
               iconv(cd,NULL,NULL,NULL,NULL);
               iconv_close(cd);],
              jm_cv_lib_iconv=yes
              jm_cv_func_iconv=yes
              jm_cv_include="giconv.h"
              jm_cv_giconv=yes
              jm_cv_lib_iconv="giconv")

           LIBS="$jm_save_LIBS"

        if test "$jm_cv_func_iconv" != yes; then
          jm_save_LIBS="$LIBS"
          LIBS="$LIBS -liconv"
          AC_TRY_LINK([#include <stdlib.h>
#include <iconv.h>],
            [iconv_t cd = iconv_open("","");
             iconv(cd,NULL,NULL,NULL,NULL);
             iconv_close(cd);],
            jm_cv_include="iconv.h"
            jm_cv_func_iconv=yes
            jm_cv_lib_iconv="iconv")
          LIBS="$jm_save_LIBS"

          if test "$jm_cv_lib_iconv" != yes; then
            jm_save_LIBS="$LIBS"
            LIBS="$LIBS -lbiconv"
            AC_TRY_LINK([#include <stdlib.h>
#include <biconv.h>],
              [iconv_t cd = iconv_open("","");
               iconv(cd,NULL,NULL,NULL,NULL);
               iconv_close(cd);],
              jm_cv_lib_iconv=yes
              jm_cv_func_iconv=yes
              jm_cv_include="biconv.h"
              jm_cv_biconv=yes
              jm_cv_lib_iconv="biconv")

            LIBS="$jm_save_LIBS"
	  fi
        fi
      fi
    fi
  fi
  if test "$jm_cv_func_iconv" = yes; then
    if test "$jm_cv_giconv" = yes; then
      AC_DEFINE(HAVE_GICONV, 1, [What header to include for iconv() function: giconv.h])
      AC_MSG_RESULT(yes)
      ICONV_FOUND=yes
    else
      if test "$jm_cv_biconv" = yes; then
        AC_DEFINE(HAVE_BICONV, 1, [What header to include for iconv() function: biconv.h])
        AC_MSG_RESULT(yes)
        ICONV_FOUND=yes
      else 
        AC_DEFINE(HAVE_ICONV, 1, [What header to include for iconv() function: iconv.h])
        AC_MSG_RESULT(yes)
        ICONV_FOUND=yes
      fi
    fi
  else
    AC_MSG_RESULT(no)
  fi
])

AC_DEFUN(rjs_CHARSET,[
  dnl Find out if we can convert from $1 to UCS2-LE
  AC_MSG_CHECKING([can we convert from $1 to UCS2-LE?])
  AC_TRY_RUN([
#include <$jm_cv_include>
main(){
    iconv_t cd = iconv_open("$1", "UCS-2LE");
    if (cd == 0 || cd == (iconv_t)-1) {
	return -1;
    }
    return 0;
}
  ],ICONV_CHARSET=$1,ICONV_CHARSET=no,ICONV_CHARSET=cross)
  AC_MSG_RESULT($ICONV_CHARSET)
])

dnl CFLAGS_ADD_DIR(CFLAGS, $INCDIR)
dnl This function doesn't add -I/usr/include into CFLAGS
AC_DEFUN(CFLAGS_ADD_DIR,[
if test "$2" != "/usr/include" ; then
    $1="$$1 -I$2"
fi
])

dnl LIB_ADD_DIR(LDFLAGS, $LIBDIR)
dnl This function doesn't add -L/usr/lib into LDFLAGS
AC_DEFUN(LIB_ADD_DIR,[
if test "$2" != "/usr/lib" ; then
    $1="$$1 -L$2"
fi
])

dnl AC_ENABLE_SHARED - implement the --enable-shared flag
dnl Usage: AC_ENABLE_SHARED[(DEFAULT)]
dnl   Where DEFAULT is either `yes' or `no'.  If omitted, it defaults to
dnl   `yes'.
AC_DEFUN([AC_ENABLE_SHARED],
[define([AC_ENABLE_SHARED_DEFAULT], ifelse($1, no, no, yes))dnl
AC_ARG_ENABLE(shared,
changequote(<<, >>)dnl
<<  --enable-shared[=PKGS]    build shared libraries [default=>>AC_ENABLE_SHARED_DEFAULT],
changequote([, ])dnl
[p=${PACKAGE-default}
case $enableval in
yes) enable_shared=yes ;;
no) enable_shared=no ;;
*)
  enable_shared=no
  # Look at the argument we got.  We use all the common list separators.
  IFS="${IFS=   }"; ac_save_ifs="$IFS"; IFS="${IFS}:,"
  for pkg in $enableval; do
    if test "X$pkg" = "X$p"; then
      enable_shared=yes
    fi

  done
  IFS="$ac_save_ifs"
  ;;
esac],
enable_shared=AC_ENABLE_SHARED_DEFAULT)dnl
])

dnl AC_ENABLE_STATIC - implement the --enable-static flag
dnl Usage: AC_ENABLE_STATIC[(DEFAULT)]
dnl   Where DEFAULT is either `yes' or `no'.  If omitted, it defaults to
dnl   `yes'.
AC_DEFUN([AC_ENABLE_STATIC],
[define([AC_ENABLE_STATIC_DEFAULT], ifelse($1, no, no, yes))dnl
AC_ARG_ENABLE(static,
changequote(<<, >>)dnl
<<  --enable-static[=PKGS]    build static libraries [default=>>AC_ENABLE_STATIC_DEFAULT],
changequote([, ])dnl
[p=${PACKAGE-default}
case $enableval in
yes) enable_static=yes ;;
no) enable_static=no ;;
*)
  enable_static=no
  # Look at the argument we got.  We use all the common list separators.
  IFS="${IFS=   }"; ac_save_ifs="$IFS"; IFS="${IFS}:,"
  for pkg in $enableval; do
    if test "X$pkg" = "X$p"; then
      enable_static=yes
    fi
  done
  IFS="$ac_save_ifs"
  ;;
esac],
enable_static=AC_ENABLE_STATIC_DEFAULT)dnl
])

dnl AC_DISABLE_STATIC - set the default static flag to --disable-static
AC_DEFUN([AC_DISABLE_STATIC],
[AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
AC_ENABLE_STATIC(no)])

dnl AC_TRY_RUN_STRICT(PROGRAM,CFLAGS,CPPFLAGS,LDFLAGS,
dnl		[ACTION-IF-TRUE],[ACTION-IF-FALSE],
dnl		[ACTION-IF-CROSS-COMPILING = RUNTIME-ERROR])
AC_DEFUN( [AC_TRY_RUN_STRICT],
[
	old_CFLAGS="$CFLAGS";
	CFLAGS="$2";
	export CFLAGS;
	old_CPPFLAGS="$CPPFLAGS";
	CPPFLAGS="$3";
	export CPPFLAGS;
	old_LDFLAGS="$LDFLAGS";
	LDFLAGS="$4";
	export LDFLAGS;
	AC_TRY_RUN([$1],[$5],[$6],[$7])
	CFLAGS="$old_CFLAGS";
	old_CFLAGS="";
	export CFLAGS;
	CPPFLAGS="$old_CPPFLAGS";
	old_CPPFLAGS="";
	export CPPFLAGS;
	LDFLAGS="$old_LDFLAGS";
	old_LDFLAGS="";
	export LDFLAGS;
])

dnl SMB_CHECK_SYSCONF(varname)
dnl Tests whether the sysconf(3) variable "varname" is available.
AC_DEFUN([SMB_CHECK_SYSCONF],
[
    AC_CACHE_CHECK([for sysconf($1)],
	samba_cv_SYSCONF$1,
	[
	    AC_TRY_LINK([#include <unistd.h>],
		[ return sysconf($1) == -1 ? 1 : 0; ],
		[ samba_cv_SYSCONF$1=yes ],
		[ samba_cv_SYSCONF$1=no ])
	])

    if test x"$samba_cv_SYSCONF$1" = x"yes" ; then
	AC_DEFINE(SYSCONF$1, 1, [Whether sysconf($1) is available])
    fi
])

dnl SMB_IS_LIBPTHREAD_LINKED([actions if true], [actions if false])
dnl Test whether the current LIBS results in libpthread being present.
dnl Execute the corresponding user action list.
AC_DEFUN([SMB_IS_LIBPTHREAD_LINKED],
[
    AC_MSG_CHECKING(if libpthread is linked)
    AC_TRY_LINK([],
	[return pthread_create(0, 0, 0, 0);],
	[
	    AC_MSG_RESULT(yes)
	    $1
	],
	[
	    AC_MSG_RESULT(no)
	    $2
	])
])

dnl SMB_REMOVE_LIB(lib)
dnl Remove the given library from $LIBS
AC_DEFUN([SMB_REMOVE_LIB],
[
    LIBS=`echo $LIBS | sed '-es/-l$1//g'`
])

dnl SMB_CHECK_DMAPI([actions if true], [actions if false])
dnl Check whether DMAPI is available and is a version that we know
dnl how to deal with. The default truth action is to set samba_dmapi_libs
dnl to the list of necessary libraries, and to define USE_DMAPI.
AC_DEFUN([SMB_CHECK_DMAPI],
[
    samba_dmapi_libs=""

    if test x"$samba_dmapi_libs" = x"" ; then
	AC_CHECK_LIB(dm, dm_get_eventlist,
		[ samba_dmapi_libs="-ldm"], [])
    fi

    if test x"$samba_dmapi_libs" = x"" ; then
	AC_CHECK_LIB(jfsdm, dm_get_eventlist,
		[samba_dmapi_libs="-ljfsdm"], [])
    fi

    if test x"$samba_dmapi_libs" = x"" ; then
	AC_CHECK_LIB(xdsm, dm_get_eventlist,
		[samba_dmapi_libs="-lxdsm"], [])
    fi

    if test x"$samba_dmapi_libs" = x"" ; then
        AC_CHECK_LIB(dmapi, dm_get_eventlist,
                [samba_dmapi_libs="-ldmapi"], [])
    fi


    # Only bother to test ehaders if we have a candidate DMAPI library
    if test x"$samba_dmapi_libs" != x"" ; then
	AC_CHECK_HEADERS(sys/dmi.h xfs/dmapi.h sys/jfsdmapi.h sys/dmapi.h dmapi.h)
    fi

    if test x"$samba_dmapi_libs" != x"" ; then
	samba_dmapi_save_LIBS="$LIBS"
	LIBS="$LIBS $samba_dmapi_libs"
	AC_TRY_LINK(
		[
#include <time.h>      /* needed by Tru64 */
#include <sys/types.h> /* needed by AIX */
#ifdef HAVE_XFS_DMAPI_H
#include <xfs/dmapi.h>
#elif defined(HAVE_SYS_DMI_H)
#include <sys/dmi.h>
#elif defined(HAVE_SYS_JFSDMAPI_H)
#include <sys/jfsdmapi.h>
#elif defined(HAVE_SYS_DMAPI_H)
#include <sys/dmapi.h>
#elif defined(HAVE_DMAPI_H)
#include <dmapi.h>
#endif
		],
		[
/* This link test is designed to fail on IRI 6.4, but should
 * succeed on Linux, IRIX 6.5 and AIX.
 */
	char * version;
	dm_eventset_t events;
	/* This doesn't take an argument on IRIX 6.4. */
	dm_init_service(&version);
	/* IRIX 6.4 expects events to be a pointer. */
	DMEV_ISSET(DM_EVENT_READ, events);
		],
		[
		    true # DMAPI link test succeeded
		],
		[
		    # DMAPI link failure
		    samba_dmapi_libs=
		])
	LIBS="$samba_dmapi_save_LIBS"
    fi

    if test x"$samba_dmapi_libs" = x"" ; then
	# DMAPI detection failure actions begin
	ifelse($2, [],
	    [
		AC_ERROR(Failed to detect a supported DMAPI implementation)
	    ],
	    [
		$2
	    ])
	# DMAPI detection failure actions end
    else
	# DMAPI detection success actions start
	ifelse($1, [],
	    [
		AC_DEFINE(USE_DMAPI, 1,
		    [Whether we should build DMAPI integration components])
		AC_MSG_NOTICE(Found DMAPI support in $samba_dmapi_libs)
	    ],
	    [
		$1
	    ])
	# DMAPI detection success actions end
    fi

])

dnl SMB_CHECK_CLOCK_ID(clockid)
dnl Test whether the specified clock_gettime clock ID is available. If it
dnl is, we define HAVE_clockid
AC_DEFUN([SMB_CHECK_CLOCK_ID],
[
    AC_MSG_CHECKING(for $1)
    AC_TRY_LINK([
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
    ],
    [
clockid_t clk = $1;
    ],
    [
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_$1, 1,
	    [Whether the clock_gettime clock ID $1 is available])
    ],
    [
	AC_MSG_RESULT(no)
    ])
])

dnl SMB_IF_RTSIGNAL_BUG([actions if true],
dnl			[actions if false],
dnl			[actions if cross compiling])
dnl Test whether we can call sigaction with RT_SIGNAL_NOTIFY and
dnl RT_SIGNAL_LEASE (also RT_SIGNAL_AIO for good measure, though
dnl I don't believe that triggers any bug.
dnl
dnl See the samba-technical thread titled "Failed to setup
dnl RT_SIGNAL_NOTIFY handler" for details on the bug in question.
AC_DEFUN([SMB_IF_RTSIGNAL_BUG],
[
    rt_signal_notify_works=yes
    rt_signal_lease_works=yes
    rt_signal_aio_works=yes

    AC_MSG_CHECKING(if sigaction works with realtime signals)
    AC_TRY_RUN(
	[
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>

/* from smbd/notify_kernel.c */
#ifndef RT_SIGNAL_NOTIFY
#define RT_SIGNAL_NOTIFY (SIGRTMIN+2)
#endif

/* from smbd/aio.c */
#ifndef RT_SIGNAL_AIO
#define RT_SIGNAL_AIO (SIGRTMIN+3)
#endif

/* from smbd/oplock_linux.c */
#ifndef RT_SIGNAL_LEASE
#define RT_SIGNAL_LEASE (SIGRTMIN+1)
#endif

static void signal_handler(int sig, siginfo_t *info, void *unused)
{
    int do_nothing = 0;
}

int main(void)
{
    int result = 0;
    struct sigaction act = {0};

    act.sa_sigaction = signal_handler;
    act.sa_flags = SA_SIGINFO;
    sigemptyset( &act.sa_mask );

    if (sigaction(RT_SIGNAL_LEASE, &act, 0) != 0) {
	    /* Failed to setup RT_SIGNAL_LEASE handler */
	    result += 1;
    }

    if (sigaction(RT_SIGNAL_NOTIFY, &act, 0) != 0) {
	    /* Failed to setup RT_SIGNAL_NOTIFY handler */
	    result += 10;
    }

    if (sigaction(RT_SIGNAL_AIO, &act, 0) != 0) {
	    /* Failed to setup RT_SIGNAL_AIO handler */
	    result += 100;
    }

    /* zero on success */
    return result;
}
	],
	[
	    AC_MSG_RESULT(yes)
	    $2
	],
	[
	    AC_MSG_RESULT(no)
	    case "$ac_status" in
		1|11|101|111)  rt_signal_lease_ok=no ;;
	    esac
	    case "$ac_status" in
		10|11|110|111)  rt_signal_notify_ok=no ;;
	    esac
	    case "$ac_status" in
		100|110|101|111)  rt_signal_aio_ok=no ;;
	    esac
	    $2
	],
	[
	    AC_MSG_RESULT(cross)
	    $3
	])
])

m4_include(lib/replace/libreplace.m4)
