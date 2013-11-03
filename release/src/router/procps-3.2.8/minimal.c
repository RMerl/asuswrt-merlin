/*
 * Copyright 1998,2004 by Albert Cahalan; all rights reserved.
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Library General Public License for more details.
 */

/* This is a minimal /bin/ps, designed to be smaller than the old ps
 * while still supporting some of the more important features of the
 * new ps. (for total size, note that this ps does not need libproc)
 * It is suitable for Linux-on-a-floppy systems only.
 *
 * Maintainers: do not compile or install for normal systems.
 * Anyone needing this will want to tweak their compiler anyway.
 */

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>


#define DEV_ENCODE(M,m) ( \
  ( (M&0xfff) << 8)   |   ( (m&0xfff00) << 12)   |   (m&0xff)   \
)

///////////////////////////////////////////////////////
#ifdef __sun__
#include <sys/mkdev.h>
#define _STRUCTURED_PROC 1
#include <sys/procfs.h>
#define NO_TTY_VALUE DEV_ENCODE(-1,-1)
#define HZ 1    // only bother with seconds
#endif

///////////////////////////////////////////////////////
#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/user.h>
#define NO_TTY_VALUE DEV_ENCODE(-1,-1)
#define HZ 1    // only bother with seconds
#endif

///////////////////////////////////////////////////////
#ifdef __linux__
#include <asm/param.h>  /* HZ */
#include <asm/page.h>   /* PAGE_SIZE */
#define NO_TTY_VALUE DEV_ENCODE(0,0)
#ifndef HZ
#warning HZ not defined, assuming it is 100
#define HZ 100
#endif
#endif

///////////////////////////////////////////////////////////

#ifndef PAGE_SIZE
#warning PAGE_SIZE not defined, assuming it is 4096
#define PAGE_SIZE 4096
#endif



static char P_tty_text[16];
static char P_cmd[16];
static char P_state;
static int P_euid;
static int P_pid;
static int P_ppid, P_pgrp, P_session, P_tty_num, P_tpgid;
static unsigned long P_flags, P_min_flt, P_cmin_flt, P_maj_flt, P_cmaj_flt, P_utime, P_stime;
static long P_cutime, P_cstime, P_priority, P_nice, P_timeout, P_alarm;
static unsigned long P_start_time, P_vsize;
static long P_rss;
static unsigned long P_rss_rlim, P_start_code, P_end_code, P_start_stack, P_kstk_esp, P_kstk_eip;
static unsigned P_signal, P_blocked, P_sigignore, P_sigcatch;
static unsigned long P_wchan, P_nswap, P_cnswap;



#if 0
static int screen_cols = 80;
static int w_count;
#endif

static int want_one_pid;
static const char *want_one_command;
static int select_notty;
static int select_all;

static int ps_format;
static int old_h_option;

/* we only pretend to support this */
static int show_args;    /* implicit with -f and all BSD options */
static int bsd_c_option; /* this option overrides the above */

static int ps_argc;    /* global argc */
static char **ps_argv; /* global argv */
static int thisarg;    /* index into ps_argv */
static char *flagptr;  /* current location in ps_argv[thisarg] */




static void usage(void){
  fprintf(stderr,
    "-C   select by command name (minimal ps only accepts one)\n"
    "-p   select by process ID (minimal ps only accepts one)\n"
    "-e   all processes (same as ax)\n"
    "a    all processes w/ tty, including other users\n"
    "x    processes w/o controlling ttys\n"
    "-f   full format\n"
    "-j,j job control format\n"
    "v    virtual memory format\n"
    "-l,l long format\n"
    "u    user-oriented format\n"
    "-o   user-defined format (limited support, only \"ps -o pid=\")\n"
    "h    no header\n"
/*
    "-A   all processes (same as ax)\n"
    "c    true command name\n"
    "-w,w wide output\n"
*/
  );
  exit(1);
}

/*
 * Return the next argument, or call the usage function.
 * This handles both:   -oFOO   -o FOO
 */
static const char *get_opt_arg(void){
  const char *ret;
  ret = flagptr+1;    /* assume argument is part of ps_argv[thisarg] */
  if(*ret) return ret;
  if(++thisarg >= ps_argc) usage();   /* there is nothing left */
  /* argument is the new ps_argv[thisarg] */
  ret = ps_argv[thisarg];
  if(!ret || !*ret) usage();
  return ret;
}


/* return the PID, or 0 if nothing good */
static void parse_pid(const char *str){
  char *endp;
  int num;
  if(!str)            goto bad;
  num = strtol(str, &endp, 0);
  if(*endp != '\0')   goto bad;
  if(num<1)           goto bad;
  if(want_one_pid)    goto bad;
  want_one_pid = num;
  return;
bad:
  usage();
}

/***************** parse SysV options, including Unix98  *****************/
static void parse_sysv_option(void){
  do{
    switch(*flagptr){
    /**** selection ****/
    case 'C': /* end */
      if(want_one_command) usage();
      want_one_command = get_opt_arg();
      return; /* can't have any more options */
    case 'p': /* end */
      parse_pid(get_opt_arg());
      return; /* can't have any more options */
    case 'A':
    case 'e':
      select_all++;
      select_notty++;
case 'w':    /* here for now, since the real one is not used */
      break;
    /**** output format ****/
    case 'f':
      show_args = 1;
      /* FALL THROUGH */
    case 'j':
    case 'l':
      if(ps_format) usage();
      ps_format = *flagptr;
      break;
    case 'o': /* end */
      /* We only support a limited form: "ps -o pid="  (yes, just "pid=") */
      if(strcmp(get_opt_arg(),"pid=")) usage();
      if(ps_format) usage();
      ps_format = 'o';
      old_h_option++;
      return; /* can't have any more options */
    /**** other stuff ****/
#if 0
    case 'w':
      w_count++;
      break;
#endif
    default:
      usage();
    } /* switch */
  }while(*++flagptr);
}

/************************* parse BSD options **********************/
static void parse_bsd_option(void){
  do{
    switch(*flagptr){
    /**** selection ****/
    case 'a':
      select_all++;
      break;
    case 'x':
      select_notty++;
      break;
    case 'p': /* end */
      parse_pid(get_opt_arg());
      return; /* can't have any more options */
    /**** output format ****/
    case 'j':
    case 'l':
    case 'u':
    case 'v':
      if(ps_format) usage();
      ps_format = 0x80 | *flagptr;  /* use 0x80 to tell BSD from SysV */
      break;
    /**** other stuff ****/
    case 'c':
      bsd_c_option++;
#if 0
      break;
#endif
    case 'w':
#if 0
      w_count++;
#endif
      break;
    case 'h':
      old_h_option++;
      break;
    default:
      usage();
    } /* switch */
  }while(*++flagptr);
}

#if 0
#include <termios.h>
/* not used yet */
static void choose_dimensions(void){
  struct winsize ws;
  char *columns;
  /* screen_cols is 80 by default */
  if(ioctl(1, TIOCGWINSZ, &ws) != -1 && ws.ws_col>30) screen_cols = ws.ws_col;
  columns = getenv("COLUMNS");
  if(columns && *columns){
    long t;
    char *endptr;
    t = strtol(columns, &endptr, 0);
    if(!*endptr && (t>30) && (t<(long)999999999)) screen_cols = (int)t;
  }
  if(w_count && (screen_cols<132)) screen_cols=132;
  if(w_count>1) screen_cols=999999999;
}
#endif

static void arg_parse(int argc, char *argv[]){
  int sel = 0;  /* to verify option sanity */
  ps_argc = argc;
  ps_argv = argv;
  thisarg = 0;
  /**** iterate over the args ****/
  while(++thisarg < ps_argc){
    flagptr = ps_argv[thisarg];
    switch(*flagptr){
    case '0' ... '9':
      show_args = 1;
      parse_pid(flagptr);
      break;
    case '-':
      flagptr++;
      parse_sysv_option();
      break;
    default:
      show_args = 1;
      parse_bsd_option();
      break;
    }
  }
  /**** sanity check and clean-up ****/
  if(want_one_pid) sel++;
  if(want_one_command) sel++;
  if(select_notty || select_all) sel++;
  if(sel>1 || select_notty>1 || select_all>1 || bsd_c_option>1 || old_h_option>1) usage();
  if(bsd_c_option) show_args = 0;
}

#ifdef __sun__
/* return 1 if it works, or 0 for failure */
static int stat2proc(int pid) {
    struct psinfo p;  //   /proc/*/psinfo, struct psinfo, psinfo_t
    char buf[32];
    int num;
    int fd;
    int tty_maj, tty_min;
    snprintf(buf, sizeof buf, "/proc/%d/psinfo", pid);
    if ( (fd = open(buf, O_RDONLY, 0) ) == -1 ) return 0;
    num = read(fd, &p, sizeof p);
    close(fd);
    if(num != sizeof p) return 0;

    num = PRFNSZ;
    if (num >= sizeof P_cmd) num = sizeof P_cmd - 1;
    memcpy(P_cmd, p.pr_fname, num);  // p.pr_fname or p.pr_lwp.pr_name
    P_cmd[num] = '\0';

    P_pid     = p.pr_pid;
    P_ppid    = p.pr_ppid;
    P_pgrp    = p.pr_pgid;
    P_session = p.pr_sid;
    P_euid    = p.pr_euid;
    P_rss     = p.pr_rssize;
    P_vsize   = p.pr_size;
    P_start_time = p.pr_start.tv_sec;
    P_wchan   = p.pr_lwp.pr_wchan;
    P_state   = p.pr_lwp.pr_sname;
    P_nice    = p.pr_lwp.pr_nice;
    P_priority = p.pr_lwp.pr_pri;  // or pr_oldpri
//    P_ruid    = p.pr_uid;
//    P_rgid    = p.pr_gid;
//    P_egid    = p.pr_egid;

#if 0
    // don't support these
    P_tpgid; P_flags,
    P_min_flt, P_cmin_flt, P_maj_flt, P_cmaj_flt, P_utime, P_stime;
    P_cutime, P_cstime, P_timeout, P_alarm;
    P_rss_rlim, P_start_code, P_end_code, P_start_stack, P_kstk_esp, P_kstk_eip;
    P_signal, P_blocked, P_sigignore, P_sigcatch;
    P_nswap, P_cnswap;
#endif

    // we like it Linux-encoded :-)
    tty_maj = major(p.pr_ttydev);
    tty_min = minor(p.pr_ttydev);
    P_tty_num = DEV_ENCODE(tty_maj,tty_min);

    snprintf(P_tty_text, sizeof P_tty_text, "%3d,%-3d", tty_maj, tty_min);
#if 1
    if (tty_maj == 24) snprintf(P_tty_text, sizeof P_tty_text, "pts/%-3u", tty_min);
    if (P_tty_num == NO_TTY_VALUE)    memcpy(P_tty_text, "   ?   ", 8);
    if (P_tty_num == DEV_ENCODE(0,0)) memcpy(P_tty_text, "console", 8);
#endif

    if(P_pid != pid) return 0;
    return 1;
}
#endif

#ifdef __FreeBSD__
/* return 1 if it works, or 0 for failure */
static int stat2proc(int pid) {
    char buf[400];
    int num;
    int fd;
    char* tmp;
    int tty_maj, tty_min;
    snprintf(buf, 32, "/proc/%d/status", pid);
    if ( (fd = open(buf, O_RDONLY, 0) ) == -1 ) return 0;
    num = read(fd, buf, sizeof buf - 1);
    close(fd);
    if(num<43) return 0;
    buf[num] = '\0';

    P_state = '-';

    // FreeBSD /proc/*/status is seriously fucked. Unlike the Linux
    // files, we can't use strrchr to find the end of a command name.
    // Spaces in command names do not get escaped. To avoid spoofing,
    // one may skip 20 characters and then look _forward_ only to
    // find a pattern of entries that are {with,with,without} a comma.
    // The entry without a comma is wchan. Then count backwards!
    //
    // Don't bother for now. FreeBSD isn't worth the trouble.

    tmp = strchr(buf,' ');
    num = tmp - buf;
    if (num >= sizeof P_cmd) num = sizeof P_cmd - 1;
    memcpy(P_cmd,buf,num);
    P_cmd[num] = '\0';

    num = sscanf(tmp+1,
       "%d %d %d %d "
       "%d,%d "
       "%*s "
       "%ld,%*d "
       "%ld,%*d "
       "%ld,%*d "
       "%*s "
       "%d %d ",
       &P_pid, &P_ppid, &P_pgrp, &P_session,
       &tty_maj, &tty_min,
       /* SKIP funny flags thing */
       &P_start_time, /* SKIP microseconds */
       &P_utime, /* SKIP microseconds */
       &P_stime, /* SKIP microseconds */
       /* SKIP &P_wchan, for now -- it is a string */
       &P_euid, &P_euid   // don't know which is which
    );
/*    fprintf(stderr, "stat2proc converted %d fields.\n",num); */

    snprintf(P_tty_text, sizeof P_tty_text, "%3d,%-3d", tty_maj, tty_min);
    P_tty_num = DEV_ENCODE(tty_maj,tty_min);
// tty decode is 224 to 256 bytes on i386
#if 1
    tmp = NULL;
    if (tty_maj ==  5) tmp = " ttyp%c ";
    if (tty_maj == 12) tmp = " ttyv%c ";
    if (tty_maj == 28) tmp = " ttyd%c ";
    if (P_tty_num == NO_TTY_VALUE) tmp = "   ?   ";
    if (P_tty_num == DEV_ENCODE(0,0)) tmp = "console";
    if (P_tty_num == DEV_ENCODE(12,255)) tmp = "consolectl";
    if (tmp) {
      snprintf(
        P_tty_text,
        sizeof P_tty_text,
        tmp,
        "0123456789abcdefghijklmnopqrstuvwxyz"[tty_min&31]
      );
    }
#endif

    if(num < 9) return 0;
    if(P_pid != pid) return 0;
    return 1;
}
#endif

#ifdef __linux__
/* return 1 if it works, or 0 for failure */
static int stat2proc(int pid) {
    char buf[800]; /* about 40 fields, 64-bit decimal is about 20 chars */
    int num;
    int fd;
    char* tmp;
    struct stat sb; /* stat() used to get EUID */
    snprintf(buf, 32, "/proc/%d/stat", pid);
    if ( (fd = open(buf, O_RDONLY, 0) ) == -1 ) return 0;
    num = read(fd, buf, sizeof buf - 1);
    fstat(fd, &sb);
    P_euid = sb.st_uid;
    close(fd);
    if(num<80) return 0;
    buf[num] = '\0';
    tmp = strrchr(buf, ')');      /* split into "PID (cmd" and "<rest>" */
    *tmp = '\0';                  /* replace trailing ')' with NUL */
    /* parse these two strings separately, skipping the leading "(". */
    memset(P_cmd, 0, sizeof P_cmd);          /* clear */
    sscanf(buf, "%d (%15c", &P_pid, P_cmd);  /* comm[16] in kernel */
    num = sscanf(tmp + 2,                    /* skip space after ')' too */
       "%c "
       "%d %d %d %d %d "
       "%lu %lu %lu %lu %lu %lu %lu "
       "%ld %ld %ld %ld %ld %ld "
       "%lu %lu "
       "%ld "
       "%lu %lu %lu %lu %lu %lu "
       "%u %u %u %u " /* no use for RT signals */
       "%lu %lu %lu",
       &P_state,
       &P_ppid, &P_pgrp, &P_session, &P_tty_num, &P_tpgid,
       &P_flags, &P_min_flt, &P_cmin_flt, &P_maj_flt, &P_cmaj_flt, &P_utime, &P_stime,
       &P_cutime, &P_cstime, &P_priority, &P_nice, &P_timeout, &P_alarm,
       &P_start_time, &P_vsize,
       &P_rss,
       &P_rss_rlim, &P_start_code, &P_end_code, &P_start_stack, &P_kstk_esp, &P_kstk_eip,
       &P_signal, &P_blocked, &P_sigignore, &P_sigcatch,
       &P_wchan, &P_nswap, &P_cnswap
    );
/*    fprintf(stderr, "stat2proc converted %d fields.\n",num); */
    P_vsize /= 1024;
    P_rss *= (PAGE_SIZE/1024);

    memcpy(P_tty_text, "   ?   ", 8);
    if (P_tty_num != NO_TTY_VALUE) {
      int tty_maj = (P_tty_num>>8)&0xfff;
      int tty_min = (P_tty_num&0xff) | ((P_tty_num>>12)&0xfff00);
      snprintf(P_tty_text, sizeof P_tty_text, "%3d,%-3d", tty_maj, tty_min);
    }

    if(num < 30) return 0;
    if(P_pid != pid) return 0;
    return 1;
}
#endif

static const char *do_time(unsigned long t){
  int hh,mm,ss;
  static char buf[32];
  int cnt = 0;
  t /= HZ;
  ss = t%60;
  t /= 60;
  mm = t%60;
  t /= 60;
  hh = t%24;
  t /= 24;
  if(t) cnt = snprintf(buf, sizeof buf, "%d-", (int)t);
  snprintf(cnt + buf, sizeof(buf)-cnt, "%02d:%02d:%02d", hh, mm, ss);
  return buf;
}

static const char *do_user(void){
  static char buf[32];
  static struct passwd *p;
  static int lastuid = -1;
  if(P_euid != lastuid){
    p = getpwuid(P_euid);
    if(p) snprintf(buf, sizeof buf, "%-8.8s", p->pw_name);
    else  snprintf(buf, sizeof buf, "%5d   ", P_euid);
  }
  return buf;
}

static const char *do_cpu(int longform){
  static char buf[8];
  strcpy(buf," -  ");
  if(!longform) buf[2] = '\0';
  return buf;
}

static const char *do_mem(int longform){
  static char buf[8];
  strcpy(buf," -  ");
  if(!longform) buf[2] = '\0';
  return buf;
}

static const char *do_stime(void){
  static char buf[32];
  strcpy(buf,"  -  ");
  return buf;
}

static void print_proc(void){
  switch(ps_format){
  case 0:
    printf("%5d %s %s", P_pid, P_tty_text, do_time(P_utime+P_stime));
    break;
  case 'o':
    printf("%d\n", P_pid);
    return; /* don't want the command */
  case 'l':
    printf(
      "0 %c %5d %5d %5d %s %3d %3d - "
      "%5ld %06x %s %s",
      P_state, P_euid, P_pid, P_ppid, do_cpu(0),
      (int)P_priority, (int)P_nice, P_vsize/(PAGE_SIZE/1024),
      (unsigned)(P_wchan&0xffffff), P_tty_text, do_time(P_utime+P_stime)
    );
    break;
  case 'f':
    printf(
      "%8s %5d %5d %s %s %s %s",
      do_user(), P_pid, P_ppid, do_cpu(0), do_stime(), P_tty_text, do_time(P_utime+P_stime)
    );
    break;
  case 'j':
    printf(
      "%5d %5d %5d %s %s",
      P_pid, P_pgrp, P_session, P_tty_text, do_time(P_utime+P_stime)
    );
    break;
  case 'u'|0x80:
    printf(
      "%8s %5d %s %s %5ld %4ld %s %c %s %s",
      do_user(), P_pid, do_cpu(1), do_mem(1), P_vsize, P_rss, P_tty_text, P_state,
      do_stime(), do_time(P_utime+P_stime)
    );
    break;
  case 'v'|0x80:
    printf(
      "%5d %s %c %s %6d   -   - %5d %s",
      P_pid, P_tty_text, P_state, do_time(P_utime+P_stime), (int)P_maj_flt,
      (int)P_rss, do_mem(1)
    );
    break;
  case 'j'|0x80:
    printf(
      "%5d %5d %5d %5d %s %5d %c %5d %s",
      P_ppid, P_pid, P_pgrp, P_session, P_tty_text, P_tpgid, P_state, P_euid, do_time(P_utime+P_stime)
    );
    break;
  case 'l'|0x80:
    printf(
      "0 %5d %5d %5d %3d %3d "
      "%5ld %4ld %06x %c %s %s",
      P_euid, P_pid, P_ppid, (int)P_priority, (int)P_nice,
      P_vsize, P_rss, (unsigned)(P_wchan&0xffffff), P_state, P_tty_text, do_time(P_utime+P_stime)
    );
    break;
  default:
    ;
  }
  if(show_args) printf(" [%s]\n", P_cmd);
  else          printf(" %s\n", P_cmd);
}


int main(int argc, char *argv[]){
  arg_parse(argc, argv);
#if 0
  choose_dimensions();
#endif
  if(!old_h_option){
    const char *head;
    switch(ps_format){
    default: /* can't happen */
    case 0:        head = "  PID TTY         TIME CMD"; break;
    case 'l':      head = "F S   UID   PID  PPID  C PRI  NI ADDR SZ WCHAN    TTY       TIME CMD"; break;
    case 'f':      head = "USER       PID  PPID  C STIME   TTY       TIME CMD"; break;
    case 'j':      head = "  PID  PGID   SID TTY         TIME CMD"; break;
    case 'u'|0x80: head = "USER       PID %CPU %MEM   VSZ  RSS   TTY   S START     TIME COMMAND"; break;
    case 'v'|0x80: head = "  PID   TTY   S     TIME  MAJFL TRS DRS   RSS %MEM COMMAND"; break;
    case 'j'|0x80: head = " PPID   PID  PGID   SID   TTY   TPGID S   UID     TIME COMMAND"; break;
    case 'l'|0x80: head = "F   UID   PID  PPID PRI  NI   VSZ  RSS WCHAN  S   TTY       TIME COMMAND"; break;
    }
    printf("%s\n",head);
  }
  if(want_one_pid){
    if(stat2proc(want_one_pid)) print_proc();
    else exit(1);
  }else{
    struct dirent *ent;          /* dirent handle */
    DIR *dir;
    int ouruid;
    int found_a_proc;
    found_a_proc = 0;
    ouruid = getuid();
    dir = opendir("/proc");
    while(( ent = readdir(dir) )){
      if(*ent->d_name<'0' || *ent->d_name>'9') continue;
      if(!stat2proc(atoi(ent->d_name))) continue;
      if(want_one_command){
        if(strcmp(want_one_command,P_cmd)) continue;
      }else{
        if(!select_notty && P_tty_num==NO_TTY_VALUE) continue;
        if(!select_all && P_euid!=ouruid) continue;
      }
      found_a_proc++;
      print_proc();
    }
    closedir(dir);
    exit(!found_a_proc);
  }
  return 0;
}
