#serial 5

dnl From Jim Meyering.
dnl Determine whether malloc accepts 0 as its argument.
dnl If it doesn't, arrange to use the replacement function.
dnl

AC_DEFUN([jm_FUNC_MALLOC],
[
 dnl xmalloc.c requires that this symbol be defined so it doesn't
 dnl mistakenly use a broken malloc -- as it might if this test were omitted.
 AC_DEFINE_UNQUOTED(HAVE_DONE_WORKING_MALLOC_CHECK, 1,
                    [Define if the malloc check has been performed. ])

 AC_CACHE_CHECK([for working malloc], jm_cv_func_working_malloc,
  [AC_TRY_RUN([
    char *malloc ();
    int
    main ()
    {
      exit (malloc (0) ? 0 : 1);
    }
	  ],
	 jm_cv_func_working_malloc=yes,
	 jm_cv_func_working_malloc=no,
	 dnl When crosscompiling, assume malloc is broken.
	 jm_cv_func_working_malloc=no)
  ])
  if test $jm_cv_func_working_malloc = no; then
    AC_LIBOBJ(malloc)
    AC_DEFINE_UNQUOTED(malloc, rpl_malloc,
      [Define to rpl_malloc if the replacement function should be used.])
  fi
])
