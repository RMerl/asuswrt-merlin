/* Copyright (C) 1996, 1999, 2001 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _SYS_SYSINFO_H
#define _SYS_SYSINFO_H	1

#include <features.h>

#ifndef _LINUX_KERNEL_H
/* Include our own copy of struct sysinfo to avoid binary compatability
 * problems with Linux 2.4, which changed things.  Grumble, grumble. */
#define SI_LOAD_SHIFT	16
struct sysinfo {
	long uptime;			/* Seconds since boot */
	unsigned long loads[3];		/* 1, 5, and 15 minute load averages */
	unsigned long totalram;		/* Total usable main memory size */
	unsigned long freeram;		/* Available memory size */
	unsigned long sharedram;	/* Amount of shared memory */
	unsigned long bufferram;	/* Memory used by buffers */
	unsigned long totalswap;	/* Total swap space size */
	unsigned long freeswap;		/* swap space still available */
	unsigned short procs;		/* Number of current processes */
	unsigned short pad;			/* Padding needed for m68k */
	unsigned long totalhigh;	/* Total high memory size */
	unsigned long freehigh;		/* Available high memory size */
	unsigned int mem_unit;		/* Memory unit size in bytes */
	char _f[20-2*sizeof(long)-sizeof(int)];	/* Padding: libc5 uses this.. */
};
#endif

__BEGIN_DECLS

/* Returns information on overall system statistics.  */
extern int sysinfo (struct sysinfo *__info) __THROW;


/* Return number of configured processors.  */
extern int get_nprocs_conf (void) __THROW;

/* Return number of available processors.  */
extern int get_nprocs (void) __THROW;


/* Return number of physical pages of memory in the system.  */
extern long int get_phys_pages (void) __THROW;

/* Return number of available physical pages of memory in the system.  */
extern long int get_avphys_pages (void) __THROW;

__END_DECLS

#endif	/* sys/sysinfo.h */
