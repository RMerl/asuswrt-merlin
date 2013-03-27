/* Interface between GDB and target environments, including files and processes

   Copyright (C) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.

   Contributed by Cygnus Support.  Written by John Gilmore.

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

#if !defined (TARGET_H)
#define TARGET_H

struct objfile;
struct ui_file;
struct mem_attrib;
struct target_ops;
struct bp_target_info;
struct regcache;

/* This include file defines the interface between the main part
   of the debugger, and the part which is target-specific, or
   specific to the communications interface between us and the
   target.

   A TARGET is an interface between the debugger and a particular
   kind of file or process.  Targets can be STACKED in STRATA,
   so that more than one target can potentially respond to a request.
   In particular, memory accesses will walk down the stack of targets
   until they find a target that is interested in handling that particular
   address.  STRATA are artificial boundaries on the stack, within
   which particular kinds of targets live.  Strata exist so that
   people don't get confused by pushing e.g. a process target and then
   a file target, and wondering why they can't see the current values
   of variables any more (the file target is handling them and they
   never get to the process target).  So when you push a file target,
   it goes into the file stratum, which is always below the process
   stratum.  */

#include "bfd.h"
#include "symtab.h"
#include "dcache.h"
#include "memattr.h"
#include "vec.h"

enum strata
  {
    dummy_stratum,		/* The lowest of the low */
    file_stratum,		/* Executable files, etc */
    core_stratum,		/* Core dump files */
    download_stratum,		/* Downloading of remote targets */
    process_stratum,		/* Executing processes */
    thread_stratum		/* Executing threads */
  };

enum thread_control_capabilities
  {
    tc_none = 0,		/* Default: can't control thread execution.  */
    tc_schedlock = 1,		/* Can lock the thread scheduler.  */
    tc_switch = 2		/* Can switch the running thread on demand.  */
  };

/* Stuff for target_wait.  */

/* Generally, what has the program done?  */
enum target_waitkind
  {
    /* The program has exited.  The exit status is in value.integer.  */
    TARGET_WAITKIND_EXITED,

    /* The program has stopped with a signal.  Which signal is in
       value.sig.  */
    TARGET_WAITKIND_STOPPED,

    /* The program has terminated with a signal.  Which signal is in
       value.sig.  */
    TARGET_WAITKIND_SIGNALLED,

    /* The program is letting us know that it dynamically loaded something
       (e.g. it called load(2) on AIX).  */
    TARGET_WAITKIND_LOADED,

    /* The program has forked.  A "related" process' ID is in
       value.related_pid.  I.e., if the child forks, value.related_pid
       is the parent's ID.  */

    TARGET_WAITKIND_FORKED,

    /* The program has vforked.  A "related" process's ID is in
       value.related_pid.  */

    TARGET_WAITKIND_VFORKED,

    /* The program has exec'ed a new executable file.  The new file's
       pathname is pointed to by value.execd_pathname.  */

    TARGET_WAITKIND_EXECD,

    /* The program has entered or returned from a system call.  On
       HP-UX, this is used in the hardware watchpoint implementation.
       The syscall's unique integer ID number is in value.syscall_id */

    TARGET_WAITKIND_SYSCALL_ENTRY,
    TARGET_WAITKIND_SYSCALL_RETURN,

    /* Nothing happened, but we stopped anyway.  This perhaps should be handled
       within target_wait, but I'm not sure target_wait should be resuming the
       inferior.  */
    TARGET_WAITKIND_SPURIOUS,

    /* An event has occured, but we should wait again.
       Remote_async_wait() returns this when there is an event
       on the inferior, but the rest of the world is not interested in
       it. The inferior has not stopped, but has just sent some output
       to the console, for instance. In this case, we want to go back
       to the event loop and wait there for another event from the
       inferior, rather than being stuck in the remote_async_wait()
       function. This way the event loop is responsive to other events,
       like for instance the user typing.  */
    TARGET_WAITKIND_IGNORE
  };

struct target_waitstatus
  {
    enum target_waitkind kind;

    /* Forked child pid, execd pathname, exit status or signal number.  */
    union
      {
	int integer;
	enum target_signal sig;
	int related_pid;
	char *execd_pathname;
	int syscall_id;
      }
    value;
  };

/* Possible types of events that the inferior handler will have to
   deal with.  */
enum inferior_event_type
  {
    /* There is a request to quit the inferior, abandon it.  */
    INF_QUIT_REQ,
    /* Process a normal inferior event which will result in target_wait
       being called.  */
    INF_REG_EVENT,
    /* Deal with an error on the inferior.  */
    INF_ERROR,
    /* We are called because a timer went off.  */
    INF_TIMER,
    /* We are called to do stuff after the inferior stops.  */
    INF_EXEC_COMPLETE,
    /* We are called to do some stuff after the inferior stops, but we
       are expected to reenter the proceed() and
       handle_inferior_event() functions. This is used only in case of
       'step n' like commands.  */
    INF_EXEC_CONTINUE
  };

/* Return the string for a signal.  */
extern char *target_signal_to_string (enum target_signal);

/* Return the name (SIGHUP, etc.) for a signal.  */
extern char *target_signal_to_name (enum target_signal);

/* Given a name (SIGHUP, etc.), return its signal.  */
enum target_signal target_signal_from_name (char *);

/* Target objects which can be transfered using target_read,
   target_write, et cetera.  */

enum target_object
{
  /* AVR target specific transfer.  See "avr-tdep.c" and "remote.c".  */
  TARGET_OBJECT_AVR,
  /* SPU target specific transfer.  See "spu-tdep.c".  */
  TARGET_OBJECT_SPU,
  /* Transfer up-to LEN bytes of memory starting at OFFSET.  */
  TARGET_OBJECT_MEMORY,
  /* Memory, avoiding GDB's data cache and trusting the executable.
     Target implementations of to_xfer_partial never need to handle
     this object, and most callers should not use it.  */
  TARGET_OBJECT_RAW_MEMORY,
  /* Kernel Unwind Table.  See "ia64-tdep.c".  */
  TARGET_OBJECT_UNWIND_TABLE,
  /* Transfer auxilliary vector.  */
  TARGET_OBJECT_AUXV,
  /* StackGhost cookie.  See "sparc-tdep.c".  */
  TARGET_OBJECT_WCOOKIE,
  /* Target memory map in XML format.  */
  TARGET_OBJECT_MEMORY_MAP,
  /* Flash memory.  This object can be used to write contents to
     a previously erased flash memory.  Using it without erasing
     flash can have unexpected results.  Addresses are physical
     address on target, and not relative to flash start.  */
  TARGET_OBJECT_FLASH,
  /* Available target-specific features, e.g. registers and coprocessors.
     See "target-descriptions.c".  ANNEX should never be empty.  */
  TARGET_OBJECT_AVAILABLE_FEATURES,
  /* Currently loaded libraries, in XML format.  */
  TARGET_OBJECT_LIBRARIES
  /* Possible future objects: TARGET_OBJECT_FILE, TARGET_OBJECT_PROC, ... */
};

/* Request that OPS transfer up to LEN 8-bit bytes of the target's
   OBJECT.  The OFFSET, for a seekable object, specifies the
   starting point.  The ANNEX can be used to provide additional
   data-specific information to the target.

   Return the number of bytes actually transfered, or -1 if the
   transfer is not supported or otherwise fails.  Return of a positive
   value less than LEN indicates that no further transfer is possible.
   Unlike the raw to_xfer_partial interface, callers of these
   functions do not need to retry partial transfers.  */

extern LONGEST target_read (struct target_ops *ops,
			    enum target_object object,
			    const char *annex, gdb_byte *buf,
			    ULONGEST offset, LONGEST len);

extern LONGEST target_write (struct target_ops *ops,
			     enum target_object object,
			     const char *annex, const gdb_byte *buf,
			     ULONGEST offset, LONGEST len);

/* Similar to target_write, except that it also calls PROGRESS with
   the number of bytes written and the opaque BATON after every
   successful partial write (and before the first write).  This is
   useful for progress reporting and user interaction while writing
   data.  To abort the transfer, the progress callback can throw an
   exception.  */

LONGEST target_write_with_progress (struct target_ops *ops,
				    enum target_object object,
				    const char *annex, const gdb_byte *buf,
				    ULONGEST offset, LONGEST len,
				    void (*progress) (ULONGEST, void *),
				    void *baton);

/* Wrapper to perform a full read of unknown size.  OBJECT/ANNEX will
   be read using OPS.  The return value will be -1 if the transfer
   fails or is not supported; 0 if the object is empty; or the length
   of the object otherwise.  If a positive value is returned, a
   sufficiently large buffer will be allocated using xmalloc and
   returned in *BUF_P containing the contents of the object.

   This method should be used for objects sufficiently small to store
   in a single xmalloc'd buffer, when no fixed bound on the object's
   size is known in advance.  Don't try to read TARGET_OBJECT_MEMORY
   through this function.  */

extern LONGEST target_read_alloc (struct target_ops *ops,
				  enum target_object object,
				  const char *annex, gdb_byte **buf_p);

/* Read OBJECT/ANNEX using OPS.  The result is NUL-terminated and
   returned as a string, allocated using xmalloc.  If an error occurs
   or the transfer is unsupported, NULL is returned.  Empty objects
   are returned as allocated but empty strings.  A warning is issued
   if the result contains any embedded NUL bytes.  */

extern char *target_read_stralloc (struct target_ops *ops,
				   enum target_object object,
				   const char *annex);

/* Wrappers to target read/write that perform memory transfers.  They
   throw an error if the memory transfer fails.

   NOTE: cagney/2003-10-23: The naming schema is lifted from
   "frame.h".  The parameter order is lifted from get_frame_memory,
   which in turn lifted it from read_memory.  */

extern void get_target_memory (struct target_ops *ops, CORE_ADDR addr,
			       gdb_byte *buf, LONGEST len);
extern ULONGEST get_target_memory_unsigned (struct target_ops *ops,
					    CORE_ADDR addr, int len);


/* If certain kinds of activity happen, target_wait should perform
   callbacks.  */
/* Right now we just call (*TARGET_ACTIVITY_FUNCTION) if I/O is possible
   on TARGET_ACTIVITY_FD.  */
extern int target_activity_fd;
/* Returns zero to leave the inferior alone, one to interrupt it.  */
extern int (*target_activity_function) (void);

struct thread_info;		/* fwd decl for parameter list below: */

struct target_ops
  {
    struct target_ops *beneath;	/* To the target under this one.  */
    char *to_shortname;		/* Name this target type */
    char *to_longname;		/* Name for printing */
    char *to_doc;		/* Documentation.  Does not include trailing
				   newline, and starts with a one-line descrip-
				   tion (probably similar to to_longname).  */
    /* Per-target scratch pad.  */
    void *to_data;
    /* The open routine takes the rest of the parameters from the
       command, and (if successful) pushes a new target onto the
       stack.  Targets should supply this routine, if only to provide
       an error message.  */
    void (*to_open) (char *, int);
    /* Old targets with a static target vector provide "to_close".
       New re-entrant targets provide "to_xclose" and that is expected
       to xfree everything (including the "struct target_ops").  */
    void (*to_xclose) (struct target_ops *targ, int quitting);
    void (*to_close) (int);
    void (*to_attach) (char *, int);
    void (*to_post_attach) (int);
    void (*to_detach) (char *, int);
    void (*to_disconnect) (struct target_ops *, char *, int);
    void (*to_resume) (ptid_t, int, enum target_signal);
    ptid_t (*to_wait) (ptid_t, struct target_waitstatus *);
    void (*to_fetch_registers) (struct regcache *, int);
    void (*to_store_registers) (struct regcache *, int);
    void (*to_prepare_to_store) (struct regcache *);

    /* Transfer LEN bytes of memory between GDB address MYADDR and
       target address MEMADDR.  If WRITE, transfer them to the target, else
       transfer them from the target.  TARGET is the target from which we
       get this function.

       Return value, N, is one of the following:

       0 means that we can't handle this.  If errno has been set, it is the
       error which prevented us from doing it (FIXME: What about bfd_error?).

       positive (call it N) means that we have transferred N bytes
       starting at MEMADDR.  We might be able to handle more bytes
       beyond this length, but no promises.

       negative (call its absolute value N) means that we cannot
       transfer right at MEMADDR, but we could transfer at least
       something at MEMADDR + N.

       NOTE: cagney/2004-10-01: This has been entirely superseeded by
       to_xfer_partial and inferior inheritance.  */

    int (*deprecated_xfer_memory) (CORE_ADDR memaddr, gdb_byte *myaddr,
				   int len, int write,
				   struct mem_attrib *attrib,
				   struct target_ops *target);

    void (*to_files_info) (struct target_ops *);
    int (*to_insert_breakpoint) (struct bp_target_info *);
    int (*to_remove_breakpoint) (struct bp_target_info *);
    int (*to_can_use_hw_breakpoint) (int, int, int);
    int (*to_insert_hw_breakpoint) (struct bp_target_info *);
    int (*to_remove_hw_breakpoint) (struct bp_target_info *);
    int (*to_remove_watchpoint) (CORE_ADDR, int, int);
    int (*to_insert_watchpoint) (CORE_ADDR, int, int);
    int (*to_stopped_by_watchpoint) (void);
    int to_have_steppable_watchpoint;
    int to_have_continuable_watchpoint;
    int (*to_stopped_data_address) (struct target_ops *, CORE_ADDR *);
    int (*to_region_ok_for_hw_watchpoint) (CORE_ADDR, int);
    void (*to_terminal_init) (void);
    void (*to_terminal_inferior) (void);
    void (*to_terminal_ours_for_output) (void);
    void (*to_terminal_ours) (void);
    void (*to_terminal_save_ours) (void);
    void (*to_terminal_info) (char *, int);
    void (*to_kill) (void);
    void (*to_load) (char *, int);
    int (*to_lookup_symbol) (char *, CORE_ADDR *);
    void (*to_create_inferior) (char *, char *, char **, int);
    void (*to_post_startup_inferior) (ptid_t);
    void (*to_acknowledge_created_inferior) (int);
    void (*to_insert_fork_catchpoint) (int);
    int (*to_remove_fork_catchpoint) (int);
    void (*to_insert_vfork_catchpoint) (int);
    int (*to_remove_vfork_catchpoint) (int);
    int (*to_follow_fork) (struct target_ops *, int);
    void (*to_insert_exec_catchpoint) (int);
    int (*to_remove_exec_catchpoint) (int);
    int (*to_reported_exec_events_per_exec_call) (void);
    int (*to_has_exited) (int, int, int *);
    void (*to_mourn_inferior) (void);
    int (*to_can_run) (void);
    void (*to_notice_signals) (ptid_t ptid);
    int (*to_thread_alive) (ptid_t ptid);
    void (*to_find_new_threads) (void);
    char *(*to_pid_to_str) (ptid_t);
    char *(*to_extra_thread_info) (struct thread_info *);
    void (*to_stop) (void);
    void (*to_rcmd) (char *command, struct ui_file *output);
    struct symtab_and_line *(*to_enable_exception_callback) (enum
							     exception_event_kind,
							     int);
    struct exception_event_record *(*to_get_current_exception_event) (void);
    char *(*to_pid_to_exec_file) (int pid);
    enum strata to_stratum;
    int to_has_all_memory;
    int to_has_memory;
    int to_has_stack;
    int to_has_registers;
    int to_has_execution;
    int to_has_thread_control;	/* control thread execution */
    struct section_table
     *to_sections;
    struct section_table
     *to_sections_end;
    /* ASYNC target controls */
    int (*to_can_async_p) (void);
    int (*to_is_async_p) (void);
    void (*to_async) (void (*cb) (enum inferior_event_type, void *context),
		      void *context);
    int to_async_mask_value;
    int (*to_find_memory_regions) (int (*) (CORE_ADDR,
					    unsigned long,
					    int, int, int,
					    void *),
				   void *);
    char * (*to_make_corefile_notes) (bfd *, int *);

    /* Return the thread-local address at OFFSET in the
       thread-local storage for the thread PTID and the shared library
       or executable file given by OBJFILE.  If that block of
       thread-local storage hasn't been allocated yet, this function
       may return an error.  */
    CORE_ADDR (*to_get_thread_local_address) (ptid_t ptid,
					      CORE_ADDR load_module_addr,
					      CORE_ADDR offset);

    /* Request that OPS transfer up to LEN 8-bit bytes of the target's
       OBJECT.  The OFFSET, for a seekable object, specifies the
       starting point.  The ANNEX can be used to provide additional
       data-specific information to the target.

       Return the number of bytes actually transfered, zero when no
       further transfer is possible, and -1 when the transfer is not
       supported.  Return of a positive value smaller than LEN does
       not indicate the end of the object, only the end of the
       transfer; higher level code should continue transferring if
       desired.  This is handled in target.c.

       The interface does not support a "retry" mechanism.  Instead it
       assumes that at least one byte will be transfered on each
       successful call.

       NOTE: cagney/2003-10-17: The current interface can lead to
       fragmented transfers.  Lower target levels should not implement
       hacks, such as enlarging the transfer, in an attempt to
       compensate for this.  Instead, the target stack should be
       extended so that it implements supply/collect methods and a
       look-aside object cache.  With that available, the lowest
       target can safely and freely "push" data up the stack.

       See target_read and target_write for more information.  One,
       and only one, of readbuf or writebuf must be non-NULL.  */

    LONGEST (*to_xfer_partial) (struct target_ops *ops,
				enum target_object object, const char *annex,
				gdb_byte *readbuf, const gdb_byte *writebuf,
				ULONGEST offset, LONGEST len);

    /* Returns the memory map for the target.  A return value of NULL
       means that no memory map is available.  If a memory address
       does not fall within any returned regions, it's assumed to be
       RAM.  The returned memory regions should not overlap.

       The order of regions does not matter; target_memory_map will
       sort regions by starting address. For that reason, this
       function should not be called directly except via
       target_memory_map.

       This method should not cache data; if the memory map could
       change unexpectedly, it should be invalidated, and higher
       layers will re-fetch it.  */
    VEC(mem_region_s) *(*to_memory_map) (struct target_ops *);

    /* Erases the region of flash memory starting at ADDRESS, of
       length LENGTH.

       Precondition: both ADDRESS and ADDRESS+LENGTH should be aligned
       on flash block boundaries, as reported by 'to_memory_map'.  */
    void (*to_flash_erase) (struct target_ops *,
                           ULONGEST address, LONGEST length);

    /* Finishes a flash memory write sequence.  After this operation
       all flash memory should be available for writing and the result
       of reading from areas written by 'to_flash_write' should be
       equal to what was written.  */
    void (*to_flash_done) (struct target_ops *);

    /* Describe the architecture-specific features of this target.
       Returns the description found, or NULL if no description
       was available.  */
    const struct target_desc *(*to_read_description) (struct target_ops *ops);

    int to_magic;
    /* Need sub-structure for target machine related rather than comm related?
     */
  };

/* Magic number for checking ops size.  If a struct doesn't end with this
   number, somebody changed the declaration but didn't change all the
   places that initialize one.  */

#define	OPS_MAGIC	3840

/* The ops structure for our "current" target process.  This should
   never be NULL.  If there is no target, it points to the dummy_target.  */

extern struct target_ops current_target;

/* Define easy words for doing these operations on our current target.  */

#define	target_shortname	(current_target.to_shortname)
#define	target_longname		(current_target.to_longname)

/* Does whatever cleanup is required for a target that we are no
   longer going to be calling.  QUITTING indicates that GDB is exiting
   and should not get hung on an error (otherwise it is important to
   perform clean termination, even if it takes a while).  This routine
   is automatically always called when popping the target off the
   target stack (to_beneath is undefined).  Closing file descriptors
   and freeing all memory allocated memory are typical things it
   should do.  */

void target_close (struct target_ops *targ, int quitting);

/* Attaches to a process on the target side.  Arguments are as passed
   to the `attach' command by the user.  This routine can be called
   when the target is not on the target-stack, if the target_can_run
   routine returns 1; in that case, it must push itself onto the stack.
   Upon exit, the target should be ready for normal operations, and
   should be ready to deliver the status of the process immediately
   (without waiting) to an upcoming target_wait call.  */

#define	target_attach(args, from_tty)	\
     (*current_target.to_attach) (args, from_tty)

/* The target_attach operation places a process under debugger control,
   and stops the process.

   This operation provides a target-specific hook that allows the
   necessary bookkeeping to be performed after an attach completes.  */
#define target_post_attach(pid) \
     (*current_target.to_post_attach) (pid)

/* Takes a program previously attached to and detaches it.
   The program may resume execution (some targets do, some don't) and will
   no longer stop on signals, etc.  We better not have left any breakpoints
   in the program or it'll die when it hits one.  ARGS is arguments
   typed by the user (e.g. a signal to send the process).  FROM_TTY
   says whether to be verbose or not.  */

extern void target_detach (char *, int);

/* Disconnect from the current target without resuming it (leaving it
   waiting for a debugger).  */

extern void target_disconnect (char *, int);

/* Resume execution of the target process PTID.  STEP says whether to
   single-step or to run free; SIGGNAL is the signal to be given to
   the target, or TARGET_SIGNAL_0 for no signal.  The caller may not
   pass TARGET_SIGNAL_DEFAULT.  */

#define	target_resume(ptid, step, siggnal)				\
  do {									\
    dcache_invalidate(target_dcache);					\
    (*current_target.to_resume) (ptid, step, siggnal);			\
  } while (0)

/* Wait for process pid to do something.  PTID = -1 to wait for any
   pid to do something.  Return pid of child, or -1 in case of error;
   store status through argument pointer STATUS.  Note that it is
   _NOT_ OK to throw_exception() out of target_wait() without popping
   the debugging target from the stack; GDB isn't prepared to get back
   to the prompt with a debugging target but without the frame cache,
   stop_pc, etc., set up.  */

#define	target_wait(ptid, status)		\
     (*current_target.to_wait) (ptid, status)

/* Fetch at least register REGNO, or all regs if regno == -1.  No result.  */

#define	target_fetch_registers(regcache, regno)	\
     (*current_target.to_fetch_registers) (regcache, regno)

/* Store at least register REGNO, or all regs if REGNO == -1.
   It can store as many registers as it wants to, so target_prepare_to_store
   must have been previously called.  Calls error() if there are problems.  */

#define	target_store_registers(regcache, regs)	\
     (*current_target.to_store_registers) (regcache, regs)

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that REGISTERS contains all the registers from the program being
   debugged.  */

#define	target_prepare_to_store(regcache)	\
     (*current_target.to_prepare_to_store) (regcache)

extern DCACHE *target_dcache;

extern int target_read_string (CORE_ADDR, char **, int, int *);

extern int target_read_memory (CORE_ADDR memaddr, gdb_byte *myaddr, int len);

extern int target_write_memory (CORE_ADDR memaddr, const gdb_byte *myaddr,
				int len);

extern int xfer_memory (CORE_ADDR, gdb_byte *, int, int,
			struct mem_attrib *, struct target_ops *);

/* Fetches the target's memory map.  If one is found it is sorted
   and returned, after some consistency checking.  Otherwise, NULL
   is returned.  */
VEC(mem_region_s) *target_memory_map (void);

/* Erase the specified flash region.  */
void target_flash_erase (ULONGEST address, LONGEST length);

/* Finish a sequence of flash operations.  */
void target_flash_done (void);

/* Describes a request for a memory write operation.  */
struct memory_write_request
  {
    /* Begining address that must be written. */
    ULONGEST begin;
    /* Past-the-end address. */
    ULONGEST end;
    /* The data to write. */
    gdb_byte *data;
    /* A callback baton for progress reporting for this request.  */
    void *baton;
  };
typedef struct memory_write_request memory_write_request_s;
DEF_VEC_O(memory_write_request_s);

/* Enumeration specifying different flash preservation behaviour.  */
enum flash_preserve_mode
  {
    flash_preserve,
    flash_discard
  };

/* Write several memory blocks at once.  This version can be more
   efficient than making several calls to target_write_memory, in
   particular because it can optimize accesses to flash memory.

   Moreover, this is currently the only memory access function in gdb
   that supports writing to flash memory, and it should be used for
   all cases where access to flash memory is desirable.

   REQUESTS is the vector (see vec.h) of memory_write_request.
   PRESERVE_FLASH_P indicates what to do with blocks which must be
     erased, but not completely rewritten.
   PROGRESS_CB is a function that will be periodically called to provide
     feedback to user.  It will be called with the baton corresponding
     to the request currently being written.  It may also be called
     with a NULL baton, when preserved flash sectors are being rewritten.

   The function returns 0 on success, and error otherwise.  */
int target_write_memory_blocks (VEC(memory_write_request_s) *requests,
				enum flash_preserve_mode preserve_flash_p,
				void (*progress_cb) (ULONGEST, void *));

/* From infrun.c.  */

extern int inferior_has_forked (int pid, int *child_pid);

extern int inferior_has_vforked (int pid, int *child_pid);

extern int inferior_has_execd (int pid, char **execd_pathname);

/* From exec.c */

extern void print_section_info (struct target_ops *, bfd *);

/* Print a line about the current target.  */

#define	target_files_info()	\
     (*current_target.to_files_info) (&current_target)

/* Insert a breakpoint at address BP_TGT->placed_address in the target
   machine.  Result is 0 for success, or an errno value.  */

#define	target_insert_breakpoint(bp_tgt)	\
     (*current_target.to_insert_breakpoint) (bp_tgt)

/* Remove a breakpoint at address BP_TGT->placed_address in the target
   machine.  Result is 0 for success, or an errno value.  */

#define	target_remove_breakpoint(bp_tgt)	\
     (*current_target.to_remove_breakpoint) (bp_tgt)

/* Initialize the terminal settings we record for the inferior,
   before we actually run the inferior.  */

#define target_terminal_init() \
     (*current_target.to_terminal_init) ()

/* Put the inferior's terminal settings into effect.
   This is preparation for starting or resuming the inferior.  */

#define target_terminal_inferior() \
     (*current_target.to_terminal_inferior) ()

/* Put some of our terminal settings into effect,
   enough to get proper results from our output,
   but do not change into or out of RAW mode
   so that no input is discarded.

   After doing this, either terminal_ours or terminal_inferior
   should be called to get back to a normal state of affairs.  */

#define target_terminal_ours_for_output() \
     (*current_target.to_terminal_ours_for_output) ()

/* Put our terminal settings into effect.
   First record the inferior's terminal settings
   so they can be restored properly later.  */

#define target_terminal_ours() \
     (*current_target.to_terminal_ours) ()

/* Save our terminal settings.
   This is called from TUI after entering or leaving the curses
   mode.  Since curses modifies our terminal this call is here
   to take this change into account.  */

#define target_terminal_save_ours() \
     (*current_target.to_terminal_save_ours) ()

/* Print useful information about our terminal status, if such a thing
   exists.  */

#define target_terminal_info(arg, from_tty) \
     (*current_target.to_terminal_info) (arg, from_tty)

/* Kill the inferior process.   Make it go away.  */

#define target_kill() \
     (*current_target.to_kill) ()

/* Load an executable file into the target process.  This is expected
   to not only bring new code into the target process, but also to
   update GDB's symbol tables to match.

   ARG contains command-line arguments, to be broken down with
   buildargv ().  The first non-switch argument is the filename to
   load, FILE; the second is a number (as parsed by strtoul (..., ...,
   0)), which is an offset to apply to the load addresses of FILE's
   sections.  The target may define switches, or other non-switch
   arguments, as it pleases.  */

extern void target_load (char *arg, int from_tty);

/* Look up a symbol in the target's symbol table.  NAME is the symbol
   name.  ADDRP is a CORE_ADDR * pointing to where the value of the
   symbol should be returned.  The result is 0 if successful, nonzero
   if the symbol does not exist in the target environment.  This
   function should not call error() if communication with the target
   is interrupted, since it is called from symbol reading, but should
   return nonzero, possibly doing a complain().  */

#define target_lookup_symbol(name, addrp) \
     (*current_target.to_lookup_symbol) (name, addrp)

/* Start an inferior process and set inferior_ptid to its pid.
   EXEC_FILE is the file to run.
   ALLARGS is a string containing the arguments to the program.
   ENV is the environment vector to pass.  Errors reported with error().
   On VxWorks and various standalone systems, we ignore exec_file.  */

#define	target_create_inferior(exec_file, args, env, FROM_TTY)	\
     (*current_target.to_create_inferior) (exec_file, args, env, (FROM_TTY))


/* Some targets (such as ttrace-based HPUX) don't allow us to request
   notification of inferior events such as fork and vork immediately
   after the inferior is created.  (This because of how gdb gets an
   inferior created via invoking a shell to do it.  In such a scenario,
   if the shell init file has commands in it, the shell will fork and
   exec for each of those commands, and we will see each such fork
   event.  Very bad.)

   Such targets will supply an appropriate definition for this function.  */

#define target_post_startup_inferior(ptid) \
     (*current_target.to_post_startup_inferior) (ptid)

/* On some targets, the sequence of starting up an inferior requires
   some synchronization between gdb and the new inferior process, PID.  */

#define target_acknowledge_created_inferior(pid) \
     (*current_target.to_acknowledge_created_inferior) (pid)

/* On some targets, we can catch an inferior fork or vfork event when
   it occurs.  These functions insert/remove an already-created
   catchpoint for such events.  */

#define target_insert_fork_catchpoint(pid) \
     (*current_target.to_insert_fork_catchpoint) (pid)

#define target_remove_fork_catchpoint(pid) \
     (*current_target.to_remove_fork_catchpoint) (pid)

#define target_insert_vfork_catchpoint(pid) \
     (*current_target.to_insert_vfork_catchpoint) (pid)

#define target_remove_vfork_catchpoint(pid) \
     (*current_target.to_remove_vfork_catchpoint) (pid)

/* If the inferior forks or vforks, this function will be called at
   the next resume in order to perform any bookkeeping and fiddling
   necessary to continue debugging either the parent or child, as
   requested, and releasing the other.  Information about the fork
   or vfork event is available via get_last_target_status ().
   This function returns 1 if the inferior should not be resumed
   (i.e. there is another event pending).  */

int target_follow_fork (int follow_child);

/* On some targets, we can catch an inferior exec event when it
   occurs.  These functions insert/remove an already-created
   catchpoint for such events.  */

#define target_insert_exec_catchpoint(pid) \
     (*current_target.to_insert_exec_catchpoint) (pid)

#define target_remove_exec_catchpoint(pid) \
     (*current_target.to_remove_exec_catchpoint) (pid)

/* Returns the number of exec events that are reported when a process
   invokes a flavor of the exec() system call on this target, if exec
   events are being reported.  */

#define target_reported_exec_events_per_exec_call() \
     (*current_target.to_reported_exec_events_per_exec_call) ()

/* Returns TRUE if PID has exited.  And, also sets EXIT_STATUS to the
   exit code of PID, if any.  */

#define target_has_exited(pid,wait_status,exit_status) \
     (*current_target.to_has_exited) (pid,wait_status,exit_status)

/* The debugger has completed a blocking wait() call.  There is now
   some process event that must be processed.  This function should
   be defined by those targets that require the debugger to perform
   cleanup or internal state changes in response to the process event.  */

/* The inferior process has died.  Do what is right.  */

#define	target_mourn_inferior()	\
     (*current_target.to_mourn_inferior) ()

/* Does target have enough data to do a run or attach command? */

#define target_can_run(t) \
     ((t)->to_can_run) ()

/* post process changes to signal handling in the inferior.  */

#define target_notice_signals(ptid) \
     (*current_target.to_notice_signals) (ptid)

/* Check to see if a thread is still alive.  */

#define target_thread_alive(ptid) \
     (*current_target.to_thread_alive) (ptid)

/* Query for new threads and add them to the thread list.  */

#define target_find_new_threads() \
     (*current_target.to_find_new_threads) (); \

/* Make target stop in a continuable fashion.  (For instance, under
   Unix, this should act like SIGSTOP).  This function is normally
   used by GUIs to implement a stop button.  */

#define target_stop current_target.to_stop

/* Send the specified COMMAND to the target's monitor
   (shell,interpreter) for execution.  The result of the query is
   placed in OUTBUF.  */

#define target_rcmd(command, outbuf) \
     (*current_target.to_rcmd) (command, outbuf)


/* Get the symbol information for a breakpointable routine called when
   an exception event occurs.
   Intended mainly for C++, and for those
   platforms/implementations where such a callback mechanism is available,
   e.g. HP-UX with ANSI C++ (aCC).  Some compilers (e.g. g++) support
   different mechanisms for debugging exceptions.  */

#define target_enable_exception_callback(kind, enable) \
     (*current_target.to_enable_exception_callback) (kind, enable)

/* Get the current exception event kind -- throw or catch, etc.  */

#define target_get_current_exception_event() \
     (*current_target.to_get_current_exception_event) ()

/* Does the target include all of memory, or only part of it?  This
   determines whether we look up the target chain for other parts of
   memory if this target can't satisfy a request.  */

#define	target_has_all_memory	\
     (current_target.to_has_all_memory)

/* Does the target include memory?  (Dummy targets don't.)  */

#define	target_has_memory	\
     (current_target.to_has_memory)

/* Does the target have a stack?  (Exec files don't, VxWorks doesn't, until
   we start a process.)  */

#define	target_has_stack	\
     (current_target.to_has_stack)

/* Does the target have registers?  (Exec files don't.)  */

#define	target_has_registers	\
     (current_target.to_has_registers)

/* Does the target have execution?  Can we make it jump (through
   hoops), or pop its stack a few times?  This means that the current
   target is currently executing; for some targets, that's the same as
   whether or not the target is capable of execution, but there are
   also targets which can be current while not executing.  In that
   case this will become true after target_create_inferior or
   target_attach.  */

#define	target_has_execution	\
     (current_target.to_has_execution)

/* Can the target support the debugger control of thread execution?
   a) Can it lock the thread scheduler?
   b) Can it switch the currently running thread?  */

#define target_can_lock_scheduler \
     (current_target.to_has_thread_control & tc_schedlock)

#define target_can_switch_threads \
     (current_target.to_has_thread_control & tc_switch)

/* Can the target support asynchronous execution? */
#define target_can_async_p() (current_target.to_can_async_p ())

/* Is the target in asynchronous execution mode? */
#define target_is_async_p() (current_target.to_is_async_p())

/* Put the target in async mode with the specified callback function. */
#define target_async(CALLBACK,CONTEXT) \
     (current_target.to_async((CALLBACK), (CONTEXT)))

/* This is to be used ONLY within call_function_by_hand(). It provides
   a workaround, to have inferior function calls done in sychronous
   mode, even though the target is asynchronous. After
   target_async_mask(0) is called, calls to target_can_async_p() will
   return FALSE , so that target_resume() will not try to start the
   target asynchronously. After the inferior stops, we IMMEDIATELY
   restore the previous nature of the target, by calling
   target_async_mask(1). After that, target_can_async_p() will return
   TRUE. ANY OTHER USE OF THIS FEATURE IS DEPRECATED.

   FIXME ezannoni 1999-12-13: we won't need this once we move
   the turning async on and off to the single execution commands,
   from where it is done currently, in remote_resume().  */

#define	target_async_mask_value	\
     (current_target.to_async_mask_value)

extern int target_async_mask (int mask);

/* Converts a process id to a string.  Usually, the string just contains
   `process xyz', but on some systems it may contain
   `process xyz thread abc'.  */

#undef target_pid_to_str
#define target_pid_to_str(PID) current_target.to_pid_to_str (PID)

#ifndef target_tid_to_str
#define target_tid_to_str(PID) \
     target_pid_to_str (PID)
extern char *normal_pid_to_str (ptid_t ptid);
#endif

/* Return a short string describing extra information about PID,
   e.g. "sleeping", "runnable", "running on LWP 3".  Null return value
   is okay.  */

#define target_extra_thread_info(TP) \
     (current_target.to_extra_thread_info (TP))

#ifndef target_pid_or_tid_to_str
#define target_pid_or_tid_to_str(ID) \
     target_pid_to_str (ID)
#endif

/* Attempts to find the pathname of the executable file
   that was run to create a specified process.

   The process PID must be stopped when this operation is used.

   If the executable file cannot be determined, NULL is returned.

   Else, a pointer to a character string containing the pathname
   is returned.  This string should be copied into a buffer by
   the client if the string will not be immediately used, or if
   it must persist.  */

#define target_pid_to_exec_file(pid) \
     (current_target.to_pid_to_exec_file) (pid)

/*
 * Iterator function for target memory regions.
 * Calls a callback function once for each memory region 'mapped'
 * in the child process.  Defined as a simple macro rather than
 * as a function macro so that it can be tested for nullity.
 */

#define target_find_memory_regions(FUNC, DATA) \
     (current_target.to_find_memory_regions) (FUNC, DATA)

/*
 * Compose corefile .note section.
 */

#define target_make_corefile_notes(BFD, SIZE_P) \
     (current_target.to_make_corefile_notes) (BFD, SIZE_P)

/* Thread-local values.  */
#define target_get_thread_local_address \
    (current_target.to_get_thread_local_address)
#define target_get_thread_local_address_p() \
    (target_get_thread_local_address != NULL)


/* Hardware watchpoint interfaces.  */

/* Returns non-zero if we were stopped by a hardware watchpoint (memory read or
   write).  */

#ifndef STOPPED_BY_WATCHPOINT
#define STOPPED_BY_WATCHPOINT(w) \
   (*current_target.to_stopped_by_watchpoint) ()
#endif

/* Non-zero if we have steppable watchpoints  */

#ifndef HAVE_STEPPABLE_WATCHPOINT
#define HAVE_STEPPABLE_WATCHPOINT \
   (current_target.to_have_steppable_watchpoint)
#endif

/* Non-zero if we have continuable watchpoints  */

#ifndef HAVE_CONTINUABLE_WATCHPOINT
#define HAVE_CONTINUABLE_WATCHPOINT \
   (current_target.to_have_continuable_watchpoint)
#endif

/* Provide defaults for hardware watchpoint functions.  */

/* If the *_hw_beakpoint functions have not been defined
   elsewhere use the definitions in the target vector.  */

/* Returns non-zero if we can set a hardware watchpoint of type TYPE.  TYPE is
   one of bp_hardware_watchpoint, bp_read_watchpoint, bp_write_watchpoint, or
   bp_hardware_breakpoint.  CNT is the number of such watchpoints used so far
   (including this one?).  OTHERTYPE is who knows what...  */

#ifndef TARGET_CAN_USE_HARDWARE_WATCHPOINT
#define TARGET_CAN_USE_HARDWARE_WATCHPOINT(TYPE,CNT,OTHERTYPE) \
 (*current_target.to_can_use_hw_breakpoint) (TYPE, CNT, OTHERTYPE);
#endif

#ifndef TARGET_REGION_OK_FOR_HW_WATCHPOINT
#define TARGET_REGION_OK_FOR_HW_WATCHPOINT(addr, len) \
    (*current_target.to_region_ok_for_hw_watchpoint) (addr, len)
#endif


/* Set/clear a hardware watchpoint starting at ADDR, for LEN bytes.  TYPE is 0
   for write, 1 for read, and 2 for read/write accesses.  Returns 0 for
   success, non-zero for failure.  */

#ifndef target_insert_watchpoint
#define	target_insert_watchpoint(addr, len, type)	\
     (*current_target.to_insert_watchpoint) (addr, len, type)

#define	target_remove_watchpoint(addr, len, type)	\
     (*current_target.to_remove_watchpoint) (addr, len, type)
#endif

#ifndef target_insert_hw_breakpoint
#define target_insert_hw_breakpoint(bp_tgt) \
     (*current_target.to_insert_hw_breakpoint) (bp_tgt)

#define target_remove_hw_breakpoint(bp_tgt) \
     (*current_target.to_remove_hw_breakpoint) (bp_tgt)
#endif

extern int target_stopped_data_address_p (struct target_ops *);

#ifndef target_stopped_data_address
#define target_stopped_data_address(target, x) \
    (*target.to_stopped_data_address) (target, x)
#else
/* Horrible hack to get around existing macros :-(.  */
#define target_stopped_data_address_p(CURRENT_TARGET) (1)
#endif

extern const struct target_desc *target_read_description (struct target_ops *);

/* Routines for maintenance of the target structures...

   add_target:   Add a target to the list of all possible targets.

   push_target:  Make this target the top of the stack of currently used
   targets, within its particular stratum of the stack.  Result
   is 0 if now atop the stack, nonzero if not on top (maybe
   should warn user).

   unpush_target: Remove this from the stack of currently used targets,
   no matter where it is on the list.  Returns 0 if no
   change, 1 if removed from stack.

   pop_target:   Remove the top thing on the stack of current targets.  */

extern void add_target (struct target_ops *);

extern int push_target (struct target_ops *);

extern int unpush_target (struct target_ops *);

extern void target_pre_inferior (int);

extern void target_preopen (int);

extern void pop_target (void);

extern CORE_ADDR target_translate_tls_address (struct objfile *objfile,
					       CORE_ADDR offset);

/* Mark a pushed target as running or exited, for targets which do not
   automatically pop when not active.  */

void target_mark_running (struct target_ops *);

void target_mark_exited (struct target_ops *);

/* Struct section_table maps address ranges to file sections.  It is
   mostly used with BFD files, but can be used without (e.g. for handling
   raw disks, or files not in formats handled by BFD).  */

struct section_table
  {
    CORE_ADDR addr;		/* Lowest address in section */
    CORE_ADDR endaddr;		/* 1+highest address in section */

    struct bfd_section *the_bfd_section;

    bfd *bfd;			/* BFD file pointer */
  };

/* Return the "section" containing the specified address.  */
struct section_table *target_section_by_addr (struct target_ops *target,
					      CORE_ADDR addr);


/* From mem-break.c */

extern int memory_remove_breakpoint (struct bp_target_info *);

extern int memory_insert_breakpoint (struct bp_target_info *);

extern int default_memory_remove_breakpoint (struct bp_target_info *);

extern int default_memory_insert_breakpoint (struct bp_target_info *);


/* From target.c */

extern void initialize_targets (void);

extern void noprocess (void);

extern void find_default_attach (char *, int);

extern void find_default_create_inferior (char *, char *, char **, int);

extern struct target_ops *find_run_target (void);

extern struct target_ops *find_core_target (void);

extern struct target_ops *find_target_beneath (struct target_ops *);

extern int target_resize_to_sections (struct target_ops *target,
				      int num_added);

extern void remove_target_sections (bfd *abfd);


/* Stuff that should be shared among the various remote targets.  */

/* Debugging level.  0 is off, and non-zero values mean to print some debug
   information (higher values, more information).  */
extern int remote_debug;

/* Speed in bits per second, or -1 which means don't mess with the speed.  */
extern int baud_rate;
/* Timeout limit for response from target. */
extern int remote_timeout;


/* Functions for helping to write a native target.  */

/* This is for native targets which use a unix/POSIX-style waitstatus.  */
extern void store_waitstatus (struct target_waitstatus *, int);

/* Predicate to target_signal_to_host(). Return non-zero if the enum
   targ_signal SIGNO has an equivalent ``host'' representation.  */
/* FIXME: cagney/1999-11-22: The name below was chosen in preference
   to the shorter target_signal_p() because it is far less ambigious.
   In this context ``target_signal'' refers to GDB's internal
   representation of the target's set of signals while ``host signal''
   refers to the target operating system's signal.  Confused?  */

extern int target_signal_to_host_p (enum target_signal signo);

/* Convert between host signal numbers and enum target_signal's.
   target_signal_to_host() returns 0 and prints a warning() on GDB's
   console if SIGNO has no equivalent host representation.  */
/* FIXME: cagney/1999-11-22: Here ``host'' is used incorrectly, it is
   refering to the target operating system's signal numbering.
   Similarly, ``enum target_signal'' is named incorrectly, ``enum
   gdb_signal'' would probably be better as it is refering to GDB's
   internal representation of a target operating system's signal.  */

extern enum target_signal target_signal_from_host (int);
extern int target_signal_to_host (enum target_signal);

/* Convert from a number used in a GDB command to an enum target_signal.  */
extern enum target_signal target_signal_from_command (int);

/* Any target can call this to switch to remote protocol (in remote.c). */
extern void push_remote_target (char *name, int from_tty);

/* Imported from machine dependent code */

/* Blank target vector entries are initialized to target_ignore. */
void target_ignore (void);

extern struct target_ops deprecated_child_ops;

#endif /* !defined (TARGET_H) */
