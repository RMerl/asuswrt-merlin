dnl SMB Build Environment Perl Checks
dnl -------------------------------------------------------
dnl  Copyright (C) Stefan (metze) Metzmacher 2004
dnl  Released under the GNU GPL
dnl -------------------------------------------------------
dnl

AC_DEFUN([AC_SAMBA_PERL],
[
case "$host_os" in
	*irix*)
		# On IRIX, we prefer Freeware or Nekoware Perl, because the
		# system perl is so ancient.
		AC_PATH_PROG(PERL, perl, "", "/usr/freeware/bin:/usr/nekoware/bin:$PATH")
		;;
	*)
		AC_PATH_PROG(PERL, perl)
		;;
esac

if test x"$PERL" = x""; then
	AC_MSG_WARN([No version of perl was found!])
	$2
else
	if test x"$debug" = x"yes";then
		PERL="$PERL -W"
	fi
	export PERL
	$1
fi
])

