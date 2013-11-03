/*
 * Copyright 1998-2002 by Albert Cahalan; all rights resered.
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Library General Public License for more details.
 */
#include <fcntl.h>
#include <pwd.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "proc/pwcache.h"
#include "proc/sig.h"
#include "proc/devname.h"
#include "proc/procps.h"  /* char *user_from_uid(uid_t uid) */
#include "proc/version.h" /* procps_version */

static int f_flag, i_flag, v_flag, w_flag, n_flag;

static int tty_count, uid_count, cmd_count, pid_count;
static int *ttys;
static uid_t *uids;
static const char **cmds;
static int *pids;

#define ENLIST(thing,addme) do{ \
if(!thing##s) thing##s = malloc(sizeof(*thing##s)*saved_argc); \
if(!thing##s) fprintf(stderr,"No memory.\n"),exit(2); \
thing##s[thing##_count++] = addme; \
}while(0)

static int my_pid;
static int saved_argc;

static int sig_or_pri;

static int program;
#define PROG_GARBAGE 0  /* keep this 0 */
#define PROG_KILL  1
#define PROG_SKILL 2
/* #define PROG_NICE  3 */ /* easy, but the old one isn't broken */
#define PROG_SNICE 4


/********************************************************************/

static void display_kill_version(void){
  switch(program) {
    case PROG_KILL:
      fprintf(stdout, "kill (%s)\n",procps_version);
      return;
    case PROG_SKILL:
      fprintf(stdout, "skill (%s)\n",procps_version);
      return;
    case PROG_SNICE:
      fprintf(stdout, "snice (%s)\n",procps_version);
      return;
    default:
      fprintf(stdout, "unknown (%s)\n",procps_version);
      return;
  }
}

/***** kill or nice a process */
static void hurt_proc(int tty, int uid, int pid, const char *restrict const cmd){
  int failed;
  int saved_errno;
  char dn_buf[1000];
  dev_to_tty(dn_buf, 999, tty, pid, ABBREV_DEV);
  if(i_flag){
    char buf[8];
    fprintf(stderr, "%-8s %-8s %5d %-16.16s   ? ",
      (char*)dn_buf,user_from_uid(uid),pid,cmd
    );
    if(!fgets(buf,7,stdin)){
      printf("\n");
      exit(0);
    }
    if(*buf!='y' && *buf!='Y') return;
  }
  /* do the actual work */
  if(program==PROG_SKILL) failed=kill(pid,sig_or_pri);
  else                    failed=setpriority(PRIO_PROCESS,pid,sig_or_pri);
  saved_errno = errno;
  if(w_flag && failed){
    fprintf(stderr, "%-8s %-8s %5d %-16.16s   ",
      (char*)dn_buf,user_from_uid(uid),pid,cmd
    );
    errno = saved_errno;
    perror("");
    return;
  }
  if(i_flag) return;
  if(v_flag){
    printf("%-8s %-8s %5d %-16.16s\n",
      (char*)dn_buf,user_from_uid(uid),pid,cmd
    );
    return;
  }
  if(n_flag){
   printf("%d\n",pid);
   return;
  }
}


/***** check one process */
static void check_proc(int pid){
  char buf[128];
  struct stat statbuf;
  char *tmp;
  int tty;
  int fd;
  int i;
  if(pid==my_pid) return;
  sprintf(buf, "/proc/%d/stat", pid); /* pid (cmd) state ppid pgrp session tty */
  fd = open(buf,O_RDONLY);
  if(fd==-1){  /* process exited maybe */
    if(pids && w_flag) printf("WARNING: process %d could not be found.",pid);
    return;
  }
  fstat(fd, &statbuf);
  if(uids){  /* check the EUID */
    i=uid_count;
    while(i--) if(uids[i]==statbuf.st_uid) break;
    if(i==-1) goto closure;
  }
  read(fd,buf,128);
  buf[127] = '\0';
  tmp = strrchr(buf, ')');
  *tmp++ = '\0';
  i = 5; while(i--) while(*tmp++!=' '); /* scan to find tty */
  tty = atoi(tmp);
  if(ttys){
    i=tty_count;
    while(i--) if(ttys[i]==tty) break;
    if(i==-1) goto closure;
  }
  tmp = strchr(buf, '(') + 1;
  if(cmds){
    i=cmd_count;
    /* fast comparison trick -- useful? */
    while(i--) if(cmds[i][0]==*tmp && !strcmp(cmds[i],tmp)) break;
    if(i==-1) goto closure;
  }
  /* This is where we kill/nice something. */
/*  fprintf(stderr, "PID %d, UID %d, TTY %d,%d, COMM %s\n",
    pid, statbuf.st_uid, tty>>8, tty&0xf, tmp
  );
*/
  hurt_proc(tty, statbuf.st_uid, pid, tmp);
closure:
  close(fd); /* kill/nice _first_ to avoid PID reuse */
}


/***** debug function */
#if 0
static void show_lists(void){
  int i;

  fprintf(stderr, "%d TTY: ", tty_count);
  if(ttys){
    i=tty_count;
    while(i--){
      fprintf(stderr, "%d,%d%c", (ttys[i]>>8)&0xff, ttys[i]&0xff, i?' ':'\n');
    }
  }else fprintf(stderr, "\n");
  
  fprintf(stderr, "%d UID: ", uid_count);
  if(uids){
    i=uid_count;
    while(i--) fprintf(stderr, "%d%c", uids[i], i?' ':'\n');
  }else fprintf(stderr, "\n");
  
  fprintf(stderr, "%d PID: ", pid_count);
  if(pids){
    i=pid_count;
    while(i--) fprintf(stderr, "%d%c", pids[i], i?' ':'\n');
  }else fprintf(stderr, "\n");
  
  fprintf(stderr, "%d CMD: ", cmd_count);
  if(cmds){
    i=cmd_count;
    while(i--) fprintf(stderr, "%s%c", cmds[i], i?' ':'\n');
  }else fprintf(stderr, "\n");
}
#endif


/***** iterate over all PIDs */
static void iterate(void){
  int pid;
  DIR *d;
  struct dirent *de;
  if(pids){
    pid = pid_count;
    while(pid--) check_proc(pids[pid]);
    return;
  }
#if 0
  /* could setuid() and kill -1 to have the kernel wipe out a user */
  if(!ttys && !cmds && !pids && !i_flag){
  }
#endif
  d = opendir("/proc");
  if(!d){
    perror("/proc");
    exit(1);
  }
  while(( de = readdir(d) )){
    if(de->d_name[0] > '9') continue;
    if(de->d_name[0] < '1') continue;
    pid = atoi(de->d_name);
    if(pid) check_proc(pid);
  }
  closedir (d);
}

/***** kill help */
static void kill_usage(void) NORETURN;
static void kill_usage(void){
  fprintf(stderr,
    "Usage:\n"
    "  kill pid ...              Send SIGTERM to every process listed.\n"
    "  kill signal pid ...       Send a signal to every process listed.\n"
    "  kill -s signal pid ...    Send a signal to every process listed.\n"
    "  kill -l                   List all signal names.\n"
    "  kill -L                   List all signal names in a nice table.\n"
    "  kill -l signal            Convert between signal numbers and names.\n"
  );
  exit(1);
}

/***** kill */
static void kill_main(int argc, const char *restrict const *restrict argv) NORETURN;
static void kill_main(int argc, const char *restrict const *restrict argv){
  const char *sigptr;
  int signo = SIGTERM;
  int exitvalue = 0;
  if(argc<2) kill_usage();
  if(!strcmp(argv[1],"-V")|| !strcmp(argv[1],"--version")){
    display_kill_version();
    exit(0);
  }
  if(argv[1][0]!='-'){
    argv++;
    argc--;
    goto no_more_args;
  }

  /* The -l option prints out signal names. */
  if(argv[1][1]=='l' && argv[1][2]=='\0'){
    if(argc==2){
      unix_print_signals();
      exit(0);
    }
    /* at this point, argc must be 3 or more */
    if(argc>128 || argv[2][0] == '-') kill_usage();
    exit(print_given_signals(argc-2, argv+2, 80));
  }

  /* The -L option prints out signal names in a nice table. */
  if(argv[1][1]=='L' && argv[1][2]=='\0'){
    if(argc==2){
      pretty_print_signals();
      exit(0);
    }
    kill_usage();
  }
  if(argv[1][1]=='-' && argv[1][2]=='\0'){
    argv+=2;
    argc-=2;
    goto no_more_args;
  }
  if(argv[1][1]=='-') kill_usage(); /* likely --help */
  // FIXME: "kill -sWINCH $$" not handled
  if(argv[1][2]=='\0' && (argv[1][1]=='s' || argv[1][1]=='n')){
    sigptr = argv[2];
    argv+=3;
    argc-=3;
  }else{
    sigptr = argv[1]+1;
    argv+=2;
    argc-=2;
  }
  signo = signal_name_to_number(sigptr);
  if(signo<0){
    fprintf(stderr, "ERROR: unknown signal name \"%s\".\n", sigptr);
    kill_usage();
  }
no_more_args:
  if(!argc) kill_usage();  /* nothing to kill? */
  while(argc--){
    long pid;
    char *endp;
    pid = strtol(argv[argc],&endp,10);
    if(!*endp){
      if(!kill((pid_t)pid,signo)) continue;
      // The UNIX standard contradicts itself. If at least one process
      // is matched for each PID (as if processes could share PID!) and
      // "the specified signal was successfully processed" (the systcall
      // returned zero?) for at least one of those processes, then we must
      // exit with zero. Note that an error might have also occured.
      // The standard says we return non-zero if an error occurs. Thus if
      // killing two processes gives 0 for one and EPERM for the other,
      // we are required to return both zero and non-zero. Quantum kill???
      exitvalue = 1;
      continue;
    }
    fprintf(stderr, "ERROR: garbage process ID \"%s\".\n", argv[argc]);
    kill_usage();
  }
  exit(exitvalue);
}

/***** skill/snice help */
static void skillsnice_usage(void) NORETURN;
static void skillsnice_usage(void){
  if(program==PROG_SKILL){
    fprintf(stderr,
      "Usage:   skill [signal to send] [options] process selection criteria\n"
      "Example: skill -KILL -v pts/*\n"
      "\n"
      "The default signal is TERM. Use -l or -L to list available signals.\n"
      "Particularly useful signals include HUP, INT, KILL, STOP, CONT, and 0.\n"
      "Alternate signals may be specified in three ways: -SIGKILL -KILL -9\n"
    );
  }else{
    fprintf(stderr,
      "Usage:   snice [new priority] [options] process selection criteria\n"
      "Example: snice netscape crack +7\n"
      "\n"
      "The default priority is +4. (snice +4 ...)\n"
      "Priority numbers range from +20 (slowest) to -20 (fastest).\n"
      "Negative priority numbers are restricted to administrative users.\n"
    );
  }
  fprintf(stderr,
    "\n"
    "General options:\n"
    "-f  fast mode            This is not currently useful.\n"
    "-i  interactive use      You will be asked to approve each action.\n"
    "-v  verbose output       Display information about selected processes.\n"
    "-w  warnings enabled     This is not currently useful.\n"
    "-n  no action            This only displays the process ID.\n"
    "\n"
    "Selection criteria can be: terminal, user, pid, command.\n"
    "The options below may be used to ensure correct interpretation.\n"
    "-t  The next argument is a terminal (tty or pty).\n"
    "-u  The next argument is a username.\n"
    "-p  The next argument is a process ID number.\n"
    "-c  The next argument is a command name.\n"
  );
  exit(1);
}

#if 0
static void _skillsnice_usage(int line){
  fprintf(stderr,"Something at line %d.\n", line);
  skillsnice_usage();
}
#define skillsnice_usage() _skillsnice_usage(__LINE__)
#endif

#define NEXTARG (argc?( argc--, ((argptr=*++argv)) ):NULL)

/***** common skill/snice argument parsing code */
#define NO_PRI_VAL ((int)0xdeafbeef)
static void skillsnice_parse(int argc, const char *restrict const *restrict argv){
  int signo = -1;
  int prino = NO_PRI_VAL;
  int force = 0;
  int num_found = 0;
  const char *restrict argptr;
  if(argc<2) skillsnice_usage();
  if(argc==2 && argv[1][0]=='-'){
    if(!strcmp(argv[1],"-L")){
      pretty_print_signals();
      exit(0);
    }
    if(!strcmp(argv[1],"-l")){
      unix_print_signals();
      exit(0);
    }
    if(!strcmp(argv[1],"-V")|| !strcmp(argv[1],"--version")){
      display_kill_version();
      exit(0);
    }
    skillsnice_usage();
  }
  NEXTARG;
  /* Time for serious parsing. What does "skill -int 123 456" mean? */
  while(argc){
    if(force && !num_found){  /* if forced, _must_ find something */
      fprintf(stderr,"ERROR: -%c used with bad data.\n", force);
      skillsnice_usage();
    }
    force = 0;
    if(program==PROG_SKILL && signo<0 && *argptr=='-'){
      signo = signal_name_to_number(argptr+1);
      if(signo>=0){      /* found a signal */
        if(!NEXTARG) break;
        continue;
      }
    }
    if(program==PROG_SNICE && prino==NO_PRI_VAL
    && (*argptr=='+' || *argptr=='-') && argptr[1]){
      long val;
      char *endp;
      val = strtol(argptr,&endp,10);
      if(!*endp && val<=999 && val>=-999){
        prino=val;
        if(!NEXTARG) break;
        continue;
      }
    }
    /* If '-' found, collect any flags. (but lone "-" is a tty) */
    if(*argptr=='-' && argptr[1]){
      argptr++;
      do{
        switch(( force = *argptr++ )){
        default:  skillsnice_usage();
        case 't':
        case 'u':
        case 'p':
        case 'c':
          if(!*argptr){ /* nothing left here, *argptr is '\0' */
            if(!NEXTARG){
              fprintf(stderr,"ERROR: -%c with nothing after it.\n", force);
              skillsnice_usage();
            }
          }
          goto selection_collection;
        case 'f': f_flag++; break;
        case 'i': i_flag++; break;
        case 'v': v_flag++; break;
        case 'w': w_flag++; break;
        case 'n': n_flag++; break;
        case 0:
          NEXTARG;
          /*
           * If no more arguments, all the "if(argc)..." tests will fail
           * and the big loop will exit.
           */
        } /* END OF SWITCH */
      }while(force);
    } /* END OF IF */
selection_collection:
    num_found = 0; /* we should find at least one thing */
    switch(force){ /* fall through each data type */
    default: skillsnice_usage();
    case 0: /* not forced */
    case 't':
      if(argc){
        struct stat sbuf;
        char path[32];
        if(!argptr) skillsnice_usage(); /* Huh? Maybe "skill -t ''". */
        snprintf(path,32,"/dev/%s",argptr);
        if(stat(path, &sbuf)>=0 && S_ISCHR(sbuf.st_mode)){
          num_found++;
          ENLIST(tty,sbuf.st_rdev);
          if(!NEXTARG) break;
        }else if(!(argptr[1])){  /* if only 1 character */
          switch(*argptr){
          default:
            if(stat(argptr,&sbuf)<0) break; /* the shell eats '?' */
          case '-':
          case '?':
            num_found++;
            ENLIST(tty,0);
            if(!NEXTARG) break;
          }
        }
      }
      if(force) continue;
    case 'u':
      if(argc){
        struct passwd *passwd_data;
        passwd_data = getpwnam(argptr);
        if(passwd_data){
          num_found++;
          ENLIST(uid,passwd_data->pw_uid);
          if(!NEXTARG) break;
        }
      }
      if(force) continue;
    case 'p':
      if(argc && *argptr>='0' && *argptr<='9'){
        char *endp;
        int num;
        num = strtol(argptr, &endp, 0);
        if(*endp == '\0'){
          num_found++;
          ENLIST(pid,num);
          if(!NEXTARG) break;
        }
      }
      if(force) continue;
      if(num_found) continue; /* could still be an option */
    case 'c':
      if(argc){
        num_found++;
        ENLIST(cmd,argptr);
        if(!NEXTARG) break;
      }
    } /* END OF SWITCH */
  } /* END OF WHILE */
  /* No more arguments to process. Must sanity check. */
  if(!tty_count && !uid_count && !cmd_count && !pid_count){
    fprintf(stderr,"ERROR: no process selection criteria.\n");
    skillsnice_usage();
  }
  if((f_flag|i_flag|v_flag|w_flag|n_flag) & ~1){
    fprintf(stderr,"ERROR: general flags may not be repeated.\n");
    skillsnice_usage();
  }
  if(i_flag && (v_flag|f_flag|n_flag)){
    fprintf(stderr,"ERROR: -i makes no sense with -v, -f, and -n.\n");
    skillsnice_usage();
  }
  if(v_flag && (i_flag|f_flag)){
    fprintf(stderr,"ERROR: -v makes no sense with -i and -f.\n");
    skillsnice_usage();
  }
  /* OK, set up defaults */
  if(prino==NO_PRI_VAL) prino=4;
  if(signo<0) signo=SIGTERM;
  if(n_flag){
    program=PROG_SKILL;
    signo=0; /* harmless */
  }
  if(program==PROG_SKILL) sig_or_pri = signo;
  else sig_or_pri = prino;
}

/***** main body */
int main(int argc, const char *argv[]){
  const char *tmpstr;
  my_pid = getpid();
  saved_argc = argc;
  if(!argc){
    fprintf(stderr,"ERROR: could not determine own name.\n");
    exit(1);
  }
  tmpstr=strrchr(*argv,'/');
  if(tmpstr) tmpstr++;
  if(!tmpstr) tmpstr=*argv;
  program = PROG_GARBAGE;
  if(*tmpstr=='s'){
    setpriority(PRIO_PROCESS,my_pid,-20);
    if(!strcmp(tmpstr,"snice")) program = PROG_SNICE;
    if(!strcmp(tmpstr,"skill")) program = PROG_SKILL;
  }else{
    if(!strcmp(tmpstr,"kill")) program = PROG_KILL;
  }
  switch(program){
  case PROG_SNICE:
  case PROG_SKILL:
    skillsnice_parse(argc, argv);
/*    show_lists(); */
    iterate(); /* this is it, go get them */
    break;
  case PROG_KILL:
    kill_main(argc, argv);
    break;
  default:
    fprintf(stderr,"ERROR: no \"%s\" support.\n",tmpstr);
  }
  return 0;
}


