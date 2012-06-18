dnl @synopsis GP_CHECK_SHELL_ENVIRONMENT([SHOW-LOCALE-VARS])
dnl 
dnl Check that the shell environment is sane.
dnl
dnl If SHOW-LOCALE-VARS is set to [true], print all LC_* and LANG*
dnl variables at configure time. (WARNING: This is not portable!)
dnl
dnl
AC_DEFUN([GP_CHECK_SHELL_ENVIRONMENT],
[
# make sure "cd" doesn't print anything on stdout
if test x"${CDPATH+set}" = xset
then
	CDPATH=:
	export CDPATH
fi

# make sure $() command substitution works
AC_MSG_CHECKING([for POSIX sh \$() command substitution])
if test "x$(pwd)" = "x`pwd`" && test "y$(echo "foobar")" = "y`echo foobar`" # ''''
then
	AC_MSG_RESULT([yes])
else
	AC_MSG_RESULT([no])
	uname=`uname 2>&1` # ''
	uname_a=`uname -a 2>&1` # ''
	AC_MSG_ERROR([

* POSIX sh \$() command substition does not work with this shell.
*
* You are running a very rare species of shell. Please report this
* sighting to <${PACKAGE_BUGREPORT}>:
* SHELL=${SHELL}
* uname=${uname}
* uname-a=${uname_a}
* Please also include your OS and version.
*
* Run this configure script using a better (i.e. POSIX compliant) shell.
])
fi
dnl
m4_if([$1],[true],[dnl
printenv | grep -E '^(LC_|LANG)'
])dnl

dnl
])dnl
dnl
