dnl (C) 2003-2004 Jelmer Vernooij <jelmer@samba.org>
dnl Published under the GNU GPL
dnl
dnl DOCS_DEFINE_TARGET
dnl arg1: Target that is defined
dnl arg2: Requirement
dnl arg3: Official name
dnl arg4: Makefile target name

AC_DEFUN(DOCS_DEFINE_TARGET, [
	if test "x$$1_REQUIRES" = x; then
		$1_REQUIRES="$$2_REQUIRES"
	else
		$1_REQUIRES="$$1_REQUIRES $$2_REQUIRES"
	fi

	if test x"$$1_REQUIRES" = x; then
		TARGETS="$TARGETS $4"
	else
		AC_MSG_RESULT([Building the $3 requires : $$1_REQUIRES])
	fi
])

dnl DOCS_TARGET_REQUIRE_PROGRAM
dnl arg1: program variable
dnl arg2: program executable name
dnl arg3: target that requires it

AC_DEFUN(DOCS_TARGET_REQUIRE_PROGRAM, [
	AC_CHECK_PROGS([$1], [$2])
	if test x"$$1" = x; then
		if test x"$$3_REQUIRES" = x; then
			$3_REQUIRES="$2"
		else
			$3_REQUIRES="$$3_REQUIRES $2"
		fi
	fi
])
