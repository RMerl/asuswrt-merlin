/* Copyright (C) 1995, 96, 97, 98, 99, 2000, 2001 Free Software Foundation, Inc.
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

#ifndef _SYS_IPC_H
# error "Never use <bits/ipc.h> directly; include <sys/ipc.h> instead."
#endif

#include <bits/types.h>

/* Mode bits for `msgget', `semget', and `shmget'.  */
#define IPC_CREAT	01000		/* Create key if key does not exist. */
#define IPC_EXCL	02000		/* Fail if key exists.	*/
#define IPC_NOWAIT	04000		/* Return error on wait.  */

/* Control commands for `msgctl', `semctl', and `shmctl'.  */
#define IPC_RMID	0		/* Remove identifier.  */
#define IPC_SET		1		/* Set `ipc_perm' options.  */
#define IPC_STAT	2		/* Get `ipc_perm' options.  */
#ifdef __USE_GNU
# define IPC_INFO	3		/* See ipcs.  */
#endif

/* Special key values.	*/
#define IPC_PRIVATE	((__key_t) 0)	/* Private key.	 */


/* Data structure used to pass permission information to IPC operations.  */
struct ipc_perm
  {
    __key_t __key;			/* Key.  */
    unsigned int uid;			/* Owner's user ID.  */
    unsigned int gid;			/* Owner's group ID.  */
    unsigned int cuid;			/* Creator's user ID.  */
    unsigned int cgid;			/* Creator's group ID.	*/
    unsigned int mode;			/* Read/write permission.  */
    unsigned short int __seq;		/* Sequence number.  */
    unsigned short int __pad1;
    unsigned long int __unused1;
    unsigned long int __unused2;
};
