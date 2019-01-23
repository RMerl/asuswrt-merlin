dnl Autoconf macros for finding a Python development environment
dnl
dnl Copyright (C) 2007-2008 Jelmer Vernooij <jelmer@samba.org>
dnl Published under the GNU GPL, v3 or later
dnl
AC_ARG_VAR([PYTHON_VER],[The installed Python
	version to use, for example '2.3'. This string 
	will be appended to the Python interpreter
	canonical name.])

AC_DEFUN([TRY_LINK_PYTHON],
[
	if test $working_python = no; then
		ac_save_LIBS="$LIBS"
		ac_save_CFLAGS="$CFLAGS"
		LIBS="$LIBS $1"
		CFLAGS="$CFLAGS $2"

		AC_TRY_LINK([
                                #undef HAVE_UINTPTR_T
				/* we have our own configure tests */
				#include <Python.h>
			],[
				Py_InitModule(NULL, NULL);
			],[
				PYTHON_LDFLAGS="$1"
				PYTHON_CFLAGS="$2"
				working_python=yes
			])
		LIBS="$ac_save_LIBS"
		CFLAGS="$ac_save_CFLAGS"
	fi
])

dnl Try to find a Python implementation including header files
dnl AC_SAMBA_PYTHON_DEVEL(RUN-IF-FOUND, RUN-IF-NOT-FOUND)
dnl
dnl Will set the following variables:
dnl $PYTHON
dnl $PYTHON_CONFIG (if found)
dnl $PYTHON_CFLAGS
dnl $PYTHON_LDFLAGS
AC_DEFUN([AC_SAMBA_PYTHON_DEVEL],
[
	if test -z "$PYTHON_VER"; then
		AC_PATH_PROGS([PYTHON], [python2.6 python2.5 python2.4 python])
	else
		AC_PATH_PROG([PYTHON],[python[$PYTHON_VER]])
	fi
	if test -z "$PYTHON"; then
		working_python=no
		AC_MSG_WARN([No python found])
	fi

	dnl assume no working python
	working_python=no

	if test -z "$PYTHON_VER"; then 
		AC_PATH_PROGS([PYTHON_CONFIG], [python2.6-config python2.5-config python2.4-config python-config])
	else 
		AC_PATH_PROG([PYTHON_CONFIG], [python[$PYTHON_VER]-config])
	fi

	if test -z "$PYTHON_CONFIG"; then
		AC_MSG_WARN([No python-config found])
	else
		TRY_LINK_PYTHON([`$PYTHON_CONFIG --ldflags`], [`$PYTHON_CONFIG --includes`])
		TRY_LINK_PYTHON([`$PYTHON_CONFIG --ldflags`], [`$PYTHON_CONFIG --cflags`])
		if test x$working_python = xno; then
			# It seems the library path isn't included on some systems
			base=`$PYTHON_CONFIG --prefix`
			TRY_LINK_PYTHON([`echo -n -L${base}/lib " "; $PYTHON_CONFIG --ldflags`], [`$PYTHON_CONFIG --includes`])
			TRY_LINK_PYTHON([`echo -n -L${base}/lib " "; $PYTHON_CONFIG --ldflags`], [`$PYTHON_CONFIG --cflags`])
		fi
	fi

	if test x$PYTHON != x
	then
		DISTUTILS_CFLAGS=`$PYTHON -c "from distutils import sysconfig; \
					      print '-I%s -I%s %s' % ( \
							sysconfig.get_python_inc(), \
							sysconfig.get_python_inc(plat_specific=1), \
							sysconfig.get_config_var('CFLAGS'))"`
		DISTUTILS_LDFLAGS=`$PYTHON -c "from distutils import sysconfig; \
					       print '%s %s -lpython%s -L%s %s -L%s' % ( \
							sysconfig.get_config_var('LIBS'), \
							sysconfig.get_config_var('SYSLIBS'), \
							sysconfig.get_config_var('VERSION'), \
							sysconfig.get_config_var('LIBDIR'), \
							sysconfig.get_config_var('LDFLAGS'), \
							sysconfig.get_config_var('LIBPL'))"`
		TRY_LINK_PYTHON($DISTUTILS_LDFLAGS, $DISTUTILS_CFLAGS)

		if `$PYTHON -c "import sys; sys.exit(sys.version_info.__getslice__(0, 2) >= (2, 4))"`
		then
			AC_MSG_WARN([Python ($PYTHON) is too old. At least version 2.4 is required])
			working_python=no
		fi
	fi

	AC_MSG_CHECKING(working python module support)
	if test $working_python = yes; then
		AC_MSG_RESULT([yes])
		$1
	else
		AC_MSG_RESULT([no])
		$2
	fi
])


