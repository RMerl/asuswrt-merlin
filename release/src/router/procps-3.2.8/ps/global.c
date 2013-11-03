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
#include <stdlib.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
                     

#include "common.h"

#include <sys/sysmacros.h>
#include "../proc/wchan.h"
#include "../proc/version.h"
#include "../proc/sysinfo.h"


#ifndef __GNU_LIBRARY__
#define __GNU_LIBRARY__ -1
#endif
#ifndef __GLIBC__
#define __GLIBC__ -1
#endif
#ifndef __GLIBC_MINOR__
#define __GLIBC_MINOR__ -1
#endif


static const char * saved_personality_text = "You found a bug!";

int             all_processes = -1;
const char     *bsd_j_format = (const char *)0xdeadbeef;
const char     *bsd_l_format = (const char *)0xdeadbeef;
const char     *bsd_s_format = (const char *)0xdeadbeef;
const char     *bsd_u_format = (const char *)0xdeadbeef;
const char     *bsd_v_format = (const char *)0xdeadbeef;
int             bsd_c_option = -1;
int             bsd_e_option = -1;
uid_t           cached_euid = -1;
dev_t           cached_tty = -1;
char            forest_prefix[4 * 32*1024 + 100];     // FIXME
int             forest_type = -1;
unsigned        format_flags = 0xffffffff;   /* -l -f l u s -j... */
format_node    *format_list = (format_node *)0xdeadbeef; /* digested formatting options */
unsigned        format_modifiers = 0xffffffff;   /* -c -j -y -P -L... */
int             header_gap = -1;
int             header_type = -1;
int             include_dead_children = -1;
int             lines_to_next_header = -1;
const char     *namelist_file = (const char *)0xdeadbeef;
int             negate_selection = -1;
int             running_only = -1;
int             page_size = -1;  // "int" for math reasons?
unsigned        personality = 0xffffffff;
int             prefer_bsd_defaults = -1;
int             screen_cols = -1;
int             screen_rows = -1;
unsigned long   seconds_since_boot = -1;
selection_node *selection_list = (selection_node *)0xdeadbeef;
unsigned        simple_select = 0xffffffff;
sort_node      *sort_list = (sort_node *)0xdeadbeef; /* ready-to-use sort list */
const char     *sysv_f_format = (const char *)0xdeadbeef;
const char     *sysv_fl_format = (const char *)0xdeadbeef;
const char     *sysv_j_format = (const char *)0xdeadbeef;
const char     *sysv_l_format = (const char *)0xdeadbeef;
unsigned        thread_flags = 0xffffffff;
int             unix_f_option = -1;
int             user_is_number = -1;
int             wchan_is_number = -1;


static void reset_selection_list(void){
  selection_node *old;
  selection_node *walk = selection_list;
  if(selection_list == (selection_node *)0xdeadbeef){
    selection_list = NULL;
    return;
  }
  while(walk){
    old = walk;
    walk = old->next;
    free(old->u);
    free(old);
  }
  selection_list = NULL;
}

// The rules:
// 1. Defaults are implementation-specific. (ioctl,termcap,guess)
// 2. COLUMNS and LINES override the defaults. (standards compliance)
// 3. Command line options override everything else.
// 4. Actual output may be more if the above is too narrow.
//
// SysV tends to spew semi-wide output in all cases. The args
// will be limited to 64 or 80 characters, without regard to
// screen size. So lines of 120 to 160 chars are normal.
// Tough luck if you want more or less than that! HP-UX has a
// new "-x" option for 1024-char args in place of comm that
// we'll implement at some point.
//
// BSD tends to make a good effort, then fall back to 80 cols.
// Use "ww" to get infinity. This is nicer for "ps | less"
// and "watch ps". It can run faster too.
static void set_screen_size(void){
  struct winsize ws;
  char *columns; /* Unix98 environment variable */
  char *lines;   /* Unix98 environment variable */

  do{
    int fd;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && ws.ws_col>0 && ws.ws_row>0) break;
    if(ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) != -1 && ws.ws_col>0 && ws.ws_row>0) break;
    if(ioctl(STDIN_FILENO,  TIOCGWINSZ, &ws) != -1 && ws.ws_col>0 && ws.ws_row>0) break;
    fd = open("/dev/tty", O_NOCTTY|O_NONBLOCK|O_RDONLY);
    if(fd != -1){
      int ret = ioctl(fd, TIOCGWINSZ, &ws);
      close(fd);
      if(ret != -1 && ws.ws_col>0 && ws.ws_row>0) break;
    }
    // TODO: ought to do tgetnum("co") and tgetnum("li") here
    ws.ws_col = 80;
    ws.ws_row = 24;
  }while(0);
  screen_cols = ws.ws_col;  // hmmm, NetBSD subtracts 1
  screen_rows = ws.ws_row;

  // TODO: delete this line
  if(!isatty(STDOUT_FILENO)) screen_cols = OUTBUF_SIZE;

  columns = getenv("COLUMNS");
  if(columns && *columns){
    long t;
    char *endptr;
    t = strtol(columns, &endptr, 0);
    if(!*endptr && (t>0) && (t<(long)OUTBUF_SIZE)) screen_cols = (int)t;
  }

  lines   = getenv("LINES");
  if(lines && *lines){
    long t;
    char *endptr;
    t = strtol(lines, &endptr, 0);
    if(!*endptr && (t>0) && (t<(long)OUTBUF_SIZE)) screen_rows = (int)t;
  }

  if((screen_cols<9) || (screen_rows<2))
    fprintf(stderr,"Your %dx%d screen size is bogus. Expect trouble.\n",
      screen_cols, screen_rows
    );
}

/**************** personality control **************/

typedef struct personality_table_struct {
  const char *name; /* personality name */
  const void *jump; /* See gcc extension info.   :-)   */
} personality_table_struct;

static int compare_personality_table_structs(const void *a, const void *b){
  return strcasecmp(((const personality_table_struct*)a)->name,((const personality_table_struct*)b)->name);
}

static const char *set_personality(void){
  const char *s;
  size_t sl;
  char buf[16];
  personality_table_struct findme = { buf, NULL};
  personality_table_struct *found;
  static const personality_table_struct personality_table[] = {
  {"390",      &&case_390},
  {"aix",      &&case_aix},
  {"bsd",      &&case_bsd},
  {"compaq",   &&case_compaq},
  {"debian",   &&case_debian},
  {"default",  &&case_default},
  {"digital",  &&case_digital},
  {"gnu",      &&case_gnu},
  {"hp",       &&case_hp},
  {"hpux",     &&case_hpux},
  {"irix",     &&case_irix},
  {"linux",    &&case_linux},
  {"old",      &&case_old},
  {"os390",    &&case_os390},
  {"posix",    &&case_posix},
  {"s390",     &&case_s390},
  {"sco",      &&case_sco},
  {"sgi",      &&case_sgi},
  {"solaris2", &&case_solaris2},
  {"sunos4",   &&case_sunos4},
  {"svr4",     &&case_svr4},
  {"sysv",     &&case_sysv},
  {"tru64",    &&case_tru64},
  {"unix",     &&case_unix},
  {"unix95",   &&case_unix95},
  {"unix98",   &&case_unix98},
  {"unknown",  &&case_unknown}
  };
  const int personality_table_count = sizeof(personality_table)/sizeof(personality_table_struct);

  personality = 0;
  prefer_bsd_defaults = 0;

  bsd_j_format = "OL_j";
  bsd_l_format = "OL_l";
  bsd_s_format = "OL_s";
  bsd_u_format = "OL_u";
  bsd_v_format = "OL_v";

  /* When these are NULL, the code does SysV output modifier logic */
  sysv_f_format  = NULL;
  sysv_fl_format = NULL;
  sysv_j_format  = NULL;
  sysv_l_format  = NULL;

  s = getenv("PS_PERSONALITY");
  if(!s || !*s) s = getenv("CMD_ENV");
  if(!s || !*s) s="unknown";   /* "Do The Right Thing[tm]" */
  if(getenv("I_WANT_A_BROKEN_PS")) s="old";
  sl = strlen(s);
  if(sl > 15) return "Environment specified an unknown personality.";
  strncpy(buf, s, sl);
  buf[sl] = '\0';
  saved_personality_text = strdup(buf);

  found = bsearch(&findme, personality_table, personality_table_count,
      sizeof(personality_table_struct), compare_personality_table_structs
  );

  if(!found) return "Environment specified an unknown personality.";

  goto *(found->jump);    /* See gcc extension info.  :-)   */

  case_bsd:
    personality = PER_FORCE_BSD | PER_BSD_h | PER_BSD_m;
    prefer_bsd_defaults = 1;
    bsd_j_format = "FB_j";
    bsd_l_format = "FB_l";
    /* bsd_s_format not used */
    bsd_u_format = "FB_u";
    bsd_v_format = "FB_v";
    return NULL;

  case_old:
    personality = PER_FORCE_BSD | PER_OLD_m;
    prefer_bsd_defaults = 1;
    return NULL;

  case_debian:  /* Toss this? They don't seem to care much. */
  case_gnu:
    personality = PER_GOOD_o | PER_OLD_m;
    prefer_bsd_defaults = 1;
    sysv_f_format  = "RD_f";
    /* sysv_fl_format = "RD_fl"; */   /* old Debian ps can't do this! */
    sysv_j_format  = "RD_j";
    sysv_l_format  = "RD_l";
    return NULL;

  case_linux:
    personality = PER_GOOD_o | PER_ZAP_ADDR | PER_SANE_USER;
    return NULL;

  case_default: /* use defaults for ps, ignoring other environment variables */
    return NULL;

  case_unknown: /* defaults, but also check inferior environment variables */
    if(
      getenv("UNIX95")     /* Irix */
      || getenv("POSIXLY_CORRECT")  /* most gnu stuff */
      || (getenv("POSIX2") && !strcmp(getenv("POSIX2"), "on")) /* Unixware 7 */
    ) personality = PER_BROKEN_o;
    return NULL;

  case_aix:
    bsd_j_format = "FB_j";
    bsd_l_format = "FB_l";
    /* bsd_s_format not used */
    bsd_u_format = "FB_u";
    bsd_v_format = "FB_v";
    return NULL;

  case_tru64:
  case_compaq:
  case_digital:
    // no PER_NO_DEFAULT_g even though man page claims it
    // Reality: the g is a NOP
    personality = PER_GOOD_o | PER_BSD_h;
    prefer_bsd_defaults = 1;
    sysv_f_format  = "F5FMT";
    sysv_fl_format = "FL5FMT";
    sysv_j_format  = "JFMT";
    sysv_l_format  = "L5FMT";
    bsd_j_format = "JFMT";
    bsd_l_format = "LFMT";
    bsd_s_format = "SFMT";
    bsd_u_format = "UFMT";
    bsd_v_format = "VFMT";
    return NULL;

  case_sunos4:
    personality = PER_NO_DEFAULT_g;
    prefer_bsd_defaults = 1;
    bsd_j_format = "FB_j";
    bsd_l_format = "FB_l";
    /* bsd_s_format not used */
    bsd_u_format = "FB_u";
    bsd_v_format = "FB_v";
    return NULL;

  case_irix:
  case_sgi:
    s = getenv("_XPG");
    if(s && s[0]>'0' && s[0]<='9') personality = PER_BROKEN_o;
    else personality = PER_IRIX_l;
    return NULL;

  case_os390:  /* IBM's OS/390 OpenEdition on the S/390 mainframe */
  case_s390:
  case_390:
    sysv_j_format  = "J390";  /* don't know what -jl and -jf do */
    return NULL;

  case_hp:
  case_hpux:
    personality = PER_BROKEN_o | PER_HPUX_x;
    return NULL;

  case_svr4:
  case_sysv:
  case_sco:
    personality = PER_BROKEN_o | PER_SVR4_x;
    return NULL;

  case_posix:
  case_solaris2:
  case_unix95:
  case_unix98:
  case_unix:
    personality = PER_BROKEN_o;
    return NULL;
}


/************ Call this to reinitialize everything ***************/
void reset_global(void){
  static proc_t p;
  reset_selection_list();
  look_up_our_self(&p);
  set_screen_size();
  set_personality();
  
  all_processes         = 0;
  bsd_c_option          = 0;
  bsd_e_option          = 0;
  cached_euid           = geteuid();
  cached_tty            = p.tty;
/* forest_prefix must be all zero because of POSIX */
  forest_type           = 0;
  format_flags          = 0;   /* -l -f l u s -j... */
  format_list           = NULL; /* digested formatting options */
  format_modifiers      = 0;   /* -c -j -y -P -L... */
  header_gap            = -1;  /* send lines_to_next_header to -infinity */
  header_type           = HEAD_SINGLE;
  include_dead_children = 0;
  lines_to_next_header  = 1;
  namelist_file         = NULL;
  negate_selection      = 0;
  page_size             = getpagesize();
  running_only          = 0;
  seconds_since_boot    = uptime(0,0);
  selection_list        = NULL;
  simple_select         = 0;
  sort_list             = NULL;
  thread_flags          = 0;
  unix_f_option         = 0;
  user_is_number        = 0;
  wchan_is_number       = 0;
}

static const char archdefs[] =
#ifdef __alpha__
" alpha"
#endif
#ifdef __arm__
" arm"
#endif
#ifdef __hppa__
" hppa"
#endif
#ifdef __i386__
" i386"
#endif
#ifdef __ia64__
" ia64"
#endif
#ifdef __mc68000__
" mc68000"
#endif
#ifdef __mips64__
" mips64"
#endif
#ifdef __mips__
" mips"
#endif
#ifdef __powerpc__
" powerpc"
#endif
#ifdef __sh3__
" sh3"
#endif
#ifdef __sh__
" sh"
#endif
#ifdef __sparc__
" sparc"
#endif
#ifdef __sparc_v9__
" sparc_v9"
#endif
#ifdef __x86_64__
" x86_64"
#endif
"";

/*********** spew variables ***********/
void self_info(void){
  fprintf(stderr,
    "BSD j    %s\n"
    "BSD l    %s\n"
    "BSD s    %s\n"
    "BSD u    %s\n"
    "BSD v    %s\n"
    "SysV -f  %s\n"
    "SysV -fl %s\n"
    "SysV -j  %s\n"
    "SysV -l  %s\n"
    "\n",
    bsd_j_format   ? bsd_j_format   : "(none)",
    bsd_l_format   ? bsd_l_format   : "(none)",
    bsd_s_format   ? bsd_s_format   : "(none)",
    bsd_u_format   ? bsd_u_format   : "(none)",
    bsd_v_format   ? bsd_v_format   : "(none)",
    sysv_f_format  ? sysv_f_format  : "(none)",
    sysv_fl_format ? sysv_fl_format : "(none)",
    sysv_j_format  ? sysv_j_format  : "(none)",
    sysv_l_format  ? sysv_l_format  : "(none)"
  );

  display_version();
  fprintf(stderr, "Linux version %d.%d.%d\n",
    LINUX_VERSION_MAJOR(linux_version_code),
    LINUX_VERSION_MINOR(linux_version_code),
    LINUX_VERSION_PATCH(linux_version_code)
  );
  /* __libc_print_version(); */  /* how can we get the run-time version? */
  fprintf(stderr, "Compiled with: glibc %d.%d, gcc %d.%d\n\n",
    __GLIBC__, __GLIBC_MINOR__, __GNUC__, __GNUC_MINOR__
  );

  fprintf(stderr,
    "header_gap=%d lines_to_next_header=%d\n"
    "screen_cols=%d screen_rows=%d\n"
    "\n",
    header_gap, lines_to_next_header,
    screen_cols, screen_rows
  );

  fprintf(stderr,
    "personality=0x%08x (from \"%s\")\n"
    "EUID=%d TTY=%d,%d Hertz=%Ld page_size=%d\n",
    personality, saved_personality_text,
    cached_euid, (int)major(cached_tty), (int)minor(cached_tty), Hertz,
    (int)(page_size)
  );

  fprintf(stderr,
    "sizeof(proc_t)=%d sizeof(long)=%d sizeof(KLONG)=%d\n",
    (int)sizeof(proc_t), (int)sizeof(long), (int)sizeof(KLONG)
  );

  fprintf(stderr, "archdefs:%s\n", archdefs);

  open_psdb(namelist_file);
  fprintf(stderr,"namelist_file=\"%s\"\n",namelist_file?namelist_file:"<no System.map file>");
}
