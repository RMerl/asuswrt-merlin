/* 
   Unix SMB/Netbios implementation.
   SMB client library implementation
   Copyright (C) Derrell Lipman 2009
   
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

#include "includes.h"
#include "libsmbclient.h"
#include "libsmb_internal.h"


/**
 * Initialize for an arbitrary thread implementation. The caller should
 * provide, as parameters, pointers to functions to implement the requisite
 * low-level thread functionality. A function must be provided for each
 * parameter; none may be null.
 *
 * If the thread implementation is POSIX Threads (pthreads), then the much
 * simpler smbc_thread_pthread() function may be used instead of this one.
 *
 * @param create_mutex
 *   Create a mutex. This function should expect three parameters: lockname,
 *   pplock, and location. It should create a unique mutex for each unique
 *   lockname it is provided, and return the mutex identifier in *pplock. The
 *   location parameter can be used for debugging, as it contains the
 *   compiler-provided __location__ of the call.
 *
 * @param destroy_mutex
 *   Destroy a mutex. This function should expect two parameters: plock and
 *   location. It should destroy the mutex associated with the identifier
 *   plock. The location parameter can be used for debugging, as it contains
 *   the compiler-provided __location__ of the call.
 *
 * @param lock_mutex
 *   Lock a mutex. This function should expect three parameters: plock,
 *   lock_type, and location. The mutex aassociated with identifier plock
 *   should be locked if lock_type is 1, and unlocked if lock_type is 2. The
 *   location parameter can be used for debugging, as it contains the
 *   compiler-provided __location__ of the call.
 *
 * @param create_tls
 *   Create thread local storage. This function should expect three
 *   parameters: keyname, ppkey, and location. It should allocate an
 *   implementation-specific amount of memory and assign the pointer to that
 *   allocated memory to *ppkey. The location parameter can be used for
 *   debugging, as it contains the compiler-provided __location__ of the
 *   call. This function should return 0 upon success, non-zero upon failure.
 *
 * @param destroy_tls
 *   Destroy thread local storage. This function should expect two parameters:
 *   ppkey and location. The ppkey parameter points to a variable containing a
 *   thread local storage key previously provided by the create_tls
 *   function. The location parameter can be used for debugging, as it
 *   contains the compiler-provided __location__ of the call.
 *
 * @param set_tls
 *   Set a thread local storage variable's value. This function should expect
 *   three parameters: pkey, pval, and location. The pkey parameter is a
 *   thread local storage key previously provided by the create_tls
 *   function. The (void *) pval parameter contains the value to be placed in
 *   the thread local storage variable identified by pkey. The location
 *   parameter can be used for debugging, as it contains the compiler-provided
 *   __location__ of the call. This function should return 0 upon success;
 *   non-zero otherwise.
 *
 * @param get_tls
 *   Retrieve a thread local storage variable's value. This function should
 *   expect two parameters: pkey and location. The pkey parameter is a thread
 *   local storage key previously provided by the create_tls function, and
 *   which has previously been used in a call to the set_tls function to
 *   initialize a thread local storage variable. The location parameter can be
 *   used for debugging, as it contains the compiler-provided __location__ of
 *   the call. This function should return the (void *) value stored in the
 *   variable identified by pkey.
 *
 * @return {void}
 */
void
smbc_thread_impl(
        /* Mutex functions. */
        int (*create_mutex)(const char *lockname,
                            void **pplock,
                            const char *location),
        void (*destroy_mutex)(void *plock,
                              const char *location),
        int (*lock_mutex)(void *plock,
                          int lock_type,
                          const char *location),
    
        /* Thread local storage. */
        int (*create_tls)(const char *keyname,
                          void **ppkey,
                          const char *location),
        void (*destroy_tls)(void **ppkey,
                            const char *location),
        int (*set_tls)(void *pkey,
                       const void *pval,
                       const char *location),
        void *(*get_tls)(void *pkey,
                         const char *location)
        )
{
        static struct smb_thread_functions tf;

        tf.create_mutex  = create_mutex;
        tf.destroy_mutex = destroy_mutex;
        tf.lock_mutex    = lock_mutex;
        tf.create_tls    = create_tls;
        tf.destroy_tls   = destroy_tls;
        tf.set_tls       = set_tls;
        tf.get_tls       = get_tls;

        smb_thread_set_functions(&tf);
}
