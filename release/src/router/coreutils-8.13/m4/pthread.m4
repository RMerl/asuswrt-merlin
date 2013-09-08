# pthread.m4 serial 3
dnl Copyright (C) 2009-2011 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_PTHREAD_CHECK],
[
   AC_REQUIRE([gl_PTHREAD_DEFAULTS])
   gl_CHECK_NEXT_HEADERS([pthread.h])
   if test $ac_cv_header_pthread_h = yes; then
     HAVE_PTHREAD_H=1
   else
     HAVE_PTHREAD_H=0
   fi

   AC_CHECK_TYPES([pthread_t, pthread_spinlock_t], [], [],
     [AC_INCLUDES_DEFAULT[
      #if HAVE_PTHREAD_H
       #include <pthread.h>
      #endif]])
   if test $ac_cv_type_pthread_t != yes; then
     HAVE_PTHREAD_T=0
   fi
   if test $ac_cv_type_pthread_spinlock_t != yes; then
     HAVE_PTHREAD_SPINLOCK_T=0
   fi

   if test $ac_cv_header_pthread_h != yes ||
      test $ac_cv_type_pthread_t != yes ||
      test $ac_cv_type_pthread_spinlock_t != yes; then
     PTHREAD_H='pthread.h'
   else
     PTHREAD_H=
   fi
   AC_SUBST([PTHREAD_H])
   AM_CONDITIONAL([GL_GENERATE_PTHREAD_H], [test -n "$PTHREAD_H"])

   LIB_PTHREAD=
   if test $ac_cv_header_pthread_h = yes; then
     dnl We cannot use AC_SEARCH_LIBS here, because on OSF/1 5.1 pthread_join
     dnl is defined as a macro which expands to __phread_join, and libpthread
     dnl contains a definition for __phread_join but none for pthread_join.
     AC_CACHE_CHECK([for library containing pthread_join],
       [gl_cv_search_pthread_join],
       [gl_saved_libs="$LIBS"
        gl_cv_search_pthread_join=
        AC_LINK_IFELSE(
          [AC_LANG_PROGRAM(
             [[#include <pthread.h>]],
             [[pthread_join (pthread_self (), (void **) 0);]])],
          [gl_cv_search_pthread_join="none required"])
        if test -z "$gl_cv_search_pthread_join"; then
          LIBS="-lpthread $gl_saved_libs"
          AC_LINK_IFELSE(
            [AC_LANG_PROGRAM(
               [[#include <pthread.h>]],
               [[pthread_join (pthread_self (), (void **) 0);]])],
            [gl_cv_search_pthread_join="-lpthread"])
        fi
        LIBS="$gl_saved_libs"
       ])
     if test "$gl_cv_search_pthread_join" != "none required"; then
       LIB_PTHREAD="$gl_cv_search_pthread_join"
     fi
   fi
   AC_SUBST([LIB_PTHREAD])

   AC_REQUIRE([AC_C_INLINE])
   AC_REQUIRE([AC_C_RESTRICT])
])

AC_DEFUN([gl_PTHREAD_DEFAULTS],
[
  dnl Assume proper GNU behavior unless another module says otherwise.
  HAVE_PTHREAD_H=1;              AC_SUBST([HAVE_PTHREAD_H])
  HAVE_PTHREAD_T=1;              AC_SUBST([HAVE_PTHREAD_T])
  HAVE_PTHREAD_SPINLOCK_T=1;     AC_SUBST([HAVE_PTHREAD_SPINLOCK_T])
])
