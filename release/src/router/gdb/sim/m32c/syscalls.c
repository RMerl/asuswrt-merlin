/* syscalls.c --- implement system calls for the M32C simulator.

Copyright (C) 2005, 2007 Free Software Foundation, Inc.
Contributed by Red Hat, Inc.

This file is part of the GNU simulators.

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


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#include "gdb/callback.h"

#include "cpu.h"
#include "mem.h"
#include "syscalls.h"

#include "syscall.h"

/* The current syscall callbacks we're using.  */
static struct host_callback_struct *callbacks;

void
set_callbacks (struct host_callback_struct *cb)
{
  callbacks = cb;
}


/* A16 ABI: arg1 in r1l (QI) or r1 (HI) or stack
            arg2 in r2 (HI) or stack
	    arg3..N on stack
	    padding: none

   A24 ABI: arg1 in r0l (QI) or r0 (HI) or stack
	    arg2..N on stack
	    padding: qi->hi

   return value in r0l (QI) r0 (HI) r2r0 (SI)
     structs: pointer pushed on stack last

*/

int argp, stackp;

static int
arg (int bytes)
{
  int rv = 0;
  argp++;
  if (A16)
    {
      switch (argp)
	{
	case 1:
	  if (bytes == 1)
	    return get_reg (r1l);
	  if (bytes == 2)
	    return get_reg (r1);
	  break;
	case 2:
	  if (bytes == 2)
	    return get_reg (r2);
	  break;
	}
    }
  else
    {
      switch (argp)
	{
	case 1:
	  if (bytes == 1)
	    return get_reg (r0l);
	  if (bytes == 2)
	    return get_reg (r0);
	  break;
	}
    }
  if (bytes == 0)
    bytes = 2;
  switch (bytes)
    {
    case 1:
      rv = mem_get_qi (get_reg (sp) + stackp);
      if (A24)
	stackp++;
      break;
    case 2:
      rv = mem_get_hi (get_reg (sp) + stackp);
      break;
    case 3:
      rv = mem_get_psi (get_reg (sp) + stackp);
      if (A24)
	stackp++;
      break;
    case 4:
      rv = mem_get_si (get_reg (sp) + stackp);
      break;
    }
  stackp += bytes;
  return rv;
}

static void
read_target (char *buffer, int address, int count, int asciiz)
{
  char byte;
  while (count > 0)
    {
      byte = mem_get_qi (address++);
      *buffer++ = byte;
      if (asciiz && (byte == 0))
	return;
      count--;
    }
}

static void
write_target (char *buffer, int address, int count, int asciiz)
{
  char byte;
  while (count > 0)
    {
      byte = *buffer++;
      mem_put_qi (address++, byte);
      if (asciiz && (byte == 0))
	return;
      count--;
    }
}

#define PTRSZ (A16 ? 2 : 3)

static char *callnames[] = {
  "SYS_zero",
  "SYS_exit",
  "SYS_open",
  "SYS_close",
  "SYS_read",
  "SYS_write",
  "SYS_lseek",
  "SYS_unlink",
  "SYS_getpid",
  "SYS_kill",
  "SYS_fstat",
  "SYS_sbrk",
  "SYS_argvlen",
  "SYS_argv",
  "SYS_chdir",
  "SYS_stat",
  "SYS_chmod",
  "SYS_utime",
  "SYS_time",
  "SYS_gettimeofday",
  "SYS_times",
  "SYS_link"
};

void
m32c_syscall (int id)
{
  static char buf[256];
  int rv;

  argp = 0;
  stackp = A16 ? 3 : 4;
  if (trace)
    printf ("\033[31m/* SYSCALL(%d) = %s */\033[0m\n", id, callnames[id]);
  switch (id)
    {
    case SYS_exit:
      {
	int ec = arg (2);
	if (verbose)
	  printf ("[exit %d]\n", ec);
	step_result = M32C_MAKE_EXITED (ec);
      }
      break;

    case SYS_open:
      {
	int path = arg (PTRSZ);
	int oflags = arg (2);
	int cflags = arg (2);

	read_target (buf, path, 256, 1);
	if (trace)
	  printf ("open(\"%s\",0x%x,%#o) = ", buf, oflags, cflags);

	if (callbacks)
	  /* The callback vector ignores CFLAGS.  */
	  rv = callbacks->open (callbacks, buf, oflags);
	else
	  {
	    int h_oflags = 0;

	    if (oflags & 0x0001)
	      h_oflags |= O_WRONLY;
	    if (oflags & 0x0002)
	      h_oflags |= O_RDWR;
	    if (oflags & 0x0200)
	      h_oflags |= O_CREAT;
	    if (oflags & 0x0008)
	      h_oflags |= O_APPEND;
	    if (oflags & 0x0400)
	      h_oflags |= O_TRUNC;
	    rv = open (buf, h_oflags, cflags);
	  }
	if (trace)
	  printf ("%d\n", rv);
	put_reg (r0, rv);
      }
      break;

    case SYS_close:
      {
	int fd = arg (2);

	if (callbacks)
	  rv = callbacks->close (callbacks, fd);
	else if (fd > 2)
	  rv = close (fd);
	else
	  rv = 0;
	if (trace)
	  printf ("close(%d) = %d\n", fd, rv);
	put_reg (r0, rv);
      }
      break;

    case SYS_read:
      {
	int fd = arg (2);
	int addr = arg (PTRSZ);
	int count = arg (2);

	if (count > sizeof (buf))
	  count = sizeof (buf);
	if (callbacks)
	  rv = callbacks->read (callbacks, fd, buf, count);
	else
	  rv = read (fd, buf, count);
	if (trace)
	  printf ("read(%d,%d) = %d\n", fd, count, rv);
	if (rv > 0)
	  write_target (buf, addr, rv, 0);
	put_reg (r0, rv);
      }
      break;

    case SYS_write:
      {
	int fd = arg (2);
	int addr = arg (PTRSZ);
	int count = arg (2);

	if (count > sizeof (buf))
	  count = sizeof (buf);
	if (trace)
	  printf ("write(%d,0x%x,%d)\n", fd, addr, count);
	read_target (buf, addr, count, 0);
	if (trace)
	  fflush (stdout);
	if (callbacks)
	  rv = callbacks->write (callbacks, fd, buf, count);
	else
	  rv = write (fd, buf, count);
	if (trace)
	  printf ("write(%d,%d) = %d\n", fd, count, rv);
	put_reg (r0, rv);
      }
      break;

    case SYS_getpid:
      put_reg (r0, 42);
      break;

    case SYS_gettimeofday:
      {
	int tvaddr = arg (PTRSZ);
	struct timeval tv;

	rv = gettimeofday (&tv, 0);
	if (trace)
	  printf ("gettimeofday: %ld sec %ld usec to 0x%x\n", tv.tv_sec,
		  tv.tv_usec, tvaddr);
	mem_put_si (tvaddr, tv.tv_sec);
	mem_put_si (tvaddr + 4, tv.tv_usec);
	put_reg (r0, rv);
      }
      break;

    case SYS_kill:
      {
	int pid = arg (2);
	int sig = arg (2);
	if (pid == 42)
	  {
	    if (verbose)
	      printf ("[signal %d]\n", sig);
	    step_result = M32C_MAKE_STOPPED (sig);
	  }
      }
      break;

    case 11:
      {
	int heaptop_arg = arg (PTRSZ);
	if (trace)
	  printf ("sbrk: heap top set to %x\n", heaptop_arg);
	heaptop = heaptop_arg;
	if (heapbottom == 0)
	  heapbottom = heaptop_arg;
      }
      break;

    }
}
