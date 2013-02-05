dnl *********** GNU libc 2 ***************
AC_DEFUN([AC_GNULIBC],[
  AC_MSG_CHECKING(for GNU libc2)
  AC_CACHE_VAL(knfsd_cv_glibc2,
  [AC_TRY_CPP([
      #include <features.h>
      #if !defined(__GLIBC__)
      # error Nope
      #endif
      ],
  knfsd_cv_glibc2=yes, knfsd_cv_glibc2=no)])
  AC_MSG_RESULT($knfsd_cv_glibc2)
  if test $knfsd_cv_glibc2 = yes; then
    CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE"
    CPPFLAGS_FOR_BUILD="$CPPFLAGS_FOR_BUILD -D_GNU_SOURCE"
  fi
])
