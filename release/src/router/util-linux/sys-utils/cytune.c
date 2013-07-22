/* cytune.c -- Tune Cyclades driver
 *
 * Copyright 1995 Nick Simicich (njs@scifi.emi.net)
 *
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
 *
 */

 /*
  * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
  * - added Native Language Support
  * Sun Mar 21 1999 - Arnaldo Carvalho de Melo <acme@conectiva.com.br>
  * - fixed strerr(errno) in gettext calls
  */

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

#include "cyclades.h"

#if 0
#ifndef XMIT
# include <linux/version.h>
# if LINUX_VERSION_CODE > 66056
#  define XMIT
# endif
#endif
#endif

#include "xalloc.h"
#include "nls.h"
				/* Until it gets put in the kernel,
				   toggle by hand. */
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

#define mvtime(tvpto, tvpfrom)  (((tvpto)->tv_sec = (tvpfrom)->tv_sec),(tvpto)->tv_usec = (tvpfrom)->tv_usec)


static inline double
dtime(struct timeval * tvpnew, struct timeval * tvpold) {
  double diff;
  diff = (double)tvpnew->tv_sec - (double)tvpold->tv_sec;
  diff += ((double)tvpnew->tv_usec - (double)tvpold->tv_usec)/1000000;
  return diff;
}

static int global_argc, global_optind;
static char ***global_argv;

static void
summary(int sig) {
  struct cyclades_control *cc;
  int argc, local_optind;
  char **argv;
  int i;

  argc = global_argc;
  argv = *global_argv;
  local_optind = global_optind;

  if (sig > 0) {
    for(i = local_optind; i < argc; i ++) {
      cc = &cmon[cmon_index];
      fprintf(stderr, _("File %s, For threshold value %lu, Maximum characters in fifo were %d,\nand the maximum transfer rate in characters/second was %f\n"), 
	      argv[i],
	      cc->threshold_value,
	      cc->maxmax,
	      cc->maxtran);
    }
    
    exit(0);
  }
  cc = &cmon[cmon_index];
  if (cc->threshold_value > 0 && sig != -1) {
    fprintf(stderr, _("File %s, For threshold value %lu and timrout value %lu, Maximum characters in fifo were %d,\nand the maximum transfer rate in characters/second was %f\n"),
	    argv[cmon_index+local_optind],
	    cc->threshold_value,
	    cc->timeout_value,
	    cc->maxmax,
	    cc->maxtran);
  }
  cc->maxmax = 0;
  cc->maxtran = 0.0;
  cc->threshold_value = 0;
  cc->timeout_value = 0;
}

static int query   = 0;
static int interval = 1;

static int set     = 0;
static int set_val = -1;
static int get     = 0;

static int set_def     = 0;
static int set_def_val = -1;
static int get_def     = 0;

static int set_time = 0;
static int set_time_val = -1;

static int set_def_time = 0;
static int set_def_time_val = -1;


int main(int argc, char *argv[]) {

  struct timeval lasttime, thistime;
  struct timezone tz = {0,0};
  double diff;
  int errflg = 0;
  int file;
  int numfiles;
  struct cyclades_monitor cywork;
  
  int i;
  unsigned long threshold_value;
  unsigned long timeout_value;
  double xfer_rate;
#ifdef XMIT
  double xmit_rate;
#endif
  
  global_argc = argc;		/* For signal routine. */
  global_argv = &argv;		/* For signal routine. */

  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);

  while ((i = getopt(argc, argv, "qs:S:t:T:gGi:")) != -1) {
    switch (i) {
    case 'q':
      query = 1;
      break;
    case 'i':
      interval = atoi(optarg);
      if(interval <= 0) {
	fprintf(stderr, _("Invalid interval value: %s\n"),optarg);
	errflg ++;
      }
      break;
    case 's':
      ++set;
      set_val = atoi(optarg);
      if(set_val <= 0 || set_val > 12) {
	fprintf(stderr, _("Invalid set value: %s\n"),optarg);
	errflg ++;
      }
      break;
    case 'S':
       ++set_def;
      set_def_val = atoi(optarg);
      if(set_def_val < 0 || set_def_val > 12) {
	fprintf(stderr, _("Invalid default value: %s\n"),optarg);
	errflg ++;
      }
      break;
    case 't':
      ++set_time;
      set_time_val = atoi(optarg);
      if(set_time_val <= 0 || set_time_val > 255) {
	fprintf(stderr, _("Invalid set time value: %s\n"),optarg);
	errflg ++;
      }
      break;
    case 'T':
       ++set_def_time;
      set_def_time_val = atoi(optarg);
      if(set_def_time_val < 0 || set_def_time_val > 255) {
	fprintf(stderr, _("Invalid default time value: %s\n"),optarg);
	errflg ++;
      }
      break;
    case 'g': ++get;     break;
    case 'G': ++get_def; break;
    default:
      errflg ++;
    }
  }
  numfiles = argc - optind;
  if(errflg || (numfiles == 0)
     || (!query && !set && !set_def && 
	 !get && !get_def && !set_time && !set_def_time) || 
     (set && set_def) || (set_time && set_def_time) || 
     (get && get_def)) {
    fprintf(stderr, 
	    _("Usage: %s [-q [-i interval]] ([-s value]|[-S value]) ([-t value]|[-T value]) [-g|-G] file [file...]\n"),
	    argv[0]);
    exit(1);
  }

  global_optind = optind;	/* For signal routine. */

  if (set || set_def) {
    for(i = optind; i < argc; i++) {
      file = open(argv[i],O_RDONLY);
      if(file == -1) {
        int errsv = errno;
	fprintf(stderr, _("Can't open %s: %s\n"),argv[i],strerror(errsv));
	exit(1);
      }
      if(ioctl(file,
	       set ? CYSETTHRESH : CYSETDEFTHRESH,
	       set ? set_val : set_def_val)) {
	int errsv = errno;
	fprintf(stderr, _("Can't set %s to threshold %d: %s\n"),
		argv[i],set?set_val:set_def_val,strerror(errsv));
	exit(1);
      }
      close(file);
    }
  }
  if (set_time || set_def_time) {
    for(i = optind; i < argc; i++) {
      file = open(argv[i],O_RDONLY);
      if(file == -1) {
        int errsv = errno;
	fprintf(stderr, _("Can't open %s: %s\n"),argv[i],strerror(errsv));
	exit(1);
      }
      if(ioctl(file,
	       set_time ? CYSETTIMEOUT : CYSETDEFTIMEOUT,
	       set_time ? set_time_val : set_def_time_val)) {
	int errsv = errno;
	fprintf(stderr, _("Can't set %s to time threshold %d: %s\n"),
		argv[i],set_time?set_time_val:set_def_time_val,strerror(errsv));
	exit(1);
      }
      close(file);
    }
  }

  if (get || get_def) {
    for(i = optind; i < argc; i++) {
      file = open(argv[i],O_RDONLY);
      if(file == -1) {
        int errsv = errno;
	fprintf(stderr, _("Can't open %s: %s\n"),argv[i],strerror(errsv));
	exit(1);
      }
      if(ioctl(file, get ? CYGETTHRESH : CYGETDEFTHRESH, &threshold_value)) {
        int errsv = errno;
	fprintf(stderr, _("Can't get threshold for %s: %s\n"),
		argv[i],strerror(errsv));
	exit(1);
      }
      if(ioctl(file, get ? CYGETTIMEOUT : CYGETDEFTIMEOUT, &timeout_value)) {
      	int errsv = errno;
	fprintf(stderr, _("Can't get timeout for %s: %s\n"),
		argv[i],strerror(errsv));
	exit(1);
      }
      close(file);
      if (get)
	      printf(_("%s: %ld current threshold and %ld current timeout\n"),
		     argv[i], threshold_value, timeout_value);
      else
	      printf(_("%s: %ld default threshold and %ld default timeout\n"),
		     argv[i], threshold_value, timeout_value);
    }
  }

  if(!query) return 0;		/* must have been something earlier */

  /* query stuff after this line */
  
  cmon = xmalloc(sizeof(struct cyclades_control) * numfiles);

  if(signal(SIGINT, summary)||
     signal(SIGQUIT, summary)||
     signal(SIGTERM, summary)) {
    perror(_("Can't set signal handler"));
    exit(1);
  }
  if(gettimeofday(&lasttime,&tz)) {
    perror(_("gettimeofday failed"));
    exit(1);
  }
  for(i = optind; i < argc; i ++) {
    cmon_index = i - optind;
    cmon[cmon_index].cfile = open(argv[i], O_RDONLY);
    if(-1 == cmon[cmon_index].cfile) {
      int errsv = errno;
      fprintf(stderr, _("Can't open %s: %s\n"),argv[i],strerror(errsv));
      exit(1);
    }
    if(ioctl(cmon[cmon_index].cfile, CYGETMON, &cmon[cmon_index].c)) {
      int errsv = errno;
      fprintf(stderr, _("Can't issue CYGETMON on %s: %s\n"),
	      argv[i],strerror(errsv));
      exit(1);
    }
    summary(-1);
    if(ioctl(cmon[cmon_index].cfile, CYGETTHRESH, &threshold_value)) {
      int errsv = errno;
      fprintf(stderr, _("Can't get threshold for %s: %s\n"),
	      argv[i],strerror(errsv));
      exit(1);
    }
    if(ioctl(cmon[cmon_index].cfile, CYGETTIMEOUT, &timeout_value)) {
      int errsv = errno;
      fprintf(stderr, _("Can't get timeout for %s: %s\n"),
	      argv[i],strerror(errsv));
      exit(1);
    }
  }
  while(1) {
    sleep(interval);
    
    if(gettimeofday(&thistime,&tz)) {
      perror(_("gettimeofday failed"));
      exit(1);
    }
    diff = dtime(&thistime, &lasttime);
    mvtime(&lasttime, &thistime);

    for(i = optind; i < argc; i ++) {
      cmon_index = i - optind;
      if(ioctl(cmon[cmon_index].cfile, CYGETMON, &cywork)) {
        int errsv = errno;
	fprintf(stderr, _("Can't issue CYGETMON on %s: %s\n"),
		argv[i],strerror(errsv));
	exit(1);
      }
      if(ioctl(cmon[cmon_index].cfile, CYGETTHRESH, &threshold_value)) {
        int errsv = errno;
	fprintf(stderr, _("Can't get threshold for %s: %s\n"),
		argv[i],strerror(errsv));
	exit(1);
      }
      if(ioctl(cmon[cmon_index].cfile, CYGETTIMEOUT, &timeout_value)) {
        int errsv = errno;
	fprintf(stderr, _("Can't get timeout for %s: %s\n"),
		argv[i],strerror(errsv));
	exit(1);
      }

      xfer_rate = cywork.char_count/diff;
#ifdef XMIT
      xmit_rate = cywork.send_count/diff;
#endif

      if(threshold_value != cmon[cmon_index].threshold_value ||
	 timeout_value != cmon[cmon_index].timeout_value) {
	summary(-2);
	/* Note that the summary must come before the setting of */
	/* threshold_value */
	cmon[cmon_index].threshold_value = threshold_value;	 
	cmon[cmon_index].timeout_value = timeout_value;	 
      } else {
	/* Don't record this first cycle after change */
	if(xfer_rate > cmon[cmon_index].maxtran) 
	  cmon[cmon_index].maxtran = xfer_rate;
#ifdef XMIT
	if(xmit_rate > cmon[cmon_index].maxxmit)
          cmon[cmon_index].maxxmit = xmit_rate;
#endif
	if(cmon[cmon_index].maxmax < 0 ||
	   cywork.char_max > (unsigned long) cmon[cmon_index].maxmax)
	  cmon[cmon_index].maxmax = cywork.char_max;
      }

#ifdef XMIT
      printf(_("%s: %lu ints, %lu/%lu chars; fifo: %lu thresh, %lu tmout, "
	       "%lu max, %lu now\n"),
	     argv[i],
	     cywork.int_count,cywork.char_count,cywork.send_count,
	     threshold_value,timeout_value,
	     cywork.char_max,cywork.char_last);
      printf(_("   %f int/sec; %f rec, %f send (char/sec)\n"),
	     cywork.int_count/diff,
	     xfer_rate,
	     xmit_rate);
#else
      printf(_("%s: %lu ints, %lu chars; fifo: %lu thresh, %lu tmout, "
	       "%lu max, %lu now\n"),
	     argv[i],
	     cywork.int_count,cywork.char_count,
	     threshold_value,timeout_value,
	     cywork.char_max,cywork.char_last);
      printf(_("   %f int/sec; %f rec (char/sec)\n"),
	     cywork.int_count/diff,
	     xfer_rate);
#endif
      memcpy(&cmon[cmon_index].c, &cywork, sizeof (struct cyclades_monitor));
    }
  }

  return 0;
}
