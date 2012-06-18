dnl
dnl GP_CONFIG_INIT
dnl      use default LHS width (called implicitly if not called explicitly)
dnl GP_CONFIG_INIT([WIDTH-OF-LHS])
dnl      explicitly set the LHS width to the given value
dnl
dnl GP_CONFIG_MSG
dnl      empty output line
dnl GP_CONFIG_MSG([LHS],[RHS])
dnl      formatted output line "LHS: RHS"
dnl
dnl GP_CONFIG_OUTPUT
dnl      print all the output messages we collected in the mean time
dnl
dnl Simple way to print a configuration summary at the end of ./configure.
dnl
dnl Example usage:
dnl
dnl    GP_CONFIG_INIT
dnl    GP_CONFIG_MSG([Source code location],[${srcdir}])
dnl    GP_CONFIG_MSG([Compiler],[${CC}])
dnl    GP_CONFIG_MSG
dnl    GP_CONFIG_MSG([Feature foo],[${foo}])
dnl    GP_CONFIG_MSG([Location of bar],[${bar}])
dnl    [...]
dnl    AC_OUTPUT
dnl    GP_CONFIG_OUTPUT
dnl
dnl
AC_DEFUN([GP_CONFIG_INIT],
[dnl
AC_REQUIRE([GP_CHECK_SHELL_ENVIRONMENT])
dnl the empty string must contain at least as many spaces as the substr length
dnl FIXME: let m4 determine that length
dnl        (collect left parts in array and choose largest length)
m4_if([$1],[],[gp_config_len="30"],[gp_config_len="$1"])
gp_config_empty=""
gp_config_len3="$(expr "$gp_config_len" - 3)"
n="$gp_config_len"
while test "$n" -gt 0; do
      gp_config_empty="${gp_config_empty} "
      n="$(expr "$n" - 1)"
done
gp_config_msg="
Configuration (${PACKAGE_TARNAME} ${PACKAGE_VERSION}):
"
])dnl
dnl
dnl
AC_DEFUN([GP_CONFIG_MSG],
[AC_REQUIRE([GP_CONFIG_INIT])dnl
m4_if([$1],[],[
gp_config_msg="${gp_config_msg}
"
],[$2],[],[
gp_config_msg="${gp_config_msg}
  [$1]
"
],[
gp_config_msg_len="$(expr "[$1]" : '.*')"
if test "$gp_config_msg_len" -ge "$gp_config_len"; then
	gp_config_msg_lhs="$(expr "[$1]" : "\(.\{0,${gp_config_len3}\}\)")..:"
else
	gp_config_msg_lhs="$(expr "[$1]:${gp_config_empty}" : "\(.\{0,${gp_config_len}\}\)")"
fi
gp_config_msg="${gp_config_msg}    ${gp_config_msg_lhs} [$2]
"
])])dnl
dnl
AC_DEFUN([GP_CONFIG_MSG_SUBDIRS],[dnl
# Message about configured subprojects
if test "x$subdirs" != "x"; then
	GP_CONFIG_MSG()dnl
	_subdirs=""
	for sd in $subdirs; do
		ssd="$(basename "$sd")"
		if test "x$_subdirs" = "x"; then
			_subdirs="$ssd";
		else
			_subdirs="$_subdirs $ssd"
		fi
	done
	GP_CONFIG_MSG([Subprojects],[${_subdirs}])dnl
fi
])dnl
dnl
AC_DEFUN([GP_CONFIG_OUTPUT],
[AC_REQUIRE([GP_CONFIG_INIT])dnl
AC_REQUIRE([GP_CONFIG_MSG])dnl
AC_REQUIRE([GP_CONFIG_MSG_SUBDIRS])dnl
echo "${gp_config_msg}
You may run \"make\" and \"make install\" now."
])dnl
dnl
dnl Please do not remove this:
dnl filetype: de774af3-dc3b-4b1d-b6f2-4aca35d3da16
dnl I use this to find all the different instances of this file which 
dnl are supposed to be synchronized.
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
