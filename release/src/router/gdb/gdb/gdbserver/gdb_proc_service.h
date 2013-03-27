/* <proc_service.h> replacement for systems that don't have it.
   Copyright (C) 2000, 2006, 2007 Free Software Foundation, Inc.

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

#ifndef GDB_PROC_SERVICE_H
#define GDB_PROC_SERVICE_H

#include <sys/types.h>

#ifdef HAVE_PROC_SERVICE_H
#include <proc_service.h>
#else

#ifdef HAVE_SYS_PROCFS_H
#include <sys/procfs.h>
#endif

/* Not all platforms bring in <linux/elf.h> via <sys/procfs.h>.  If
   <sys/procfs.h> wasn't enough to find elf_fpregset_t, try the kernel
   headers also (but don't if we don't need to).  */
#ifndef HAVE_ELF_FPREGSET_T
# ifdef HAVE_LINUX_ELF_H
#  include <linux/elf.h>
# endif
#endif

typedef enum
{
  PS_OK,			/* Success.  */
  PS_ERR,			/* Generic error.  */
  PS_BADPID,			/* Bad process handle.  */
  PS_BADLID,			/* Bad LWP id.  */
  PS_BADADDR,			/* Bad address.  */
  PS_NOSYM,			/* Symbol not found.  */
  PS_NOFREGS			/* FPU register set not available.  */
} ps_err_e;

#ifndef HAVE_LWPID_T
typedef unsigned int lwpid_t;
#endif

#ifndef HAVE_PSADDR_T
typedef unsigned long psaddr_t;
#endif

#ifndef HAVE_PRGREGSET_T
typedef elf_gregset_t prgregset_t;
#endif

#endif /* HAVE_PROC_SERVICE_H */

/* Structure that identifies the target process.  */
struct ps_prochandle
{
  /* The process id is all we need.  */
  pid_t pid;
};

#endif /* gdb_proc_service.h */
