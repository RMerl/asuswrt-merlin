/* 
   Unix SMB/CIFS implementation.

   thread model: standard (1 thread per client connection)

   Copyright (C) Andrew Tridgell 2003-2005
   Copyright (C) James J Myers 2003 <myersjj@samba.org>
   Copyright (C) Stefan (metze) Metzmacher 2004
   
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
#include "version.h"
#include <pthread.h>
#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif
#include "system/wait.h"
#include "system/filesys.h"
#include "system/time.h"
#include "lib/events/events.h"
#include "lib/util/dlinklist.h"
#include "lib/util/mutex.h"
#include "smbd/process_model.h"

static pthread_key_t title_key;

struct new_conn_state {
	struct tevent_context *ev;
	struct socket_context *sock;
	struct loadparm_context *lp_ctx;
	void (*new_conn)(struct tevent_context *, struct loadparm_context *lp_ctx, struct socket_context *, uint32_t , void *);
	void *private_data;
};

static void *thread_connection_fn(void *thread_parm)
{
	struct new_conn_state *new_conn = talloc_get_type(thread_parm, struct new_conn_state);

	new_conn->new_conn(new_conn->ev, new_conn->lp_ctx, new_conn->sock, pthread_self(), new_conn->private_data);

	/* run this connection from here */
	event_loop_wait(new_conn->ev);

	talloc_free(new_conn);

	return NULL;
}

/*
  called when a listening socket becomes readable
*/
static void thread_accept_connection(struct tevent_context *ev, 
				     struct loadparm_context *lp_ctx,
				     struct socket_context *sock,
				     void (*new_conn)(struct tevent_context *, 
						      struct loadparm_context *,
						      struct socket_context *, 
						      uint32_t , void *), 
				     void *private_data)
{		
	NTSTATUS status;
	int rc;
	pthread_t thread_id;
	pthread_attr_t thread_attr;
	struct new_conn_state *state;
	struct tevent_context *ev2;

	ev2 = s4_event_context_init(ev);
	if (ev2 == NULL) return;

	state = talloc(ev2, struct new_conn_state);
	if (state == NULL) {
		talloc_free(ev2);
		return;
	}

	state->new_conn = new_conn;
	state->private_data  = private_data;
	state->lp_ctx   = lp_ctx;
	state->ev       = ev2;

	/* accept an incoming connection. */
	status = socket_accept(sock, &state->sock);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(ev2);
		/* We need to throttle things until the system clears
		   enough resources to handle this new socket. If we
		   don't then we will spin filling the log and causing
		   more problems. We don't panic as this is probably a
		   temporary resource constraint */
		sleep(1);
		return;
	}

	talloc_steal(state, state->sock);

	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&thread_id, &thread_attr, thread_connection_fn, state);
	pthread_attr_destroy(&thread_attr);
	if (rc == 0) {
		DEBUG(4,("accept_connection_thread: created thread_id=%lu for fd=%d\n", 
			(unsigned long int)thread_id, socket_get_fd(sock)));
	} else {
		DEBUG(0,("accept_connection_thread: thread create failed for fd=%d, rc=%d\n", socket_get_fd(sock), rc));
		talloc_free(ev2);
	}
}


struct new_task_state {
	struct tevent_context *ev;
	struct loadparm_context *lp_ctx;
	void (*new_task)(struct tevent_context *, struct loadparm_context *, 
			 uint32_t , void *);
	void *private_data;
};

static void *thread_task_fn(void *thread_parm)
{
	struct new_task_state *new_task = talloc_get_type(thread_parm, struct new_task_state);

	new_task->new_task(new_task->ev, new_task->lp_ctx, pthread_self(),
			   new_task->private_data);

	/* run this connection from here */
	event_loop_wait(new_task->ev);

	talloc_free(new_task);

	return NULL;
}

/*
  called when a new task is needed
*/
static void thread_new_task(struct tevent_context *ev, 
			    struct loadparm_context *lp_ctx,
			    const char *service_name,
			    void (*new_task)(struct tevent_context *, 
					     struct loadparm_context *,
					     uint32_t , void *), 
			    void *private_data)
{		
	int rc;
	pthread_t thread_id;
	pthread_attr_t thread_attr;
	struct new_task_state *state;
	struct tevent_context *ev2;

	ev2 = s4_event_context_init(ev);
	if (ev2 == NULL) return;

	state = talloc(ev2, struct new_task_state);
	if (state == NULL) {
		talloc_free(ev2);
		return;
	}

	state->new_task = new_task;
	state->lp_ctx   = lp_ctx;
	state->private_data  = private_data;
	state->ev       = ev2;

	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&thread_id, &thread_attr, thread_task_fn, state);
	pthread_attr_destroy(&thread_attr);
	if (rc == 0) {
		DEBUG(4,("thread_new_task: created %s thread_id=%lu\n", 
			 service_name, (unsigned long int)thread_id));
	} else {
		DEBUG(0,("thread_new_task: thread create for %s failed rc=%d\n", service_name, rc));
		talloc_free(ev2);
	}
}

/* called when a task goes down */
static void thread_terminate(struct tevent_context *event_ctx, struct loadparm_context *lp_ctx, const char *reason)
{
	DEBUG(10,("thread_terminate: reason[%s]\n",reason));

	talloc_free(event_ctx);

	/* terminate this thread */
	pthread_exit(NULL);  /* thread cleanup routine will do actual cleanup */
}

/* called to set a title of a task or connection */
static void thread_set_title(struct tevent_context *ev, const char *title) 
{
	char *old_title;
	char *new_title;

	old_title = pthread_getspecific(title_key);
	talloc_free(old_title);

	new_title = talloc_strdup(ev, title);
	pthread_setspecific(title_key, new_title);
}

/*
  mutex init function for thread model
*/
static int thread_mutex_init(smb_mutex_t *mutex, const char *name)
{
	pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
	mutex->mutex = memdup(&m, sizeof(m));
	if (! mutex->mutex) {
		errno = ENOMEM;
		return -1;
	}
	return pthread_mutex_init((pthread_mutex_t *)mutex->mutex, NULL);
}

/*
  mutex destroy function for thread model
*/
static int thread_mutex_destroy(smb_mutex_t *mutex, const char *name)
{
	return pthread_mutex_destroy((pthread_mutex_t *)mutex->mutex);
}

static void mutex_start_timer(struct timespec *tp1)
{
	clock_gettime_mono(tp1);
}

static double mutex_end_timer(struct timespec tp1)
{
	struct timespec tp2;

	clock_gettime_mono(&tp2);
	return((tp2.tv_sec - tp1.tv_sec) + 
	       (tp2.tv_nsec - tp1.tv_nsec)*1.0e-9);
}

/*
  mutex lock function for thread model
*/
static int thread_mutex_lock(smb_mutex_t *mutexP, const char *name)
{
	pthread_mutex_t *mutex = (pthread_mutex_t *)mutexP->mutex;
	int rc;
	double t;
	struct timespec tp1;
	/* Test below is ONLY for debugging */
	if ((rc = pthread_mutex_trylock(mutex))) {
		if (rc == EBUSY) {
			mutex_start_timer(&tp1);
			printf("mutex lock: thread %d, lock %s not available\n", 
				(uint32_t)pthread_self(), name);
			print_suspicious_usage("mutex_lock", name);
			pthread_mutex_lock(mutex);
			t = mutex_end_timer(tp1);
			printf("mutex lock: thread %d, lock %s now available, waited %g seconds\n", 
				(uint32_t)pthread_self(), name, t);
			return 0;
		}
		printf("mutex lock: thread %d, lock %s failed rc=%d\n", 
				(uint32_t)pthread_self(), name, rc);
		SMB_ASSERT(errno == 0); /* force error */
	}
	return 0;
}

/* 
   mutex unlock for thread model
*/
static int thread_mutex_unlock(smb_mutex_t *mutex, const char *name)
{
	return pthread_mutex_unlock((pthread_mutex_t *)mutex->mutex);
}

/*****************************************************************
 Read/write lock routines.
*****************************************************************/  
/*
  rwlock init function for thread model
*/
static int thread_rwlock_init(smb_rwlock_t *rwlock, const char *name)
{
	pthread_rwlock_t m = PTHREAD_RWLOCK_INITIALIZER;
	rwlock->rwlock = memdup(&m, sizeof(m));
	if (! rwlock->rwlock) {
		errno = ENOMEM;
		return -1;
	}
	return pthread_rwlock_init((pthread_rwlock_t *)rwlock->rwlock, NULL);
}

/*
  rwlock destroy function for thread model
*/
static int thread_rwlock_destroy(smb_rwlock_t *rwlock, const char *name)
{
	return pthread_rwlock_destroy((pthread_rwlock_t *)rwlock->rwlock);
}

/*
  rwlock lock for read function for thread model
*/
static int thread_rwlock_lock_read(smb_rwlock_t *rwlockP, const char *name)
{
	pthread_rwlock_t *rwlock = (pthread_rwlock_t *)rwlockP->rwlock;
	int rc;
	double t;
	struct timespec tp1;
	/* Test below is ONLY for debugging */
	if ((rc = pthread_rwlock_tryrdlock(rwlock))) {
		if (rc == EBUSY) {
			mutex_start_timer(&tp1);
			printf("rwlock lock_read: thread %d, lock %s not available\n", 
				(uint32_t)pthread_self(), name);
			print_suspicious_usage("rwlock_lock_read", name);
			pthread_rwlock_rdlock(rwlock);
			t = mutex_end_timer(tp1);
			printf("rwlock lock_read: thread %d, lock %s now available, waited %g seconds\n", 
				(uint32_t)pthread_self(), name, t);
			return 0;
		}
		printf("rwlock lock_read: thread %d, lock %s failed rc=%d\n", 
				(uint32_t)pthread_self(), name, rc);
		SMB_ASSERT(errno == 0); /* force error */
	}
	return 0;
}

/*
  rwlock lock for write function for thread model
*/
static int thread_rwlock_lock_write(smb_rwlock_t *rwlockP, const char *name)
{
	pthread_rwlock_t *rwlock = (pthread_rwlock_t *)rwlockP->rwlock;
	int rc;
	double t;
	struct timespec tp1;
	/* Test below is ONLY for debugging */
	if ((rc = pthread_rwlock_trywrlock(rwlock))) {
		if (rc == EBUSY) {
			mutex_start_timer(&tp1);
			printf("rwlock lock_write: thread %d, lock %s not available\n", 
				(uint32_t)pthread_self(), name);
			print_suspicious_usage("rwlock_lock_write", name);
			pthread_rwlock_wrlock(rwlock);
			t = mutex_end_timer(tp1);
			printf("rwlock lock_write: thread %d, lock %s now available, waited %g seconds\n", 
				(uint32_t)pthread_self(), name, t);
			return 0;
		}
		printf("rwlock lock_write: thread %d, lock %s failed rc=%d\n", 
				(uint32_t)pthread_self(), name, rc);
		SMB_ASSERT(errno == 0); /* force error */
	}
	return 0;
}


/* 
   rwlock unlock for thread model
*/
static int thread_rwlock_unlock(smb_rwlock_t *rwlock, const char *name)
{
	return pthread_rwlock_unlock((pthread_rwlock_t *)rwlock->rwlock);
}

/*****************************************************************
 Log suspicious usage (primarily for possible thread-unsafe behavior).
*****************************************************************/  
static void thread_log_suspicious_usage(const char* from, const char* info)
{
	DEBUG(1,("log_suspicious_usage: from %s info='%s'\n", from, info));
#ifdef HAVE_BACKTRACE
	{
		void *addresses[10];
		int num_addresses = backtrace(addresses, 8);
		char **bt_symbols = backtrace_symbols(addresses, num_addresses);
		int i;

		if (bt_symbols) {
			for (i=0; i<num_addresses; i++) {
				DEBUG(1,("log_suspicious_usage: %s%s\n", DEBUGTAB(1), bt_symbols[i]));
			}
			free(bt_symbols);
		}
	}
#endif
}

/*****************************************************************
 Log suspicious usage to stdout (primarily for possible thread-unsafe behavior.
 Used in mutex code where DEBUG calls would cause recursion.
*****************************************************************/  
static void thread_print_suspicious_usage(const char* from, const char* info)
{
	printf("log_suspicious_usage: from %s info='%s'\n", from, info);
#ifdef HAVE_BACKTRACE
	{
		void *addresses[10];
		int num_addresses = backtrace(addresses, 8);
		char **bt_symbols = backtrace_symbols(addresses, num_addresses);
		int i;

		if (bt_symbols) {
			for (i=0; i<num_addresses; i++) {
				printf("log_suspicious_usage: %s%s\n", DEBUGTAB(1), bt_symbols[i]);
			}
			free(bt_symbols);
		}
	}
#endif
}

static uint32_t thread_get_task_id(void)
{
	return (uint32_t)pthread_self();
}

static void thread_log_task_id(int fd)
{
	char *s= NULL;

	asprintf(&s, "thread[%u][%s]:\n", 
		(uint32_t)pthread_self(),
		(const char *)pthread_getspecific(title_key));
	if (!s) return;
	write(fd, s, strlen(s));
	free(s);
}

/****************************************************************************
catch serious errors
****************************************************************************/
static void thread_sig_fault(int sig)
{
	DEBUG(0,("===============================================================\n"));
	DEBUG(0,("TERMINAL ERROR: Recursive signal %d in thread [%u][%s] (%s)\n",
		sig,(uint32_t)pthread_self(),
		(const char *)pthread_getspecific(title_key),
		SAMBA_VERSION_STRING));
	DEBUG(0,("===============================================================\n"));
	exit(1); /* kill the whole server for now */
}

/*******************************************************************
setup our recursive fault handlers
********************************************************************/
static void thread_fault_setup(void)
{
#ifdef SIGSEGV
	CatchSignal(SIGSEGV, thread_sig_fault);
#endif
#ifdef SIGBUS
	CatchSignal(SIGBUS, thread_sig_fault);
#endif
#ifdef SIGABRT
	CatchSignal(SIGABRT, thread_sig_fault);
#endif
}

/*******************************************************************
report a fault in a thread
********************************************************************/
static void thread_fault_handler(int sig)
{
	static int counter;
	
	/* try to catch recursive faults */
	thread_fault_setup();
	
	counter++;	/* count number of faults that have occurred */

	DEBUG(0,("===============================================================\n"));
	DEBUG(0,("INTERNAL ERROR: Signal %d in thread [%u] [%s] (%s)\n",
		sig,(uint32_t)pthread_self(),
		(const char *)pthread_getspecific(title_key),
		SAMBA_VERSION_STRING));
	DEBUG(0,("Please read the file BUGS.txt in the distribution\n"));
	DEBUG(0,("===============================================================\n"));
#ifdef HAVE_BACKTRACE
	{
		void *addresses[10];
		int num_addresses = backtrace(addresses, 8);
		char **bt_symbols = backtrace_symbols(addresses, num_addresses);
		int i;

		if (bt_symbols) {
			for (i=0; i<num_addresses; i++) {
				DEBUG(1,("fault_report: %s%s\n", DEBUGTAB(1), bt_symbols[i]));
			}
			free(bt_symbols);
		}
	}
#endif
	pthread_exit(NULL); /* terminate failing thread only */
}

/*
  called when the process model is selected
*/
static void thread_model_init(void)
{
	struct mutex_ops m_ops;
	struct debug_ops d_ops;

	ZERO_STRUCT(m_ops);
	ZERO_STRUCT(d_ops);

	pthread_key_create(&title_key, NULL);
	pthread_setspecific(title_key, NULL);

	/* register mutex/rwlock handlers */
	m_ops.mutex_init = thread_mutex_init;
	m_ops.mutex_lock = thread_mutex_lock;
	m_ops.mutex_unlock = thread_mutex_unlock;
	m_ops.mutex_destroy = thread_mutex_destroy;
	
	m_ops.rwlock_init = thread_rwlock_init;
	m_ops.rwlock_lock_write = thread_rwlock_lock_write;
	m_ops.rwlock_lock_read = thread_rwlock_lock_read;
	m_ops.rwlock_unlock = thread_rwlock_unlock;
	m_ops.rwlock_destroy = thread_rwlock_destroy;

	register_mutex_handlers("thread", &m_ops);

	register_fault_handler("thread", thread_fault_handler);

	d_ops.log_suspicious_usage = thread_log_suspicious_usage;
	d_ops.print_suspicious_usage = thread_print_suspicious_usage;
	d_ops.get_task_id = thread_get_task_id;
	d_ops.log_task_id = thread_log_task_id;

	register_debug_handlers("thread", &d_ops);
}


static const struct model_ops thread_ops = {
	.name			= "thread",
	.model_init		= thread_model_init,
	.accept_connection	= thread_accept_connection,
	.new_task               = thread_new_task,
	.terminate              = thread_terminate,
	.set_title		= thread_set_title,
};

/*
  initialise the thread process model, registering ourselves with the model subsystem
 */
NTSTATUS process_model_thread_init(void)
{
	NTSTATUS ret;

	/* register ourselves with the PROCESS_MODEL subsystem. */
	ret = register_process_model(&thread_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register process_model 'thread'!\n"));
		return ret;
	}

	return ret;
}
