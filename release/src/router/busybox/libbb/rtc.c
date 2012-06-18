/*
 * Common RTC functions
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "rtc_.h"

#if ENABLE_FEATURE_HWCLOCK_ADJTIME_FHS
# define ADJTIME_PATH "/var/lib/hwclock/adjtime"
#else
# define ADJTIME_PATH "/etc/adjtime"
#endif

int FAST_FUNC rtc_adjtime_is_utc(void)
{
	int utc = 0;
	FILE *f = fopen_for_read(ADJTIME_PATH);

	if (f) {
		char buffer[128];

		while (fgets(buffer, sizeof(buffer), f)) {
			int len = strlen(buffer);

			while (len && isspace(buffer[len - 1]))
				len--;

			buffer[len] = 0;

			if (strncmp(buffer, "UTC", 3) == 0) {
				utc = 1;
				break;
			}
		}
		fclose(f);
	}

	return utc;
}

int FAST_FUNC rtc_xopen(const char **default_rtc, int flags)
{
	int rtc;

	if (!*default_rtc) {
		*default_rtc = "/dev/rtc";
		rtc = open(*default_rtc, flags);
		if (rtc >= 0)
			return rtc;
		*default_rtc = "/dev/rtc0";
		rtc = open(*default_rtc, flags);
		if (rtc >= 0)
			return rtc;
		*default_rtc = "/dev/misc/rtc";
	}

	return xopen(*default_rtc, flags);
}

void FAST_FUNC rtc_read_tm(struct tm *ptm, int fd)
{
	memset(ptm, 0, sizeof(*ptm));
	xioctl(fd, RTC_RD_TIME, ptm);
	ptm->tm_isdst = -1; /* "not known" */
}

time_t FAST_FUNC rtc_tm2time(struct tm *ptm, int utc)
{
	char *oldtz = oldtz; /* for compiler */
	time_t t;

	if (utc) {
		oldtz = getenv("TZ");
		putenv((char*)"TZ=UTC0");
		tzset();
	}

	t = mktime(ptm);

	if (utc) {
		unsetenv("TZ");
		if (oldtz)
			putenv(oldtz - 3);
		tzset();
	}

	return t;
}
