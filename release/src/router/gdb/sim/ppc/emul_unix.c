/*  This file is part of the program psim.

    Copyright (C) 1996-1998, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

    */


#ifndef _EMUL_UNIX_C_
#define _EMUL_UNIX_C_


/* Note: this module is called via a table.  There is no benefit in
   making it inline */

#include "emul_generic.h"
#include "emul_unix.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/stat.h>
#else
#undef HAVE_STAT
#undef HAVE_LSTAT
#undef HAVE_FSTAT
#endif

#include <stdio.h>
#include <signal.h>
#include <errno.h>

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifndef HAVE_TERMIOS_STRUCTURE
#undef HAVE_SYS_TERMIOS_H
#undef HAVE_TCGETATTR
#else
#ifndef HAVE_SYS_TERMIOS_H
#undef HAVE_TERMIOS_STRUCTURE
#endif
#endif

#ifdef HAVE_TERMIOS_STRUCTURE
#include <sys/termios.h>

/* If we have TERMIOS, use that for the termio structure, since some systems
   don't like including both sys/termios.h and sys/termio.h at the same
   time.  */
#undef	HAVE_TERMIO_STRUCTURE
#undef	TCGETA
#undef	termio
#define termio termios
#endif

#ifndef HAVE_TERMIO_STRUCTURE
#undef HAVE_SYS_TERMIO_H
#else
#ifndef HAVE_SYS_TERMIO_H
#undef HAVE_TERMIO_STRUCTURE
#endif
#endif

#ifdef HAVE_TERMIO_STRUCTURE
#include <sys/termio.h>
#endif

#ifdef HAVE_GETRUSAGE
#ifndef HAVE_SYS_RESOURCE_H
#undef HAVE_GETRUSAGE
#endif
#endif

#ifdef HAVE_GETRUSAGE
#include <sys/resource.h>
int getrusage();
#endif

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#ifdef HAVE_UNISTD_H
#undef MAXPATHLEN		/* sys/param.h might define this also */
#include <unistd.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if defined(BSD) && !defined(errno) && (BSD < 199306)	/* here BSD as just a bug */
extern int errno;
#endif

#ifndef STATIC_INLINE_EMUL_UNIX
#define STATIC_INLINE_EMUL_UNIX STATIC_INLINE
#endif

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifndef EINVAL
#define EINVAL -1
#endif

/* UNIX's idea of what is needed to implement emulations */

struct _os_emul_data {
  device *vm;
  emul_syscall *syscalls;
};


/* Emulation of simple UNIX system calls that are common on all systems.  */

/* Structures that are common agmonst the UNIX varients */
struct unix_timeval {
  signed32 tv_sec;		/* seconds */
  signed32 tv_usec;		/* microseconds */
};

struct unix_timezone {
  signed32 tz_minuteswest;	/* minutes west of Greenwich */
  signed32 tz_dsttime;		/* type of dst correction */
};

#define	UNIX_RUSAGE_SELF	0
#define	UNIX_RUSAGE_CHILDREN	(-1)
#define UNIX_RUSAGE_BOTH	(-2)	/* sys_wait4() uses this */

struct	unix_rusage {
	struct unix_timeval ru_utime;	/* user time used */
	struct unix_timeval ru_stime;	/* system time used */
	signed32 ru_maxrss;		/* maximum resident set size */
	signed32 ru_ixrss;		/* integral shared memory size */
	signed32 ru_idrss;		/* integral unshared data size */
	signed32 ru_isrss;		/* integral unshared stack size */
	signed32 ru_minflt;		/* any page faults not requiring I/O */
	signed32 ru_majflt;		/* any page faults requiring I/O */
	signed32 ru_nswap;		/* swaps */
	signed32 ru_inblock;		/* block input operations */
	signed32 ru_oublock;		/* block output operations */
	signed32 ru_msgsnd;		/* messages sent */
	signed32 ru_msgrcv;		/* messages received */
	signed32 ru_nsignals;		/* signals received */
	signed32 ru_nvcsw;		/* voluntary context switches */
	signed32 ru_nivcsw;		/* involuntary " */
};


static void
do_unix_exit(os_emul_data *emul,
	     unsigned call,
	     const int arg0,
	     cpu *processor,
	     unsigned_word cia)
{
  int status = (int)cpu_registers(processor)->gpr[arg0];
  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("%d)\n", status);

  cpu_halt(processor, cia, was_exited, status);
}


static void
do_unix_read(os_emul_data *emul,
	     unsigned call,
	     const int arg0,
	     cpu *processor,
	     unsigned_word cia)
{
  void *scratch_buffer;
  int d = (int)cpu_registers(processor)->gpr[arg0];
  unsigned_word buf = cpu_registers(processor)->gpr[arg0+1];
  int nbytes = cpu_registers(processor)->gpr[arg0+2];
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("%d, 0x%lx, %d", d, (long)buf, nbytes);

  /* get a tempoary bufer */
  scratch_buffer = zalloc(nbytes);

  /* check if buffer exists by reading it */
  emul_read_buffer(scratch_buffer, buf, nbytes, processor, cia);

  /* read */
  status = read (d, scratch_buffer, nbytes);

  emul_write_status(processor, status, errno);
  if (status > 0)
    emul_write_buffer(scratch_buffer, buf, status, processor, cia);

  zfree(scratch_buffer);
}


static void
do_unix_write(os_emul_data *emul,
	      unsigned call,
	      const int arg0,
	      cpu *processor,
	      unsigned_word cia)
{
  void *scratch_buffer = NULL;
  int d = (int)cpu_registers(processor)->gpr[arg0];
  unsigned_word buf = cpu_registers(processor)->gpr[arg0+1];
  int nbytes = cpu_registers(processor)->gpr[arg0+2];
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("%d, 0x%lx, %d", d, (long)buf, nbytes);

  /* get a tempoary bufer */
  scratch_buffer = zalloc(nbytes); /* FIXME - nbytes == 0 */

  /* copy in */
  emul_read_buffer(scratch_buffer, buf, nbytes,
		   processor, cia);

  /* write */
  status = write(d, scratch_buffer, nbytes);
  emul_write_status(processor, status, errno);
  zfree(scratch_buffer);

  flush_stdoutput();
}


static void
do_unix_open(os_emul_data *emul,
	     unsigned call,
	     const int arg0,
	     cpu *processor,
	     unsigned_word cia)
{
  unsigned_word path_addr = cpu_registers(processor)->gpr[arg0];
  char path_buf[PATH_MAX];
  char *path = emul_read_string(path_buf, path_addr, PATH_MAX, processor, cia);
  int flags = (int)cpu_registers(processor)->gpr[arg0+1];
  int mode = (int)cpu_registers(processor)->gpr[arg0+2];
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("0x%lx [%s], 0x%x, 0x%x", (long)path_addr, path, flags, mode);

  status = open(path, flags, mode);
  emul_write_status(processor, status, errno);
}


static void
do_unix_close(os_emul_data *emul,
	      unsigned call,
	      const int arg0,
	      cpu *processor,
	      unsigned_word cia)
{
  int d = (int)cpu_registers(processor)->gpr[arg0];
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("%d", d);

  status = close(d);
  emul_write_status(processor, status, errno);
}


static void
do_unix_break(os_emul_data *emul,
	      unsigned call,
	      const int arg0,
	      cpu *processor,
	      unsigned_word cia)
{
  /* just pass this onto the `vm' device */
  unsigned_word new_break = cpu_registers(processor)->gpr[arg0];
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("0x%lx", (long)cpu_registers(processor)->gpr[arg0]);

  status = device_ioctl(emul->vm,
			processor,
			cia,
			device_ioctl_break,
			new_break); /*ioctl-data*/

  emul_write_status(processor, 0, status);
}

#ifndef HAVE_ACCESS
#define do_unix_access 0
#else
static void
do_unix_access(os_emul_data *emul,
	       unsigned call,
	       const int arg0,
	       cpu *processor,
	       unsigned_word cia)
{
  unsigned_word path_addr = cpu_registers(processor)->gpr[arg0];
  char path_buf[PATH_MAX];
  char *path = emul_read_string(path_buf, path_addr, PATH_MAX, processor, cia);
  int mode = (int)cpu_registers(processor)->gpr[arg0+1];
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("0x%lx [%s], 0x%x [0%o]", (long)path_addr, path, mode, mode);

  status = access(path, mode);
  emul_write_status(processor, status, errno);
}
#endif

#ifndef HAVE_GETPID
#define do_unix_getpid 0
#else
static void
do_unix_getpid(os_emul_data *emul,
	       unsigned call,
	       const int arg0,
	       cpu *processor,
	       unsigned_word cia)
{
  pid_t status = getpid();
  emul_write_status(processor, (int)status, errno);
}
#endif

#ifndef HAVE_GETPPID
#define do_unix_getppid 0
#else
static void
do_unix_getppid(os_emul_data *emul,
		unsigned call,
		const int arg0,
		cpu *processor,
		unsigned_word cia)
{
  pid_t status = getppid();
  emul_write_status(processor, (int)status, errno);
}
#endif

#if !defined(HAVE_GETPID) || !defined(HAVE_GETPPID)
#define do_unix_getpid2 0
#else
static void
do_unix_getpid2(os_emul_data *emul,
		unsigned call,
		const int arg0,
		cpu *processor,
		unsigned_word cia)
{
  int pid  = (int)getpid();
  int ppid = (int)getppid();
  emul_write2_status(processor, pid, ppid, errno);
}
#endif

#if !defined(HAVE_GETUID) || !defined(HAVE_GETEUID)
#define do_unix_getuid2 0
#else
static void
do_unix_getuid2(os_emul_data *emul,
		unsigned call,
		const int arg0,
		cpu *processor,
		unsigned_word cia)
{
  uid_t uid  = getuid();
  uid_t euid = geteuid();
  emul_write2_status(processor, (int)uid, (int)euid, errno);
}
#endif

#ifndef HAVE_GETUID
#define do_unix_getuid 0
#else
static void
do_unix_getuid(os_emul_data *emul,
	       unsigned call,
	       const int arg0,
	       cpu *processor,
	       unsigned_word cia)
{
  uid_t status = getuid();
  emul_write_status(processor, (int)status, errno);
}
#endif

#ifndef HAVE_GETEUID
#define do_unix_geteuid 0
#else
static void
do_unix_geteuid(os_emul_data *emul,
		unsigned call,
		const int arg0,
		cpu *processor,
		unsigned_word cia)
{
  uid_t status = geteuid();
  emul_write_status(processor, (int)status, errno);
}
#endif

#if 0
#ifndef HAVE_KILL
#define do_unix_kill 0
#else
static void
do_unix_kill(os_emul_data *emul,
	     unsigned call,
	     const int arg0,
	     cpu *processor,
	     unsigned_word cia)
{
  pid_t pid = cpu_registers(processor)->gpr[arg0];
  int sig = cpu_registers(processor)->gpr[arg0+1];

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("%d, %d", (int)pid, sig);

  printf_filtered("SYS_kill at 0x%lx - more to this than just being killed\n",
		  (long)cia);

  cpu_halt(processor, cia, was_signalled, sig);
}
#endif
#endif

#ifndef HAVE_DUP
#define do_unix_dup 0
#else
static void
do_unix_dup(os_emul_data *emul,
	    unsigned call,
	    const int arg0,
	    cpu *processor,
	    unsigned_word cia)
{
  int oldd = cpu_registers(processor)->gpr[arg0];
  int status = dup(oldd);
  int err = errno;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("%d", oldd);

  emul_write_status(processor, status, err);
}
#endif

#ifndef HAVE_DUP2
#define do_unix_dup2 0
#else
static void
do_unix_dup2(os_emul_data *emul,
	     unsigned call,
	     const int arg0,
	     cpu *processor,
	     unsigned_word cia)
{
  int oldd = cpu_registers(processor)->gpr[arg0];
  int newd = cpu_registers(processor)->gpr[arg0+1];
  int status = dup2(oldd, newd);
  int err = errno;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("%d, %d", oldd, newd);

  emul_write_status(processor, status, err);
}
#endif

#ifndef HAVE_LSEEK
#define do_unix_lseek 0
#else
static void
do_unix_lseek(os_emul_data *emul,
	      unsigned call,
	      const int arg0,
	      cpu *processor,
	      unsigned_word cia)
{
  int fildes   = (int)cpu_registers(processor)->gpr[arg0];
  off_t offset = (off_t)cpu_registers(processor)->gpr[arg0+1];
  int whence   = (int)cpu_registers(processor)->gpr[arg0+2];
  off_t status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("%d %ld %d", fildes, (long)offset, whence);

  status = lseek(fildes, offset, whence);
  emul_write_status(processor, (int)status, errno);
}
#endif


#if !defined(HAVE_GETGID) || !defined(HAVE_GETEGID)
#define do_unix_getgid2 0
#else
static void
do_unix_getgid2(os_emul_data *emul,
		unsigned call,
		const int arg0,
		cpu *processor,
		unsigned_word cia)
{
  gid_t gid  = getgid();
  gid_t egid = getegid();
  emul_write2_status(processor, (int)gid, (int)egid, errno);
}
#endif

#ifndef HAVE_GETGID
#define do_unix_getgid 0
#else
static void
do_unix_getgid(os_emul_data *emul,
	       unsigned call,
	       const int arg0,
	       cpu *processor,
	       unsigned_word cia)
{
  gid_t status = getgid();
  emul_write_status(processor, (int)status, errno);
}
#endif

#ifndef HAVE_GETEGID
#define do_unix_getegid 0
#else
static void
do_unix_getegid(os_emul_data *emul,
		unsigned call,
		const int arg0,
		cpu *processor,
		unsigned_word cia)
{
  gid_t status = getegid();
  emul_write_status(processor, (int)status, errno);
}
#endif

#ifndef HAVE_UMASK
#define do_unix_umask 0
#else
static void
do_unix_umask(os_emul_data *emul,
	      unsigned call,
	      const int arg0,
	      cpu *processor,
	      unsigned_word cia)
{
  mode_t mask = (mode_t)cpu_registers(processor)->gpr[arg0];
  int status = umask(mask);

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("0%o", (unsigned int)mask);

  emul_write_status(processor, status, errno);
}
#endif

#ifndef HAVE_CHDIR
#define do_unix_chdir 0
#else
static void
do_unix_chdir(os_emul_data *emul,
	      unsigned call,
	      const int arg0,
	      cpu *processor,
	      unsigned_word cia)
{
  unsigned_word path_addr = cpu_registers(processor)->gpr[arg0];
  char path_buf[PATH_MAX];
  char *path = emul_read_string(path_buf, path_addr, PATH_MAX, processor, cia);
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("0x%lx [%s]", (long)path_addr, path);

  status = chdir(path);
  emul_write_status(processor, status, errno);
}
#endif

#ifndef HAVE_LINK
#define do_unix_link 0
#else
static void
do_unix_link(os_emul_data *emul,
	     unsigned call,
	     const int arg0,
	     cpu *processor,
	     unsigned_word cia)
{
  unsigned_word path1_addr = cpu_registers(processor)->gpr[arg0];
  char path1_buf[PATH_MAX];
  char *path1 = emul_read_string(path1_buf, path1_addr, PATH_MAX, processor, cia);
  unsigned_word path2_addr = cpu_registers(processor)->gpr[arg0+1];
  char path2_buf[PATH_MAX];
  char *path2 = emul_read_string(path2_buf, path2_addr, PATH_MAX, processor, cia);
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("0x%lx [%s], 0x%lx [%s]", (long)path1_addr, path1, (long)path2_addr, path2);

  status = link(path1, path2);
  emul_write_status(processor, status, errno);
}
#endif

#ifndef HAVE_SYMLINK
#define do_unix_symlink 0
#else
static void
do_unix_symlink(os_emul_data *emul,
		unsigned call,
		const int arg0,
		cpu *processor,
		unsigned_word cia)
{
  unsigned_word path1_addr = cpu_registers(processor)->gpr[arg0];
  char path1_buf[PATH_MAX];
  char *path1 = emul_read_string(path1_buf, path1_addr, PATH_MAX, processor, cia);
  unsigned_word path2_addr = cpu_registers(processor)->gpr[arg0+1];
  char path2_buf[PATH_MAX];
  char *path2 = emul_read_string(path2_buf, path2_addr, PATH_MAX, processor, cia);
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("0x%lx [%s], 0x%lx [%s]", (long)path1_addr, path1, (long)path2_addr, path2);

  status = symlink(path1, path2);
  emul_write_status(processor, status, errno);
}
#endif

#ifndef HAVE_UNLINK
#define do_unix_unlink 0
#else
static void
do_unix_unlink(os_emul_data *emul,
	       unsigned call,
	       const int arg0,
	       cpu *processor,
	       unsigned_word cia)
{
  unsigned_word path_addr = cpu_registers(processor)->gpr[arg0];
  char path_buf[PATH_MAX];
  char *path = emul_read_string(path_buf, path_addr, PATH_MAX, processor, cia);
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("0x%lx [%s]", (long)path_addr, path);

  status = unlink(path);
  emul_write_status(processor, status, errno);
}
#endif

#ifndef HAVE_MKDIR
#define do_unix_mkdir 0
#else
static void
do_unix_mkdir(os_emul_data *emul,
	      unsigned call,
	      const int arg0,
	      cpu *processor,
	      unsigned_word cia)
{
  unsigned_word path_addr = cpu_registers(processor)->gpr[arg0];
  char path_buf[PATH_MAX];
  char *path = emul_read_string(path_buf, path_addr, PATH_MAX, processor, cia);
  int mode = (int)cpu_registers(processor)->gpr[arg0+1];
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("0x%lx [%s], 0%3o", (long)path_addr, path, mode);

#ifdef USE_WIN32API
  status = mkdir(path);
#else
  status = mkdir(path, mode);
#endif
  emul_write_status(processor, status, errno);
}
#endif

#ifndef HAVE_RMDIR
#define do_unix_rmdir 0
#else
static void
do_unix_rmdir(os_emul_data *emul,
	      unsigned call,
	      const int arg0,
	      cpu *processor,
	      unsigned_word cia)
{
  unsigned_word path_addr = cpu_registers(processor)->gpr[arg0];
  char path_buf[PATH_MAX];
  char *path = emul_read_string(path_buf, path_addr, PATH_MAX, processor, cia);
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("0x%lx [%s]", (long)path_addr, path);

  status = rmdir(path);
  emul_write_status(processor, status, errno);
}
#endif

#ifndef HAVE_TIME
#define do_unix_time 0
#else
static void
do_unix_time(os_emul_data *emul,
	     unsigned call,
	     const int arg0,
	     cpu *processor,
	     unsigned_word cia)
{
  unsigned_word tp = cpu_registers(processor)->gpr[arg0];
  time_t now = time ((time_t *)0);
  unsigned_word status = H2T_4(now);

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("0x%lx", (long)tp);

  emul_write_status(processor, (int)status, errno);

  if (tp)
    emul_write_buffer(&status, tp, sizeof(status), processor, cia);
}
#endif

#if !defined(HAVE_GETTIMEOFDAY) || !defined(HAVE_SYS_TIME_H)
#define do_unix_gettimeofday 0
#else
static void
do_unix_gettimeofday(os_emul_data *emul,
		     unsigned call,
		     const int arg0,
		     cpu *processor,
		     unsigned_word cia)
{
  unsigned_word tv = cpu_registers(processor)->gpr[arg0];
  unsigned_word tz = cpu_registers(processor)->gpr[arg0+1];
  struct unix_timeval target_timeval;
  struct timeval host_timeval;
  struct unix_timezone target_timezone;
  struct timezone host_timezone;
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("0x%lx, 0x%lx", (long)tv, (long)tz);

  /* Just in case the system doesn't set the timezone structure */
  host_timezone.tz_minuteswest = 0;
  host_timezone.tz_dsttime = 0;

  status = gettimeofday(&host_timeval, &host_timezone);
  if (status >= 0) {
    if (tv) {
      target_timeval.tv_sec = H2T_4(host_timeval.tv_sec);
      target_timeval.tv_usec = H2T_4(host_timeval.tv_usec);
      emul_write_buffer((void *) &target_timeval, tv, sizeof(target_timeval), processor, cia);
    }

    if (tz) {
      target_timezone.tz_minuteswest = H2T_4(host_timezone.tz_minuteswest);
      target_timezone.tz_dsttime = H2T_4(host_timezone.tz_dsttime);
      emul_write_buffer((void *) &target_timezone, tv, sizeof(target_timezone), processor, cia);
    }
  }
    
  emul_write_status(processor, (int)status, errno);
}
#endif


#ifndef HAVE_GETRUSAGE
#define do_unix_getrusage 0
#else
static void
do_unix_getrusage(os_emul_data *emul,
		  unsigned call,
		  const int arg0,
		  cpu *processor,
		  unsigned_word cia)
{
  signed_word who = (signed_word)cpu_registers(processor)->gpr[arg0];
  unsigned_word usage = cpu_registers(processor)->gpr[arg0+1];
  struct rusage host_rusage, host_rusage2;
  struct unix_rusage target_rusage;
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("%ld, 0x%lx", (long)who, (long)usage);

  switch (who) {
  default:
    status = -1;
    errno = EINVAL;
    break;

  case UNIX_RUSAGE_SELF:
    status = getrusage(RUSAGE_SELF, &host_rusage);
    break;

  case UNIX_RUSAGE_CHILDREN:
    status = getrusage(RUSAGE_CHILDREN, &host_rusage);
    break;

  case UNIX_RUSAGE_BOTH:
    status = getrusage(RUSAGE_SELF, &host_rusage);
    if (status >= 0) {
      status = getrusage(RUSAGE_CHILDREN, &host_rusage2);
      if (status >= 0) {
	host_rusage.ru_utime.tv_sec += host_rusage2.ru_utime.tv_sec;
	host_rusage.ru_utime.tv_usec += host_rusage2.ru_utime.tv_usec;
	host_rusage.ru_stime.tv_sec += host_rusage2.ru_stime.tv_sec;
	host_rusage.ru_stime.tv_usec += host_rusage2.ru_stime.tv_usec;
	host_rusage.ru_maxrss += host_rusage2.ru_maxrss;
	host_rusage.ru_ixrss += host_rusage2.ru_ixrss;
	host_rusage.ru_idrss += host_rusage2.ru_idrss;
	host_rusage.ru_isrss += host_rusage2.ru_isrss;
	host_rusage.ru_minflt += host_rusage2.ru_minflt;
	host_rusage.ru_majflt += host_rusage2.ru_majflt;
	host_rusage.ru_nswap += host_rusage2.ru_nswap;
	host_rusage.ru_inblock += host_rusage2.ru_inblock;
	host_rusage.ru_oublock += host_rusage2.ru_oublock;
	host_rusage.ru_msgsnd += host_rusage2.ru_msgsnd;
	host_rusage.ru_msgrcv += host_rusage2.ru_msgrcv;
	host_rusage.ru_nsignals += host_rusage2.ru_nsignals;
	host_rusage.ru_nvcsw += host_rusage2.ru_nvcsw;
	host_rusage.ru_nivcsw += host_rusage2.ru_nivcsw;
      }
    }
  }

  if (status >= 0) {
    target_rusage.ru_utime.tv_sec = H2T_4(host_rusage2.ru_utime.tv_sec);
    target_rusage.ru_utime.tv_usec = H2T_4(host_rusage2.ru_utime.tv_usec);
    target_rusage.ru_stime.tv_sec = H2T_4(host_rusage2.ru_stime.tv_sec);
    target_rusage.ru_stime.tv_usec = H2T_4(host_rusage2.ru_stime.tv_usec);
    target_rusage.ru_maxrss = H2T_4(host_rusage2.ru_maxrss);
    target_rusage.ru_ixrss = H2T_4(host_rusage2.ru_ixrss);
    target_rusage.ru_idrss = H2T_4(host_rusage2.ru_idrss);
    target_rusage.ru_isrss = H2T_4(host_rusage2.ru_isrss);
    target_rusage.ru_minflt = H2T_4(host_rusage2.ru_minflt);
    target_rusage.ru_majflt = H2T_4(host_rusage2.ru_majflt);
    target_rusage.ru_nswap = H2T_4(host_rusage2.ru_nswap);
    target_rusage.ru_inblock = H2T_4(host_rusage2.ru_inblock);
    target_rusage.ru_oublock = H2T_4(host_rusage2.ru_oublock);
    target_rusage.ru_msgsnd = H2T_4(host_rusage2.ru_msgsnd);
    target_rusage.ru_msgrcv = H2T_4(host_rusage2.ru_msgrcv);
    target_rusage.ru_nsignals = H2T_4(host_rusage2.ru_nsignals);
    target_rusage.ru_nvcsw = H2T_4(host_rusage2.ru_nvcsw);
    target_rusage.ru_nivcsw = H2T_4(host_rusage2.ru_nivcsw);
    emul_write_buffer((void *) &target_rusage, usage, sizeof(target_rusage), processor, cia);
  }
    
  emul_write_status(processor, status, errno);
}
#endif


static void
do_unix_nop(os_emul_data *emul,
	    unsigned call,
	    const int arg0,
	    cpu *processor,
	    unsigned_word cia)
{
  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx",
		     (long)cpu_registers(processor)->gpr[arg0],
		     (long)cpu_registers(processor)->gpr[arg0+1],
		     (long)cpu_registers(processor)->gpr[arg0+2],
		     (long)cpu_registers(processor)->gpr[arg0+3],
		     (long)cpu_registers(processor)->gpr[arg0+4],
		     (long)cpu_registers(processor)->gpr[arg0+5]);

  emul_write_status(processor, 0, errno);
}


/* Common code for initializing the system call stuff */

static os_emul_data *
emul_unix_create(device *root,
		 bfd *image,
		 const char *name,
		 emul_syscall *syscall)
{
  unsigned_word top_of_stack;
  unsigned stack_size;
  int elf_binary;
  os_emul_data *data;
  device *vm;
  char *filename;

  /* merge any emulation specific entries into the device tree */

  /* establish a few defaults */
  if (image->xvec->flavour == bfd_target_elf_flavour) {
    elf_binary = 1;
    top_of_stack = 0xe0000000;
    stack_size =   0x00100000;
  }
  else {
    elf_binary = 0;
    top_of_stack = 0x20000000;
    stack_size =   0x00100000;
  }

  /* options */
  emul_add_tree_options(root, image, name,
			(WITH_ENVIRONMENT == USER_ENVIRONMENT
			 ? "user" : "virtual"),
			0 /*oea-interrupt-prefix*/);

  /* virtual memory - handles growth of stack/heap */
  vm = tree_parse(root, "/openprom/vm@0x%lx",
		  (unsigned long)(top_of_stack - stack_size));
  tree_parse(vm, "./stack-base 0x%lx",
	     (unsigned long)(top_of_stack - stack_size));
  tree_parse(vm, "./nr-bytes 0x%x", stack_size);

  filename = tree_quote_property (bfd_get_filename(image));
  tree_parse(root, "/openprom/vm/map-binary/file-name %s",
	     filename);
  free (filename);

  /* finish the init */
  tree_parse(root, "/openprom/init/register/pc 0x%lx",
	     (unsigned long)bfd_get_start_address(image));
  tree_parse(root, "/openprom/init/register/sp 0x%lx",
	     (unsigned long)top_of_stack);
  tree_parse(root, "/openprom/init/register/msr 0x%x",
	     ((tree_find_boolean_property(root, "/options/little-endian?")
	       ? msr_little_endian_mode
	       : 0)
	      | (tree_find_boolean_property(root, "/openprom/options/floating-point?")
		 ? (msr_floating_point_available
		    | msr_floating_point_exception_mode_0
		    | msr_floating_point_exception_mode_1)
		 : 0)));
  tree_parse(root, "/openprom/init/stack/stack-type %s",
	     (elf_binary ? "ppc-elf" : "ppc-xcoff"));

  /* finally our emulation data */
  data = ZALLOC(os_emul_data);
  data->vm = vm;
  data->syscalls = syscall;
  return data;
}


/* EMULATION

   Solaris - Emulation of user programs for Solaris/PPC

   DESCRIPTION

   */


/* Solaris specific implementation */

typedef	signed32	solaris_uid_t;
typedef	signed32	solaris_gid_t;
typedef signed32	solaris_off_t;
typedef signed32	solaris_pid_t;
typedef signed32	solaris_time_t;
typedef unsigned32	solaris_dev_t;
typedef unsigned32	solaris_ino_t;
typedef unsigned32	solaris_mode_t;
typedef	unsigned32	solaris_nlink_t;

#ifdef HAVE_SYS_STAT_H
#define	SOLARIS_ST_FSTYPSZ 16		/* array size for file system type name */

struct solaris_stat {
  solaris_dev_t		st_dev;
  signed32		st_pad1[3];	/* reserved for network id */
  solaris_ino_t		st_ino;
  solaris_mode_t	st_mode;
  solaris_nlink_t 	st_nlink;
  solaris_uid_t 	st_uid;
  solaris_gid_t 	st_gid;
  solaris_dev_t		st_rdev;
  signed32		st_pad2[2];
  solaris_off_t		st_size;
  signed32		st_pad3;	/* future off_t expansion */
  struct unix_timeval	st_atim;
  struct unix_timeval	st_mtim;
  struct unix_timeval	st_ctim;
  signed32		st_blksize;
  signed32		st_blocks;
  char			st_fstype[SOLARIS_ST_FSTYPSZ];
  signed32		st_pad4[8];	/* expansion area */
};

/* Convert from host stat structure to solaris stat structure */
STATIC_INLINE_EMUL_UNIX void
convert_to_solaris_stat(unsigned_word addr,
			struct stat *host,
			cpu *processor,
			unsigned_word cia)
{
  struct solaris_stat target;
  int i;

  target.st_dev   = H2T_4(host->st_dev);
  target.st_ino   = H2T_4(host->st_ino);
  target.st_mode  = H2T_4(host->st_mode);
  target.st_nlink = H2T_4(host->st_nlink);
  target.st_uid   = H2T_4(host->st_uid);
  target.st_gid   = H2T_4(host->st_gid);
  target.st_size  = H2T_4(host->st_size);

#ifdef HAVE_ST_RDEV
  target.st_rdev  = H2T_4(host->st_rdev);
#else
  target.st_rdev  = 0;
#endif

#ifdef HAVE_ST_BLKSIZE
  target.st_blksize = H2T_4(host->st_blksize);
#else
  target.st_blksize = 0;
#endif

#ifdef HAVE_ST_BLOCKS
  target.st_blocks  = H2T_4(host->st_blocks);
#else
  target.st_blocks  = 0;
#endif

  target.st_atim.tv_sec  = H2T_4(host->st_atime);
  target.st_atim.tv_usec = 0;

  target.st_ctim.tv_sec  = H2T_4(host->st_ctime);
  target.st_ctim.tv_usec = 0;

  target.st_mtim.tv_sec  = H2T_4(host->st_mtime);
  target.st_mtim.tv_usec = 0;

  for (i = 0; i < sizeof (target.st_pad1) / sizeof (target.st_pad1[0]); i++)
    target.st_pad1[i] = 0;

  for (i = 0; i < sizeof (target.st_pad2) / sizeof (target.st_pad2[0]); i++)
    target.st_pad2[i] = 0;

  target.st_pad3 = 0;

  for (i = 0; i < sizeof (target.st_pad4) / sizeof (target.st_pad4[0]); i++)
    target.st_pad4[i] = 0;

  /* For now, just punt and always say it is a ufs file */
  strcpy (target.st_fstype, "ufs");

  emul_write_buffer(&target, addr, sizeof(target), processor, cia);
}
#endif /* HAVE_SYS_STAT_H */

#ifndef HAVE_STAT
#define do_solaris_stat 0
#else
static void
do_solaris_stat(os_emul_data *emul,
		unsigned call,
		const int arg0,
		cpu *processor,
		unsigned_word cia)
{
  unsigned_word path_addr = cpu_registers(processor)->gpr[arg0];
  unsigned_word stat_pkt = cpu_registers(processor)->gpr[arg0+1];
  char path_buf[PATH_MAX];
  struct stat buf;
  char *path = emul_read_string(path_buf, path_addr, PATH_MAX, processor, cia);
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("0x%lx [%s], 0x%lx", (long)path_addr, path, (long)stat_pkt);

  status = stat (path, &buf);
  if (status == 0)
    convert_to_solaris_stat (stat_pkt, &buf, processor, cia);

  emul_write_status(processor, status, errno);
}
#endif

#ifndef HAVE_LSTAT
#define do_solaris_lstat 0
#else
static void
do_solaris_lstat(os_emul_data *emul,
		 unsigned call,
		 const int arg0,
		 cpu *processor,
		 unsigned_word cia)
{
  unsigned_word path_addr = cpu_registers(processor)->gpr[arg0];
  unsigned_word stat_pkt = cpu_registers(processor)->gpr[arg0+1];
  char path_buf[PATH_MAX];
  struct stat buf;
  char *path = emul_read_string(path_buf, path_addr, PATH_MAX, processor, cia);
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("0x%lx [%s], 0x%lx", (long)path_addr, path, (long)stat_pkt);

  status = lstat (path, &buf);
  if (status == 0)
    convert_to_solaris_stat (stat_pkt, &buf, processor, cia);

  emul_write_status(processor, status, errno);
}
#endif

#ifndef HAVE_FSTAT
#define do_solaris_fstat 0
#else
static void
do_solaris_fstat(os_emul_data *emul,
		 unsigned call,
		 const int arg0,
		 cpu *processor,
		 unsigned_word cia)
{
  int fildes = (int)cpu_registers(processor)->gpr[arg0];
  unsigned_word stat_pkt = cpu_registers(processor)->gpr[arg0+1];
  struct stat buf;
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("%d, 0x%lx", fildes, (long)stat_pkt);

  status = fstat (fildes, &buf);
  if (status == 0)
    convert_to_solaris_stat (stat_pkt, &buf, processor, cia);

  emul_write_status(processor, status, errno);
}
#endif

#if defined(HAVE_TERMIO_STRUCTURE) || defined(HAVE_TERMIOS_STRUCTURE)
#define	SOLARIS_TIOC	  ('T'<<8)
#define SOLARIS_NCC	  8
#define SOLARIS_NCCS	  19

#define	SOLARIS_VINTR	  0
#define	SOLARIS_VQUIT	  1
#define	SOLARIS_VERASE	  2
#define	SOLARIS_VKILL	  3
#define	SOLARIS_VEOF	  4
#define	SOLARIS_VEOL	  5
#define	SOLARIS_VEOL2	  6
#define	SOLARIS_VSWTCH	  7
#define	SOLARIS_VSTART	  8
#define	SOLARIS_VSTOP	  9
#define	SOLARIS_VSUSP	 10
#define	SOLARIS_VDSUSP	 11
#define	SOLARIS_VREPRINT 12
#define	SOLARIS_VDISCARD 13
#define	SOLARIS_VWERASE	 14
#define	SOLARIS_VLNEXT	 15
#endif

#if defined(HAVE_TERMIO_STRUCTURE) || defined(HAVE_TERMIOS_STRUCTURE)
/* Convert to/from host termio structure */

struct solaris_termio {
	unsigned16	c_iflag;		/* input modes */
	unsigned16	c_oflag;		/* output modes */
	unsigned16	c_cflag;		/* control modes */
	unsigned16	c_lflag;		/* line discipline modes */
	unsigned8	c_line;			/* line discipline */
	unsigned8	c_cc[SOLARIS_NCC];	/* control chars */
};

STATIC_INLINE_EMUL_UNIX void
convert_to_solaris_termio(unsigned_word addr,
			  struct termio *host,
			  cpu *processor,
			  unsigned_word cia)
{
  struct solaris_termio target;
  int i;

  target.c_iflag = H2T_2 (host->c_iflag);
  target.c_oflag = H2T_2 (host->c_oflag);
  target.c_cflag = H2T_2 (host->c_cflag);
  target.c_lflag = H2T_2 (host->c_lflag);

#if defined(HAVE_TERMIO_CLINE) || defined(HAVE_TERMIOS_CLINE)
  target.c_line  = host->c_line;
#else
  target.c_line  = 0;
#endif

  for (i = 0; i < SOLARIS_NCC; i++)
    target.c_cc[i] = 0;

#ifdef VINTR
  target.c_cc[SOLARIS_VINTR] = host->c_cc[VINTR];
#endif

#ifdef VQUIT
  target.c_cc[SOLARIS_VQUIT] = host->c_cc[VQUIT];
#endif

#ifdef VERASE
  target.c_cc[SOLARIS_VERASE] = host->c_cc[VERASE];
#endif

#ifdef VKILL
  target.c_cc[SOLARIS_VKILL] = host->c_cc[VKILL];
#endif

#ifdef VEOF
  target.c_cc[SOLARIS_VEOF] = host->c_cc[VEOF];
#endif

#ifdef VEOL
  target.c_cc[SOLARIS_VEOL] = host->c_cc[VEOL];
#endif

#ifdef VEOL2
  target.c_cc[SOLARIS_VEOL2] = host->c_cc[VEOL2];
#endif

#ifdef VSWTCH
  target.c_cc[SOLARIS_VSWTCH] = host->c_cc[VSWTCH];

#else
#ifdef VSWTC
  target.c_cc[SOLARIS_VSWTCH] = host->c_cc[VSWTC];
#endif
#endif

  emul_write_buffer(&target, addr, sizeof(target), processor, cia);
}
#endif /* HAVE_TERMIO_STRUCTURE || HAVE_TERMIOS_STRUCTURE */

#ifdef HAVE_TERMIOS_STRUCTURE
/* Convert to/from host termios structure */

typedef unsigned32 solaris_tcflag_t;
typedef unsigned8  solaris_cc_t;
typedef unsigned32 solaris_speed_t;

struct solaris_termios {
  solaris_tcflag_t	c_iflag;
  solaris_tcflag_t	c_oflag;
  solaris_tcflag_t	c_cflag;
  solaris_tcflag_t	c_lflag;
  solaris_cc_t		c_cc[SOLARIS_NCCS];
};

STATIC_INLINE_EMUL_UNIX void
convert_to_solaris_termios(unsigned_word addr,
			   struct termios *host,
			   cpu *processor,
			   unsigned_word cia)
{
  struct solaris_termios target;
  int i;

  target.c_iflag = H2T_4 (host->c_iflag);
  target.c_oflag = H2T_4 (host->c_oflag);
  target.c_cflag = H2T_4 (host->c_cflag);
  target.c_lflag = H2T_4 (host->c_lflag);

  for (i = 0; i < SOLARIS_NCCS; i++)
    target.c_cc[i] = 0;

#ifdef VINTR
  target.c_cc[SOLARIS_VINTR] = host->c_cc[VINTR];
#endif

#ifdef VQUIT
  target.c_cc[SOLARIS_VQUIT] = host->c_cc[VQUIT];
#endif

#ifdef VERASE
  target.c_cc[SOLARIS_VERASE] = host->c_cc[VERASE];
#endif

#ifdef VKILL
  target.c_cc[SOLARIS_VKILL] = host->c_cc[VKILL];
#endif

#ifdef VEOF
  target.c_cc[SOLARIS_VEOF] = host->c_cc[VEOF];
#endif

#ifdef VEOL
  target.c_cc[SOLARIS_VEOL] = host->c_cc[VEOL];
#endif

#ifdef VEOL2
  target.c_cc[SOLARIS_VEOL2] = host->c_cc[VEOL2];
#endif

#ifdef VSWTCH
  target.c_cc[SOLARIS_VSWTCH] = host->c_cc[VSWTCH];

#else
#ifdef VSWTC
  target.c_cc[SOLARIS_VSWTCH] = host->c_cc[VSWTC];
#endif
#endif

#ifdef VSTART
  target.c_cc[SOLARIS_VSTART] = host->c_cc[VSTART];
#endif

#ifdef VSTOP
  target.c_cc[SOLARIS_VSTOP] = host->c_cc[VSTOP];
#endif

#ifdef VSUSP
  target.c_cc[SOLARIS_VSUSP] = host->c_cc[VSUSP];
#endif

#ifdef VDSUSP
  target.c_cc[SOLARIS_VDSUSP] = host->c_cc[VDSUSP];
#endif

#ifdef VREPRINT
  target.c_cc[SOLARIS_VREPRINT] = host->c_cc[VREPRINT];
#endif

#ifdef VDISCARD
  target.c_cc[SOLARIS_VDISCARD] = host->c_cc[VDISCARD];
#endif

#ifdef VWERASE
  target.c_cc[SOLARIS_VWERASE] = host->c_cc[VWERASE];
#endif

#ifdef VLNEXT
  target.c_cc[SOLARIS_VLNEXT] = host->c_cc[VLNEXT];
#endif

  emul_write_buffer(&target, addr, sizeof(target), processor, cia);
}
#endif /* HAVE_TERMIOS_STRUCTURE */

#ifndef HAVE_IOCTL
#define do_solaris_ioctl 0
#else
static void
do_solaris_ioctl(os_emul_data *emul,
		 unsigned call,
		 const int arg0,
		 cpu *processor,
		 unsigned_word cia)
{
  int fildes = cpu_registers(processor)->gpr[arg0];
  unsigned request = cpu_registers(processor)->gpr[arg0+1];
  unsigned_word argp_addr = cpu_registers(processor)->gpr[arg0+2];
  int status = 0;
  const char *name = "<unknown>";

#ifdef HAVE_TERMIOS_STRUCTURE
  struct termios host_termio;

#else
#ifdef HAVE_TERMIO_STRUCTURE
  struct termio host_termio;
#endif
#endif

  switch (request)
    {
    case 0:					/* make sure we have at least one case */
    default:
      status = -1;
      errno = EINVAL;
      break;

#if defined(HAVE_TERMIO_STRUCTURE) || defined(HAVE_TERMIOS_STRUCTURE)
#if defined(TCGETA) || defined(TCGETS) || defined(HAVE_TCGETATTR)
    case SOLARIS_TIOC | 1:			/* TCGETA */
      name = "TCGETA";
#ifdef HAVE_TCGETATTR
      status = tcgetattr(fildes, &host_termio);
#elif defined(TCGETS)
      status = ioctl (fildes, TCGETS, &host_termio);
#else
      status = ioctl (fildes, TCGETA, &host_termio);
#endif
      if (status == 0)
	convert_to_solaris_termio (argp_addr, &host_termio, processor, cia);
      break;
#endif /* TCGETA */
#endif /* HAVE_TERMIO_STRUCTURE */

#ifdef HAVE_TERMIOS_STRUCTURE
#if defined(TCGETS) || defined(HAVE_TCGETATTR)
    case SOLARIS_TIOC | 13:			/* TCGETS */
      name = "TCGETS";
#ifdef HAVE_TCGETATTR
      status = tcgetattr(fildes, &host_termio);
#else
      status = ioctl (fildes, TCGETS, &host_termio);
#endif
      if (status == 0)
	convert_to_solaris_termios (argp_addr, &host_termio, processor, cia);
      break;
#endif /* TCGETS */
#endif /* HAVE_TERMIOS_STRUCTURE */
    }

  emul_write_status(processor, status, errno);

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("%d, 0x%x [%s], 0x%lx", fildes, request, name, (long)argp_addr);
}
#endif /* HAVE_IOCTL */

static emul_syscall_descriptor solaris_descriptors[] = {
  /*   0 */ { 0, "syscall" },
  /*   1 */ { do_unix_exit, "exit" },
  /*   2 */ { 0, "fork" },
  /*   3 */ { do_unix_read, "read" },
  /*   4 */ { do_unix_write, "write" },
  /*   5 */ { do_unix_open, "open" },
  /*   6 */ { do_unix_close, "close" },
  /*   7 */ { 0, "wait" },
  /*   8 */ { 0, "creat" },
  /*   9 */ { do_unix_link, "link" },
  /*  10 */ { do_unix_unlink, "unlink" },
  /*  11 */ { 0, "exec" },
  /*  12 */ { do_unix_chdir, "chdir" },
  /*  13 */ { do_unix_time, "time" },
  /*  14 */ { 0, "mknod" },
  /*  15 */ { 0, "chmod" },
  /*  16 */ { 0, "chown" },
  /*  17 */ { do_unix_break, "brk" },
  /*  18 */ { do_solaris_stat, "stat" },
  /*  19 */ { do_unix_lseek, "lseek" },
  /*  20 */ { do_unix_getpid2, "getpid" },
  /*  21 */ { 0, "mount" },
  /*  22 */ { 0, "umount" },
  /*  23 */ { 0, "setuid" },
  /*  24 */ { do_unix_getuid2, "getuid" },
  /*  25 */ { 0, "stime" },
  /*  26 */ { 0, "ptrace" },
  /*  27 */ { 0, "alarm" },
  /*  28 */ { do_solaris_fstat, "fstat" },
  /*  29 */ { 0, "pause" },
  /*  30 */ { 0, "utime" },
  /*  31 */ { 0, "stty" },
  /*  32 */ { 0, "gtty" },
  /*  33 */ { do_unix_access, "access" },
  /*  34 */ { 0, "nice" },
  /*  35 */ { 0, "statfs" },
  /*  36 */ { 0, "sync" },
  /*  37 */ { 0, "kill" },
  /*  38 */ { 0, "fstatfs" },
  /*  39 */ { 0, "pgrpsys" },
  /*  40 */ { 0, "xenix" },
  /*  41 */ { do_unix_dup, "dup" },
  /*  42 */ { 0, "pipe" },
  /*  43 */ { 0, "times" },
  /*  44 */ { 0, "profil" },
  /*  45 */ { 0, "plock" },
  /*  46 */ { 0, "setgid" },
  /*  47 */ { do_unix_getgid2, "getgid" },
  /*  48 */ { 0, "signal" },
  /*  49 */ { 0, "msgsys" },
  /*  50 */ { 0, "syssun" },
  /*  51 */ { 0, "acct" },
  /*  52 */ { 0, "shmsys" },
  /*  53 */ { 0, "semsys" },
  /*  54 */ { do_solaris_ioctl, "ioctl" },
  /*  55 */ { 0, "uadmin" },
  /*  56 */ { 0, 0 /* reserved for exch */ },
  /*  57 */ { 0, "utssys" },
  /*  58 */ { 0, "fdsync" },
  /*  59 */ { 0, "execve" },
  /*  60 */ { do_unix_umask, "umask" },
  /*  61 */ { 0, "chroot" },
  /*  62 */ { 0, "fcntl" },
  /*  63 */ { 0, "ulimit" },
  /*  64 */ { 0, 0 /* reserved for UNIX PC */ },
  /*  64 */ { 0, 0 /* reserved for UNIX PC */ },
  /*  65 */ { 0, 0 /* reserved for UNIX PC */ },
  /*  66 */ { 0, 0 /* reserved for UNIX PC */ },
  /*  67 */ { 0, 0 /* reserved for UNIX PC */ },
  /*  68 */ { 0, 0 /* reserved for UNIX PC */ },
  /*  69 */ { 0, 0 /* reserved for UNIX PC */ },
  /*  70 */ { 0, 0 /* was advfs */ },
  /*  71 */ { 0, 0 /* was unadvfs */ },
  /*  72 */ { 0, 0 /* was rmount */ },
  /*  73 */ { 0, 0 /* was rumount */ },
  /*  74 */ { 0, 0 /* was rfstart */ },
  /*  75 */ { 0, 0 /* was sigret */ },
  /*  76 */ { 0, 0 /* was rdebug */ },
  /*  77 */ { 0, 0 /* was rfstop */ },
  /*  78 */ { 0, 0 /* was rfsys */ },
  /*  79 */ { do_unix_rmdir, "rmdir" },
  /*  80 */ { do_unix_mkdir, "mkdir" },
  /*  81 */ { 0, "getdents" },
  /*  82 */ { 0, 0 /* was libattach */ },
  /*  83 */ { 0, 0 /* was libdetach */ },
  /*  84 */ { 0, "sysfs" },
  /*  85 */ { 0, "getmsg" },
  /*  86 */ { 0, "putmsg" },
  /*  87 */ { 0, "poll" },
  /*  88 */ { do_solaris_lstat, "lstat" },
  /*  89 */ { do_unix_symlink, "symlink" },
  /*  90 */ { 0, "readlink" },
  /*  91 */ { 0, "setgroups" },
  /*  92 */ { 0, "getgroups" },
  /*  93 */ { 0, "fchmod" },
  /*  94 */ { 0, "fchown" },
  /*  95 */ { 0, "sigprocmask" },
  /*  96 */ { 0, "sigsuspend" },
  /*  97 */ { do_unix_nop, "sigaltstack" },
  /*  98 */ { do_unix_nop, "sigaction" },
  /*  99 */ { 0, "sigpending" },
  /* 100 */ { 0, "context" },
  /* 101 */ { 0, "evsys" },
  /* 102 */ { 0, "evtrapret" },
  /* 103 */ { 0, "statvfs" },
  /* 104 */ { 0, "fstatvfs" },
  /* 105 */ { 0, 0 /* reserved */ },
  /* 106 */ { 0, "nfssys" },
  /* 107 */ { 0, "waitsys" },
  /* 108 */ { 0, "sigsendsys" },
  /* 109 */ { 0, "hrtsys" },
  /* 110 */ { 0, "acancel" },
  /* 111 */ { 0, "async" },
  /* 112 */ { 0, "priocntlsys" },
  /* 113 */ { 0, "pathconf" },
  /* 114 */ { 0, "mincore" },
  /* 115 */ { 0, "mmap" },
  /* 116 */ { 0, "mprotect" },
  /* 117 */ { 0, "munmap" },
  /* 118 */ { 0, "fpathconf" },
  /* 119 */ { 0, "vfork" },
  /* 120 */ { 0, "fchdir" },
  /* 121 */ { 0, "readv" },
  /* 122 */ { 0, "writev" },
  /* 123 */ { 0, "xstat" },
  /* 124 */ { 0, "lxstat" },
  /* 125 */ { 0, "fxstat" },
  /* 126 */ { 0, "xmknod" },
  /* 127 */ { 0, "clocal" },
  /* 128 */ { 0, "setrlimit" },
  /* 129 */ { 0, "getrlimit" },
  /* 130 */ { 0, "lchown" },
  /* 131 */ { 0, "memcntl" },
  /* 132 */ { 0, "getpmsg" },
  /* 133 */ { 0, "putpmsg" },
  /* 134 */ { 0, "rename" },
  /* 135 */ { 0, "uname" },
  /* 136 */ { 0, "setegid" },
  /* 137 */ { 0, "sysconfig" },
  /* 138 */ { 0, "adjtime" },
  /* 139 */ { 0, "systeminfo" },
  /* 140 */ { 0, 0 /* reserved */ },
  /* 141 */ { 0, "seteuid" },
  /* 142 */ { 0, "vtrace" },
  /* 143 */ { 0, "fork1" },
  /* 144 */ { 0, "sigtimedwait" },
  /* 145 */ { 0, "lwp_info" },
  /* 146 */ { 0, "yield" },
  /* 147 */ { 0, "lwp_sema_wait" },
  /* 148 */ { 0, "lwp_sema_post" },
  /* 149 */ { 0, 0 /* reserved */ },
  /* 150 */ { 0, 0 /* reserved */ },
  /* 151 */ { 0, 0 /* reserved */ },
  /* 152 */ { 0, "modctl" },
  /* 153 */ { 0, "fchroot" },
  /* 154 */ { 0, "utimes" },
  /* 155 */ { 0, "vhangup" },
  /* 156 */ { do_unix_gettimeofday, "gettimeofday" },
  /* 157 */ { 0, "getitimer" },
  /* 158 */ { 0, "setitimer" },
  /* 159 */ { 0, "lwp_create" },
  /* 160 */ { 0, "lwp_exit" },
  /* 161 */ { 0, "lwp_suspend" },
  /* 162 */ { 0, "lwp_continue" },
  /* 163 */ { 0, "lwp_kill" },
  /* 164 */ { 0, "lwp_self" },
  /* 165 */ { 0, "lwp_setprivate" },
  /* 166 */ { 0, "lwp_getprivate" },
  /* 167 */ { 0, "lwp_wait" },
  /* 168 */ { 0, "lwp_mutex_unlock" },
  /* 169 */ { 0, "lwp_mutex_lock" },
  /* 170 */ { 0, "lwp_cond_wait" },
  /* 171 */ { 0, "lwp_cond_signal" },
  /* 172 */ { 0, "lwp_cond_broadcast" },
  /* 173 */ { 0, "pread" },
  /* 174 */ { 0, "pwrite" },
  /* 175 */ { 0, "llseek" },
  /* 176 */ { 0, "inst_sync" },
  /* 177 */ { 0, 0 /* reserved */ },
  /* 178 */ { 0, "kaio" },
  /* 179 */ { 0, 0 /* reserved */ },
  /* 180 */ { 0, 0 /* reserved */ },
  /* 181 */ { 0, 0 /* reserved */ },
  /* 182 */ { 0, 0 /* reserved */ },
  /* 183 */ { 0, 0 /* reserved */ },
  /* 184 */ { 0, "tsolsys" },
  /* 185 */ { 0, "acl" },
  /* 186 */ { 0, "auditsys" },
  /* 187 */ { 0, "processor_bind" },
  /* 188 */ { 0, "processor_info" },
  /* 189 */ { 0, "p_online" },
  /* 190 */ { 0, "sigqueue" },
  /* 191 */ { 0, "clock_gettime" },
  /* 192 */ { 0, "clock_settime" },
  /* 193 */ { 0, "clock_getres" },
  /* 194 */ { 0, "timer_create" },
  /* 195 */ { 0, "timer_delete" },
  /* 196 */ { 0, "timer_settime" },
  /* 197 */ { 0, "timer_gettime" },
  /* 198 */ { 0, "timer_getoverrun" },
  /* 199 */ { 0, "nanosleep" },
  /* 200 */ { 0, "facl" },
  /* 201 */ { 0, "door" },
  /* 202 */ { 0, "setreuid" },
  /* 203 */ { 0, "setregid" },
  /* 204 */ { 0, "install_utrap" },
  /* 205 */ { 0, 0 /* reserved */ },
  /* 206 */ { 0, 0 /* reserved */ },
  /* 207 */ { 0, 0 /* reserved */ },
  /* 208 */ { 0, 0 /* reserved */ },
  /* 209 */ { 0, 0 /* reserved */ },
  /* 210 */ { 0, "signotifywait" },
  /* 211 */ { 0, "lwp_sigredirect" },
  /* 212 */ { 0, "lwp_alarm" },
};

static char *(solaris_error_names[]) = {
  /*   0 */ "ESUCCESS",
  /*   1 */ "EPERM",
  /*   2 */ "ENOENT",
  /*   3 */ "ESRCH",
  /*   4 */ "EINTR",
  /*   5 */ "EIO",
  /*   6 */ "ENXIO",
  /*   7 */ "E2BIG",
  /*   8 */ "ENOEXEC",
  /*   9 */ "EBADF",
  /*  10 */ "ECHILD",
  /*  11 */ "EAGAIN",
  /*  12 */ "ENOMEM",
  /*  13 */ "EACCES",
  /*  14 */ "EFAULT",
  /*  15 */ "ENOTBLK",
  /*  16 */ "EBUSY",
  /*  17 */ "EEXIST",
  /*  18 */ "EXDEV",
  /*  19 */ "ENODEV",
  /*  20 */ "ENOTDIR",
  /*  21 */ "EISDIR",
  /*  22 */ "EINVAL",
  /*  23 */ "ENFILE",
  /*  24 */ "EMFILE",
  /*  25 */ "ENOTTY",
  /*  26 */ "ETXTBSY",
  /*  27 */ "EFBIG",
  /*  28 */ "ENOSPC",
  /*  29 */ "ESPIPE",
  /*  30 */ "EROFS",
  /*  31 */ "EMLINK",
  /*  32 */ "EPIPE",
  /*  33 */ "EDOM",
  /*  34 */ "ERANGE",
  /*  35 */ "ENOMSG",
  /*  36 */ "EIDRM",
  /*  37 */ "ECHRNG",
  /*  38 */ "EL2NSYNC",
  /*  39 */ "EL3HLT",
  /*  40 */ "EL3RST",
  /*  41 */ "ELNRNG",
  /*  42 */ "EUNATCH",
  /*  43 */ "ENOCSI",
  /*  44 */ "EL2HLT",
  /*  45 */ "EDEADLK",
  /*  46 */ "ENOLCK",
  /*  47 */ "ECANCELED",
  /*  48 */ "ENOTSUP",
  /*  49 */ "EDQUOT",
  /*  50 */ "EBADE",
  /*  51 */ "EBADR",
  /*  52 */ "EXFULL",
  /*  53 */ "ENOANO",
  /*  54 */ "EBADRQC",
  /*  55 */ "EBADSLT",
  /*  56 */ "EDEADLOCK",
  /*  57 */ "EBFONT",
  /*  58 */ "Error code 58",
  /*  59 */ "Error code 59",
  /*  60 */ "ENOSTR",
  /*  61 */ "ENODATA",
  /*  62 */ "ETIME",
  /*  63 */ "ENOSR",
  /*  64 */ "ENONET",
  /*  65 */ "ENOPKG",
  /*  66 */ "EREMOTE",
  /*  67 */ "ENOLINK",
  /*  68 */ "EADV",
  /*  69 */ "ESRMNT",
  /*  70 */ "ECOMM",
  /*  71 */ "EPROTO",
  /*  72 */ "Error code 72",
  /*  73 */ "Error code 73",
  /*  74 */ "EMULTIHOP",
  /*  75 */ "Error code 75",
  /*  76 */ "Error code 76",
  /*  77 */ "EBADMSG",
  /*  78 */ "ENAMETOOLONG",
  /*  79 */ "EOVERFLOW",
  /*  80 */ "ENOTUNIQ",
  /*  81 */ "EBADFD",
  /*  82 */ "EREMCHG",
  /*  83 */ "ELIBACC",
  /*  84 */ "ELIBBAD",
  /*  85 */ "ELIBSCN",
  /*  86 */ "ELIBMAX",
  /*  87 */ "ELIBEXEC",
  /*  88 */ "EILSEQ",
  /*  89 */ "ENOSYS",
  /*  90 */ "ELOOP",
  /*  91 */ "ERESTART",
  /*  92 */ "ESTRPIPE",
  /*  93 */ "ENOTEMPTY",
  /*  94 */ "EUSERS",
  /*  95 */ "ENOTSOCK",
  /*  96 */ "EDESTADDRREQ",
  /*  97 */ "EMSGSIZE",
  /*  98 */ "EPROTOTYPE",
  /*  99 */ "ENOPROTOOPT",
  /* 100 */ "Error code 100",
  /* 101 */ "Error code 101",
  /* 102 */ "Error code 102",
  /* 103 */ "Error code 103",
  /* 104 */ "Error code 104",
  /* 105 */ "Error code 105",
  /* 106 */ "Error code 106",
  /* 107 */ "Error code 107",
  /* 108 */ "Error code 108",
  /* 109 */ "Error code 109",
  /* 110 */ "Error code 110",
  /* 111 */ "Error code 111",
  /* 112 */ "Error code 112",
  /* 113 */ "Error code 113",
  /* 114 */ "Error code 114",
  /* 115 */ "Error code 115",
  /* 116 */ "Error code 116",
  /* 117 */ "Error code 117",
  /* 118 */ "Error code 118",
  /* 119 */ "Error code 119",
  /* 120 */ "EPROTONOSUPPORT",
  /* 121 */ "ESOCKTNOSUPPORT",
  /* 122 */ "EOPNOTSUPP",
  /* 123 */ "EPFNOSUPPORT",
  /* 124 */ "EAFNOSUPPORT",
  /* 125 */ "EADDRINUSE",
  /* 126 */ "EADDRNOTAVAIL",
  /* 127 */ "ENETDOWN",
  /* 128 */ "ENETUNREACH",
  /* 129 */ "ENETRESET",
  /* 130 */ "ECONNABORTED",
  /* 131 */ "ECONNRESET",
  /* 132 */ "ENOBUFS",
  /* 133 */ "EISCONN",
  /* 134 */ "ENOTCONN",
  /* 135 */ "Error code 135",	/* XENIX has 135 - 142 */
  /* 136 */ "Error code 136",
  /* 137 */ "Error code 137",
  /* 138 */ "Error code 138",
  /* 139 */ "Error code 139",
  /* 140 */ "Error code 140",
  /* 141 */ "Error code 141",
  /* 142 */ "Error code 142",
  /* 143 */ "ESHUTDOWN",
  /* 144 */ "ETOOMANYREFS",
  /* 145 */ "ETIMEDOUT",
  /* 146 */ "ECONNREFUSED",
  /* 147 */ "EHOSTDOWN",
  /* 148 */ "EHOSTUNREACH",
  /* 149 */ "EALREADY",
  /* 150 */ "EINPROGRESS",
  /* 151 */ "ESTALE",
};

static char *(solaris_signal_names[]) = {
  /*  0 */ 0,
  /*  1 */ "SIGHUP",
  /*  2 */ "SIGINT",
  /*  3 */ "SIGQUIT",
  /*  4 */ "SIGILL",
  /*  5 */ "SIGTRAP",
  /*  6 */ "SIGABRT",
  /*  7 */ "SIGEMT",
  /*  8 */ "SIGFPE",
  /*  9 */ "SIGKILL",
  /* 10 */ "SIGBUS",
  /* 11 */ "SIGSEGV",
  /* 12 */ "SIGSYS",
  /* 13 */ "SIGPIPE",
  /* 14 */ "SIGALRM",
  /* 15 */ "SIGTERM",
  /* 16 */ "SIGUSR1",
  /* 17 */ "SIGUSR2",
  /* 18 */ "SIGCHLD",
  /* 19 */ "SIGPWR",
  /* 20 */ "SIGWINCH",
  /* 21 */ "SIGURG",
  /* 22 */ "SIGPOLL",
  /* 23 */ "SIGSTOP",
  /* 24 */ "SIGTSTP",
  /* 25 */ "SIGCONT",
  /* 26 */ "SIGTTIN",
  /* 27 */ "SIGTTOU",
  /* 28 */ "SIGVTALRM",
  /* 29 */ "SIGPROF",
  /* 30 */ "SIGXCPU",
  /* 31 */ "SIGXFSZ",
  /* 32 */ "SIGWAITING",
  /* 33 */ "SIGLWP",
  /* 34 */ "SIGFREEZE",
  /* 35 */ "SIGTHAW",
  /* 36 */ "SIGCANCEL",
};

static emul_syscall emul_solaris_syscalls = {
  solaris_descriptors,
  sizeof(solaris_descriptors) / sizeof(solaris_descriptors[0]),
  solaris_error_names,
  sizeof(solaris_error_names) / sizeof(solaris_error_names[0]),
  solaris_signal_names,
  sizeof(solaris_signal_names) / sizeof(solaris_signal_names[0]),
};


/* Solaris's os_emul interface, most are just passed on to the generic
   syscall stuff */

static os_emul_data *
emul_solaris_create(device *root,
		    bfd *image,
		    const char *name)
{
  /* check that this emulation is really for us */
  if (name != NULL && strcmp(name, "solaris") != 0)
    return NULL;

  if (image == NULL)
    return NULL;

  return emul_unix_create(root, image, "solaris", &emul_solaris_syscalls);
}
  
static void
emul_solaris_init(os_emul_data *emul_data,
		  int nr_cpus)
{
  /* nothing yet */
}

static void
emul_solaris_system_call(cpu *processor,
			 unsigned_word cia,
			 os_emul_data *emul_data)
{
  emul_do_system_call(emul_data,
		      emul_data->syscalls,
		      cpu_registers(processor)->gpr[0],
		      3, /*r3 contains arg0*/
		      processor,
		      cia);
}

const os_emul emul_solaris = {
  "solaris",
  emul_solaris_create,
  emul_solaris_init,
  emul_solaris_system_call,
  0, /*instruction_call*/
  0  /*data*/
};


/* EMULATION

   Linux - Emulation of user programs for Linux/PPC

   DESCRIPTION

   */


/* Linux specific implementation */

typedef unsigned32	linux_dev_t;
typedef unsigned32	linux_ino_t;
typedef unsigned32	linux_mode_t;
typedef unsigned16	linux_nlink_t;
typedef signed32	linux_off_t;
typedef signed32	linux_pid_t;
typedef unsigned32	linux_uid_t;
typedef unsigned32	linux_gid_t;
typedef unsigned32	linux_size_t;
typedef signed32	linux_ssize_t;
typedef signed32	linux_ptrdiff_t;
typedef signed32	linux_time_t;
typedef signed32	linux_clock_t;
typedef signed32	linux_daddr_t;

#ifdef HAVE_SYS_STAT_H
/* For the PowerPC, don't both with the 'old' stat structure, since there
   should be no extant binaries with that structure.  */

struct linux_stat {
	linux_dev_t	st_dev;
	linux_ino_t	st_ino;
	linux_mode_t	st_mode;
	linux_nlink_t	st_nlink;
	linux_uid_t 	st_uid;
	linux_gid_t 	st_gid;
	linux_dev_t	st_rdev;
	linux_off_t	st_size;
	unsigned32  	st_blksize;
	unsigned32  	st_blocks;
	unsigned32  	st_atimx;	/* don't use st_{a,c,m}time, that might a macro */
	unsigned32  	__unused1;	/* defined by the host's stat.h */
	unsigned32  	st_mtimx;
	unsigned32  	__unused2;
	unsigned32  	st_ctimx;
	unsigned32  	__unused3;
	unsigned32  	__unused4;
	unsigned32  	__unused5;
};

/* Convert from host stat structure to solaris stat structure */
STATIC_INLINE_EMUL_UNIX void
convert_to_linux_stat(unsigned_word addr,
		      struct stat *host,
		      cpu *processor,
		      unsigned_word cia)
{
  struct linux_stat target;

  target.st_dev   = H2T_4(host->st_dev);
  target.st_ino   = H2T_4(host->st_ino);
  target.st_mode  = H2T_4(host->st_mode);
  target.st_nlink = H2T_2(host->st_nlink);
  target.st_uid   = H2T_4(host->st_uid);
  target.st_gid   = H2T_4(host->st_gid);
  target.st_size  = H2T_4(host->st_size);

#ifdef HAVE_ST_RDEV
  target.st_rdev  = H2T_4(host->st_rdev);
#else
  target.st_rdev  = 0;
#endif

#ifdef HAVE_ST_BLKSIZE
  target.st_blksize = H2T_4(host->st_blksize);
#else
  target.st_blksize = 0;
#endif

#ifdef HAVE_ST_BLOCKS
  target.st_blocks  = H2T_4(host->st_blocks);
#else
  target.st_blocks  = 0;
#endif

  target.st_atimx   = H2T_4(host->st_atime);
  target.st_ctimx   = H2T_4(host->st_ctime);
  target.st_mtimx   = H2T_4(host->st_mtime);
  target.__unused1  = 0;
  target.__unused2  = 0;
  target.__unused3  = 0;
  target.__unused4  = 0;
  target.__unused5  = 0;

  emul_write_buffer(&target, addr, sizeof(target), processor, cia);
}
#endif /* HAVE_SYS_STAT_H */

#ifndef HAVE_STAT
#define do_linux_stat 0
#else
static void
do_linux_stat(os_emul_data *emul,
	      unsigned call,
	      const int arg0,
	      cpu *processor,
	      unsigned_word cia)
{
  unsigned_word path_addr = cpu_registers(processor)->gpr[arg0];
  unsigned_word stat_pkt = cpu_registers(processor)->gpr[arg0+1];
  char path_buf[PATH_MAX];
  struct stat buf;
  char *path = emul_read_string(path_buf, path_addr, PATH_MAX, processor, cia);
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("0x%lx [%s], 0x%lx", (long)path_addr, path, (long)stat_pkt);

  status = stat (path, &buf);
  if (status == 0)
    convert_to_linux_stat (stat_pkt, &buf, processor, cia);

  emul_write_status(processor, status, errno);
}
#endif

#ifndef HAVE_LSTAT
#define do_linux_lstat 0
#else
static void
do_linux_lstat(os_emul_data *emul,
	       unsigned call,
	       const int arg0,
	       cpu *processor,
	       unsigned_word cia)
{
  unsigned_word path_addr = cpu_registers(processor)->gpr[arg0];
  unsigned_word stat_pkt = cpu_registers(processor)->gpr[arg0+1];
  char path_buf[PATH_MAX];
  struct stat buf;
  char *path = emul_read_string(path_buf, path_addr, PATH_MAX, processor, cia);
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("0x%lx [%s], 0x%lx", (long)path_addr, path, (long)stat_pkt);

  status = lstat (path, &buf);
  if (status == 0)
    convert_to_linux_stat (stat_pkt, &buf, processor, cia);

  emul_write_status(processor, status, errno);
}
#endif

#ifndef HAVE_FSTAT
#define do_linux_fstat 0
#else
static void
do_linux_fstat(os_emul_data *emul,
	       unsigned call,
	       const int arg0,
	       cpu *processor,
	       unsigned_word cia)
{
  int fildes = (int)cpu_registers(processor)->gpr[arg0];
  unsigned_word stat_pkt = cpu_registers(processor)->gpr[arg0+1];
  struct stat buf;
  int status;

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("%d, 0x%lx", fildes, (long)stat_pkt);

  status = fstat (fildes, &buf);
  if (status == 0)
    convert_to_linux_stat (stat_pkt, &buf, processor, cia);

  emul_write_status(processor, status, errno);
}
#endif

#if defined(HAVE_TERMIO_STRUCTURE) || defined(HAVE_TERMIOS_STRUCTURE)
#define LINUX_NCC		10
#define LINUX_NCCS		19
	
#define	LINUX_VINTR		 0
#define	LINUX_VQUIT		 1
#define	LINUX_VERASE		 2
#define	LINUX_VKILL		 3
#define	LINUX_VEOF		 4
#define LINUX_VMIN		 5
#define	LINUX_VEOL		 6
#define	LINUX_VTIME		 7
#define LINUX_VEOL2		 8
#define LINUX_VSWTC		 9
#define LINUX_VWERASE 		10
#define LINUX_VREPRINT		11
#define LINUX_VSUSP 		12
#define LINUX_VSTART		13
#define LINUX_VSTOP		14
#define LINUX_VLNEXT		15
#define LINUX_VDISCARD		16
	
#define LINUX_IOC_NRBITS	 8
#define LINUX_IOC_TYPEBITS	 8
#define LINUX_IOC_SIZEBITS	13
#define LINUX_IOC_DIRBITS	 3

#define LINUX_IOC_NRMASK	((1 << LINUX_IOC_NRBITS)-1)
#define LINUX_IOC_TYPEMASK	((1 << LINUX_IOC_TYPEBITS)-1)
#define LINUX_IOC_SIZEMASK	((1 << LINUX_IOC_SIZEBITS)-1)
#define LINUX_IOC_DIRMASK	((1 << LINUX_IOC_DIRBITS)-1)

#define LINUX_IOC_NRSHIFT	0
#define LINUX_IOC_TYPESHIFT	(LINUX_IOC_NRSHIFT+LINUX_IOC_NRBITS)
#define LINUX_IOC_SIZESHIFT	(LINUX_IOC_TYPESHIFT+LINUX_IOC_TYPEBITS)
#define LINUX_IOC_DIRSHIFT	(LINUX_IOC_SIZESHIFT+LINUX_IOC_SIZEBITS)

/*
 * Direction bits _IOC_NONE could be 0, but OSF/1 gives it a bit.
 * And this turns out useful to catch old ioctl numbers in header
 * files for us.
 */
#define LINUX_IOC_NONE		1U
#define LINUX_IOC_READ		2U
#define LINUX_IOC_WRITE		4U

#define LINUX_IOC(dir,type,nr,size) \
	(((dir)  << LINUX_IOC_DIRSHIFT) | \
	 ((type) << LINUX_IOC_TYPESHIFT) | \
	 ((nr)   << LINUX_IOC_NRSHIFT) | \
	 ((size) << LINUX_IOC_SIZESHIFT))

/* used to create numbers */
#define LINUX_IO(type,nr)	 LINUX_IOC(LINUX_IOC_NONE,(type),(nr),0)
#define LINUX_IOR(type,nr,size)	 LINUX_IOC(LINUX_IOC_READ,(type),(nr),sizeof(size))
#define LINUX_IOW(type,nr,size)	 LINUX_IOC(LINUX_IOC_WRITE,(type),(nr),sizeof(size))
#define LINUX_IOWR(type,nr,size) LINUX_IOC(LINUX_IOC_READ|LINUX_IOC_WRITE,(type),(nr),sizeof(size))
#endif

#if defined(HAVE_TERMIO_STRUCTURE) || defined(HAVE_TERMIOS_STRUCTURE)
/* Convert to/from host termio structure */

struct linux_termio {
	unsigned16	c_iflag;		/* input modes */
	unsigned16	c_oflag;		/* output modes */
	unsigned16	c_cflag;		/* control modes */
	unsigned16	c_lflag;		/* line discipline modes */
	unsigned8	c_line;			/* line discipline */
	unsigned8	c_cc[LINUX_NCC];	/* control chars */
};

STATIC_INLINE_EMUL_UNIX void
convert_to_linux_termio(unsigned_word addr,
			struct termio *host,
			cpu *processor,
			unsigned_word cia)
{
  struct linux_termio target;
  int i;

  target.c_iflag = H2T_2 (host->c_iflag);
  target.c_oflag = H2T_2 (host->c_oflag);
  target.c_cflag = H2T_2 (host->c_cflag);
  target.c_lflag = H2T_2 (host->c_lflag);

#if defined(HAVE_TERMIO_CLINE) || defined(HAVE_TERMIOS_CLINE)
  target.c_line  = host->c_line;
#else
  target.c_line  = 0;
#endif

  for (i = 0; i < LINUX_NCC; i++)
    target.c_cc[i] = 0;

#ifdef VINTR
  target.c_cc[LINUX_VINTR] = host->c_cc[VINTR];
#endif

#ifdef VQUIT
  target.c_cc[LINUX_VQUIT] = host->c_cc[VQUIT];
#endif

#ifdef VERASE
  target.c_cc[LINUX_VERASE] = host->c_cc[VERASE];
#endif

#ifdef VKILL
  target.c_cc[LINUX_VKILL] = host->c_cc[VKILL];
#endif

#ifdef VEOF
  target.c_cc[LINUX_VEOF] = host->c_cc[VEOF];
#endif

#ifdef VMIN
  target.c_cc[LINUX_VMIN] = host->c_cc[VMIN];
#endif

#ifdef VEOL
  target.c_cc[LINUX_VEOL] = host->c_cc[VEOL];
#endif

#ifdef VTIME
  target.c_cc[LINUX_VTIME] = host->c_cc[VTIME];
#endif

#ifdef VEOL2
  target.c_cc[LINUX_VEOL2] = host->c_cc[VEOL2];
#endif

#ifdef VSWTC
  target.c_cc[LINUX_VSWTC] = host->c_cc[VSWTC];
#endif

#ifdef VSWTCH
  target.c_cc[LINUX_VSWTC] = host->c_cc[VSWTCH];
#endif

  emul_write_buffer(&target, addr, sizeof(target), processor, cia);
}
#endif /* HAVE_TERMIO_STRUCTURE */

#ifdef HAVE_TERMIOS_STRUCTURE
/* Convert to/from host termios structure */

typedef unsigned32 linux_tcflag_t;
typedef unsigned8  linux_cc_t;
typedef unsigned32 linux_speed_t;

struct linux_termios {
  linux_tcflag_t	c_iflag;
  linux_tcflag_t	c_oflag;
  linux_tcflag_t	c_cflag;
  linux_tcflag_t	c_lflag;
  linux_cc_t		c_cc[LINUX_NCCS];
  linux_cc_t		c_line;
  signed32		c_ispeed;
  signed32		c_ospeed;
};

STATIC_INLINE_EMUL_UNIX void
convert_to_linux_termios(unsigned_word addr,
			 struct termios *host,
			 cpu *processor,
			 unsigned_word cia)
{
  struct linux_termios target;
  int i;

  target.c_iflag = H2T_4 (host->c_iflag);
  target.c_oflag = H2T_4 (host->c_oflag);
  target.c_cflag = H2T_4 (host->c_cflag);
  target.c_lflag = H2T_4 (host->c_lflag);

  for (i = 0; i < LINUX_NCCS; i++)
    target.c_cc[i] = 0;

#ifdef VINTR
  target.c_cc[LINUX_VINTR] = host->c_cc[VINTR];
#endif

#ifdef VQUIT
  target.c_cc[LINUX_VQUIT] = host->c_cc[VQUIT];
#endif

#ifdef VERASE
  target.c_cc[LINUX_VERASE] = host->c_cc[VERASE];
#endif

#ifdef VKILL
  target.c_cc[LINUX_VKILL] = host->c_cc[VKILL];
#endif

#ifdef VEOF
  target.c_cc[LINUX_VEOF] = host->c_cc[VEOF];
#endif

#ifdef VEOL
  target.c_cc[LINUX_VEOL] = host->c_cc[VEOL];
#endif

#ifdef VEOL2
  target.c_cc[LINUX_VEOL2] = host->c_cc[VEOL2];
#endif

#ifdef VSWTCH
  target.c_cc[LINUX_VSWTC] = host->c_cc[VSWTCH];
#endif

#ifdef HAVE_TERMIOS_CLINE
  target.c_line   = host->c_line;
#else
  target.c_line   = 0;
#endif

#ifdef HAVE_CFGETISPEED
  target.c_ispeed = cfgetispeed (host);
#else
  target.c_ispeed = 0;
#endif

#ifdef HAVE_CFGETOSPEED
  target.c_ospeed = cfgetospeed (host);
#else
  target.c_ospeed = 0;
#endif

  emul_write_buffer(&target, addr, sizeof(target), processor, cia);
}
#endif /* HAVE_TERMIOS_STRUCTURE */

#ifndef HAVE_IOCTL
#define do_linux_ioctl 0
#else
static void
do_linux_ioctl(os_emul_data *emul,
	       unsigned call,
	       const int arg0,
	       cpu *processor,
	       unsigned_word cia)
{
  int fildes = cpu_registers(processor)->gpr[arg0];
  unsigned request = cpu_registers(processor)->gpr[arg0+1];
  unsigned_word argp_addr = cpu_registers(processor)->gpr[arg0+2];
  int status = 0;
  const char *name = "<unknown>";

#ifdef HAVE_TERMIOS_STRUCTURE
  struct termios host_termio;

#else
#ifdef HAVE_TERMIO_STRUCTURE
  struct termio host_termio;
#endif
#endif

  switch (request)
    {
    case 0:					/* make sure we have at least one case */
    default:
      status = -1;
      errno = EINVAL;
      break;

#if defined(HAVE_TERMIO_STRUCTURE) || defined(HAVE_TERMIOS_STRUCTURE)
#if defined(TCGETA) || defined(TCGETS) || defined(HAVE_TCGETATTR)
    case LINUX_IOR('t', 23, struct linux_termio):	/* TCGETA */
      name = "TCGETA";
#ifdef HAVE_TCGETATTR
      status = tcgetattr(fildes, &host_termio);
#elif defined(TCGETS)
      status = ioctl (fildes, TCGETS, &host_termio);
#else
      status = ioctl (fildes, TCGETA, &host_termio);
#endif
      if (status == 0)
	convert_to_linux_termio (argp_addr, &host_termio, processor, cia);
      break;
#endif /* TCGETA */
#endif /* HAVE_TERMIO_STRUCTURE */

#ifdef HAVE_TERMIOS_STRUCTURE
#if defined(TCGETS) || defined(HAVE_TCGETATTR)
    case LINUX_IOR('t', 19, struct linux_termios):	/* TCGETS */
      name = "TCGETS";
#ifdef HAVE_TCGETATTR
      status = tcgetattr(fildes, &host_termio);
#else
      status = ioctl (fildes, TCGETS, &host_termio);
#endif
      if (status == 0)
	convert_to_linux_termios (argp_addr, &host_termio, processor, cia);
      break;
#endif /* TCGETS */
#endif /* HAVE_TERMIOS_STRUCTURE */
    }

  emul_write_status(processor, status, errno);

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    printf_filtered ("%d, 0x%x [%s], 0x%lx", fildes, request, name, (long)argp_addr);
}
#endif /* HAVE_IOCTL */

static emul_syscall_descriptor linux_descriptors[] = {
  /*   0 */ { 0, "setup" },
  /*   1 */ { do_unix_exit, "exit" },
  /*   2 */ { 0, "fork" },
  /*   3 */ { do_unix_read, "read" },
  /*   4 */ { do_unix_write, "write" },
  /*   5 */ { do_unix_open, "open" },
  /*   6 */ { do_unix_close, "close" },
  /*   7 */ { 0, "waitpid" },
  /*   8 */ { 0, "creat" },
  /*   9 */ { do_unix_link, "link" },
  /*  10 */ { do_unix_unlink, "unlink" },
  /*  11 */ { 0, "execve" },
  /*  12 */ { do_unix_chdir, "chdir" },
  /*  13 */ { do_unix_time, "time" },
  /*  14 */ { 0, "mknod" },
  /*  15 */ { 0, "chmod" },
  /*  16 */ { 0, "chown" },
  /*  17 */ { 0, "break" },
  /*  18 */ { 0, "stat" },
  /*  19 */ { do_unix_lseek, "lseek" },
  /*  20 */ { do_unix_getpid, "getpid" },
  /*  21 */ { 0, "mount" },
  /*  22 */ { 0, "umount" },
  /*  23 */ { 0, "setuid" },
  /*  24 */ { do_unix_getuid, "getuid" },
  /*  25 */ { 0, "stime" },
  /*  26 */ { 0, "ptrace" },
  /*  27 */ { 0, "alarm" },
  /*  28 */ { 0, "fstat" },
  /*  29 */ { 0, "pause" },
  /*  30 */ { 0, "utime" },
  /*  31 */ { 0, "stty" },
  /*  32 */ { 0, "gtty" },
  /*  33 */ { do_unix_access, "access" },
  /*  34 */ { 0, "nice" },
  /*  35 */ { 0, "ftime" },
  /*  36 */ { 0, "sync" },
  /*  37 */ { 0, "kill" },
  /*  38 */ { 0, "rename" },
  /*  39 */ { do_unix_mkdir, "mkdir" },
  /*  40 */ { do_unix_rmdir, "rmdir" },
  /*  41 */ { do_unix_dup, "dup" },
  /*  42 */ { 0, "pipe" },
  /*  43 */ { 0, "times" },
  /*  44 */ { 0, "prof" },
  /*  45 */ { do_unix_break, "brk" },
  /*  46 */ { 0, "setgid" },
  /*  47 */ { do_unix_getgid, "getgid" },
  /*  48 */ { 0, "signal" },
  /*  49 */ { do_unix_geteuid, "geteuid" },
  /*  50 */ { do_unix_getegid, "getegid" },
  /*  51 */ { 0, "acct" },
  /*  52 */ { 0, "phys" },
  /*  53 */ { 0, "lock" },
  /*  54 */ { do_linux_ioctl, "ioctl" },
  /*  55 */ { 0, "fcntl" },
  /*  56 */ { 0, "mpx" },
  /*  57 */ { 0, "setpgid" },
  /*  58 */ { 0, "ulimit" },
  /*  59 */ { 0, "olduname" },
  /*  60 */ { do_unix_umask, "umask" },
  /*  61 */ { 0, "chroot" },
  /*  62 */ { 0, "ustat" },
  /*  63 */ { do_unix_dup2, "dup2" },
  /*  64 */ { do_unix_getppid, "getppid" },
  /*  65 */ { 0, "getpgrp" },
  /*  66 */ { 0, "setsid" },
  /*  67 */ { 0, "sigaction" },
  /*  68 */ { 0, "sgetmask" },
  /*  69 */ { 0, "ssetmask" },
  /*  70 */ { 0, "setreuid" },
  /*  71 */ { 0, "setregid" },
  /*  72 */ { 0, "sigsuspend" },
  /*  73 */ { 0, "sigpending" },
  /*  74 */ { 0, "sethostname" },
  /*  75 */ { 0, "setrlimit" },
  /*  76 */ { 0, "getrlimit" },
  /*  77 */ { do_unix_getrusage, "getrusage" },
  /*  78 */ { do_unix_gettimeofday, "gettimeofday" },
  /*  79 */ { 0, "settimeofday" },
  /*  80 */ { 0, "getgroups" },
  /*  81 */ { 0, "setgroups" },
  /*  82 */ { 0, "select" },
  /*  83 */ { do_unix_symlink, "symlink" },
  /*  84 */ { 0, "lstat" },
  /*  85 */ { 0, "readlink" },
  /*  86 */ { 0, "uselib" },
  /*  87 */ { 0, "swapon" },
  /*  88 */ { 0, "reboot" },
  /*  89 */ { 0, "readdir" },
  /*  90 */ { 0, "mmap" },
  /*  91 */ { 0, "munmap" },
  /*  92 */ { 0, "truncate" },
  /*  93 */ { 0, "ftruncate" },
  /*  94 */ { 0, "fchmod" },
  /*  95 */ { 0, "fchown" },
  /*  96 */ { 0, "getpriority" },
  /*  97 */ { 0, "setpriority" },
  /*  98 */ { 0, "profil" },
  /*  99 */ { 0, "statfs" },
  /* 100 */ { 0, "fstatfs" },
  /* 101 */ { 0, "ioperm" },
  /* 102 */ { 0, "socketcall" },
  /* 103 */ { 0, "syslog" },
  /* 104 */ { 0, "setitimer" },
  /* 105 */ { 0, "getitimer" },
  /* 106 */ { do_linux_stat, "newstat" },
  /* 107 */ { do_linux_lstat, "newlstat" },
  /* 108 */ { do_linux_fstat, "newfstat" },
  /* 109 */ { 0, "uname" },
  /* 110 */ { 0, "iopl" },
  /* 111 */ { 0, "vhangup" },
  /* 112 */ { 0, "idle" },
  /* 113 */ { 0, "vm86" },
  /* 114 */ { 0, "wait4" },
  /* 115 */ { 0, "swapoff" },
  /* 116 */ { 0, "sysinfo" },
  /* 117 */ { 0, "ipc" },
  /* 118 */ { 0, "fsync" },
  /* 119 */ { 0, "sigreturn" },
  /* 120 */ { 0, "clone" },
  /* 121 */ { 0, "setdomainname" },
  /* 122 */ { 0, "newuname" },
  /* 123 */ { 0, "modify_ldt" },
  /* 124 */ { 0, "adjtimex" },
  /* 125 */ { 0, "mprotect" },
  /* 126 */ { 0, "sigprocmask" },
  /* 127 */ { 0, "create_module" },
  /* 128 */ { 0, "init_module" },
  /* 129 */ { 0, "delete_module" },
  /* 130 */ { 0, "get_kernel_syms" },
  /* 131 */ { 0, "quotactl" },
  /* 132 */ { 0, "getpgid" },
  /* 133 */ { 0, "fchdir" },
  /* 134 */ { 0, "bdflush" },
  /* 135 */ { 0, "sysfs" },
  /* 136 */ { 0, "personality" },
  /* 137 */ { 0, "afs_syscall" },
  /* 138 */ { 0, "setfsuid" },
  /* 139 */ { 0, "setfsgid" },
  /* 140 */ { 0, "llseek" },
  /* 141 */ { 0, "getdents" },
  /* 142 */ { 0, "newselect" },
  /* 143 */ { 0, "flock" },
  /* 144 */ { 0, "msync" },
  /* 145 */ { 0, "readv" },
  /* 146 */ { 0, "writev" },
  /* 147 */ { 0, "getsid" },
  /* 148 */ { 0, "fdatasync" },
  /* 149 */ { 0, "sysctl" },
  /* 150 */ { 0, "mlock" },
  /* 151 */ { 0, "munlock" },
  /* 152 */ { 0, "mlockall" },
  /* 153 */ { 0, "munlockall" },
  /* 154 */ { 0, "sched_setparam" },
  /* 155 */ { 0, "sched_getparam" },
  /* 156 */ { 0, "sched_setscheduler" },
  /* 157 */ { 0, "sched_getscheduler" },
  /* 158 */ { 0, "sched_yield" },
  /* 159 */ { 0, "sched_get_priority_max" },
  /* 160 */ { 0, "sched_get_priority_min" },
  /* 161 */ { 0, "sched_rr_get_interval" },
};

static char *(linux_error_names[]) = {
  /*   0 */ "ESUCCESS",
  /*   1 */ "EPERM",
  /*   2 */ "ENOENT",
  /*   3 */ "ESRCH",
  /*   4 */ "EINTR",
  /*   5 */ "EIO",
  /*   6 */ "ENXIO",
  /*   7 */ "E2BIG",
  /*   8 */ "ENOEXEC",
  /*   9 */ "EBADF",
  /*  10 */ "ECHILD",
  /*  11 */ "EAGAIN",
  /*  12 */ "ENOMEM",
  /*  13 */ "EACCES",
  /*  14 */ "EFAULT",
  /*  15 */ "ENOTBLK",
  /*  16 */ "EBUSY",
  /*  17 */ "EEXIST",
  /*  18 */ "EXDEV",
  /*  19 */ "ENODEV",
  /*  20 */ "ENOTDIR",
  /*  21 */ "EISDIR",
  /*  22 */ "EINVAL",
  /*  23 */ "ENFILE",
  /*  24 */ "EMFILE",
  /*  25 */ "ENOTTY",
  /*  26 */ "ETXTBSY",
  /*  27 */ "EFBIG",
  /*  28 */ "ENOSPC",
  /*  29 */ "ESPIPE",
  /*  30 */ "EROFS",
  /*  31 */ "EMLINK",
  /*  32 */ "EPIPE",
  /*  33 */ "EDOM",
  /*  34 */ "ERANGE",
  /*  35 */ "EDEADLK",
  /*  36 */ "ENAMETOOLONG",
  /*  37 */ "ENOLCK",
  /*  38 */ "ENOSYS",
  /*  39 */ "ENOTEMPTY",
  /*  40 */ "ELOOP",
  /*  41 */ 0,
  /*  42 */ "ENOMSG",
  /*  43 */ "EIDRM",
  /*  44 */ "ECHRNG",
  /*  45 */ "EL2NSYNC",
  /*  46 */ "EL3HLT",
  /*  47 */ "EL3RST",
  /*  48 */ "ELNRNG",
  /*  49 */ "EUNATCH",
  /*  50 */ "ENOCSI",
  /*  51 */ "EL2HLT",
  /*  52 */ "EBADE",
  /*  53 */ "EBADR",
  /*  54 */ "EXFULL",
  /*  55 */ "ENOANO",
  /*  56 */ "EBADRQC",
  /*  57 */ "EBADSLT",
  /*  58 */ "EDEADLOCK",
  /*  59 */ "EBFONT",
  /*  60 */ "ENOSTR",
  /*  61 */ "ENODATA",
  /*  62 */ "ETIME",
  /*  63 */ "ENOSR",
  /*  64 */ "ENONET",
  /*  65 */ "ENOPKG",
  /*  66 */ "EREMOTE",
  /*  67 */ "ENOLINK",
  /*  68 */ "EADV",
  /*  69 */ "ESRMNT",
  /*  70 */ "ECOMM",
  /*  71 */ "EPROTO",
  /*  72 */ "EMULTIHOP",
  /*  73 */ "EDOTDOT",
  /*  74 */ "EBADMSG",
  /*  75 */ "EOVERFLOW",
  /*  76 */ "ENOTUNIQ",
  /*  77 */ "EBADFD",
  /*  78 */ "EREMCHG",
  /*  79 */ "ELIBACC",
  /*  80 */ "ELIBBAD",
  /*  81 */ "ELIBSCN",
  /*  82 */ "ELIBMAX",
  /*  83 */ "ELIBEXEC",
  /*  84 */ "EILSEQ",
  /*  85 */ "ERESTART",
  /*  86 */ "ESTRPIPE",
  /*  87 */ "EUSERS",
  /*  88 */ "ENOTSOCK",
  /*  89 */ "EDESTADDRREQ",
  /*  90 */ "EMSGSIZE",
  /*  91 */ "EPROTOTYPE",
  /*  92 */ "ENOPROTOOPT",
  /*  93 */ "EPROTONOSUPPORT",
  /*  94 */ "ESOCKTNOSUPPORT",
  /*  95 */ "EOPNOTSUPP",
  /*  96 */ "EPFNOSUPPORT",
  /*  97 */ "EAFNOSUPPORT",
  /*  98 */ "EADDRINUSE",
  /*  99 */ "EADDRNOTAVAIL",
  /* 100 */ "ENETDOWN",
  /* 101 */ "ENETUNREACH",
  /* 102 */ "ENETRESET",
  /* 103 */ "ECONNABORTED",
  /* 104 */ "ECONNRESET",
  /* 105 */ "ENOBUFS",
  /* 106 */ "EISCONN",
  /* 107 */ "ENOTCONN",
  /* 108 */ "ESHUTDOWN",
  /* 109 */ "ETOOMANYREFS",
  /* 110 */ "ETIMEDOUT",
  /* 111 */ "ECONNREFUSED",
  /* 112 */ "EHOSTDOWN",
  /* 113 */ "EHOSTUNREACH",
  /* 114 */ "EALREADY",
  /* 115 */ "EINPROGRESS",
  /* 116 */ "ESTALE",
  /* 117 */ "EUCLEAN",
  /* 118 */ "ENOTNAM",
  /* 119 */ "ENAVAIL",
  /* 120 */ "EISNAM",
  /* 121 */ "EREMOTEIO",
  /* 122 */ "EDQUOT",
};

static char *(linux_signal_names[]) = {
  /*  0 */ 0,
  /*  1 */ "SIGHUP",
  /*  2 */ "SIGINT",
  /*  3 */ "SIGQUIT",
  /*  4 */ "SIGILL",
  /*  5 */ "SIGTRAP",
  /*  6 */ "SIGABRT",
  /*  6 */ "SIGIOT",
  /*  7 */ "SIGBUS",
  /*  8 */ "SIGFPE",
  /*  9 */ "SIGKILL",
  /* 10 */ "SIGUSR1",
  /* 11 */ "SIGSEGV",
  /* 12 */ "SIGUSR2",
  /* 13 */ "SIGPIPE",
  /* 14 */ "SIGALRM",
  /* 15 */ "SIGTERM",
  /* 16 */ "SIGSTKFLT",
  /* 17 */ "SIGCHLD",
  /* 18 */ "SIGCONT",
  /* 19 */ "SIGSTOP",
  /* 20 */ "SIGTSTP",
  /* 21 */ "SIGTTIN",
  /* 22 */ "SIGTTOU",
  /* 23 */ "SIGURG",
  /* 24 */ "SIGXCPU",
  /* 25 */ "SIGXFSZ",
  /* 26 */ "SIGVTALRM",
  /* 27 */ "SIGPROF",
  /* 28 */ "SIGWINCH",
  /* 29 */ "SIGIO",
  /* 30 */ "SIGPWR",
  /* 31 */ "SIGUNUSED",
};

static emul_syscall emul_linux_syscalls = {
  linux_descriptors,
  sizeof(linux_descriptors) / sizeof(linux_descriptors[0]),
  linux_error_names,
  sizeof(linux_error_names) / sizeof(linux_error_names[0]),
  linux_signal_names,
  sizeof(linux_signal_names) / sizeof(linux_signal_names[0]),
};


/* Linux's os_emul interface, most are just passed on to the generic
   syscall stuff */

static os_emul_data *
emul_linux_create(device *root,
		  bfd *image,
		  const char *name)
{
  /* check that this emulation is really for us */
  if (name != NULL && strcmp(name, "linux") != 0)
    return NULL;

  if (image == NULL)
    return NULL;

  return emul_unix_create(root, image, "linux", &emul_linux_syscalls);
}

static void
emul_linux_init(os_emul_data *emul_data,
		int nr_cpus)
{
  /* nothing yet */
}

static void
emul_linux_system_call(cpu *processor,
		       unsigned_word cia,
		       os_emul_data *emul_data)
{
  emul_do_system_call(emul_data,
		      emul_data->syscalls,
		      cpu_registers(processor)->gpr[0],
		      3, /*r3 contains arg0*/
		      processor,
		      cia);
}

const os_emul emul_linux = {
  "linux",
  emul_linux_create,
  emul_linux_init,
  emul_linux_system_call,
  0, /*instruction_call*/
  0  /*data*/
};

#endif /* _EMUL_UNIX_C_ */
