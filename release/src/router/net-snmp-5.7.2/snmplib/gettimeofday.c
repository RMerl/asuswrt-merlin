/*
 * gettimeofday() replacement for MSVC.
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/types.h>

#ifdef HAVE_SYS_TIMEB_H
# include <sys/timeb.h> /* _ftime() */
#endif
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <net-snmp/library/system.h>

int
gettimeofday(struct timeval *tv, struct timezone *tz)
{
    struct _timeb   timebuffer;

    _ftime(&timebuffer);
    tv->tv_usec = timebuffer.millitm * 1000;
    tv->tv_sec = (long)timebuffer.time;
    return (0);
}
