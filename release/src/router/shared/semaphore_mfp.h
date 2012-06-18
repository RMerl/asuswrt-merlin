/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef __SEMAPHORE_MFP_H
#define __SEMAPHORE_MFP_H

#pragma pack(1)

#define SEM_MAGIC       0x89674523
#define SEM_NAME	"/tmp/Semaphore"
#define	FILE_MODE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

#define SPINLOCK_SiteSurvey	1
#define SPINLOCK_NVRAMCommit	2
#define SPINLOCK_Networkmap     3
#define SPINLOCK_SPIN_MAX	SPINLOCK_Networkmap+1

typedef struct
{
        int sem_fd[2];
        int sem_magic;
} semaphore_t;

int  semaphore_create();
int  semaphore_close();
int  semaphore_open(const char *, int, ... );
int  semaphore_unlink(const char *);
int  semaphore_post();
int  semaphore_wait();

int spinlock_init(int idx);
int spinlock_destroy(int idx);
int spinlock_lock(int idx);
int spinlock_unlock(int idx);

#pragma pack()

#endif
