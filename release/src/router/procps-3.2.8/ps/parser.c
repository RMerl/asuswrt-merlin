/*
 * Copyright 1998-2003 by Albert Cahalan; all rights reserved.
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Library General Public License for more details.
 */

/* Ought to have debug print stuff like this:
 * #define Print(fmt, args...) printf("Debug: " fmt, ## args)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* username lookups */
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include <sys/stat.h>
#include <unistd.h>

#include "common.h"
#include "../proc/version.h"

#define ARG_GNU  0
#define ARG_END  1
#define ARG_PGRP 2
#define ARG_SYSV 3
#define ARG_PID  4
#define ARG_BSD  5
#define ARG_FAIL 6
#define ARG_SESS 7

static int w_count = 0;

static int ps_argc;    /* global argc */
static char **ps_argv; /* global argv */
static int thisarg;    /* index into ps_argv */
static char *flagptr;  /* current location in ps_argv[thisarg] */
static int not_pure_unix = 0;  /* set by BSD and GNU options */
static int force_bsd = 0;  /* set when normal parsing fails */

#define exclusive(x) if((ps_argc != 2) || strcmp(ps_argv[1],x))\
  return "The " x " option is exclusive."


/********** utility functions **********/

/*
 * Both "-Oppid" and "-O ppid" should be legal, though Unix98
 * does not require it. BSD and Digital Unix allow both.
 * Return the argument or NULL;
 */
static const char *get_opt_arg(void){
  if(*(flagptr+1)){     /* argument is part of ps_argv[thisarg] */
    not_pure_unix = 1;
    return flagptr+1;
  }
  if(thisarg+2 > ps_argc) return NULL;   /* there is nothing left */
  /* argument follows ps_argv[thisarg] */
  if(*(ps_argv[thisarg+1]) == '\0') return NULL;
  return ps_argv[++thisarg];
}

/********** parse lists (of UID, tty, GID, PID...) **********/

static const char *parse_pid(char *str, sel_union *ret){
  char *endp;
  unsigned long num;
  static const char pidrange[]  = "Process ID out of range.";
  static const char pidsyntax[] = "Process ID list syntax error.";
  num = strtoul(str, &endp, 0);
  if(*endp != '\0')      return pidsyntax;
  if(num<1)              return pidrange;
  if(num > 0x7fffffffUL) return pidrange;
  ret->pid = num;
  return 0;
}

static const char *parse_uid(char *str, sel_union *ret){
  struct passwd *passwd_data;
  char *endp;
  unsigned long num;
  static const char uidrange[] = "User ID out of range.";
  static const char uidexist[] = "User name does not exist.";
  num = strtoul(str, &endp, 0);
  if(*endp != '\0'){  /* hmmm, try as login name */
    passwd_data = getpwnam(str);
    if(!passwd_data)    return uidexist;
    num = passwd_data->pw_uid;
  }
  if(num > 0xfffffffeUL) return uidrange;
  ret->uid = num;
  return 0;
}

static const char *parse_gid(char *str, sel_union *ret){
  struct group *group_data;
  char *endp;
  unsigned long num;
  static const char gidrange[] = "Group ID out of range.";
  static const char gidexist[] = "Group name does not exist.";
  num = strtoul(str, &endp, 0);
  if(*endp != '\0'){  /* hmmm, try as login name */
    group_data = getgrnam(str);
    if(!group_data)    return gidexist;
    num = group_data->gr_gid;
  }
  if(num > 0xfffffffeUL) return gidrange;
  ret->gid = num;
  return 0;
}

static const char *parse_cmd(char *str, sel_union *ret){
  strncpy(ret->cmd, str, sizeof ret->cmd);  // strncpy pads to end
  return 0;
}

static const char *parse_tty(char *str, sel_union *ret){
  struct stat sbuf;
  static const char missing[] = "TTY could not be found.";
  static const char not_tty[] = "List member was not a TTY.";
  char path[4096];
  if(str[0]=='/'){
    if(stat(str, &sbuf) >= 0) goto found_it;
    return missing;
  }
#define lookup(p) \
  snprintf(path,4096,p,str); \
  if(stat(path, &sbuf) >= 0) goto found_it

  lookup("/dev/pts/%s");  /* New Unix98 ptys go first */
  lookup("/dev/%s");
  lookup("/dev/tty%s");
  lookup("/dev/pty%s");
  lookup("/dev/%snsole"); /* "co" means "console", maybe do all VCs too? */
  if(!strcmp(str,"-")){   /* "-" means no tty (from AIX) */
    ret->tty = 0;  /* processes w/o tty */
    return 0;
  }
  if(!strcmp(str,"?")){   /* "?" means no tty, which bash eats (Reno BSD?) */
    ret->tty = 0;  /* processes w/o tty */
    return 0;
  }
  if(!*(str+1) && (stat(str,&sbuf)>=0)){  /* Kludge! Assume bash ate '?'. */
    ret->tty = 0;  /* processes w/o tty */
    return 0;
  }
#undef lookup
  return missing;
found_it:
  if(!S_ISCHR(sbuf.st_mode)) return not_tty;
  ret->tty = sbuf.st_rdev;
  return 0;
}

/*
 * Used to parse lists in a generic way. (function pointers)
 */
static const char *parse_list(const char *arg, const char *(*parse_fn)(char *, sel_union *) ){
  selection_node *node;
  char *buf;                      /* temp copy of arg to hack on */
  char *sep_loc;                  /* separator location: " \t," */
  char *walk;
  int items;
  int need_item;
  const char *err;       /* error code that could or did happen */
  /*** prepare to operate ***/
  node = malloc(sizeof(selection_node));
  node->u = malloc(strlen(arg)*sizeof(sel_union)); /* waste is insignificant */
  node->n = 0;
  buf = malloc(strlen(arg)+1);
  strcpy(buf, arg);
  /*** sanity check and count items ***/
  need_item = 1; /* true */
  items = 0;
  walk = buf;
  err = "Improper list.";
  do{
    switch(*walk){
    case ' ': case ',': case '\t': case '\0':
      if(need_item) goto parse_error;
      need_item=1;
      break;
    default:
      if(need_item) items++;
      need_item=0;
    }
  } while (*++walk);
  if(need_item) goto parse_error;
  node->n = items;
  /*** actually parse the list ***/
  walk = buf;
  while(items--){
    sep_loc = strpbrk(walk," ,\t");
    if(sep_loc) *sep_loc = '\0';
    if(( err=(parse_fn)(walk, node->u+items) )) goto parse_error;
    walk = sep_loc + 1; /* point to next item, if any */
  }
  free(buf);
  node->next = selection_list;
  selection_list = node;
  return NULL;
parse_error:
  free(buf);
  free(node->u);
  free(node);
  return err;
}

/***************** parse SysV options, including Unix98  *****************/
static const char *parse_sysv_option(void){
  const char *arg;
  const char *err;

  flagptr = ps_argv[thisarg];
  while(*++flagptr){
    // Find any excuse to ignore stupid Unix98 misfeatures.
    //
    // This list of options is ONLY for those defined by the
    // "IEEE Std 1003.1, 2004 Edition", "ISO/IEC 9945:2003",
    // or "Version 2 of the Single Unix Specification".
    //
    // It may be time to re-think the existance of this list.
    // In the meantime, please do not add to it. The list is
    // intended to ONLY contain flags defined by the POSIX and UNIX
    // standards published by The Open Group, IEEE, and ISO.
    if(!strchr("aAdefgGlnoptuU", *flagptr)) not_pure_unix = 1;  // dude, -Z ain't in POSIX

    switch(*flagptr){
    case 'A':
      trace("-A selects all processes.\n");
      all_processes = 1;
      break;
    case 'C': /* end */
      trace("-C select by process name.\n");  /* Why only HP/UX and us? */
      arg=get_opt_arg();
      if(!arg) return "List of command names must follow -C.";
      err=parse_list(arg, parse_cmd);
      if(err) return err;
      selection_list->typecode = SEL_COMM;
      return NULL; /* can't have any more options */
    case 'F':  /* DYNIX/ptx -f plus sz,rss,psr=ENG between c and stime */
      trace("-F does fuller listing\n");
      format_modifiers |= FM_F;
      format_flags |= FF_Uf;
      unix_f_option = 1; /* does this matter? */
      break;
    case 'G': /* end */
      trace("-G select by RGID (supports names)\n");
      arg=get_opt_arg();
      if(!arg) return "List of real groups must follow -G.";
      err=parse_list(arg, parse_gid);
      if(err) return err;
      selection_list->typecode = SEL_RGID;
      return NULL; /* can't have any more options */
    case 'H':     /* another nice HP/UX feature */
      trace("-H Process hierarchy (like ASCII art forest option)\n");
      forest_type = 'u';
      break;
#if 0
    case 'J':  // specify list of job IDs in hex (IRIX) -- like HP "-R" maybe?
      trace("-J select by job ID\n");  // want a JID ("jid") for "-j" too
      arg=get_opt_arg();
      if(!arg) return "List of jobs must follow -J.";
      err=parse_list(arg, parse_jid);
      if(err) return err;
      selection_list->typecode = SEL_JID;
      return NULL; /* can't have any more options */
#endif
    case 'L':  /*  */
      /* In spite of the insane 2-level thread system, Sun appears to
       * have made this option Linux-compatible. If a process has N
       * threads, ps will produce N lines of output. (not N+1 lines)
       * Zombies are the only exception, with NLWP==0 and 1 output line.
       * SCO UnixWare uses -L too.
       */
      trace("-L Print LWP (thread) info.\n");
      thread_flags |= TF_U_L;
//      format_modifiers |= FM_L;
      break;
    case 'M':  // typically the SE Linux context
      trace("-M Print security label for Mandatory Access Control.\n");
      format_modifiers |= FM_M;
      break;
    case 'N':
      trace("-N negates.\n");
      negate_selection = 1;
      break;
    case 'O': /* end */
      trace("-O is preloaded -o.\n");
      arg=get_opt_arg();
      if(!arg) return "Format or sort specification must follow -O.";
      defer_sf_option(arg, SF_U_O);
      return NULL; /* can't have any more options */
    case 'P':     /* SunOS 5 "psr" or unknown HP/UX feature */
      trace("-P adds columns of PRM info (HP-UX), PSR (SunOS), or capabilities (IRIX)\n");
      format_modifiers |= FM_P;
      break;
#if 0
    case 'R':    // unknown HP/UX feature, like IRIX "-J" maybe?
      trace("-R select by PRM group\n");
      arg=get_opt_arg();
      if(!arg) return "List of PRM groups must follow -R.";
      err=parse_list(arg, parse_prm);
      if(err) return err;
      selection_list->typecode = SEL_PRM;
      return NULL; /* can't have any more options */
#endif
    case 'T':
      /* IRIX 6.5 docs suggest POSIX threads get shown individually.
       * This would make -T be like -L, -m, and m. (but an extra column)
       * Testing (w/ normal processes) shows 1 line/process, not 2.
       * Also, testing shows PID==SPID for all normal processes.
       */
      trace("-T adds strange SPID column (old sproc() threads?)\n");
      thread_flags |= TF_U_T;
//      format_modifiers |= FM_T;
      break;
    case 'U': /* end */
      trace("-U select by RUID (supports names).\n");
      arg=get_opt_arg();
      if(!arg) return "List of real groups must follow -U.";
      err=parse_list(arg, parse_uid);
      if(err) return err;
      selection_list->typecode = SEL_RUID;
      return NULL; /* can't have any more options */
    case 'V': /* single */
      trace("-V prints version.\n");
      exclusive("-V");
      display_version();
      exit(0);
    // This must be verified against SVR4-MP. (UnixWare or Powermax)
    // Leave it undocumented until that problem is solved.
    case 'Z':     /* full Mandatory Access Control level info */
      trace("-Z shows full MAC info\n");
      format_modifiers |= FM_M;
      break;
    case 'a':
      trace("-a select all with a tty, but omit session leaders.\n");
      simple_select |= SS_U_a;
      break;
    case 'c':
      /* HP-UX and SunOS 5 scheduling info modifier */
      trace("-c changes scheduling info.\n");
      format_modifiers |= FM_c;
      break;
    case 'd':
      trace("-d select all, but omit session leaders.\n");
      simple_select |= SS_U_d;
      break;
    case 'e':
      trace("-e selects all processes.\n");
      all_processes = 1;
      break;
    case 'f':
      trace("-f does full listing\n");
      format_flags |= FF_Uf;
      unix_f_option = 1; /* does this matter? */
      break;
    case 'g': /* end */
      trace("-g selects by session leader OR by group name\n");
      arg=get_opt_arg();
      if(!arg) return "List of session leaders OR effective group names must follow -g.";
      err=parse_list(arg, parse_pid);
      if(!err){
        selection_list->typecode = SEL_SESS;
        return NULL; /* can't have any more options */
      }
      err=parse_list(arg, parse_gid);
      if(!err){
        selection_list->typecode = SEL_EGID;
        return NULL; /* can't have any more options */
      }
      return "List of session leaders OR effective group IDs was invalid.";
    case 'j':
      trace("-j jobs format.\n");
      /* old Debian used RD_j and Digital uses JFMT */
      if(sysv_j_format) format_flags |= FF_Uj;
      else format_modifiers |= FM_j;
      break;
    case 'l':
      trace("-l long format.\n");
      format_flags |= FF_Ul;
      break;
    case 'm':
      trace("-m shows threads.\n");
      /* note that AIX shows 2 lines for a normal process */
      thread_flags |= TF_U_m;
      break;
    case 'n': /* end */
      trace("-n sets namelist file.\n");
      arg=get_opt_arg();
      if(!arg) return "Alternate System.map file must follow -n.";
      namelist_file = arg;
      return NULL; /* can't have any more options */
    case 'o': /* end */
      /* Unix98 has gross behavior regarding this. From the following: */
      /*            ps -o pid,nice=NICE,tty=TERMINAL,comm              */
      /* The result must be 2 columns: "PID NICE,tty=TERMINAL,comm"    */
      /* Yes, the second column has the name "NICE,tty=TERMINAL,comm"  */
      /* This parser looks for any excuse to ignore that braindamage.  */
      trace("-o user-defined format.\n");
      arg=get_opt_arg();
      if(!arg) return "Format specification must follow -o.";
      not_pure_unix |= defer_sf_option(arg, SF_U_o);
      return NULL; /* can't have any more options */
    case 'p': /* end */
      trace("-p select by PID.\n");
      arg=get_opt_arg();
      if(!arg) return "List of process IDs must follow -p.";
      err=parse_list(arg, parse_pid);
      if(err) return err;
      selection_list->typecode = SEL_PID;
      return NULL; /* can't have any more options */
#if 0
    case 'r':
      trace("-r some Digital Unix thing about warnings...\n");
      trace("   or SCO's option to chroot() for new /proc and /dev.\n");
      return "The -r option is reserved.";
      break;
#endif
    case 's': /* end */
      trace("-s Select processes belonging to the sessions given.\n");
      arg=get_opt_arg();
      if(!arg) return "List of session IDs must follow -s.";
      err=parse_list(arg, parse_pid);
      if(err) return err;
      selection_list->typecode = SEL_SESS;
      return NULL; /* can't have any more options */
    case 't': /* end */
      trace("-t select by tty.\n");
      arg=get_opt_arg();
      if(!arg) return "List of terminals (pty, tty...) must follow -t.";
      err=parse_list(arg, parse_tty);
      if(err) return err;
      selection_list->typecode = SEL_TTY;
      return NULL; /* can't have any more options */
    case 'u': /* end */
      trace("-u select by user ID (the EUID?) (supports names).\n");
      arg=get_opt_arg();
      if(!arg) return "List of users must follow -u.";
      err=parse_list(arg, parse_uid);
      if(err) return err;
      selection_list->typecode = SEL_EUID;
      return NULL; /* can't have any more options */
    case 'w':
      trace("-w wide output.\n");
      w_count++;
      break;
    case 'x':  /* behind personality until "ps -ax" habit is uncommon */
      if(personality & PER_SVR4_x){
        // Same as -y, but for System V Release 4 MP
        trace("-x works like Sun Solaris & SCO Unixware -y option\n");
        format_modifiers |= FM_y;
        break;
      }
      if(personality & PER_HPUX_x){
        trace("-x extends the command line\n");
        w_count += 2;
        unix_f_option = 1;
        break;
      }
      return "Must set personality to get -x option.";
    case 'y':  /* Sun's -l hack (also: Irix "lnode" resource control info) */
      trace("-y Print lnone info in UID/USER column or do Sun -l hack.\n");
      format_modifiers |= FM_y;
      break;
#if 0
    // This must be verified against SVR4-MP (UnixWare or Powermax)
    case 'z':     /* alias of Mandatory Access Control level info */
      trace("-z shows aliased MAC info\n");
      format_modifiers |= FM_M;
      break;
    // Solaris 10 does this
    case 'z':     /* select by zone */
      trace("-z secects by zone\n");
      arg=get_opt_arg();
      if(!arg) return "List of zones (contexts, labels, whatever?) must follow -z.";
      err=parse_list(arg, parse_zone);
      if(err) return err;
      selection_list->typecode = SEL_ZONE;
      return NULL; /* can't have any more options */
#endif
    case '-':
      return "Embedded '-' among SysV options makes no sense.";
      break;
    case '\0':
      return "Please report the \"SysV \\0 can't happen\" bug.";
      break;
    default:
      return "Unsupported SysV option.";
    } /* switch */
  } /* while */
  return NULL;
}

/************************* parse BSD options **********************/
static const char *parse_bsd_option(void){
  const char *arg;
  const char *err;

  flagptr = ps_argv[thisarg];  /* assume we _have_ a '-' */
  if(flagptr[0]=='-'){
    if(!force_bsd) return "Can't happen!  Problem #1.";
  }else{
    flagptr--; /* off beginning, will increment before use */
    if(personality & PER_FORCE_BSD){
      if(!force_bsd) return "Can't happen!  Problem #2.";
    }else{
      if(force_bsd) return "2nd chance parse failed, not BSD or SysV.";
    }
  }

  while(*++flagptr){
    switch(*flagptr){
    case '0' ... '9': /* end */
      trace("0..9  Old BSD-style select by process ID\n");
      arg=flagptr;
      err=parse_list(arg, parse_pid);
      if(err) return err;
      selection_list->typecode = SEL_PID;
      return NULL; /* can't have any more options */
#if 0
    case 'A':
      /* maybe this just does a larger malloc() ? */
      trace("A Increases the argument space (Digital Unix)\n");
      return "Option A is reserved.";
      break;
    case 'C':
      /* should divide result by 1-(e**(foo*log(bar))) */
      trace("C Use raw CPU time for %%CPU instead of decaying ave\n");
      return "Option C is reserved.";
      break;
#endif
    case 'H':    // The FreeBSD way (NetBSD:s OpenBSD:k FreeBSD:H  -- NIH???)
      trace("H Print LWP (thread) info.\n");   // was: Use /vmcore as c-dumpfile\n");
      thread_flags |= TF_B_H;
      //format_modifiers |= FM_L;    // FIXME: determine if we need something like this
      break;
    case 'L': /* single */
      trace("L List all format specifiers\n");
      exclusive("L");
      print_format_specifiers();
      exit(0);
    case 'M':   // undocumented for now: these are proliferating!
      trace("M MacOS X thread display, like AIX/Tru64\n");
      thread_flags |= TF_B_m;
      break;
    case 'N': /* end */
      trace("N Specify namelist file\n");
      arg=get_opt_arg();
      if(!arg) return "Alternate System.map file must follow N.";
      namelist_file = arg;
      return NULL; /* can't have any more options */
    case 'O': /* end */
      trace("O Like o + defaults, add new columns after PID. Also sort.\n");
      arg=get_opt_arg();
      if(!arg) return "Format or sort specification must follow O.";
      defer_sf_option(arg, SF_B_O);
      return NULL; /* can't have any more options */
      break;
    case 'S':
      trace("S include dead kids in sum\n");
      include_dead_children = 1;
      break;
    case 'T':
      trace("T Select all processes on this terminal\n");
      /* put our tty on a tiny list */
      {
        selection_node *node;
        node = malloc(sizeof(selection_node));
        node->u = malloc(sizeof(sel_union));
        node->u[0].tty = cached_tty;
        node->typecode = SEL_TTY;
        node->n = 1;
        node->next = selection_list;
        selection_list = node;
      }
      break;
    case 'U': /* end */
      trace("U Select processes for specified users.\n");
      arg=get_opt_arg();
      if(!arg) return "List of users must follow U.";
      err=parse_list(arg, parse_uid);
      if(err) return err;
      selection_list->typecode = SEL_EUID;
      return NULL; /* can't have any more options */
    case 'V': /* single */
      trace("V show version info\n");
      exclusive("V");
      display_version();
      exit(0);
    case 'W':
      trace("W N/A get swap info from ... not /dev/drum.\n");
      return "Obsolete W option not supported. (You have a /dev/drum?)";
      break;
    case 'X':
      trace("X Old Linux i386 register format\n");
      format_flags |= FF_LX;
      break;
    case 'Z':  /* FreeBSD does MAC like SGI's Irix does it */
      trace("Z Print security label for Mandatory Access Control.\n");
      format_modifiers |= FM_M;
      break;
    case 'a':
      trace("a Select all w/tty, including other users\n");
      simple_select |= SS_B_a;
      break;
    case 'c':
      trace("c true command name\n");
      bsd_c_option = 1;
      break;
    case 'e':
      trace("e environment\n");
      bsd_e_option = 1;
      break;
    case 'f':
      trace("f ASCII art forest\n");
      forest_type = 'b';
      break;
    case 'g':
      trace("g _all_, even group leaders!.\n");
      simple_select |= SS_B_g;
      break;
    case 'h':
      trace("h Repeat header... yow.\n");
      if(header_type) return "Only one heading option may be specified.";
      if(personality & PER_BSD_h) header_type = HEAD_MULTI;
      else                        header_type = HEAD_NONE;
      break;
    case 'j':
      trace("j job control format\n");
      format_flags |= FF_Bj;
      break;
    case 'k':
      // OpenBSD: don't hide "kernel threads" -- like the swapper?
      // trace("k Print LWP (thread) info.\n");   // was: Use /vmcore as c-dumpfile\n");

      // NetBSD, and soon (?) FreeBSD: sort-by-keyword
      trace("k Specify sorting keywords.\n");
      arg=get_opt_arg();
      if(!arg) return "Long sort specification must follow 'k'.";
      defer_sf_option(arg, SF_G_sort);
      return NULL; /* can't have any more options */
    case 'l':
      trace("l Display long format\n");
      format_flags |= FF_Bl;
      break;
    case 'm':
      trace("m all threads, sort on mem use, show mem info\n");
      if(personality & PER_OLD_m){
        format_flags |= FF_Lm;
        break;
      }
      if(personality & PER_BSD_m){
        defer_sf_option("pmem", SF_B_m);
        break;
      }
      thread_flags |= TF_B_m;
      break;
    case 'n':
      trace("n Numeric output for WCHAN, and USER replaced by UID\n");
      wchan_is_number = 1;
      user_is_number = 1;
      /* TODO add tty_is_number too? */
      break;
    case 'o': /* end */
      trace("o Specify user-defined format\n");
      arg=get_opt_arg();
      if(!arg) return "Format specification must follow o.";
      defer_sf_option(arg, SF_B_o);
      return NULL; /* can't have any more options */
    case 'p': /* end */
      trace("p Select by process ID\n");
      arg=get_opt_arg();
      if(!arg) return "List of process IDs must follow p.";
      err=parse_list(arg, parse_pid);
      if(err) return err;
      selection_list->typecode = SEL_PID;
      return NULL; /* can't have any more options */
    case 'r':
      trace("r Select running processes\n");
      running_only = 1;
      break;
    case 's':
      trace("s Display signal format\n");
      format_flags |= FF_Bs;
      break;
    case 't': /* end */
      trace("t Select by tty.\n");
      /* List of terminals (tty, pty...) _should_ follow t. */
      arg=get_opt_arg();
      if(!arg){
        /* Wow, obsolete BSD syntax. Put our tty on a tiny list. */
        selection_node *node;
        node = malloc(sizeof(selection_node));
        node->u = malloc(sizeof(sel_union));
        node->u[0].tty = cached_tty;
        node->typecode = SEL_TTY;
        node->n = 1;
        node->next = selection_list;
        selection_list = node;
        return NULL;
      }
      err=parse_list(arg, parse_tty);
      if(err) return err;
      selection_list->typecode = SEL_TTY;
      return NULL; /* can't have any more options */
    case 'u':
      trace("u Display user-oriented\n");
      format_flags |= FF_Bu;
      break;
    case 'v':
      trace("v Display virtual memory\n");
      format_flags |= FF_Bv;
      break;
    case 'w':
      trace("w wide output\n");
      w_count++;
      break;
    case 'x':
      trace("x Select processes without controlling ttys\n");
      simple_select |= SS_B_x;
      break;
    case '-':
      return "Embedded '-' among BSD options makes no sense.";
      break;
    case '\0':
      return "Please report the \"BSD \\0 can't happen\" bug.";
      break;
    default:
      return "Unsupported option (BSD syntax)";
    } /* switch */
  } /* while */
  return NULL;
}

/*************** gnu long options **********************/

/*
 * Return the argument or NULL
 */
static const char *grab_gnu_arg(void){
  switch(*flagptr){     /* argument is part of ps_argv[thisarg] */
  default:
    return NULL;                     /* something bad */
  case '=': case ':':
    if(*++flagptr) return flagptr;   /* found it */
    return NULL;                     /* empty '=' or ':' */
  case '\0': /* try next argv[] */
    ;
  }
  if(thisarg+2 > ps_argc) return NULL;   /* there is nothing left */
  /* argument follows ps_argv[thisarg] */
  if(*(ps_argv[thisarg+1]) == '\0') return NULL;
  return ps_argv[++thisarg];
}

typedef struct gnu_table_struct {
  const char *name; /* long option name */
  const void *jump; /* See gcc extension info.   :-)   */
} gnu_table_struct;

static int compare_gnu_table_structs(const void *a, const void *b){
  return strcmp(((const gnu_table_struct*)a)->name,((const gnu_table_struct*)b)->name);
}

/* Option arguments are after ':', after '=', or in argv[n+1] */
static const char *parse_gnu_option(void){
  const char *arg;
  const char *err;
  char *s;
  size_t sl;
  char buf[16];
  gnu_table_struct findme = { buf, NULL};
  gnu_table_struct *found;
  static const gnu_table_struct gnu_table[] = {
  {"Group",         &&case_Group},       /* rgid */
  {"User",          &&case_User},        /* ruid */
  {"cols",          &&case_cols},
  {"columns",       &&case_columns},
  {"context",       &&case_context},
  {"cumulative",    &&case_cumulative},
  {"deselect",      &&case_deselect},    /* -N */
  {"forest",        &&case_forest},      /* f -H */
  {"format",        &&case_format},
  {"group",         &&case_group},       /* egid */
  {"header",        &&case_header},
  {"headers",       &&case_headers},
  {"heading",       &&case_heading},
  {"headings",      &&case_headings},
  {"help",          &&case_help},
  {"info",          &&case_info},
  {"lines",         &&case_lines},
  {"no-header",     &&case_no_header},
  {"no-headers",    &&case_no_headers},
  {"no-heading",    &&case_no_heading},
  {"no-headings",   &&case_no_headings},
  {"noheader",      &&case_noheader},
  {"noheaders",     &&case_noheaders},
  {"noheading",     &&case_noheading},
  {"noheadings",    &&case_noheadings},
  {"pid",           &&case_pid},
  {"ppid",          &&case_ppid},
  {"rows",          &&case_rows},
  {"sid",           &&case_sid},
  {"sort",          &&case_sort},
  {"tty",           &&case_tty},
  {"user",          &&case_user},        /* euid */
  {"version",       &&case_version},
  {"width",         &&case_width},
  };
  const int gnu_table_count = sizeof(gnu_table)/sizeof(gnu_table_struct);

  s = ps_argv[thisarg]+2;
  sl = strcspn(s,":=");
  if(sl > 15) return "Unknown gnu long option.";
  strncpy(buf, s, sl);
  buf[sl] = '\0';
  flagptr = s+sl;

  found = bsearch(&findme, gnu_table, gnu_table_count,
      sizeof(gnu_table_struct), compare_gnu_table_structs
  );

  if(!found) return "Unknown gnu long option.";

  goto *(found->jump);    /* See gcc extension info.  :-)   */

  case_Group:
    trace("--Group\n");
    arg = grab_gnu_arg();
    if(!arg) return "List of real groups must follow --Group.";
    err=parse_list(arg, parse_gid);
    if(err) return err;
    selection_list->typecode = SEL_RGID;
    return NULL;
  case_User:
    trace("--User\n");
    arg = grab_gnu_arg();
    if(!arg) return "List of real users must follow --User.";
    err=parse_list(arg, parse_uid);
    if(err) return err;
    selection_list->typecode = SEL_RUID;
    return NULL;
  case_cols:
  case_width:
  case_columns:
    trace("--cols\n");
    arg = grab_gnu_arg();
    if(arg && *arg){
      long t;
      char *endptr;
      t = strtol(arg, &endptr, 0);
      if(!*endptr && (t>0) && (t<2000000000)){
        screen_cols = (int)t;
        return NULL;
      }
    }
    return "Number of columns must follow --cols, --width, or --columns.";
  case_cumulative:
    trace("--cumulative\n");
    if(s[sl]) return "Option --cumulative does not take an argument.";
    include_dead_children = 1;
    return NULL;
  case_deselect:
    trace("--deselect\n");
    if(s[sl]) return "Option --deselect does not take an argument.";
    negate_selection = 1;
    return NULL;
  case_no_header:
  case_no_headers:
  case_no_heading:
  case_no_headings:
  case_noheader:
  case_noheaders:
  case_noheading:
  case_noheadings:
    trace("--noheaders\n");
    if(s[sl]) return "Option --no-heading does not take an argument.";
    if(header_type) return "Only one heading option may be specified.";
    header_type = HEAD_NONE;
    return NULL;
  case_header:
  case_headers:
  case_heading:
  case_headings:
    trace("--headers\n");
    if(s[sl]) return "Option --heading does not take an argument.";
    if(header_type) return "Only one heading option may be specified.";
    header_type = HEAD_MULTI;
    return NULL;
  case_forest:
    trace("--forest\n");
    if(s[sl]) return "Option --forest does not take an argument.";
    forest_type = 'g';
    return NULL;
  case_format:
    trace("--format\n");
    arg=grab_gnu_arg();
    if(!arg) return "Format specification must follow --format.";
    defer_sf_option(arg, SF_G_format);
    return NULL;
  case_group:
    trace("--group\n");
    arg = grab_gnu_arg();
    if(!arg) return "List of effective groups must follow --group.";
    err=parse_list(arg, parse_gid);
    if(err) return err;
    selection_list->typecode = SEL_EGID;
    return NULL;
  case_help:
    trace("--help\n");
    exclusive("--help");
    fwrite(help_message,1,strlen(help_message),stdout);
    exit(0);
    return NULL;
  case_info:
    trace("--info\n");
    exclusive("--info");
    self_info();
    exit(0);
    return NULL;
  case_pid:
    trace("--pid\n");
    arg = grab_gnu_arg();
    if(!arg) return "List of process IDs must follow --pid.";
    err=parse_list(arg, parse_pid);
    if(err) return err;
    selection_list->typecode = SEL_PID;
    return NULL;
  case_ppid:
    trace("--ppid\n");
    arg = grab_gnu_arg();
    if(!arg) return "List of process IDs must follow --ppid.";
    err=parse_list(arg, parse_pid);
    if(err) return err;
    selection_list->typecode = SEL_PPID;
    return NULL;
  case_rows:
  case_lines:
    trace("--rows\n");
    arg = grab_gnu_arg();
    if(arg && *arg){
      long t;
      char *endptr;
      t = strtol(arg, &endptr, 0);
      if(!*endptr && (t>0) && (t<2000000000)){
        screen_rows = (int)t;
        return NULL;
      }
    }
    return "Number of rows must follow --rows or --lines.";
  case_sid:
    trace("--sid\n");
    arg = grab_gnu_arg();
    if(!arg) return "Some sid thing(s) must follow --sid.";
    err=parse_list(arg, parse_pid);
    if(err) return err;
    selection_list->typecode = SEL_SESS;
    return NULL;
  case_sort:
    trace("--sort\n");
    arg=grab_gnu_arg();
    if(!arg) return "Long sort specification must follow --sort.";
    defer_sf_option(arg, SF_G_sort);
    return NULL;
  case_tty:
    trace("--tty\n");
    arg = grab_gnu_arg();
    if(!arg) return "List of ttys must follow --tty.";
    err=parse_list(arg, parse_tty);
    if(err) return err;
    selection_list->typecode = SEL_TTY;
    return NULL;
  case_user:
    trace("--user\n");
    arg = grab_gnu_arg();
    if(!arg) return "List of effective users must follow --user.";
    err=parse_list(arg, parse_uid);
    if(err) return err;
    selection_list->typecode = SEL_EUID;
    return NULL;
  case_version:
    trace("--version\n");
    exclusive("--version");
    display_version();
    exit(0);
    return NULL;
  case_context:
    trace("--context\n");
    format_flags |= FF_Fc;
    return NULL;
}

/*************** process trailing PIDs  **********************/
static const char *parse_trailing_pids(void){
  selection_node *pidnode;  /* pid */
  selection_node *grpnode;  /* process group */
  selection_node *sidnode;  /* session */
  char **argp;     /* pointer to pointer to text of PID */
  const char *err;       /* error code that could or did happen */
  int i;

  i = ps_argc - thisarg;  /* how many trailing PIDs, SIDs, PGRPs?? */
  argp = ps_argv + thisarg;
  thisarg = ps_argc - 1;   /* we must be at the end now */

  pidnode = malloc(sizeof(selection_node));
  pidnode->u = malloc(i*sizeof(sel_union)); /* waste is insignificant */
  pidnode->n = 0;

  grpnode = malloc(sizeof(selection_node));
  grpnode->u = malloc(i*sizeof(sel_union)); /* waste is insignificant */
  grpnode->n = 0;

  sidnode = malloc(sizeof(selection_node));
  sidnode->u = malloc(i*sizeof(sel_union)); /* waste is insignificant */
  sidnode->n = 0;

  while(i--){
    char *data;
    data = *(argp++);
    switch(*data){
    default:   err = parse_pid(  data, pidnode->u + pidnode->n++); break;
    case '-':  err = parse_pid(++data, grpnode->u + grpnode->n++); break;
    case '+':  err = parse_pid(++data, sidnode->u + sidnode->n++); break;
    }
    if(err) return err;     /* the node gets freed with the list */
  }

  if(pidnode->n){
    pidnode->next = selection_list;
    selection_list = pidnode;
    selection_list->typecode = SEL_PID;
  }  /* else free both parts */

  if(grpnode->n){
    grpnode->next = selection_list;
    selection_list = grpnode;
    selection_list->typecode = SEL_PGRP;
  }  /* else free both parts */

  if(sidnode->n){
    sidnode->next = selection_list;
    selection_list = sidnode;
    selection_list->typecode = SEL_SESS;
  }  /* else free both parts */

  return NULL;
}

/************** misc stuff ***********/

static void reset_parser(void){
  w_count = 0;
}

static int arg_type(const char *str){
  int tmp = str[0];
  if((tmp>='a') && (tmp<='z'))   return ARG_BSD;
  if((tmp>='A') && (tmp<='Z'))   return ARG_BSD;
  if((tmp>='0') && (tmp<='9'))   return ARG_PID;
  if(tmp=='+')                   return ARG_SESS;
  if(tmp!='-')                   return ARG_FAIL;
  tmp = str[1];
  if((tmp>='a') && (tmp<='z'))   return ARG_SYSV;
  if((tmp>='A') && (tmp<='Z'))   return ARG_SYSV;
  if((tmp>='0') && (tmp<='9'))   return ARG_PGRP;
  if(tmp!='-')                   return ARG_FAIL;
  tmp = str[2];
  if((tmp>='a') && (tmp<='z'))   return ARG_GNU;
  if((tmp>='A') && (tmp<='Z'))   return ARG_GNU;
  if(tmp=='\0')                  return ARG_END;
                                 return ARG_FAIL;
}

/* First assume sysv, because that is the POSIX and Unix98 standard. */
static const char *parse_all_options(void){
  const char *err = NULL;
  int at;
  while(++thisarg < ps_argc){
  trace("parse_all_options calling arg_type for \"%s\"\n", ps_argv[thisarg]);
    at = arg_type(ps_argv[thisarg]);
    trace("ps_argv[thisarg] is %s\n", ps_argv[thisarg]);
    if(at != ARG_SYSV) not_pure_unix = 1;
    switch(at){
    case ARG_GNU:
      err = parse_gnu_option();
      break;
    case ARG_SYSV:
      if(!force_bsd){   /* else go past case ARG_BSD */
        err = parse_sysv_option();
        break;
    case ARG_BSD:
        if(force_bsd && !(personality & PER_FORCE_BSD)) return "way bad";
      }
      prefer_bsd_defaults = 1;
      err = parse_bsd_option();
      break;
    case ARG_PGRP:
    case ARG_SESS:
    case ARG_PID:
      prefer_bsd_defaults = 1;
      err = parse_trailing_pids();
      break;
    case ARG_END:
    case ARG_FAIL:
      trace("              FAIL/END on [%s]\n",ps_argv[thisarg]);
      return "Garbage option.";
      break;
    default:
      printf("                  ?    %s\n",ps_argv[thisarg]);
      return "Something broke.";
    } /* switch */
    if(err) return err;
  } /* while */
  return NULL;
}

static void choose_dimensions(void){
  if(w_count && (screen_cols<132)) screen_cols=132;
  if(w_count>1) screen_cols=OUTBUF_SIZE;
  /* perhaps --html and --null should set unlimited width */
}

static const char *thread_option_check(void){
  if(!thread_flags){
    thread_flags = TF_show_proc;
    return NULL;
  }

  if(forest_type){
    return "Thread display conflicts with forest display.";
  }
  //thread_flags |= TF_no_forest;

  if((thread_flags&TF_B_H) && (thread_flags&(TF_B_m|TF_U_m)))
    return "Thread flags conflict; can't use H with m or -m.";
  if((thread_flags&TF_B_m) && (thread_flags&TF_U_m))
    return "Thread flags conflict; can't use both m and -m.";
  if((thread_flags&TF_U_L) && (thread_flags&TF_U_T))
    return "Thread flags conflict; can't use both -L and -T.";

  if(thread_flags&TF_B_H) thread_flags |= (TF_show_proc|TF_loose_tasks);
  if(thread_flags&(TF_B_m|TF_U_m)) thread_flags |= (TF_show_proc|TF_show_task|TF_show_both);

  if(thread_flags&(TF_U_T|TF_U_L)){
    if(thread_flags&(TF_B_m|TF_U_m|TF_B_H)){
      // Got a thread style, so format modification is a requirement?
      // Maybe -T/-L has H thread style though. (sorting interaction?)
      //return "Huh? Tell procps-feedback@lists.sf.net what you expected.";
      thread_flags |= TF_must_use;
    }else{
      // using -L/-T thread style, so format from elsewhere is OK
      thread_flags |= TF_show_task;  // or like the H option?
      //thread_flags |= TF_no_sort;
    }
  }

  return NULL;
}

int arg_parse(int argc, char *argv[]){
  const char *err = NULL;
  const char *err2 = NULL;
  ps_argc = argc;
  ps_argv = argv;
  thisarg = 0;

  if(personality & PER_FORCE_BSD) goto try_bsd;

  err = parse_all_options();
  if(err) goto try_bsd;
  err = thread_option_check();
  if(err) goto try_bsd;
  err = process_sf_options(!not_pure_unix);
  if(err) goto try_bsd;
  err = select_bits_setup();
  if(err) goto try_bsd;

  choose_dimensions();
  return 0;

try_bsd:
  trace("--------- now try BSD ------\n");

  reset_global();
  reset_parser();
  reset_sortformat();
  format_flags = 0;
  ps_argc = argc;
  ps_argv = argv;
  thisarg = 0;
  /* no need to reset flagptr */
  not_pure_unix=1;
  force_bsd=1;
  prefer_bsd_defaults=1;
  if(!( (PER_OLD_m|PER_BSD_m) & personality )) /* if default m setting... */
    personality |= PER_OLD_m; /* Prefer old Linux over true BSD. */
  /* Do not set PER_FORCE_BSD! It is tested below. */

  err2 = parse_all_options();
  if(err2) goto total_failure;
  err2 = thread_option_check();
  if(err2) goto total_failure;
  err2 = process_sf_options(!not_pure_unix);
  if(err2) goto total_failure;
  err2 = select_bits_setup();
  if(err2) goto total_failure;

  // Feel a need to patch this out? First of all, read the FAQ.
  // Second of all, talk to me. Without this warning, people can
  // get seriously confused. Ask yourself if users would freak out
  // about "ps -aux" suddenly changing behavior if a user "x" were
  // added to the system.
  //
  // Also, a "-x" option is coming. It's already there in fact,
  // for some non-default personalities. So "ps -ax" will parse
  // as SysV options... and you're screwed if you've been patching
  // out the friendly warning. Cut-over is likely to be in 2005.
  if(!(personality & PER_FORCE_BSD))
    fprintf(stderr, "Warning: bad ps syntax, perhaps a bogus '-'? See http://procps.sf.net/faq.html\n");
  // Remember: contact albert@users.sf.net or procps-feedback@lists.sf.net
  // if you should feel tempted. Be damn sure you understand all
  // the issues. The same goes for other stuff too, BTW. Please ask.
  // I'm happy to justify various implementation choices.

  choose_dimensions();
  return 0;

total_failure:
  reset_parser();
  if(personality & PER_FORCE_BSD) fprintf(stderr, "ERROR: %s\n", err2);
  else fprintf(stderr, "ERROR: %s\n", err);
  fwrite(help_message,1,strlen(help_message),stderr);
  exit(1);
  /* return 1; */ /* useless */
}
