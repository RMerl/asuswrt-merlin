# Configure paths for netatalk
# Based on libsigc++ script by Karl Nelson
# Modified by jeff b (jeff@univrel.pr.uconn.edu)

dnl Test for netatalk, and define NETATALK_CFLAGS and NETATALK_LIBS
dnl   to be used as follows:
dnl AM_PATH_NETATALK(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl
AC_DEFUN([AM_PATH_NETATALK],
[dnl 
dnl Get the cflags and libraries from the netatalk-config script
dnl

dnl
dnl Prefix options
dnl
AC_ARG_WITH(netatalk-prefix,
[  --with-netatalk-prefix=PREFIX
                          Prefix where netatalk is installed (optional)]
, netatalk_config_prefix="$withval", netatalk_config_prefix="")

AC_ARG_WITH(netatalk-exec-prefix,
[  --with-netatalk-exec-prefix=PREFIX 
                          Exec prefix where netatalk is installed (optional)]
, netatalk_config_exec_prefix="$withval", netatalk_config_exec_prefix="")

AC_ARG_ENABLE(netatalktest, 
[  --disable-netatalktest     Do not try to compile and run a test netatalk 
                          program],
, enable_netatalktest=yes)

dnl
dnl Prefix handling
dnl
  if test x$netatalk_config_exec_prefix != x ; then
     netatalk_config_args="$netatalk_config_args --exec-prefix=$netatalk_config_exec_prefix"
     if test x${NETATALK_CONFIG+set} != xset ; then
        NETATALK_CONFIG=$netatalk_config_exec_prefix/bin/netatalk-config
     fi
  fi
  if test x$netatalk_config_prefix != x ; then
     netatalk_config_args="$netatalk_config_args --prefix=$netatalk_config_prefix"
     if test x${NETATALK_CONFIG+set} != xset ; then
        NETATALK_CONFIG=$netatalk_config_prefix/bin/netatalk-config
     fi
  fi

dnl
dnl See if netatalk-config is alive
dnl
  AC_PATH_PROG(NETATALK_CONFIG, netatalk-config, no)
  netatalk_version_min=$1

dnl
dnl  Version check
dnl
  AC_MSG_CHECKING(for netatalk - version >= $netatalk_version_min)
  no_netatalk=""
  if test "$NETATALK_CONFIG" = "no" ; then
    no_netatalk=yes
  else
    netatalk_version=`$NETATALK_CONFIG --version`

    NETATALK_CFLAGS=`$NETATALK_CONFIG $netatalk_config_args --cflags`
    NETATALK_LIBS=`$NETATALK_CONFIG $netatalk_config_args --libs`

    netatalk_major_version=`echo $netatalk_version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    netatalk_minor_version=`echo $netatalk_version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    netatalk_micro_version=`echo $netatalk_version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`

    netatalk_major_min=`echo $netatalk_major_version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    netatalk_minor_min=`echo $netatalk_minor_version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    netatalk_micro_min=`echo $netatalk_micro_version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`

    netatalk_version_proper=`expr \
        $netatalk_major_version \> $netatalk_major_min \| \
        $netatalk_major_version \= $netatalk_major_min \& \
        $netatalk_minor_version \> $netatalk_minor_min \| \
        $netatalk_major_version \= $netatalk_major_min \& \
        $netatalk_minor_version \= $netatalk_minor_min \& \
        $netatalk_micro_version \>= $netatalk_micro_min `

    if test "$netatalk_version_proper" = "1" ; then
      AC_MSG_RESULT([$netatalk_major_version.$netatalk_minor_version.$netatalk_micro_version])
    else
      AC_MSG_RESULT(no)
      no_netatalk=yes
    fi

    if test "X$no_netatalk" = "Xyes" ; then
      enable_netatalktest=no
    fi

    AC_LANG_SAVE
    AC_LANG_CPLUSPLUS

dnl
dnl
dnl
    if test "x$enable_netatalktest" = "xyes" ; then
      AC_MSG_CHECKING(if netatalk sane)
      ac_save_CXXFLAGS="$CXXFLAGS"
      ac_save_LIBS="$LIBS"
      CXXFLAGS="$CXXFLAGS $NETATALK_CFLAGS"
      LIBS="$LIBS $NETATALK_LIBS"

      rm -f conf.netatalktest

      AC_TRY_RUN([
#include <stdio.h>
#include <libatalk/version.h>

int main(int argc,char **argv)
  {
   if (netatalk_major_version!=$netatalk_major_version ||
       netatalk_minor_version!=$netatalk_minor_version ||
       netatalk_micro_version!=$netatalk_micro_version)
     { printf("(%d.%d.%d) ",
         netatalk_major_version,netatalk_minor_version,netatalk_micro_version);
       return 1;
     }
  }

],[
  AC_MSG_RESULT(yes)
],[
  AC_MSG_RESULT(no)
  no_netatalk=yes
]
,[echo $ac_n "cross compiling; assumed OK... $ac_c"])

       CXXFLAGS="$ac_save_CXXFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi

  dnl
  dnl
  if test "x$no_netatalk" = x ; then
     ifelse([$2], , :, [$2])     
  else
     NETATALK_CFLAGS=""
     NETATALK_LIBS=""
     ifelse([$3], , :, [$3])
  fi

  AC_LANG_RESTORE

  AC_SUBST(NETATALK_CFLAGS)
  AC_SUBST(NETATALK_LIBS)
])

