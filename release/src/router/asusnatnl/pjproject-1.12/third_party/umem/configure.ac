AC_INIT([umem], [1.0.1], [], [umem])
AM_INIT_AUTOMAKE([dist-bzip2])

AC_PROG_CC
AM_PROG_AS
AC_PROG_LIBTOOL

AC_C_INLINE

AC_MSG_CHECKING([whether pthread_mutex_t is larger than 24 bytes])
AC_TRY_RUN( 
  [   
#include <pthread.h>
int main(void){return (sizeof(pthread_mutex_t) > 24);}
  ],
  [AC_MSG_RESULT(yes)],
  [
    AC_MSG_RESULT(no)
    AC_DEFINE(UMEM_PTHREAD_MUTEX_TOO_BIG, [1], [need bigger cache])
    AC_MSG_WARN([*** increasing umem cpu cache size to compensate.])
  ]
) 


AC_CHECK_HEADERS([sys/mman.h sys/sysmacros.h sys/time.h malloc.h])
AC_CHECK_FUNCS([issetugid mallinfo malloc_stats])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile Doxyfile umem.spec])

AC_OUTPUT
