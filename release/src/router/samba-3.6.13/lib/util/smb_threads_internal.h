/*
	SMB/CIFS implementation.
	SMB thread interface internal macros.
	Copyright (C) Jeremy Allison, 2009.

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

#ifndef _smb_threads_internal_h_
#define _smb_threads_internal_h_

#define SMB_THREAD_CREATE_MUTEX(name, lockvar) \
	(global_tfp ? global_tfp->create_mutex((name), &(lockvar), __location__) : 0)

#define SMB_THREAD_DESTROY_MUTEX(plock) \
	do { \
		if (global_tfp) { \
			global_tfp->destroy_mutex(plock, __location__); \
		}; \
	} while (0)

#define SMB_THREAD_LOCK_INTERNAL(plock, type, location) \
	(global_tfp ? global_tfp->lock_mutex((plock), (type), location) : 0)

#define SMB_THREAD_LOCK(plock) \
        SMB_THREAD_LOCK_INTERNAL(plock, SMB_THREAD_LOCK, __location__)

#define SMB_THREAD_UNLOCK(plock) \
        SMB_THREAD_LOCK_INTERNAL(plock, SMB_THREAD_UNLOCK, __location__)

#define SMB_THREAD_ONCE(ponce, init_fn, pdata)                  \
        (global_tfp                                             \
         ? (! *(ponce)                                          \
            ? smb_thread_once((ponce), (init_fn), (pdata))      \
            : 0)                                                \
         : ((init_fn(pdata)), *(ponce) = true, 1))

#define SMB_THREAD_CREATE_TLS(keyname, key) \
	(global_tfp ? global_tfp->create_tls((keyname), &(key), __location__) : 0)

#define SMB_THREAD_DESTROY_TLS(key) \
	do { \
		if (global_tfp) { \
			global_tfp->destroy_tls(&(key), __location__); \
		}; \
	} while (0)

#define SMB_THREAD_SET_TLS(key, val) \
	(global_tfp ? global_tfp->set_tls((key),(val),__location__) : \
		((key) = (val), 0))

#define SMB_THREAD_GET_TLS(key) \
	(global_tfp ? global_tfp->get_tls((key), __location__) : (key))

/*
 * Global thread lock list.
 */

#define NUM_GLOBAL_LOCKS 1

#define GLOBAL_LOCK(locknum) (global_lock_array ? global_lock_array[(locknum)] : NULL)

#endif
