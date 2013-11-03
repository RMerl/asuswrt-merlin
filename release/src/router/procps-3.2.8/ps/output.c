/*
 * Copyright 1999-2004 by Albert Cahalan; all rights reserved.
 *
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Library General Public License for more details.
 */

/*
 * This file is really gross, and I know it. I looked into several
 * alternate ways to deal with the mess, and they were all ugly.
 *
 * FreeBSD has a fancy hack using offsets into a struct -- that
 * saves code but it is _really_ gross. See the PO macro below.
 *
 * We could have a second column width for wide output format.
 * For example, Digital prints the real-time signals.
 */


/*
 * Data table idea:
 *
 * table 1 maps aix to specifier
 * table 2 maps shortsort to specifier
 * table 3 maps macro to specifiers
 * table 4 maps specifier to title,datatype,offset,vendor,helptext
 * table 5 maps datatype to justification,width,widewidth,sorting,printing
 *
 * Here, "datatype" could be user,uid,u16,pages,deltaT,signals,tty,longtty...
 * It must be enough to determine printing and sorting.
 *
 * After the tables, increase width as needed to fit the header.
 *
 * Table 5 could go in a file with the output functions.
 */
 
#include <ctype.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <dlfcn.h>

#include "../proc/readproc.h"
#include "../proc/sysinfo.h"
#include "../proc/wchan.h"
#include "../proc/procps.h"
#include "../proc/devname.h"
#include "../proc/escape.h"
#include "common.h"

/* TODO:
 * Stop assuming system time is local time.
 */

#define COLWID 240 /* satisfy snprintf, which is faster than sprintf */

static unsigned max_rightward = 0x12345678; /* space for RIGHT stuff */
static unsigned max_leftward = 0x12345678; /* space for LEFT stuff */



static int wide_signals;  /* true if we have room */

static unsigned long seconds_since_1970;
static unsigned long time_of_boot;
static unsigned long page_shift;


/*************************************************************************/
/************ Lots of sort functions, starting with the NOP **************/

static int sr_nop(const proc_t* a, const proc_t* b){
  (void)a;(void)b; /* shut up gcc */
  return 0;
}

#define CMP_STR(NAME) \
static int sr_ ## NAME(const proc_t* P, const proc_t* Q) { \
    return strcmp(P->NAME, Q->NAME); \
}

#define CMP_INT(NAME) \
static int sr_ ## NAME (const proc_t* P, const proc_t* Q) { \
    if (P->NAME < Q->NAME) return -1; \
    if (P->NAME > Q->NAME) return  1; \
    return 0; \
}

/* fast version, for values which either:
 * a. differ by no more than 0x7fffffff
 * b. only need to be grouped same w/ same
 */
#define CMP_SMALL(NAME) \
static int sr_ ## NAME (const proc_t* P, const proc_t* Q) { \
    return (int)(P->NAME) - (int)(Q->NAME); \
}

CMP_INT(rtprio)
CMP_SMALL(sched)
CMP_INT(cutime)
CMP_INT(cstime)
CMP_SMALL(priority)                                             /* nice */
CMP_SMALL(nlwp)
CMP_SMALL(nice)                                                 /* priority */
CMP_INT(rss)      /* resident set size from stat file */ /* vm_rss, resident */
CMP_INT(alarm)
CMP_INT(size)      /* total pages */                     /* vm_size, vsize */
CMP_INT(resident)  /* resident pages */                     /* vm_rss, rss */
CMP_INT(share)     /* shared pages */
CMP_INT(trs)       /* executable pages */
CMP_INT(lrs)       /* obsolete "library" pages above 0x60000000 */
CMP_INT(drs)       /* other pages (assumed data?) */
CMP_INT(dt)        /* dirty pages */

CMP_INT(vm_size)    /* kB VM */                             /* size, vsize */
CMP_INT(vm_lock)    /* kB locked */
CMP_INT(vm_rss)     /* kB rss */                          /* rss, resident */
CMP_INT(vm_data)    /* kB "data" == data-stack */
CMP_INT(vm_stack)   /* kB stack */
CMP_INT(vm_exe)     /* kB "exec" == exec-lib */
CMP_INT(vm_lib)     /* kB "libraries" */
CMP_INT(vsize)      /* pages VM */                        /* size, vm_size */
CMP_INT(rss_rlim)
CMP_SMALL(flags)
CMP_INT(min_flt)
CMP_INT(maj_flt)
CMP_INT(cmin_flt)
CMP_INT(cmaj_flt)
CMP_INT(utime)
CMP_INT(stime)    /* Old: sort by systime. New: show start time. Uh oh. */
CMP_INT(start_code)
CMP_INT(end_code)
CMP_INT(start_stack)
CMP_INT(kstk_esp)
CMP_INT(kstk_eip)
CMP_INT(start_time)
CMP_INT(wchan)

/* CMP_STR(*environ) */
/* CMP_STR(*cmdline) */

CMP_STR(ruser)
CMP_STR(euser)
CMP_STR(suser)
CMP_STR(fuser)
CMP_STR(rgroup)
CMP_STR(egroup)
CMP_STR(sgroup)
CMP_STR(fgroup)
CMP_STR(cmd)
/* CMP_STR(ttyc) */    /* FIXME -- use strncmp with 8 max */

CMP_INT(ruid)
CMP_INT(rgid)
CMP_INT(euid)
CMP_INT(egid)
CMP_INT(suid)
CMP_INT(sgid)
CMP_INT(fuid)
CMP_INT(fgid)
CMP_SMALL(tid)
CMP_SMALL(tgid)
CMP_SMALL(ppid)
CMP_SMALL(pgrp)
CMP_SMALL(session)
CMP_INT(tty)
CMP_SMALL(tpgid)

CMP_SMALL(pcpu)

CMP_SMALL(state)

/* approximation to: kB of address space that could end up in swap */
static int sr_swapable(const proc_t* P, const proc_t* Q) {
  unsigned long p_swapable = P->vm_data + P->vm_stack;
  unsigned long q_swapable = Q->vm_data + Q->vm_stack;
  if (p_swapable < q_swapable) return -1;
  if (p_swapable > q_swapable) return  1;
  return 0;
}


/***************************************************************************/
/************ Lots of format functions, starting with the NOP **************/

// so popular it can't be "static"
int pr_nop(char *restrict const outbuf, const proc_t *restrict const pp){
  (void)pp;
  return snprintf(outbuf, COLWID, "%c", '-');
}


/********* Unix 98 ************/

/***

Only comm and args are allowed to contain blank characters; all others are
not. Any implementation-dependent variables will be specified in the system
documentation along with the default header and indicating if the field
may contain blank characters.

Some headers do not have a standardized specifier!

%CPU	pcpu	The % of cpu time used recently, with unspecified "recently".
ADDR		The address of the process.
C		Processor utilisation for scheduling.
CMD		The command name, or everything with -f.
COMMAND	args	Command + args. May chop as desired. May use either version.
COMMAND	comm	argv[0]
ELAPSED	etime	Elapsed time since the process was started. [[dd-]hh:]mm:ss
F		Flags (octal and additive)
GROUP	group	Effective group ID, prefer text over decimal.
NI	nice	Decimal system scheduling priority, see nice(1).
PGID	pgid	The decimal value of the process group ID.
PID	pid	Decimal PID.
PPID	ppid	Decimal PID.
PRI		Priority. Higher numbers mean lower priority.
RGROUP	rgroup	Real group ID, prefer text over decimal.
RUSER	ruser	Real user ID, prefer text over decimal.
S		The state of the process.
STIME		Starting time of the process.
SZ		The size in blocks of the core image of the process.
TIME	time	Cumulative CPU time. [dd-]hh:mm:ss
TT	tty	Name of tty in format used by who(1).
TTY		The controlling terminal for the process.
UID		UID, or name when -f
USER	user	Effective user ID, prefer text over decimal.
VSZ	vsz	Virtual memory size in decimal kB.
WCHAN		Where waiting/sleeping or blank if running.

The nice value is used to compute the priority.

For some undefined ones, Digital does:

F       flag    Process flags -- but in hex!
PRI     pri     Process priority
S       state   Symbolic process status
TTY     tt,tty,tname,longtname  -- all do "ttyp1", "console", "??"
UID     uid     Process user ID (effective UID)
WCHAN   wchan   Address of event on which a

For some undefined ones, Sun does:

ADDR	addr	memory address of the process
C	c	Processor utilization  for  scheduling  (obsolete).
CMD
F	f
S	s	state: OSRZT
STIME		start time, printed w/o blanks. If 24h old, months & days
SZ		size (in pages) of the swappable process's image in main memory
TTY
UID	uid
WCHAN	wchan

For some undefined ones, SCO does:
ADDR	addr	Virtual address of the process' entry in the process table.
SZ		swappable size in kB of the virtual data and stack
STIME	stime	hms or md time format
***/

/* Source & destination are known. Return bytes or screen characters? */
static int forest_helper(char *restrict const outbuf){
  char *p = forest_prefix;
  char *q = outbuf;
  int rightward=max_rightward;
  if(!*p) return 0;
  /* Arrrgh! somebody defined unix as 1 */
  if(forest_type == 'u') goto unixy;
  while(*p){
    switch(*p){
    case ' ': strcpy(q, "    ");  break;
    case 'L': strcpy(q, " \\_ "); break;
    case '+': strcpy(q, " \\_ "); break;
    case '|': strcpy(q, " |  ");  break;
    case '\0': return q-outbuf;    /* redundant & not used */
    }
    if (rightward-4 < 0) {
      *(q+rightward)='\0';
      return max_rightward;
    }
    q += 4;
    rightward -= 4;
    p++;
  }
  return q-outbuf;   /* gcc likes this here */
unixy:
  while(*p){
    switch(*p){
    case ' ': strcpy(q, "  "); break;
    case 'L': strcpy(q, "  "); break;
    case '+': strcpy(q, "  "); break;
    case '|': strcpy(q, "  "); break;
    case '\0': return q-outbuf;    /* redundant & not used */
    }
    if (rightward-2 < 0) {
      *(q+rightward)='\0';
      return max_rightward;
    }
    q += 2;
    rightward -= 2;
    p++;
  }
  return q-outbuf;   /* gcc likes this here */
}


/* XPG4-UNIX, according to Digital:
The "args" and "command" specifiers show what was passed to the command.
Modifications to the arguments are not shown.
*/

/*
 * pp->cmd       short accounting name (comm & ucomm)
 * pp->cmdline   long name with args (args & command)
 * pp->environ   environment
 */

// FIXME: some of these may hit the guard page in forest mode

/* "command" is the same thing: long unless c */
static int pr_args(char *restrict const outbuf, const proc_t *restrict const pp){
  char *endp = outbuf;
  unsigned flags;
  int rightward=max_rightward;

  if(forest_prefix){
    int fh = forest_helper(outbuf);
    endp += fh;
    rightward -= fh;
  }
  if(bsd_c_option) flags = ESC_DEFUNCT;
  else             flags = ESC_DEFUNCT | ESC_BRACKETS | ESC_ARGS;
  endp += escape_command(endp, pp, OUTBUF_SIZE, &rightward, flags);

  if(bsd_e_option && rightward>1){
    const char **env = (const char**)pp->environ;
    if(env && *env){
      *endp++ = ' ';
      rightward--;
      endp += escape_strlist(endp, env, OUTBUF_SIZE, &rightward);
    }
  }
  //return endp - outbuf;
  return max_rightward-rightward;
}

/* "ucomm" is the same thing: short unless -f */
static int pr_comm(char *restrict const outbuf, const proc_t *restrict const pp){
  char *endp = outbuf;
  unsigned flags;
  int rightward=max_rightward;
  
  if(forest_prefix){
    int fh = forest_helper(outbuf);
    endp += fh;
    rightward -= fh;
  }
  if(unix_f_option) flags = ESC_DEFUNCT | ESC_BRACKETS | ESC_ARGS;
  else              flags = ESC_DEFUNCT;
  endp += escape_command(endp, pp, OUTBUF_SIZE, &rightward, flags);

  if(bsd_e_option && rightward>1){
    const char **env = (const char**)pp->environ;
    if(env && *env){
      *endp++ = ' ';
      rightward--;
      endp += escape_strlist(endp, env, OUTBUF_SIZE, &rightward);
    }
  }
  //return endp - outbuf;
  return max_rightward-rightward;
}
/* Non-standard, from SunOS 5 */
static int pr_fname(char *restrict const outbuf, const proc_t *restrict const pp){
  char *endp = outbuf;
  int rightward = max_rightward;
  
  if(forest_prefix){
    int fh = forest_helper(outbuf);
    endp += fh;
    rightward -= fh;
  }
  if (rightward>8)  /* 8=default, but forest maybe feeds more */
    rightward = 8;
  
  endp += escape_str(endp, pp->cmd, OUTBUF_SIZE, &rightward);
  //return endp - outbuf;
  return max_rightward-rightward;
}

/* elapsed wall clock time, [[dd-]hh:]mm:ss format (not same as "time") */
static int pr_etime(char *restrict const outbuf, const proc_t *restrict const pp){
  unsigned long t;
  unsigned dd,hh,mm,ss;
  char *cp = outbuf;
  t = seconds_since_boot - (unsigned long)(pp->start_time / Hertz);
  ss = t%60;
  t /= 60;
  mm = t%60;
  t /= 60;
  hh = t%24;
  t /= 24;
  dd = t;
  cp +=(     dd      ?  snprintf(cp, COLWID, "%u-", dd)           :  0 );
  cp +=( (dd || hh)  ?  snprintf(cp, COLWID, "%02u:", hh)         :  0 );
  cp +=                 snprintf(cp, COLWID, "%02u:%02u", mm, ss)       ;
  return (int)(cp-outbuf);
}

/* "Processor utilisation for scheduling."  --- we use %cpu w/o fraction */
static int pr_c(char *restrict const outbuf, const proc_t *restrict const pp){
  unsigned long long total_time;   /* jiffies used by this process */
  unsigned pcpu = 0;               /* scaled %cpu, 99 means 99% */
  unsigned long long seconds;      /* seconds of process life */
  total_time = pp->utime + pp->stime;
  if(include_dead_children) total_time += (pp->cutime + pp->cstime);
  seconds = seconds_since_boot - pp->start_time / Hertz;
  if(seconds) pcpu = (total_time * 100ULL / Hertz) / seconds;
  if (pcpu > 99U) pcpu = 99U;
  return snprintf(outbuf, COLWID, "%2u", pcpu);
}
/* normal %CPU in ##.# format. */
static int pr_pcpu(char *restrict const outbuf, const proc_t *restrict const pp){
  unsigned long long total_time;   /* jiffies used by this process */
  unsigned pcpu = 0;               /* scaled %cpu, 999 means 99.9% */
  unsigned long long seconds;      /* seconds of process life */
  total_time = pp->utime + pp->stime;
  if(include_dead_children) total_time += (pp->cutime + pp->cstime);
  seconds = seconds_since_boot - pp->start_time / Hertz;
  if(seconds) pcpu = (total_time * 1000ULL / Hertz) / seconds;
  if (pcpu > 999U)
    return snprintf(outbuf, COLWID, "%u", pcpu/10U);
  return snprintf(outbuf, COLWID, "%u.%u", pcpu/10U, pcpu%10U);
}
/* this is a "per-mill" format, like %cpu with no decimal point */
static int pr_cp(char *restrict const outbuf, const proc_t *restrict const pp){
  unsigned long long total_time;   /* jiffies used by this process */
  unsigned pcpu = 0;               /* scaled %cpu, 999 means 99.9% */
  unsigned long long seconds;      /* seconds of process life */
  total_time = pp->utime + pp->stime;
  if(include_dead_children) total_time += (pp->cutime + pp->cstime);
  seconds = seconds_since_boot - pp->start_time / Hertz ;
  if(seconds) pcpu = (total_time * 1000ULL / Hertz) / seconds;
  if (pcpu > 999U) pcpu = 999U;
  return snprintf(outbuf, COLWID, "%3u", pcpu);
}

static int pr_pgid(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%u", pp->pgrp);
}
static int pr_pid(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%u", pp->tgid);
}
static int pr_ppid(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%u", pp->ppid);
}


/* cumulative CPU time, [dd-]hh:mm:ss format (not same as "etime") */
static int pr_time(char *restrict const outbuf, const proc_t *restrict const pp){
  unsigned long t;
  unsigned dd,hh,mm,ss;
  int c;
  t = (pp->utime + pp->stime) / Hertz;
  ss = t%60;
  t /= 60;
  mm = t%60;
  t /= 60;
  hh = t%24;
  t /= 24;
  dd = t;
  c  =( dd ? snprintf(outbuf, COLWID, "%u-", dd) : 0              );
  c +=( snprintf(outbuf+c, COLWID, "%02u:%02u:%02u", hh, mm, ss)    );
  return c;
}

/* HP-UX puts this (I forget, vsz or vsize?) in kB and uses "sz" for pages.
 * Unix98 requires "vsz" to be kB.
 * Tru64 does both vsize and vsz like "1.23M"
 *
 * Our pp->vm_size is kB and our pp->vsize is pages.
 *
 * TODO: add flag for "1.23M" behavior, on this and other columns.
 */
static int pr_vsz(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%lu", pp->vm_size);
}

//////////////////////////////////////////////////////////////////////////////////////

// "PRI" is created by "opri", or by "pri" when -c is used.
//
// Unix98 only specifies that a high "PRI" is low priority.
// Sun and SCO add the -c behavior. Sun defines "pri" and "opri".
// Linux may use "priority" for historical purposes.
//
// According to the kernel's fs/proc/array.c and kernel/sched.c source,
// the kernel reports it in /proc via this:
//        p->prio - MAX_RT_PRIO
// such that "RT tasks are offset by -200. Normal tasks are centered
// around 0, value goes from -16 to +15" but who knows if that is
// before or after the conversion...
//
// <linux/sched.h> says:
// MAX_RT_PRIO is currently 100.       (so we see 0 in /proc)
// RT tasks have a p->prio of 0 to 99. (so we see -100 to -1)
// non-RT tasks are from 100 to 139.   (so we see 0 to 39)
// Lower values have higher priority, as in the UNIX standard.
//
// In any case, pp->priority+100 should get us back to what the kernel
// has for p->prio.
//
// Test results with the "yes" program on a 2.6.x kernel:
//
// # ps -C19,_20 -o pri,opri,intpri,priority,ni,pcpu,pid,comm
// PRI PRI PRI PRI  NI %CPU  PID COMMAND
//   0  99  99  39  19 10.6 8686 19
//  34  65  65   5 -20 94.7 8687 _20
//
// Grrr. So the UNIX standard "PRI" must NOT be from "pri".
// Either of the others will do. We use "opri" for this.
// (and use "pri" when the "-c" option is used)
// Probably we should have Linux-specific "pri_for_l" and "pri_for_lc"
//
// sched_get_priority_min.2 says the Linux static priority is
// 1..99 for RT and 0 for other... maybe 100 is kernel-only?
//
// A nice range would be -99..0 for RT and 1..40 for normal,
// which is pp->priority+1. (3-digit max, positive is normal,
// negative or 0 is RT, and meets the standard for PRI)
//

// legal as UNIX "PRI"
// "priority"         (was -20..20, now -100..39)
static int pr_priority(char *restrict const outbuf, const proc_t *restrict const pp){    /* -20..20 */
    return snprintf(outbuf, COLWID, "%ld", pp->priority);
}

// legal as UNIX "PRI"
// "intpri" and "opri" (was 39..79, now  -40..99)
static int pr_opri(char *restrict const outbuf, const proc_t *restrict const pp){        /* 39..79 */
    return snprintf(outbuf, COLWID, "%ld", 60 + pp->priority);
}

// legal as UNIX "PRI"
// "pri_foo"   --  match up w/ nice values of sleeping processes (-120..19)
static int pr_pri_foo(char *restrict const outbuf, const proc_t *restrict const pp){
    return snprintf(outbuf, COLWID, "%ld", pp->priority - 20);
}

// legal as UNIX "PRI"
// "pri_bar"   --  makes RT pri show as negative       (-99..40)
static int pr_pri_bar(char *restrict const outbuf, const proc_t *restrict const pp){
    return snprintf(outbuf, COLWID, "%ld", pp->priority + 1);
}

// legal as UNIX "PRI"
// "pri_baz"   --  the kernel's ->prio value, as of Linux 2.6.8     (1..140)
static int pr_pri_baz(char *restrict const outbuf, const proc_t *restrict const pp){
    return snprintf(outbuf, COLWID, "%ld", pp->priority + 100);
}


// not legal as UNIX "PRI"
// "pri"               (was 20..60, now    0..139)
static int pr_pri(char *restrict const outbuf, const proc_t *restrict const pp){         /* 20..60 */
    return snprintf(outbuf, COLWID, "%ld", 39 - pp->priority);
}

// not legal as UNIX "PRI"
// "pri_api"   --  match up w/ RT API    (-40..99)
static int pr_pri_api(char *restrict const outbuf, const proc_t *restrict const pp){
    return snprintf(outbuf, COLWID, "%ld", -1 - pp->priority);
}

static int pr_nice(char *restrict const outbuf, const proc_t *restrict const pp){
  if(pp->sched!=0 && pp->sched!=-1) return snprintf(outbuf, COLWID, "-");
  return snprintf(outbuf, COLWID, "%ld", pp->nice);
}

// HP-UX   "cls": RT RR RR2 ???? HPUX FIFO KERN
// Solaris "class": SYS TS FX IA RT FSS (FIFO is RR w/ Inf quant)
//                  FIFO+RR share RT; FIFO has Inf quant
//                  IA=interactive; FX=fixed; TS=timeshare; SYS=system
//                  FSS=fairshare; INTS=interrupts
// Tru64   "policy": FF RR TS
// IRIX    "class": RT TS B BC WL GN
//                  RT=real-time; TS=time-share; B=batch; BC=batch-critical
//                  WL=weightless; GN=gang-scheduled
//                  see miser(1) for this; PRI has some letter codes too
static int pr_class(char *restrict const outbuf, const proc_t *restrict const pp){
  switch(pp->sched){
  case -1: return snprintf(outbuf, COLWID, "-");   // not reported
  case  0: return snprintf(outbuf, COLWID, "TS");  // SCHED_OTHER SCHED_NORMAL
  case  1: return snprintf(outbuf, COLWID, "FF");  // SCHED_FIFO
  case  2: return snprintf(outbuf, COLWID, "RR");  // SCHED_RR
  case  3: return snprintf(outbuf, COLWID, "B");   // SCHED_BATCH
  case  4: return snprintf(outbuf, COLWID, "ISO"); // reserved for SCHED_ISO (Con Kolivas)
  case  5: return snprintf(outbuf, COLWID, "IDL"); // SCHED_IDLE
  case  6: return snprintf(outbuf, COLWID, "#6");  //
  case  7: return snprintf(outbuf, COLWID, "#7");  //
  case  8: return snprintf(outbuf, COLWID, "#8");  //
  case  9: return snprintf(outbuf, COLWID, "#9");  //
  default: return snprintf(outbuf, COLWID, "?");   // unknown value
  }
}
// Based on "type", FreeBSD would do:
//    REALTIME  "real:%u", prio
//    NORMAL    "normal"
//    IDLE      "idle:%u", prio
//    default   "%u:%u", type, prio
// We just print the priority, and have other keywords for type.
static int pr_rtprio(char *restrict const outbuf, const proc_t *restrict const pp){
  if(pp->sched==0 || pp->sched==-1) return snprintf(outbuf, COLWID, "-");
  return snprintf(outbuf, COLWID, "%ld", pp->rtprio);
}
static int pr_sched(char *restrict const outbuf, const proc_t *restrict const pp){
  if(pp->sched==-1) return snprintf(outbuf, COLWID, "-");
  return snprintf(outbuf, COLWID, "%ld", pp->sched);
}

////////////////////////////////////////////////////////////////////////////////

static int pr_wchan(char *restrict const outbuf, const proc_t *restrict const pp){
/*
 * Unix98 says "blank if running" and also "no blanks"! :-(
 * Unix98 also says to use '-' if something is meaningless.
 * Digital uses both '*' and '-', with undocumented differences.
 * (the '*' for -1 (rare) and the '-' for 0)
 * Sun claims to use a blank AND use '-', in the same man page.
 * Perhaps "blank" should mean '-'.
 *
 * AIX uses '-' for running processes, the location when there is
 * only one thread waiting in the kernel, and '*' when there is
 * more than one thread waiting in the kernel.
 *
 * The output should be truncated to maximal columns width -- overflow
 * is not supported for the "wchan".
 */
  const char *w;
  size_t len;
  if(!(pp->wchan & 0xffffff)) return memcpy(outbuf,"-",2),1;
  if(wchan_is_number) return snprintf(outbuf, COLWID, "%x", (unsigned)(pp->wchan) & 0xffffffu);
  w = lookup_wchan(pp->wchan, pp->XXXID);
  len = strlen(w);
  if(len>max_rightward) len=max_rightward;
  memcpy(outbuf, w, len);
  outbuf[len] = '\0';
  return len;
}

static int pr_wname(char *restrict const outbuf, const proc_t *restrict const pp){
/* SGI's IRIX always uses a number for "wchan", so "wname" is provided too.
 *
 * We use '-' for running processes, the location when there is
 * only one thread waiting in the kernel, and '*' when there is
 * more than one thread waiting in the kernel.
 *
 * The output should be truncated to maximal columns width -- overflow
 * is not supported for the "wchan".
 */
  const char *w;
  size_t len;
  if(!(pp->wchan & 0xffffff)) return memcpy(outbuf,"-",2),1;
  w = lookup_wchan(pp->wchan, pp->XXXID);
  len = strlen(w);
  if(len>max_rightward) len=max_rightward;
  memcpy(outbuf, w, len);
  outbuf[len] = '\0';
  return len;
}

static int pr_nwchan(char *restrict const outbuf, const proc_t *restrict const pp){
  if(!(pp->wchan & 0xffffff)) return memcpy(outbuf,"-",2),1;
  return snprintf(outbuf, COLWID, "%x", (unsigned)(pp->wchan) & 0xffffffu);
}

/* Terrible trunctuation, like BSD crap uses: I999 J999 K999 */
/* FIXME: disambiguate /dev/tty69 and /dev/pts/69. */
static int pr_tty4(char *restrict const outbuf, const proc_t *restrict const pp){
/* snprintf(outbuf, COLWID, "%02x:%02x", pp->tty>>8, pp->tty&0xff); */
  return dev_to_tty(outbuf, 4, pp->tty, pp->XXXID, ABBREV_DEV|ABBREV_TTY|ABBREV_PTS);
}

/* Unix98: format is unspecified, but must match that used by who(1). */
static int pr_tty8(char *restrict const outbuf, const proc_t *restrict const pp){
/* snprintf(outbuf, COLWID, "%02x:%02x", pp->tty>>8, pp->tty&0xff); */
  return dev_to_tty(outbuf, COLWID, pp->tty, pp->XXXID, ABBREV_DEV);
}

#if 0
/* This BSD state display may contain spaces, which is illegal. */
static int pr_oldstate(char *restrict const outbuf, const proc_t *restrict const pp){
    return snprintf(outbuf, COLWID, "%s", status(pp));
}
#endif

// This state display is Unix98 compliant and has lots of info like BSD.
static int pr_stat(char *restrict const outbuf, const proc_t *restrict const pp){
    int end = 0;
    outbuf[end++] = pp->state;
//  if(pp->rss==0 && pp->state!='Z')  outbuf[end++] = 'W'; // useless "swapped out"
    if(pp->nice < 0)                  outbuf[end++] = '<';
    if(pp->nice > 0)                  outbuf[end++] = 'N';
// In this order, NetBSD would add:
//     traced   'X'
//     systrace 'x'
//     exiting  'E' (not printed for zombies)
//     vforked  'V'
//     system   'K' (and do not print 'L' too)
    if(pp->vm_lock)                   outbuf[end++] = 'L';
    if(pp->session == pp->tgid)       outbuf[end++] = 's'; // session leader
    if(pp->nlwp > 1)                  outbuf[end++] = 'l'; // multi-threaded
    if(pp->pgrp == pp->tpgid)         outbuf[end++] = '+'; // in foreground process group
    outbuf[end] = '\0';
    return end;
}

/* This minimal state display is Unix98 compliant, like SCO and SunOS 5 */
static int pr_s(char *restrict const outbuf, const proc_t *restrict const pp){
    outbuf[0] = pp->state;
    outbuf[1] = '\0';
    return 1;
}

static int pr_flag(char *restrict const outbuf, const proc_t *restrict const pp){
    /* Unix98 requires octal flags */
    /* this user-hostile and volatile junk gets 1 character */
    return snprintf(outbuf, COLWID, "%o", (unsigned)(pp->flags>>6U)&0x7U);
}

// plus these: euid,ruid,egroup,rgroup (elsewhere in this file)

/*********** non-standard ***********/

/*** BSD
sess	session pointer
(SCO has:Process session leader ID as a decimal value. (SESSION))
jobc	job control count
cpu	short-term cpu usage factor (for scheduling)
sl	sleep time (in seconds; 127 = infinity)
re	core residency time (in seconds; 127 = infinity)
pagein	pageins (same as majflt)
lim	soft memory limit
tsiz	text size (in Kbytes)
***/

static int pr_stackp(char *restrict const outbuf, const proc_t *restrict const pp){
    return snprintf(outbuf, COLWID, "%08x", (unsigned)(pp->start_stack));
}

static int pr_esp(char *restrict const outbuf, const proc_t *restrict const pp){
    return snprintf(outbuf, COLWID, "%08x", (unsigned)(pp->kstk_esp));
}

static int pr_eip(char *restrict const outbuf, const proc_t *restrict const pp){
    return snprintf(outbuf, COLWID, "%08x", (unsigned)(pp->kstk_eip));
}

/* This function helps print old-style time formats */
static int old_time_helper(char *dst, unsigned long long t, unsigned long long rel) {
  if(!t)            return snprintf(dst, COLWID, "    -");
  if(t == ~0ULL)    return snprintf(dst, COLWID, "   xx");
  if((long long)(t-=rel) < 0)  t=0ULL;
  if(t>9999ULL)     return snprintf(dst, COLWID, "%5Lu", t/100ULL);
  else              return snprintf(dst, COLWID, "%2u.%02u", (unsigned)t/100U, (unsigned)t%100U);
}

static int pr_bsdtime(char *restrict const outbuf, const proc_t *restrict const pp){
    unsigned long long t;
    unsigned u;
    t = pp->utime + pp->stime;
    if(include_dead_children) t += (pp->cutime + pp->cstime);
    u = t / Hertz;
    return snprintf(outbuf, COLWID, "%3u:%02u", u/60U, u%60U);
}

static int pr_bsdstart(char *restrict const outbuf, const proc_t *restrict const pp){
  time_t start;
  time_t seconds_ago;
  start = time_of_boot + pp->start_time / Hertz;
  seconds_ago = seconds_since_1970 - start;
  if(seconds_ago < 0) seconds_ago=0;
  if(seconds_ago > 3600*24)  strcpy(outbuf, ctime(&start)+4);
  else                       strcpy(outbuf, ctime(&start)+10);
  outbuf[6] = '\0';
  return 6;
}

static int pr_alarm(char *restrict const outbuf, const proc_t *restrict const pp){
    return old_time_helper(outbuf, pp->alarm, 0ULL);
}

/* HP-UX puts this in pages and uses "vsz" for kB */
static int pr_sz(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%lu", (pp->vm_size)/(page_size/1024));
}


/*
 * FIXME: trs,drs,tsiz,dsiz,m_trs,m_drs,vm_exe,vm_data,trss
 * I suspect some/all of those are broken. They seem to have been
 * inherited by Linux and AIX from early BSD systems. FreeBSD only
 * retains tsiz. The prefixed versions come from Debian.
 * Sun and Digital have none of this crap. The code here comes
 * from an old Linux ps, and might not be correct for ELF executables.
 *
 * AIX            TRS    size of resident-set (real memory) of text
 * AIX            TSIZ   size of text (shared-program) image
 * FreeBSD        tsiz   text size (in Kbytes)
 * 4.3BSD NET/2   trss   text resident set size (in Kbytes)
 * 4.3BSD NET/2   tsiz   text size (in Kbytes)
 */

/* kB data size. See drs, tsiz & trs. */
static int pr_dsiz(char *restrict const outbuf, const proc_t *restrict const pp){
    long dsiz = 0;
    if(pp->vsize) dsiz += (pp->vsize - pp->end_code + pp->start_code) >> 10;
    return snprintf(outbuf, COLWID, "%ld", dsiz);
}

/* kB text (code) size. See trs, dsiz & drs. */
static int pr_tsiz(char *restrict const outbuf, const proc_t *restrict const pp){
    long tsiz = 0;
    if(pp->vsize) tsiz += (pp->end_code - pp->start_code) >> 10;
    return snprintf(outbuf, COLWID, "%ld", tsiz);
}

/* kB _resident_ data size. See dsiz, tsiz & trs. */
static int pr_drs(char *restrict const outbuf, const proc_t *restrict const pp){
    long drs = 0;
    if(pp->vsize) drs += (pp->vsize - pp->end_code + pp->start_code) >> 10;
    return snprintf(outbuf, COLWID, "%ld", drs);
}

/* kB text _resident_ (code) size. See tsiz, dsiz & drs. */
static int pr_trs(char *restrict const outbuf, const proc_t *restrict const pp){
    long trs = 0;
    if(pp->vsize) trs += (pp->end_code - pp->start_code) >> 10;
    return snprintf(outbuf, COLWID, "%ld", trs);
}

/* approximation to: kB of address space that could end up in swap */
static int pr_swapable(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%ld", pp->vm_data + pp->vm_stack);
}

/* nasty old Debian thing */
static int pr_size(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%ld", pp->size);
}


static int pr_minflt(char *restrict const outbuf, const proc_t *restrict const pp){
    long flt = pp->min_flt;
    if(include_dead_children) flt += pp->cmin_flt;
    return snprintf(outbuf, COLWID, "%ld", flt);
}

static int pr_majflt(char *restrict const outbuf, const proc_t *restrict const pp){
    long flt = pp->maj_flt;
    if(include_dead_children) flt += pp->cmaj_flt;
    return snprintf(outbuf, COLWID, "%ld", flt);
}

static int pr_lim(char *restrict const outbuf, const proc_t *restrict const pp){
    if(pp->rss_rlim == RLIM_INFINITY){
      outbuf[0] = 'x';
      outbuf[1] = 'x';
      outbuf[2] = '\0';
      return 2;
    }
    return snprintf(outbuf, COLWID, "%5ld", pp->rss_rlim >> 10);
}

/* should print leading tilde ('~') if process is bound to the CPU */
static int pr_psr(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%d", pp->processor);
}

static int pr_rss(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%lu", pp->vm_rss);
}

/* pp->vm_rss * 1000 would overflow on 32-bit systems with 64 GB memory */
static int pr_pmem(char *restrict const outbuf, const proc_t *restrict const pp){
  unsigned long pmem = 0;
  pmem = pp->vm_rss * 1000ULL / kb_main_total;
  if (pmem > 999) pmem = 999;
  return snprintf(outbuf, COLWID, "%2u.%u", (unsigned)(pmem/10), (unsigned)(pmem%10));
}

static int pr_lstart(char *restrict const outbuf, const proc_t *restrict const pp){
  time_t t;
  t = time_of_boot + pp->start_time / Hertz;
  return snprintf(outbuf, COLWID, "%24.24s", ctime(&t));
}

/* Unix98 specifies a STIME header for a column that shows the start
 * time of the process, but does not specify a format or format specifier.
 * From the general Unix98 rules, we know there must not be any spaces.
 * Most systems violate that rule, though the Solaris documentation
 * claims to print the column without spaces. (NOT!)
 *
 * So this isn't broken, but could be renamed to u98_std_stime,
 * as long as it still shows as STIME when using the -f option.
 */
static int pr_stime(char *restrict const outbuf, const proc_t *restrict const pp){
  struct tm *proc_time;
  struct tm *our_time;
  time_t t;
  const char *fmt;
  int tm_year;
  int tm_yday;
  our_time = localtime(&seconds_since_1970);   /* not reentrant */
  tm_year = our_time->tm_year;
  tm_yday = our_time->tm_yday;
  t = time_of_boot + pp->start_time / Hertz;
  proc_time = localtime(&t); /* not reentrant, this corrupts our_time */
  fmt = "%H:%M";                                   /* 03:02 23:59 */
  if(tm_yday != proc_time->tm_yday) fmt = "%b%d";  /* Jun06 Aug27 */
  if(tm_year != proc_time->tm_year) fmt = "%Y";    /* 1991 2001 */
  return strftime(outbuf, 42, fmt, proc_time);
}

static int pr_start(char *restrict const outbuf, const proc_t *restrict const pp){
  time_t t;
  char *str;
  t = time_of_boot + pp->start_time / Hertz;
  str = ctime(&t);
  if(str[8]==' ')  str[8]='0';
  if(str[11]==' ') str[11]='0';
  if((unsigned long)t+60*60*24 > seconds_since_1970)
    return snprintf(outbuf, COLWID, "%8.8s", str+11);
  return snprintf(outbuf, COLWID, "  %6.6s", str+4);
}


#ifdef SIGNAL_STRING
static int help_pr_sig(char *restrict const outbuf, const char *restrict const sig){
  long len = 0;
  len = strlen(sig);
  if(wide_signals){
    if(len>8) return snprintf(outbuf, COLWID, "%s", sig);
    return snprintf(outbuf, COLWID, "00000000%s", sig);
  }
  if(len-strspn(sig,"0") > 8)
    return snprintf(outbuf, COLWID, "<%s", sig+len-8);
  return snprintf(outbuf, COLWID,  "%s", sig+len-8);
}
#else
static int help_pr_sig(unsigned long long sig){
  if(wide_signals) return snprintf(outbuf, COLWID, "%016Lx", sig);
  if(sig>>32)      return snprintf(outbuf, COLWID, "<%08Lx", sig&0xffffffffLL);
  return                  snprintf(outbuf, COLWID,  "%08Lx", sig&0xffffffffLL);
}
#endif

// This one is always thread-specific pending. (from Dragonfly BSD)
static int pr_tsig(char *restrict const outbuf, const proc_t *restrict const pp){
  return help_pr_sig(outbuf, pp->_sigpnd);
}
// This one is (wrongly?) thread-specific when printing thread lines,
// but process-pending otherwise.
static int pr_sig(char *restrict const outbuf, const proc_t *restrict const pp){
  return help_pr_sig(outbuf, pp->signal);
}
static int pr_sigmask(char *restrict const outbuf, const proc_t *restrict const pp){
  return help_pr_sig(outbuf, pp->blocked);
}
static int pr_sigignore(char *restrict const outbuf, const proc_t *restrict const pp){
  return help_pr_sig(outbuf, pp->sigignore);
}
static int pr_sigcatch(char *restrict const outbuf, const proc_t *restrict const pp){
  return help_pr_sig(outbuf, pp->sigcatch);
}


////////////////////////////////////////////////////////////////////////////////

/*
 * internal terms:  ruid  euid  suid  fuid
 * kernel vars:      uid  euid  suid fsuid
 * command args:    ruid   uid svuid   n/a
 */

static int pr_egid(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%d", pp->egid);
}
static int pr_rgid(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%d", pp->rgid);
}
static int pr_sgid(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%d", pp->sgid);
}
static int pr_fgid(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%d", pp->fgid);
}

static int pr_euid(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%d", pp->euid);
}
static int pr_ruid(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%d", pp->ruid);
}
static int pr_suid(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%d", pp->suid);
}
static int pr_fuid(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%d", pp->fuid);
}

// The Open Group Base Specifications Issue 6 (IEEE Std 1003.1, 2004 Edition)
// requires that user and group names print as decimal numbers if there is
// not enough room in the column, so tough luck if you don't like it.
//
// The UNIX and POSIX way to change column width is to rename it:
//      ps -o pid,user=CumbersomeUserNames -o comm
// The easy way is to directly specify the desired width:
//      ps -o pid,user:19,comm
//
static int do_pr_name(char *restrict const outbuf, const char *restrict const name, unsigned u){
  if(!user_is_number){
    int rightward = OUTBUF_SIZE;	/* max cells */
    int len;				/* real cells */
    
    escape_str(outbuf, name, OUTBUF_SIZE, &rightward);
    len = OUTBUF_SIZE-rightward;
    
    if(len <= (int)max_rightward)
      return len;  /* returns number of cells */
  }
  return snprintf(outbuf, COLWID, "%u", u);
}

static int pr_ruser(char *restrict const outbuf, const proc_t *restrict const pp){
  return do_pr_name(outbuf, pp->ruser, pp->ruid);
}
static int pr_euser(char *restrict const outbuf, const proc_t *restrict const pp){
  return do_pr_name(outbuf, pp->euser, pp->euid);
}
static int pr_fuser(char *restrict const outbuf, const proc_t *restrict const pp){
  return do_pr_name(outbuf, pp->fuser, pp->fuid);
}
static int pr_suser(char *restrict const outbuf, const proc_t *restrict const pp){
  return do_pr_name(outbuf, pp->suser, pp->suid);
}

static int pr_egroup(char *restrict const outbuf, const proc_t *restrict const pp){
  return do_pr_name(outbuf, pp->egroup, pp->egid);
}
static int pr_rgroup(char *restrict const outbuf, const proc_t *restrict const pp){
  return do_pr_name(outbuf, pp->rgroup, pp->rgid);
}
static int pr_fgroup(char *restrict const outbuf, const proc_t *restrict const pp){
  return do_pr_name(outbuf, pp->fgroup, pp->fgid);
}
static int pr_sgroup(char *restrict const outbuf, const proc_t *restrict const pp){
  return do_pr_name(outbuf, pp->sgroup, pp->sgid);
}

//////////////////////////////////////////////////////////////////////////////////

// TID tid LWP lwp SPID spid
static int pr_thread(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%u", pp->tid);
}
// thcount THCNT
static int pr_nlwp(char *restrict const outbuf, const proc_t *restrict const pp){
    return snprintf(outbuf, COLWID, "%d", pp->nlwp);
}

static int pr_sess(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%u", pp->session);
}
static int pr_tpgid(char *restrict const outbuf, const proc_t *restrict const pp){
  return snprintf(outbuf, COLWID, "%d", pp->tpgid);
}


/* SGI uses "cpu" to print the processor ID with header "P" */
static int pr_sgi_p(char *restrict const outbuf, const proc_t *restrict const pp){          /* FIXME */
  if(pp->state == 'R') return snprintf(outbuf, COLWID, "%d", pp->processor);
  return snprintf(outbuf, COLWID, "*");
}


/****************** FLASK & seLinux security stuff **********************/
// move the bulk of this to libproc sometime

static int pr_context(char *restrict const outbuf, const proc_t *restrict const pp){
  char filename[48];
  size_t len;
  ssize_t num_read;
  int fd;

// wchan file is suitable for testing
//snprintf(filename, sizeof filename, "/proc/%d/wchan", pp->tgid);
snprintf(filename, sizeof filename, "/proc/%d/attr/current", pp->tgid);

  fd = open(filename, O_RDONLY, 0);
  if(likely(fd==-1)) goto fail;
  num_read = read(fd, outbuf, 666);
  close(fd);
  if(unlikely(num_read<=0)) goto fail;
  outbuf[num_read] = '\0';

  len = 0;
  while(outbuf[len]>' ' && outbuf[len]<='~') len++;
  outbuf[len] = '\0';
  if(len) return len;

fail:
  outbuf[0] = '-';
  outbuf[1] = '\0';
  return 1;
}

#if 0
// This needs more study, considering:
// 1. the static linking option (maybe disable this in that case)
// 2. the -z and -Z option issue
// 3. width of output
static int pr_context(char *restrict const outbuf, const proc_t *restrict const pp){
  static int (*ps_getpidcon)(pid_t pid, char **context) = 0;
  static int tried_load = 0;
  size_t len;
  char *context;

  if(!ps_getpidcon && !tried_load){
    void *handle = dlopen("libselinux.so.1", RTLD_NOW);
    if(handle){
      dlerror();
      ps_getpidcon = dlsym(handle, "getpidcon");
      if(dlerror())
        ps_getpidcon = 0;
    }
    tried_load++;
  }
  if(ps_getpidcon && !ps_getpidcon(pp->tgid, &context)){
    size_t max_len = OUTBUF_SIZE-1;
    len = strlen(context);
    if(len > max_len) len = max_len;
    memcpy(outbuf, context, len);
    outbuf[len] = '\0';
    free(context);
  }else{
    outbuf[0] = '-';
    outbuf[1] = '\0';
    len = 1;
  }
  return len;
}
#endif


////////////////////////////// Test code /////////////////////////////////

// like "args"
static int pr_t_unlimited(char *restrict const outbuf, const proc_t *restrict const pp){
  static const char *const vals[] = {"[123456789-12345] <defunct>","ps","123456789-123456"};
  (void)pp;
  snprintf(outbuf, max_rightward+1, "%s", vals[lines_to_next_header%3u]);
  return strlen(outbuf);
}
static int pr_t_unlimited2(char *restrict const outbuf, const proc_t *restrict const pp){
  static const char *const vals[] = {"unlimited", "[123456789-12345] <defunct>","ps","123456789-123456"};
  (void)pp;
  snprintf(outbuf, max_rightward+1, "%s", vals[lines_to_next_header%4u]);
  return strlen(outbuf);
}

// like "etime"
static int pr_t_right(char *restrict const outbuf, const proc_t *restrict const pp){
  static const char *const vals[] = {"999-23:59:59","99-23:59:59","9-23:59:59","59:59"};
  (void)pp;
  return snprintf(outbuf, COLWID, "%s", vals[lines_to_next_header%4u]);
}
static int pr_t_right2(char *restrict const outbuf, const proc_t *restrict const pp){
  static const char *const vals[] = {"999-23:59:59","99-23:59:59","9-23:59:59"};
  (void)pp;
  return snprintf(outbuf, COLWID, "%s", vals[lines_to_next_header%3u]);
}

// like "tty"
static int pr_t_left(char *restrict const outbuf, const proc_t *restrict const pp){
  static const char *const vals[] = {"tty7","pts/9999","iseries/vtty42","ttySMX0","3270/tty4"};
  (void)pp;
  return snprintf(outbuf, COLWID, "%s", vals[lines_to_next_header%5u]);
}
static int pr_t_left2(char *restrict const outbuf, const proc_t *restrict const pp){
  static const char *const vals[] = {"tty7","pts/9999","ttySMX0","3270/tty4"};
  (void)pp;
  return snprintf(outbuf, COLWID, "%s", vals[lines_to_next_header%4u]);
}

/***************************************************************************/
/*************************** other stuff ***********************************/

/*
 * Old header specifications.
 *
 * short   Up  "  PID TTY STAT  TIME COMMAND"
 * long  l Pp  " FLAGS   UID   PID  PPID PRI  NI   SIZE   RSS WCHAN       STA TTY TIME COMMAND
 * user  u up  "USER       PID %CPU %MEM  SIZE   RSS TTY STAT START   TIME COMMAND
 * jobs  j gPp " PPID   PID  PGID   SID TTY TPGID  STAT   UID   TIME COMMAND
 * sig   s p   "  UID   PID SIGNAL   BLOCKED  IGNORED  CATCHED  STAT TTY   TIME COMMAND
 * vm    v r   "  PID TTY STAT  TIME  PAGEIN TSIZ DSIZ  RSS   LIM %MEM COMMAND
 * m     m r   "  PID TTY MAJFLT MINFLT   TRS   DRS  SIZE  SWAP   RSS  SHRD   LIB  DT COMMAND
 * regs  X p   "NR   PID    STACK      ESP      EIP TMOUT ALARM STAT TTY   TIME COMMAND
 */

/*
 * Unix98 requires that the heading for tty is TT, though XPG4, Digital,
 * and BSD use TTY. The Unix98 headers are:
 *              args,comm,etime,group,nice,pcpu,pgid
 *              pid,ppid,rgroup,ruser,time,tty,user,vsz
 *
 * BSD c:   "command" becomes accounting name ("comm" or "ucomm")
 * BSD n:   "user" becomes "uid" and "wchan" becomes "nwchan" (number)
 */

/* Justification control for flags field. */
#define USER      CF_USER   // left if text, right if numeric
#define LEFT      CF_LEFT
#define RIGHT     CF_RIGHT
#define UNLIMITED CF_UNLIMITED
#define WCHAN     CF_WCHAN  // left if text, right if numeric
#define SIGNAL    CF_SIGNAL // right in 9, or 16 if room
#define PIDMAX    CF_PIDMAX
#define TO        CF_PRINT_THREAD_ONLY
#define PO        CF_PRINT_PROCESS_ONLY
#define ET        CF_PRINT_EVERY_TIME
#define AN        CF_PRINT_AS_NEEDED // no idea

/* short names to save space */
#define MEM PROC_FILLMEM     /* read statm  */
#define ARG PROC_FILLARG     /* read cmdline (cleared if c option) */
#define COM PROC_FILLCOM     /* read cmdline (cleared if not -f option) */
#define ENV PROC_FILLENV     /* read environ */
#define USR PROC_FILLUSR     /* uid_t -> user names */
#define GRP PROC_FILLGRP     /* gid_t -> group names */
#define WCH PROC_FILLWCHAN   /* do WCHAN lookup */


/* TODO
 *      pull out annoying BSD aliases into another table (to macro table?)
 *      add sorting functions here (to unify names)
 */

/* temporary hack -- mark new stuff grabbed from Debian ps */
#define LNx LNX

/* there are about 211 listed */

/* Many of these are placeholders for unsupported options. */
static const format_struct format_array[] = {
/* code       header     print()      sort()    width need vendor flags  */
{"%cpu",      "%CPU",    pr_pcpu,     sr_pcpu,    4,   0,    BSD, ET|RIGHT}, /*pcpu*/
{"%mem",      "%MEM",    pr_pmem,     sr_nop,     4,   0,    BSD, PO|RIGHT}, /*pmem*/
{"_left",     "LLLLLLLL", pr_t_left,  sr_nop,     8,   0,    TST, ET|LEFT},
{"_left2",    "L2L2L2L2", pr_t_left2, sr_nop,     8,   0,    TST, ET|LEFT},
{"_right",    "RRRRRRRRRRR", pr_t_right, sr_nop, 11,   0,    TST, ET|RIGHT},
{"_right2",   "R2R2R2R2R2R", pr_t_right2, sr_nop, 11,  0,    TST, ET|RIGHT},
{"_unlimited","U",   pr_t_unlimited,  sr_nop,    16,   0,    TST, ET|UNLIMITED},
{"_unlimited2","U2", pr_t_unlimited2, sr_nop,    16,   0,    TST, ET|UNLIMITED},
{"acflag",    "ACFLG",   pr_nop,      sr_nop,     5,   0,    XXX, AN|RIGHT}, /*acflg*/
{"acflg",     "ACFLG",   pr_nop,      sr_nop,     5,   0,    BSD, AN|RIGHT}, /*acflag*/
{"addr",      "ADDR",    pr_nop,      sr_nop,     4,   0,    XXX, AN|RIGHT},
{"addr_1",    "ADDR",    pr_nop,      sr_nop,     1,   0,    LNX, AN|LEFT},
{"alarm",     "ALARM",   pr_alarm,    sr_alarm,   5,   0,    LNX, AN|RIGHT},
{"argc",      "ARGC",    pr_nop,      sr_nop,     4,   0,    LNX, PO|RIGHT},
{"args",      "COMMAND", pr_args,     sr_cmd,    27, ARG,    U98, PO|UNLIMITED}, /*command*/
{"atime",     "TIME",    pr_time,     sr_nop,     8,   0,    SOE, ET|RIGHT}, /*cputime*/ /* was 6 wide */
{"blocked",   "BLOCKED", pr_sigmask,  sr_nop,     9,   0,    BSD, TO|SIGNAL}, /*sigmask*/
{"bnd",       "BND",     pr_nop,      sr_nop,     1,   0,    AIX, TO|RIGHT},
{"bsdstart",  "START",   pr_bsdstart, sr_nop,     6,   0,    LNX, ET|RIGHT},
{"bsdtime",   "TIME",    pr_bsdtime,  sr_nop,     6,   0,    LNX, ET|RIGHT},
{"c",         "C",       pr_c,        sr_pcpu,    2,   0,    SUN, ET|RIGHT},
{"caught",    "CAUGHT",  pr_sigcatch, sr_nop,     9,   0,    BSD, TO|SIGNAL}, /*sigcatch*/
{"class",     "CLS",     pr_class,    sr_sched,   3,   0,    XXX, TO|LEFT},
{"cls",       "CLS",     pr_class,    sr_sched,   3,   0,    HPU, TO|RIGHT}, /*says HPUX or RT*/
{"cmaj_flt",  "-",       pr_nop,      sr_cmaj_flt, 1,  0,    LNX, AN|RIGHT},
{"cmd",       "CMD",     pr_args,     sr_cmd,    27, ARG,    DEC, PO|UNLIMITED}, /*ucomm*/
{"cmin_flt",  "-",       pr_nop,      sr_cmin_flt, 1,  0,    LNX, AN|RIGHT},
{"cnswap",    "-",       pr_nop,      sr_nop,     1,   0,    LNX, AN|RIGHT},
{"comm",      "COMMAND", pr_comm,     sr_cmd,    15, COM,    U98, PO|UNLIMITED}, /*ucomm*/
{"command",   "COMMAND", pr_args,     sr_cmd,    27, ARG,    XXX, PO|UNLIMITED}, /*args*/
{"context",   "CONTEXT", pr_context,  sr_nop,    31,   0,    LNX, ET|LEFT},
{"cp",        "CP",      pr_cp,       sr_pcpu,    3,   0,    DEC, ET|RIGHT}, /*cpu*/
{"cpu",       "CPU",     pr_nop,      sr_nop,     3,   0,    BSD, AN|RIGHT}, /* FIXME ... HP-UX wants this as the CPU number for SMP? */
{"cpuid",     "CPUID",   pr_psr,      sr_nop,     5,   0,    BSD, TO|RIGHT}, // OpenBSD: 8 wide!
{"cputime",   "TIME",    pr_time,     sr_nop,     8,   0,    DEC, ET|RIGHT}, /*time*/
{"cstime",    "-",       pr_nop,      sr_cstime,  1,   0,    LNX, AN|RIGHT},
{"ctid",      "CTID",    pr_nop,      sr_nop,     5,   0,    SUN, ET|RIGHT}, // resource contracts?
{"cursig",    "CURSIG",  pr_nop,      sr_nop,     6,   0,    DEC, AN|RIGHT},
{"cutime",    "-",       pr_nop,      sr_cutime,  1,   0,    LNX, AN|RIGHT},
{"cwd",       "CWD",     pr_nop,      sr_nop,     3,   0,    LNX, AN|LEFT},
{"drs",       "DRS",     pr_drs,      sr_drs,     5, MEM,    LNX, PO|RIGHT},
{"dsiz",      "DSIZ",    pr_dsiz,     sr_nop,     4,   0,    LNX, PO|RIGHT},
{"egid",      "EGID",    pr_egid,     sr_egid,    5,   0,    LNX, ET|RIGHT},
{"egroup",    "EGROUP",  pr_egroup,   sr_egroup,  8, GRP,    LNX, ET|USER},
{"eip",       "EIP",     pr_eip,      sr_kstk_eip, 8,  0,    LNX, TO|RIGHT},
{"emul",      "EMUL",    pr_nop,      sr_nop,    13,   0,    BSD, PO|LEFT}, /* "FreeBSD ELF32" and such */
{"end_code",  "E_CODE",  pr_nop,      sr_end_code, 8,  0,    LNx, PO|RIGHT},
{"environ","ENVIRONMENT",pr_nop,      sr_nop,    11, ENV,    LNx, PO|UNLIMITED},
{"esp",       "ESP",     pr_esp,      sr_kstk_esp, 8,  0,    LNX, TO|RIGHT},
{"etime",     "ELAPSED", pr_etime,    sr_nop,    11,   0,    U98, ET|RIGHT}, /* was 7 wide */
{"euid",      "EUID",    pr_euid,     sr_euid,    5,   0,    LNX, ET|RIGHT},
{"euser",     "EUSER",   pr_euser,    sr_euser,   8, USR,    LNX, ET|USER},
{"f",         "F",       pr_flag,     sr_flags,   1,   0,    XXX, ET|RIGHT}, /*flags*/
{"fgid",      "FGID",    pr_fgid,     sr_fgid,    5,   0,    LNX, ET|RIGHT},
{"fgroup",    "FGROUP",  pr_fgroup,   sr_fgroup,  8, GRP,    LNX, ET|USER},
{"flag",      "F",       pr_flag,     sr_flags,   1,   0,    DEC, ET|RIGHT},
{"flags",     "F",       pr_flag,     sr_flags,   1,   0,    BSD, ET|RIGHT}, /*f*/ /* was FLAGS, 8 wide */
{"fname",     "COMMAND", pr_fname,    sr_nop,     8,   0,    SUN, PO|LEFT},
{"fsgid",     "FSGID",   pr_fgid,     sr_fgid,    5,   0,    LNX, ET|RIGHT},
{"fsgroup",   "FSGROUP", pr_fgroup,   sr_fgroup,  8, GRP,    LNX, ET|USER},
{"fsuid",     "FSUID",   pr_fuid,     sr_fuid,    5,   0,    LNX, ET|RIGHT},
{"fsuser",    "FSUSER",  pr_fuser,    sr_fuser,   8, USR,    LNX, ET|USER},
{"fuid",      "FUID",    pr_fuid,     sr_fuid,    5,   0,    LNX, ET|RIGHT},
{"fuser",     "FUSER",   pr_fuser,    sr_fuser,   8, USR,    LNX, ET|USER},
{"gid",       "GID",     pr_egid,     sr_egid,    5,   0,    SUN, ET|RIGHT},
{"group",     "GROUP",   pr_egroup,   sr_egroup,  8, GRP,    U98, ET|USER},
{"ignored",   "IGNORED", pr_sigignore,sr_nop,     9,   0,    BSD, TO|SIGNAL}, /*sigignore*/
{"inblk",     "INBLK",   pr_nop,      sr_nop,     5,   0,    BSD, AN|RIGHT}, /*inblock*/
{"inblock",   "INBLK",   pr_nop,      sr_nop,     5,   0,    DEC, AN|RIGHT}, /*inblk*/
{"intpri",    "PRI",     pr_opri,     sr_priority, 3,  0,    HPU, TO|RIGHT},
{"jid",       "JID",     pr_nop,      sr_nop,     1,   0,    SGI, PO|RIGHT},
{"jobc",      "JOBC",    pr_nop,      sr_nop,     4,   0,    XXX, AN|RIGHT},
{"ktrace",    "KTRACE",  pr_nop,      sr_nop,     8,   0,    BSD, AN|RIGHT},
{"ktracep",   "KTRACEP", pr_nop,      sr_nop,     8,   0,    BSD, AN|RIGHT},
{"label",     "LABEL",   pr_context,  sr_nop,    31,  0,     SGI, ET|LEFT},
{"lastcpu",   "C",       pr_psr,      sr_nop,     3,   0,    BSD, TO|RIGHT}, // DragonFly
{"lim",       "LIM",     pr_lim,      sr_rss_rlim, 5,  0,    BSD, AN|RIGHT},
{"login",     "LOGNAME", pr_nop,      sr_nop,     8,   0,    BSD, AN|LEFT}, /*logname*/   /* double check */
{"logname",   "LOGNAME", pr_nop,      sr_nop,     8,   0,    XXX, AN|LEFT}, /*login*/
{"longtname", "TTY",     pr_tty8,     sr_tty,     8,   0,    DEC, PO|LEFT},
{"lstart",    "STARTED", pr_lstart,   sr_nop,    24,   0,    XXX, ET|RIGHT},
{"luid",      "LUID",    pr_nop,      sr_nop,     5,   0,    LNX, ET|RIGHT}, /* login ID */
{"luser",     "LUSER",   pr_nop,      sr_nop,     8, USR,    LNX, ET|USER}, /* login USER */
{"lwp",       "LWP",     pr_thread,   sr_tid,     5,   0,    SUN, TO|PIDMAX|RIGHT},
{"m_drs",     "DRS",     pr_drs,      sr_drs,     5, MEM,    LNx, PO|RIGHT},
{"m_dt",      "DT",      pr_nop,      sr_dt,      4, MEM,    LNx, PO|RIGHT},
{"m_lrs",     "LRS",     pr_nop,      sr_lrs,     5, MEM,    LNx, PO|RIGHT},
{"m_resident", "RES",    pr_nop,      sr_resident, 5,MEM,    LNx, PO|RIGHT},
{"m_share",   "SHRD",    pr_nop,      sr_share,   5, MEM,    LNx, PO|RIGHT},
{"m_size",    "SIZE",    pr_size,     sr_size,    5, MEM,    LNX, PO|RIGHT},
{"m_swap",    "SWAP",    pr_nop,      sr_nop,     5,   0,    LNx, PO|RIGHT},
{"m_trs",     "TRS",     pr_trs,      sr_trs,     5, MEM,    LNx, PO|RIGHT},
{"maj_flt",   "MAJFL",   pr_majflt,   sr_maj_flt, 6,   0,    LNX, AN|RIGHT},
{"majflt",    "MAJFLT",  pr_majflt,   sr_maj_flt, 6,   0,    XXX, AN|RIGHT},
{"min_flt",   "MINFL",   pr_minflt,   sr_min_flt, 6,   0,    LNX, AN|RIGHT},
{"minflt",    "MINFLT",  pr_minflt,   sr_min_flt, 6,   0,    XXX, AN|RIGHT},
{"msgrcv",    "MSGRCV",  pr_nop,      sr_nop,     6,   0,    XXX, AN|RIGHT},
{"msgsnd",    "MSGSND",  pr_nop,      sr_nop,     6,   0,    XXX, AN|RIGHT},
{"mwchan",    "MWCHAN",  pr_nop,      sr_nop,     6, WCH,    BSD, TO|WCHAN}, /* mutex (FreeBSD) */
{"ni",        "NI",      pr_nice,     sr_nice,    3,   0,    BSD, TO|RIGHT}, /*nice*/
{"nice",      "NI",      pr_nice,     sr_nice,    3,   0,    U98, TO|RIGHT}, /*ni*/
{"nivcsw",    "IVCSW",   pr_nop,      sr_nop,     5,   0,    XXX, AN|RIGHT},
{"nlwp",      "NLWP",    pr_nlwp,     sr_nlwp,    4,   0,    SUN, PO|RIGHT},
{"nsignals",  "NSIGS",   pr_nop,      sr_nop,     5,   0,    DEC, AN|RIGHT}, /*nsigs*/
{"nsigs",     "NSIGS",   pr_nop,      sr_nop,     5,   0,    BSD, AN|RIGHT}, /*nsignals*/
{"nswap",     "NSWAP",   pr_nop,      sr_nop,     5,   0,    XXX, AN|RIGHT},
{"nvcsw",     "VCSW",    pr_nop,      sr_nop,     5,   0,    XXX, AN|RIGHT},
{"nwchan",    "WCHAN",   pr_nwchan,   sr_nop,     6,   0,    XXX, TO|RIGHT},
{"opri",      "PRI",     pr_opri,     sr_priority, 3,  0,    SUN, TO|RIGHT},
{"osz",       "SZ",      pr_nop,      sr_nop,     2,   0,    SUN, PO|RIGHT},
{"oublk",     "OUBLK",   pr_nop,      sr_nop,     5,   0,    BSD, AN|RIGHT}, /*oublock*/
{"oublock",   "OUBLK",   pr_nop,      sr_nop,     5,   0,    DEC, AN|RIGHT}, /*oublk*/
{"p_ru",      "P_RU",    pr_nop,      sr_nop,     6,   0,    BSD, AN|RIGHT},
{"paddr",     "PADDR",   pr_nop,      sr_nop,     6,   0,    BSD, AN|RIGHT},
{"pagein",    "PAGEIN",  pr_majflt,   sr_maj_flt, 6,   0,    XXX, AN|RIGHT},
{"pcpu",      "%CPU",    pr_pcpu,     sr_pcpu,    4,   0,    U98, ET|RIGHT}, /*%cpu*/
{"pending",   "PENDING", pr_sig,      sr_nop,     9,   0,    BSD, ET|SIGNAL}, /*sig*/
{"pgid",      "PGID",    pr_pgid,     sr_pgrp,    5,   0,    U98, PO|PIDMAX|RIGHT},
{"pgrp",      "PGRP",    pr_pgid,     sr_pgrp,    5,   0,    LNX, PO|PIDMAX|RIGHT},
{"pid",       "PID",     pr_pid,      sr_tgid,    5,   0,    U98, PO|PIDMAX|RIGHT},
{"pmem",      "%MEM",    pr_pmem,     sr_nop,     4,   0,    XXX, PO|RIGHT}, /*%mem*/
{"poip",      "-",       pr_nop,      sr_nop,     1,   0,    BSD, AN|RIGHT},
{"policy",    "POL",     pr_class,    sr_sched,   3,   0,    DEC, TO|LEFT},
{"ppid",      "PPID",    pr_ppid,     sr_ppid,    5,   0,    U98, PO|PIDMAX|RIGHT},
{"pri",       "PRI",     pr_pri,      sr_nop,     3,   0,    XXX, TO|RIGHT},
{"pri_api",   "API",     pr_pri_api,  sr_nop,     3,   0,    LNX, TO|RIGHT},
{"pri_bar",   "BAR",     pr_pri_bar,  sr_nop,     3,   0,    LNX, TO|RIGHT},
{"pri_baz",   "BAZ",     pr_pri_baz,  sr_nop,     3,   0,    LNX, TO|RIGHT},
{"pri_foo",   "FOO",     pr_pri_foo,  sr_nop,     3,   0,    LNX, TO|RIGHT},
{"priority",  "PRI",     pr_priority, sr_priority, 3,  0,    LNX, TO|RIGHT},
{"prmgrp",    "PRMGRP",  pr_nop,      sr_nop,    12,   0,    HPU, PO|RIGHT},
{"prmid",     "PRMID",   pr_nop,      sr_nop,    12,   0,    HPU, PO|RIGHT},
{"project",   "PROJECT", pr_nop,      sr_nop,    12,   0,    SUN, PO|LEFT}, // see prm* andctid
{"projid",    "PROJID",  pr_nop,      sr_nop,     5,   0,    SUN, PO|RIGHT},
{"pset",      "PSET",    pr_nop,      sr_nop,     4,   0,    DEC, TO|RIGHT},
{"psr",       "PSR",     pr_psr,      sr_nop,     3,   0,    DEC, TO|RIGHT},
{"psxpri",    "PPR",     pr_nop,      sr_nop,     3,   0,    DEC, TO|RIGHT},
{"re",        "RE",      pr_nop,      sr_nop,     3,   0,    BSD, AN|RIGHT},
{"resident",  "RES",     pr_nop,      sr_resident, 5,MEM,    LNX, PO|RIGHT},
{"rgid",      "RGID",    pr_rgid,     sr_rgid,    5,   0,    XXX, ET|RIGHT},
{"rgroup",    "RGROUP",  pr_rgroup,   sr_rgroup,  8, GRP,    U98, ET|USER}, /* was 8 wide */
{"rlink",     "RLINK",   pr_nop,      sr_nop,     8,   0,    BSD, AN|RIGHT},
{"rss",       "RSS",     pr_rss,      sr_rss,     5,   0,    XXX, PO|RIGHT}, /* was 5 wide */
{"rssize",    "RSS",     pr_rss,      sr_vm_rss,  5,   0,    DEC, PO|RIGHT}, /*rsz*/
{"rsz",       "RSZ",     pr_rss,      sr_vm_rss,  5,   0,    BSD, PO|RIGHT}, /*rssize*/
{"rtprio",    "RTPRIO",  pr_rtprio,   sr_rtprio,  6,   0,    BSD, TO|RIGHT},
{"ruid",      "RUID",    pr_ruid,     sr_ruid,    5,   0,    XXX, ET|RIGHT},
{"ruser",     "RUSER",   pr_ruser,    sr_ruser,   8, USR,    U98, ET|USER},
{"s",         "S",       pr_s,        sr_state,   1,   0,    SUN, TO|LEFT}, /*stat,state*/
{"sched",     "SCH",     pr_sched,    sr_sched,   3,   0,    AIX, TO|RIGHT},
{"scnt",      "SCNT",    pr_nop,      sr_nop,     4,   0,    DEC, AN|RIGHT},  /* man page misspelling of scount? */
{"scount",    "SC",      pr_nop,      sr_nop,     4,   0,    AIX, AN|RIGHT},  /* scnt==scount, DEC claims both */
{"sess",      "SESS",    pr_sess,     sr_session, 5,   0,    XXX, PO|PIDMAX|RIGHT},
{"session",   "SESS",    pr_sess,     sr_session, 5,   0,    LNX, PO|PIDMAX|RIGHT},
{"sgi_p",     "P",       pr_sgi_p,    sr_nop,     1,   0,    LNX, TO|RIGHT}, /* "cpu" number */
{"sgi_rss",   "RSS",     pr_rss,      sr_nop,     4,   0,    LNX, PO|LEFT}, /* SZ:RSS */
{"sgid",      "SGID",    pr_sgid,     sr_sgid,    5,   0,    LNX, ET|RIGHT},
{"sgroup",    "SGROUP",  pr_sgroup,   sr_sgroup,  8, GRP,    LNX, ET|USER},
{"share",     "-",       pr_nop,      sr_share,   1, MEM,    LNX, PO|RIGHT},
{"sid",       "SID",     pr_sess,     sr_session, 5,   0,    XXX, PO|PIDMAX|RIGHT}, /* Sun & HP */
{"sig",       "PENDING", pr_sig,      sr_nop,     9,   0,    XXX, ET|SIGNAL}, /*pending -- Dragonfly uses this for whole-proc and "tsig" for thread */
{"sig_block", "BLOCKED",  pr_sigmask, sr_nop,     9,   0,    LNX, TO|SIGNAL},
{"sig_catch", "CATCHED", pr_sigcatch, sr_nop,     9,   0,    LNX, TO|SIGNAL},
{"sig_ignore", "IGNORED",pr_sigignore, sr_nop,    9,   0,    LNX, TO|SIGNAL},
{"sig_pend",  "SIGNAL",   pr_sig,     sr_nop,     9,   0,    LNX, ET|SIGNAL},
{"sigcatch",  "CAUGHT",  pr_sigcatch, sr_nop,     9,   0,    XXX, TO|SIGNAL}, /*caught*/
{"sigignore", "IGNORED", pr_sigignore,sr_nop,     9,   0,    XXX, TO|SIGNAL}, /*ignored*/
{"sigmask",   "BLOCKED", pr_sigmask,  sr_nop,     9,   0,    XXX, TO|SIGNAL}, /*blocked*/
{"size",      "SZ",      pr_swapable, sr_swapable, 5,  0,    SCO, PO|RIGHT},
{"sl",        "SL",      pr_nop,      sr_nop,     3,   0,    XXX, AN|RIGHT},
{"spid",      "SPID",    pr_thread,   sr_tid,     5,   0,    SGI, TO|PIDMAX|RIGHT},
{"stackp",    "STACKP",  pr_stackp,   sr_start_stack, 8, 0,  LNX, PO|RIGHT}, /*start_stack*/
{"start",     "STARTED", pr_start,    sr_nop,     8,   0,    XXX, ET|RIGHT},
{"start_code", "S_CODE",  pr_nop,     sr_start_code, 8, 0,   LNx, PO|RIGHT},
{"start_stack", "STACKP", pr_stackp,  sr_start_stack, 8, 0,  LNX, PO|RIGHT}, /*stackp*/
{"start_time", "START",  pr_stime,    sr_start_time, 5, 0,   LNx, ET|RIGHT},
{"stat",      "STAT",    pr_stat,     sr_state,   4,   0,    BSD, TO|LEFT}, /*state,s*/
{"state",     "S",       pr_s,        sr_state,   1,   0,    XXX, TO|LEFT}, /*stat,s*/ /* was STAT */
{"status",    "STATUS",  pr_nop,      sr_nop,     6,   0,    DEC, AN|RIGHT},
{"stime",     "STIME",   pr_stime,    sr_stime,   5,   0,    XXX, ET|RIGHT}, /* was 6 wide */
{"suid",      "SUID",    pr_suid,     sr_suid,    5,   0,    LNx, ET|RIGHT},
{"suser",     "SUSER",   pr_suser,    sr_suser,   8, USR,    LNx, ET|USER},
{"svgid",     "SVGID",   pr_sgid,     sr_sgid,    5,   0,    XXX, ET|RIGHT},
{"svgroup",   "SVGROUP", pr_sgroup,   sr_sgroup,  8, GRP,    LNX, ET|USER},
{"svuid",     "SVUID",   pr_suid,     sr_suid,    5,   0,    XXX, ET|RIGHT},
{"svuser",    "SVUSER",  pr_suser,    sr_suser,   8, USR,    LNX, ET|USER},
{"systime",   "SYSTEM",  pr_nop,      sr_nop,     6,   0,    DEC, ET|RIGHT},
{"sz",        "SZ",      pr_sz,       sr_nop,     5,   0,    HPU, PO|RIGHT},
{"taskid",    "TASKID",  pr_nop,      sr_nop,     5,   0,    SUN, TO|PIDMAX|RIGHT}, // is this a thread ID?
{"tdev",      "TDEV",    pr_nop,      sr_nop,     4,   0,    XXX, AN|RIGHT},
{"thcount",   "THCNT",   pr_nlwp,     sr_nlwp,    5,   0,    AIX, PO|RIGHT},
{"tid",       "TID",     pr_thread,   sr_tid,     5,   0,    AIX, TO|PIDMAX|RIGHT},
{"time",      "TIME",    pr_time,     sr_nop,     8,   0,    U98, ET|RIGHT}, /*cputime*/ /* was 6 wide */
{"timeout",   "TMOUT",   pr_nop,      sr_nop,     5,   0,    LNX, AN|RIGHT}, // 2.0.xx era
{"tmout",     "TMOUT",   pr_nop,      sr_nop,     5,   0,    LNX, AN|RIGHT}, // 2.0.xx era
{"tname",     "TTY",     pr_tty8,     sr_tty,     8,   0,    DEC, PO|LEFT},
{"tpgid",     "TPGID",   pr_tpgid,    sr_tpgid,   5,   0,    XXX, PO|PIDMAX|RIGHT},
{"trs",       "TRS",     pr_trs,      sr_trs,     4, MEM,    AIX, PO|RIGHT},
{"trss",      "TRSS",    pr_trs,      sr_trs,     4, MEM,    BSD, PO|RIGHT}, /* 4.3BSD NET/2 */
{"tsess",     "TSESS",   pr_nop,      sr_nop,     5,   0,    BSD, PO|PIDMAX|RIGHT},
{"tsession",  "TSESS",   pr_nop,      sr_nop,     5,   0,    DEC, PO|PIDMAX|RIGHT},
{"tsid",      "TSID",    pr_nop,      sr_nop,     5,   0,    BSD, PO|PIDMAX|RIGHT},
{"tsig",      "PENDING", pr_tsig,     sr_nop,     9,   0,    BSD, ET|SIGNAL}, /* Dragonfly used this for thread-specific, and "sig" for whole-proc */
{"tsiz",      "TSIZ",    pr_tsiz,     sr_nop,     4,   0,    BSD, PO|RIGHT},
{"tt",        "TT",      pr_tty8,     sr_tty,     8,   0,    BSD, PO|LEFT},
{"tty",       "TT",      pr_tty8,     sr_tty,     8,   0,    U98, PO|LEFT}, /* Unix98 requires "TT" but has "TTY" too. :-( */  /* was 3 wide */
{"tty4",      "TTY",     pr_tty4,     sr_tty,     4,   0,    LNX, PO|LEFT},
{"tty8",      "TTY",     pr_tty8,     sr_tty,     8,   0,    LNX, PO|LEFT},
{"u_procp",   "UPROCP",  pr_nop,      sr_nop,     6,   0,    DEC, AN|RIGHT},
{"ucmd",      "CMD",     pr_comm,     sr_cmd,    15, COM,    DEC, PO|UNLIMITED}, /*ucomm*/
{"ucomm",     "COMMAND", pr_comm,     sr_cmd,    15, COM,    XXX, PO|UNLIMITED}, /*comm*/
{"uid",       "UID",     pr_euid,     sr_euid,    5,   0,    XXX, ET|RIGHT},
{"uid_hack",  "UID",     pr_euser,    sr_euser,   8, USR,    XXX, ET|USER},
{"umask",     "UMASK",   pr_nop,      sr_nop,     5,   0,    DEC, AN|RIGHT},
{"uname",     "USER",    pr_euser,    sr_euser,   8, USR,    DEC, ET|USER}, /* man page misspelling of user? */
{"upr",       "UPR",     pr_nop,      sr_nop,     3,   0,    BSD, TO|RIGHT}, /*usrpri*/
{"uprocp",    "UPROCP",  pr_nop,      sr_nop,     8,   0,    BSD, AN|RIGHT},
{"user",      "USER",    pr_euser,    sr_euser,   8, USR,    U98, ET|USER}, /* BSD n forces this to UID */
{"usertime",  "USER",    pr_nop,      sr_nop,     4,   0,    DEC, ET|RIGHT},
{"usrpri",    "UPR",     pr_nop,      sr_nop,     3,   0,    DEC, TO|RIGHT}, /*upr*/
{"util",      "C",       pr_c,        sr_pcpu,    2,   0,    SGI, ET|RIGHT}, // not sure about "C"
{"utime",     "UTIME",   pr_nop,      sr_utime,   6,   0,    LNx, ET|RIGHT},
{"vm_data",   "DATA",    pr_nop,      sr_vm_data, 5,   0,    LNx, PO|RIGHT},
{"vm_exe",    "EXE",     pr_nop,      sr_vm_exe,  5,   0,    LNx, PO|RIGHT},
{"vm_lib",    "LIB",     pr_nop,      sr_vm_lib,  5,   0,    LNx, PO|RIGHT},
{"vm_lock",   "LCK",     pr_nop,      sr_vm_lock, 3,   0,    LNx, PO|RIGHT},
{"vm_stack",  "STACK",   pr_nop,      sr_vm_stack, 5,  0,    LNx, PO|RIGHT},
{"vsize",     "VSZ",     pr_vsz,      sr_vsize,   6,   0,    DEC, PO|RIGHT}, /*vsz*/
{"vsz",       "VSZ",     pr_vsz,      sr_vm_size, 6,   0,    U98, PO|RIGHT}, /*vsize*/
{"wchan",     "WCHAN",   pr_wchan,    sr_wchan,   6, WCH,    XXX, TO|WCHAN}, /* BSD n forces this to nwchan */ /* was 10 wide */
{"wname",     "WCHAN",   pr_wname,    sr_nop,     6, WCH,    SGI, TO|WCHAN}, /* opposite of nwchan */
{"xstat",     "XSTAT",   pr_nop,      sr_nop,     5,   0,    BSD, AN|RIGHT},
{"zone",      "ZONE",    pr_context,  sr_nop,    31,   0,    SUN, ET|LEFT}, // Solaris zone == Linux context?
{"zoneid",    "ZONEID",  pr_nop,      sr_nop,    31,   0,    SUN, ET|RIGHT},// Linux only offers context names
{"~",         "-",       pr_nop,      sr_nop,     1,   0,    LNX, AN|RIGHT}  /* NULL would ruin alphabetical order */
};

#undef USER
#undef LEFT
#undef RIGHT
#undef UNLIMITED
#undef WCHAN
#undef SIGNAL
#undef PIDMAX
#undef PO
#undef TO
#undef AN
#undef ET

static const int format_array_count = sizeof(format_array)/sizeof(format_struct);


/****************************** Macro formats *******************************/
/* First X field may be NR, which is p->start_code>>26 printed with %2ld */
/* That seems useless though, and Debian already killed it. */
/* The ones marked "Digital" have the name defined, not just the data. */
static const macro_struct macro_array[] = {
{"DFMT",     "pid,tname,state,cputime,cmd"},         /* Digital's default */
{"DefBSD",   "pid,tname,stat,bsdtime,args"},               /* Our BSD default */
{"DefSysV",  "pid,tname,time,cmd"},                     /* Our SysV default */
{"END_BSD",  "state,tname,cputime,comm"},                 /* trailer for O */
{"END_SYS5", "state,tname,time,command"},                 /* trailer for -O */
{"F5FMT",    "uname,pid,ppid,c,start,tname,time,cmd"},       /* Digital -f */

{"FB_",      "pid,tt,stat,time,command"},                          /* FreeBSD default */
{"FB_j",     "user,pid,ppid,pgid,sess,jobc,stat,tt,time,command"},     /* FreeBSD j */
{"FB_l",     "uid,pid,ppid,cpu,pri,nice,vsz,rss,wchan,stat,tt,time,command"},   /* FreeBSD l */
{"FB_u",     "user,pid,pcpu,pmem,vsz,rss,tt,stat,start,time,command"},     /* FreeBSD u */
{"FB_v",     "pid,stat,time,sl,re,pagein,vsz,rss,lim,tsiz,pcpu,pmem,command"},   /* FreeBSD v */

{"FD_",      "pid,tty,time,comm"},                                 /* Fictional Debian SysV default */
{"FD_f",     "user,pid,ppid,start_time,tty,time,comm"},                /* Fictional Debian -f */
{"FD_fj",    "user,pid,ppid,start_time,tty,time,pgid,sid,comm"},        /* Fictional Debian -jf */
{"FD_j",     "pid,tty,time,pgid,sid,comm"},                                  /* Fictional Debian -j */
{"FD_l",     "flags,state,uid,pid,ppid,priority,nice,vsz,wchan,tty,time,comm"},    /* Fictional Debian -l */
{"FD_lj",    "flags,state,uid,pid,ppid,priority,nice,vsz,wchan,tty,time,pgid,sid,comm"}, /* Fictional Debian -jl */

{"FL5FMT",   "f,state,uid,pid,ppid,pcpu,pri,nice,rss,wchan,start,time,command"},  /* Digital -fl */

{"FLASK_context",   "pid,context,command"},  /* Flask Linux context, --context */

{"HP_",      "pid,tty,time,comm"},  /* HP default */
{"HP_f",     "user,pid,ppid,cpu,stime,tty,time,args"},  /* HP -f */
{"HP_fl",    "flags,state,user,pid,ppid,cpu,intpri,nice,addr,sz,wchan,stime,tty,time,args"},  /* HP -fl */
{"HP_l",     "flags,state,uid,pid,ppid,cpu,intpri,nice,addr,sz,wchan,tty,time,comm"},  /* HP -l */

{"J390",     "pid,sid,pgrp,tname,atime,args"},   /* OS/390 -j */
{"JFMT",     "user,pid,ppid,pgid,sess,jobc,state,tname,cputime,command"},   /* Digital j and -j */
{"L5FMT",    "f,state,uid,pid,ppid,c,pri,nice,addr,sz,wchan,tt,time,ucmd"},   /* Digital -l */
{"LFMT",     "uid,pid,ppid,cp,pri,nice,vsz,rss,wchan,state,tname,cputime,command"},   /* Digital l */

{"OL_X",     "pid,start_stack,esp,eip,timeout,alarm,stat,tname,bsdtime,args"},      /* Old i386 Linux X */
{"OL_j",     "ppid,pid,pgid,sid,tname,tpgid,stat,uid,bsdtime,args"},                   /* Old Linux j */
{"OL_l",     "flags,uid,pid,ppid,priority,nice,vsz,rss,wchan,stat,tname,bsdtime,args"},     /* Old Linux l */
{"OL_m",     "pid,tname,majflt,minflt,m_trs,m_drs,m_size,m_swap,rss,m_share,vm_lib,m_dt,args"}, /* Old Linux m */
{"OL_s",     "uid,pid,pending,sig_block,sig_ignore,caught,stat,tname,bsdtime,args"},  /* Old Linux s */
{"OL_u",     "user,pid,pcpu,pmem,vsz,rss,tname,stat,start_time,bsdtime,args"},       /* Old Linux u */
{"OL_v",     "pid,tname,stat,bsdtime,maj_flt,m_trs,m_drs,rss,pmem,args"},            /* Old Linux v */

{"RD_",      "pid,tname,state,bsdtime,comm"},                                       /* Real Debian default */
{"RD_f",     "uid,pid,ppid,start_time,tname,bsdtime,args"},                         /* Real Debian -f */
{"RD_fj",    "uid,pid,ppid,start_time,tname,bsdtime,pgid,sid,args"},                /* Real Debian -jf */
{"RD_j",     "pid,tname,state,bsdtime,pgid,sid,comm"},                               /* Real Debian -j */
{"RD_l",     "flags,state,uid,pid,ppid,priority,nice,wchan,tname,bsdtime,comm"},           /* Real Debian -l */
{"RD_lj",    "flags,state,uid,pid,ppid,priority,nice,wchan,tname,bsdtime,pgid,sid,comm"},  /* Real Debian -jl */

{"RUSAGE",   "minflt,majflt,nswap,inblock,oublock,msgsnd,msgrcv,nsigs,nvcsw,nivcsw"}, /* Digital -o "RUSAGE" */
{"SCHED",    "user,pcpu,pri,usrpri,nice,psxpri,psr,policy,pset"},                /* Digital -o "SCHED" */
{"SFMT",     "uid,pid,cursig,sig,sigmask,sigignore,sigcatch,stat,tname,command"},  /* Digital s */

{"Std_f",    "uid_hack,pid,ppid,c,stime,tname,time,cmd"},                     /* new -f */
{"Std_fl",   "f,s,uid_hack,pid,ppid,c,opri,ni,addr,sz,wchan,stime,tname,time,cmd"}, /* -fl */
{"Std_l",    "f,s,uid,pid,ppid,c,opri,ni,addr,sz,wchan,tname,time,ucmd"},  /* new -l */

{"THREAD",   "user,pcpu,pri,scnt,wchan,usertime,systime"},                /* Digital -o "THREAD" */
{"UFMT",     "uname,pid,pcpu,pmem,vsz,rss,tt,state,start,time,command"},   /* Digital u */
{"VFMT",     "pid,tt,state,time,sl,pagein,vsz,rss,pcpu,pmem,command"},   /* Digital v */
{"~", "~"} /* NULL would ruin alphabetical order */
};

static const int macro_array_count = sizeof(macro_array)/sizeof(macro_struct);


/*************************** AIX formats ********************/
/* Convert AIX format codes to normal format specifiers. */
static const aix_struct aix_array[] = {
{'C', "pcpu",   "%CPU"},
{'G', "group",  "GROUP"},
{'P', "ppid",   "PPID"},
{'U', "user",   "USER"},
{'a', "args",   "COMMAND"},
{'c', "comm",   "COMMAND"},
{'g', "rgroup", "RGROUP"},
{'n', "nice",   "NI"},
{'p', "pid",    "PID"},
{'r', "pgid",   "PGID"},
{'t', "etime",  "ELAPSED"},
{'u', "ruser",  "RUSER"},
{'x', "time",   "TIME"},
{'y', "tty",    "TTY"},
{'z', "vsz",    "VSZ"},
{'~', "~",      "~"} /* NULL would ruin alphabetical order */
};
static const int aix_array_count = sizeof(aix_array)/sizeof(aix_struct);


/********************* sorting ***************************/
/* Convert short sorting codes to normal format specifiers. */
static const shortsort_struct shortsort_array[] = {
{'C', "pcpu"       },
{'G', "tpgid"      },
{'J', "cstime"     },
/* {'K', "stime"      }, */  /* conflict, system vs. start time */
{'M', "maj_flt"    },
{'N', "cmaj_flt"   },
{'P', "ppid"       },
{'R', "resident"   },
{'S', "share"      },
{'T', "start_time" },
{'U', "uid"        }, /* euid */
{'c', "cmd"        },
{'f', "flags"      },
{'g', "pgrp"       },
{'j', "cutime"     },
{'k', "utime"      },
{'m', "min_flt"    },
{'n', "cmin_flt"   },
{'o', "session"    },
{'p', "pid"        },
{'r', "rss"        },
{'s', "size"       },
{'t', "tty"        },
{'u', "user"       },
{'v', "vsize"      },
{'y', "priority"   }, /* nice */
{'~', "~"          } /* NULL would ruin alphabetical order */
};
static const int shortsort_array_count = sizeof(shortsort_array)/sizeof(shortsort_struct);


/*********** print format_array **********/
/* called by the parser in another file */
void print_format_specifiers(void){
  const format_struct *walk = format_array;
  while(*(walk->spec) != '~'){
    if(walk->pr != pr_nop) printf("%-12.12s %-8.8s\n", walk->spec, walk->head);
    walk++;
  }
}

/************ comparison functions for bsearch *************/

static int compare_format_structs(const void *a, const void *b){
  return strcmp(((const format_struct*)a)->spec,((const format_struct*)b)->spec);
}

static int compare_macro_structs(const void *a, const void *b){
  return strcmp(((const macro_struct*)a)->spec,((const macro_struct*)b)->spec);
}

/******** look up structs as needed by the sort & format parsers ******/

const shortsort_struct *search_shortsort_array(const int findme){
  const shortsort_struct *walk = shortsort_array;
  while(walk->desc != '~'){
    if(walk->desc == findme) return walk;
    walk++;
  }
  return NULL;
}

const aix_struct *search_aix_array(const int findme){
  const aix_struct *walk = aix_array;
  while(walk->desc != '~'){
    if(walk->desc == findme) return walk;
    walk++;
  }
  return NULL;
}

const format_struct *search_format_array(const char *findme){
  format_struct key;
  key.spec = findme;
  return bsearch(&key, format_array, format_array_count,
    sizeof(format_struct), compare_format_structs
  );
}

const macro_struct *search_macro_array(const char *findme){
  macro_struct key;
  key.spec = findme;
  return bsearch(&key, macro_array, macro_array_count,
    sizeof(macro_struct), compare_macro_structs
  );
}

static unsigned int active_cols;  /* some multiple of screen_cols */

/***** Last chance, avoid needless trunctuation. */
static void check_header_width(void){
  format_node *walk = format_list;
  unsigned int total = 0;
  int was_normal = 0;
  unsigned int i = 0;
  unsigned int sigs = 0;
  while(walk){
    switch((walk->flags) & CF_JUST_MASK){
    default:
      total += walk->width;
      total += was_normal;
      was_normal = 1;
      break;
    case CF_SIGNAL:
      sigs++;
      total += walk->width;
      total += was_normal;
      was_normal = 1;
      break;
    case CF_UNLIMITED:  /* could chop this a bit */
      if(walk->next) total += walk->width;
      else total += 3; /* not strlen(walk->name) */
      total += was_normal;
      was_normal = 1;
      break;
    case 0:  /* AIX */
      total += walk->width;
      was_normal = 0;
      break;
    }
    walk = walk->next;
  }
  for(;;){
    i++;
    active_cols = screen_cols * i;
    if(active_cols>=total) break;
    if(screen_cols*i >= OUTBUF_SIZE/2) break; /* can't go over */
  }
  wide_signals = (total+sigs*7 <= active_cols);
}


/********** show one process (NULL proc prints header) **********/

//#define SPACE_AMOUNT page_size
#define SPACE_AMOUNT 144

static char *saved_outbuf;

void show_one_proc(const proc_t *restrict const p, const format_node *restrict fmt){
  /* unknown: maybe set correct & actual to 1, remove +/- 1 below */
  int correct  = 0;  /* screen position we should be at */
  int actual   = 0;  /* screen position we are at */
  int amount   = 0;  /* amount of text that this data is */
  int leftpad  = 0;  /* amount of space this column _could_ need */
  int space    = 0;  /* amount of space we actually need to print */
  int dospace  = 0;  /* previous column determined that we need a space */
  int legit    = 0;  /* legitimately stolen extra space */
  int sz       = 0;  /* real size of data in outbuffer */
  int tmpspace = 0;
  char *restrict const outbuf = saved_outbuf;
  static int did_stuff = 0;  /* have we ever printed anything? */

  if(unlikely(-1==(long)p)){    /* true only once, at the end */
    if(did_stuff) return;
    /* have _never_ printed anything, but might need a header */
    if(!--lines_to_next_header){
      lines_to_next_header = header_gap;
      show_one_proc(NULL,fmt);
    }
    /* fprintf(stderr, "No processes available.\n"); */  /* legal? */
    exit(1);
  }
  if(likely(p)){  /* not header, maybe we should call ourselves for it */
    if(unlikely(!--lines_to_next_header)){
      lines_to_next_header = header_gap;
      show_one_proc(NULL,fmt);
    }
  }
  did_stuff = 1;
  if(unlikely(active_cols>(int)OUTBUF_SIZE)) fprintf(stderr,"Fix bigness error.\n");

  /* print row start sequence */
  for(;;){
    legit = 0;
    /* set width suggestion which might be ignored */
//    if(likely(fmt->next)) max_rightward = fmt->width;
//    else max_rightward = active_cols-((correct>actual) ? correct : actual);

    if(likely(fmt->next)){
      max_rightward = fmt->width;
      tmpspace = 0;
    }else{
      tmpspace = correct-actual;
      if (tmpspace<1){
        tmpspace = dospace;
        max_rightward = active_cols-actual-tmpspace;
      }else{
	max_rightward = active_cols - ( (correct>actual) ? correct : actual );
      }
    }
    max_leftward  = fmt->width + actual - correct; /* TODO check this */
    
//    fprintf(stderr, "cols: %d, max_rightward: %d, max_leftward: %d, actual: %d, correct: %d\n",
//		    active_cols, max_rightward, max_leftward, actual, correct);
    
    /* prepare data and calculate leftpad */
    if(likely(p) && likely(fmt->pr)) amount = (*fmt->pr)(outbuf,p);
    else amount = strlen(strcpy(outbuf, fmt->name)); /* AIX or headers */
    
    switch((fmt->flags) & CF_JUST_MASK){
    case 0:  /* for AIX, assigned outside this file */
      leftpad = 0;
      break;
    case CF_LEFT:          /* bad */
      leftpad = 0;
      break;
    case CF_RIGHT:     /* OK */
      leftpad = fmt->width - amount;
      if(leftpad < 0) leftpad = 0;
      break;
    case CF_SIGNAL:
      /* if the screen is wide enough, use full 16-character output */
      if(wide_signals){
        leftpad = 16 - amount;
        legit = 7;
      }else{
        leftpad =  9 - amount;
      }
      if(leftpad < 0) leftpad = 0;
      break;
    case CF_USER:       /* bad */
      leftpad = fmt->width - amount;
      if(leftpad < 0) leftpad = 0;
      if(!user_is_number) leftpad = 0;
      break;
    case CF_WCHAN:       /* bad */
      if(wchan_is_number){
        leftpad = fmt->width - amount;
        if(leftpad < 0) leftpad = 0;
        break;
      }else{
        if ((active_cols-actual-tmpspace)<1)
          outbuf[1] = '\0';  /* oops, we (mostly) lose this column... */
        leftpad = 0;
        break;
      }
    case CF_UNLIMITED:
    {
      if(active_cols-actual-tmpspace < 1)
        outbuf[1] = '\0';    /* oops, we (mostly) lose this column... */
      leftpad = 0;
      break;
    }
    default:
      fprintf(stderr, "bad alignment code\n");
      break;
    }
    /* At this point:
     *
     * correct   from previous column
     * actual    from previous column
     * amount    not needed (garbage due to chopping)
     * leftpad   left padding for this column alone (not make-up or gap)
     * space     not needed (will recalculate now)
     * dospace   if we require space between this and the prior column
     * legit     space we were allowed to steal, and thus did steal
     */
    space = correct - actual + leftpad;
    if(space<1) space=dospace;
    if(unlikely(space>SPACE_AMOUNT)) space=SPACE_AMOUNT;  // only so much available

    /* real size -- don't forget in 'amount' is number of cells */
    sz = strlen(outbuf);
    
    /* print data, set x position stuff */
    if(unlikely(!fmt->next)){
      /* Last column. Write padding + data + newline all together. */
      outbuf[sz] = '\n';
      fwrite(outbuf-space, space+sz+1, 1, stdout);
      break;
    }
    /* Not the last column. Write padding + data together. */
    fwrite(outbuf-space, space+sz, 1, stdout);
    actual  += space+amount;
    correct += fmt->width;
    correct += legit;        /* adjust for SIGNAL expansion */
    if(fmt->pr && fmt->next->pr){ /* neither is AIX filler */
      correct++;
      dospace = 1;
    }else{
      dospace = 0;
    }
    fmt = fmt->next;
    /* At this point:
     *
     * correct   screen position we should be at
     * actual    screen position we are at
     * amount    not needed
     * leftpad   not needed
     * space     not needed
     * dospace   if have determined that we need a space next time
     * legit     not needed
     */
  }
}


#ifdef TESTING
static void sanity_check(void){
  format_struct *fs = format_array;
  while((fs->spec)[0] != '~'){
    if(strlen(fs->head) > fs->width) printf("%d %s\n",strlen(fs->head),fs->spec);
    fs++;
  }
}
#endif


void init_output(void){
  int outbuf_pages;
  char *outbuf;

  switch(page_size){
  case 65536: page_shift = 16; break;
  case 32768: page_shift = 15; break;
  case 16384: page_shift = 14; break;
  case  8192: page_shift = 13; break;
  default: fprintf(stderr, "Unknown page size! (assume 4096)\n");
  case  4096: page_shift = 12; break;
  case  2048: page_shift = 11; break;
  case  1024: page_shift = 10; break;
  }

  // add page_size-1 to round up
  outbuf_pages = (OUTBUF_SIZE+SPACE_AMOUNT+page_size-1)/page_size;
  outbuf = mmap(
    0,
    page_size * (outbuf_pages+1), // 1 more, for guard page at high addresses
    PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANONYMOUS,
    -1,
    0
  );
  memset(outbuf, ' ', SPACE_AMOUNT);
  if(SPACE_AMOUNT==page_size) mprotect(outbuf, page_size, PROT_READ);
  mprotect(outbuf + page_size*outbuf_pages, page_size, PROT_NONE); // gaurd page
  saved_outbuf = outbuf + SPACE_AMOUNT;
  // available space:  page_size*outbuf_pages-SPACE_AMOUNT

  seconds_since_1970 = time(NULL);
  time_of_boot = seconds_since_1970 - seconds_since_boot;

  meminfo();

  check_header_width();
}
