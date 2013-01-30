/* vi: set sw=4 ts=4: */
/*
 * Mini hwclock implementation for busybox
 *
 * Copyright (C) 2002 Robert Griebl <griebl@gmx.de>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
*/

#include "libbb.h"
/* After libbb.h, since it needs sys/types.h on some systems */
#include <sys/utsname.h>
#include "rtc_.h"

/* diff code is disabled: it's not sys/hw clock diff, it's some useless
 * "time between hwclock was started and we saw CMOS tick" quantity.
 * It's useless since hwclock is started at a random moment,
 * thus the quantity is also random, useless. Showing 0.000000 does not
 * deprive us from any useful info.
 *
 * SHOW_HWCLOCK_DIFF code in this file shows the difference between system
 * and hw clock. It is useful, but not compatible with standard hwclock.
 * Thus disabled.
 */
#define SHOW_HWCLOCK_DIFF 0


#if !SHOW_HWCLOCK_DIFF
# define read_rtc(pp_rtcname, sys_tv, utc) read_rtc(pp_rtcname, utc)
#endif
static time_t read_rtc(const char **pp_rtcname, struct timeval *sys_tv, int utc)
{
	struct tm tm_time;
	int fd;

	fd = rtc_xopen(pp_rtcname, O_RDONLY);

	rtc_read_tm(&tm_time, fd);

#if SHOW_HWCLOCK_DIFF
	{
		int before = tm_time.tm_sec;
		while (1) {
			rtc_read_tm(&tm_time, fd);
			gettimeofday(sys_tv, NULL);
			if (before != tm_time.tm_sec)
				break;
		}
	}
#endif

	if (ENABLE_FEATURE_CLEAN_UP)
		close(fd);

	return rtc_tm2time(&tm_time, utc);
}

static void show_clock(const char **pp_rtcname, int utc)
{
#if SHOW_HWCLOCK_DIFF
	struct timeval sys_tv;
#endif
	time_t t;
	char *cp;

	t = read_rtc(pp_rtcname, &sys_tv, utc);
	cp = ctime(&t);
	strchrnul(cp, '\n')[0] = '\0';
#if !SHOW_HWCLOCK_DIFF
	printf("%s  0.000000 seconds\n", cp);
#else
	{
		long diff = sys_tv.tv_sec - t;
		if (diff < 0 /*&& tv.tv_usec != 0*/) {
			/* Why? */
			/* diff >= 0 is ok:   diff < 0, can't just use tv.tv_usec: */
			/*   45.520820          43.520820 */
			/* - 44.000000        - 45.000000 */
			/* =  1.520820        = -1.479180, not -2.520820! */
			diff++;
			/* should be 1000000 - tv.tv_usec, but then we must check tv.tv_usec != 0 */
			sys_tv.tv_usec = 999999 - sys_tv.tv_usec;
		}
		printf("%s  %ld.%06lu seconds\n", cp, diff, (unsigned long)sys_tv.tv_usec);
	}
#endif
}

static void to_sys_clock(const char **pp_rtcname, int utc)
{
	struct timeval tv;
	struct timezone tz;

	tz.tz_minuteswest = timezone/60 - 60*daylight;
	tz.tz_dsttime = 0;

	tv.tv_sec = read_rtc(pp_rtcname, NULL, utc);
	tv.tv_usec = 0;
	if (settimeofday(&tv, &tz))
		bb_perror_msg_and_die("settimeofday");
}

static void from_sys_clock(const char **pp_rtcname, int utc)
{
#if 1
	struct timeval tv;
	struct tm tm_time;
	int rtc;

	rtc = rtc_xopen(pp_rtcname, O_WRONLY);
	gettimeofday(&tv, NULL);
	/* Prepare tm_time */
	if (sizeof(time_t) == sizeof(tv.tv_sec)) {
		if (utc)
			gmtime_r((time_t*)&tv.tv_sec, &tm_time);
		else
			localtime_r((time_t*)&tv.tv_sec, &tm_time);
	} else {
		time_t t = tv.tv_sec;
		if (utc)
			gmtime_r(&t, &tm_time);
		else
			localtime_r(&t, &tm_time);
	}
#else
/* Bloated code which tries to set hw clock with better precision.
 * On x86, even though code does set hw clock within <1ms of exact
 * whole seconds, apparently hw clock (at least on some machines)
 * doesn't reset internal fractional seconds to 0,
 * making all this a pointless excercise.
 */
	/* If we see that we are N usec away from whole second,
	 * we'll sleep for N-ADJ usecs. ADJ corrects for the fact
	 * that CPU is not infinitely fast.
	 * On infinitely fast CPU, next wakeup would be
	 * on (exactly_next_whole_second - ADJ). On real CPUs,
	 * this difference between current time and whole second
	 * is less than ADJ (assuming system isn't heavily loaded).
	 */
	/* Small value of 256us gives very precise sync for 2+ GHz CPUs.
	 * Slower CPUs will fail to sync and will go to bigger
	 * ADJ values. qemu-emulated armv4tl with ~100 MHz
	 * performance ends up using ADJ ~= 4*1024 and it takes
	 * 2+ secs (2 tries with successively larger ADJ)
	 * to sync. Even straced one on the same qemu (very slow)
	 * takes only 4 tries.
	 */
#define TWEAK_USEC 256
	unsigned adj = TWEAK_USEC;
	struct tm tm_time;
	struct timeval tv;
	int rtc = rtc_xopen(pp_rtcname, O_WRONLY);

	/* Try to catch the moment when whole second is close */
	while (1) {
		unsigned rem_usec;
		time_t t;

		gettimeofday(&tv, NULL);

		t = tv.tv_sec;
		rem_usec = 1000000 - tv.tv_usec;
		if (rem_usec < adj) {
			/* Close enough */
 small_rem:
			t++;
		}

		/* Prepare tm_time from t */
		if (utc)
			gmtime_r(&t, &tm_time); /* may read /etc/xxx (it takes time) */
		else
			localtime_r(&t, &tm_time); /* same */

		if (adj >= 32*1024) {
			break; /* 32 ms diff and still no luck?? give up trying to sync */
		}

		/* gmtime/localtime took some time, re-get cur time */
		gettimeofday(&tv, NULL);

		if (tv.tv_sec < t /* we are still in old second */
		 || (tv.tv_sec == t && tv.tv_usec < adj) /* not too far into next second */
		) {
			break; /* good, we are in sync! */
		}

		rem_usec = 1000000 - tv.tv_usec;
		if (rem_usec < adj) {
			t = tv.tv_sec;
			goto small_rem; /* already close to next sec, don't sleep */
		}

		/* Try to sync up by sleeping */
		usleep(rem_usec - adj);

		/* Jump to 1ms diff, then increase fast (x2): EVERY loop
		 * takes ~1 sec, people won't like slowly converging code here!
		 */
	//bb_error_msg("adj:%d tv.tv_usec:%d", adj, (int)tv.tv_usec);
		if (adj < 512)
			adj = 512;
		/* ... and if last "overshoot" does not look insanely big,
		 * just use it as adj increment. This makes convergence faster.
		 */
		if (tv.tv_usec < adj * 8) {
			adj += tv.tv_usec;
			continue;
		}
		adj *= 2;
	}
	/* Debug aid to find "optimal" TWEAK_USEC with nearly exact sync.
	 * Look for a value which makes tv_usec close to 999999 or 0.
	 * For 2.20GHz Intel Core 2: optimal TWEAK_USEC ~= 200
	 */
	//bb_error_msg("tv.tv_usec:%d", (int)tv.tv_usec);
#endif

	tm_time.tm_isdst = 0;
	xioctl(rtc, RTC_SET_TIME, &tm_time);

	if (ENABLE_FEATURE_CLEAN_UP)
		close(rtc);
}

#define HWCLOCK_OPT_LOCALTIME   0x01
#define HWCLOCK_OPT_UTC         0x02
#define HWCLOCK_OPT_SHOW        0x04
#define HWCLOCK_OPT_HCTOSYS     0x08
#define HWCLOCK_OPT_SYSTOHC     0x10
#define HWCLOCK_OPT_RTCFILE     0x20

int hwclock_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int hwclock_main(int argc UNUSED_PARAM, char **argv)
{
	const char *rtcname = NULL;
	unsigned opt;
	int utc;

#if ENABLE_FEATURE_HWCLOCK_LONG_OPTIONS
	static const char hwclock_longopts[] ALIGN1 =
		"localtime\0" No_argument "l"
		"utc\0"       No_argument "u"
		"show\0"      No_argument "r"
		"hctosys\0"   No_argument "s"
		"systohc\0"   No_argument "w"
		"file\0"      Required_argument "f"
		;
	applet_long_options = hwclock_longopts;
#endif
	opt_complementary = "r--ws:w--rs:s--wr:l--u:u--l";
	opt = getopt32(argv, "lurswf:", &rtcname);

	/* If -u or -l wasn't given check if we are using utc */
	if (opt & (HWCLOCK_OPT_UTC | HWCLOCK_OPT_LOCALTIME))
		utc = (opt & HWCLOCK_OPT_UTC);
	else
		utc = rtc_adjtime_is_utc();

	if (opt & HWCLOCK_OPT_HCTOSYS)
		to_sys_clock(&rtcname, utc);
	else if (opt & HWCLOCK_OPT_SYSTOHC)
		from_sys_clock(&rtcname, utc);
	else
		/* default HWCLOCK_OPT_SHOW */
		show_clock(&rtcname, utc);

	return 0;
}
