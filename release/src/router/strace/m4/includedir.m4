dnl ### A macro to find the include directory, useful for cross-compiling.
AC_DEFUN([AC_INCLUDEDIR],
[AC_REQUIRE([AC_PROG_AWK])dnl
AC_SUBST(includedir)
AC_MSG_CHECKING(for primary include directory)
includedir=/usr/include
if test -n "$GCC"
then
	>conftest.c
	new_includedir=`
		$CC -v -E conftest.c 2>&1 | $AWK '
			/^End of search list/ { print last; exit }
			{ last = [$]1 }
		'
	`
	rm -f conftest.c
	if test -n "$new_includedir" && test -d "$new_includedir"
	then
		includedir=$new_includedir
	fi
fi
AC_MSG_RESULT($includedir)
])
