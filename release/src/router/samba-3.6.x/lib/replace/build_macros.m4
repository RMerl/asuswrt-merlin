AC_DEFUN(BUILD_WITH_SHARED_BUILD_DIR,
  [ AC_ARG_WITH([shared-build-dir],
                [AC_HELP_STRING([--with-shared-build-dir=DIR],
                                [temporary build directory where libraries are installed [$srcdir/sharedbuild]])])

    sharedbuilddir="$srcdir/sharedbuild"
    if test x"$with_shared_build_dir" != x; then
        sharedbuilddir=$with_shared_build_dir
        CFLAGS="$CFLAGS -I$with_shared_build_dir/include"
        LDFLAGS="$LDFLAGS -L$with_shared_build_dir/lib"
    fi
    AC_SUBST(sharedbuilddir)
  ])

