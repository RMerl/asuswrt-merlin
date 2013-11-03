/* top.c - Source file:         show Linux processes */
/*
 * Copyright (c) 2002, by:      James C. Warner
 *    All rights reserved.      8921 Hilloway Road
 *                              Eden Prairie, Minnesota 55347 USA
 *                             <warnerjc@worldnet.att.net>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * For their contributions to this program, the author wishes to thank:
 *    Albert D. Cahalan, <albert@users.sf.net>
 *    Craig Small, <csmall@small.dropbear.id.au>
 *
 * Changes by Albert Cahalan, 2002-2004.
 */
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Foul POS defines all sorts of stuff...
#include <term.h>
#undef tab

#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <values.h>

#include "proc/devname.h"
#include "proc/wchan.h"
#include "proc/procps.h"
#include "proc/readproc.h"
#include "proc/escape.h"
#include "proc/sig.h"
#include "proc/sysinfo.h"
#include "proc/version.h"
#include "proc/whattime.h"

#include "top.h"

/*######  Miscellaneous global stuff  ####################################*/

        /* The original and new terminal attributes */
static struct termios Savedtty,
                      Rawtty;
static int Ttychanged = 0;

        /* Program name used in error messages and local 'rc' file name */
static char *Myname;

        /* Name of user config file (dynamically constructed) and our
           'Current' rcfile contents, initialized with defaults but may be
           overridden with the local rcfile (old or new-style) values */
static char  Rc_name [OURPATHSZ];
static RCF_t Rc = DEF_RCFILE;

        /* The run-time acquired page size */
static unsigned Page_size;
static unsigned page_to_kb_shift;

        /* SMP Irix/Solaris mode */
static int  Cpu_tot;
static double pcpu_max_value;  // usually 99.9, for %CPU display
        /* assume no IO-wait stats, overridden if linux 2.5.41 */
static const char *States_fmts = STATES_line2x4;

        /* Specific process id monitoring support */
static pid_t Monpids [MONPIDMAX] = { 0 };
static int   Monpidsidx = 0;

        /* A postponed error message */
static char Msg_delayed [SMLBUFSIZ];
static int  Msg_awaiting = 0;

// This is the select() timeout. Clear it in sig handlers to avoid a race.
// (signal happens just as we are about to select() and thus does not
// break us out of the select(), causing us to delay until timeout)
static volatile struct timeval tv;
#define ZAP_TIMEOUT do{tv.tv_usec=0; tv.tv_sec=0;}while(0);

        /* Configurable Display support ##################################*/

        /* Current screen dimensions.
           note: the number of processes displayed is tracked on a per window
                 basis (see the WIN_t).  Max_lines is the total number of
                 screen rows after deducting summary information overhead. */
        /* Current terminal screen size. */
static int Screen_cols, Screen_rows, Max_lines;

// set to 1 if writing to the last column would be troublesome
// (we don't distinguish the lowermost row from the other rows)
static int avoid_last_column;

        /* This is really the number of lines needed to display the summary
           information (0 - nn), but is used as the relative row where we
           stick the cursor between frames. */
static int Msg_row;

        /* Global/Non-windows mode stuff that is NOT persistent */
static int No_ksyms = -1,       // set to '0' if ksym avail, '1' otherwise
           PSDBopen = 0,        // set to '1' if psdb opened (now postponed)
           Batch = 0,           // batch mode, collect no input, dumb output
           Loops = -1,          // number of iterations, -1 loops forever
           Secure_mode = 0;     // set if some functionality restricted

        /* Some cap's stuff to reduce runtime calls --
           to accomodate 'Batch' mode, they begin life as empty strings */
static char  Cap_clr_eol    [CAPBUFSIZ],
             Cap_clr_eos    [CAPBUFSIZ],
             Cap_clr_scr    [CAPBUFSIZ],
             Cap_rmam       [CAPBUFSIZ],
             Cap_smam       [CAPBUFSIZ],
             Cap_curs_norm  [CAPBUFSIZ],
             Cap_curs_huge  [CAPBUFSIZ],
             Cap_home       [CAPBUFSIZ],
             Cap_norm       [CAPBUFSIZ],
             Cap_reverse    [CAPBUFSIZ],
             Caps_off       [CAPBUFSIZ];
static int   Cap_can_goto = 0;

        /* Some optimization stuff, to reduce output demands...
           The Pseudo_ guys are managed by wins_resize and frame_make.  They
           are exploited in a macro and represent 90% of our optimization.
           The Stdout_buf is transparent to our code and regardless of whose
           buffer is used, stdout is flushed at frame end or if interactive. */
static char *Pseudo_scrn;
static int   Pseudo_row, Pseudo_cols, Pseudo_size;
#ifndef STDOUT_IOLBF
        // less than stdout's normal buffer but with luck mostly '\n' anyway
static char  Stdout_buf[2048];
#endif


        /* ////////////////////////////////////////////////////////////// */
        /* Special Section: multiple windows/field groups  ---------------*/

        /* The pointers to our four WIN_t's, and which of those is considered
           the 'current' window (ie. which window is associated with any summ
           info displayed and to which window commands are directed) */
static WIN_t Winstk [GROUPSMAX],
             *Curwin;

        /* Frame oriented stuff that can't remain local to any 1 function
           and/or that would be too cumbersome managed as parms,
           and/or that are simply more efficiently handled as globals
           (first 2 persist beyond a single frame, changed infrequently) */
static int       Frames_libflags;       // PROC_FILLxxx flags (0 = need new)
//atic int       Frames_maxcmdln;       // the largest from the 4 windows
static unsigned  Frame_maxtask;         // last known number of active tasks
                                        // ie. current 'size' of proc table
static unsigned  Frame_running,         // state categories for this frame
                 Frame_sleepin,
                 Frame_stopped,
                 Frame_zombied;
static float     Frame_tscale;          // so we can '*' vs. '/' WHEN 'pcpu'
static int       Frame_srtflg,          // the subject window's sort direction
                 Frame_ctimes,          // the subject window's ctimes flag
                 Frame_cmdlin;          // the subject window's cmdlin flag
        /* ////////////////////////////////////////////////////////////// */


/*######  Sort callbacks  ################################################*/

        /*
         * These happen to be coded in the same order as the enum 'pflag'
         * values.  Note that 2 of these routines serve double duty --
         * 2 columns each.
         */

SCB_NUMx(P_PID, XXXID)
SCB_NUMx(P_PPD, ppid)
SCB_STRx(P_URR, ruser)
SCB_NUMx(P_UID, euid)
SCB_STRx(P_URE, euser)
SCB_STRx(P_GRP, egroup)
SCB_NUMx(P_TTY, tty)
SCB_NUMx(P_PRI, priority)
SCB_NUMx(P_NCE, nice)
SCB_NUMx(P_CPN, processor)
SCB_NUM1(P_CPU, pcpu)
                                        // also serves P_TM2 !
static int sort_P_TME (const proc_t **P, const proc_t **Q)
{
   if (Frame_ctimes) {
      if ( ((*P)->cutime + (*P)->cstime + (*P)->utime + (*P)->stime)
        < ((*Q)->cutime + (*Q)->cstime + (*Q)->utime + (*Q)->stime) )
           return SORT_lt;
      if ( ((*P)->cutime + (*P)->cstime + (*P)->utime + (*P)->stime)
        > ((*Q)->cutime + (*Q)->cstime + (*Q)->utime + (*Q)->stime) )
           return SORT_gt;
   } else {
      if ( ((*P)->utime + (*P)->stime) < ((*Q)->utime + (*Q)->stime))
         return SORT_lt;
      if ( ((*P)->utime + (*P)->stime) > ((*Q)->utime + (*Q)->stime))
         return SORT_gt;
   }
   return SORT_eq;
}

SCB_NUM1(P_VRT, size)
SCB_NUM2(P_SWP, size, resident)
SCB_NUM1(P_RES, resident)               // also serves P_MEM !
SCB_NUM1(P_COD, trs)
SCB_NUM1(P_DAT, drs)
SCB_NUM1(P_SHR, share)
SCB_NUM1(P_FLT, maj_flt)
SCB_NUM1(P_DRT, dt)
SCB_NUMx(P_STA, state)

static int sort_P_CMD (const proc_t **P, const proc_t **Q)
{
   /* if a process doesn't have a cmdline, we'll consider it a kernel thread
      -- since displayed tasks are given special treatment, we must too */
   if (Frame_cmdlin && ((*P)->cmdline || (*Q)->cmdline)) {
      if (!(*Q)->cmdline) return Frame_srtflg * -1;
      if (!(*P)->cmdline) return Frame_srtflg;
      return Frame_srtflg *
         strncmp((*Q)->cmdline[0], (*P)->cmdline[0], (unsigned)Curwin->maxcmdln);
   }
   // this part also handles the compare if both are kernel threads
   return Frame_srtflg * strcmp((*Q)->cmd, (*P)->cmd);
}

SCB_NUM1(P_WCH, wchan)
SCB_NUM1(P_FLG, flags)

        /* ///////////////////////////////// special sort for prochlp() ! */
static int sort_HST_t (const HST_t *P, const HST_t *Q)
{
   return P->pid - Q->pid;
}


/*######  Tiny useful routine(s)  ########################################*/

        /*
         * This routine isolates ALL user INPUT and ensures that we
         * wont be mixing I/O from stdio and low-level read() requests */
static int chin (int ech, char *buf, unsigned cnt)
{
   int rc;

   fflush(stdout);
   if (!ech)
      rc = read(STDIN_FILENO, buf, cnt);
   else {
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &Savedtty);
      rc = read(STDIN_FILENO, buf, cnt);
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &Rawtty);
   }
   // may be the beginning of a lengthy escape sequence
   tcflush(STDIN_FILENO, TCIFLUSH);
   return rc;                   // note: we do NOT produce a vaid 'string'
}


// This routine simply formats whatever the caller wants and
// returns a pointer to the resulting 'const char' string...
static const char *fmtmk (const char *fmts, ...) __attribute__((format(printf,1,2)));
static const char *fmtmk (const char *fmts, ...)
{
   static char buf[BIGBUFSIZ];          // with help stuff, our buffer
   va_list va;                          // requirements exceed 1k

   va_start(va, fmts);
   vsnprintf(buf, sizeof(buf), fmts, va);
   va_end(va);
   return (const char *)buf;
}


// This guy is just our way of avoiding the overhead of the standard
// strcat function (should the caller choose to participate)
static inline char *scat (char *restrict dst, const char *restrict src)
{
   while (*dst) dst++;
   while ((*(dst++) = *(src++)));
   return --dst;
}


// Trim the rc file lines and any 'open_psdb_message' result which arrives
// with an inappropriate newline (thanks to 'sysmap_mmap')
static char *strim_0 (char *str)
{
   static const char ws[] = "\b\e\f\n\r\t\v\x9b";  // 0x9b is an escape
   char *p;

   if ((p = strpbrk(str, ws))) *p = 0;
   return str;
}


// This guy just facilitates Batch and protects against dumb ttys
// -- we'd 'inline' him but he's only called twice per frame,
// yet used in many other locations.
static const char *tg2 (int x, int y)
{
   return Cap_can_goto ? tgoto(cursor_address, x, y) : "";
}


/*######  Exit/Interrput routines  #######################################*/

// The usual program end -- called only by functions in this section.
static void bye_bye (FILE *fp, int eno, const char *str) NORETURN;
static void bye_bye (FILE *fp, int eno, const char *str)
{
   if (!Batch)
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &Savedtty);
   putp(tg2(0, Screen_rows));
   putp(Cap_curs_norm);
   putp(Cap_smam);
   putp("\n");
   fflush(stdout);

//#define ATEOJ_REPORT
#ifdef ATEOJ_REPORT

   fprintf(fp,
      "\n\tTerminal: %s"
      "\n\t   device = %s, ncurses = v%s"
      "\n\t   max_colors = %d, max_pairs = %d"
      "\n\t   Cap_can_goto = %s"
      "\n\t   Screen_cols = %d, Screen_rows = %d"
      "\n\t   Max_lines = %d, most recent Pseudo_size = %d"
      "\n"
#ifdef PRETENDNOCAP
      , "dumb"
#else
      , termname()
#endif
      , ttyname(STDOUT_FILENO), NCURSES_VERSION
      , max_colors, max_pairs
      , Cap_can_goto ? "yes" : "No!"
      , Screen_cols, Screen_rows
      , Max_lines, Pseudo_size
      );

   fprintf(fp,
#ifndef STDOUT_IOLBF
      "\n\t   Stdout_buf = %d, BUFSIZ = %u"
#endif
      "\n\tWindows and Curwin->"
      "\n\t   sizeof(WIN_t) = %u, GROUPSMAX = %d"
      "\n\t   rc.winname = %s, grpname = %s"
      "\n\t   rc.winflags = %08x, maxpflgs = %d"
      "\n\t   rc.fieldscur = %s"
      "\n\t   winlines  = %d, rc.maxtasks = %d, maxcmdln = %d"
      "\n\t   rc.sortindx  = %d"
      "\n"
#ifndef STDOUT_IOLBF
      , sizeof(Stdout_buf), (unsigned)BUFSIZ
#endif
      , sizeof(WIN_t), GROUPSMAX
      , Curwin->rc.winname, Curwin->grpname
      , Curwin->rc.winflags, Curwin->maxpflgs
      , Curwin->rc.fieldscur
      , Curwin->winlines, Curwin->rc.maxtasks, Curwin->maxcmdln
      , Curwin->rc.sortindx
      );

   fprintf(fp,
      "\n\tProgram"
      "\n\t   Linux version = %u.%u.%u, %s"
      "\n\t   Hertz = %u (%u bytes, %u-bit time)"
      "\n\t   Page_size = %d, Cpu_tot = %d, sizeof(proc_t) = %u"
      "\n\t   sizeof(CPU_t) = %u, sizeof(HST_t) = %u (%u HST_t's/Page)"
      "\n"
      , LINUX_VERSION_MAJOR(linux_version_code)
      , LINUX_VERSION_MINOR(linux_version_code)
      , LINUX_VERSION_PATCH(linux_version_code)
      , procps_version
      , (unsigned)Hertz, sizeof(Hertz), sizeof(Hertz) * 8
      , Page_size, Cpu_tot, sizeof(proc_t)
      , sizeof(CPU_t), sizeof(HST_t), Page_size / sizeof(HST_t)
      );


#endif

   if (str) fputs(str, fp);
   exit(eno);
}


        /*
         * Normal end of execution.
         * catches:
         *    SIGALRM, SIGHUP, SIGINT, SIGPIPE, SIGQUIT and SIGTERM */
// FIXME: can't do this shit in a signal handler
static void end_pgm (int sig) NORETURN;
static void end_pgm (int sig)
{
   if(sig)
      sig |= 0x80; // for a proper process exit code
   bye_bye(stdout, sig, NULL);
}


        /*
         * Standard error handler to normalize the look of all err o/p */
static void std_err (const char *str) NORETURN;
static void std_err (const char *str)
{
   static char buf[SMLBUFSIZ];

   fflush(stdout);
   /* we'll use our own buffer so callers can still use fmtmk() and, yes the
      leading tab is not the standard convention, but the standard is wrong
      -- OUR msg won't get lost in screen clutter, like so many others! */
   snprintf(buf, sizeof(buf), "\t%s: %s\n", Myname, str);
   if (!Ttychanged) {
      fprintf(stderr, "%s\n", buf);
      exit(1);
   }
      /* not to worry, he'll change our exit code to 1 due to 'buf' */
   bye_bye(stderr, 1, buf);
}


        /*
         * Standard out handler */
static void std_out (const char *str) NORETURN;
static void std_out (const char *str)
{
   static char buf[SMLBUFSIZ];

   fflush(stdout);
   /* we'll use our own buffer so callers can still use fmtmk() and, yes the
      leading tab is not the standard convention, but the standard is wrong
      -- OUR msg won't get lost in screen clutter, like so many others! */
   snprintf(buf, sizeof(buf), "\t%s: %s\n", Myname, str);
   if (!Ttychanged) {
      fprintf(stdout, "%s\n", buf);
      exit(0);
   }
   bye_bye(stdout, 0, buf);
}


        /*
         * Suspend ourself.
         * catches:
         *    SIGTSTP, SIGTTIN and SIGTTOU */
// FIXME: can't do this shit in a signal handler!
static void suspend (int dont_care_sig)
{
  (void)dont_care_sig;
      /* reset terminal */
   tcsetattr(STDIN_FILENO, TCSAFLUSH, &Savedtty);
   putp(tg2(0, Screen_rows));
   putp(Cap_curs_norm);
   putp(Cap_smam);
   putp("\n");
   fflush(stdout);
   raise(SIGSTOP);
      /* later, after SIGCONT... */
   ZAP_TIMEOUT
   if (!Batch)
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &Rawtty);
   putp(Cap_clr_scr);
   putp(Cap_rmam);
}


/*######  Misc Color/Display support  ####################################*/

   /* macro to test if a basic (non-color) capability is valid
         thanks: Floyd Davidson <floyd@ptialaska.net> */
#define tIF(s)  s ? s : ""
#define CAPCOPY(dst,src) src && strcpy(dst,src)

        /*
         * Make the appropriate caps/color strings and set some
         * lengths which are used to distinguish twix the displayed
         * columns and an actual printed row!
         * note: we avoid the use of background color so as to maximize
         *       compatibility with the user's xterm settings */
static void capsmk (WIN_t *q)
{
   static int capsdone = 0;

   // we must NOT disturb our 'empty' terminfo strings!
   if (Batch) return;

   // these are the unchangeable puppies, so we only do 'em once
   if (!capsdone) {
      CAPCOPY(Cap_clr_eol, clr_eol);
      CAPCOPY(Cap_clr_eos, clr_eos);
      CAPCOPY(Cap_clr_scr, clear_screen);

      if (!eat_newline_glitch) {  // we like the eat_newline_glitch
         CAPCOPY(Cap_rmam, exit_am_mode);
         CAPCOPY(Cap_smam, enter_am_mode);
         if (!*Cap_rmam || !*Cap_smam) {  // need both
            *Cap_rmam = '\0';
            *Cap_smam = '\0';
            if (auto_right_margin) {
               avoid_last_column = 1;
            }
         }
      }

      CAPCOPY(Cap_curs_huge, cursor_visible);
      CAPCOPY(Cap_curs_norm, cursor_normal);
      CAPCOPY(Cap_home, cursor_home);
      CAPCOPY(Cap_norm, exit_attribute_mode);
      CAPCOPY(Cap_reverse, enter_reverse_mode);

      snprintf(Caps_off, sizeof(Caps_off), "%s%s", Cap_norm, tIF(orig_pair));
      if (tgoto(cursor_address, 1, 1)) Cap_can_goto = 1;
      capsdone = 1;
   }
   /* the key to NO run-time costs for configurable colors -- we spend a
      little time with the user now setting up our terminfo strings, and
      the job's done until he/she/it has a change-of-heart */
   strcpy(q->cap_bold, CHKw(q, View_NOBOLD) ? Cap_norm : tIF(enter_bold_mode));
   if (CHKw(q, Show_COLORS) && max_colors > 0) {
      strcpy(q->capclr_sum, tparm(set_a_foreground, q->rc.summclr));
      snprintf(q->capclr_msg, sizeof(q->capclr_msg), "%s%s"
         , tparm(set_a_foreground, q->rc.msgsclr), Cap_reverse);
      snprintf(q->capclr_pmt, sizeof(q->capclr_pmt), "%s%s"
         , tparm(set_a_foreground, q->rc.msgsclr), q->cap_bold);
      snprintf(q->capclr_hdr, sizeof(q->capclr_hdr), "%s%s"
         , tparm(set_a_foreground, q->rc.headclr), Cap_reverse);
      snprintf(q->capclr_rownorm, sizeof(q->capclr_rownorm), "%s%s"
         , Caps_off, tparm(set_a_foreground, q->rc.taskclr));
   } else {
      q->capclr_sum[0] = '\0';
      strcpy(q->capclr_msg, Cap_reverse);
      strcpy(q->capclr_pmt, q->cap_bold);
      strcpy(q->capclr_hdr, Cap_reverse);
      strcpy(q->capclr_rownorm, Cap_norm);
   }
   // composite(s), so we do 'em outside and after the if
   snprintf(q->capclr_rowhigh, sizeof(q->capclr_rowhigh), "%s%s"
      , q->capclr_rownorm, CHKw(q, Show_HIBOLD) ? q->cap_bold : Cap_reverse);
   q->len_rownorm = strlen(q->capclr_rownorm);
   q->len_rowhigh = strlen(q->capclr_rowhigh);

#undef tIF
}


// Show an error, but not right now.
// Due to the postponed opening of ksym, using open_psdb_message,
// if P_WCH had been selected and the program is restarted, the
// message would otherwise be displayed prematurely.
static void msg_save (const char *fmts, ...) __attribute__((format(printf,1,2)));
static void msg_save (const char *fmts, ...)
{
   char tmp[SMLBUFSIZ];
   va_list va;

   va_start(va, fmts);
   vsnprintf(tmp, sizeof(tmp), fmts, va);
   va_end(va);
      /* we'll add some extra attention grabbers to whatever this is */
   snprintf(Msg_delayed, sizeof(Msg_delayed), "\a***  %s  ***", strim_0(tmp));
   Msg_awaiting = 1;
}


        /*
         * Show an error message (caller may include a '\a' for sound) */
static void show_msg (const char *str)
{
   PUTT("%s%s %s %s%s",
      tg2(0, Msg_row),
      Curwin->capclr_msg,
      str,
      Caps_off,
      Cap_clr_eol
   );
   fflush(stdout);
   sleep(MSG_SLEEP);
   Msg_awaiting = 0;
}


        /*
         * Show an input prompt + larger cursor */
static void show_pmt (const char *str)
{
   PUTT("%s%s%s: %s%s",
      tg2(0, Msg_row),
      Curwin->capclr_pmt,
      str,
      Cap_curs_huge,
      Caps_off
   );
   fflush(stdout);
}


        /*
         * Show lines with specially formatted elements, but only output
         * what will fit within the current screen width.
         *    Our special formatting consists of:
         *       "some text <_delimiter_> some more text <_delimiter_>...\n"
         *    Where <_delimiter_> is a single byte in the range of:
         *       \01 through \10  (in decimalizee, 1 - 8)
         *    and is used to select an 'attribute' from a capabilities table
         *    which is then applied to the *preceding* substring.
         * Once recognized, the delimiter is replaced with a null character
         * and viola, we've got a substring ready to output!  Strings or
         * substrings without delimiters will receive the Cap_norm attribute.
         *
         * Caution:
         *    This routine treats all non-delimiter bytes as displayable
         *    data subject to our screen width marching orders.  If callers
         *    embed non-display data like tabs or terminfo strings in our
         *    glob, a line will truncate incorrectly at best.  Worse case
         *    would be truncation of an embedded tty escape sequence.
         *
         *    Tabs must always be avoided or our efforts are wasted and
         *    lines will wrap.  To lessen but not eliminate the risk of
         *    terminfo string truncation, such non-display stuff should
         *    be placed at the beginning of a "short" line.
         *    (and as for tabs, gimme 1 more color then no worries, mate) */
static void show_special (int interact, const char *glob)
{ /* note: the following is for documentation only,
           the real captab is now found in a group's WIN_t !
     +------------------------------------------------------+
     | char *captab[] = {                 :   Cap's/Delim's |
     |   Cap_norm, Cap_norm, Cap_bold,    =   \00, \01, \02 |
     |   Sum_color,                       =   \03           |
     |   Msg_color, Pmt_color,            =   \04, \05      |
     |   Hdr_color,                       =   \06           |
     |   Row_color_high,                  =   \07           |
     |   Row_color_norm  };               =   \10 [octal!]  |
     +------------------------------------------------------+ */
   char lin[BIGBUFSIZ], row[ROWBUFSIZ], tmp[ROWBUFSIZ];
   char *rp, *cap, *lin_end, *sub_beg, *sub_end;
   int room;

      /* handle multiple lines passed in a bunch */
   while ((lin_end = strchr(glob, '\n'))) {

         /* create a local copy we can extend and otherwise abuse */
      size_t amt = lin_end - glob;
      if(amt > sizeof lin - 1)
         amt = sizeof lin - 1;  // shit happens
      memcpy(lin, glob, amt);
         /* zero terminate this part and prepare to parse substrings */
      lin[amt] = '\0';
      room = Screen_cols;
      sub_beg = sub_end = lin;
      *(rp = row) = '\0';

      while (*sub_beg) {
         switch (*sub_end) {
            case 0:                     /* no end delim, captab makes normal */
               *(sub_end + 1) = '\0';   /* extend str end, then fall through */
            case 1 ... 8:
               cap = Curwin->captab[(int)*sub_end];
               *sub_end = '\0';
               snprintf(tmp, sizeof(tmp), "%s%.*s%s", cap, room, sub_beg, Caps_off);
               amt = strlen(tmp);
               if(rp - row + amt + 1 > sizeof row)
                  goto overflow;  // shit happens
               rp = scat(rp, tmp);
               room -= (sub_end - sub_beg);
               sub_beg = ++sub_end;
               break;
            default:                    /* nothin' special, just text */
               ++sub_end;
         }
         if (unlikely(0 >= room)) break; /* skip substrings that won't fit */
      }
overflow:
      if (interact) PUTT("%s%s\n", row, Cap_clr_eol);
      else PUFF("%s%s\n", row, Cap_clr_eol);
      glob = ++lin_end;                 /* point to next line (maybe) */
   } /* end: while 'lines' */

   /* If there's anything left in the glob (by virtue of no trailing '\n'),
      it probably means caller wants to retain cursor position on this final
      line.  That, in turn, means we're interactive and so we'll just do our
      'fit-to-screen' thingy... */
   if (*glob) PUTT("%.*s", Screen_cols, glob);
}


/*######  Small Utility routines  ########################################*/

// Get a string from the user
static char *ask4str (const char *prompt)
{
   static char buf[GETBUFSIZ];

   show_pmt(prompt);
   memset(buf, '\0', sizeof(buf));
   chin(1, buf, sizeof(buf) - 1);
   putp(Cap_curs_norm);
   return strim_0(buf);
}


// Get a float from the user
static float get_float (const char *prompt)
{
   char *line;
   float f;

   if (!(*(line = ask4str(prompt)))) return -1;
   // note: we're not allowing negative floats
   if (strcspn(line, ",.1234567890")) {
      show_msg("\aNot valid");
      return -1;
   }
   sscanf(line, "%f", &f);
   return f;
}


// Get an integer from the user
static int get_int (const char *prompt)
{
   char *line;
   int n;

   if (!(*(line = ask4str(prompt)))) return -1;
   // note: we've got to allow negative ints (renice)
   if (strcspn(line, "-1234567890")) {
      show_msg("\aNot valid");
      return -1;
   }
   sscanf(line, "%d", &n);
   return n;
}


        /*
         * Do some scaling stuff.
         * We'll interpret 'num' as one of the following types and
         * try to format it to fit 'width'.
         *    SK_no (0) it's a byte count
         *    SK_Kb (1) it's kilobytes
         *    SK_Mb (2) it's megabytes
         *    SK_Gb (3) it's gigabytes
         *    SK_Tb (4) it's terabytes  */
static const char *scale_num (unsigned long num, const int width, const unsigned type)
{
      /* kilobytes, megabytes, gigabytes, terabytes, duh! */
   static double scale[] = { 1024.0, 1024.0*1024, 1024.0*1024*1024, 1024.0*1024*1024*1024, 0 };
      /* kilo, mega, giga, tera, none */
#ifdef CASEUP_SCALE
   static char nextup[] =  { 'K', 'M', 'G', 'T', 0 };
#else
   static char nextup[] =  { 'k', 'm', 'g', 't', 0 };
#endif
   static char buf[TNYBUFSIZ];
   double *dp;
   char *up;

      /* try an unscaled version first... */
   if (width >= snprintf(buf, sizeof(buf), "%lu", num)) return buf;

      /* now try successively higher types until it fits */
   for (up = nextup + type, dp = scale; *dp; ++dp, ++up) {
         /* the most accurate version */
      if (width >= snprintf(buf, sizeof(buf), "%.1f%c", num / *dp, *up))
         return buf;
         /* the integer version */
      if (width >= snprintf(buf, sizeof(buf), "%ld%c", (unsigned long)(num / *dp), *up))
         return buf;
   }
      /* well shoot, this outta' fit... */
   return "?";
}


        /*
         * Do some scaling stuff.
         * format 'tics' to fit 'width'. */
static const char *scale_tics (TIC_t tics, const int width)
{
#ifdef CASEUP_SCALE
#define HH "%uH"
#define DD "%uD"
#define WW "%uW"
#else
#define HH "%uh"
#define DD "%ud"
#define WW "%uw"
#endif
   static char buf[TNYBUFSIZ];
   unsigned long nt;    // narrow time, for speed on 32-bit
   unsigned cc;         // centiseconds
   unsigned nn;         // multi-purpose whatever

   nt  = (tics * 100ull) / Hertz;
   cc  = nt % 100;                              // centiseconds past second
   nt /= 100;                                   // total seconds
   nn  = nt % 60;                               // seconds past the minute
   nt /= 60;                                    // total minutes
   if (width >= snprintf(buf, sizeof(buf), "%lu:%02u.%02u", nt, nn, cc))
      return buf;
   if (width >= snprintf(buf, sizeof buf, "%lu:%02u", nt, nn))
      return buf;
   nn  = nt % 60;                               // minutes past the hour
   nt /= 60;                                    // total hours
   if (width >= snprintf(buf, sizeof buf, "%lu,%02u", nt, nn))
      return buf;
   nn = nt;                                     // now also hours
   if (width >= snprintf(buf, sizeof buf, HH, nn))
      return buf;
   nn /= 24;                                    // now days
   if (width >= snprintf(buf, sizeof buf, DD, nn))
      return buf;
   nn /= 7;                                     // now weeks
   if (width >= snprintf(buf, sizeof buf, WW, nn))
      return buf;
      // well shoot, this outta' fit...
   return "?";

#undef HH
#undef DD
#undef WW
}

#include <pwd.h>

static int selection_type;
static uid_t selection_uid;

// FIXME: this is "temporary" code we hope
static int good_uid(const proc_t *restrict const pp){
   switch(selection_type){
   case 'p':
      return 1;
   case 0:
      return 1;
   case 'U':
      if (pp->ruid == selection_uid) return 1;
      if (pp->suid == selection_uid) return 1;
      if (pp->fuid == selection_uid) return 1;
      // FALLTHROUGH
   case 'u':
      if (pp->euid == selection_uid) return 1;
      // FALLTHROUGH
   default:
      ;  // don't know what it is; find bugs fast
   }
   return 0;
}

// swiped from ps, and ought to be in libproc
static const char *parse_uid(const char *restrict const str, uid_t *restrict const ret){
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
   *ret = num;
   return 0;
}


/*######  Library Alternatives  ##########################################*/

        /*
         * Handle our own memory stuff without the risk of leaving the
         * user's terminal in an ugly state should things go sour. */

static void *alloc_c (unsigned numb) MALLOC;
static void *alloc_c (unsigned numb)
{
   void * p;

   if (!numb) ++numb;
   if (!(p = calloc(1, numb)))
      std_err("failed memory allocate");
   return p;
}

static void *alloc_r (void *q, unsigned numb) MALLOC;
static void *alloc_r (void *q, unsigned numb)
{
   void *p;

   if (!numb) ++numb;
   if (!(p = realloc(q, numb)))
      std_err("failed memory allocate");
   return p;
}


        /*
         * This guy's modeled on libproc's 'five_cpu_numbers' function except
         * we preserve all cpu data in our CPU_t array which is organized
         * as follows:
         *    cpus[0] thru cpus[n] == tics for each separate cpu
         *    cpus[Cpu_tot]        == tics from the 1st /proc/stat line */
static CPU_t *cpus_refresh (CPU_t *cpus)
{
   static FILE *fp = NULL;
   int i;
   int num;
   // enough for a /proc/stat CPU line (not the intr line)
   char buf[SMLBUFSIZ];

   /* by opening this file once, we'll avoid the hit on minor page faults
      (sorry Linux, but you'll have to close it for us) */
   if (!fp) {
      if (!(fp = fopen("/proc/stat", "r")))
         std_err(fmtmk("Failed /proc/stat open: %s", strerror(errno)));
      /* note: we allocate one more CPU_t than Cpu_tot so that the last slot
               can hold tics representing the /proc/stat cpu summary (the first
               line read) -- that slot supports our View_CPUSUM toggle */
      cpus = alloc_c((1 + Cpu_tot) * sizeof(CPU_t));
   }
   rewind(fp);
   fflush(fp);

   // first value the last slot with the cpu summary line
   if (!fgets(buf, sizeof(buf), fp)) std_err("failed /proc/stat read");
   cpus[Cpu_tot].x = 0;  // FIXME: can't tell by kernel version number
   cpus[Cpu_tot].y = 0;  // FIXME: can't tell by kernel version number
   cpus[Cpu_tot].z = 0;  // FIXME: can't tell by kernel version number
   num = sscanf(buf, "cpu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu",
      &cpus[Cpu_tot].u,
      &cpus[Cpu_tot].n,
      &cpus[Cpu_tot].s,
      &cpus[Cpu_tot].i,
      &cpus[Cpu_tot].w,
      &cpus[Cpu_tot].x,
      &cpus[Cpu_tot].y,
      &cpus[Cpu_tot].z
   );
   if (num < 4)
         std_err("failed /proc/stat read");

   // and just in case we're 2.2.xx compiled without SMP support...
   if (Cpu_tot == 1) {
      cpus[1].id = 0;
      memcpy(cpus, &cpus[1], sizeof(CPU_t));
   }

   // now value each separate cpu's tics
   for (i = 0; 1 < Cpu_tot && i < Cpu_tot; i++) {
      if (!fgets(buf, sizeof(buf), fp)) std_err("failed /proc/stat read");
      cpus[i].x = 0;  // FIXME: can't tell by kernel version number
      cpus[i].y = 0;  // FIXME: can't tell by kernel version number
      cpus[i].z = 0;  // FIXME: can't tell by kernel version number
      num = sscanf(buf, "cpu%u %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu",
         &cpus[i].id,
         &cpus[i].u, &cpus[i].n, &cpus[i].s, &cpus[i].i, &cpus[i].w, &cpus[i].x, &cpus[i].y, &cpus[i].z
      );
      if (num < 4)
            std_err("failed /proc/stat read");
   }
   return cpus;
}


        /*
         * Refresh procs *Helper* function to eliminate yet one more need
         * to loop through our darn proc_t table.  He's responsible for:
         *    1) calculating the elapsed time since the previous frame
         *    2) counting the number of tasks in each state (run, sleep, etc)
         *    3) maintaining the HST_t's and priming the proc_t pcpu field
         *    4) establishing the total number tasks for this frame */
static void prochlp (proc_t *this)
{
   static HST_t    *hist_sav = NULL;
   static HST_t    *hist_new = NULL;
   static unsigned  hist_siz = 0;       // number of structs
   static unsigned  maxt_sav;           // prior frame's max tasks
   TIC_t tics;

   if (unlikely(!this)) {
      static struct timeval oldtimev;
      struct timeval timev;
      struct timezone timez;
      HST_t *hist_tmp;
      float et;

      gettimeofday(&timev, &timez);
      et = (timev.tv_sec - oldtimev.tv_sec)
         + (float)(timev.tv_usec - oldtimev.tv_usec) / 1000000.0;
      oldtimev.tv_sec = timev.tv_sec;
      oldtimev.tv_usec = timev.tv_usec;

      // if in Solaris mode, adjust our scaling for all cpus
      Frame_tscale = 100.0f / ((float)Hertz * (float)et * (Rc.mode_irixps ? 1 : Cpu_tot));
      maxt_sav = Frame_maxtask;
      Frame_maxtask = Frame_running = Frame_sleepin = Frame_stopped = Frame_zombied = 0;

      // reuse memory each time around
      hist_tmp = hist_sav;
      hist_sav = hist_new;
      hist_new = hist_tmp;
      // prep for our binary search by sorting the last frame's HST_t's
      qsort(hist_sav, maxt_sav, sizeof(HST_t), (QFP_t)sort_HST_t);
      return;
   }

   switch (this->state) {
      case 'R':
         Frame_running++;
         break;
      case 'S':
      case 'D':
         Frame_sleepin++;
         break;
      case 'T':
         Frame_stopped++;
         break;
      case 'Z':
         Frame_zombied++;
         break;
   }

   if (unlikely(Frame_maxtask+1 >= hist_siz)) {
      hist_siz = hist_siz * 5 / 4 + 100;  // grow by at least 25%
      hist_sav = alloc_r(hist_sav, sizeof(HST_t) * hist_siz);
      hist_new = alloc_r(hist_new, sizeof(HST_t) * hist_siz);
   }
   /* calculate time in this process; the sum of user time (utime) and
      system time (stime) -- but PLEASE dont waste time and effort on
      calcs and saves that go unused, like the old top! */
   hist_new[Frame_maxtask].pid  = this->tid;
   hist_new[Frame_maxtask].tics = tics = (this->utime + this->stime);

#if 0
{  int i;
   int lo = 0;
   int hi = maxt_sav - 1;

   // find matching entry from previous frame and make ticks elapsed
   while (lo <= hi) {
      i = (lo + hi) / 2;
      if (this->tid < hist_sav[i].pid)
         hi = i - 1;
      else if (likely(this->tid > hist_sav[i].pid))
         lo = i + 1;
      else {
         tics -= hist_sav[i].tics;
         break;
      }
   }
}
#else
{
   HST_t tmp;
   const HST_t *ptr;
   tmp.pid = this->tid;
   ptr = bsearch(&tmp, hist_sav, maxt_sav, sizeof tmp, sort_HST_t);
   if(ptr) tics -= ptr->tics;
}
#endif

   // we're just saving elapsed tics, to be converted into %cpu if
   // this task wins it's displayable screen row lottery... */
   this->pcpu = tics;
// if (Frames_maxcmdln) { }
   // shout this to the world with the final call (or us the next time in)
   Frame_maxtask++;
}


        /*
         * This guy's modeled on libproc's 'readproctab' function except
         * we reuse and extend any prior proc_t's.  He's been customized
         * for our specific needs and to avoid the use of <stdarg.h> */
static proc_t **procs_refresh (proc_t **table, int flags)
{
#define PTRsz  sizeof(proc_t *)
#define ENTsz  sizeof(proc_t)
   static unsigned savmax = 0;          // first time, Bypass: (i)
   proc_t *ptsk = (proc_t *)-1;         // first time, Force: (ii)
   unsigned curmax = 0;                 // every time  (jeeze)
   PROCTAB* PT;
   static int show_threads_was_enabled = 0; // optimization

   prochlp(NULL);                       // prep for a new frame
   if (Monpidsidx)
      PT = openproc(flags, Monpids);
   else
      PT = openproc(flags);

   // i) Allocated Chunks:  *Existing* table;  refresh + reuse
   if (!(CHKw(Curwin, Show_THREADS))) {
      while (curmax < savmax) {
         if (table[curmax]->cmdline) {
            unsigned idx;
            // Skip if Show_THREADS was never enabled
            if (show_threads_was_enabled) {
               for (idx = curmax + 1; idx < savmax; idx++) {
                  if (table[idx]->cmdline == table[curmax]->cmdline)
                     table[idx]->cmdline = NULL;
               }
            }
            free(*table[curmax]->cmdline);
            table[curmax]->cmdline = NULL;
         }
         if (unlikely(!(ptsk = readproc(PT, table[curmax])))) break;
         prochlp(ptsk);                    // tally & complete this proc_t
         ++curmax;
      }
   }
   else {                          // show each thread in a process separately
      while (curmax < savmax) {
         proc_t *ttsk;
         if (unlikely(!(ptsk = readproc(PT, NULL)))) break;
         show_threads_was_enabled = 1;
         while (curmax < savmax) {
            unsigned idx;
            if (table[curmax]->cmdline) {
               // threads share the same cmdline storage.  'table' is
               // qsort()ed, so must look through the rest of the table.
               for (idx = curmax + 1; idx < savmax; idx++) {
                  if (table[idx]->cmdline == table[curmax]->cmdline)
                     table[idx]->cmdline = NULL;
               }
               free(*table[curmax]->cmdline);  // only free once
               table[curmax]->cmdline = NULL;
            }
            if (!(ttsk = readtask(PT, ptsk, table[curmax]))) break;
            prochlp(ttsk);
            ++curmax;
         }
         free(ptsk);  // readproc() proc_t not used
      }
   }

   // ii) Unallocated Chunks:  *New* or *Existing* table;  extend + fill
   if (!(CHKw(Curwin, Show_THREADS))) {
      while (ptsk) {
         // realloc as we go, keeping 'table' ahead of 'currmax++'
         table = alloc_r(table, (curmax + 1) * PTRsz);
         // here, readproc will allocate the underlying proc_t stg
         if (likely(ptsk = readproc(PT, NULL))) {
            prochlp(ptsk);                 // tally & complete this proc_t
            table[curmax++] = ptsk;
         }
      }
   }
   else {                          // show each thread in a process separately
      while (ptsk) {
         proc_t *ttsk;
         if (likely(ptsk = readproc(PT, NULL))) {
            show_threads_was_enabled = 1;
            while (1) {
               table = alloc_r(table, (curmax + 1) * PTRsz);
               if (!(ttsk = readtask(PT, ptsk, NULL))) break;
               prochlp(ttsk);
               table[curmax++] = ttsk;
            }
            free(ptsk);   // readproc() proc_t not used
         }
      }
   }
   closeproc(PT);

   // iii) Chunkless:  make 'eot' entry, after ensuring proc_t exists
   if (curmax >= savmax) {
      table = alloc_r(table, (curmax + 1) * PTRsz);
      // here, we must allocate the underlying proc_t stg ourselves
      table[curmax] = alloc_c(ENTsz);
      savmax = curmax + 1;
   }
   // this frame's end, but not necessarily end of allocated space
   table[curmax]->tid = -1;
   return table;

#undef PTRsz
#undef ENTsz
}

/*######  Field Table/RCfile compatability support  ######################*/

// from either 'stat' or 'status' (preferred), via bits not otherwise used
#define L_EITHER   PROC_SPARE_1
// These are the Fieldstab.lflg values used here and in reframewins.
// (own identifiers as documentation and protection against changes)
#define L_stat     PROC_FILLSTAT
#define L_statm    PROC_FILLMEM
#define L_status   PROC_FILLSTATUS
#define L_CMDLINE  L_EITHER | PROC_FILLARG
#define L_EUSER    PROC_FILLUSR
#define L_RUSER    L_status | PROC_FILLUSR
#define L_GROUP    L_status | PROC_FILLGRP
#define L_NONE     0
// for reframewins and summary_show 1st pass
#define L_DEFAULT  PROC_FILLSTAT

// a temporary macro, soon to be undef'd...
#define SF(f) (QFP_t)sort_P_ ## f

        /* These are our gosh darn 'Fields' !
           They MUST be kept in sync with pflags !!
           note: for integer data, the length modifiers found in .fmts may
                 NOT reflect the true field type found in proc_t -- this plus
                 a cast when/if displayed provides minimal width protection. */
static FLD_t Fieldstab[] = {
/* .lflg anomolies:
      P_UID, L_NONE  - natural outgrowth of 'stat()' in readproc        (euid)
      P_CPU, L_stat  - never filled by libproc, but requires times      (pcpu)
      P_CMD, L_stat  - may yet require L_CMDLINE in reframewins  (cmd/cmdline)
      L_EITHER       - must L_status, else 64-bit math, __udivdi3 on 32-bit !
      keys   head           fmts     width   scale  sort   desc                     lflg
     ------  -----------    -------  ------  -----  -----  ----------------------   -------- */
   { "AaAa", "   PID",      " %5u",     -1,    -1, SF(PID), "Process Id",           L_NONE   },
   { "BbBb", "  PPID",      " %5u",     -1,    -1, SF(PPD), "Parent Process Pid",   L_EITHER },
   { "CcQq", " RUSER   ",   " %-8.8s",  -1,    -1, SF(URR), "Real user name",       L_RUSER  },
   { "DdCc", "  UID",       " %4u",     -1,    -1, SF(UID), "User Id",              L_NONE   },
   { "EeDd", " USER    ",   " %-8.8s",  -1,    -1, SF(URE), "User Name",            L_EUSER  },
   { "FfNn", " GROUP   ",   " %-8.8s",  -1,    -1, SF(GRP), "Group Name",           L_GROUP  },
   { "GgGg", " TTY     ",   " %-8.8s",   8,    -1, SF(TTY), "Controlling Tty",      L_stat   },
   { "HhHh", "  PR",        " %3d",     -1,    -1, SF(PRI), "Priority",             L_stat   },
   { "IiIi", "  NI",        " %3d",     -1,    -1, SF(NCE), "Nice value",           L_stat   },
   { "JjYy", " #C",         " %2u",     -1,    -1, SF(CPN), "Last used cpu (SMP)",  L_stat   },
   { "KkEe", " %CPU",       " %#4.1f",  -1,    -1, SF(CPU), "CPU usage",            L_stat   },
   { "LlWw", "   TIME",     " %6.6s",    6,    -1, SF(TME), "CPU Time",             L_stat   },
   { "MmRr", "    TIME+ ",  " %9.9s",    9,    -1, SF(TME), "CPU Time, hundredths", L_stat   },
   { "NnFf", " %MEM",       " %#4.1f",  -1,    -1, SF(RES), "Memory usage (RES)",   L_statm  },
   { "OoMm", "  VIRT",      " %5.5s",    5, SK_Kb, SF(VRT), "Virtual Image (kb)",   L_statm  },
   { "PpOo", " SWAP",       " %4.4s",    4, SK_Kb, SF(SWP), "Swapped size (kb)",    L_statm  },
   { "QqTt", "  RES",       " %4.4s",    4, SK_Kb, SF(RES), "Resident size (kb)",   L_statm  },
   { "RrKk", " CODE",       " %4.4s",    4, SK_Kb, SF(COD), "Code size (kb)",       L_statm  },
   { "SsLl", " DATA",       " %4.4s",    4, SK_Kb, SF(DAT), "Data+Stack size (kb)", L_statm  },
   { "TtPp", "  SHR",       " %4.4s",    4, SK_Kb, SF(SHR), "Shared Mem size (kb)", L_statm  },
   { "UuJj", " nFLT",       " %4.4s",    4, SK_no, SF(FLT), "Page Fault count",     L_stat   },
   { "VvSs", " nDRT",       " %4.4s",    4, SK_no, SF(DRT), "Dirty Pages count",    L_statm  },
   { "WwVv", " S",          " %c",      -1,    -1, SF(STA), "Process Status",       L_EITHER },
   // next entry's special: '.head' will be formatted using table entry's own
   //                       '.fmts' plus runtime supplied conversion args!
   { "XxXx", " COMMAND",    " %-*.*s",  -1,    -1, SF(CMD), "Command name/line",    L_EITHER },
   { "YyUu", " WCHAN    ",  " %-9.9s",  -1,    -1, SF(WCH), "Sleeping in Function", L_stat   },
   // next entry's special: the 0's will be replaced with '.'!
   { "ZzZz", " Flags   ",   " %08lx",   -1,    -1, SF(FLG), "Task Flags <sched.h>", L_stat   },
#if 0
   { "..Qq", "   A",        " %4.4s",    4, SK_no, SF(PID), "Accessed Page count",  L_stat   },
   { "..Nn", "  TRS",       " %4.4s",    4, SK_Kb, SF(PID), "Code in memory (kb)",  L_stat   },
   { "..Rr", "  WP",        " %4.4s",    4, SK_no, SF(PID), "Unwritable Pages",     L_stat   },
   { "Jj[{", " #C",         " %2u",     -1,    -1, SF(CPN), "Last used cpu (SMP)",  L_stat   },
   { "..\\|"," Bad",        " %2u",     -1,    -1, SF(CPN), "-- must ignore | --",  0        },
   { "..]}", " Bad",        " %2u",     -1,    -1, SF(CPN), "-- not used --",       0        },
   { "..^~", " Bad",        " %2u",     -1,    -1, SF(CPN), "-- not used --",       0        },
#endif
};
#undef SF


        /* All right, those-that-follow -- Listen Up!
         * For the above table keys and the following present/future rc file
         * compatibility support, you have Mr. Albert D. Cahalan to thank.
         * He must have been in a 'Christmas spirit'.  Were it left to me,
         * this top would never have gotten that close to the former top's
         * crufty rcfile.  Not only is it illogical, it's odoriferous !
         */

        // used as 'to' and/or 'from' args in the ft_xxx utilities...
#define FT_NEW_fmt 0
#define FT_OLD_fmt 2


#if 0
        // convert, or 0 for failure
static int ft_cvt_char (const int fr, const int to, int c) {
   int j = -1;

   while (++j < MAXTBL(Fieldstab)) {
      if (c == Fieldstab[j].keys[fr])   return Fieldstab[j].keys[to];
      if (c == Fieldstab[j].keys[fr+1]) return Fieldstab[j].keys[to+1];
   }
   return 0;
}
#endif


        // convert
static inline int ft_get_char (const int fr, int i) {
   int c;
   if (i < 0) return 0;
   if (i >= MAXTBL(Fieldstab)) return 0;
   c = Fieldstab[i].keys[fr];
   if (c == '.') c = 0;         // '.' marks a bad entry
   return c;
}


#if 0
        // convert, or -1 for failure
static int ft_get_idx (const int fr, int c) {
   int j = -1;

   while (++j < MAXTBL(Fieldstab)) {
      if (c == Fieldstab[j].keys[fr])   return j;
      if (c == Fieldstab[j].keys[fr+1]) return j;
   }
   return -1;
}
#endif


        // convert, or NULL for failure
static const FLD_t *ft_get_ptr (const int fr, int c) {
   int j = -1;

   while (++j < MAXTBL(Fieldstab)) {
      if (c == Fieldstab[j].keys[fr])   return Fieldstab+j;
      if (c == Fieldstab[j].keys[fr+1]) return Fieldstab+j;
   }
   return NULL;
}


#if 0
        // convert, or NULL for failure
static const FLD_t *ft_idx_to_ptr (const int i) {
   if (i < 0) return NULL;
   if (i >= MAXTBL(Fieldstab)) return NULL;
   return Fieldstab + i;
}


        // convert, or -1 for failure
static int ft_ptr_to_idx (const FLD_t *p) {
   int i;
   if (p < Fieldstab) return -1;
   i = p - Fieldstab;
   if (i >= MAXTBL(Fieldstab)) return -1;
   return i;
}
#endif


#if 0
static void rc_bugless (const RCF_t *const rc) {
   const RCW_t *w;
   int i = 0;

   fprintf(stderr,"\n%d %d %f %d\n",
      rc->mode_altscr, rc->mode_irixps, rc->delay_time, rc->win_index
   );
   while(i < 4) {
      w = &rc->win[i++];
      fprintf(stderr, "<%s> <%s> %d %08x %d %d %d %d %d\n",
         w->winname, w->fieldscur, w->sortindx, w->winflags, w->maxtasks,
         w->summclr, w->msgsclr, w->headclr, w->taskclr
      );
   }
}
#endif


// '$HOME/Rc_name' contains multiple lines - 2 global + 3 per window.
//   line 1: an eyecatcher, with a shameless advertisement
//   line 2: an id, Mode_altcsr, Mode_irixps, Delay_time and Curwin.
// For each of the 4 windows:
//   line a: contains winname, fieldscur
//   line b: contains winflags, sortindx, maxtasks
//   line c: contains summclr, msgsclr, headclr, taskclr
//   line d: if present, would crash procps-3.1.1
static int rc_read_new (const char *const buf, RCF_t *rc) {
   int i;
   int cnt;
   const char *cp;

   cp = strstr(buf, "\n\n" RCF_EYECATCHER);
   if (!cp) return -1;
   cp = strchr(cp + 2, '\n');
   if (!cp++) return -2;

   cnt = sscanf(cp, "Id:a, Mode_altscr=%d, Mode_irixps=%d, Delay_time=%f, Curwin=%d\n",
      &rc->mode_altscr, &rc->mode_irixps, &rc->delay_time, &rc->win_index
   );
   if (cnt != 4) return -3;
   cp = strchr(cp, '\n');
   if (!cp++) return -4;

   for (i = 0; i < GROUPSMAX; i++) {
      RCW_t *ptr = &rc->win[i];
      cnt = sscanf(cp, "%3s\tfieldscur=%31s\n", ptr->winname, ptr->fieldscur);
      if (cnt != 2) return 5+100*i;  // OK to have less than 4 windows
      if (WINNAMSIZ <= strlen(ptr->winname)) return -6;
      if (strlen(DEF_FIELDS) != strlen(ptr->fieldscur)) return -7;
      cp = strchr(cp, '\n');
      if (!cp++) return -(8+100*i);

      cnt = sscanf(cp, "\twinflags=%d, sortindx=%u, maxtasks=%d \n",
         &ptr->winflags, &ptr->sortindx, &ptr->maxtasks
      );
      if (cnt != 3) return -(9+100*i);
      cp = strchr(cp, '\n');
      if (!cp++) return -(10+100*i);

      cnt = sscanf(cp, "\tsummclr=%d, msgsclr=%d, headclr=%d, taskclr=%d \n",
         &ptr->summclr, &ptr->msgsclr, &ptr->headclr, &ptr->taskclr
      );
      if (cnt != 4) return -(11+100*i);
      cp = strchr(cp, '\n');
      if (!cp++) return -(12+100*i);
      while (*cp == '\t') {  // skip unknown per-window settings
        cp = strchr(cp, '\n');
        if (!cp++) return -(13+100*i);
      }
   }
   return 13;
}



static int rc_read_old (const char *const buf, RCF_t *rc) {
   const char std[] = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZzJj......";
   const char old[] = "AaBb..CcDd..GgHhIiYyEeWw..FfMmOoTtKkLlPpJjSsVvXxUuZz[{QqNnRr";
   unsigned u;
   const char *cp;
   unsigned c_show = 0;
   int badchar = 0;     // allow a limited number of duplicates and junk

   char scoreboard[256];
   memset(scoreboard, '\0', sizeof scoreboard);

   cp = buf+2;  // skip the "\n\n" we stuck at the beginning
   u = 0;
   for (;;) {
      const char *tmp;
      int c = *cp++;
      if (u+1 >= sizeof rc->win[0].fieldscur) return -1;
      if (c == '\0') return -2;
      if (c == '\n') break;
      if (c & ~0x7f) return -3;
      if (~c & 0x20) c_show |= 1 << (c & 0x1f); // 0x20 means lowercase means hidden
      if (scoreboard[c|0xe0u]) badchar++;       // duplicates not allowed
      scoreboard[c|0xe0u]++;
      tmp = strchr(old,c);
      if (!tmp) continue;
      c = *((tmp-old)+std);
      if (c == '.') continue;
      if (scoreboard[c&0x1fu]) badchar++;       // duplicates not allowed
      scoreboard[c&0x1fu]++;
      rc->win[0].fieldscur[u++] = c;
   }
   rc->win[0].fieldscur[u++] = '\0';
   if (u < 21) return -6;  // catch junk, not good files (had 23 chars in one)
   if (u > 33) return -7;  // catch junk, not good files (had 29 chars in one)
// fprintf(stderr, "badchar: %d\n", badchar); sleep(2);
   if (badchar > 8) return -8;          // too much junk
   if (!c_show) return -9;              // nothing was shown

   // rest of file is optional, but better look right if it exists
   if (!*cp) return 12;
   if (*cp < '2' || *cp > '9') return -13; // stupid, and why isn't '1' valid?
   rc->delay_time = *cp - '0';

   memset(scoreboard, '\0', sizeof(scoreboard));
   for (;;) {
      int c = *++cp & 0xffu;    // protect scoreboard[] from negative char
      if (!c) return -14;       // not OK to hit EOL w/o '\n'
      if (c == '\n') break;
      switch (c) {
         case ' ':
         case '.':
         case '0' ... '9':
            return -15;                 // not supposed to have digits here

//       case 's':                      // mostly for global rcfile
//          rc->mode_secure = 1;
//          break;
         case 'S':
            rc->win[0].winflags |= Show_CTIMES;
            break;
         case 'c':
            rc->win[0].winflags |= Show_CMDLIN;
            break;
         case 'i':
            rc->win[0].winflags &= ~Show_IDLEPS;
            break;
         case 'H':
            rc->win[0].winflags |= Show_THREADS;
            break;
         case 'm':
            rc->win[0].winflags &= ~View_MEMORY;
            break;
         case 'l':
            rc->win[0].winflags &= ~View_LOADAV;
            break;
         case 't':
            rc->win[0].winflags &= ~View_STATES;
            break;
         case 'I':
            rc->mode_irixps = 0;
            break;

         case 'M':
            c = 0; // for scoreboard
            rc->win[0].sortindx = P_MEM;
            break;
         case 'P':
            c = 0; // for scoreboard
            rc->win[0].sortindx = P_CPU;
            break;
         case 'A':                      // supposed to be start_time
            c = 0; // for scoreboard
            rc->win[0].sortindx = P_PID;
            break;
         case 'T':
            c = 0; // for scoreboard
            rc->win[0].sortindx = P_TM2;
            break;
         case 'N':
            c = 0; // for scoreboard
            rc->win[0].sortindx = P_PID;
            break;

         default:
            // just ignore it, except for the scoreboard of course
            break;
      }
      if (scoreboard[c]) return -16;    // duplicates not allowed
      scoreboard[c] = 1;
   }
   return 17;
}


static void rc_write_new (FILE *fp) {
   int i;

   fprintf(fp, RCF_EYECATCHER "\"%s with windows\"\t\t# shameless braggin'\n",
      Myname
   );
   fprintf(fp, RCF_DEPRECATED
      "Mode_altscr=%d, Mode_irixps=%d, Delay_time=%.3f, Curwin=%u\n",
      Rc.mode_altscr, Rc.mode_irixps, Rc.delay_time, (unsigned)(Curwin - Winstk)
   );
   for (i = 0; i < GROUPSMAX; i++) {
      char buf[40];
      char *cp = Winstk[i].rc.fieldscur;
      int j = 0;

      while (j < 36) {
         int c = *cp++ & 0xff;
         switch (c) {
            case '.':
            case 1 ... ' ':
            case 0x7f ... 0xff:
               continue;                // throw away junk (some of it)
            default:
               buf[j++] = c;            // gets the '\0' too
         }
         if (!c) break;
      }
      fprintf(fp, "%s\tfieldscur=%s\n",
         Winstk[i].rc.winname, buf
      );
      fprintf(fp, "\twinflags=%d, sortindx=%d, maxtasks=%d\n",
         Winstk[i].rc.winflags, Winstk[i].rc.sortindx, Winstk[i].rc.maxtasks
      );
      fprintf(fp, "\tsummclr=%d, msgsclr=%d, headclr=%d, taskclr=%d\n",
         Winstk[i].rc.summclr, Winstk[i].rc.msgsclr,
         Winstk[i].rc.headclr, Winstk[i].rc.taskclr
      );
   }
}


static const char *rc_write_whatever (void) {
   FILE *fp = fopen(Rc_name, "w");

   if (!fp) return strerror(errno);
   rc_write_new(fp);
   fclose(fp);
   return NULL;
}


/*######  Startup routines  ##############################################*/

// No mater what *they* say, we handle the really really BIG and
// IMPORTANT stuff upon which all those lessor functions depend!
static void before (char *me)
{
   int i;

      /* setup our program name -- big! */
   Myname = strrchr(me, '/');
   if (Myname) ++Myname; else Myname = me;

      /* establish cpu particulars -- even bigger! */
   Cpu_tot = smp_num_cpus;
   if (linux_version_code > LINUX_VERSION(2, 5, 41))
      States_fmts = STATES_line2x5;
   if (linux_version_code >= LINUX_VERSION(2, 6, 0))  // grrr... only some 2.6.0-testX :-(
      States_fmts = STATES_line2x6;
   if (linux_version_code >= LINUX_VERSION(2, 6, 11))
      States_fmts = STATES_line2x7;

      /* get virtual page size -- nearing huge! */
   Page_size = getpagesize();
   i = Page_size;
   while(i>1024){
     i >>= 1;
     page_to_kb_shift++;
   }

   pcpu_max_value = 99.9;

   Fieldstab[P_CPN].head = " P";
   Fieldstab[P_CPN].fmts = " %1u";
   if(smp_num_cpus>9){
      Fieldstab[P_CPN].head = "  P";
      Fieldstab[P_CPN].fmts = " %2u";
   }
   if(smp_num_cpus>99){
      Fieldstab[P_CPN].head = "   P";
      Fieldstab[P_CPN].fmts = " %3u";
   }
   if(smp_num_cpus>999){
      Fieldstab[P_CPN].head = "    P";
      Fieldstab[P_CPN].fmts = " %4u";
   }

   {
      static char pid_fmt[6];
      unsigned pid_digits = get_pid_digits();
      if(pid_digits<4) pid_digits=4;
      snprintf(pid_fmt, sizeof pid_fmt, " %%%uu", pid_digits);
      Fieldstab[P_PID].fmts = pid_fmt;
      Fieldstab[P_PID].head = "        PID" + 10 - pid_digits;
      Fieldstab[P_PPD].fmts = pid_fmt;
      Fieldstab[P_PPD].head = "       PPID" + 10 - pid_digits;
   }
}


// Config file read *helper* function.
// Anything missing won't show as a choice in the field editor,
// so make sure there is exactly one of each letter.
//
// Due to Rik blindly accepting damem's broken patches, procps-2.0.1x
// has 3 ("three"!!!) instances of "#C", "LC", or "CPU". Fix that too.
static void confighlp (char *fields) {
   unsigned upper[PFLAGSSIZ];
   unsigned lower[PFLAGSSIZ];
   char c;
   char *cp;

   memset(upper, '\0', sizeof upper);
   memset(lower, '\0', sizeof lower);

   cp = fields;
   for (;;) {
      c = *cp++;
      if (!c) break;
      if(isupper(c)) upper[c&0x1f]++;
      else           lower[c&0x1f]++;
   }

   c = 'a';
   while (c <= 'z') {
      if (upper[c&0x1f] && lower[c&0x1f]) {
         lower[c&0x1f] = 0;             // got both, so wipe out unseen column
         for (;;) {
            cp = strchr(fields, c);
            if (cp) memmove(cp, cp+1, strlen(cp));
            else break;
         }
      }
      while (lower[c&0x1f] > 1) {               // got too many a..z
         lower[c&0x1f]--;
         cp = strchr(fields, c);
         memmove(cp, cp+1, strlen(cp));
      }
      while (upper[c&0x1f] > 1) {               // got too many A..Z
         upper[c&0x1f]--;
         cp = strchr(fields, toupper(c));
         memmove(cp, cp+1, strlen(cp));
      }
      if (!upper[c&0x1f] && !lower[c&0x1f]) {   // both missing
         lower[c&0x1f]++;
         memmove(fields+1, fields, strlen(fields)+1);
         fields[0] = c;
      }
      c++;
   }
}


// First attempt to read the /etc/rcfile which contains two lines
// consisting of the secure mode switch and an update interval.
// It's presence limits what ordinary users are allowed to do.
// (it's actually an old-style config file)
//
// Then build the local rcfile name and try to read a crufty old-top
// rcfile (whew, odoriferous), which may contain an embedded new-style
// rcfile.   Whether embedded or standalone, new-style rcfile values
// will always override that crufty stuff!
// note: If running in secure mode via the /etc/rcfile,
//       Delay_time will be ignored except for root.
static void configs_read (void)
{
   const RCF_t def_rcf = DEF_RCFILE;
   char fbuf[MEDBUFSIZ];
   int i, fd;
   RCF_t rcf;
   float delay = Rc.delay_time;

   // read part of an old-style config in /etc/toprc
   fd = open(SYS_RCFILESPEC, O_RDONLY);
   if (fd > 0) {
      ssize_t num;
      num = read(fd, fbuf, sizeof(fbuf) - 1);
      if (num > 0) {
         const char *sec = strchr(fbuf, 's');
         const char *eol = strchr(fbuf, '\n');
         if (eol) {
            const char *two = eol + 1;  // line two
            if (sec < eol) Secure_mode = !!sec;
            eol = strchr(two, '\n');
            if (eol && eol > two && isdigit(*two)) Rc.delay_time = atof(two);
         }
      }
      close(fd);
   }

   if (getenv("TOPRC")) {    // should switch on Myname before documenting this?
      // not the most optimal here...
      snprintf(Rc_name, sizeof(Rc_name), "%s", getenv("TOPRC"));
   } else {
      snprintf(Rc_name, sizeof(Rc_name), ".%src", Myname);  // eeew...
      if (getenv("HOME"))
         snprintf(Rc_name, sizeof(Rc_name), "%s/.%src", getenv("HOME"), Myname);
   }

   rcf = def_rcf;
   fd = open(Rc_name, O_RDONLY);
   if (fd > 0) {
      ssize_t num;
      num = read(fd, fbuf+2, sizeof(fbuf) -3);
      if (num > 0) {
         fbuf[0] = '\n';
         fbuf[1] = '\n';
         fbuf[num+2] = '\0';
//fprintf(stderr,"rc_read_old returns %d\n",rc_read_old(fbuf, &rcf));
//sleep(2);
         if (rc_read_new(fbuf, &rcf) < 0) {
            rcf = def_rcf;                       // on failure, maybe mangled
            if (rc_read_old(fbuf, &rcf) < 0) rcf = def_rcf;
         }
         delay = rcf.delay_time;
      }
      close(fd);
   }

   // update Rc defaults, establish a Curwin and fix up the window stack
   Rc.mode_altscr = rcf.mode_altscr;
   Rc.mode_irixps = rcf.mode_irixps;
   if (rcf.win_index >= GROUPSMAX) rcf.win_index = 0;
   Curwin = &Winstk[rcf.win_index];
   for (i = 0; i < GROUPSMAX; i++) {
      memcpy(&Winstk[i].rc, &rcf.win[i], sizeof rcf.win[i]);
      confighlp(Winstk[i].rc.fieldscur);
   }

   if(Rc.mode_irixps && smp_num_cpus>1){
      // good for 100 CPUs per process
      pcpu_max_value = 9999.0;
      Fieldstab[P_CPU].fmts = " %4.0f";
   }

   // lastly, establish the true runtime secure mode and delay time
   if (!getuid()) Secure_mode = 0;
   if (!Secure_mode) Rc.delay_time = delay;
}


// Parse command line arguments.
// Note: it's assumed that the rc file(s) have already been read
//       and our job is to see if any of those options are to be
//       overridden -- we'll force some on and negate others in our
//       best effort to honor the loser's (oops, user's) wishes...
static void parse_args (char **args)
{
   /* differences between us and the former top:
      -C (separate CPU states for SMP) is left to an rcfile
      -p (pid monitoring) allows a comma delimited list
      -q (zero delay) eliminated as redundant, incomplete and inappropriate
            use: "nice -n-10 top -d0" to achieve what was only claimed
      -c,i,S act as toggles (not 'on' switches) for enhanced user flexibility
      .  no deprecated/illegal use of 'breakargv:' with goto
      .  bunched args are actually handled properly and none are ignored
      .  we tolerate NO whitespace and NO switches -- maybe too tolerant? */
   static const char usage[] =
      " -hv | -bcisSH -d delay -n iterations [-u user | -U user] -p pid [,pid ...]";
   float tmp_delay = MAXFLOAT;
   char *p;

   while (*args) {
      const char *cp = *(args++);

      while (*cp) {
         switch (*cp) {
            case '\0':
            case '-':
               break;
            case 'b':
               Batch = 1;
               break;
            case 'c':
               TOGw(Curwin, Show_CMDLIN);
               break;
            case 'd':
               if (cp[1]) ++cp;
               else if (*args) cp = *args++;
               else std_err("-d requires argument");
                  /* a negative delay will be dealt with shortly... */
               if (sscanf(cp, "%f", &tmp_delay) != 1)
                  std_err(fmtmk("bad delay '%s'", cp));
               break;
            case 'H':
               TOGw(Curwin, Show_THREADS);
               break;
            case 'h':
            case 'v': case 'V':
               std_out(fmtmk("%s\nusage:\t%s%s", procps_version, Myname, usage));
            case 'i':
               TOGw(Curwin, Show_IDLEPS);
               Curwin->rc.maxtasks = 0;
               break;
            case 'n':
               if (cp[1]) cp++;
               else if (*args) cp = *args++;
               else std_err("-n requires argument");
               if (sscanf(cp, "%d", &Loops) != 1 || Loops < 1)
                  std_err(fmtmk("bad iterations arg '%s'", cp));
               break;
            case 'p':
               do {
                  if (selection_type && selection_type != 'p') std_err("conflicting process selection");
                  selection_type = 'p';
                  if (cp[1]) cp++;
                  else if (*args) cp = *args++;
                  else std_err("-p argument missing");
                  if (Monpidsidx >= MONPIDMAX)
                     std_err(fmtmk("pid limit (%d) exceeded", MONPIDMAX));
                  if (sscanf(cp, "%d", &Monpids[Monpidsidx]) != 1 || Monpids[Monpidsidx] < 0)
                     std_err(fmtmk("bad pid '%s'", cp));
                  if (!Monpids[Monpidsidx])
                     Monpids[Monpidsidx] = getpid();
                  Monpidsidx++;
                  if (!(p = strchr(cp, ',')))
                     break;
                  cp = p;
               } while (*cp);
               break;
            case 's':
               Secure_mode = 1;
               break;
            case 'S':
               TOGw(Curwin, Show_CTIMES);
               break;
            case 'u':
               do {
                  const char *errmsg;
                  if (selection_type /* && selection_type != 'u' */) std_err("conflicting process selection");
                  if (cp[1]) cp++;
                  else if (*args) cp = *args++;
                  else std_err("-u missing name");
                  errmsg = parse_uid(cp, &selection_uid);
                  if (errmsg) std_err(errmsg);
                  selection_type = 'u';
                  cp += snprintf(Curwin->colusrnam, USRNAMSIZ-1, "%s", cp); // FIXME: junk
               } while(0);
               break;
            case 'U':
               do {
                  const char *errmsg;
                  if (selection_type /* && selection_type != 'U' */) std_err("conflicting process selection");
                  if (cp[1]) cp++;
                  else if (*args) cp = *args++;
                  else std_err("-u missing name");
                  errmsg = parse_uid(cp, &selection_uid);
                  if (errmsg) std_err(errmsg);
                  selection_type = 'U';
                  cp += snprintf(Curwin->colusrnam, USRNAMSIZ-1, "%s", cp); // FIXME: junk
               } while(0);
               break;
            default :
               std_err(fmtmk("unknown argument '%c'\nusage:\t%s%s"
                  , *cp, Myname, usage));

         } /* end: switch (*cp) */

            /* advance cp and jump over any numerical args used above */
         if (*cp) cp += strspn(&cp[1], "- ,.1234567890") + 1;
      } /* end: while (*cp) */
   } /* end: while (*args) */

      /* fixup delay time, maybe... */
   if (MAXFLOAT != tmp_delay) {
      if (Secure_mode || tmp_delay < 0)
         msg_save("Delay time Not changed");
      else
         Rc.delay_time = tmp_delay;
   }
}


        /*
         * Set up the terminal attributes */
static void whack_terminal (void)
{
   struct termios newtty;

   if (Batch) {
      setupterm("dumb", STDOUT_FILENO, NULL);
      return;
   }
   setupterm(NULL, STDOUT_FILENO, NULL);
   if (tcgetattr(STDIN_FILENO, &Savedtty) == -1)
      std_err("failed tty get");
   newtty = Savedtty;
   newtty.c_lflag &= ~(ICANON | ECHO);
   newtty.c_oflag &= ~(TAB3);
   newtty.c_cc[VMIN] = 1;
   newtty.c_cc[VTIME] = 0;

   Ttychanged = 1;
   if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &newtty) == -1) {
      putp(Cap_clr_scr);
      std_err(fmtmk("failed tty set: %s", strerror(errno)));
   }
   tcgetattr(STDIN_FILENO, &Rawtty);
#ifndef STDOUT_IOLBF
   // thanks anyway stdio, but we'll manage buffering at the frame level...
   setbuffer(stdout, Stdout_buf, sizeof(Stdout_buf));
#endif
   putp(Cap_clr_scr);
   fflush(stdout);
}


/*######  Field Selection/Ordering routines  #############################*/


// Display each field represented in the Fields Table along with its
// description and mark (with a leading asterisk) fields associated
// with upper case letter(s) in the passed 'fields string'.
//
// After all fields have been displayed, some extra explanatory
// text may also be output
static void display_fields (const char *fields, const char *xtra)
{
#define yRSVD 3
   const char *p;
   int i, cmax = Screen_cols / 2, rmax = Screen_rows - yRSVD;

   /* we're relying on callers to first clear the screen and thus avoid screen
      flicker if they're too lazy to handle their own asterisk (*) logic */
   putp(Curwin->cap_bold);
   for (i = 0; fields[i]; ++i) {
      const FLD_t *f = ft_get_ptr(FT_NEW_fmt, fields[i]);
      int b = isupper(fields[i]);

      if (!f) continue;                 // hey, should be std_err!
      for (p = f->head; ' ' == *p; ++p) // advance past any leading spaces
         ;
      PUTT("%s%s%c %c: %-10s = %s",
         tg2((i / rmax) * cmax, (i % rmax) + yRSVD),
         b ? Curwin->cap_bold : Cap_norm,
         b ? '*' : ' ',
         fields[i],
         p,
         f->desc
      );
   }
   if (xtra) {
      putp(Curwin->capclr_rownorm);
      while ((p = strchr(xtra, '\n'))) {
         ++i;
         PUTT("%s%.*s",
            tg2((i / rmax) * cmax, (i % rmax) + yRSVD),
            (int)(p - xtra),
            xtra
         );
         xtra = ++p;
      }
   }
   putp(Caps_off);

#undef yRSVD
}


// Change order of displayed fields.
static void fields_reorder (void)
{
   static const char prompt[] =
      "Upper case letter moves field left, lower case right";
   char c, *p;
   int i;

   putp(Cap_clr_scr);
   putp(Cap_curs_huge);
   for (;;) {
      display_fields(Curwin->rc.fieldscur, FIELDS_xtra);
      show_special(1, fmtmk(FIELDS_current
         , Cap_home, Curwin->rc.fieldscur, Curwin->grpname, prompt));
      chin(0, &c, 1);
      if (!ft_get_ptr(FT_NEW_fmt, c)) break;
      i = toupper(c) - 'A';
      if (((p = strchr(Curwin->rc.fieldscur, i + 'A')))
      || ((p = strchr(Curwin->rc.fieldscur, i + 'a')))) {
         if (isupper(c)) p--;
         if (('\0' != p[1]) && (p >= Curwin->rc.fieldscur)) {
            c    = p[0];
            p[0] = p[1];
            p[1] = c;
         }
      }
   }
   putp(Cap_curs_norm);
}

// Select sort field.
static void fields_sort (void)
{
   static const char prompt[] =
      "Select sort field via field letter, type any other key to return";
   char phoney[PFLAGSSIZ];
   char c, *p;
   int i, x;

   strcpy(phoney, NUL_FIELDS);
   x = i = Curwin->rc.sortindx;
   putp(Cap_clr_scr);
   putp(Cap_curs_huge);
   for (;;) {
      p  = phoney + i;
      *p = toupper(*p);
      display_fields(phoney, SORT_xtra);
      show_special(1, fmtmk(SORT_fields, Cap_home, *p, Curwin->grpname, prompt));
      chin(0, &c, 1);
      if (!ft_get_ptr(FT_NEW_fmt, c)) break;
      i = toupper(c) - 'A';
      *p = tolower(*p);
      x = i;
   }
   if ((p = strchr(Curwin->rc.fieldscur, x + 'a')))
      *p = x + 'A';
   Curwin->rc.sortindx = x;
   putp(Cap_curs_norm);
}


// Toggle displayed fields.
static void fields_toggle (void)
{
   static const char prompt[] =
      "Toggle fields via field letter, type any other key to return";
   char c, *p;
   int i;

   putp(Cap_clr_scr);
   putp(Cap_curs_huge);
   for (;;) {
      display_fields(Curwin->rc.fieldscur, FIELDS_xtra);
      show_special(1, fmtmk(FIELDS_current, Cap_home, Curwin->rc.fieldscur, Curwin->grpname, prompt));
      chin(0, &c, 1);
      if (!ft_get_ptr(FT_NEW_fmt, c)) break;
      i = toupper(c) - 'A';
      if ((p = strchr(Curwin->rc.fieldscur, i + 'A')))
         *p = i + 'a';
      else if ((p = strchr(Curwin->rc.fieldscur, i + 'a')))
         *p = i + 'A';
   }
   putp(Cap_curs_norm);
}

/*######  Windows/Field Groups support  #################################*/

// For each of the four windows:
//    1) Set the number of fields/columns to display
//    2) Create the field columns heading
//    3) Set maximum cmdline length, if command lines are in use
// In the process, the required PROC_FILLxxx flags will be rebuilt!
static void reframewins (void)
{
   WIN_t *w;
   char *s;
   const char *h;
   int i, needpsdb = 0;

// Frames_libflags = 0;  // should be called only when it's zero
// Frames_maxcmdln = 0;  // to become largest from up to 4 windows, if visible
   w = Curwin;
   do {
      if (!Rc.mode_altscr || CHKw(w, VISIBLE_tsk)) {
         // build window's procflags array and establish a tentative maxpflgs
         for (i = 0, w->maxpflgs = 0; w->rc.fieldscur[i]; i++) {
            if (isupper(w->rc.fieldscur[i]))
               w->procflags[w->maxpflgs++] = w->rc.fieldscur[i] - 'A';
         }

         /* build a preliminary columns header not to exceed screen width
            while accounting for a possible leading window number */
         *(s = w->columnhdr) = '\0';
         if (Rc.mode_altscr) s = scat(s, " ");
         for (i = 0; i < w->maxpflgs; i++) {
            h = Fieldstab[w->procflags[i]].head;
            // oops, won't fit -- we're outta here...
            if (Screen_cols+1 < (int)((s - w->columnhdr) + strlen(h))) break;
            s = scat(s, h);
         }

         // establish the final maxpflgs and prepare to grow the command column
         // heading via maxcmdln - it may be a fib if P_CMD wasn't encountered,
         // but that's ok because it won't be displayed anyway
         w->maxpflgs = i;
         w->maxcmdln = Screen_cols - (strlen(w->columnhdr) - strlen(Fieldstab[P_CMD].head));

         // finally, we can build the true run-time columns header, format the
         // command column heading, if P_CMD is really being displayed, and
         // rebuild the all-important PROC_FILLxxx flags that will be used
         // until/if we're we're called again
         *(s = w->columnhdr) = '\0';
//         if (Rc.mode_altscr) s = scat(s, fmtmk("%d", w->winnum));
         for (i = 0; i < w->maxpflgs; i++) {
            int advance = (i==0) && !Rc.mode_altscr;
            h = Fieldstab[w->procflags[i]].head;
            if (P_WCH == w->procflags[i]) needpsdb = 1;
            if (P_CMD == w->procflags[i]) {
               s = scat(s, fmtmk(Fieldstab[P_CMD].fmts+advance, w->maxcmdln, w->maxcmdln, "COMMAND"/*h*/  ));
               if (CHKw(w, Show_CMDLIN)) {
                  Frames_libflags |= L_CMDLINE;
//                if (w->maxcmdln > Frames_maxcmdln) Frames_maxcmdln = w->maxcmdln;
               }
            } else
               s = scat(s, h+advance);
            Frames_libflags |= Fieldstab[w->procflags[i]].lflg;
         }
         if (Rc.mode_altscr) w->columnhdr[0] = w->winnum + '0';
      }
      if (Rc.mode_altscr) w = w->next;
   } while (w != Curwin);

   // do we need the kernel symbol table (and is it already open?)
   if (needpsdb) {
      if (No_ksyms == -1) {
         No_ksyms = 0;
         if (open_psdb_message(NULL, msg_save))
            No_ksyms = 1;
         else
            PSDBopen = 1;
      }
   }

   if (selection_type=='U') Frames_libflags |= L_status;

   if (Frames_libflags & L_EITHER) {
      Frames_libflags &= ~L_EITHER;
      if (!(Frames_libflags & L_stat)) Frames_libflags |= L_status;
   }
   if (!Frames_libflags) Frames_libflags = L_DEFAULT;
   if (selection_type=='p') Frames_libflags |= PROC_PID;
}


// Value a window's name and make the associated group name.
static void win_names (WIN_t *q, const char *name)
{
   sprintf(q->rc.winname, "%.*s", WINNAMSIZ -1, name);
   sprintf(q->grpname, "%d:%.*s", q->winnum, WINNAMSIZ -1, name);
}


// Display a window/field group (ie. make it "current").
static void win_select (char ch)
{
   static const char prompt[] = "Choose field group (1 - 4)";

   /* if there's no ch, it means we're supporting the external interface,
      so we must try to get our own darn ch by begging the user... */
   if (!ch) {
      show_pmt(prompt);
      chin(0, (char *)&ch, 1);
   }
   switch (ch) {
      case 'a':                         /* we don't carry 'a' / 'w' in our */
         Curwin = Curwin->next;         /* pmt - they're here for a good   */
         break;                         /* friend of ours -- wins_colors.  */
      case 'w':                         /* (however those letters work via */
         Curwin = Curwin->prev;         /* the pmt too but gee, end-loser  */
         break;                         /* should just press the darn key) */
      case '1': case '2':
      case '3': case '4':
         Curwin = &Winstk[ch - '1'];
         break;
   }
}


// Just warn the user when a command can't be honored.
static int win_warn (void)
{
   show_msg(fmtmk("\aCommand disabled, activate %s with '-' or '_'", Curwin->grpname));
   // we gotta' return false 'cause we're somewhat well known within
   // macro society, by way of that sassy little tertiary operator...
   return 0;
}


// Change colors *Helper* function to save/restore settings;
// ensure colors will show; and rebuild the terminfo strings.
static void winsclrhlp (WIN_t *q, int save)
{
   static int flgssav, summsav, msgssav, headsav, tasksav;

   if (save) {
      flgssav = q->rc.winflags; summsav = q->rc.summclr;
      msgssav = q->rc.msgsclr;  headsav = q->rc.headclr; tasksav = q->rc.taskclr;
      SETw(q, Show_COLORS);
   } else {
      q->rc.winflags = flgssav; q->rc.summclr = summsav;
      q->rc.msgsclr = msgssav;  q->rc.headclr = headsav; q->rc.taskclr = tasksav;
   }
   capsmk(q);
}


// Change colors used in display
static void wins_colors (void)
{
#define kbdABORT  'q'
#define kbdAPPLY  '\n'
   int clr = Curwin->rc.taskclr, *pclr = &Curwin->rc.taskclr;
   char ch, tgt = 'T';

   if (0 >= max_colors) {
      show_msg("\aNo colors to map!");
      return;
   }
   winsclrhlp(Curwin, 1);
   putp(Cap_clr_scr);
   putp(Cap_curs_huge);

   do {
      putp(Cap_home);
         /* this string is well above ISO C89's minimum requirements! */
      show_special(
         1,
         fmtmk(
            COLOR_help,
            procps_version,
            Curwin->grpname,
            CHKw(Curwin, View_NOBOLD) ? "On" : "Off",
            CHKw(Curwin, Show_COLORS) ? "On" : "Off",
            CHKw(Curwin, Show_HIBOLD) ? "On" : "Off",
            tgt,
            clr,
            Curwin->grpname
         )
      );
      chin(0, &ch, 1);
      switch (ch) {
         case 'S':
            pclr = &Curwin->rc.summclr;
            clr = *pclr;
            tgt = ch;
            break;
         case 'M':
            pclr = &Curwin->rc.msgsclr;
            clr = *pclr;
            tgt = ch;
            break;
         case 'H':
            pclr = &Curwin->rc.headclr;
            clr = *pclr;
            tgt = ch;
            break;
         case 'T':
            pclr = &Curwin->rc.taskclr;
            clr = *pclr;
            tgt = ch;
            break;
         case '0' ... '7':
            clr = ch - '0';
            *pclr = clr;
            break;
         case 'B':
            TOGw(Curwin, View_NOBOLD);
            break;
         case 'b':
            TOGw(Curwin, Show_HIBOLD);
            break;
         case 'z':
            TOGw(Curwin, Show_COLORS);
            break;
         case 'a':
         case 'w':
            win_select(ch);
            winsclrhlp(Curwin, 1);
            clr = Curwin->rc.taskclr, pclr = &Curwin->rc.taskclr;
            tgt = 'T';
            break;
      }
      capsmk(Curwin);
   } while (kbdAPPLY != ch && kbdABORT != ch);

   if (kbdABORT == ch)
      winsclrhlp(Curwin, 0);
   putp(Cap_curs_norm);

#undef kbdABORT
#undef kbdAPPLY
}


// Manipulate flag(s) for all our windows.
static void wins_reflag (int what, int flg)
{
   WIN_t *w;

   w = Curwin;
   do {
      switch (what) {
         case Flags_TOG:
            TOGw(w, flg);
            break;
         case Flags_SET:                /* Ummmm, i can't find anybody */
            SETw(w, flg);               /* who uses Flags_set ...      */
            break;
         case Flags_OFF:
            OFFw(w, flg);
            break;
      }
      // a flag with special significance -- user wants to rebalance
      // display so we gotta' 'off' one number then force on two flags...
      if (EQUWINS_cwo == flg) {
         w->rc.maxtasks = 0;
         SETw(w, Show_IDLEPS | VISIBLE_tsk);
      }
      w = w->next;
   } while (w != Curwin);
}


// using a flag to avoid other code seeing inconsistant state
static volatile int need_resize;
static void wins_resize_sighandler (int dont_care_sig)
{
   (void)dont_care_sig;
   need_resize = 1;
   ZAP_TIMEOUT
}


// Set the screen dimensions and arrange for the real workhorse.
// (also) catches:
//    SIGWINCH and SIGCONT
static void wins_resize (void)
{
   struct winsize wz;
   char *env_columns;  // Unix98 environment variable COLUMNS
   char *env_lines;    // Unix98 environment variable LINES

   Screen_cols = columns;   // <term.h>
   Screen_rows = lines;     // <term.h>

   if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &wz) != -1 && wz.ws_col>0 && wz.ws_row>0) {
      Screen_cols = wz.ws_col;
      Screen_rows = wz.ws_row;
   }

   if (Batch) Screen_rows = MAXINT;

   env_columns = getenv("COLUMNS");
   if(env_columns && *env_columns){
      long t;
      char *endptr;
      t = strtol(env_columns, &endptr, 0);
      if(!*endptr && (t>0) && (t<=0x7fffffffL)) Screen_cols = (int)t;
   }
   env_lines   = getenv("LINES");
   if(env_lines && *env_lines){
      long t;
      char *endptr;
      t = strtol(env_lines, &endptr, 0);
      if(!*endptr && (t>0) && (t<=0x7fffffffL)) Screen_rows = (int)t;
   }

   // be crudely tolerant of crude tty emulators
   if (avoid_last_column) Screen_cols--;

   // we might disappoint some folks (but they'll deserve it)
   if (SCREENMAX < Screen_cols) Screen_cols = SCREENMAX;

   // keep our support for output optimization in sync with current reality
   // note: when we're in Batch mode, we don't really need a Pseudo_scrn and
   //       when not Batch, our buffer will contain 1 extra 'line' since
   //       Msg_row is never represented -- but it's nice to have some space
   //       between us and the great-beyond...
   Pseudo_cols = Screen_cols + CLRBUFSIZ + 1;
   if (Batch) Pseudo_size = ROWBUFSIZ + 1;
   else       Pseudo_size = Pseudo_cols * Screen_rows;
   Pseudo_scrn = alloc_r(Pseudo_scrn, Pseudo_size);

   // force rebuild of column headers AND libproc/readproc requirements
   Frames_libflags = 0;
}


// Set up the raw/incomplete field group windows --
// they'll be finished off after startup completes.
// [ and very likely that will override most/all of our efforts ]
// [               --- life-is-NOT-fair ---                     ]
static void windows_stage1 (void)
{
   WIN_t *w;
   int i;

   for (i = 0; i < GROUPSMAX; i++) {
      w = &Winstk[i];
      w->winnum = i + 1;
      w->rc = Rc.win[i];
      w->captab[0] = Cap_norm;
      w->captab[1] = Cap_norm;
      w->captab[2] = w->cap_bold;
      w->captab[3] = w->capclr_sum;
      w->captab[4] = w->capclr_msg;
      w->captab[5] = w->capclr_pmt;
      w->captab[6] = w->capclr_hdr;
      w->captab[7] = w->capclr_rowhigh;
      w->captab[8] = w->capclr_rownorm;
      w->next = w + 1;
      w->prev = w - 1;
      ++w;
   }
      /* fixup the circular chains... */
   Winstk[3].next = &Winstk[0];
   Winstk[0].prev = &Winstk[3];
   Curwin = Winstk;
}


// This guy just completes the field group windows after the
// rcfiles have been read and command line arguments parsed
static void windows_stage2 (void)
{
   int i;

   for (i = 0; i < GROUPSMAX; i++) {
      win_names(&Winstk[i], Winstk[i].rc.winname);
      capsmk(&Winstk[i]);
   }
   // rely on this next guy to force a call (eventually) to reframewins
   wins_resize();
}


/*######  Main Screen routines  ##########################################*/

// Process keyboard input during the main loop
static void do_key (unsigned c)
{
   // standardized 'secure mode' errors
   static const char err_secure[] = "\aUnavailable in secure mode";
   static const char err_num_cpus[] = "\aSorry, terminal is not big enough";
#ifdef WARN_NOT_SMP
   // standardized 'smp' errors
   static const char err_smp[] = "\aSorry, only 1 cpu detected";
#endif

   switch (c) {
      case '1':
         if (Cpu_tot+7 > Screen_rows && CHKw(Curwin, View_CPUSUM)) {
            show_msg(err_num_cpus);
            break;
         }
#ifdef WARN_NOT_SMP
         if (Cpu_tot > 1) TOGw(Curwin, View_CPUSUM);
         else show_msg(err_smp);
#else
         TOGw(Curwin, View_CPUSUM);
#endif
         break;

      case 'a':
         if (Rc.mode_altscr) Curwin = Curwin->next;
         break;

      case 'A':
         Rc.mode_altscr = !Rc.mode_altscr;
         wins_resize();
         break;

      case 'b':
         if (VIZCHKc) {
            if (!CHKw(Curwin, Show_HICOLS | Show_HIROWS))
               show_msg("\aNothing to highlight!");
            else {
               TOGw(Curwin, Show_HIBOLD);
               capsmk(Curwin);
            }
         }
         break;

      case 'B':
         TOGw(Curwin, View_NOBOLD);
         capsmk(Curwin);
         break;

      case 'c':
         VIZTOGc(Show_CMDLIN);
         break;

      case 'd':
      case 's':
         if (Secure_mode)
            show_msg(err_secure);
         else {
            float tmp =
               get_float(fmtmk("Change delay from %.1f to", Rc.delay_time));
            if (tmp > -1) Rc.delay_time = tmp;
         }
         break;

      case 'f':
         if (VIZCHKc) fields_toggle();
         break;

      case 'F':
      case 'O':
         if (VIZCHKc) fields_sort();
         break;

      case 'g':
         if (Rc.mode_altscr) {
            char tmp[GETBUFSIZ];
            strcpy(tmp, ask4str(fmtmk("Rename window '%s' to (1-3 chars)", Curwin->rc.winname)));
            if (tmp[0]) win_names(Curwin, tmp);
         }
         break;

      case 'G':
         win_select(0);
         break;

      case 'H':
         if (VIZCHKc) {
            TOGw(Curwin, Show_THREADS);
            show_msg(fmtmk("Show threads %s"
               , CHKw(Curwin, Show_THREADS) ? "On" : "Off"));
         }
         break;

      case 'h':
      case '?':
      {  char ch;
         putp(Cap_clr_scr);
         putp(Cap_curs_huge);
            /* this string is well above ISO C89's minimum requirements! */
         show_special(
            1,
            fmtmk(
               KEYS_help,
               procps_version,
               Curwin->grpname,
               CHKw(Curwin, Show_CTIMES) ? "On" : "Off",
               Rc.delay_time,
               Secure_mode ? "On" : "Off",
               Secure_mode ? "" : KEYS_help_unsecured
            )
         );
         chin(0, &ch, 1);
         if ('?' == ch || 'h' == ch) {
            do {
               putp(Cap_clr_scr);
               show_special(1, fmtmk(WINDOWS_help
                  , Curwin->grpname
                  , Winstk[0].rc.winname
                  , Winstk[1].rc.winname
                  , Winstk[2].rc.winname
                  , Winstk[3].rc.winname));
               chin(0, &ch, 1);
               win_select(ch);
            } while ('\n' != ch);
         }
         putp(Cap_curs_norm);
      }
         break;

      case 'i':
         VIZTOGc(Show_IDLEPS);
         break;

      case 'I':
#ifdef WARN_NOT_SMP
         if (Cpu_tot > 1) {
            Rc.mode_irixps = !Rc.mode_irixps;
            show_msg(fmtmk("Irix mode %s", Rc.mode_irixps ? "On" : "Off"));
         } else
            show_msg(err_smp);
#else
         Rc.mode_irixps = !Rc.mode_irixps;
         show_msg(fmtmk("Irix mode %s", Rc.mode_irixps ? "On" : "Off"));
#endif
         if(Rc.mode_irixps && smp_num_cpus>1){
            // good for 100 CPUs per process
            pcpu_max_value = 9999.0;
            Fieldstab[P_CPU].fmts = " %4.0f";
         } else {
            pcpu_max_value = 99.9;
            Fieldstab[P_CPU].fmts = " %#4.1f";
         }
         break;

      case 'k':
         if (Secure_mode) {
            show_msg(err_secure);
         } else {
            int sig, pid = get_int("PID to kill");
            if (pid > 0) {
               sig = signal_name_to_number(
                  ask4str(fmtmk("Kill PID %d with signal [%i]", pid, DEF_SIGNAL)));
               if (sig == -1) sig = DEF_SIGNAL;
               if (sig && kill(pid, sig))
                  show_msg(fmtmk("\aKill of PID '%d' with '%d' failed: %s", pid, sig, strerror(errno)));
            }
         }
         break;

      case 'l':
         TOGw(Curwin, View_LOADAV);
         break;

      case 'm':
         TOGw(Curwin, View_MEMORY);
         break;

      case 'n':
      case '#':
         if (VIZCHKc) {
            int num =
               get_int(fmtmk("Maximum tasks = %d, change to (0 is unlimited)", Curwin->rc.maxtasks));
            if (num > -1) Curwin->rc.maxtasks = num;
         }
         break;

      case 'o':
         if (VIZCHKc) fields_reorder();
         break;

      case 'q':
         end_pgm(0);

      case 'r':
         if (Secure_mode)
            show_msg(err_secure);
         else {
            int val, pid = get_int("PID to renice");
            if (pid > 0) {
               val = get_int(fmtmk("Renice PID %d to value", pid));
               if (setpriority(PRIO_PROCESS, (unsigned)pid, val))
                  show_msg(fmtmk("\aRenice of PID %d to %d failed: %s", pid, val, strerror(errno)));
            }
         }
         break;

      case 'R':
         VIZTOGc(Qsrt_NORMAL);
         break;

      case 'S':
         if (VIZCHKc) {
            TOGw(Curwin, Show_CTIMES);
            show_msg(fmtmk("Cumulative time %s", CHKw(Curwin, Show_CTIMES) ? "On" : "Off"));
         }
         break;

      case 't':
         TOGw(Curwin, View_STATES);
         break;

//    case 'u':
//       if (VIZCHKc)
//          strcpy(Curwin->colusrnam, ask4str("Which user (blank for all)"));
//       break;

      case 'u':
//       if (!VIZCHKc) break;
         do {
            const char *errmsg;
            const char *answer;
            answer = ask4str("Which user (blank for all)");
            // FIXME: do this better:
            if (!answer || *answer=='\0' || *answer=='\n' || *answer=='\r' || *answer=='\t' || *answer==' ') {
               selection_type = 0;
               selection_uid = -1;
               break;
            }
            errmsg = parse_uid(answer, &selection_uid);
            if (errmsg) {
               show_msg(errmsg);
               // Change settings here? I guess not.
               break;
            }
            selection_type = 'u';
         } while(0);
         break;

      case 'U':
//       if (!VIZCHKc) break;
         do {
            const char *errmsg;
            const char *answer;
            answer = ask4str("Which user (blank for all)");
            // FIXME: do this better:
            if (!answer || *answer=='\0' || *answer=='\n' || *answer=='\r' || *answer=='\t' || *answer==' ') {
               selection_type = 0;
               selection_uid = -1;
               break;
            }
            errmsg = parse_uid(answer, &selection_uid);
            if (errmsg) {
               show_msg(errmsg);
               // Change settings here? I guess not.
               break;
            }
            selection_type = 'U';
         } while(0);
         break;

      case 'w':
         if (Rc.mode_altscr) Curwin = Curwin->prev;
         break;

      case 'W':
      {  const char *err = rc_write_whatever();
         if (err)
            show_msg(fmtmk("\aFailed '%s' open: %s", Rc_name, err));
         else
            show_msg(fmtmk("Wrote configuration to '%s'", Rc_name));
      }
         break;

      case 'x':
         if (VIZCHKc) {
            TOGw(Curwin, Show_HICOLS);
            capsmk(Curwin);
         }
         break;

      case 'y':
         if (VIZCHKc) {
            TOGw(Curwin, Show_HIROWS);
            capsmk(Curwin);
         }
         break;

      case 'z':
         if (VIZCHKc) {
            TOGw(Curwin, Show_COLORS);
            capsmk(Curwin);
         }
         break;

      case 'Z':
         wins_colors();
         break;

      case '-':
         if (Rc.mode_altscr) TOGw(Curwin, VISIBLE_tsk);
         break;

      case '_':
         if (Rc.mode_altscr) wins_reflag(Flags_TOG, VISIBLE_tsk);
         break;

      case '=':
         Curwin->rc.maxtasks = 0;
         SETw(Curwin, Show_IDLEPS | VISIBLE_tsk);
         Monpidsidx = 0;
         selection_type = '\0';
         break;

      case '+':
         if (Rc.mode_altscr) SETw(Curwin, EQUWINS_cwo);
         break;

      case '<':
         if (VIZCHKc) {
            FLG_t *p = Curwin->procflags + Curwin->maxpflgs - 1;
            while (*p != Curwin->rc.sortindx) --p;
            if (--p >= Curwin->procflags)
               Curwin->rc.sortindx = *p;
         }
         break;

      case '>':
         if (VIZCHKc) {
            FLG_t *p = Curwin->procflags;
            while (*p != Curwin->rc.sortindx) ++p;
            if (++p < Curwin->procflags + Curwin->maxpflgs)
               Curwin->rc.sortindx = *p;
         }
         break;

      case 'M':         // these keys represent old-top compatability
      case 'N':         // -- grouped here so that if users could ever
      case 'P':         // be weaned, we would just whack this part...
      case 'T':
      {  static struct {
            const char     *xmsg;
            const unsigned  xkey;
            const FLG_t     sort;
         } xtab[] = {
            { "Memory", 'M', P_MEM, }, { "Numerical", 'N', P_PID, },
            { "CPU",    'P', P_CPU, }, { "Time",      'T', P_TM2  }, };
         int i;
         for (i = 0; i < MAXTBL(xtab); ++i)
            if (c == xtab[i].xkey) {
               Curwin->rc.sortindx = xtab[i].sort;
//               show_msg(fmtmk("%s sort compatibility key honored", xtab[i].xmsg));
               break;
            }
      }
         break;

      case '\n':        // just ignore these, they'll have the effect
      case ' ':         // of refreshing display after waking us up !
         break;

      default:
         show_msg("\aUnknown command - try 'h' for help");
   }
   // The following assignment will force a rebuild of all column headers and
   // the PROC_FILLxxx flags.  It's NOT simply lazy programming.  Here are
   // some keys that COULD require new column headers and/or libproc flags:
   //    'A' - likely
   //    'c' - likely when !Mode_altscr, maybe when Mode_altscr
   //    'F' - maybe, if new field forced on
   //    'f' - likely
   //    'G' - likely
   //    'O' - maybe, if new field forced on
   //    'o' - maybe, if new field brought into view
   //    'Z' - likely, if 'Curwin' changed when !Mode_altscr
   //    '-' - likely (restricted to Mode_altscr)
   //    '_' - likely (restricted to Mode_altscr)
   //    '=' - maybe, but only when Mode_altscr
   //    '+' - likely (restricted to Mode_altscr)
   // ( At this point we have a human being involved and so have all the time )
   // ( in the world.  We can afford a few extra cpu cycles every now & then! )
   Frames_libflags = 0;
}


// State display *Helper* function to calc and display the state
// percentages for a single cpu.  In this way, we can support
// the following environments without the usual code bloat.
//    1) single cpu machines
//    2) modest smp boxes with room for each cpu's percentages
//    3) massive smp guys leaving little or no room for process
//       display and thus requiring the cpu summary toggle
static void summaryhlp (CPU_t *cpu, const char *pfx)
{
   // we'll trim to zero if we get negative time ticks,
   // which has happened with some SMP kernels (pre-2.4?)
#define TRIMz(x)  ((tz = (SIC_t)(x)) < 0 ? 0 : tz)
   SIC_t u_frme, s_frme, n_frme, i_frme, w_frme, x_frme, y_frme, z_frme, tot_frme, tz;
   float scale;

   u_frme = cpu->u - cpu->u_sav;
   s_frme = cpu->s - cpu->s_sav;
   n_frme = cpu->n - cpu->n_sav;
   i_frme = TRIMz(cpu->i - cpu->i_sav);
   w_frme = cpu->w - cpu->w_sav;
   x_frme = cpu->x - cpu->x_sav;
   y_frme = cpu->y - cpu->y_sav;
   z_frme = cpu->z - cpu->z_sav;
   tot_frme = u_frme + s_frme + n_frme + i_frme + w_frme + x_frme + y_frme + z_frme;
   if (tot_frme < 1) tot_frme = 1;
   scale = 100.0 / (float)tot_frme;

   // display some kinda' cpu state percentages
   // (who or what is explained by the passed prefix)
   show_special(
      0,
      fmtmk(
         States_fmts,
         pfx,
         (float)u_frme * scale,
         (float)s_frme * scale,
         (float)n_frme * scale,
         (float)i_frme * scale,
         (float)w_frme * scale,
         (float)x_frme * scale,
         (float)y_frme * scale,
         (float)z_frme * scale
      )
   );
   Msg_row += 1;

   // remember for next time around
   cpu->u_sav = cpu->u;
   cpu->s_sav = cpu->s;
   cpu->n_sav = cpu->n;
   cpu->i_sav = cpu->i;
   cpu->w_sav = cpu->w;
   cpu->x_sav = cpu->x;
   cpu->y_sav = cpu->y;
   cpu->z_sav = cpu->z;

#undef TRIMz
}


// Begin a new frame by:
//    1) Refreshing the all important proc table
//    2) Displaying uptime and load average (maybe)
//    3) Displaying task/cpu states (maybe)
//    4) Displaying memory & swap usage (maybe)
// and then, returning a pointer to the pointers to the proc_t's!
static proc_t **summary_show (void)
{
   static proc_t **p_table = NULL;
   static CPU_t  *smpcpu = NULL;

   // whoa first time, gotta' prime the pump...
   if (!p_table) {
      p_table = procs_refresh(NULL, Frames_libflags);
      putp(Cap_clr_scr);
      putp(Cap_rmam);
#ifndef PROF
      // sleep for half a second
      tv.tv_sec = 0;
      tv.tv_usec = 500000;
      select(0, NULL, NULL, NULL, &tv);  // ought to loop until done
#endif
   } else {
      putp(Batch ? "\n\n" : Cap_home);
   }
   p_table = procs_refresh(p_table, Frames_libflags);

   // Display Uptime and Loadavg
   if (CHKw(Curwin, View_LOADAV)) {
      if (!Rc.mode_altscr) {
         show_special(0, fmtmk(LOADAV_line, Myname, sprint_uptime()));
      } else {
         show_special(
            0,
            fmtmk(
               CHKw(Curwin, VISIBLE_tsk) ? LOADAV_line_alt : LOADAV_line,
               Curwin->grpname,
               sprint_uptime()
            )
         );
      }
      Msg_row += 1;
   }

   // Display Task and Cpu(s) States
   if (CHKw(Curwin, View_STATES)) {
      show_special(
         0,
         fmtmk(
            STATES_line1,
            Frame_maxtask, Frame_running, Frame_sleepin, Frame_stopped, Frame_zombied
         )
      );
      Msg_row += 1;

      smpcpu = cpus_refresh(smpcpu);

      if (CHKw(Curwin, View_CPUSUM)) {
         // display just the 1st /proc/stat line
         summaryhlp(&smpcpu[Cpu_tot], "Cpu(s):");
      } else {
         int i;
         char tmp[SMLBUFSIZ];
         // display each cpu's states separately
         for (i = 0; i < Cpu_tot; i++) {
            snprintf(tmp, sizeof(tmp), "Cpu%-3d:", smpcpu[i].id);
            summaryhlp(&smpcpu[i], tmp);
         }
      }
   }

   // Display Memory and Swap stats
   meminfo();
   if (CHKw(Curwin, View_MEMORY)) {
      show_special(0, fmtmk(MEMORY_line1
         , kb_main_total, kb_main_used, kb_main_free, kb_main_buffers));
      show_special(0, fmtmk(MEMORY_line2
         , kb_swap_total, kb_swap_used, kb_swap_free, kb_main_cached));
      Msg_row += 2;
   }

   SETw(Curwin, NEWFRAM_cwo);
   return p_table;
}


#define PAGES_TO_KB(n)  (unsigned long)( (n) << page_to_kb_shift )

// the following macro is our means to 'inline' emitting a column -- next to
// procs_refresh, that's the most frequent and costly part of top's job !
#define MKCOL(va...) do {                                                    \
   if(likely(!(   CHKw(q, Show_HICOLS)  &&  q->rc.sortindx==i   ))) {        \
      snprintf(cbuf, sizeof(cbuf), f, ## va);                                \
   } else {                                                                  \
      snprintf(_z, sizeof(_z), f, ## va);                                    \
      snprintf(cbuf, sizeof(cbuf), "%s%s%s",                                 \
         q->capclr_rowhigh,                                                  \
         _z,                                                                 \
         !(CHKw(q, Show_HIROWS) && 'R' == p->state) ? q->capclr_rownorm : "" \
      );                                                                     \
      pad += q->len_rowhigh;                                                 \
      if (!(CHKw(q, Show_HIROWS) && 'R' == p->state)) pad += q->len_rownorm; \
   }                                                                         \
} while (0)

// Display information for a single task row.
static void task_show (const WIN_t *q, const proc_t *p)
{
   char rbuf[ROWBUFSIZ];
   char *rp = rbuf;
   int j, x, pad;

   *rp = '\0';

   pad = Rc.mode_altscr;
//   if (pad) rp = scat(rp, " ");

   for (x = 0; x < q->maxpflgs; x++) {
      char cbuf[ROWBUFSIZ], _z[ROWBUFSIZ];
      FLG_t       i = q->procflags[x];          // support for our field/column
      const char *f = Fieldstab[i].fmts;        // macro AND sometimes the fmt
      unsigned    s = Fieldstab[i].scale;       // string must be altered !
      unsigned    w = Fieldstab[i].width;

      int advance = (x==0) && !Rc.mode_altscr;

      switch (i) {
         case P_CMD:
         {  char tmp[ROWBUFSIZ];
            unsigned flags;
	    int maxcmd = q->maxcmdln;
            if (CHKw(q, Show_CMDLIN)) flags = ESC_DEFUNCT | ESC_BRACKETS | ESC_ARGS;
            else                      flags = ESC_DEFUNCT;
            escape_command(tmp, p, sizeof tmp, &maxcmd, flags);
            MKCOL(q->maxcmdln, q->maxcmdln, tmp);
         }
            break;
         case P_COD:
            MKCOL(scale_num(PAGES_TO_KB(p->trs), w, s));
            break;
         case P_CPN:
            MKCOL((unsigned)p->processor);
            break;
         case P_CPU:
         {  float u = (float)p->pcpu * Frame_tscale;
            if (u > pcpu_max_value) u = pcpu_max_value;
            MKCOL(u);
         }
            break;
         case P_DAT:
            MKCOL(scale_num(PAGES_TO_KB(p->drs), w, s));
            break;
         case P_DRT:
            MKCOL(scale_num((unsigned)p->dt, w, s));
            break;
         case P_FLG:
         {  char tmp[TNYBUFSIZ];
            snprintf(tmp, sizeof(tmp), f, (long)p->flags);
            for (j = 0; tmp[j]; j++) if ('0' == tmp[j]) tmp[j] = '.';
            f = tmp;
            MKCOL("");
         }
            break;
         case P_FLT:
            MKCOL(scale_num(p->maj_flt, w, s));
            break;
         case P_GRP:
            MKCOL(p->egroup);
            break;
         case P_MEM:
            MKCOL((float)PAGES_TO_KB(p->resident) * 100 / kb_main_total);
            break;
         case P_NCE:
            MKCOL((int)p->nice);
            break;
         case P_PID:
            MKCOL((unsigned)p->XXXID);
            break;
         case P_PPD:
            MKCOL((unsigned)p->ppid);
            break;
         case P_PRI:
            if (unlikely(-99 > p->priority) || unlikely(999 < p->priority)) {
               f = "  RT";
               MKCOL("");
            } else
               MKCOL((int)p->priority);
            break;
         case P_RES:
            MKCOL(scale_num(PAGES_TO_KB(p->resident), w, s));
            break;
         case P_SHR:
            MKCOL(scale_num(PAGES_TO_KB(p->share), w, s));
            break;
         case P_STA:
            MKCOL(p->state);
            break;
         case P_SWP:
            MKCOL(scale_num(PAGES_TO_KB(p->size - p->resident), w, s));
            break;
         case P_TME:
         case P_TM2:
         {  TIC_t t = p->utime + p->stime;
            if (CHKw(q, Show_CTIMES))
               t += (p->cutime + p->cstime);
            MKCOL(scale_tics(t, w));
         }
            break;
         case P_TTY:
         {  char tmp[TNYBUFSIZ];
            dev_to_tty(tmp, (int)w, p->tty, p->XXXID, ABBREV_DEV);
            MKCOL(tmp);
         }
            break;
         case P_UID:
            MKCOL((unsigned)p->euid);
            break;
         case P_URE:
            MKCOL(p->euser);
            break;
         case P_URR:
            MKCOL(p->ruser);
            break;
         case P_VRT:
            MKCOL(scale_num(PAGES_TO_KB(p->size), w, s));
            break;
         case P_WCH:
            if (No_ksyms) {
               f = " %08lx ";
               MKCOL((long)p->wchan);
            } else {
               MKCOL(lookup_wchan(p->wchan, p->XXXID));
            }
            break;

        } /* end: switch 'procflag' */

        rp = scat(rp, cbuf+advance);
   } /* end: for 'maxpflgs' */

   PUFF(
      "\n%s%.*s%s%s",
      (CHKw(q, Show_HIROWS) && 'R' == p->state) ? q->capclr_rowhigh : q->capclr_rownorm,
      Screen_cols + pad,
      rbuf,
      Caps_off,
      "" /*Cap_clr_eol*/
   );

#undef MKCOL
}


// Squeeze as many tasks as we can into a single window,
// after sorting the passed proc table.
static void window_show (proc_t **ppt, WIN_t *q, int *lscr)
{
#ifdef SORT_SUPRESS
   // the 1 flag that DOES and 2 flags that MAY impact our proc table qsort
#define srtMASK  ~( Qsrt_NORMAL | Show_CMDLIN | Show_CTIMES )
   static FLG_t sav_indx = 0;
   static int   sav_flgs = -1;
#endif
   int i, lwin;

   // Display Column Headings -- and distract 'em while we sort (maybe)
   PUFF("\n%s%s%s%s", q->capclr_hdr, q->columnhdr, Caps_off, Cap_clr_eol);

#ifdef SORT_SUPRESS
   if (CHKw(Curwin, NEWFRAM_cwo)
   || sav_indx != q->rc.sortindx
   || sav_flgs != (q->rc.winflags & srtMASK)) {
      sav_indx = q->rc.sortindx;
      sav_flgs = (q->rc.winflags & srtMASK);
#endif
      if (CHKw(q, Qsrt_NORMAL)) Frame_srtflg = 1; // this one's always needed!
      else                      Frame_srtflg = -1;
      Frame_ctimes = CHKw(q, Show_CTIMES);        // this and next, only maybe
      Frame_cmdlin = CHKw(q, Show_CMDLIN);
      qsort(ppt, Frame_maxtask, sizeof(proc_t *), Fieldstab[q->rc.sortindx].sort);
#ifdef SORT_SUPRESS
   }
#endif
   // account for column headings
   (*lscr)++;
   lwin = 1;
   i = 0;

   while ( ppt[i]->tid != -1 && *lscr < Max_lines  &&  (!q->winlines || (lwin <= q->winlines)) ) {
      if ((CHKw(q, Show_IDLEPS) || ('S' != ppt[i]->state && 'Z' != ppt[i]->state && 'T' != ppt[i]->state))
      && good_uid(ppt[i]) ) {
         // Display a process Row
         task_show(q, ppt[i]);
         (*lscr)++;
         ++lwin;
      }
      ++i;
   }
   // for this frame that window's toast, cleanup for next time
   q->winlines = 0;
   OFFw(Curwin, FLGSOFF_cwo);

#ifdef SORT_SUPRESS
#undef srtMASK
#endif
}


/*######  Entry point plus two  ##########################################*/

// This guy's just a *Helper* function who apportions the
// remaining amount of screen real estate under multiple windows
static void framehlp (int wix, int max)
{
   int i, rsvd, size, wins;

   // calc remaining number of visible windows + total 'user' lines
   for (i = wix, rsvd = 0, wins = 0; i < GROUPSMAX; i++) {
      if (CHKw(&Winstk[i], VISIBLE_tsk)) {
         rsvd += Winstk[i].rc.maxtasks;
         ++wins;
         if (max <= rsvd) break;
      }
   }
   if (!wins) wins = 1;
   // set aside 'rsvd' & deduct 1 line/window for the columns heading
   size = (max - wins) - rsvd;
   if (0 <= size) size = max;
   size = (max - wins) / wins;

   // for remaining windows, set WIN_t winlines to either the user's
   // maxtask (1st choice) or our 'foxized' size calculation
   // (foxized  adj. -  'fair and balanced')
   for (i = wix ; i < GROUPSMAX; i++) {
      if (CHKw(&Winstk[i], VISIBLE_tsk)) {
         Winstk[i].winlines =
            Winstk[i].rc.maxtasks ? Winstk[i].rc.maxtasks : size;
      }
   }
}


// Initiate the Frame Display Update cycle at someone's whim!
// This routine doesn't do much, mostly he just calls others.
//
// (Whoa, wait a minute, we DO caretake those row guys, plus)
// (we CALCULATE that IMPORTANT Max_lines thingy so that the)
// (*subordinate* functions invoked know WHEN the user's had)
// (ENOUGH already.  And at Frame End, it SHOULD be apparent)
// (WE am d'MAN -- clearing UNUSED screen LINES and ensuring)
// (the CURSOR is STUCK in just the RIGHT place, know what I)
// (mean?  Huh, "doesn't DO MUCH"!  Never, EVER think or say)
// (THAT about THIS function again, Ok?  Good that's better.)
static void frame_make (void)
{
   proc_t **ppt;
   int i, scrlins;

   // note: all libproc flags are managed by
   //       reframewins(), who also builds each window's column headers
   if (!Frames_libflags) {
      reframewins();
      memset(Pseudo_scrn, '\0', Pseudo_size);
   }
   Pseudo_row = Msg_row = scrlins = 0;
   ppt = summary_show();
   Max_lines = (Screen_rows - Msg_row) - 1;

   if (CHKw(Curwin, EQUWINS_cwo))
      wins_reflag(Flags_OFF, EQUWINS_cwo);

   // sure hope each window's columns header begins with a newline...
   putp(tg2(0, Msg_row));

   if (!Rc.mode_altscr) {
      // only 1 window to show so, piece o' cake
      Curwin->winlines = Curwin->rc.maxtasks;
      window_show(ppt, Curwin, &scrlins);
   } else {
      // maybe NO window is visible but assume, pieces o' cakes
      for (i = 0 ; i < GROUPSMAX; i++) {
         if (CHKw(&Winstk[i], VISIBLE_tsk)) {
            framehlp(i, Max_lines - scrlins);
            window_show(ppt, &Winstk[i], &scrlins);
         }
         if (Max_lines <= scrlins) break;
      }
   }
   // clear to end-of-screen (critical if last window is 'idleps off'),
   // then put the cursor in-its-place, and rid us of any prior frame's msg
   // (main loop must iterate such that we're always called before sleep)
   PUTT(
      "%s%s%s%s",
      scrlins < Max_lines ? "\n"        : "",
      scrlins < Max_lines ? Cap_clr_eos : "",
      tg2(0, Msg_row),
      Cap_clr_eol
   );
   fflush(stdout);
}


int main (int dont_care_argc, char *argv[])
{
   (void)dont_care_argc;
   before(*argv);
                                        //                 +-------------+
   windows_stage1();                    //                 top (sic) slice
   configs_read();                      //                 > spread etc, <
   parse_args(&argv[1]);                //                 > lean stuff, <
   whack_terminal();                    //                 > onions etc. <
   windows_stage2();                    //                 as bottom slice
                                        //                 +-------------+
   signal(SIGALRM,  end_pgm);
   signal(SIGHUP,   end_pgm);
   signal(SIGINT,   end_pgm);
   signal(SIGPIPE,  end_pgm);
   signal(SIGQUIT,  end_pgm);
   signal(SIGTERM,  end_pgm);
   signal(SIGTSTP,  suspend);
   signal(SIGTTIN,  suspend);
   signal(SIGTTOU,  suspend);
   signal(SIGCONT,  wins_resize_sighandler);
   signal(SIGWINCH, wins_resize_sighandler);

   for (;;) {
      if (need_resize){
         need_resize = 0;
         wins_resize();
      }
      frame_make();

      if (Msg_awaiting) show_msg(Msg_delayed);
      if (Loops > 0) --Loops;
      if (!Loops) end_pgm(0);

      tv.tv_sec = Rc.delay_time;
      tv.tv_usec = (Rc.delay_time - (int)Rc.delay_time) * 1000000;

      if (Batch) {
         select(0, NULL, NULL, NULL, &tv);  // ought to loop until done
      } else {
         long file_flags;
         int rc;
         char c;
         fd_set fs;
         FD_ZERO(&fs);
         FD_SET(STDIN_FILENO, &fs);
         file_flags = fcntl(STDIN_FILENO, F_GETFL);
         if(file_flags==-1) file_flags=0;
         fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK|file_flags);

         // check 1st, in case tv zeroed (by sig handler) before it got set
         rc = chin(0, &c, 1);
         if (rc <= 0) {
            // EOF is pretty much a "can't happen" except for a kernel bug.
            // We should quickly die via SIGHUP, and thus not spin here.
            // if (rc == 0) end_pgm(0); /* EOF from terminal */
            fcntl(STDIN_FILENO, F_SETFL, file_flags);
            select(1, &fs, NULL, NULL, &tv);
            fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK|file_flags);
         }
         if (chin(0, &c, 1) > 0) {
            fcntl(STDIN_FILENO, F_SETFL, file_flags);
            do_key((unsigned)c);
         } else {
            fcntl(STDIN_FILENO, F_SETFL, file_flags);
         }
      }
   }

   return 0;
}
