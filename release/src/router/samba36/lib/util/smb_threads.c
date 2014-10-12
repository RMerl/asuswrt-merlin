/*
   Unix SMB/CIFS implementation.
   SMB client library implementation (thread interface functions).
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

/*
 * This code is based in the ideas in openssl
 * but somewhat simpler and expended to include
 * thread local storage.
 */

#include "includes.h"
#include "smb_threads.h"

/*********************************************************
 Functions to vector the locking primitives used internally
 by libsmbclient.
*********************************************************/

const struct smb_thread_functions *global_tfp;

/*********************************************************
 Dynamic lock array.
*********************************************************/

void **global_lock_array;

/*********************************************************
 Mutex used for our internal "once" function
*********************************************************/

static void *once_mutex = NULL;


/*********************************************************
 Function to set the locking primitives used by libsmbclient.
*********************************************************/

int smb_thread_set_functions(const struct smb_thread_functions *tf)
{
	int i;

	global_tfp = tf;

#if defined(PARANOID_MALLOC_CHECKER)
#ifdef malloc
#undef malloc
#endif
#endif

	/* Here we initialize any static locks we're using. */
	global_lock_array = (void **)malloc(sizeof(void *) *NUM_GLOBAL_LOCKS);

#if defined(PARANOID_MALLOC_CHECKER)
#define malloc(s) __ERROR_DONT_USE_MALLOC_DIRECTLY
#endif

	if (global_lock_array == NULL) {
		return ENOMEM;
	}

	for (i = 0; i < NUM_GLOBAL_LOCKS; i++) {
		char *name = NULL;
		if (asprintf(&name, "global_lock_%d", i) == -1) {
			SAFE_FREE(global_lock_array);
			return ENOMEM;
		}
		if (global_tfp->create_mutex(name,
				&global_lock_array[i],
				__location__)) {
			smb_panic("smb_thread_set_functions: create mutexes failed");
		}
		SAFE_FREE(name);
	}

        /* Create the mutex we'll use for our "once" function */
	if (SMB_THREAD_CREATE_MUTEX("smb_once", once_mutex) != 0) {
		smb_panic("smb_thread_set_functions: failed to create 'once' mutex");
	}

	return 0;
}

/*******************************************************************
 Call a function only once. We implement this ourselves
 using our own mutex rather than using the thread implementation's
 *_once() function because each implementation has its own
 type for the variable which keeps track of whether the function
 has been called, and there's no easy way to allocate the correct
 size variable in code internal to Samba without knowing the
 implementation's "once" type.
********************************************************************/

int smb_thread_once(smb_thread_once_t *ponce,
                    void (*init_fn)(void *pdata),
                    void *pdata)
{
        int ret;

        /* Lock our "once" mutex in order to test and initialize ponce */
	if (SMB_THREAD_LOCK(once_mutex) != 0) {
                smb_panic("error locking 'once'");
	}

        /* Keep track of whether we ran their init function */
        ret = ! *ponce;

        /*
         * See if another thread got here after we tested it initially but
         * before we got our lock.
         */
        if (! *ponce) {
                /* Nope, we need to run the initialization function */
                (*init_fn)(pdata);

                /* Now we can indicate that the function has been run */
                *ponce = true;
        }

        /* Unlock the mutex */
	if (SMB_THREAD_UNLOCK(once_mutex) != 0) {
                smb_panic("error unlocking 'once'");
	}
        
        /*
         * Tell 'em whether we ran their init function. If they passed a data
         * pointer to the init function and the init function could change
         * something in the pointed-to data, this will tell them whether that
         * data is valid or not.
         */
	return ret;
}


#if 0
/* Test. - pthread implementations. */
#include <pthread.h>

#ifdef malloc
#undef malloc
#endif

SMB_THREADS_DEF_PTHREAD_IMPLEMENTATION(tf);

static smb_thread_once_t ot = SMB_THREAD_ONCE_INIT;
void *pkey = NULL;

static void init_fn(void)
{
	int ret;

	if (!global_tfp) {
		/* Non-thread safe init case. */
		if (ot) {
			return;
		}
		ot = true;
	}

	if ((ret = SMB_THREAD_CREATE_TLS("test_tls", pkey)) != 0) {
		printf("Create tls once error: %d\n", ret);
	}
}

/* Test function. */
int test_threads(void)
{
	int ret;
	void *plock = NULL;
	smb_thread_set_functions(&tf);

	SMB_THREAD_ONCE(&ot, init_fn);

	if ((ret = SMB_THREAD_CREATE_MUTEX("test", plock)) != 0) {
		printf("Create lock error: %d\n", ret);
	}
	if ((ret = SMB_THREAD_LOCK(plock, SMB_THREAD_LOCK)) != 0) {
		printf("lock error: %d\n", ret);
	}
	if ((ret = SMB_THREAD_LOCK(plock, SMB_THREAD_UNLOCK)) != 0) {
		printf("unlock error: %d\n", ret);
	}
	SMB_THREAD_DESTROY_MUTEX(plock);
	SMB_THREAD_DESTROY_TLS(pkey);

	return 0;
}
#endif
