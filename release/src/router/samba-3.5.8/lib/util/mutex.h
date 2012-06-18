#ifndef _MUTEX_H_
#define _MUTEX_H_
/* 
   Unix SMB/CIFS implementation.
   Samba mutex functions
   Copyright (C) Andrew Tridgell 2003
   Copyright (C) James J Myers 2003
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/** 
 * @file
 * @brief Mutex operations
 */

struct mutex_ops;

/* To add a new read/write lock, add it to enum rwlock_id
 */
enum rwlock_id { RWLOCK_SMBD, 		/* global smbd lock */

		RWLOCK_MAX /* this MUST be kept last */
};

#define MUTEX_LOCK_BY_ID(mutex_index) smb_mutex_lock_by_id(mutex_index, #mutex_index)
#define MUTEX_UNLOCK_BY_ID(mutex_index) smb_mutex_unlock_by_id(mutex_index, #mutex_index)
#define MUTEX_INIT(mutex, name) smb_mutex_init(mutex, #name)
#define MUTEX_DESTROY(mutex, name) smb_mutex_destroy(mutex, #name)
#define MUTEX_LOCK(mutex, name) smb_mutex_lock(mutex, #name)
#define MUTEX_UNLOCK(mutex, name) smb_mutex_unlock(mutex, #name)

#define RWLOCK_INIT(rwlock, name) smb_rwlock_init(rwlock, #name)
#define RWLOCK_DESTROY(rwlock, name) smb_rwlock_destroy(rwlock, #name)
#define RWLOCK_LOCK_WRITE(rwlock, name) smb_rwlock_lock_write(rwlock, #name)
#define RWLOCK_LOCK_READ(rwlock, name) smb_rwlock_lock_read(rwlock, #name)
#define RWLOCK_UNLOCK(rwlock, name) smb_rwlock_unlock(rwlock, #name)



/* this null typedef ensures we get the types right and avoids the
   pitfalls of void* */
typedef struct smb_mutex {
	void *mutex;
} smb_mutex_t;
typedef struct {
	void *rwlock;
} smb_rwlock_t;

/* the mutex model operations structure - contains function pointers to 
   the model-specific implementations of each operation */
struct mutex_ops {
	int (*mutex_init)(smb_mutex_t *mutex, const char *name);
	int (*mutex_lock)(smb_mutex_t *mutex, const char *name);
	int (*mutex_unlock)(smb_mutex_t *mutex, const char *name);
	int (*mutex_destroy)(smb_mutex_t *mutex, const char *name);
	int (*rwlock_init)(smb_rwlock_t *rwlock, const char *name);
	int (*rwlock_lock_write)(smb_rwlock_t *rwlock, const char *name);
	int (*rwlock_lock_read)(smb_rwlock_t *rwlock, const char *name);
	int (*rwlock_unlock)(smb_rwlock_t *rwlock, const char *name);
	int (*rwlock_destroy)(smb_rwlock_t *rwlock, const char *name);
};

#endif /* endif _MUTEX_H_ */
