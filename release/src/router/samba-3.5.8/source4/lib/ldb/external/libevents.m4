AC_SUBST(TEVENT_OBJ)
AC_SUBST(TEVENT_CFLAGS)
AC_SUBST(TEVENT_LIBS)

AC_CHECK_HEADER(tevent.h,
   [AC_CHECK_LIB(tevent, tevent_context_init, [TEVENT_LIBS="-ltevent"], , -ltalloc) ],
   [PKG_CHECK_MODULES(TEVENT, tevent)])
