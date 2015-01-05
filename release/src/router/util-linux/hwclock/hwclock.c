/*
 * hwclock.c
 *
 * clock.c was written by Charles Hedrick, hedrick@cs.rutgers.edu, Apr 1992
 * Modified for clock adjustments - Rob Hooft <hooft@chem.ruu.nl>, Nov 1992
 * Improvements by Harald Koenig <koenig@nova.tat.physik.uni-tuebingen.de>
 * and Alan Modra <alan@spri.levels.unisa.edu.au>.
 *
 * Major rewrite by Bryan Henderson <bryanh@giraffe-data.com>, 96.09.19.
 * The new program is called hwclock. New features:
 *
 *	- You can set the hardware clock without also modifying the system
 *	  clock.
 *	- You can read and set the clock with finer than 1 second precision.
 *	- When you set the clock, hwclock automatically refigures the drift
 *	  rate, based on how far off the clock was before you set it.
 *
 * Reshuffled things, added sparc code, and re-added alpha stuff
 * by David Mosberger <davidm@azstarnet.com>
 * and Jay Estabrook <jestabro@amt.tay1.dec.com>
 * and Martin Ostermann <ost@coments.rwth-aachen.de>, aeb@cwi.nl, 990212.
 *
 * Fix for Award 2094 bug, Dave Coffin (dcoffin@shore.net) 11/12/98
 * Change of local time handling, Stefan Ring <e9725446@stud3.tuwien.ac.at>
 * Change of adjtime handling, James P. Rutledge <ao112@rgfn.epcc.edu>.
 *
 * Distributed under GPL
 */
/*
 * Explanation of `adjusting' (Rob Hooft):
 *
 * The problem with my machine is that its CMOS clock is 10 seconds
 * per day slow. With this version of clock.c, and my '/etc/rc.local'
 * reading '/etc/clock -au' instead of '/etc/clock -u -s', this error
 * is automatically corrected at every boot.
 *
 * To do this job, the program reads and writes the file '/etc/adjtime'
 * to determine the correction, and to save its data. In this file are
 * three numbers:
 *
 *	1) the correction in seconds per day. (So if your clock runs 5
 *	   seconds per day fast, the first number should read -5.0)
 *	2) the number of seconds since 1/1/1970 the last time the program
 *	   was used
 *	3) the remaining part of a second which was leftover after the last
 *	   adjustment
 *
 * Installation and use of this program:
 *
 *	a) create a file '/etc/adjtime' containing as the first and only
 *	   line: '0.0 0 0.0'
 *	b) run 'clock -au' or 'clock -a', depending on whether your cmos is
 *	   in universal or local time. This updates the second number.
 *	c) set your system time using the 'date' command.
 *	d) update your cmos time using 'clock -wu' or 'clock -w'
 *	e) replace the first number in /etc/adjtime by your correction.
 *	f) put the command 'clock -au' or 'clock -a' in your '/etc/rc.local'
 */

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "c.h"
#include "clock.h"
#include "nls.h"
#include "pathnames.h"
#include "strutils.h"

#ifdef HAVE_LIBAUDIT
#include <libaudit.h>
static int hwaudit_fd = -1;
static int hwaudit_on;
#endif

/* The struct that holds our hardware access routines */
struct clock_ops *ur;

#define FLOOR(arg) ((arg >= 0 ? (int) arg : ((int) arg) - 1));

const char *adj_file_name = NULL;

struct adjtime {
	/*
	 * This is information we keep in the adjtime file that tells us how
	 * to do drift corrections. Elements are all straight from the
	 * adjtime file, so see documentation of that file for details.
	 * Exception is <dirty>, which is an indication that what's in this
	 * structure is not what's in the disk file (because it has been
	 * updated since read from the disk file).
	 */
	bool dirty;
	/* line 1 */
	double drift_factor;
	time_t last_adj_time;
	double not_adjusted;
	/* line 2 */
	time_t last_calib_time;
	/*
	 * The most recent time that we set the clock from an external
	 * authority (as opposed to just doing a drift adjustment)
	 */
	/* line 3 */
	enum a_local_utc { LOCAL, UTC, UNKNOWN } local_utc;
	/*
	 * To which time zone, local or UTC, we most recently set the
	 * hardware clock.
	 */
};

/* Long only options. */
enum {
	OPT_SET = CHAR_MAX + 1,
	OPT_GETEPOCH,
	OPT_SETEPOCH,
	OPT_NOADJFILE,
	OPT_LOCALTIME,
	OPT_BADYEAR,
	OPT_DIRECTISA,
	OPT_TEST,
	OPT_DATE,
	OPT_EPOCH,
	OPT_ADJFILE,
	OPT_SYSTZ,
	OPT_PREDICT_HC
};

/*
 * We are running in debug mode, wherein we put a lot of information about
 * what we're doing to standard output.
 */
bool debug;

/* Workaround for Award 4.50g BIOS bug: keep the year in a file. */
bool badyear;

/* User-specified epoch, used when rtc fails to return epoch. */
unsigned long epoch_option = -1;

/*
 * Almost all Award BIOS's made between 04/26/94 and 05/31/95 have a nasty
 * bug limiting the RTC year byte to the range 94-99. Any year between 2000
 * and 2093 gets changed to 2094, every time you start the system.
 *
 * With the --badyear option, we write the date to file and hope that the
 * file is updated at least once a year. I recommend putting this command
 * "hwclock --badyear" in the monthly crontab, just to be safe.
 *
 * -- Dave Coffin 11/12/98
 */
static void write_date_to_file(struct tm *tm)
{
	FILE *fp;

	if ((fp = fopen(_PATH_LASTDATE, "w"))) {
		fprintf(fp, "%02d.%02d.%04d\n", tm->tm_mday, tm->tm_mon + 1,
			tm->tm_year + 1900);
		fclose(fp);
	} else
		warn(_("cannot write %s"), _PATH_LASTDATE);
}

static void read_date_from_file(struct tm *tm)
{
	int last_mday, last_mon, last_year;
	FILE *fp;

	if ((fp = fopen(_PATH_LASTDATE, "r"))) {
		if (fscanf(fp, "%d.%d.%d\n", &last_mday, &last_mon, &last_year)
		    == 3) {
			tm->tm_year = last_year - 1900;
			if ((tm->tm_mon << 5) + tm->tm_mday <
			    ((last_mon - 1) << 5) + last_mday)
				tm->tm_year++;
		}
		fclose(fp);
	}
	write_date_to_file(tm);
}

/*
 * The difference in seconds between two times in "timeval" format.
 */
double time_diff(struct timeval subtrahend, struct timeval subtractor)
{
	return (subtrahend.tv_sec - subtractor.tv_sec)
	    + (subtrahend.tv_usec - subtractor.tv_usec) / 1E6;
}

/*
 * The time, in "timeval" format, which is <increment> seconds after the
 * time <addend>. Of course, <increment> may be negative.
 */
static struct timeval time_inc(struct timeval addend, double increment)
{
	struct timeval newtime;

	newtime.tv_sec = addend.tv_sec + (int)increment;
	newtime.tv_usec = addend.tv_usec + (increment - (int)increment) * 1E6;

	/*
	 * Now adjust it so that the microsecond value is between 0 and 1
	 * million.
	 */
	if (newtime.tv_usec < 0) {
		newtime.tv_usec += 1E6;
		newtime.tv_sec -= 1;
	} else if (newtime.tv_usec >= 1E6) {
		newtime.tv_usec -= 1E6;
		newtime.tv_sec += 1;
	}
	return newtime;
}

static bool
hw_clock_is_utc(const bool utc, const bool local_opt,
		const struct adjtime adjtime)
{
	bool ret;

	if (utc)
		ret = TRUE;	/* --utc explicitly given on command line */
	else if (local_opt)
		ret = FALSE;	/* --localtime explicitly given */
	else
		/* get info from adjtime file - default is UTC */
		ret = (adjtime.local_utc != LOCAL);
	if (debug)
		printf(_("Assuming hardware clock is kept in %s time.\n"),
		       ret ? _("UTC") : _("local"));
	return ret;
}

/*
 * Read the adjustment parameters out of the /etc/adjtime file.
 *
 * Return them as the adjtime structure <*adjtime_p>. If there is no
 * /etc/adjtime file, return defaults. If values are missing from the file,
 * return defaults for them.
 *
 * return value 0 if all OK, !=0 otherwise.
 */
static int read_adjtime(struct adjtime *adjtime_p)
{
	FILE *adjfile;
	int rc;			/* local return code */
	struct stat statbuf;	/* We don't even use the contents of this. */
	char line1[81];		/* String: first line of adjtime file */
	char line2[81];		/* String: second line of adjtime file */
	char line3[81];		/* String: third line of adjtime file */
	long timeval;

	rc = stat(adj_file_name, &statbuf);
	if (rc < 0 && errno == ENOENT) {
		/* He doesn't have a adjtime file, so we'll use defaults. */
		adjtime_p->drift_factor = 0;
		adjtime_p->last_adj_time = 0;
		adjtime_p->not_adjusted = 0;
		adjtime_p->last_calib_time = 0;
		adjtime_p->local_utc = UNKNOWN;
		adjtime_p->dirty = FALSE;	/* don't create a zero adjfile */

		return 0;
	}

	adjfile = fopen(adj_file_name, "r");	/* open file for reading */
	if (adjfile == NULL) {
		warn("cannot open file %s", adj_file_name);
		return EX_OSFILE;
	}


	if (!fgets(line1, sizeof(line1), adjfile))
		line1[0] = '\0';	/* In case fgets fails */
	if (!fgets(line2, sizeof(line2), adjfile))
		line2[0] = '\0';	/* In case fgets fails */
	if (!fgets(line3, sizeof(line3), adjfile))
		line3[0] = '\0';	/* In case fgets fails */

	fclose(adjfile);

	/* Set defaults in case values are missing from file */
	adjtime_p->drift_factor = 0;
	adjtime_p->last_adj_time = 0;
	adjtime_p->not_adjusted = 0;
	adjtime_p->last_calib_time = 0;
	timeval = 0;

	sscanf(line1, "%lf %ld %lf",
	       &adjtime_p->drift_factor,
	       &timeval, &adjtime_p->not_adjusted);
	adjtime_p->last_adj_time = timeval;

	sscanf(line2, "%ld", &timeval);
	adjtime_p->last_calib_time = timeval;

	if (!strcmp(line3, "UTC\n")) {
		adjtime_p->local_utc = UTC;
	} else if (!strcmp(line3, "LOCAL\n")) {
		adjtime_p->local_utc = LOCAL;
	} else {
		adjtime_p->local_utc = UNKNOWN;
		if (line3[0]) {
			warnx(_("Warning: unrecognized third line in adjtime file\n"
				"(Expected: `UTC' or `LOCAL' or nothing.)"));
		}
	}

	adjtime_p->dirty = FALSE;

	if (debug) {
		printf(_
		       ("Last drift adjustment done at %ld seconds after 1969\n"),
		       (long)adjtime_p->last_adj_time);
		printf(_("Last calibration done at %ld seconds after 1969\n"),
		       (long)adjtime_p->last_calib_time);
		printf(_("Hardware clock is on %s time\n"),
		       (adjtime_p->local_utc ==
			LOCAL) ? _("local") : (adjtime_p->local_utc ==
					       UTC) ? _("UTC") : _("unknown"));
	}

	return 0;
}

/*
 * Wait until the falling edge of the Hardware Clock's update flag so that
 * any time that is read from the clock immediately after we return will be
 * exact.
 *
 * The clock only has 1 second precision, so it gives the exact time only
 * once per second, right on the falling edge of the update flag.
 *
 * We wait (up to one second) either blocked waiting for an rtc device or in
 * a CPU spin loop. The former is probably not very accurate.
 *
 * Return 0 if it worked, nonzero if it didn't.
 */
static int synchronize_to_clock_tick(void)
{
	int rc;

	if (debug)
		printf(_("Waiting for clock tick...\n"));

	rc = ur->synchronize_to_clock_tick();

	if (debug) {
		if (rc)
			printf(_("...synchronization failed\n"));
		else
			printf(_("...got clock tick\n"));
	}

	return rc;
}

/*
 * Convert a time in broken down format (hours, minutes, etc.) into standard
 * unix time (seconds into epoch). Return it as *systime_p.
 *
 * The broken down time is argument <tm>. This broken down time is either
 * in local time zone or UTC, depending on value of logical argument
 * "universal". True means it is in UTC.
 *
 * If the argument contains values that do not constitute a valid time, and
 * mktime() recognizes this, return *valid_p == false and *systime_p
 * undefined. However, mktime() sometimes goes ahead and computes a
 * fictional time "as if" the input values were valid, e.g. if they indicate
 * the 31st day of April, mktime() may compute the time of May 1. In such a
 * case, we return the same fictional value mktime() does as *systime_p and
 * return *valid_p == true.
 */
static void
mktime_tz(struct tm tm, const bool universal,
	  bool * valid_p, time_t * systime_p)
{
	time_t mktime_result;	/* The value returned by our mktime() call */
	char *zone;		/* Local time zone name */

	/*
	 * We use the C library function mktime(), but since it only works
	 * on local time zone input, we may have to fake it out by
	 * temporarily changing the local time zone to UTC.
	 */
	zone = getenv("TZ");	/* remember original time zone */
	if (universal) {
		/* Set timezone to UTC */
		setenv("TZ", "", TRUE);
		/*
		 * Note: tzset() gets called implicitly by the time code,
		 * but only the first time. When changing the environment
		 * variable, better call tzset() explicitly.
		 */
		tzset();
	}
	mktime_result = mktime(&tm);
	if (mktime_result == -1) {
		/*
		 * This apparently (not specified in mktime() documentation)
		 * means the 'tm' structure does not contain valid values
		 * (however, not containing valid values does _not_ imply
		 * mktime() returns -1).
		 */
		*valid_p = FALSE;
		*systime_p = 0;
		if (debug)
			printf(_("Invalid values in hardware clock: "
				 "%4d/%.2d/%.2d %.2d:%.2d:%.2d\n"),
			       tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			       tm.tm_hour, tm.tm_min, tm.tm_sec);
	} else {
		*valid_p = TRUE;
		*systime_p = mktime_result;
		if (debug)
			printf(_
			       ("Hw clock time : %4d/%.2d/%.2d %.2d:%.2d:%.2d = "
				"%ld seconds since 1969\n"), tm.tm_year + 1900,
			       tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
			       tm.tm_sec, (long)*systime_p);
	}
	/* now put back the original zone. */
	if (zone)
		setenv("TZ", zone, TRUE);
	else
		unsetenv("TZ");
	tzset();
}

/*
 * Read the hardware clock and return the current time via <tm> argument.
 *
 * Use the method indicated by <method> argument to access the hardware
 * clock.
 */
static int
read_hardware_clock(const bool universal, bool * valid_p, time_t * systime_p)
{
	struct tm tm;
	int err;

	err = ur->read_hardware_clock(&tm);
	if (err)
		return err;

	if (badyear)
		read_date_from_file(&tm);

	if (debug)
		printf(_
		       ("Time read from Hardware Clock: %4d/%.2d/%.2d %02d:%02d:%02d\n"),
		       tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
		       tm.tm_min, tm.tm_sec);
	mktime_tz(tm, universal, valid_p, systime_p);

	return 0;
}

/*
 * Set the Hardware Clock to the time <newtime>, in local time zone or UTC,
 * according to <universal>.
 */
static void
set_hardware_clock(const time_t newtime,
		   const bool universal, const bool testing)
{
	struct tm new_broken_time;
	/*
	 * Time to which we will set Hardware Clock, in broken down format,
	 * in the time zone of caller's choice
	 */

	if (universal)
		new_broken_time = *gmtime(&newtime);
	else
		new_broken_time = *localtime(&newtime);

	if (debug)
		printf(_("Setting Hardware Clock to %.2d:%.2d:%.2d "
			 "= %ld seconds since 1969\n"),
		       new_broken_time.tm_hour, new_broken_time.tm_min,
		       new_broken_time.tm_sec, (long)newtime);

	if (testing)
		printf(_("Clock not changed - testing only.\n"));
	else {
		if (badyear) {
			/*
			 * Write the real year to a file, then write a fake
			 * year between 1995 and 1998 to the RTC. This way,
			 * Award BIOS boots on 29 Feb 2000 thinking that
			 * it's 29 Feb 1996.
			 */
			write_date_to_file(&new_broken_time);
			new_broken_time.tm_year =
			    95 + ((new_broken_time.tm_year + 1) & 3);
		}
		ur->set_hardware_clock(&new_broken_time);
	}
}

/*
 * Set the Hardware Clock to the time "sethwtime", in local time zone or
 * UTC, according to "universal".
 *
 * Wait for a fraction of a second so that "sethwtime" is the value of the
 * Hardware Clock as of system time "refsystime", which is in the past. For
 * example, if "sethwtime" is 14:03:05 and "refsystime" is 12:10:04.5 and
 * the current system time is 12:10:06.0: Wait .5 seconds (to make exactly 2
 * seconds since "refsystime") and then set the Hardware Clock to 14:03:07,
 * thus getting a precise and retroactive setting of the clock.
 *
 * (Don't be confused by the fact that the system clock and the Hardware
 * Clock differ by two hours in the above example. That's just to remind you
 * that there are two independent time scales here).
 *
 * This function ought to be able to accept set times as fractional times.
 * Idea for future enhancement.
 */
static void
set_hardware_clock_exact(const time_t sethwtime,
			 const struct timeval refsystime,
			 const bool universal, const bool testing)
{
	time_t newhwtime = sethwtime;
	struct timeval beginsystime, nowsystime;
	double tdiff;
	int time_resync = 1;

	/*
	 * Now delay some more until Hardware Clock time newhwtime arrives.
	 * The 0.5 s is because the Hardware Clock always sets to your set
	 * time plus 500 ms (because it is designed to update to the next
	 * second precisely 500 ms after you finish the setting).
	 */
	do {
		if (time_resync) {
			gettimeofday(&beginsystime, NULL);
			tdiff = time_diff(beginsystime, refsystime);
			newhwtime = sethwtime + (int)(tdiff + 0.5);
			if (debug)
				printf(_
				       ("Time elapsed since reference time has been %.6f seconds.\n"
					"Delaying further to reach the new time.\n"),
				       tdiff);
			time_resync = 0;
		}

		gettimeofday(&nowsystime, NULL);
		tdiff = time_diff(nowsystime, beginsystime);
		if (tdiff < 0) {
			time_resync = 1;	/* probably backward time reset */
			continue;
		}
		if (tdiff > 0.1) {
			time_resync = 1;	/* probably forward time reset */
			continue;
		}
		beginsystime = nowsystime;
		tdiff = time_diff(nowsystime, refsystime);
	} while (newhwtime == sethwtime + (int)(tdiff + 0.5));

	set_hardware_clock(newhwtime, universal, testing);
}

/*
 * Put the time "systime" on standard output in display format. Except if
 * hclock_valid == false, just tell standard output that we don't know what
 * time it is.
 *
 * Include in the output the adjustment "sync_duration".
 */
static void
display_time(const bool hclock_valid, const time_t systime,
	     const double sync_duration)
{
	if (!hclock_valid)
		warnx(_
		      ("The Hardware Clock registers contain values that are "
		       "either invalid (e.g. 50th day of month) or beyond the range "
		       "we can handle (e.g. Year 2095)."));
	else {
		struct tm *lt;
		char *format = "%c";
		char ctime_now[200];

		lt = localtime(&systime);
		strftime(ctime_now, sizeof(ctime_now), format, lt);
		printf(_("%s  %.6f seconds\n"), ctime_now, -(sync_duration));
	}
}

/*
 * Interpret the value of the --date option, which is something like
 * "13:05:01". In fact, it can be any of the myriad ASCII strings that
 * specify a time which the "date" program can understand. The date option
 * value in question is our "dateopt" argument.
 *
 * The specified time is in the local time zone.
 *
 * Our output, "*time_p", is a seconds-into-epoch time.
 *
 * We use the "date" program to interpret the date string. "date" must be
 * runnable by issuing the command "date" to the /bin/sh shell. That means
 * in must be in the current PATH.
 *
 * If anything goes wrong (and many things can), we return return code 10
 * and arbitrary *time_p. Otherwise, return code is 0 and *time_p is valid.
 */
static int interpret_date_string(const char *date_opt, time_t * const time_p)
{
	FILE *date_child_fp;
	char date_resp[100];
	const char magic[] = "seconds-into-epoch=";
	char date_command[100];
	int retcode;		/* our eventual return code */
	int rc;			/* local return code */

	if (date_opt == NULL) {
		warnx(_("No --date option specified."));
		return 14;
	}

	/* prevent overflow - a security risk */
	if (strlen(date_opt) > sizeof(date_command) - 50) {
		warnx(_("--date argument too long"));
		return 13;
	}

	/* Quotes in date_opt would ruin the date command we construct. */
	if (strchr(date_opt, '"') != NULL) {
		warnx(_
		      ("The value of the --date option is not a valid date.\n"
		       "In particular, it contains quotation marks."));
		return 12;
	}

	sprintf(date_command, "date --date=\"%s\" +seconds-into-epoch=%%s",
		date_opt);
	if (debug)
		printf(_("Issuing date command: %s\n"), date_command);

	date_child_fp = popen(date_command, "r");
	if (date_child_fp == NULL) {
		warn(_("Unable to run 'date' program in /bin/sh shell. "
			    "popen() failed"));
		return 10;
	}

	if (!fgets(date_resp, sizeof(date_resp), date_child_fp))
		date_resp[0] = '\0';	/* in case fgets fails */
	if (debug)
		printf(_("response from date command = %s\n"), date_resp);
	if (strncmp(date_resp, magic, sizeof(magic) - 1) != 0) {
		warnx(_("The date command issued by %s returned "
				  "unexpected results.\n"
				  "The command was:\n  %s\n"
				  "The response was:\n  %s"),
			program_invocation_short_name, date_command, date_resp);
		retcode = 8;
	} else {
		long seconds_since_epoch;
		rc = sscanf(date_resp + sizeof(magic) - 1, "%ld",
			    &seconds_since_epoch);
		if (rc < 1) {
			warnx(_("The date command issued by %s returned "
				"something other than an integer where the "
				"converted time value was expected.\n"
				"The command was:\n  %s\n"
				"The response was:\n %s\n"),
			      program_invocation_short_name, date_command,
			      date_resp);
			retcode = 6;
		} else {
			retcode = 0;
			*time_p = seconds_since_epoch;
			if (debug)
				printf(_("date string %s equates to "
					 "%ld seconds since 1969.\n"),
				       date_opt, (long)*time_p);
		}
	}
	pclose(date_child_fp);

	return retcode;
}

/*
 * Set the System Clock to time 'newtime'.
 *
 * Also set the kernel time zone value to the value indicated by the TZ
 * environment variable and/or /usr/lib/zoneinfo/, interpreted as tzset()
 * would interpret them.
 *
 * EXCEPT: if hclock_valid is false, just issue an error message saying
 * there is no valid time in the Hardware Clock to which to set the system
 * time.
 *
 * If 'testing' is true, don't actually update anything -- just say we would
 * have.
 */
static int
set_system_clock(const bool hclock_valid, const time_t newtime,
		 const bool testing)
{
	int retcode;

	if (!hclock_valid) {
		warnx(_
		      ("The Hardware Clock does not contain a valid time, so "
		       "we cannot set the System Time from it."));
		retcode = 1;
	} else {
		struct timeval tv;
		struct tm *broken;
		int minuteswest;
		int rc;

		tv.tv_sec = newtime;
		tv.tv_usec = 0;

		broken = localtime(&newtime);
#ifdef HAVE_TM_GMTOFF
		minuteswest = -broken->tm_gmtoff / 60;	/* GNU extension */
#else
		minuteswest = timezone / 60;
		if (broken->tm_isdst)
			minuteswest -= 60;
#endif

		if (debug) {
			printf(_("Calling settimeofday:\n"));
			printf(_("\ttv.tv_sec = %ld, tv.tv_usec = %ld\n"),
			       (long)tv.tv_sec, (long)tv.tv_usec);
			printf(_("\ttz.tz_minuteswest = %d\n"), minuteswest);
		}
		if (testing) {
			printf(_
			       ("Not setting system clock because running in test mode.\n"));
			retcode = 0;
		} else {
			const struct timezone tz = { minuteswest, 0 };

			rc = settimeofday(&tv, &tz);
			if (rc) {
				if (errno == EPERM) {
					warnx(_
					      ("Must be superuser to set system clock."));
					retcode = EX_NOPERM;
				} else {
					warn(_("settimeofday() failed"));
					retcode = 1;
				}
			} else
				retcode = 0;
		}
	}
	return retcode;
}

/*
 * Reset the System Clock from local time to UTC, based on its current value
 * and the timezone unless universal is TRUE.
 *
 * Also set the kernel time zone value to the value indicated by the TZ
 * environment variable and/or /usr/lib/zoneinfo/, interpreted as tzset()
 * would interpret them.
 *
 * If 'testing' is true, don't actually update anything -- just say we would
 * have.
 */
static int set_system_clock_timezone(const bool universal, const bool testing)
{
	int retcode;
	struct timeval tv;
	struct tm *broken;
	int minuteswest;
	int rc;

	gettimeofday(&tv, NULL);
	if (debug) {
		struct tm broken_time;
		char ctime_now[200];

		broken_time = *gmtime(&tv.tv_sec);
		strftime(ctime_now, sizeof(ctime_now), "%Y/%m/%d %H:%M:%S",
			 &broken_time);
		printf(_("Current system time: %ld = %s\n"), (long)tv.tv_sec,
		       ctime_now);
	}

	broken = localtime(&tv.tv_sec);
#ifdef HAVE_TM_GMTOFF
	minuteswest = -broken->tm_gmtoff / 60;	/* GNU extension */
#else
	minuteswest = timezone / 60;
	if (broken->tm_isdst)
		minuteswest -= 60;
#endif

	gettimeofday(&tv, NULL);
	if (!universal)
		tv.tv_sec += minuteswest * 60;

	if (debug) {
		struct tm broken_time;
		char ctime_now[200];

		broken_time = *gmtime(&tv.tv_sec);
		strftime(ctime_now, sizeof(ctime_now), "%Y/%m/%d %H:%M:%S",
			 &broken_time);

		printf(_("Calling settimeofday:\n"));
		printf(_("\tUTC: %s\n"), ctime_now);
		printf(_("\ttv.tv_sec = %ld, tv.tv_usec = %ld\n"),
		       (long)tv.tv_sec, (long)tv.tv_usec);
		printf(_("\ttz.tz_minuteswest = %d\n"), minuteswest);
	}
	if (testing) {
		printf(_
		       ("Not setting system clock because running in test mode.\n"));
		retcode = 0;
	} else {
		const struct timezone tz = { minuteswest, 0 };

		rc = settimeofday(&tv, &tz);
		if (rc) {
			if (errno == EPERM) {
				warnx(_
				      ("Must be superuser to set system clock."));
				retcode = EX_NOPERM;
			} else {
				warn(_("settimeofday() failed"));
				retcode = 1;
			}
		} else
			retcode = 0;
	}
	return retcode;
}

/*
 * Update the drift factor in <*adjtime_p> to reflect the fact that the
 * Hardware Clock was calibrated to <nowtime> and before that was set to
 * <hclocktime>.
 *
 * We record in the adjtime file the time at which we last calibrated the
 * clock so we can compute the drift rate each time we calibrate.
 *
 * EXCEPT: if <hclock_valid> is false, assume Hardware Clock was not set
 * before to anything meaningful and regular adjustments have not been done,
 * so don't adjust the drift factor.
 */
static void
adjust_drift_factor(struct adjtime *adjtime_p,
		    const time_t nowtime,
		    const bool hclock_valid,
		    const time_t hclocktime, const double sync_delay)
{
	if (!hclock_valid) {
		if (debug)
			printf(_("Not adjusting drift factor because the "
				 "Hardware Clock previously contained "
				 "garbage.\n"));
	} else if (adjtime_p->last_calib_time == 0) {
		if (debug)
			printf(_("Not adjusting drift factor because last "
				 "calibration time is zero,\n"
				 "so history is bad and calibration startover "
				 "is necessary.\n"));
	} else if ((hclocktime - adjtime_p->last_calib_time) < 23 * 60 * 60) {
		if (debug)
			printf(_("Not adjusting drift factor because it has "
				 "been less than a day since the last "
				 "calibration.\n"));
	} else if (adjtime_p->last_calib_time != 0) {
		/*
		 * At adjustment time we adjust the hardware clock according
		 * to the contents of /etc/adjtime.
		 *
		 * At calibration time we set the hardware clock and update
		 * /etc/adjtime, that is, for each calibration (except the
		 * first) we also do an adjustment.
		 *
		 * We are now at calibration time.
		 *
		 * Let us do computation in doubles. (Floats almost suffice,
		 * but 195 days + 1 second equals 195 days in floats.)
		 */
		const double sec_per_day = 24.0 * 60.0 * 60.0;
		double atime_per_htime;
		double adj_days, cal_days;
		double exp_drift, unc_drift;
		double factor_adjust;

		/* Adjusted time units per hardware time unit */
		atime_per_htime = 1.0 + adjtime_p->drift_factor / sec_per_day;

		/* Days since last adjustment (in hardware clock time) */
		adj_days = (double)(hclocktime - adjtime_p->last_adj_time)
		    / sec_per_day;

		/* Expected drift (sec) since last adjustment */
		exp_drift = adj_days * adjtime_p->drift_factor
		    + adjtime_p->not_adjusted;

		/* Uncorrected drift (sec) since last calibration */
		unc_drift = (double)(nowtime - hclocktime)
		    + sync_delay - exp_drift;

		/* Days since last calibration (in hardware clock time) */
		cal_days = ((double)(adjtime_p->last_adj_time
				     - adjtime_p->last_calib_time)
			    + adjtime_p->not_adjusted)
		    / (sec_per_day * atime_per_htime) + adj_days;

		/* Amount to add to previous drift factor */
		factor_adjust = unc_drift / cal_days;

		if (debug)
			printf(_("Clock drifted %.1f seconds in the past "
				 "%d seconds in spite of a drift factor of "
				 "%f seconds/day.\n"
				 "Adjusting drift factor by %f seconds/day\n"),
			       unc_drift,
			       (int)(nowtime - adjtime_p->last_calib_time),
			       adjtime_p->drift_factor, factor_adjust);

		adjtime_p->drift_factor += factor_adjust;
	}
	adjtime_p->last_calib_time = nowtime;

	adjtime_p->last_adj_time = nowtime;

	adjtime_p->not_adjusted = 0;

	adjtime_p->dirty = TRUE;
}

/*
 * Do the drift adjustment calculation.
 *
 * The way we have to set the clock, we need the adjustment in two parts:
 *
 *	1) an integer number of seconds (return as *adjustment_p)
 *	2) a positive fraction of a second (less than 1) (return as *retro_p)
 *
 * The sum of these two values is the adjustment needed. Positive means to
 * advance the clock or insert seconds. Negative means to retard the clock
 * or remove seconds.
 */
static void
calculate_adjustment(const double factor,
		     const time_t last_time,
		     const double not_adjusted,
		     const time_t systime, int *adjustment_p, double *retro_p)
{
	double exact_adjustment;

	exact_adjustment =
	    ((double)(systime - last_time)) * factor / (24 * 60 * 60)
	    + not_adjusted;
	*adjustment_p = FLOOR(exact_adjustment);

	*retro_p = exact_adjustment - (double)*adjustment_p;
	if (debug) {
		printf(_("Time since last adjustment is %d seconds\n"),
		       (int)(systime - last_time));
		printf(_("Need to insert %d seconds and refer time back "
			 "%.6f seconds ago\n"), *adjustment_p, *retro_p);
	}
}

/*
 * Write the contents of the <adjtime> structure to its disk file.
 *
 * But if the contents are clean (unchanged since read from disk), don't
 * bother.
 */
static void save_adjtime(const struct adjtime adjtime, const bool testing)
{
	char newfile[412];	/* Stuff to write to disk file */

	if (adjtime.dirty) {
		/*
		 * snprintf is not always available, but this is safe as
		 * long as libc does not use more than 100 positions for %ld
		 * or %f
		 */
		sprintf(newfile, "%f %ld %f\n%ld\n%s\n",
			adjtime.drift_factor,
			(long)adjtime.last_adj_time,
			adjtime.not_adjusted,
			(long)adjtime.last_calib_time,
			(adjtime.local_utc == UTC) ? "UTC" : "LOCAL");

		if (testing) {
			printf(_
			       ("Not updating adjtime file because of testing mode.\n"));
			printf(_("Would have written the following to %s:\n%s"),
			       adj_file_name, newfile);
		} else {
			FILE *adjfile;
			int err = 0;

			adjfile = fopen(adj_file_name, "w");
			if (adjfile == NULL) {
				warn(_
				     ("Could not open file with the clock adjustment parameters "
				      "in it (%s) for writing"), adj_file_name);
				err = 1;
			} else {
				if (fputs(newfile, adjfile) < 0) {
					warn(_
					     ("Could not update file with the clock adjustment "
					      "parameters (%s) in it"),
					     adj_file_name);
					err = 1;
				}
				if (fclose(adjfile) < 0) {
					warn(_
					     ("Could not update file with the clock adjustment "
					      "parameters (%s) in it"),
					     adj_file_name);
					err = 1;
				}
			}
			if (err)
				warnx(_
				      ("Drift adjustment parameters not updated."));
		}
	}
}

/*
 * Do the adjustment requested, by 1) setting the Hardware Clock (if
 * necessary), and 2) updating the last-adjusted time in the adjtime
 * structure.
 *
 * Do not update anything if the Hardware Clock does not currently present a
 * valid time.
 *
 * Arguments <factor> and <last_time> are current values from the adjtime
 * file.
 *
 * <hclock_valid> means the Hardware Clock contains a valid time, and that
 * time is <hclocktime>.
 *
 * <read_time> is the current system time (to be precise, it is the system
 * time at the time <hclocktime> was read, which due to computational delay
 * could be a short time ago).
 *
 * <universal>: the Hardware Clock is kept in UTC.
 *
 * <testing>:  We are running in test mode (no updating of clock).
 *
 * We do not bother to update the clock if the adjustment would be less than
 * one second. This is to avoid cumulative error and needless CPU hogging
 * (remember we use an infinite loop for some timing) if the user runs us
 * frequently.
 */
static void
do_adjustment(struct adjtime *adjtime_p,
	      const bool hclock_valid, const time_t hclocktime,
	      const struct timeval read_time,
	      const bool universal, const bool testing)
{
	if (!hclock_valid) {
		warnx(_("The Hardware Clock does not contain a valid time, "
			"so we cannot adjust it."));
		adjtime_p->last_calib_time = 0;	/* calibration startover is required */
		adjtime_p->last_adj_time = 0;
		adjtime_p->not_adjusted = 0;
		adjtime_p->dirty = TRUE;
	} else if (adjtime_p->last_adj_time == 0) {
		if (debug)
			printf(_
			       ("Not setting clock because last adjustment time is zero, "
				"so history is bad."));
	} else {
		int adjustment;
		/* Number of seconds we must insert in the Hardware Clock */
		double retro;
		/*
		 * Fraction of second we have to remove from clock after
		 * inserting <adjustment> whole seconds.
		 */
		calculate_adjustment(adjtime_p->drift_factor,
				     adjtime_p->last_adj_time,
				     adjtime_p->not_adjusted,
				     hclocktime, &adjustment, &retro);
		if (adjustment > 0 || adjustment < -1) {
			set_hardware_clock_exact(hclocktime + adjustment,
						 time_inc(read_time, -retro),
						 universal, testing);
			adjtime_p->last_adj_time = hclocktime + adjustment;
			adjtime_p->not_adjusted = 0;
			adjtime_p->dirty = TRUE;
		} else if (debug)
			printf(_("Needed adjustment is less than one second, "
				 "so not setting clock.\n"));
	}
}

static void determine_clock_access_method(const bool user_requests_ISA)
{
	ur = NULL;

	if (user_requests_ISA)
		ur = probe_for_cmos_clock();

#ifdef __linux__
	if (!ur)
		ur = probe_for_rtc_clock();
#endif

	if (!ur)
		ur = probe_for_kd_clock();

	if (!ur && !user_requests_ISA)
		ur = probe_for_cmos_clock();

	if (debug) {
		if (ur)
			printf(_("Using %s.\n"), ur->interface_name);
		else
			printf(_("No usable clock interface found.\n"));
	}
}

/*
 * Do all the normal work of hwclock - read, set clock, etc.
 *
 * Issue output to stdout and error message to stderr where appropriate.
 *
 * Return rc == 0 if everything went OK, rc != 0 if not.
 */
static int
manipulate_clock(const bool show, const bool adjust, const bool noadjfile,
		 const bool set, const time_t set_time,
		 const bool hctosys, const bool systohc, const bool systz,
		 const struct timeval startup_time,
		 const bool utc, const bool local_opt,
		 const bool testing, const bool predict)
{
	/* Contents of the adjtime file, or what they should be. */
	struct adjtime adjtime;
	bool universal;
	/* Set if user lacks necessary authorization to access the clock */
	bool no_auth;
	/* The time at which we read the Hardware Clock */
	struct timeval read_time;
	/*
	 * The Hardware Clock gives us a valid time, or at
	 * least something close enough to fool mktime().
	 */
	bool hclock_valid = FALSE;
	/*
	 * The time the hardware clock had just after we
	 * synchronized to its next clock tick when we
	 * started up. Defined only if hclock_valid is true.
	 */
	time_t hclocktime = 0;
	/* local return code */
	int rc = 0;

	if (!systz && !predict) {
		no_auth = ur->get_permissions();
		if (no_auth)
			return EX_NOPERM;
	}

	if (!noadjfile
	    && (adjust || set || systohc || (!utc && !local_opt) || predict)) {
		rc = read_adjtime(&adjtime);
		if (rc)
			return rc;
	} else {
		/* A little trick to avoid reading the file if we don't have to */
		adjtime.dirty = FALSE;
	}

	universal = hw_clock_is_utc(utc, local_opt, adjtime);

	if ((set || systohc || adjust) &&
	    (adjtime.local_utc == UTC) != universal) {
		adjtime.local_utc = universal ? UTC : LOCAL;
		adjtime.dirty = TRUE;
	}

	if (show || adjust || hctosys || (!noadjfile && !systz && !predict)) {
		/* data from HW-clock are required */
		rc = synchronize_to_clock_tick();

		/*
		 * 2 = synchronization timeout. We don't
		 * error out if the user is attempting to
		 * set the RTC - the RTC could be
		 * functioning but contain invalid time data
		 * so we still want to allow a user to set
		 * the RTC time.
		 */
		if (rc && rc != 2 && !set && !systohc)
			return EX_IOERR;
		gettimeofday(&read_time, NULL);

		/*
		 * If we can't synchronize to a clock tick,
		 * we likely can't read from the RTC so
		 * don't bother reading it again.
		 */
		if (!rc) {
			rc = read_hardware_clock(universal,
						 &hclock_valid, &hclocktime);
			if (rc && !set && !systohc)
				return EX_IOERR;
		}
	}

	if (show) {
		display_time(hclock_valid, hclocktime,
			     time_diff(read_time, startup_time));
	} else if (set) {
		set_hardware_clock_exact(set_time, startup_time,
					 universal, testing);
		if (!noadjfile)
			adjust_drift_factor(&adjtime, set_time,
					    hclock_valid,
					    hclocktime,
					    time_diff(read_time, startup_time));
	} else if (adjust) {
		do_adjustment(&adjtime, hclock_valid,
			      hclocktime, read_time, universal, testing);
	} else if (systohc) {
		struct timeval nowtime, reftime;
		/*
		 * We can only set_hardware_clock_exact to a
		 * whole seconds time, so we set it with
		 * reference to the most recent whole
		 * seconds time.
		 */
		gettimeofday(&nowtime, NULL);
		reftime.tv_sec = nowtime.tv_sec;
		reftime.tv_usec = 0;
		set_hardware_clock_exact((time_t)
					 reftime.tv_sec,
					 reftime, universal, testing);
		if (!noadjfile)
			adjust_drift_factor(&adjtime, (time_t)
					    reftime.tv_sec,
					    hclock_valid, hclocktime, (double)
					    read_time.tv_usec / 1E6);
	} else if (hctosys) {
		rc = set_system_clock(hclock_valid, hclocktime, testing);
		if (rc) {
			printf(_("Unable to set system clock.\n"));
			return rc;
		}
	} else if (systz) {
		rc = set_system_clock_timezone(universal, testing);
		if (rc) {
			printf(_("Unable to set system clock.\n"));
			return rc;
		}
	} else if (predict) {
		int adjustment;
		double retro;

		calculate_adjustment(adjtime.drift_factor,
				     adjtime.last_adj_time,
				     adjtime.not_adjusted,
				     set_time, &adjustment, &retro);
		if (debug) {
			printf(_
			       ("At %ld seconds after 1969, RTC is predicted to read %ld seconds after 1969.\n"),
			       set_time, set_time + adjustment);
		}
		display_time(TRUE, set_time + adjustment, -retro);
	}
	if (!noadjfile)
		save_adjtime(adjtime, testing);
	return 0;
}

/*
 * Get or set the Hardware Clock epoch value in the kernel, as appropriate.
 * <getepoch>, <setepoch>, and <epoch> are hwclock invocation options.
 *
 * <epoch> == -1 if the user did not specify an "epoch" option.
 */
#ifdef __linux__
/*
 * Maintenance note: This should work on non-Alpha machines, but the
 * evidence today (98.03.04) indicates that the kernel only keeps the epoch
 * value on Alphas. If that is ever fixed, this function should be changed.
 */
# ifndef __alpha__
static void
manipulate_epoch(const bool getepoch __attribute__ ((__unused__)),
		 const bool setepoch __attribute__ ((__unused__)),
		 const unsigned long epoch_opt __attribute__ ((__unused__)),
		 const bool testing __attribute__ ((__unused__)))
{
	warnx(_("The kernel keeps an epoch value for the Hardware Clock "
		"only on an Alpha machine.\nThis copy of hwclock was built for "
		"a machine other than Alpha\n(and thus is presumably not running "
		"on an Alpha now).  No action taken."));
}
# else
static void
manipulate_epoch(const bool getepoch,
		 const bool setepoch,
		 const unsigned long epoch_opt,
		 const bool testing)
{
	if (getepoch) {
		unsigned long epoch;

		if (get_epoch_rtc(&epoch, 0))
			warnx(_
			      ("Unable to get the epoch value from the kernel."));
		else
			printf(_("Kernel is assuming an epoch value of %lu\n"),
			       epoch);
	} else if (setepoch) {
		if (epoch_opt == -1)
			warnx(_
			      ("To set the epoch value, you must use the 'epoch' "
			       "option to tell to what value to set it."));
		else if (testing)
			printf(_
			       ("Not setting the epoch to %d - testing only.\n"),
			       epoch_opt);
		else if (set_epoch_rtc(epoch_opt))
			printf(_
			       ("Unable to set the epoch value in the kernel.\n"));
	}
}
# endif		/* __alpha__ */
#endif		/* __linux__ */

static void out_version(void)
{
	printf(_("%s from %s\n"), program_invocation_short_name, PACKAGE_STRING);
}

/*
 * usage - Output (error and) usage information
 *
 * This function is called both directly from main to show usage information
 * and as fatal function from shhopt if some argument is not understood. In
 * case of normal usage info FMT should be NULL. In that case the info is
 * printed to stdout. If FMT is given usage will act like fprintf( stderr,
 * fmt, ... ), show a usage information and terminate the program
 * afterwards.
 */
static void usage(const char *fmt, ...)
{
	FILE *usageto;
	va_list ap;

	usageto = fmt ? stderr : stdout;

	fputs(_("\nUsage:\n"), usageto);
	fputs(_(" hwclock [function] [option...]\n"), usageto);

	fputs(_("\nFunctions:\n"), usageto);
	fputs(_(" -h, --help           show this help text and exit\n"
		" -r, --show           read hardware clock and print result\n"
		"     --set            set the RTC to the time given with --date\n"), usageto);
	fputs(_(" -s, --hctosys        set the system time from the hardware clock\n"
		" -w, --systohc        set the hardware clock from the current system time\n"
		"     --systz          set the system time based on the current timezone\n"
		"     --adjust         adjust the RTC to account for systematic drift since\n"
		"                        the clock was last set or adjusted\n"), usageto);
#ifdef __linux__
	fputs(_("     --getepoch       print out the kernel's hardware clock epoch value\n"
		"     --setepoch       set the kernel's hardware clock epoch value to the \n"
		"                        value given with --epoch\n"), usageto);
#endif
	fputs(_("     --predict        predict RTC reading at time given with --date\n"
		" -V, --version        display version information and exit\n"), usageto);

	fputs(_("\nOptions:\n"), usageto);
	fputs(_(" -u, --utc            the hardware clock is kept in UTC\n"
		"     --localtime      the hardware clock is kept in local time\n"), usageto);
#ifdef __linux__
	fputs(_(" -f, --rtc <file>     special /dev/... file to use instead of default\n"), usageto);
#endif
	fprintf(usageto, _(
		"     --directisa      access the ISA bus directly instead of %s\n"
		"     --badyear        ignore RTC's year because the BIOS is broken\n"
		"     --date <time>    specifies the time to which to set the hardware clock\n"
		"     --epoch <year>   specifies the year which is the beginning of the\n"
		"                        hardware clock's epoch value\n"), _PATH_RTC_DEV);
	fprintf(usageto, _(
		"     --noadjfile      do not access %s; this requires the use of\n"
		"                        either --utc or --localtime\n"
		"     --adjfile <file> specifies the path to the adjust file;\n"
		"                        the default is %s\n"), _PATH_ADJPATH, _PATH_ADJPATH);
	fputs(_("     --test           do not update anything, just show what would happen\n"
		" -D, --debug          debugging mode\n" "\n"), usageto);
#ifdef __alpha__
	fputs(_(" -J|--jensen, -A|--arc, -S|--srm, -F|--funky-toy\n"
		"      tell hwclock the type of Alpha you have (see hwclock(8))\n"
		 "\n"), usageto);
#endif

	if (fmt) {
		va_start(ap, fmt);
		vfprintf(usageto, fmt, ap);
		va_end(ap);
	}

	fflush(usageto);
	hwclock_exit(fmt ? EX_USAGE : EX_OK);
}

/*
 * Returns:
 *  EX_USAGE: bad invocation
 *  EX_NOPERM: no permission
 *  EX_OSFILE: cannot open /dev/rtc or /etc/adjtime
 *  EX_IOERR: ioctl error getting or setting the time
 *  0: OK (or not)
 *  1: failure
 */
int main(int argc, char **argv)
{
	struct timeval startup_time;
	/*
	 * The time we started up, in seconds into the epoch, including
	 * fractions.
	 */
	time_t set_time = 0;	/* Time to which user said to set Hardware Clock */

	bool permitted;		/* User is permitted to do the function */
	int rc, c;

	/* Variables set by various options; show may also be set later */
	/* The options debug, badyear and epoch_option are global */
	bool show, set, systohc, hctosys, systz, adjust, getepoch, setepoch,
	    predict;
	bool utc, testing, local_opt, noadjfile, directisa;
	char *date_opt;
#ifdef __alpha__
	bool ARCconsole, Jensen, SRM, funky_toy;
#endif

	static const struct option longopts[] = {
		{"adjust",	0, 0, 'a'},
		{"help",	0, 0, 'h'},
		{"show",	0, 0, 'r'},
		{"hctosys",	0, 0, 's'},
		{"utc",		0, 0, 'u'},
		{"version",	0, 0, 'v'},
		{"systohc",	0, 0, 'w'},
		{"debug",	0, 0, 'D'},
#ifdef __alpha__
		{"ARC",		0, 0, 'A'},
		{"arc",		0, 0, 'A'},
		{"Jensen",	0, 0, 'J'},
		{"jensen",	0, 0, 'J'},
		{"SRM",		0, 0, 'S'},
		{"srm",		0, 0, 'S'},
		{"funky-toy",	0, 0, 'F'},
#endif
		{"set",		0, 0, OPT_SET},
#ifdef __linux__
		{"getepoch",	0, 0, OPT_GETEPOCH},
		{"setepoch",	0, 0, OPT_SETEPOCH},
#endif
		{"noadjfile",	0, 0, OPT_NOADJFILE},
		{"localtime",	0, 0, OPT_LOCALTIME},
		{"badyear",	0, 0, OPT_BADYEAR},
		{"directisa",	0, 0, OPT_DIRECTISA},
		{"test",	0, 0, OPT_TEST},
		{"date",	1, 0, OPT_DATE},
		{"epoch",	1, 0, OPT_EPOCH},
#ifdef __linux__
		{"rtc",		1, 0, 'f'},
#endif
		{"adjfile",	1, 0, OPT_ADJFILE},
		{"systz",	0, 0, OPT_SYSTZ},
		{"predict-hc",	0, 0, OPT_PREDICT_HC},
		{NULL,		0, NULL, 0}
	};

	/* Remember what time we were invoked */
	gettimeofday(&startup_time, NULL);

#ifdef HAVE_LIBAUDIT
	hwaudit_fd = audit_open();
	if (hwaudit_fd < 0 && !(errno == EINVAL || errno == EPROTONOSUPPORT ||
				errno == EAFNOSUPPORT)) {
		/*
		 * You get these error codes only when the kernel doesn't
		 * have audit compiled in.
		 */
		warnx(_("Unable to connect to audit system"));
		return EX_NOPERM;
	}
#endif
	setlocale(LC_ALL, "");
#ifdef LC_NUMERIC
	/*
	 * We need LC_CTYPE and LC_TIME and LC_MESSAGES, but must avoid
	 * LC_NUMERIC since it gives problems when we write to /etc/adjtime.
	 *  - gqueri@mail.dotcom.fr
	 */
	setlocale(LC_NUMERIC, "C");
#endif
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	/* Set option defaults */
	show = set = systohc = hctosys = systz = adjust = noadjfile = predict =
	    FALSE;
	getepoch = setepoch = utc = local_opt = directisa = testing = debug = FALSE;
#ifdef __alpha__
	ARCconsole = Jensen = SRM = funky_toy = badyear = FALSE;
#endif
	date_opt = NULL;

	while ((c = getopt_long(argc, argv, "?hvVDarsuwAJSFf:", longopts, NULL))
	       != -1) {
		switch (c) {
		case 'D':
			debug = TRUE;
			break;
		case 'a':
			adjust = TRUE;
			break;
		case 'r':
			show = TRUE;
			break;
		case 's':
			hctosys = TRUE;
			break;
		case 'u':
			utc = TRUE;
			break;
		case 'w':
			systohc = TRUE;
			break;
#ifdef __alpha__
		case 'A':
			ARCconsole = TRUE;
			break;
		case 'J':
			Jensen = TRUE;
			break;
		case 'S':
			SRM = TRUE;
			break;
		case 'F':
			funky_toy = TRUE;
			break;
#endif
		case OPT_SET:
			set = TRUE;
			break;
#ifdef __linux__
		case OPT_GETEPOCH:
			getepoch = TRUE;
			break;
		case OPT_SETEPOCH:
			setepoch = TRUE;
			break;
#endif
		case OPT_NOADJFILE:
			noadjfile = TRUE;
			break;
		case OPT_LOCALTIME:
			local_opt = TRUE;	/* --localtime */
			break;
		case OPT_BADYEAR:
			badyear = TRUE;
			break;
		case OPT_DIRECTISA:
			directisa = TRUE;
			break;
		case OPT_TEST:
			testing = TRUE;		/* --test */
			break;
		case OPT_DATE:
			date_opt = optarg;	/* --date */
			break;
		case OPT_EPOCH:
			epoch_option =		/* --epoch */
			    strtoul_or_err(optarg, _("failed to parse epoch"));
			break;
		case OPT_ADJFILE:
			adj_file_name = optarg;	/* --adjfile */
			break;
		case OPT_SYSTZ:
			systz = TRUE;		/* --systz */
			break;
		case OPT_PREDICT_HC:
			predict = TRUE;		/* --predict-hc */
			break;
#ifdef __linux__
		case 'f':
			rtc_dev_name = optarg;	/* --rtc */
			break;
#endif
		case 'v':			/* --version */
		case 'V':
			out_version();
			return 0;
		case 'h':			/* --help */
		case '?':
		default:
			usage(NULL);
		}
	}

	argc -= optind;
	argv += optind;

#ifdef HAVE_LIBAUDIT
	if (testing != TRUE) {
		if (adjust == TRUE || hctosys == TRUE || systohc == TRUE ||
		    set == TRUE || setepoch == TRUE) {
			hwaudit_on = TRUE;
		}
	}
#endif
	if (argc > 0) {
		usage(_("%s takes no non-option arguments.  "
			"You supplied %d.\n"), program_invocation_short_name,
		      argc);
	}

	if (show + set + systohc + hctosys + systz + adjust + getepoch
	    + setepoch + predict > 1) {
		warnx(_("You have specified multiple functions.\n"
			"You can only perform one function at a time."));
		hwclock_exit(EX_USAGE);
	}

	if (utc && local_opt) {
		warnx(_("The --utc and --localtime options "
			"are mutually exclusive.  You specified both."));
		hwclock_exit(EX_USAGE);
	}

	if (adjust && noadjfile) {
		warnx(_("The --adjust and --noadjfile options "
			"are mutually exclusive.  You specified both."));
		hwclock_exit(EX_USAGE);
	}

	if (adj_file_name && noadjfile) {
		warnx(_("The --adjfile and --noadjfile options "
			"are mutually exclusive.  You specified both."));
		hwclock_exit(EX_USAGE);
	}
	if (!adj_file_name)
		adj_file_name = _PATH_ADJPATH;

	if (noadjfile && !(utc || local_opt)) {
		warnx(_("With --noadjfile, you must specify "
			"either --utc or --localtime"));
		hwclock_exit(EX_USAGE);
	}
#ifdef __alpha__
	set_cmos_epoch(ARCconsole, SRM);
	set_cmos_access(Jensen, funky_toy);
#endif

	if (set || predict) {
		rc = interpret_date_string(date_opt, &set_time);
		/* (time-consuming) */
		if (rc != 0) {
			warnx(_("No usable set-to time.  "
				"Cannot set clock."));
			hwclock_exit(EX_USAGE);
		}
	}

	if (!(show | set | systohc | hctosys | systz | adjust | getepoch
	      | setepoch | predict))
		show = 1;	/* default to show */

	if (getuid() == 0)
		permitted = TRUE;
	else {
		/* program is designed to run setuid (in some situations) */
		if (set || systohc || adjust) {
			warnx(_("Sorry, only the superuser can change "
				"the Hardware Clock."));
			permitted = FALSE;
		} else if (systz || hctosys) {
			warnx(_("Sorry, only the superuser can change "
				"the System Clock."));
			permitted = FALSE;
		} else if (setepoch) {
			warnx(_("Sorry, only the superuser can change the "
				"Hardware Clock epoch in the kernel."));
			permitted = FALSE;
		} else
			permitted = TRUE;
	}

	if (!permitted)
		hwclock_exit(EX_NOPERM);

#ifdef __linux__
	if (getepoch || setepoch) {
		manipulate_epoch(getepoch, setepoch, epoch_option, testing);
		hwclock_exit(EX_OK);
	}
#endif

	if (debug)
		out_version();

	if (!systz && !predict) {
		determine_clock_access_method(directisa);
		if (!ur) {
			warnx(_("Cannot access the Hardware Clock via "
				"any known method."));
			if (!debug)
				warnx(_("Use the --debug option to see the "
					"details of our search for an access "
					"method."));
			hwclock_exit(EX_SOFTWARE);
		}
	}

	rc = manipulate_clock(show, adjust, noadjfile, set, set_time,
			      hctosys, systohc, systz, startup_time, utc,
			      local_opt, testing, predict);
	hwclock_exit(rc);
	return rc;		/* Not reached */
}

#ifdef HAVE_LIBAUDIT
/*
 * hwclock_exit calls either this function or plain exit depending
 * HAVE_LIBAUDIT see also clock.h
 */
void __attribute__((__noreturn__)) hwaudit_exit(int status)
{
	if (hwaudit_on) {
		audit_log_user_message(hwaudit_fd, AUDIT_USYS_CONFIG,
				       "changing system time", NULL, NULL, NULL,
				       status ? 0 : 1);
		close(hwaudit_fd);
	}
	exit(status);
}
#endif

/*
 * History of this program:
 *
 * 98.08.12 BJH Version 2.4
 *
 * Don't use century byte from Hardware Clock. Add comments telling why.
 *
 * 98.06.20 BJH Version 2.3.
 *
 * Make --hctosys set the kernel timezone from TZ environment variable
 * and/or /usr/lib/zoneinfo. From Klaus Ripke (klaus@ripke.com).
 *
 * 98.03.05 BJH. Version 2.2.
 *
 * Add --getepoch and --setepoch.
 *
 * Fix some word length things so it works on Alpha.
 *
 * Make it work when /dev/rtc doesn't have the interrupt functions. In this
 * case, busywait for the top of a second instead of blocking and waiting
 * for the update complete interrupt.
 *
 * Fix a bunch of bugs too numerous to mention.
 *
 * 97.06.01: BJH. Version 2.1. Read and write the century byte (Byte 50) of
 * the ISA Hardware Clock when using direct ISA I/O. Problem discovered by
 * job (jei@iclnl.icl.nl).
 *
 * Use the rtc clock access method in preference to the KDGHWCLK method.
 * Problem discovered by Andreas Schwab <schwab@LS5.informatik.uni-dortmund.de>.
 *
 * November 1996: Version 2.0.1. Modifications by Nicolai Langfeldt
 * (janl@math.uio.no) to make it compile on linux 1.2 machines as well as
 * more recent versions of the kernel. Introduced the NO_CLOCK access method
 * and wrote feature test code to detect absense of rtc headers.
 *
 ***************************************************************************
 * Maintenance notes
 *
 * To compile this, you must use GNU compiler optimization (-O option) in
 * order to make the "extern inline" functions from asm/io.h (inb(), etc.)
 * compile. If you don't optimize, which means the compiler will generate no
 * inline functions, the references to these functions in this program will
 * be compiled as external references. Since you probably won't be linking
 * with any functions by these names, you will have unresolved external
 * references when you link.
 *
 * The program is designed to run setuid superuser, since we need to be able
 * to do direct I/O. (More to the point: we need permission to execute the
 * iopl() system call). (However, if you use one of the methods other than
 * direct ISA I/O to access the clock, no setuid is required).
 *
 * Here's some info on how we must deal with the time that elapses while
 * this program runs: There are two major delays as we run:
 *
 *	1) Waiting up to 1 second for a transition of the Hardware Clock so
 *	   we are synchronized to the Hardware Clock.
 *	2) Running the "date" program to interpret the value of our --date
 *	   option.
 *
 * Reading the /etc/adjtime file is the next biggest source of delay and
 * uncertainty.
 *
 * The user wants to know what time it was at the moment he invoked us, not
 * some arbitrary time later. And in setting the clock, he is giving us the
 * time at the moment we are invoked, so if we set the clock some time
 * later, we have to add some time to that.
 *
 * So we check the system time as soon as we start up, then run "date" and
 * do file I/O if necessary, then wait to synchronize with a Hardware Clock
 * edge, then check the system time again to see how much time we spent. We
 * immediately read the clock then and (if appropriate) report that time,
 * and additionally, the delay we measured.
 *
 * If we're setting the clock to a time given by the user, we wait some more
 * so that the total delay is an integral number of seconds, then set the
 * Hardware Clock to the time the user requested plus that integral number
 * of seconds. N.B. The Hardware Clock can only be set in integral seconds.
 *
 * If we're setting the clock to the system clock value, we wait for the
 * system clock to reach the top of a second, and then set the Hardware
 * Clock to the system clock's value.
 *
 * Here's an interesting point about setting the Hardware Clock:  On my
 * machine, when you set it, it sets to that precise time. But one can
 * imagine another clock whose update oscillator marches on a steady one
 * second period, so updating the clock between any two oscillator ticks is
 * the same as updating it right at the earlier tick. To avoid any
 * complications that might cause, we set the clock as soon as possible
 * after an oscillator tick.
 *
 * About synchronizing to the Hardware Clock when reading the time: The
 * precision of the Hardware Clock counters themselves is one second. You
 * can't read the counters and find out that is 12:01:02.5. But if you
 * consider the location in time of the counter's ticks as part of its
 * value, then its precision is as infinite as time is continuous! What I'm
 * saying is this: To find out the _exact_ time in the hardware clock, we
 * wait until the next clock tick (the next time the second counter changes)
 * and measure how long we had to wait. We then read the value of the clock
 * counters and subtract the wait time and we know precisely what time it
 * was when we set out to query the time.
 *
 * hwclock uses this method, and considers the Hardware Clock to have
 * infinite precision.
 *
 * TODO: Enhancements needed:
 *
 *  - When waiting for whole second boundary in set_hardware_clock_exact,
 *    fail if we miss the goal by more than .1 second, as could happen if we
 *    get pre-empted (by the kernel dispatcher).
 */
