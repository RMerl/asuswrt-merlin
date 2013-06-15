dnl find the talloc sources. This is meant to work both for 
dnl talloc standalone builds, and builds of packages using talloc
tallocdir=""
tallocpaths="$srcdir $srcdir/lib/talloc $srcdir/talloc $srcdir/../talloc"
for d in $tallocpaths; do
	if test -f "$d/talloc.c"; then
		tallocdir="$d"		
		AC_SUBST(tallocdir)
		break;
	fi
done
if test x"$tallocdir" = "x"; then
   AC_MSG_ERROR([cannot find talloc source in $tallocpaths])
fi
TALLOCOBJ="talloc.o"
AC_SUBST(TALLOCOBJ)

AC_CHECK_SIZEOF(size_t,cross)
AC_CHECK_SIZEOF(void *,cross)

if test $ac_cv_sizeof_size_t -lt $ac_cv_sizeof_void_p; then
	AC_WARN([size_t cannot represent the amount of used memory of a process])
	AC_WARN([please report this to <samba-technical@samba.org>])
	AC_WARN([sizeof(size_t) = $ac_cv_sizeof_size_t])
	AC_WARN([sizeof(void *) = $ac_cv_sizeof_void_p])
	AC_ERROR([sizeof(size_t) < sizeof(void *)])
fi
