#include <stdio.h>
#include <sys/sysinfo.h>
#include <time.h>

#define USE_UPTIME 1

// When system time change, the lease table will error, so we use system uptime
time_t	// long int
get_time(time_t *t){
#ifdef USE_UPTIME
        struct sysinfo info;

        sysinfo(&info);

        return info.uptime;
#else
        return time(0);
#endif
}

