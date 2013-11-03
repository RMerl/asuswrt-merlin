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

#ifndef PROCPS_PS_H
#define PROCPS_PS_H

#include "../proc/procps.h"
#include "../proc/escape.h"
#include "../proc/readproc.h"

#if 0
#define trace(args...) printf(## args)
#else
#define trace(args...)
#endif


/***************** GENERAL DEFINE ********************/


/* selection list */
#define SEL_RUID 1
#define SEL_EUID 2
#define SEL_SUID 3
#define SEL_FUID 4
#define SEL_RGID 5
#define SEL_EGID 6
#define SEL_SGID 7
#define SEL_FGID 8
#define SEL_PGRP 9
#define SEL_PID  10
#define SEL_TTY  11
#define SEL_SESS 12
#define SEL_COMM 13
#define SEL_PPID 14

/* Since an enum could be smashed by a #define, it would be bad. */
#define U98  0 /* Unix98 standard */    /* This must be 0 */
#define XXX  1 /* Common extension */
#define DEC  2 /* Digital Unix */
#define AIX  3 /* AIX */
#define SCO  4 /* SCO */
#define LNX  5 /* Linux original :-) */
#define BSD  6 /* FreeBSD and OpenBSD */
#define SUN  7 /* SunOS 5 (Solaris) */
#define HPU  8 /* HP-UX */
#define SGI  9 /* Irix */
#define SOE 10 /* IBM's S/390 OpenEdition */
#define TST 11 /* test code */

/*
 * Try not to overflow the output buffer:
 *    32 pages for env+cmd
 *    64 kB pages on IA-64
 *    4 chars for "\377", or 1 when mangling to '?'  (ESC_STRETCH)
 *    plus some slack for other stuff
 * That is about 8.5 MB on IA-64, or 0.6 MB on i386
 *
 * Sadly, current kernels only supply one page of env/command data.
 * The buffer is now protected with a guard page, and via other means
 * to avoid hitting the guard page.
 */

/* output buffer size */
#define OUTBUF_SIZE (2 * 64*1024 * ESC_STRETCH)

/******************* PS DEFINE *******************/

// Column flags
// Justification control for flags field comes first.
#define CF_JUST_MASK                0x0f
//      CF_AIXHACK                     0
#define CF_USER                        1 // left if text, right if numeric
#define CF_LEFT                        2
#define CF_RIGHT                       3
#define CF_UNLIMITED                   4
#define CF_WCHAN                       5 // left if text, right if numeric
#define CF_SIGNAL                      6 // right in 9, or 16 if screen_cols>107
// Then the other flags
#define CF_PIDMAX             0x00000010 // react to pid_max
// Only one allowed; use separate bits to catch errors.
#define CF_PRINT_THREAD_ONLY  0x10000000
#define CF_PRINT_PROCESS_ONLY 0x20000000
#define CF_PRINT_EVERY_TIME   0x40000000
#define CF_PRINT_AS_NEEDED    0x80000000 // means we have no clue, so assume EVERY TIME
#define CF_PRINT_MASK         0xf0000000

#define needs_for_select (PROC_FILLSTAT | PROC_FILLSTATUS)

/* thread_flags */
#define TF_B_H         0x0001
#define TF_B_m         0x0002
#define TF_U_m         0x0004
#define TF_U_T         0x0008
#define TF_U_L         0x0010
#define TF_show_proc   0x0100  // show the summary line
#define TF_show_task   0x0200  // show the per-thread lines
#define TF_show_both   0x0400  // distinct proc/task format lists
#define TF_loose_tasks 0x0800  // let sorting break up task groups (BSDish)
#define TF_no_sort     0x1000  // don't know if thread-grouping should survive a sort
#define TF_no_forest   0x2000  // don't see how to do threads w/ forest option
#define TF_must_use    0x4000  // options only make sense if LWP/SPID column added

/* personality control flags */
#define PER_BROKEN_o      0x0001
#define PER_BSD_h         0x0002
#define PER_BSD_m         0x0004
#define PER_IRIX_l        0x0008
#define PER_FORCE_BSD     0x0010
#define PER_GOOD_o        0x0020
#define PER_OLD_m         0x0040
#define PER_NO_DEFAULT_g  0x0080
#define PER_ZAP_ADDR      0x0100
#define PER_SANE_USER     0x0200
#define PER_HPUX_x        0x0400
#define PER_SVR4_x        0x0800
#define PER_BSD_COLS      0x1000
#define PER_UNIX_COLS     0x2000

/* Simple selections by bit mask */
#define SS_B_x 0x01
#define SS_B_g 0x02
#define SS_U_d 0x04
#define SS_U_a 0x08
#define SS_B_a 0x10

/* predefined format flags such as:  -l -f l u s -j */
#define FF_Uf 0x0001 /* -f */
#define FF_Uj 0x0002 /* -j */
#define FF_Ul 0x0004 /* -l */
#define FF_Bj 0x0008 /* j */
#define FF_Bl 0x0010 /* l */
#define FF_Bs 0x0020 /* s */
#define FF_Bu 0x0040 /* u */
#define FF_Bv 0x0080 /* v */
#define FF_LX 0x0100 /* X */
#define FF_Lm 0x0200 /* m */  /* overloaded: threads, sort, format */
#define FF_Fc 0x0400 /* --context */  /* Flask security context format */

/* predefined format modifier flags such as:  -l -f l u s -j */
#define FM_c 0x0001 /* -c */
#define FM_j 0x0002 /* -j */  /* only set when !sysv_j_format */
#define FM_y 0x0004 /* -y */
//#define FM_L 0x0008 /* -L */
#define FM_P 0x0010 /* -P */
#define FM_M 0x0020 /* -M */
//#define FM_T 0x0040 /* -T */
#define FM_F 0x0080 /* -F */  /* -F also sets the regular -f flags */

/* sorting & formatting */
/* U,B,G is Unix,BSD,Gnu and then there is the option itself */
#define SF_U_O      1
#define SF_U_o      2
#define SF_B_O      3
#define SF_B_o      4
#define SF_B_m      5       /* overloaded: threads, sort, format */
#define SF_G_sort   6
#define SF_G_format 7

/* headers */
#define HEAD_SINGLE 0  /* default, must be 0 */
#define HEAD_NONE   1
#define HEAD_MULTI  2


/********************** GENERAL TYPEDEF *******************/

/* Other fields that might be useful:
 *
 * char *name;     user-defined column name (format specification)
 * int reverse;    sorting in reverse (sort specification)
 *
 * name in place of u
 * reverse in place of n
 */

typedef union sel_union {
  pid_t pid;
  pid_t ppid;
  uid_t uid;
  gid_t gid;
  dev_t tty;
  char  cmd[16];  /* this is _not_ \0 terminated */
} sel_union;

typedef struct selection_node {
  struct selection_node *next;
  sel_union *u;  /* used if selection type has a list of values */
  int n;         /* used if selection type has a list of values */
  int typecode;
} selection_node;

typedef struct sort_node {
  struct sort_node *next;
  int (*sr)(const proc_t* P, const proc_t* Q); /* sort function */
  int reverse;   /* can sort backwards */
  int typecode;
  int need;
} sort_node;

typedef struct format_node {
  struct format_node *next;
  char *name;                             /* user can override default name */
  int (*pr)(char *restrict const outbuf, const proc_t *restrict const pp); // print function
/*  int (* const sr)(const proc_t* P, const proc_t* Q); */ /* sort function */
  int width;
  int need;
  int vendor;                             /* Vendor that invented this */
  int flags;
  int typecode;
} format_node;

typedef struct format_struct {
  const char *spec; /* format specifier */
  const char *head; /* default header in the POSIX locale */
  int (* const pr)(char *restrict const outbuf, const proc_t *restrict const pp); // print function
  int (* const sr)(const proc_t* P, const proc_t* Q); /* sort function */
  const int width;
  const int need;       /* data we will need (files to read, etc.) */
  const int vendor; /* Where does this come from? */
  const int flags;
} format_struct;

/* though ps-specific, needed by general file */
typedef struct macro_struct {
  const char *spec; /* format specifier */
  const char *head; /* default header in the POSIX locale */
} macro_struct;

/**************** PS TYPEDEF ***********************/

typedef struct aix_struct {
  const int   desc; /* 1-character format code */
  const char *spec; /* format specifier */
  const char *head; /* default header in the POSIX locale */
} aix_struct;

typedef struct shortsort_struct {
  const int   desc; /* 1-character format code */
  const char *spec; /* format specifier */
} shortsort_struct;

/* Save these options for later: -o o -O O --format --sort */
typedef struct sf_node {
  struct sf_node *next;  /* next arg */
  format_node *f_cooked;  /* convert each arg alone, then merge */
  sort_node   *s_cooked;  /* convert each arg alone, then merge */
  char *sf;
  int sf_code;
} sf_node;

/********************* UNDECIDED GLOBALS **************/

/* output.c */
extern void show_one_proc(const proc_t *restrict const p, const format_node *restrict fmt);
extern void print_format_specifiers(void);
extern const aix_struct *search_aix_array(const int findme);
extern const shortsort_struct *search_shortsort_array(const int findme);
extern const format_struct *search_format_array(const char *findme);
extern const macro_struct *search_macro_array(const char *findme);
extern void init_output(void);
extern int pr_nop(char *restrict const outbuf, const proc_t *restrict const pp);

/* global.c */
extern void reset_global(void);

/* global.c */
extern int             all_processes;
extern const char     *bsd_j_format;
extern const char     *bsd_l_format;
extern const char     *bsd_s_format;
extern const char     *bsd_u_format;
extern const char     *bsd_v_format;
extern int             bsd_c_option;
extern int             bsd_e_option;
extern uid_t           cached_euid;
extern dev_t           cached_tty;
extern char            forest_prefix[4 * 32*1024 + 100];
extern int             forest_type;
extern unsigned        format_flags;     /* -l -f l u s -j... */
extern format_node    *format_list; /* digested formatting options */
extern unsigned        format_modifiers; /* -c -j -y -P -L... */
extern int             header_gap;
extern int             header_type; /* none, single, multi... */
extern int             include_dead_children;
extern int             lines_to_next_header;
extern int             max_line_width;
extern const char     *namelist_file;
extern int             negate_selection;
extern int             page_size;  // "int" for math reasons?
extern unsigned        personality;
extern int             prefer_bsd_defaults;
extern int             running_only;
extern int             screen_cols;
extern int             screen_rows;
extern unsigned long   seconds_since_boot;
extern selection_node *selection_list;
extern unsigned        simple_select;
extern sort_node      *sort_list;
extern const char     *sysv_f_format;
extern const char     *sysv_fl_format;
extern const char     *sysv_j_format;
extern const char     *sysv_l_format;
extern unsigned        thread_flags;
extern int             unix_f_option;
extern int             user_is_number;
extern int             wchan_is_number;

/************************* PS GLOBALS *********************/

/* sortformat.c */
extern int defer_sf_option(const char *arg, int source);
extern const char *process_sf_options(int localbroken);
extern void reset_sortformat(void);

/* select.c */
extern int want_this_proc(proc_t *buf);
extern const char *select_bits_setup(void);

/* help.c */
extern const char *help_message;

/* global.c */
extern void self_info(void);

/* parser.c */
extern int arg_parse(int argc, char *argv[]);

#endif
