#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#if !defined(HAVE_USLEEP)

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif /* HAVE_SYS_TIME_H */
#endif /* TIME_WITH_SYS_TIME */

#ifdef HAVE_UNISTD_H
# include <sys/types.h>
# include <unistd.h>
#else
# warning "Don't know what to do, where am I?"
#endif /* HAVE_UNISTD_H */

#include "usleep.h"

void
usleep (unsigned long usec)
{
  struct timeval timeout; 

  timeout.tv_sec  = 0;
  timeout.tv_usec = usec;

  select (0, NULL, NULL, NULL, &timeout);
}

#endif /* HAVE_USLEEP */
