dnl Checks for libevent
AC_DEFUN([AC_LIBEVENT], [

  dnl Check for libevent, but do not add -levent to LIBS
  AC_CHECK_LIB([event], [event_dispatch], [libevent=1],
               [AC_MSG_ERROR([libevent not found.])])

  AC_CHECK_HEADERS([event.h], ,
                   [AC_MSG_ERROR([libevent headers not found.])])

])dnl
