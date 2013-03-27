/* Variables that describe the inferior process running under GDB:
   Where it is, why it stopped, and how to step it.

   Copyright (C) 1986, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996,
   1998, 1999, 2000, 2001, 2003, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#if !defined (INFERIOR_H)
#define INFERIOR_H 1

struct target_waitstatus;
struct frame_info;
struct ui_file;
struct type;
struct gdbarch;
struct regcache;

/* For bpstat.  */
#include "breakpoint.h"

/* For enum target_signal.  */
#include "target.h"

/* For struct frame_id.  */
#include "frame.h"

/* Structure in which to save the status of the inferior.  Create/Save
   through "save_inferior_status", restore through
   "restore_inferior_status".

   This pair of routines should be called around any transfer of
   control to the inferior which you don't want showing up in your
   control variables.  */

struct inferior_status;

extern struct inferior_status *save_inferior_status (int);

extern void restore_inferior_status (struct inferior_status *);

extern struct cleanup *make_cleanup_restore_inferior_status (struct inferior_status *);

extern void discard_inferior_status (struct inferior_status *);

extern void write_inferior_status_register (struct inferior_status
					    *inf_status, int regno,
					    LONGEST val);

/* The -1 ptid, often used to indicate either an error condition
   or a "don't care" condition, i.e, "run all threads."  */
extern ptid_t minus_one_ptid;

/* The null or zero ptid, often used to indicate no process. */
extern ptid_t null_ptid;

/* Attempt to find and return an existing ptid with the given PID, LWP,
   and TID components.  If none exists, create a new one and return
   that.  */
ptid_t ptid_build (int pid, long lwp, long tid);

/* Find/Create a ptid from just a pid. */
ptid_t pid_to_ptid (int pid);

/* Fetch the pid (process id) component from a ptid. */
int ptid_get_pid (ptid_t ptid);

/* Fetch the lwp (lightweight process) component from a ptid. */
long ptid_get_lwp (ptid_t ptid);

/* Fetch the tid (thread id) component from a ptid. */
long ptid_get_tid (ptid_t ptid);

/* Compare two ptids to see if they are equal */
extern int ptid_equal (ptid_t p1, ptid_t p2);

/* Save value of inferior_ptid so that it may be restored by
   a later call to do_cleanups().  Returns the struct cleanup
   pointer needed for later doing the cleanup.  */
extern struct cleanup * save_inferior_ptid (void);

extern void set_sigint_trap (void);

extern void clear_sigint_trap (void);

extern void set_sigio_trap (void);

extern void clear_sigio_trap (void);

/* Set/get file name for default use for standard in/out in the inferior.  */

extern void set_inferior_io_terminal (const char *terminal_name);
extern const char *get_inferior_io_terminal (void);

/* Collected pid, tid, etc. of the debugged inferior.  When there's
   no inferior, PIDGET (inferior_ptid) will be 0. */

extern ptid_t inferior_ptid;

/* Is the inferior running right now, as a result of a 'run&',
   'continue&' etc command? This is used in asycn gdb to determine
   whether a command that the user enters while the target is running
   is allowed or not. */
extern int target_executing;

/* Are we simulating synchronous execution? This is used in async gdb
   to implement the 'run', 'continue' etc commands, which will not
   redisplay the prompt until the execution is actually over. */
extern int sync_execution;

/* Some targets (stupidly) report more than one exec event per actual
   call to an event() system call.  If only the last such exec event
   need actually be noticed and responded to by the debugger (i.e.,
   be reported to the user), then this is the number of "leading"
   exec events which should be ignored.
 */
extern int inferior_ignoring_leading_exec_events;

/* Inferior environment. */

extern struct gdb_environ *inferior_environ;

extern void clear_proceed_status (void);

extern void proceed (CORE_ADDR, enum target_signal, int);

/* When set, stop the 'step' command if we enter a function which has
   no line number information.  The normal behavior is that we step
   over such function.  */
extern int step_stop_if_no_debug;

extern void generic_mourn_inferior (void);

extern void terminal_save_ours (void);

extern void terminal_ours (void);

extern CORE_ADDR read_pc (void);

extern CORE_ADDR read_pc_pid (ptid_t);

extern void write_pc (CORE_ADDR);

extern void write_pc_pid (CORE_ADDR, ptid_t);

extern CORE_ADDR unsigned_pointer_to_address (struct type *type,
					      const gdb_byte *buf);
extern void unsigned_address_to_pointer (struct type *type, gdb_byte *buf,
					 CORE_ADDR addr);
extern CORE_ADDR signed_pointer_to_address (struct type *type,
					    const gdb_byte *buf);
extern void address_to_signed_pointer (struct type *type, gdb_byte *buf,
				       CORE_ADDR addr);

extern void wait_for_inferior (void);

extern void fetch_inferior_event (void *);

extern void init_wait_for_inferior (void);

extern void close_exec_file (void);

extern void reopen_exec_file (void);

/* The `resume' routine should only be called in special circumstances.
   Normally, use `proceed', which handles a lot of bookkeeping.  */

extern void resume (int, enum target_signal);

/* From misc files */

extern void default_print_registers_info (struct gdbarch *gdbarch,
					  struct ui_file *file,
					  struct frame_info *frame,
					  int regnum, int all);

extern void child_terminal_info (char *, int);

extern void term_info (char *, int);

extern void terminal_ours_for_output (void);

extern void terminal_inferior (void);

extern void terminal_init_inferior (void);

extern void terminal_init_inferior_with_pgrp (int pgrp);

/* From procfs.c */

extern int proc_iterate_over_mappings (int (*)(int, CORE_ADDR));

extern ptid_t procfs_first_available (void);

/* From fork-child.c */

extern void fork_inferior (char *, char *, char **,
			   void (*)(void),
			   void (*)(int), void (*)(void), char *);


extern void startup_inferior (int);

extern char *construct_inferior_arguments (struct gdbarch *, int, char **);

/* From inflow.c */

extern void new_tty_prefork (const char *);

extern int gdb_has_a_terminal (void);

/* From infrun.c */

extern void start_remote (int from_tty);

extern void normal_stop (void);

extern int signal_stop_state (int);

extern int signal_print_state (int);

extern int signal_pass_state (int);

extern int signal_stop_update (int, int);

extern int signal_print_update (int, int);

extern int signal_pass_update (int, int);

extern void get_last_target_status(ptid_t *ptid,
                                   struct target_waitstatus *status);

extern void follow_inferior_reset_breakpoints (void);

/* From infcmd.c */

extern void tty_command (char *, int);

extern void post_create_inferior (struct target_ops *, int);

extern void attach_command (char *, int);

extern char *get_inferior_args (void);

extern char *set_inferior_args (char *);

extern void set_inferior_args_vector (int, char **);

extern void registers_info (char *, int);

extern void nexti_command (char *, int);

extern void stepi_command (char *, int);

extern void continue_command (char *, int);

extern void interrupt_target_command (char *args, int from_tty);

/* Last signal that the inferior received (why it stopped).  */

extern enum target_signal stop_signal;

/* Address at which inferior stopped.  */

extern CORE_ADDR stop_pc;

/* Chain containing status of breakpoint(s) that we have stopped at.  */

extern bpstat stop_bpstat;

/* Flag indicating that a command has proceeded the inferior past the
   current breakpoint.  */

extern int breakpoint_proceeded;

/* Nonzero if stopped due to a step command.  */

extern int stop_step;

/* Nonzero if stopped due to completion of a stack dummy routine.  */

extern int stop_stack_dummy;

/* Nonzero if program stopped due to a random (unexpected) signal in
   inferior process.  */

extern int stopped_by_random_signal;

/* Range to single step within.
   If this is nonzero, respond to a single-step signal
   by continuing to step if the pc is in this range.

   If step_range_start and step_range_end are both 1, it means to step for
   a single instruction (FIXME: it might clean up wait_for_inferior in a
   minor way if this were changed to the address of the instruction and
   that address plus one.  But maybe not.).  */

extern CORE_ADDR step_range_start;	/* Inclusive */
extern CORE_ADDR step_range_end;	/* Exclusive */

/* Stack frame address as of when stepping command was issued.
   This is how we know when we step into a subroutine call,
   and how to set the frame for the breakpoint used to step out.  */

extern struct frame_id step_frame_id;

/* 1 means step over all subroutine calls.
   -1 means step over calls to undebuggable functions.  */

enum step_over_calls_kind
  {
    STEP_OVER_NONE,
    STEP_OVER_ALL,
    STEP_OVER_UNDEBUGGABLE
  };

extern enum step_over_calls_kind step_over_calls;

/* If stepping, nonzero means step count is > 1
   so don't print frame next time inferior stops
   if it stops due to stepping.  */

extern int step_multi;

/* Anything but NO_STOP_QUIETLY means we expect a trap and the caller
   will handle it themselves.  STOP_QUIETLY is used when running in
   the shell before the child program has been exec'd and when running
   through shared library loading.  STOP_QUIETLY_REMOTE is used when
   setting up a remote connection; it is like STOP_QUIETLY_NO_SIGSTOP
   except that there is no need to hide a signal.  */

/* It is also used after attach, due to attaching to a process. This
   is a bit trickier.  When doing an attach, the kernel stops the
   debuggee with a SIGSTOP.  On newer GNU/Linux kernels (>= 2.5.61)
   the handling of SIGSTOP for a ptraced process has changed. Earlier
   versions of the kernel would ignore these SIGSTOPs, while now
   SIGSTOP is treated like any other signal, i.e. it is not muffled.
   
   If the gdb user does a 'continue' after the 'attach', gdb passes
   the global variable stop_signal (which stores the signal from the
   attach, SIGSTOP) to the ptrace(PTRACE_CONT,...)  call.  This is
   problematic, because the kernel doesn't ignore such SIGSTOP
   now. I.e. it is reported back to gdb, which in turn presents it
   back to the user.
 
   To avoid the problem, we use STOP_QUIETLY_NO_SIGSTOP, which allows
   gdb to clear the value of stop_signal after the attach, so that it
   is not passed back down to the kernel.  */

enum stop_kind
  {
    NO_STOP_QUIETLY = 0,
    STOP_QUIETLY,
    STOP_QUIETLY_REMOTE,
    STOP_QUIETLY_NO_SIGSTOP
  };

extern enum stop_kind stop_soon;

/* Nonzero if proceed is being used for a "finish" command or a similar
   situation when stop_registers should be saved.  */

extern int proceed_to_finish;

/* Save register contents here when about to pop a stack dummy frame,
   if-and-only-if proceed_to_finish is set.
   Thus this contains the return value from the called function (assuming
   values are returned in a register).  */

extern struct regcache *stop_registers;

/* Nonzero if the child process in inferior_ptid was attached rather
   than forked.  */

extern int attach_flag;

/* Possible values for gdbarch_call_dummy_location.  */
#define ON_STACK 1
#define AT_ENTRY_POINT 4
#define AT_SYMBOL 5

/* If STARTUP_WITH_SHELL is set, GDB's "run"
   will attempts to start up the debugee under a shell.
   This is in order for argument-expansion to occur. E.g.,
   (gdb) run *
   The "*" gets expanded by the shell into a list of files.
   While this is a nice feature, it turns out to interact badly
   with some of the catch-fork/catch-exec features we have added.
   In particular, if the shell does any fork/exec's before
   the exec of the target program, that can confuse GDB.
   To disable this feature, set STARTUP_WITH_SHELL to 0.
   To enable this feature, set STARTUP_WITH_SHELL to 1.
   The catch-exec traps expected during start-up will
   be 1 if target is not started up with a shell, 2 if it is.
   - RT
   If you disable this, you need to decrement
   START_INFERIOR_TRAPS_EXPECTED in tm.h. */
#define STARTUP_WITH_SHELL 1
#if !defined(START_INFERIOR_TRAPS_EXPECTED)
#define START_INFERIOR_TRAPS_EXPECTED	2
#endif
#endif /* !defined (INFERIOR_H) */
