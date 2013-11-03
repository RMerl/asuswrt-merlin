// top.h - Header file:         show Linux processes
//
// Copyright (c) 2002, by:      James C. Warner
//    All rights reserved.      8921 Hilloway Road
//                              Eden Prairie, Minnesota 55347 USA
//                             <warnerjc@worldnet.att.net>
//
// This file may be used subject to the terms and conditions of the
// GNU Library General Public License Version 2, or any later version
// at your option, as published by the Free Software Foundation.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
// 
// For their contributions to this program, the author wishes to thank:
//    Albert D. Cahalan, <albert@users.sf.net>
//    Craig Small, <csmall@small.dropbear.id.au>
//
// Changes by Albert Cahalan, 2002, 2004.

#ifndef _Itop
#define _Itop

// Defines intended to be experimented with ------------------------
//#define CASEUP_HEXES    // show any hex values in upper case
//#define CASEUP_SCALE    // show scaled time/num suffix upper case
//#define CASEUP_SUMMK    // show memory summary kilobytes with 'K'
//#define SORT_SUPRESS    // *attempt* to reduce qsort overhead
//#define WARN_NOT_SMP    // restrict '1' & 'I' commands to true smp

// Development/Debugging defines -----------------------------------
//#define ATEOJ_REPORT    // report a bunch of stuff, at end-of-job
//#define PRETEND2_5_X    // pretend we're linux 2.5.x (for IO-wait)
//#define PRETENDNOCAP    // use a terminal without essential caps
//#define STDOUT_IOLBF    // disable our own stdout _IOFBF override

#ifdef PRETEND2_5_X
#define linux_version_code LINUX_VERSION(2,5,43)
#endif

/*######  Some Miscellaneous constants  ##################################*/

// The default delay twix updates
#define DEF_DELAY  3.0

// The length of time a 'message' is displayed
#define MSG_SLEEP  2

// The default value for the 'k' (kill) request
#define DEF_SIGNAL  SIGTERM

// Specific process id monitoring support (command line only)
#define MONPIDMAX  20

// Power-of-two sizes lead to trouble; the largest power of
// two factor should be the cache line size. It'll mean the
// array indexing math gets slower, but cache aliasing is
// avoided.
#define CACHE_TWEAK_FACTOR 64

// Miscellaneous buffer sizes with liberal values -- mostly
// just to pinpoint source code usage/dependancies
#define SCREENMAX ( 512 + CACHE_TWEAK_FACTOR)
// the above might seem pretty stingy, until you consider that with every
// one of top's fields displayed we're talking a 160 byte column header --
// so that will provide for all fields plus a 350+ byte command line
#define WINNAMSIZ     4
#define CAPTABMAX     9
#define PFLAGSSIZ    32
#define CAPBUFSIZ    32
#define CLRBUFSIZ    64
#define GETBUFSIZ    32
#define TNYBUFSIZ    32
#define SMLBUFSIZ ( 256 + CACHE_TWEAK_FACTOR)
#define OURPATHSZ (1024 + CACHE_TWEAK_FACTOR)
#define MEDBUFSIZ (1024 + CACHE_TWEAK_FACTOR)
#define BIGBUFSIZ (2048 + CACHE_TWEAK_FACTOR)
#define USRNAMSIZ  GETBUFSIZ
#define ROWBUFSIZ  SCREENMAX + CLRBUFSIZ


/*######  Some Miscellaneous Macro definitions  ##########################*/

// Yield table size as 'int'
#define MAXTBL(t)  (int)(sizeof(t) / sizeof(t[0]))

// Used as return arguments in *some* of the sort callbacks
#define SORT_lt  ( Frame_srtflg > 0 ?  1 : -1 )
#define SORT_gt  ( Frame_srtflg > 0 ? -1 :  1 )
#define SORT_eq  0

// Used to create *most* of the sort callback functions
#define SCB_NUM1(f,n) \
   static int sort_ ## f (const proc_t **P, const proc_t **Q) { \
      if (        (*P)->n < (*Q)->n  ) return SORT_lt; \
      if (likely( (*P)->n > (*Q)->n )) return SORT_gt; \
      return SORT_eq; }
#define SCB_NUM2(f,n1,n2) \
   static int sort_ ## f (const proc_t **P, const proc_t **Q) { \
      if (        ((*P)->n1 - (*P)->n2) < ((*Q)->n1 - (*Q)->n2)  ) return SORT_lt; \
      if (likely( ((*P)->n1 - (*P)->n2) > ((*Q)->n1 - (*Q)->n2) )) return SORT_gt; \
      return SORT_eq; }
#define SCB_NUMx(f,n) \
   static int sort_ ## f (const proc_t **P, const proc_t **Q) { \
      return Frame_srtflg * ( (*Q)->n - (*P)->n ); }
#define SCB_STRx(f,s) \
   static int sort_ ## f (const proc_t **P, const proc_t **Q) { \
      return Frame_srtflg * strcmp((*Q)->s, (*P)->s); }

// The following two macros are used to 'inline' those portions of the
// display process requiring formatting, while protecting against any
// potential embedded 'millesecond delay' escape sequences.

// PUTT - Put to Tty (used in many places)
// - for temporary, possibly interactive, 'replacement' output
// - may contain ANY valid terminfo escape sequences
// - need NOT represent an entire screen row
#define PUTT(fmt,arg...) do { \
      char _str[ROWBUFSIZ]; \
      snprintf(_str, sizeof(_str), fmt, ## arg); \
      putp(_str); \
   } while (0)

// PUFF - Put for Frame (used in only 3 places)
// - for more permanent frame-oriented 'update' output
// - may NOT contain cursor motion terminfo escapes
// - assumed to represent a complete screen ROW
// - subject to optimization, thus MAY be discarded

// The evil version   (53892 byte stripped top, oddly enough)
#define _PUFF(fmt,arg...)                               \
do {                                                     \
   char _str[ROWBUFSIZ];                                   \
   int _len = 1 + snprintf(_str, sizeof(_str), fmt, ## arg);   \
   putp ( Batch ? _str :                                   \
   ({                                                 \
      char *restrict const _pse = &Pseudo_scrn[Pseudo_row++ * Pseudo_cols];  \
      memcmp(_pse, _str, _len) ? memcpy(_pse, _str, _len) : "\n";  \
   })                                \
   );                   \
} while (0)

// The good version  (53876 byte stripped top)
#define PUFF(fmt,arg...)                               \
do {                                                     \
   char _str[ROWBUFSIZ];                                   \
   char *_ptr;                                               \
   int _len = 1 + snprintf(_str, sizeof(_str), fmt, ## arg);   \
   if (Batch) _ptr = _str;                                   \
   else {                                                 \
      _ptr = &Pseudo_scrn[Pseudo_row++ * Pseudo_cols];  \
      if (memcmp(_ptr, _str, _len)) {                \
         memcpy(_ptr, _str, _len);                \
      } else {                                 \
         _ptr = "\n";                       \
      }                                 \
   }                                \
   putp(_ptr);                   \
} while (0)


/*------  Special Macros (debug and/or informative)  ---------------------*/

// Orderly end, with any sort of message - see fmtmk
#define debug_END(s) { \
           static void std_err (const char *); \
           fputs(Cap_clr_scr, stdout); \
           std_err(s); \
        }

// A poor man's breakpoint, if he's too lazy to learn gdb
#define its_YOUR_fault { *((char *)0) = '!'; }


/*######  Some Typedef's and Enum's  #####################################*/

// This typedef just ensures consistent 'process flags' handling
typedef unsigned FLG_t;

// These typedefs attempt to ensure consistent 'ticks' handling
typedef unsigned long long TIC_t;
typedef          long long SIC_t;

// Sort support, callback funtion signature
typedef int (*QFP_t)(const void *, const void *);

// This structure consolidates the information that's used
// in a variety of display roles.
typedef struct FLD_t {
   const char    keys [4];      // order: New-on New-off Old-on Old-off
   // misaligned on 64-bit, but part of a table -- oh well
   const char   *head;          // name for col heads + toggle/reorder fields
   const char   *fmts;          // sprintf format string for field display
   const int     width;         // field width, if applicable
   const int     scale;         // scale_num type, if applicable
   const QFP_t   sort;          // sort function
   const char   *desc;          // description for toggle/reorder fields
   const int     lflg;          // PROC_FILLxxx flag(s) needed by this field
} FLD_t;

// This structure stores one piece of critical 'history'
// information from one frame to the next -- we don't calc
// and save data that goes unused
typedef struct HST_t {
   TIC_t tics;
   int   pid;
} HST_t;

// This structure stores a frame's cpu tics used in history
// calculations.  It exists primarily for SMP support but serves
// all environments.
typedef struct CPU_t {
   TIC_t u, n, s, i, w, x, y, z; // as represented in /proc/stat
   TIC_t u_sav, s_sav, n_sav, i_sav, w_sav, x_sav, y_sav, z_sav; // in the order of our display
   unsigned id;  // the CPU ID number
} CPU_t;

// These 2 types support rcfile compatibility
typedef struct RCW_t {  // the 'window' portion of an rcfile
   FLG_t  sortindx;             // sort field, represented as a procflag
   int    winflags,             // 'view', 'show' and 'sort' mode flags
          maxtasks,             // user requested maximum, 0 equals all
          summclr,                      // color num used in summ info
          msgsclr,                      //        "       in msgs/pmts
          headclr,                      //        "       in cols head
          taskclr;                      //        "       in task rows
   char   winname [WINNAMSIZ],          // window name, user changeable
          fieldscur [PFLAGSSIZ];        // fields displayed and ordered
} RCW_t;

typedef struct RCF_t {  // the complete rcfile (new style)
   int    mode_altscr;          // 'A' - Alt display mode (multi task windows)
   int    mode_irixps;          // 'I' - Irix vs. Solaris mode (SMP-only)
   float  delay_time;           // 'd' or 's' - How long to sleep twixt updates
   int    win_index;            // Curwin, as index
   RCW_t  win [4];              // a 'WIN_t.rc' for each of the 4 windows
} RCF_t;

// The scaling 'type' used with scale_num() -- this is how
// the passed number is interpreted should scaling be necessary
enum scale_num {
   SK_no, SK_Kb, SK_Mb, SK_Gb, SK_Tb
};

// Flags for each possible field
enum pflag {
   P_PID, P_PPD, P_URR, P_UID, P_URE, P_GRP, P_TTY,
   P_PRI, P_NCE,
   P_CPN, P_CPU, P_TME, P_TM2,
   P_MEM, P_VRT, P_SWP, P_RES, P_COD, P_DAT, P_SHR,
   P_FLT, P_DRT,
   P_STA, P_CMD, P_WCH, P_FLG
};


///////////////////////////////////////////////////////////////////////////
// Special Section: multiple windows/field groups  -------------
// (kind of a header within a header:  constants, macros & types)

#define GROUPSMAX  4            // the max number of simultaneous windows
#define GRPNAMSIZ  WINNAMSIZ+2  // window's name + number as in: '#:...'

#define Flags_TOG  1            // these are used to direct wins_reflag
#define Flags_SET  2
#define Flags_OFF  3

// The Persistent 'Mode' flags!
// These are preserved in the rc file, as a single integer and the
// letter shown is the corresponding 'command' toggle

// 'View_' flags affect the summary (minimum), taken from 'Curwin'
#define View_CPUSUM  0x8000     // '1' - show combined cpu stats (vs. each)
#define View_LOADAV  0x4000     // 'l' - display load avg and uptime summary
#define View_STATES  0x2000     // 't' - display task/cpu(s) states summary
#define View_MEMORY  0x1000     // 'm' - display memory summary
#define View_NOBOLD  0x0001     // 'B' - disable 'bold' attribute globally

// 'Show_' & 'Qsrt_' flags are for task display in a visible window
#define Show_THREADS 0x10000    // 'H' - show threads in each task
#define Show_COLORS  0x0800     // 'z' - show in color (vs. mono)
#define Show_HIBOLD  0x0400     // 'b' - rows and/or cols bold (vs. reverse)
#define Show_HICOLS  0x0200     // 'x' - show sort column highlighted
#define Show_HIROWS  0x0100     // 'y' - show running tasks highlighted
#define Show_CMDLIN  0x0080     // 'c' - show cmdline vs. name
#define Show_CTIMES  0x0040     // 'S' - show times as cumulative
#define Show_IDLEPS  0x0020     // 'i' - show idle processes (all tasks)
#define Qsrt_NORMAL  0x0010     // 'R' - reversed column sort (high to low)

// these flag(s) have no command as such - they're for internal use
#define VISIBLE_tsk  0x0008     // tasks are showable when in 'Mode_altscr'
#define NEWFRAM_cwo  0x0004     // new frame (if anyone cares) - in Curwin
#define EQUWINS_cwo  0x0002     // rebalance tasks next frame (off 'i'/ 'n')
                                // ...set in Curwin, but impacts all windows

// Current-window-only flags -- always turned off at end-of-window!
#define FLGSOFF_cwo  EQUWINS_cwo | NEWFRAM_cwo

// Default flags if there's no rcfile to provide user customizations
#define DEF_WINFLGS ( \
   View_LOADAV | View_STATES | View_CPUSUM | View_MEMORY | View_NOBOLD | \
   Show_HIBOLD | Show_HIROWS | Show_IDLEPS | Qsrt_NORMAL | \
   VISIBLE_tsk \
)

        // Used to test/manipulate the window flags
#define CHKw(q,f)   (int)((q)->rc.winflags & (f))
#define TOGw(q,f)   (q)->rc.winflags ^=  (f)
#define SETw(q,f)   (q)->rc.winflags |=  (f)
#define OFFw(q,f)   (q)->rc.winflags &= ~(f)
#define VIZCHKc     (!Rc.mode_altscr || Curwin->rc.winflags & VISIBLE_tsk) \
                        ? 1 : win_warn()
#define VIZTOGc(f)  (!Rc.mode_altscr || Curwin->rc.winflags & VISIBLE_tsk) \
                        ? TOGw(Curwin, f) : win_warn()

// This structure stores configurable information for each window.
// By expending a little effort in its creation and user requested
// maintainence, the only real additional per frame cost of having
// windows is an extra sort -- but that's just on ptrs!
typedef struct WIN_t {
   struct WIN_t *next,                  // next window in window stack
                *prev;                  // prior window in window stack
   char       *captab [CAPTABMAX];      // captab needed by show_special
   int         winnum,                  // window's num (array pos + 1)
               winlines;                // task window's rows (volatile)
   FLG_t       procflags [PFLAGSSIZ];   // fieldscur subset, as enum
   int         maxpflgs,        // number of procflags (upcase fieldscur)
               maxcmdln;        // max length of a process' command line
   int         len_rownorm,     // lengths of the corresponding terminfo
               len_rowhigh;     // strings to avoid repeated strlen calls
   RCW_t       rc;              // stuff that gets saved in the rcfile
   char        capclr_sum [CLRBUFSIZ],  // terminfo strings built from
               capclr_msg [CLRBUFSIZ],  //    above clrs (& rebuilt too),
               capclr_pmt [CLRBUFSIZ],  //    but NO recurring costs !!!
               capclr_hdr [CLRBUFSIZ],     // note: sum, msg and pmt strs
               capclr_rowhigh [CLRBUFSIZ], //    are only used when this
               capclr_rownorm [CLRBUFSIZ]; //    window is the 'Curwin'!
   char        cap_bold [CAPBUFSIZ];    // support for View_NOBOLD toggle
   char        grpname   [GRPNAMSIZ],   // window number:name, printable
               columnhdr [SCREENMAX],   // column headings for procflags
               colusrnam [USRNAMSIZ];   // if selected by the 'u' command
} WIN_t;


/*######  Display Support *Data*  ########################################*/

// Configuration files support
#define SYS_RCFILESPEC  "/etc/toprc"
#define RCF_EYECATCHER  "RCfile for "
#define RCF_DEPRECATED  "Id:a, "

// The default fields displayed and their order,
#define DEF_FIELDS  "AEHIOQTWKNMbcdfgjplrsuvyzX"
// Pre-configured field groupss
#define JOB_FIELDS  "ABcefgjlrstuvyzMKNHIWOPQDX"
#define MEM_FIELDS  "ANOPQRSTUVbcdefgjlmyzWHIKX"
#define USR_FIELDS  "ABDECGfhijlopqrstuvyzMKNWX"
// Used by fields_sort, placed here for peace-of-mind
#define NUL_FIELDS  "abcdefghijklmnopqrstuvwxyz"


// The default values for the local config file
#define DEF_RCFILE { \
   0, 1, DEF_DELAY, 0, { \
   { P_CPU, DEF_WINFLGS, 0, \
      COLOR_RED, COLOR_RED, COLOR_YELLOW, COLOR_RED, \
      "Def", DEF_FIELDS }, \
   { P_PID, DEF_WINFLGS, 0, \
      COLOR_CYAN, COLOR_CYAN, COLOR_WHITE, COLOR_CYAN, \
      "Job", JOB_FIELDS }, \
   { P_MEM, DEF_WINFLGS, 0, \
      COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLUE, COLOR_MAGENTA, \
      "Mem", MEM_FIELDS }, \
   { P_URE, DEF_WINFLGS, 0, \
      COLOR_YELLOW, COLOR_YELLOW, COLOR_GREEN, COLOR_YELLOW, \
      "Usr", USR_FIELDS } \
   } }


// Summary Lines specially formatted string(s) --
// see 'show_special' for syntax details + other cautions.
#define LOADAV_line  "%s -%s\n"
#define LOADAV_line_alt  "%s\06 -%s\n"
#define STATES_line1  "Tasks:\03" \
   " %3u \02total,\03 %3u \02running,\03 %3u \02sleeping,\03 %3u \02stopped,\03 %3u \02zombie\03\n"
#define STATES_line2x4  "%s\03" \
   " %#5.1f%% \02user,\03 %#5.1f%% \02system,\03 %#5.1f%% \02nice,\03 %#5.1f%% \02idle\03\n"
#define STATES_line2x5  "%s\03" \
   " %#5.1f%% \02user,\03 %#5.1f%% \02system,\03 %#5.1f%% \02nice,\03 %#5.1f%% \02idle,\03 %#5.1f%% \02IO-wait\03\n"
#define STATES_line2x6  "%s\03" \
   " %#4.1f%% \02us,\03 %#4.1f%% \02sy,\03 %#4.1f%% \02ni,\03 %#4.1f%% \02id,\03 %#4.1f%% \02wa,\03 %#4.1f%% \02hi,\03 %#4.1f%% \02si\03\n"
#define STATES_line2x7  "%s\03" \
   "%#5.1f%%\02us,\03%#5.1f%%\02sy,\03%#5.1f%%\02ni,\03%#5.1f%%\02id,\03%#5.1f%%\02wa,\03%#5.1f%%\02hi,\03%#5.1f%%\02si,\03%#5.1f%%\02st\03\n"
#ifdef CASEUP_SUMMK
#define MEMORY_line1  "Mem: \03" \
   " %8luK \02total,\03 %8luK \02used,\03 %8luK \02free,\03 %8luK \02buffers\03\n"
#define MEMORY_line2  "Swap:\03" \
   " %8luK \02total,\03 %8luK \02used,\03 %8luK \02free,\03 %8luK \02cached\03\n"
#else
#define MEMORY_line1  "Mem: \03" \
   " %8luk \02total,\03 %8luk \02used,\03 %8luk \02free,\03 %8luk \02buffers\03\n"
#define MEMORY_line2  "Swap:\03" \
   " %8luk \02total,\03 %8luk \02used,\03 %8luk \02free,\03 %8luk \02cached\03\n"
#endif

// Keyboard Help specially formatted string(s) --
// see 'show_special' for syntax details + other cautions.
#define KEYS_help \
   "Help for Interactive Commands\02 - %s\n" \
   "Window \01%s\06: \01Cumulative mode \03%s\02.  \01System\06: \01Delay \03%.1f secs\02; \01Secure mode \03%s\02.\n" \
   "\n" \
   "  Z\05,\01B\05       Global: '\01Z\02' change color mappings; '\01B\02' disable/enable bold\n" \
   "  l,t,m     Toggle Summaries: '\01l\02' load avg; '\01t\02' task/cpu stats; '\01m\02' mem info\n" \
   "  1,I       Toggle SMP view: '\0011\02' single/separate states; '\01I\02' Irix/Solaris mode\n" \
   "\n" \
   "  f,o     . Fields/Columns: '\01f\02' add or remove; '\01o\02' change display order\n" \
   "  F or O  . Select sort field\n" \
   "  <,>     . Move sort field: '\01<\02' next col left; '\01>\02' next col right\n" \
   "  R,H     . Toggle: '\01R\02' normal/reverse sort; '\01H\02' show threads\n" \
   "  c,i,S   . Toggle: '\01c\02' cmd name/line; '\01i\02' idle tasks; '\01S\02' cumulative time\n" \
   "  x\05,\01y\05     . Toggle highlights: '\01x\02' sort field; '\01y\02' running tasks\n" \
   "  z\05,\01b\05     . Toggle: '\01z\02' color/mono; '\01b\02' bold/reverse (only if 'x' or 'y')\n" \
   "  u       . Show specific user only\n" \
   "  n or #  . Set maximum tasks displayed\n" \
   "\n" \
   "%s" \
   "  W         Write configuration file\n" \
   "  q         Quit\n" \
   "          ( commands shown with '.' require a \01visible\02 task display \01window\02 ) \n" \
   "Press '\01h\02' or '\01?\02' for help with \01Windows\02,\n" \
   "any other key to continue " \
   ""

// This guy goes into the help text (maybe)
#define KEYS_help_unsecured \
   "  k,r       Manipulate tasks: '\01k\02' kill; '\01r\02' renice\n" \
   "  d or s    Set update interval\n" \
   ""

// Fields Reorder/Toggle specially formatted string(s) --
// see 'show_special' for syntax details + other cautions
// note: the leading newline below serves really dumb terminals;
//       if there's no 'cursor_home', the screen will be a mess
//       but this part will still be functional.
#define FIELDS_current \
   "\n%sCurrent Fields\02: \01 %s \04 for window \01%s\06\n%s " \
   ""

// Some extra explanatory text which accompanies the Fields display.
// note: the newlines cannot actually be used, they just serve as
// substring delimiters for the 'display_fields' routine.
#define FIELDS_xtra \
   "Flags field:\n" \
   "  0x00000001  PF_ALIGNWARN\n" \
   "  0x00000002  PF_STARTING\n" \
   "  0x00000004  PF_EXITING\n" \
   "  0x00000040  PF_FORKNOEXEC\n" \
   "  0x00000100  PF_SUPERPRIV\n" \
   "  0x00000200  PF_DUMPCORE\n" \
   "  0x00000400  PF_SIGNALED\n" \
   "  0x00000800  PF_MEMALLOC\n" \
   "  0x00002000  PF_FREE_PAGES (2.5)\n" \
   "  0x00008000  debug flag (2.5)\n" \
   "  0x00024000  special threads (2.5)\n" \
   "  0x001D0000  special states (2.5)\n" \
   "  0x00100000  PF_USEDFPU (thru 2.4)\n" \
   ""
/* no room, sacrificed this one:  'Killed for out-of-memory' */
/* "  0x00001000  PF_MEMDIE (2.5)\n" ....................... */

// Sort Select specially formatted string(s) --
// see 'show_special' for syntax details + other cautions
// note: the leading newline below serves really dumb terminals;
//       if there's no 'cursor_home', the screen will be a mess
//       but this part will still be functional.
#define SORT_fields \
   "\n%sCurrent Sort Field\02: \01 %c \04 for window \01%s\06\n%s " \
   ""

// Some extra explanatory text which accompanies the Sort display.
// note: the newlines cannot actually be used, they just serve as
//       substring delimiters for the 'display_fields' routine.
#define SORT_xtra \
   "Note1:\n" \
   "  If a selected sort field can't be\n" \
   "  shown due to screen width or your\n" \
   "  field order, the '<' and '>' keys\n" \
   "  will be unavailable until a field\n" \
   "  within viewable range is chosen.\n" \
   "\n" \
   "Note2:\n" \
   "  Field sorting uses internal values,\n" \
   "  not those in column display.  Thus,\n" \
   "  the TTY & WCHAN fields will violate\n" \
   "  strict ASCII collating sequence.\n" \
   "  (shame on you if WCHAN is chosen)\n" \
   ""

// Colors Help specially formatted string(s) --
// see 'show_special' for syntax details + other cautions.
#define COLOR_help \
   "Help for color mapping\02 - %s\n" \
   "current window: \01%s\06\n" \
   "\n" \
   "   color - 04:25:44 up 8 days, 50 min,  7 users,  load average:\n" \
   "   Tasks:\03  64 \02total,\03   2 \03running,\03  62 \02sleeping,\03   0 \02stopped,\03\n" \
   "   Cpu(s):\03  76.5%% \02user,\03  11.2%% \02system,\03   0.0%% \02nice,\03  12.3%% \02idle\03\n" \
   "   \01 Nasty Message! \04  -or-  \01Input Prompt\05\n" \
   "   \01  PID TTY     PR  NI %%CPU    TIME+   VIRT SWAP STA Command  \06\n" \
   "   17284 \10pts/2  \07  8   0  0.0   0:00.75  1380    0 S   /bin/bash \10\n" \
   "   \01 8601 pts/1    7 -10  0.4   0:00.03   916    0 R < color -b \07\n" \
   "   11005 \10?      \07  9   0  0.0   0:02.50  2852 1008 S   amor -ses \10\n" \
   "   available toggles: \01B\02 =disable bold globally (\01%s\02),\n" \
   "       \01z\02 =color/mono (\01%s\02), \01b\02 =tasks \"bold\"/reverse (\01%s\02)\n" \
   "\n" \
   "Select \01target\02 as upper case letter:\n" \
   "   S\02 = Summary Data,\01  M\02 = Messages/Prompts,\n" \
   "   H\02 = Column Heads,\01  T\02 = Task Information\n" \
   "Select \01color\02 as number:\n" \
   "   0\02 = black,\01  1\02 = red,    \01  2\02 = green,\01  3\02 = yellow,\n" \
   "   4\02 = blue, \01  5\02 = magenta,\01  6\02 = cyan, \01  7\02 = white\n" \
   "\n" \
   "Selected: \01target\02 \01 %c \04; \01color\02 \01 %d \04\n" \
   "   press 'q' to abort changes to window '\01%s\02'\n" \
   "   press 'a' or 'w' to commit & change another, <Enter> to commit and end " \
   ""

// Windows/Field Group Help specially formatted string(s) --
// see 'show_special' for syntax details + other cautions.
#define WINDOWS_help \
   "Help for Windows / Field Groups\02 - \"Current Window\" = \01 %s \06\n" \
   "\n" \
   ". Use multiple \01windows\02, each with separate config opts (color,fields,sort,etc)\n" \
   ". The 'current' window controls the \01Summary Area\02 and responds to your \01Commands\02\n" \
   "  . that window's \01task display\02 can be turned \01Off\02 & \01On\02, growing/shrinking others\n" \
   "  . with \01NO\02 task display, some commands will be \01disabled\02 ('i','R','n','c', etc)\n" \
   "    until a \01different window\02 has been activated, making it the 'current' window\n" \
   ". You \01change\02 the 'current' window by: \01 1\02) cycling forward/backward;\01 2\02) choosing\n" \
   "  a specific field group; or\01 3\02) exiting the color mapping screen\n" \
   ". Commands \01available anytime   -------------\02\n" \
   "    A       . Alternate display mode toggle, show \01Single\02 / \01Multiple\02 windows\n" \
   "    G       . Choose another field group and make it 'current', or change now\n" \
   "              by selecting a number from: \01 1\02 =%s;\01 2\02 =%s;\01 3\02 =%s; or\01 4\02 =%s\n" \
   ". Commands \01requiring\02 '\01A\02' mode\01  -------------\02\n" \
   "    g       . Change the \01Name\05 of the 'current' window/field group\n" \
   " \01*\04  a , w   . Cycle through all four windows:  '\01a\05' Forward; '\01w\05' Backward\n" \
   " \01*\04  - , _   . Show/Hide:  '\01-\05' \01Current\02 window; '\01_\05' all \01Visible\02/\01Invisible\02\n" \
   "  The screen will be divided evenly between task displays.  But you can make\n" \
   "  some \01larger\02 or \01smaller\02, using '\01n\02' and '\01i\02' commands.  Then later you could:\n" \
   " \01*\04  = , +   . Rebalance tasks:  '\01=\05' \01Current\02 window; '\01+\05' \01Every\02 window\n" \
   "              (this also forces the \01current\02 or \01every\02 window to become visible)\n" \
   "\n" \
   "In '\01A\02' mode, '\01*\04' keys are your \01essential\02 commands.  Please try the '\01a\02' and '\01w\02'\n" \
   "commands plus the 'G' sub-commands NOW.  Press <Enter> to make 'Current' " \
   ""


/*######  For Piece of mind  #############################################*/

        /* just sanity check(s)... */
#if USRNAMSIZ < GETBUFSIZ
# error "Jeeze, USRNAMSIZ Must NOT be less than GETBUFSIZ !"
#endif



#endif /* _Itop */
