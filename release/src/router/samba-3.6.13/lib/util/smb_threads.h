/*
	Unix SMB/CIFS implementation.
	SMB thread interface functions.
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

#ifndef _smb_threads_h_
#define _smb_threads_h_

typedef bool smb_thread_once_t;
#define SMB_THREAD_ONCE_INIT false

enum smb_thread_lock_type {
	SMB_THREAD_LOCK = 1,
	SMB_THREAD_UNLOCK
};

struct smb_thread_functions {
	/* Mutex and tls functions. */
	int (*create_mutex)(const char *lockname,
			void **pplock,
			const char *location);
	void (*destroy_mutex)(void *plock,
			const char *location);
	int (*lock_mutex)(void *plock,
                          int lock_type,
                          const char *location);

	/* Thread local storage. */
	int (*create_tls)(const char *keyname,
			void **ppkey,
			const char *location);
	void (*destroy_tls)(void **pkey,
			const char *location);
	int (*set_tls)(void *pkey, const void *pval, const char *location);
	void *(*get_tls)(void *pkey, const char *location);
};

int smb_thread_set_functions(const struct smb_thread_functions *tf);
int smb_thread_once(smb_thread_once_t *ponce,
                    void (*init_fn)(void *pdata),
                    void *pdata);

extern const struct smb_thread_functions *global_tfp;

/* Define the pthread version of the functions. */

#define SMB_THREADS_DEF_PTHREAD_IMPLEMENTATION(tf) \
 \
static int smb_create_mutex_pthread(const char *lockname, void **pplock, const char *location) \
{ \
	pthread_mutex_t *pmut = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t)); \
	if (!pmut) { \
		return ENOMEM; \
	} \
	pthread_mutex_init(pmut, NULL); \
	*pplock = (void *)pmut; \
	return 0; \
} \
 \
static void smb_destroy_mutex_pthread(void *plock, const char *location) \
{ \
	pthread_mutex_destroy((pthread_mutex_t *)plock); \
	free(plock); \
} \
 \
static int smb_lock_pthread(void *plock, int lock_type, const char *location) \
{ \
	if (lock_type == SMB_THREAD_UNLOCK) { \
		return pthread_mutex_unlock((pthread_mutex_t *)plock); \
	} else { \
		return pthread_mutex_lock((pthread_mutex_t *)plock); \
	} \
} \
 \
static int smb_create_tls_pthread(const char *keyname, void **ppkey, const char *location) \
{ \
	int ret; \
	pthread_key_t *pkey; \
	pkey = (pthread_key_t *)malloc(sizeof(pthread_key_t)); \
	if (!pkey) { \
		return ENOMEM; \
	} \
	ret = pthread_key_create(pkey, NULL); \
	if (ret) { \
		free(pkey); \
		return ret; \
	} \
	*ppkey = (void *)pkey; \
	return 0; \
} \
 \
static void smb_destroy_tls_pthread(void **ppkey, const char *location) \
{ \
	if (*ppkey) { \
		pthread_key_delete(*(pthread_key_t *)ppkey); \
		free(*ppkey); \
		*ppkey = NULL; \
	} \
} \
 \
static int smb_set_tls_pthread(void *pkey, const void *pval, const char *location) \
{ \
        return pthread_setspecific(*(pthread_key_t *)pkey, pval); \
} \
 \
static void *smb_get_tls_pthread(void *pkey, const char *location) \
{ \
        return pthread_getspecific(*(pthread_key_t *)pkey); \
} \
 \
static const struct smb_thread_functions (tf) = { \
			smb_create_mutex_pthread, \
			smb_destroy_mutex_pthread, \
			smb_lock_pthread, \
			smb_create_tls_pthread, \
			smb_destroy_tls_pthread, \
			smb_set_tls_pthread, \
			smb_get_tls_pthread }

#endif
