#ifndef _system_shmem_h
#define _system_shmem_h
/* 
   Unix SMB/CIFS implementation.

   shared memory system include wrappers

   Copyright (C) Andrew Tridgell 2004
   
     ** NOTE! The following LGPL license applies to the replace
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.

*/

#if defined(HAVE_SYS_IPC_H)
#include <sys/ipc.h>
#endif /* HAVE_SYS_IPC_H */

#if defined(HAVE_SYS_SHM_H)
#include <sys/shm.h>
#endif /* HAVE_SYS_SHM_H */

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

/* NetBSD doesn't have these */
#ifndef SHM_R
#define SHM_R 0400
#endif

#ifndef SHM_W
#define SHM_W 0200
#endif


#ifndef MAP_FILE
#define MAP_FILE 0
#endif

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

#endif
