/* $Id: os_core_linux_kernel.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pj/os.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/log.h>
#include <pj/except.h>
#include <pj/errno.h>
#include <pj/string.h>
#include <pj/compat/high_precision.h>
#include <pj/compat/sprintf.h>

#include <linux/config.h>
#include <linux/version.h>
#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif
#include <linux/kernel.h>
#include <linux/sched.h>
//#include <linux/tqueue.h>
#include <linux/wait.h>
#include <linux/signal.h>

#include <asm/atomic.h>
#include <asm/unistd.h>
#include <asm/semaphore.h>

#define THIS_FILE   "oslinuxkern"

struct pj_thread_t
{
    /** Thread's name. */
    char obj_name[PJ_MAX_OBJ_NAME];

    /** Linux task structure for thread. */
    struct task_struct *thread;	

    /** Flags (specified in pj_thread_create) */
    unsigned flags;

    /** Task queue needed to launch thread. */
    //struct tq_struct	tq;	

    /** Semaphore needed to control thread startup. */
    struct semaphore	startstop_sem;

    /** Semaphore to suspend thread during startup. */
    struct semaphore	suspend_sem;

    /** Queue thread is waiting on. Gets initialized by
        thread_initialize, can be used by thread itself.
     */
    wait_queue_head_t	queue;

    /** Flag to tell thread whether to die or not.
        When the thread receives a signal, it must check
        the value of terminate and call thread_deinitialize and terminate
        if set.
     */
    int terminate;    

    /** Thread's entry. */
    pj_thread_proc *func;

    /** Argument. */
    void *arg;
};

struct pj_atomic_t
{
    atomic_t atom;
};

struct pj_mutex_t
{
    struct semaphore sem;
    pj_bool_t	     recursive;
    pj_thread_t	    *owner;
	int		     own_count;
	int                 inst_id;
};

struct pj_sem_t
{
    struct semaphore sem;
};

/*
 * Static global variables.
 */
#define MAX_TLS_ID  32
static void *tls_values[MAX_TLS_ID];
static int tls_id;
static long thread_tls_id;
static spinlock_t critical_section = SPIN_LOCK_UNLOCKED;
static unsigned long spinlock_flags;
static pj_thread_t main_thread;

/* private functions */
//#define TRACE_(expr)	PJ_LOG(3,expr)
#define TRACE_(x)


/* This must be called in the context of the new thread. */
static void thread_initialize( pj_thread_t *thread )
{
    TRACE_((THIS_FILE, "---new thread initializing..."));

    /* Set TLS */
    pj_thread_local_set(thread_tls_id, thread);

    /* fill in thread structure */
    thread->thread = current;
    pj_assert(thread->thread != NULL);

    /* set signal mask to what we want to respond */
    siginitsetinv(&current->blocked, 
		  sigmask(SIGKILL)|sigmask(SIGINT)|sigmask(SIGTERM));

    /* initialise wait queue */
    init_waitqueue_head(&thread->queue);

    /* initialise termination flag */
    thread->terminate = 0;

    /* set name of this process (making sure obj_name is null 
     * terminated first) 
     */
    thread->obj_name[PJ_MAX_OBJ_NAME-1] = '\0';
    sprintf(current->comm, thread->obj_name);
        
    /* tell the creator that we are ready and let him continue */
    up(&thread->startstop_sem);	
}

/* cleanup of thread. Called by the exiting thread. */
static void thread_deinitialize(pj_thread_t *thread)
{
    /* we are terminating */

    /* lock the kernel, the exit will unlock it */
    thread->thread = NULL;
    mb();

    /* notify the stop_kthread() routine that we are terminating. */
    up(&thread->startstop_sem);

    /* the kernel_thread that called clone() does a do_exit here. */

    /* there is no race here between execution of the "killer" and 
       real termination of the thread (race window between up and do_exit), 
       since both the thread and the "killer" function are running with 
       the kernel lock held.
       The kernel lock will be freed after the thread exited, so the code
       is really not executed anymore as soon as the unload functions gets
       the kernel lock back.
       The init process may not have made the cleanup of the process here,
       but the cleanup can be done safely with the module unloaded.
    */

}

static int thread_proc(void *arg)
{
    pj_thread_t *thread = arg;

    TRACE_((THIS_FILE, "---new thread starting!"));

    /* Initialize thread. */
    thread_initialize( thread );

    /* Wait if created suspended. */
    if (thread->flags & PJ_THREAD_SUSPENDED) {
	TRACE_((THIS_FILE, "---new thread suspended..."));
	down(&thread->suspend_sem);
    }

    TRACE_((THIS_FILE, "---new thread running..."));

    pj_assert(thread->func != NULL);

    /* Call thread's entry. */
    (*thread->func)(thread->arg);

    TRACE_((THIS_FILE, "---thread exiting..."));

    /* Cleanup thread. */
    thread_deinitialize(thread);

    return 0;
}

/* The very task entry. */
static void kthread_launcher(void *arg)
{
    TRACE_((THIS_FILE, "...launching thread!..."));
    kernel_thread(&thread_proc, arg, 0);
}

PJ_DEF(pj_status_t) pj_init(void)
{
    pj_status_t rc;

    PJ_LOG(5, ("pj_init", "Initializing PJ Library.."));

    rc = pj_thread_init();
    if (rc != PJ_SUCCESS)
	return rc;

    /* Initialize exception ID for the pool. 
     * Must do so after critical section is configured.
     */
    rc = pj_exception_id_alloc("PJLIB/No memory", &PJ_NO_MEMORY_EXCEPTION);
    if (rc != PJ_SUCCESS)
        return rc;

    return PJ_SUCCESS;
}

PJ_DEF(pj_uint32_t) pj_getpid(void)
{
	return 1;
}

PJ_DEF(pj_uint32_t) pj_gettid(void)
{
	return 1;
}

PJ_DEF(pj_status_t) pj_thread_register ( const char *cstr_thread_name,
					 pj_thread_desc desc,
					 pj_thread_t **ptr_thread)
{
    char stack_ptr;
    pj_thread_t *thread = (pj_thread_t *)desc;
    pj_str_t thread_name = pj_str((char*)cstr_thread_name);

    /* Size sanity check. */
    if (sizeof(pj_thread_desc) < sizeof(pj_thread_t)) {
	pj_assert(!"Not enough pj_thread_desc size!");
	return PJ_EBUG;
    }

    /* If a thread descriptor has been registered before, just return it. */
    if (pj_thread_local_get (thread_tls_id) != 0) {
	// 2006-02-26 bennylp:
	//  This wouldn't work in all cases!.
	//  If thread is created by external module (e.g. sound thread),
	//  thread may be reused while the pool used for the thread descriptor
	//  has been deleted by application.
	//*thread_ptr = (pj_thread_t*)pj_thread_local_get (thread_tls_id);
        return PJ_SUCCESS;
    }

    /* Initialize and set the thread entry. */
    pj_bzero(desc, sizeof(struct pj_thread_t));

    if(cstr_thread_name && pj_strlen(&thread_name) < sizeof(thread->obj_name)-1)
	pj_sprintf(thread->obj_name, cstr_thread_name, thread->thread);
    else
	pj_snprintf(thread->obj_name, sizeof(thread->obj_name), 
		    "thr%p", (void*)thread->thread);
    
    /* Initialize. */
    thread_initialize(thread);

    /* Eat semaphore. */
    down(&thread->startstop_sem);

#if defined(PJ_OS_HAS_CHECK_STACK) && PJ_OS_HAS_CHECK_STACK!=0
    thread->stk_start = &stack_ptr;
    thread->stk_size = 0xFFFFFFFFUL;
    thread->stk_max_usage = 0;
#else
    stack_ptr = '\0';
#endif

    *ptr_thread = thread;
    return PJ_SUCCESS;
}


pj_status_t pj_thread_init(void)
{
    pj_status_t rc;
    pj_thread_t *dummy;
    
    rc = pj_thread_local_alloc(&thread_tls_id);
    if (rc != PJ_SUCCESS)
	return rc;

    return pj_thread_register("pjlib-main", (long*)&main_thread, &dummy);
}

PJ_DEF(pj_status_t) pj_thread_create( pj_pool_t *pool, const char *thread_name,
				      pj_thread_proc *proc, void *arg,
				      pj_size_t stack_size, unsigned flags,
				      pj_thread_t **ptr_thread)
{
    pj_thread_t *thread;

    TRACE_((THIS_FILE, "pj_thread_create()"));
    
    PJ_ASSERT_RETURN(pool && proc && ptr_thread, PJ_EINVAL);

    thread = pj_pool_zalloc(pool, sizeof(pj_thread_t));
    if (!thread)
	return PJ_ENOMEM;

    PJ_UNUSED_ARG(stack_size);

    /* Thread name. */
    if (!thread_name) 
	thread_name = "thr%p";
    
    if (strchr(thread_name, '%')) {
	pj_snprintf(thread->obj_name, PJ_MAX_OBJ_NAME, thread_name, thread);
    } else {
	strncpy(thread->obj_name, thread_name, PJ_MAX_OBJ_NAME);
	thread->obj_name[PJ_MAX_OBJ_NAME-1] = '\0';
    }
    
    /* Init thread's semaphore. */
    TRACE_((THIS_FILE, "...init semaphores..."));
    init_MUTEX_LOCKED(&thread->startstop_sem);
    init_MUTEX_LOCKED(&thread->suspend_sem);

    thread->flags = flags;

    if ((flags & PJ_THREAD_SUSPENDED) == 0) {
	up(&thread->suspend_sem);
    }

    /* Store the functions and argument. */
    thread->func = proc;
    thread->arg = arg;
    
    /* Save return value. */
    *ptr_thread = thread;
    
    /* Create the new thread by running a task through keventd. */

#if 0
    /* Initialize the task queue struct. */
    thread->tq.sync = 0;
    INIT_LIST_HEAD(&thread->tq.list);
    thread->tq.routine = kthread_launcher;
    thread->tq.data = thread;

    /* and schedule it for execution. */
    schedule_task(&thread->tq);
#endif
    kthread_launcher(thread);

    /* Wait until thread has reached the setup_thread routine. */
    TRACE_((THIS_FILE, "...wait for the new thread..."));
    down(&thread->startstop_sem);

    TRACE_((THIS_FILE, "...main thread resumed..."));
    return PJ_SUCCESS;
}

PJ_DEF(const char*) pj_thread_get_name(pj_thread_t *thread)
{
    return thread->obj_name;
}

PJ_DEF(pj_status_t) pj_thread_resume(pj_thread_t *thread)
{
    up(&thread->suspend_sem);
    return PJ_SUCCESS;
}

PJ_DEF(pj_thread_t*) pj_thread_this(void)
{
    return (pj_thread_t*)pj_thread_local_get(thread_tls_id);
}

PJ_DEF(pj_status_t) pj_thread_join(pj_thread_t *p)
{
    TRACE_((THIS_FILE, "pj_thread_join()"));
    down(&p->startstop_sem);
    TRACE_((THIS_FILE, "  joined!"));
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_thread_destroy(pj_thread_t *thread)
{
    PJ_ASSERT_RETURN(thread != NULL, PJ_EINVALIDOP);
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_thread_sleep(unsigned msec)
{
    pj_highprec_t ticks;
    pj_thread_t *thread = pj_thread_this();

    PJ_ASSERT_RETURN(thread != NULL, PJ_EBUG);
    
    /* Use high precision calculation to make sure we don't
     * crop values:
     *
     *	ticks = HZ * msec / 1000
     */
    ticks = HZ;
    pj_highprec_mul(ticks, msec);
    pj_highprec_div(ticks, 1000);

    TRACE_((THIS_FILE, "this thread will sleep for %u ticks", ticks));
    interruptible_sleep_on_timeout( &thread->queue, ticks);
    return PJ_SUCCESS;
}


///////////////////////////////////////////////////////////////////////////////
PJ_DEF(pj_status_t) pj_atomic_create( pj_pool_t *pool, 
				      pj_atomic_value_t value,
				      pj_atomic_t **ptr_var)
{
    pj_atomic_t *t = pj_pool_calloc(pool, 1, sizeof(pj_atomic_t));
    if (!t) return PJ_ENOMEM;

    atomic_set(&t->atom, value);
    *ptr_var = t;
    
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_atomic_destroy( pj_atomic_t *var )
{
    return PJ_SUCCESS;
}

PJ_DEF(void) pj_atomic_set(pj_atomic_t *var, pj_atomic_value_t value)
{
    atomic_set(&var->atom, value);
}

PJ_DEF(pj_atomic_value_t) pj_atomic_get(pj_atomic_t *var)
{
    return atomic_read(&var->atom);
}

PJ_DEF(void) pj_atomic_inc(pj_atomic_t *var)
{
    atomic_inc(&var->atom);
}

PJ_DEF(void) pj_atomic_dec(pj_atomic_t *var)
{
    atomic_dec(&var->atom);
}

PJ_DEF(void) pj_atomic_add( pj_atomic_t *var, pj_atomic_value_t value )
{
    atomic_add(value, &var->atom);
}


///////////////////////////////////////////////////////////////////////////////
PJ_DEF(pj_status_t) pj_thread_local_alloc(long *index)
{
    if (tls_id >= MAX_TLS_ID)
	return PJ_ETOOMANY;
    
    *index = tls_id++;

    return PJ_SUCCESS;
}

PJ_DEF(void) pj_thread_local_free(long index)
{
    pj_assert(index >= 0 && index < MAX_TLS_ID);
}

PJ_DEF(pj_status_t) pj_thread_local_set(long index, void *value)
{
    pj_assert(index >= 0 && index < MAX_TLS_ID);
    tls_values[index] = value;
    return PJ_SUCCESS;
}

PJ_DEF(void*) pj_thread_local_get(long index)
{
    pj_assert(index >= 0 && index < MAX_TLS_ID);
    return tls_values[index];
}


///////////////////////////////////////////////////////////////////////////////
PJ_DEF(void) pj_enter_critical_section(void)
{
    spin_lock_irqsave(&critical_section, spinlock_flags);
}

PJ_DEF(void) pj_leave_critical_section(void)
{
    spin_unlock_irqrestore(&critical_section, spinlock_flags);
}


///////////////////////////////////////////////////////////////////////////////
PJ_DEF(pj_status_t) pj_mutex_create( pj_pool_t *pool, 
				     const char *name, 
				     int type,
				     pj_mutex_t **ptr_mutex)
{
    pj_mutex_t *mutex;
    
    PJ_UNUSED_ARG(name);

    mutex = pj_pool_alloc(pool, sizeof(pj_mutex_t));
    if (!mutex)
	return PJ_ENOMEM;

    init_MUTEX(&mutex->sem);

    mutex->recursive = (type == PJ_MUTEX_RECURSE);
    mutex->owner = NULL;
    mutex->own_count = 0;
    
    /* Done. */
    *ptr_mutex = mutex;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_mutex_create_simple( pj_pool_t *pool, const char *name,
					    pj_mutex_t **mutex )
{
    return pj_mutex_create(pool, name, PJ_MUTEX_SIMPLE, mutex);
}

PJ_DEF(pj_status_t) pj_mutex_create_recursive( pj_pool_t *pool,
					       const char *name,
					       pj_mutex_t **mutex )
{
    return pj_mutex_create( pool, name, PJ_MUTEX_RECURSE, mutex);
}

PJ_DEF(pj_status_t) pj_mutex_lock(pj_mutex_t *mutex)
{
    PJ_ASSERT_RETURN(mutex, PJ_EINVAL);

    if (mutex->recursive) {
	pj_thread_t *this_thread = pj_thread_this();
	if (mutex->owner == this_thread) {
	    ++mutex->own_count;
	} else {
	    down(&mutex->sem);
	    pj_assert(mutex->own_count == 0);
	    mutex->owner = this_thread;
	    mutex->own_count = 1;
	}
    } else {
	down(&mutex->sem);
    }
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_mutex_trylock(pj_mutex_t *mutex)
{
    long rc;

    PJ_ASSERT_RETURN(mutex, PJ_EINVAL);

    if (mutex->recursive) {
	pj_thread_t *this_thread = pj_thread_this();
	if (mutex->owner == this_thread) {
	    ++mutex->own_count;
	} else {
	    rc = down_interruptible(&mutex->sem);
	    if (rc != 0)
		return PJ_RETURN_OS_ERROR(-rc);
	    pj_assert(mutex->own_count == 0);
	    mutex->owner = this_thread;
	    mutex->own_count = 1;
	}
    } else {
	int rc = down_trylock(&mutex->sem);
	if (rc != 0)
	    return PJ_RETURN_OS_ERROR(-rc);
    }
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_mutex_unlock(pj_mutex_t *mutex)
{
    PJ_ASSERT_RETURN(mutex, PJ_EINVAL);

    if (mutex->recursive) {
	pj_thread_t *this_thread = pj_thread_this();
	if (mutex->owner == this_thread) {
	    pj_assert(mutex->own_count > 0);
	    --mutex->own_count;
	    if (mutex->own_count == 0) {
		mutex->owner = NULL;
		up(&mutex->sem);
	    }
	} else {
	    pj_assert(!"Not owner!");
	    return PJ_EINVALIDOP;
	}
    } else {
	up(&mutex->sem);
    }
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_mutex_destroy(pj_mutex_t *mutex)
{
    PJ_ASSERT_RETURN(mutex != NULL, PJ_EINVAL);

    return PJ_SUCCESS;
}

#if defined(PJ_DEBUG) && PJ_DEBUG != 0
PJ_DEF(pj_bool_t) pj_mutex_is_locked(pj_mutex_t *mutex)
{
    if (mutex->recursive)
	return mutex->owner == pj_thread_this();
    else
	return 1;
}
#endif	/* PJ_DEBUG */

PJ_DEF(int) pj_get_mutex_inst_id(pj_mutex_t *mutex)
{
	PJ_ASSERT_RETURN(mutex, -1);
	return mutex->inst_id;
}


#if defined(PJ_HAS_SEMAPHORE) && PJ_HAS_SEMAPHORE != 0

PJ_DEF(pj_status_t) pj_sem_create(  pj_pool_t *pool, 
                                    const char *name,
				    unsigned initial, 
                                    unsigned max,
				    pj_sem_t **sem)
{
    pj_sem_t *sem;

    PJ_UNUSED_ARG(max);

    PJ_ASSERT_RETURN(pool && sem, PJ_EINVAL);
    
    sem = pj_pool_alloc(pool, sizeof(pj_sem_t));
    sema_init(&sem->sem, initial);
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_sem_wait(pj_sem_t *sem)
{
    PJ_ASSERT_RETURN(pool && sem, PJ_EINVAL);

    down(&sem->sem);
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_sem_trywait(pj_sem_t *sem)
{
    int rc;

    PJ_ASSERT_RETURN(pool && sem, PJ_EINVAL);

    rc = down_trylock(&sem->sem);
    if (rc != 0) {
	return PJ_RETURN_OS_ERROR(-rc);
    } else {
	return PJ_SUCCESS;
    }
}

/*
 * pj_sem_trywait2()
 */
PJ_DEF(pj_status_t) pj_sem_trywait2(pj_sem_t *sem)
{
	return pj_sem_trywait(sem);
}

PJ_DEF(pj_status_t) pj_sem_post(pj_sem_t *sem)
{
    PJ_ASSERT_RETURN(pool && sem, PJ_EINVAL);

    up(&sem->sem);
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_sem_destroy(pj_sem_t *sem)
{
    PJ_ASSERT_RETURN(pool && sem, PJ_EINVAL);

    return PJ_SUCCESS;
}

#endif	/* PJ_HAS_SEMAPHORE */




