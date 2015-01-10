/* cytune.c -- Tune Cyclades driver
 *
 * Copyright 1995 Nick Simicich (njs@scifi.emi.net)
 * Modifications by Rik Faith (faith@cs.unc.edu)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Nick Simicich
 * 4. Neither the name of the Nick Simicich nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY NICK SIMICICH AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NICK SIMICICH OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
 /*
  * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
  * - added Native Language Support
  * Sun Mar 21 1999 - Arnaldo Carvalho de Melo <acme@conectiva.com.br>
  * - fixed strerr(errno) in gettext calls
  */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>

#include "c.h"
#include "cyclades.h"
#include "strutils.h"

#if 0
# ifndef XMIT
#  include <linux/version.h>
#  if LINUX_VERSION_CODE > 66056
#   define XMIT
#  endif
# endif
#endif

#include "xalloc.h"
#include "nls.h"
/* Until it gets put in the kernel, toggle by hand. */
#undef XMIT

struct cyclades_control {
	struct cyclades_monitor c;
	int cfile;
	int maxmax;
	double maxtran;
	double maxxmit;
	unsigned long threshold_value;
	unsigned long timeout_value;
};
struct cyclades_control *cmon;
int cmon_index;

static int global_argc, global_optind;
static char ***global_argv;

#define mvtime(tvpto, tvpfrom)  (((tvpto)->tv_sec = (tvpfrom)->tv_sec),(tvpto)->tv_usec = (tvpfrom)->tv_usec)

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fprintf(out, USAGE_HEADER);
	fprintf(out, _(" %s [options] <tty> [...]\n"), program_invocation_short_name);
	fprintf(out, USAGE_OPTIONS);
	fprintf(out, _(" -s, --set-threshold <num>          set interruption threshold value\n"));
	fprintf(out, _(" -g, --get-threshold                display current threshold value\n"));
	fprintf(out, _(" -S, --set-default-threshold <num>  set default threshold value\n"));
	fprintf(out, _(" -t, --set-flush <num>              set flush timeout to value\n"));
	fprintf(out, _(" -G, --get-glush                    display default flush timeout value\n"));
	fprintf(out, _(" -T, --set-default-flush <num>      set the default flush timeout to value\n"));
	fprintf(out, _(" -q, --stats                        display statistics about the tty\n"));
	fprintf(out, _(" -i, --interval <seconds>           gather statistics every <seconds> interval\n"));
	fprintf(out, USAGE_SEPARATOR);
	fprintf(out, USAGE_HELP);
	fprintf(out, USAGE_VERSION);
	fprintf(out, USAGE_MAN_TAIL("cytune(8)"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static inline double dtime(struct timeval *tvpnew, struct timeval *tvpold)
{
	double diff;
	diff = (double)tvpnew->tv_sec - (double)tvpold->tv_sec;
	diff += ((double)tvpnew->tv_usec - (double)tvpold->tv_usec) / 1000000;
	return diff;
}

static void summary(int sig)
{
	struct cyclades_control *cc;
	int argc, local_optind;
	char **argv;
	int i;

	argc = global_argc;
	argv = *global_argv;
	local_optind = global_optind;

	if (sig > 0) {
		for (i = local_optind; i < argc; i++) {
			cc = &cmon[cmon_index];
			warnx(_("File %s, For threshold value %lu, Maximum characters in fifo were %d,\n"
				"and the maximum transfer rate in characters/second was %f"),
			      argv[i], cc->threshold_value, cc->maxmax,
			      cc->maxtran);
		}
		exit(EXIT_SUCCESS);
	}
	cc = &cmon[cmon_index];
	if (cc->threshold_value > 0 && sig != -1) {
		warnx(_("File %s, For threshold value %lu and timrout value %lu, Maximum characters in fifo were %d,\n"
			"and the maximum transfer rate in characters/second was %f"),
		      argv[cmon_index + local_optind], cc->threshold_value,
		      cc->timeout_value, cc->maxmax, cc->maxtran);
	}
	cc->maxmax = 0;
	cc->maxtran = 0.0;
	cc->threshold_value = 0;
	cc->timeout_value = 0;
}

void query_tty_stats(int argc, char **argv, int interval, int numfiles,
		     unsigned long *threshold_value,
		     unsigned long *timeout_value)
{
	struct cyclades_monitor cywork;
	struct timeval lasttime, thistime;
	struct timezone tz = { 0, 0 };
	int i;
	double diff;
	double xfer_rate;
#ifdef XMIT
	double xmit_rate;
#endif

	cmon = xmalloc(sizeof(struct cyclades_control) * numfiles);

	if (signal(SIGINT, summary) ||
	    signal(SIGQUIT, summary) || signal(SIGTERM, summary))
		err(EXIT_FAILURE, _("cannot set signal handler"));
	if (gettimeofday(&lasttime, &tz))
		err(EXIT_FAILURE, _("gettimeofday failed"));

	for (i = optind; i < argc; i++) {
		cmon_index = i - optind;
		cmon[cmon_index].cfile = open(argv[i], O_RDONLY);
		if (cmon[cmon_index].cfile == -1)
			err(EXIT_FAILURE, _("cannot open %s"), argv[i]);
		if (ioctl
		    (cmon[cmon_index].cfile, CYGETMON, &cmon[cmon_index].c))
			err(EXIT_FAILURE, _("cannot issue CYGETMON on %s"),
			    argv[i]);
		summary(-1);
		if (ioctl
		    (cmon[cmon_index].cfile, CYGETTHRESH, &threshold_value))
			err(EXIT_FAILURE, _("cannot get threshold for %s"),
			    argv[i]);
		if (ioctl(cmon[cmon_index].cfile, CYGETTIMEOUT, &timeout_value))
			err(EXIT_FAILURE, _("cannot get timeout for %s"),
			    argv[i]);
	}
	while (1) {
		sleep(interval);

		if (gettimeofday(&thistime, &tz))
			err(EXIT_FAILURE, _("gettimeofday failed"));
		diff = dtime(&thistime, &lasttime);
		mvtime(&lasttime, &thistime);

		for (i = optind; i < argc; i++) {
			cmon_index = i - optind;
			if (ioctl(cmon[cmon_index].cfile, CYGETMON, &cywork))
				err(EXIT_FAILURE,
				    _("cannot issue CYGETMON on %s"), argv[i]);
			if (ioctl
			    (cmon[cmon_index].cfile, CYGETTHRESH,
			     &threshold_value))
				err(EXIT_FAILURE,
				    _("cannot get threshold for %s"), argv[i]);
			if (ioctl
			    (cmon[cmon_index].cfile, CYGETTIMEOUT,
			     &timeout_value))
				err(EXIT_FAILURE,
				    _("cannot get timeout for %s"), argv[i]);

			xfer_rate = cywork.char_count / diff;
#ifdef XMIT
			xmit_rate = cywork.send_count / diff;
#endif
			if ((*threshold_value) !=
			    cmon[cmon_index].threshold_value
			    || (*timeout_value) !=
			    cmon[cmon_index].timeout_value) {
				summary(-2);
				/* Note that the summary must come before the
				 * setting of threshold_value */
				cmon[cmon_index].threshold_value =
				    (*threshold_value);
				cmon[cmon_index].timeout_value =
				    (*timeout_value);
			} else {
				/* Don't record this first cycle after change */
				if (xfer_rate > cmon[cmon_index].maxtran)
					cmon[cmon_index].maxtran = xfer_rate;
#ifdef XMIT
				if (xmit_rate > cmon[cmon_index].maxxmit)
					cmon[cmon_index].maxxmit = xmit_rate;
#endif
				if (cmon[cmon_index].maxmax < 0 ||
				    cywork.char_max >
				    (unsigned long)cmon[cmon_index].maxmax)
					cmon[cmon_index].maxmax =
					    cywork.char_max;
			}

#ifdef XMIT
			printf(_("%s: %lu ints, %lu/%lu chars; fifo: %lu thresh, %lu tmout, "
				 "%lu max, %lu now\n"), argv[i],
			       cywork.int_count, cywork.char_count,
			       cywork.send_count, *threshold_value,
			       *timeout_value, cywork.char_max,
			       cywork.char_last);
			printf(_("   %f int/sec; %f rec, %f send (char/sec)\n"),
			       cywork.int_count / diff, xfer_rate, xmit_rate);
#else
			printf(_("%s: %lu ints, %lu chars; fifo: %lu thresh, %lu tmout, "
				 "%lu max, %lu now\n"), argv[i],
			       cywork.int_count, cywork.char_count,
			       *threshold_value, *timeout_value, cywork.char_max,
			       cywork.char_last);
			printf(_("   %f int/sec; %f rec (char/sec)\n"),
			       cywork.int_count / diff, xfer_rate);
#endif
			memcpy(&cmon[cmon_index].c, &cywork,
			       sizeof(struct cyclades_monitor));
		}
	}

	free(cmon);
	return;
}

int main(int argc, char **argv)
{
	int query = 0;
	int interval = 1;
	int set = 0;
	int set_val = -1;
	int get = 0;
	int set_def = 0;
	int set_def_val = -1;
	int get_def = 0;
	int set_time = 0;
	int set_time_val = -1;
	int set_def_time = 0;
	int set_def_time_val = -1;
	int errflg = 0;
	int file;
	int numfiles;
	int i;
	unsigned long threshold_value;
	unsigned long timeout_value;

	static const struct option longopts[] = {
		{"set-threshold", required_argument, NULL, 's'},
		{"get-threshold", no_argument, NULL, 'g'},
		{"set-default-threshold", required_argument, NULL, 'S'},
		{"set-flush", required_argument, NULL, 't'},
		{"get-flush", no_argument, NULL, 'G'},
		{"set-default-flush", required_argument, NULL, 'T'},
		{"stats", no_argument, NULL, 'q'},
		{"interval", required_argument, NULL, 'i'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	/* For signal routine. */
	global_argc = argc;
	global_argv = &argv;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((i =
		getopt_long(argc, argv, "qs:S:t:T:gGi:Vh", longopts,
			    NULL)) != -1) {
		switch (i) {
		case 'q':
			query = 1;
			break;
		case 'i':
			interval =
			    (int)strtoul_or_err(optarg,
						_("Invalid interval value"));
			if (interval < 1) {
				warnx(_("Invalid interval value: %d"),
				      interval);
				errflg++;
			}
			break;
		case 's':
			++set;
			set_val =
			    (int)strtoul_or_err(optarg, _("Invalid set value"));
			if (set_val < 1 || 12 < set_val) {
				warnx(_("Invalid set value: %d"), set_val);
				errflg++;
			}
			break;
		case 'S':
			++set_def;
			set_def_val =
			    (int)strtoul_or_err(optarg,
						_("Invalid default value"));
			if (set_def_val < 0 || 12 < set_def_val) {
				warnx(_("Invalid default value: %d"),
				      set_def_val);
				errflg++;
			}
			break;
		case 't':
			++set_time;
			set_time_val =
			    (int)strtoul_or_err(optarg,
						_("Invalid set time value"));
			if (set_time_val < 1 || 255 < set_time_val) {
				warnx(_("Invalid set time value: %d"),
				      set_time_val);
				errflg++;
			}
			break;
		case 'T':
			++set_def_time;
			set_def_time_val =
			    (int)strtoul_or_err(optarg,
						_("Invalid default time value"));
			if (set_def_time_val < 0 || 255 < set_def_time_val) {
				warnx(_("Invalid default time value: %d"),
				      set_def_time_val);
				errflg++;
			}
			break;
		case 'g':
			++get;
			break;
		case 'G':
			++get_def;
			break;
		case 'V':
			printf(_("%s from %s\n"), program_invocation_short_name,
			       PACKAGE_STRING);
			return EXIT_SUCCESS;
		case 'h':
			usage(stdout);
		default:
			usage(stderr);
		}
	}
	numfiles = argc - optind;

	if (errflg
	    || (numfiles == 0)
	    || (!query && !set && !set_def && !get && !get_def && !set_time && !set_def_time)
	    || (set && set_def)
	    || (set_time && set_def_time)
	    || (get && get_def))
		usage(stderr);

	/* For signal routine. */
	global_optind = optind;

	if (set || set_def) {
		for (i = optind; i < argc; i++) {
			file = open(argv[i], O_RDONLY);
			if (file == -1)
				err(EXIT_FAILURE, _("cannot open %s"), argv[i]);
			if (ioctl(file,
				  set ? CYSETTHRESH : CYSETDEFTHRESH,
				  set ? set_val : set_def_val))
				err(EXIT_FAILURE,
				    _("cannot set %s to threshold %d"), argv[i],
				    set ? set_val : set_def_val);
			close(file);
		}
	}
	if (set_time || set_def_time) {
		for (i = optind; i < argc; i++) {
			file = open(argv[i], O_RDONLY);
			if (file == -1)
				err(EXIT_FAILURE, _("cannot open %s"), argv[i]);
			if (ioctl(file,
				  set_time ? CYSETTIMEOUT : CYSETDEFTIMEOUT,
				  set_time ? set_time_val : set_def_time_val))
				err(EXIT_FAILURE,
				    _("cannot set %s to time threshold %d"),
				    argv[i],
				    set_time ? set_time_val : set_def_time_val);
			close(file);
		}
	}

	if (get || get_def) {
		for (i = optind; i < argc; i++) {
			file = open(argv[i], O_RDONLY);
			if (file == -1)
				err(EXIT_FAILURE, _("cannot open %s"), argv[i]);
			if (ioctl
			    (file, get ? CYGETTHRESH : CYGETDEFTHRESH,
			     &threshold_value))
				err(EXIT_FAILURE,
				    _("cannot get threshold for %s"), argv[i]);
			if (ioctl
			    (file, get ? CYGETTIMEOUT : CYGETDEFTIMEOUT,
			     &timeout_value))
				err(EXIT_FAILURE,
				    _("cannot get timeout for %s"), argv[i]);
			close(file);
			if (get)
				printf(_("%s: %ld current threshold and %ld current timeout\n"),
					argv[i], threshold_value, timeout_value);
			else
				printf(_("%s: %ld default threshold and %ld default timeout\n"),
					argv[i], threshold_value, timeout_value);
		}
	}

	if (!query)
		return EXIT_SUCCESS;

	query_tty_stats(argc, argv, interval, numfiles, &threshold_value, &timeout_value);

	return EXIT_SUCCESS;
}
