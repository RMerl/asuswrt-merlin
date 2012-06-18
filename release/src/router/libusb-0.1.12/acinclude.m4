dnl Autoconf macros used by libusb
dnl
dnl Copyright (C) 2003 Brad Hards <bradh@frogmouth.net
dnl Qt tests based on pinentry code Copyright (C) 2002 g10 Code GmbH
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2, or (at your option)
dnl any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
dnl

dnl LIBUSB_FIND_PATH(program-name, variable-name, list of directories, if-not-found, test-parameter)
AC_DEFUN([LIBUSB_FIND_PATH],
[
   AC_MSG_CHECKING([for $1])
   if test -n "$$2"; then
        libusb_cv_path="$$2";
   else
        libusb_cache=`echo $1 | sed 'y%./+-%__p_%'`

        AC_CACHE_VAL(libusb_cv_path_$libusb_cache,
        [
        libusb_cv_path="NONE"
        dirs="$3"
        libusb_save_IFS=$IFS
        IFS=':'
        for dir in $PATH; do
          dirs="$dirs $dir"
        done
        IFS=$libusb_save_IFS

        for dir in $dirs; do
          if test -x "$dir/$1"; then
            if test -n "$5"
            then
              evalstr="$dir/$1 $5 2>&1 "
              if eval $evalstr; then
                libusb_cv_path="$dir/$1"
                break
              fi
            else
                libusb_cv_path="$dir/$1"
                break
            fi
          fi
        done

        eval "libusb_cv_path_$libusb_cache=$libusb_cv_path"

        ])

      eval "libusb_cv_path=\"`echo '$libusb_cv_path_'$libusb_cache`\""

   fi

   if test -z "$libusb_cv_path" || test "$libusb_cv_path" = NONE; then
      AC_MSG_RESULT(not found)
      $4
   else
      AC_MSG_RESULT($libusb_cv_path)
      $2=$libusb_cv_path

   fi
])

AC_DEFUN([LIBUSB_INIT_DOXYGEN],
[
AC_MSG_CHECKING([for Doxygen tools])

LIBUSB_FIND_PATH(dot, DOT, [], [])
if test -n "$DOT"; then
  LIBUSB_HAVE_DOT="YES"
else
  LIBUSB_HAVE_DOT="NO"
fi
AC_SUBST(LIBUSB_HAVE_DOT)

LIBUSB_FIND_PATH(doxygen, DOXYGEN, [], [])
AC_SUBST(DOXYGEN)

DOXYGEN_PROJECT_NAME="$1"
DOXYGEN_PROJECT_NUMBER="$2"
AC_SUBST(DOXYGEN_PROJECT_NAME)
AC_SUBST(DOXYGEN_PROJECT_NUMBER)

LIBUSB_HAS_DOXYGEN=no
if test -n "$DOXYGEN" && test -x "$DOXYGEN"; then
  LIBUSB_HAS_DOXYGEN=yes
fi
AC_SUBST(LIBUSB_HAS_DOXYGEN)

])
