#ifndef UTIL_LINUX_USLEEP_H
#define UTIL_LINUX_USLEEP_H

#ifndef HAVE_USLEEP
/*
 * This function is marked obsolete in POSIX.1-2001 and removed in
 * POSIX.1-2008. It is replaced with nanosleep().
 */
# define usleep(x) \
	do { \
		struct timespec xsleep; \
		xsleep.tv_sec = x / 1000 / 1000; \
		xsleep.tv_nsec = (x - xsleep.tv_sec * 1000 * 1000) * 1000; \
		nanosleep(&xsleep, NULL); \
	} while (0)
#endif

#endif /* UTIL_LINUX_USLEEP_H */
