/* $Id: os_core_symbian.cpp 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/rand.h>
#include <pj/string.h>
#include <pj/guid.h>
#include <pj/except.h>
#include <pj/errno.h>

#include "os_symbian.h"


#define PJ_MAX_TLS	    32
#define DUMMY_MUTEX	    ((pj_mutex_t*)101)
#define DUMMY_SEMAPHORE	    ((pj_sem_t*)102)
#define THIS_FILE	    "os_core_symbian.c"

/* Default message slot number for RSocketServ::Connect().
 * Increase it to 32 from the default 8 (KESockDefaultMessageSlots)
 */
#ifndef PJ_SYMBIAN_SOCK_MSG_SLOTS
#  define PJ_SYMBIAN_SOCK_MSG_SLOTS  32
#endif

/*
 * Note:
 *
 * The Symbian implementation does not support threading!
 */ 

struct pj_thread_t
{
    char	    obj_name[PJ_MAX_OBJ_NAME];
    void	   *tls_values[PJ_MAX_TLS];

#if defined(PJ_OS_HAS_CHECK_STACK) && PJ_OS_HAS_CHECK_STACK!=0
    pj_uint32_t	    stk_size;
    pj_uint32_t	    stk_max_usage;
    char	   *stk_start;
    const char	   *caller_file;
    int		    caller_line;
#endif

} main_thread;

struct pj_atomic_t
{
    pj_atomic_value_t	value;
};

struct pj_sem_t
{
    int value;
    int max;
};

/* Flags to indicate which TLS variables have been used */
static int tls_vars[PJ_MAX_TLS];

/* atexit handlers */
static unsigned atexit_count;
static void (*atexit_func[32])(void);




/////////////////////////////////////////////////////////////////////////////
//
// CPjTimeoutTimer implementation
//

CPjTimeoutTimer::CPjTimeoutTimer()
: CActive(PJ_SYMBIAN_TIMER_PRIORITY), hasTimedOut_(PJ_FALSE)
{
}

CPjTimeoutTimer::~CPjTimeoutTimer()
{
    Cancel();
    timer_.Close();
}

void CPjTimeoutTimer::ConstructL()
{
    hasTimedOut_ = PJ_FALSE;
    timer_.CreateLocal();
    CActiveScheduler::Add(this);
}

CPjTimeoutTimer *CPjTimeoutTimer::NewL()
{
    CPjTimeoutTimer *self = new CPjTimeoutTimer;
    CleanupStack::PushL(self);

    self->ConstructL();

    CleanupStack::Pop(self);
    return self;

}

void CPjTimeoutTimer::StartTimer(TUint miliSeconds)
{
    Cancel();

    hasTimedOut_ = PJ_FALSE;
    timer_.After(iStatus, miliSeconds * 1000);
    SetActive();
}

bool CPjTimeoutTimer::HasTimedOut() const
{
    return hasTimedOut_ != 0;
}

void CPjTimeoutTimer::RunL()
{
    hasTimedOut_ = PJ_TRUE;
}

void CPjTimeoutTimer::DoCancel()
{
    timer_.Cancel();
}

TInt CPjTimeoutTimer::RunError(TInt aError)
{
    PJ_UNUSED_ARG(aError);
    return KErrNone;
}



/////////////////////////////////////////////////////////////////////////////
//
// PjSymbianOS implementation
//

PjSymbianOS::PjSymbianOS()
: isConnectionUp_(false),
  isSocketServInitialized_(false), isResolverInitialized_(false),
  console_(NULL), selectTimeoutTimer_(NULL),
  appSocketServ_(NULL), appConnection_(NULL), appHostResolver_(NULL),
  appHostResolver6_(NULL)
{
}

// Set parameters
void PjSymbianOS::SetParameters(pj_symbianos_params *params) 
{
    appSocketServ_ = (RSocketServ*) params->rsocketserv;
    appConnection_ = (RConnection*) params->rconnection;
    appHostResolver_ = (RHostResolver*) params->rhostresolver;
    appHostResolver6_ = (RHostResolver*) params->rhostresolver6;
}

// Get PjSymbianOS instance
PjSymbianOS *PjSymbianOS::Instance()
{
    static PjSymbianOS instance_;
    return &instance_;
}


// Initialize
TInt PjSymbianOS::Initialize()
{
    TInt err;

    selectTimeoutTimer_ = CPjTimeoutTimer::NewL();

#if 0
    pj_assert(console_ == NULL);
    TRAPD(err, console_ = Console::NewL(_L("PJLIB"), 
				        TSize(KConsFullScreen,KConsFullScreen)));
    return err;
#endif

    /* Only create RSocketServ if application doesn't specify it
     * in the parameters
     */
    if (!isSocketServInitialized_ && appSocketServ_ == NULL) {
	err = socketServ_.Connect(PJ_SYMBIAN_SOCK_MSG_SLOTS);
	if (err != KErrNone)
	    goto on_error;

	isSocketServInitialized_ = true;
    }

    if (!isResolverInitialized_) {
    	if (appHostResolver_ == NULL) {
    	    if (Connection())
    	    	err = hostResolver_.Open(SocketServ(), KAfInet, KSockStream,
    	    			     	 *Connection());
    	    else
	    	err = hostResolver_.Open(SocketServ(), KAfInet, KSockStream);
    	
	    if (err != KErrNone)
	    	goto on_error;
    	}
    	
#if defined(PJ_HAS_IPV6) && PJ_HAS_IPV6!=0
    	if (appHostResolver6_ == NULL) {
    	    if (Connection())
    	    	err = hostResolver6_.Open(SocketServ(), KAfInet6, KSockStream,
    	    			     	  *Connection());
    	    else
	    	err = hostResolver6_.Open(SocketServ(), KAfInet6, KSockStream);
    	
	    if (err != KErrNone)
	    	goto on_error;
    	}
#endif
    	
    	
	isResolverInitialized_ = true;
    }

    isConnectionUp_ = true;
    
    return KErrNone;

on_error:
    Shutdown();
    return err;
}

// Shutdown
void PjSymbianOS::Shutdown()
{
    isConnectionUp_ = false;
    
    if (isResolverInitialized_) {
		hostResolver_.Close();
#if defined(PJ_HAS_IPV6) && PJ_HAS_IPV6!=0
    	hostResolver6_.Close();
#endif
    	isResolverInitialized_ = false;
    }

    if (isSocketServInitialized_) {
	socketServ_.Close();
	isSocketServInitialized_ = false;
    }

    delete console_;
    console_ = NULL;

    delete selectTimeoutTimer_;
    selectTimeoutTimer_ = NULL;
    
    appSocketServ_ = NULL;
    appConnection_ = NULL;
    appHostResolver_ = NULL;
    appHostResolver6_ = NULL;
}

// Convert to Unicode
TInt PjSymbianOS::ConvertToUnicode(TDes16 &aUnicode, const TDesC8 &aForeign)
{
#if 0
    pj_assert(conv_ != NULL);
    return conv_->ConvertToUnicode(aUnicode, aForeign, convToUnicodeState_);
#else
    return CnvUtfConverter::ConvertToUnicodeFromUtf8(aUnicode, aForeign);
#endif
}

// Convert from Unicode
TInt PjSymbianOS::ConvertFromUnicode(TDes8 &aForeign, const TDesC16 &aUnicode)
{
#if 0
    pj_assert(conv_ != NULL);
    return conv_->ConvertFromUnicode(aForeign, aUnicode, convToAnsiState_);
#else
    return CnvUtfConverter::ConvertFromUnicodeToUtf8(aForeign, aUnicode);
#endif
}


/////////////////////////////////////////////////////////////////////////////
//
// PJLIB os.h implementation
//

PJ_DEF(pj_uint32_t) pj_getpid(void)
{
    return 0;
}


/* Set Symbian specific parameters */
PJ_DEF(pj_status_t) pj_symbianos_set_params(pj_symbianos_params *prm) 
{
    PJ_ASSERT_RETURN(prm != NULL, PJ_EINVAL);
    PjSymbianOS::Instance()->SetParameters(prm);
    return PJ_SUCCESS;
}


/* Set connection status */
PJ_DEF(void) pj_symbianos_set_connection_status(pj_bool_t up)
{
    PjSymbianOS::Instance()->SetConnectionStatus(up != 0);
}


/*
 * pj_init(void).
 * Init PJLIB!
 */
PJ_DEF(pj_status_t) pj_init(void)
{
	char stack_ptr;
    pj_status_t status;
    
    pj_ansi_strcpy(main_thread.obj_name, "pjthread");

    // Init main thread
    pj_memset(&main_thread, 0, sizeof(main_thread));

    // Initialize PjSymbianOS instance
    PjSymbianOS *os = PjSymbianOS::Instance();

    PJ_LOG(4,(THIS_FILE, "Initializing PJLIB for Symbian OS.."));

    TInt err; 
    err = os->Initialize();
    if (err != KErrNone)
    	return PJ_RETURN_OS_ERROR(err);

    /* Init logging */
    pj_log_init();

    /* Initialize exception ID for the pool. 
     * Must do so after critical section is configured.
     */ 
    status = pj_exception_id_alloc("PJLIB/No memory", &PJ_NO_MEMORY_EXCEPTION);
    if (status != PJ_SUCCESS)
        goto on_error;

#if defined(PJ_OS_HAS_CHECK_STACK) && PJ_OS_HAS_CHECK_STACK!=0
    main_thread.stk_start = &stack_ptr;
    main_thread.stk_size = 0xFFFFFFFFUL;
    main_thread.stk_max_usage = 0;
#else
    stack_ptr = '\0';
#endif

    PJ_LOG(5,(THIS_FILE, "PJLIB initialized."));
    return PJ_SUCCESS;

on_error:
    pj_shutdown();
    return PJ_RETURN_OS_ERROR(err);
}


PJ_DEF(pj_status_t) pj_atexit(pj_exit_callback func)
{
    if (atexit_count >= PJ_ARRAY_SIZE(atexit_func))
	return PJ_ETOOMANY;

    atexit_func[atexit_count++] = func;
    return PJ_SUCCESS;
}



PJ_DEF(void) pj_shutdown(void)
{
    /* Call atexit() functions */
    while (atexit_count > 0) {
	(*atexit_func[atexit_count-1])();
	--atexit_count;
    }

    /* Free exception ID */
    if (PJ_NO_MEMORY_EXCEPTION != -1) {
	pj_exception_id_free(PJ_NO_MEMORY_EXCEPTION);
	PJ_NO_MEMORY_EXCEPTION = -1;
    }

    /* Clear static variables */
    pj_errno_clear_handlers();

    PjSymbianOS *os = PjSymbianOS::Instance();
    os->Shutdown();
}

/////////////////////////////////////////////////////////////////////////////

class CPollTimeoutTimer : public CActive 
{
public:
    static CPollTimeoutTimer* NewL(int msec, TInt prio);
    ~CPollTimeoutTimer();
    
    virtual void RunL();
    virtual void DoCancel();

private:	
    RTimer	     rtimer_;
    
    explicit CPollTimeoutTimer(TInt prio);
    void ConstructL(int msec);
};

CPollTimeoutTimer::CPollTimeoutTimer(TInt prio)
: CActive(prio)
{
}


CPollTimeoutTimer::~CPollTimeoutTimer() 
{
    rtimer_.Close();
}

void CPollTimeoutTimer::ConstructL(int msec) 
{
    rtimer_.CreateLocal();
    CActiveScheduler::Add(this);
    rtimer_.After(iStatus, msec*1000);
    SetActive();
}

CPollTimeoutTimer* CPollTimeoutTimer::NewL(int msec, TInt prio) 
{
    CPollTimeoutTimer *self = new CPollTimeoutTimer(prio);
    CleanupStack::PushL(self);
    self->ConstructL(msec);    
    CleanupStack::Pop(self);

    return self;
}

void CPollTimeoutTimer::RunL() 
{
}

void CPollTimeoutTimer::DoCancel() 
{
     rtimer_.Cancel();
}


/*
 * Wait the completion of any Symbian active objects. 
 */
PJ_DEF(pj_bool_t) pj_symbianos_poll(int priority, int ms_timeout)
{
    CPollTimeoutTimer *timer = NULL;
    
    if (priority==-1)
    	priority = EPriorityNull;
    
    if (ms_timeout >= 0) {
    	timer = CPollTimeoutTimer::NewL(ms_timeout, priority);
    }
    
    PjSymbianOS::Instance()->WaitForActiveObjects(priority);
    
    if (timer) {
        bool timer_is_active = timer->IsActive();
    
	timer->Cancel();
        
        delete timer;
        
    	return timer_is_active ? PJ_TRUE : PJ_FALSE;
    	
    } else {
    	return PJ_TRUE;
    }
}


/*
 * pj_thread_is_registered()
 */
PJ_DEF(pj_bool_t) pj_thread_is_registered(void)
{
    return PJ_FALSE;
}


/*
 * Get thread priority value for the thread.
 */
PJ_DEF(int) pj_thread_get_prio(pj_thread_t *thread)
{
    PJ_UNUSED_ARG(thread);
    return 1;
}


/*
 * Set the thread priority.
 */
PJ_DEF(pj_status_t) pj_thread_set_prio(pj_thread_t *thread,  int prio)
{
    PJ_UNUSED_ARG(thread);
    PJ_UNUSED_ARG(prio);
    return PJ_SUCCESS;
}


/*
 * Get the lowest priority value available on this system.
 */
PJ_DEF(int) pj_thread_get_prio_min(pj_thread_t *thread)
{
    PJ_UNUSED_ARG(thread);
    return 1;
}


/*
 * Get the highest priority value available on this system.
 */
PJ_DEF(int) pj_thread_get_prio_max(pj_thread_t *thread)
{
    PJ_UNUSED_ARG(thread);
    return 1;
}


/*
 * pj_thread_get_os_handle()
 */
PJ_DEF(void*) pj_thread_get_os_handle(pj_thread_t *thread) 
{
    PJ_UNUSED_ARG(thread);
    return NULL;
}

/*
 * pj_thread_register(..)
 */
PJ_DEF(pj_status_t) pj_thread_register ( const char *cstr_thread_name,
					 pj_thread_desc desc,
                                         pj_thread_t **thread_ptr)
{
    PJ_UNUSED_ARG(cstr_thread_name);
    PJ_UNUSED_ARG(desc);
    PJ_UNUSED_ARG(thread_ptr);
    return PJ_EINVALIDOP;
}


/*
 * pj_thread_create(...)
 */
PJ_DEF(pj_status_t) pj_thread_create( pj_pool_t *pool, 
				      const char *thread_name,
				      pj_thread_proc *proc, 
				      void *arg,
				      pj_size_t stack_size, 
				      unsigned flags,
				      pj_thread_t **ptr_thread)
{
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(thread_name);
    PJ_UNUSED_ARG(proc);
    PJ_UNUSED_ARG(arg);
    PJ_UNUSED_ARG(stack_size);
    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(ptr_thread);

    /* Sorry mate, we don't support threading */
    return PJ_ENOTSUP;
}

/*
 * pj_thread-get_name()
 */
PJ_DEF(const char*) pj_thread_get_name(pj_thread_t *p)
{
    pj_assert(p == &main_thread);
    return p->obj_name;
}

/*
 * pj_thread_resume()
 */
PJ_DEF(pj_status_t) pj_thread_resume(pj_thread_t *p)
{
    PJ_UNUSED_ARG(p);
    return PJ_EINVALIDOP;
}

/*
 * pj_thread_this()
 */
PJ_DEF(pj_thread_t*) pj_thread_this(void)
{
    return &main_thread;
}

/*
 * pj_thread_join()
 */
PJ_DEF(pj_status_t) pj_thread_join(pj_thread_t *rec)
{
    PJ_UNUSED_ARG(rec);
    return PJ_EINVALIDOP;
}

/*
 * pj_thread_destroy()
 */
PJ_DEF(pj_status_t) pj_thread_destroy(pj_thread_t *rec)
{
    PJ_UNUSED_ARG(rec);
    return PJ_EINVALIDOP;
}

/*
 * pj_thread_sleep()
 */
PJ_DEF(pj_status_t) pj_thread_sleep(unsigned msec)
{
    User::After(msec*1000);

    return PJ_SUCCESS;
}


///////////////////////////////////////////////////////////////////////////////
/*
 * pj_thread_local_alloc()
 */

PJ_DEF(pj_status_t) pj_thread_local_alloc(long *index)
{
    unsigned i;

    /* Find unused TLS variable */
    for (i=0; i<PJ_ARRAY_SIZE(tls_vars); ++i) {
	if (tls_vars[i] == 0)
	    break;
    }

    if (i == PJ_ARRAY_SIZE(tls_vars))
	return PJ_ETOOMANY;

    tls_vars[i] = 1;
    *index = i;

    return PJ_SUCCESS;
}

/*
 * pj_thread_local_free()
 */
PJ_DEF(void) pj_thread_local_free(long index)
{
    PJ_ASSERT_ON_FAIL(index >= 0 && index < (int)PJ_ARRAY_SIZE(tls_vars) &&
		     tls_vars[index] != 0, return);

    tls_vars[index] = 0;
}


/*
 * pj_thread_local_set()
 */
PJ_DEF(pj_status_t) pj_thread_local_set(long index, void *value)
{
    pj_thread_t *rec = pj_thread_this();

    PJ_ASSERT_RETURN(index >= 0 && index < (int)PJ_ARRAY_SIZE(tls_vars) &&
		     tls_vars[index] != 0, PJ_EINVAL);

    rec->tls_values[index] = value;
    return PJ_SUCCESS;
}

/*
 * pj_thread_local_get()
 */
PJ_DEF(void*) pj_thread_local_get(long index)
{
    pj_thread_t *rec = pj_thread_this();

    PJ_ASSERT_RETURN(index >= 0 && index < (int)PJ_ARRAY_SIZE(tls_vars) &&
		     tls_vars[index] != 0, NULL);

    return rec->tls_values[index];
}


///////////////////////////////////////////////////////////////////////////////
/*
 * Create atomic variable.
 */
PJ_DEF(pj_status_t) pj_atomic_create( pj_pool_t *pool, 
				      pj_atomic_value_t initial,
				      pj_atomic_t **atomic )
{
    *atomic = (pj_atomic_t*)pj_pool_alloc(pool, sizeof(struct pj_atomic_t));
    (*atomic)->value = initial;
    return PJ_SUCCESS;
}


/*
 * Destroy atomic variable.
 */
PJ_DEF(pj_status_t) pj_atomic_destroy( pj_atomic_t *atomic_var )
{
    PJ_UNUSED_ARG(atomic_var);
    return PJ_SUCCESS;
}


/*
 * Set the value of an atomic type, and return the previous value.
 */
PJ_DEF(void) pj_atomic_set( pj_atomic_t *atomic_var, 
			    pj_atomic_value_t value)
{
    atomic_var->value = value;
}


/*
 * Get the value of an atomic type.
 */
PJ_DEF(pj_atomic_value_t) pj_atomic_get(pj_atomic_t *atomic_var)
{
    return atomic_var->value;
}


/*
 * Increment the value of an atomic type.
 */
PJ_DEF(void) pj_atomic_inc(pj_atomic_t *atomic_var)
{
    ++atomic_var->value;
}


/*
 * Increment the value of an atomic type and get the result.
 */
PJ_DEF(pj_atomic_value_t) pj_atomic_inc_and_get(pj_atomic_t *atomic_var)
{
    return ++atomic_var->value;
}


/*
 * Decrement the value of an atomic type.
 */
PJ_DEF(void) pj_atomic_dec(pj_atomic_t *atomic_var)
{
    --atomic_var->value;
}	


/*
 * Decrement the value of an atomic type and get the result.
 */
PJ_DEF(pj_atomic_value_t) pj_atomic_dec_and_get(pj_atomic_t *atomic_var)
{
    return --atomic_var->value;
}


/*
 * Add a value to an atomic type.
 */
PJ_DEF(void) pj_atomic_add( pj_atomic_t *atomic_var,
			    pj_atomic_value_t value)
{
    atomic_var->value += value;
}


/*
 * Add a value to an atomic type and get the result.
 */
PJ_DEF(pj_atomic_value_t) pj_atomic_add_and_get( pj_atomic_t *atomic_var,
			                         pj_atomic_value_t value)
{
    atomic_var->value += value;
    return atomic_var->value;
}



/////////////////////////////////////////////////////////////////////////////

PJ_DEF(pj_status_t) pj_mutex_create( pj_pool_t *pool, 
                                     const char *name,
				     int type, 
                                     pj_mutex_t **mutex)
{
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(name);
    PJ_UNUSED_ARG(type);

    *mutex = DUMMY_MUTEX;
    return PJ_SUCCESS;
}

/*
 * pj_mutex_create_simple()
 */
PJ_DEF(pj_status_t) pj_mutex_create_simple( pj_pool_t *pool, 
                                            const char *name,
					    pj_mutex_t **mutex )
{
    return pj_mutex_create(pool, name, PJ_MUTEX_SIMPLE, mutex);
}


PJ_DEF(pj_status_t) pj_mutex_create_recursive( pj_pool_t *pool,
					       const char *name,
					       pj_mutex_t **mutex )
{
    return pj_mutex_create(pool, name, PJ_MUTEX_RECURSE, mutex);
}


/*
 * pj_mutex_lock()
 */
PJ_DEF(pj_status_t) pj_mutex_lock(pj_mutex_t *mutex)
{
    pj_assert(mutex == DUMMY_MUTEX);
    return PJ_SUCCESS;
}

/*
 * pj_mutex_trylock()
 */
PJ_DEF(pj_status_t) pj_mutex_trylock(pj_mutex_t *mutex)
{
    pj_assert(mutex == DUMMY_MUTEX);
    return PJ_SUCCESS;
}

/*
 * pj_mutex_unlock()
 */
PJ_DEF(pj_status_t) pj_mutex_unlock(pj_mutex_t *mutex)
{
    pj_assert(mutex == DUMMY_MUTEX);
    return PJ_SUCCESS;
}

/*
 * pj_mutex_destroy()
 */
PJ_DEF(pj_status_t) pj_mutex_destroy(pj_mutex_t *mutex)
{
    pj_assert(mutex == DUMMY_MUTEX);
    return PJ_SUCCESS;
}


/////////////////////////////////////////////////////////////////////////////
/*
 * RW Mutex
 */
#include "os_rwmutex.c"


/////////////////////////////////////////////////////////////////////////////

/*
 * Enter critical section.
 */
PJ_DEF(void) pj_enter_critical_section(void)
{
    /* Nothing to do */
}


/*
 * Leave critical section.
 */
PJ_DEF(void) pj_leave_critical_section(void)
{
    /* Nothing to do */
}


/////////////////////////////////////////////////////////////////////////////

/*
 * Create semaphore.
 */
PJ_DEF(pj_status_t) pj_sem_create( pj_pool_t *pool, 
                                   const char *name,
				   unsigned initial, 
                                   unsigned max,
				   pj_sem_t **p_sem)
{
    pj_sem_t *sem;
 
    PJ_UNUSED_ARG(name);

    sem = (pj_sem_t*) pj_pool_zalloc(pool, sizeof(pj_sem_t));
    sem->value = initial;
    sem->max = max;

    *p_sem = sem;

    return PJ_SUCCESS;
}


/*
 * Wait for semaphore.
 */
PJ_DEF(pj_status_t) pj_sem_wait(pj_sem_t *sem)
{
    if (sem->value > 0) {
	sem->value--;
	return PJ_SUCCESS;
    } else {
	pj_assert(!"Unexpected!");
	return PJ_EINVALIDOP;
    }
}


/*
 * Try wait for semaphore.
 */
PJ_DEF(pj_status_t) pj_sem_trywait(pj_sem_t *sem)
{
    if (sem->value > 0) {
	sem->value--;
	return PJ_SUCCESS;
    } else {
	pj_assert(!"Unexpected!");
	return PJ_EINVALIDOP;
    }
}


/*
 * Release semaphore.
 */
PJ_DEF(pj_status_t) pj_sem_post(pj_sem_t *sem)
{
    sem->value++;
    return PJ_SUCCESS;
}


/*
 * Destroy semaphore.
 */
PJ_DEF(pj_status_t) pj_sem_destroy(pj_sem_t *sem)
{
    PJ_UNUSED_ARG(sem);
    return PJ_SUCCESS;
}


#if defined(PJ_OS_HAS_CHECK_STACK) && PJ_OS_HAS_CHECK_STACK != 0
/*
 * The implementation of stack checking. 
 */
PJ_DEF(void) pj_thread_check_stack(const char *file, int line)
{
    char stk_ptr;
    pj_uint32_t usage;
    pj_thread_t *thread = pj_thread_this();

    pj_assert(thread);

    /* Calculate current usage. */
    usage = (&stk_ptr > thread->stk_start) ? &stk_ptr - thread->stk_start :
		thread->stk_start - &stk_ptr;

    /* Assert if stack usage is dangerously high. */
    pj_assert("STACK OVERFLOW!! " && (usage <= thread->stk_size - 128));

    /* Keep statistic. */
    if (usage > thread->stk_max_usage) {
	thread->stk_max_usage = usage;
	thread->caller_file = file;
	thread->caller_line = line;
    }
}

/*
 * Get maximum stack usage statistic. 
 */
PJ_DEF(pj_uint32_t) pj_thread_get_stack_max_usage(pj_thread_t *thread)
{
    return thread->stk_max_usage;
}

/*
 * Dump thread stack status. 
 */
PJ_DEF(pj_status_t) pj_thread_get_stack_info(pj_thread_t *thread,
					     const char **file,
					     int *line)
{
    pj_assert(thread);

    *file = thread->caller_file;
    *line = thread->caller_line;
    return 0;
}

#endif	/* PJ_OS_HAS_CHECK_STACK */
