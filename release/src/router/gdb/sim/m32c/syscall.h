/* Copied from libgloss */
/* General use syscall.h file.
   The more ports that use this file, the simpler sim/common/nltvals.def
   remains.  */

#ifndef LIBGLOSS_SYSCALL_H
#define LIBGLOSS_SYSCALL_H

/* Note: This file may be included by assembler source.  */

/* These should be as small as possible to allow a port to use a trap type
   instruction, which the system call # as the trap (the d10v for instance
   supports traps 0..31).  An alternative would be to define one trap for doing
   system calls, and put the system call number in a register that is not used
   for the normal calling sequence (so that you don't have to shift down the
   arguments to add the system call number).  Obviously, if these system call
   numbers are ever changed, all of the simulators and potentially user code
   will need to be updated.  */

/* There is no current need for the following: SYS_execv, SYS_creat, SYS_wait,
   etc. etc.  Don't add them.  */

/* These are required by the ANSI C part of newlib (excluding system() of
   course).  */
#define	SYS_exit	1
#define	SYS_open	2
#define	SYS_close	3
#define	SYS_read	4
#define	SYS_write	5
#define	SYS_lseek	6
#define	SYS_unlink	7
#define	SYS_getpid	8
#define	SYS_kill	9
#define SYS_fstat       10
/*#define SYS_sbrk	11 - not currently a system call, but reserved.  */

/* ARGV support.  */
#define SYS_argvlen	12
#define SYS_argv	13

/* These are extras added for one reason or another.  */
#define SYS_chdir	 14
#define SYS_stat	 15
#define SYS_chmod 	 16
#define SYS_utime 	 17
#define SYS_time 	 18
#define SYS_gettimeofday 19
#define SYS_times	 20
#define SYS_link	 21
#endif
