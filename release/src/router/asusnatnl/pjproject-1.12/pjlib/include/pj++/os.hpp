/* $Id: os.hpp 2394 2008-12-23 17:27:53Z bennylp $ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJPP_OS_HPP__
#define __PJPP_OS_HPP__

#include <pj/os.h>
#include <pj/string.h>
#include <pj++/types.hpp>
#include <pj++/pool.hpp>

class Pj_Thread;

//
// Thread API.
//
class Pj_Thread_API
{
public:
    //
    // Create a thread.
    //
    static pj_status_t create( Pj_Pool *pool, pj_thread_t **thread,
                               pj_thread_proc *proc, void *arg,
                               unsigned flags = 0,
                               const char *name = NULL,
                               pj_size_t stack_size = 0 )
    {
        return pj_thread_create(pool->pool_(), name, proc, arg, stack_size,
                                flags, thread);
    }

    //
    // Register a thread.
    //
    static pj_status_t register_this_thread( pj_thread_desc desc,
                                             pj_thread_t **thread,
                                             const char *name = NULL )
    {
        return pj_thread_register( name, desc, thread );
    }

    //
    // Get current thread.
    // Will return pj_thread_t (sorry folks, not Pj_Thread).
    //
    static pj_thread_t *this_thread()
    {
        return pj_thread_this();
    }

    //
    // Get thread name.
    //
    static const char *get_name(pj_thread_t *thread)
    {
        return pj_thread_get_name(thread);
    }

    //
    // Resume thread.
    //
    static pj_status_t resume(pj_thread_t *thread)
    {
        return pj_thread_resume(thread);
    }

    //
    // Sleep.
    //
    static pj_status_t sleep(unsigned msec)
    {
	return pj_thread_sleep(msec);
    }

    //
    // Join the specified thread.
    //
    static pj_status_t join(pj_thread_t *thread)
    {
        return pj_thread_join(thread);
    }

    //
    // Destroy thread
    //
    static pj_status_t destroy(pj_thread_t *thread)
    {
        return pj_thread_destroy(thread);
    }
};



//
// Thread object.
//
// How to use:
//  Derive a class from this class, then override main().
//
class Pj_Thread : public Pj_Object
{
public:
    enum Flags
    {
	FLAG_SUSPENDED = PJ_THREAD_SUSPENDED
    };

    //
    // Default constructor.
    //
    Pj_Thread()
        : thread_(NULL)
    {
    }

    //
    // Destroy thread.
    //
    ~Pj_Thread()
    {
        destroy();
    }

    //
    // This is the main thread function.
    //
    virtual int main() = 0;

    //
    // Start a thread.
    //
    pj_status_t create( Pj_Pool *pool, 
                        unsigned flags = 0,
                        const char *thread_name = NULL,
			pj_size_t stack_size = PJ_THREAD_DEFAULT_STACK_SIZE)
    {
        destroy();
        return Pj_Thread_API::create( pool, &thread_, &thread_proc, this,
                                      flags, thread_name, stack_size);
    }

    //
    // Get pjlib compatible thread object.
    //
    pj_thread_t *pj_thread_t_()
    {
	return thread_;
    }

    //
    // Get thread name.
    //
    const char *get_name()
    {
        return Pj_Thread_API::get_name(thread_);
    }

    //
    // Resume a suspended thread.
    //
    pj_status_t resume()
    {
        return Pj_Thread_API::resume(thread_);
    }

    //
    // Join this thread.
    //
    pj_status_t join()
    {
        return Pj_Thread_API::join(thread_);
    }

    //
    // Destroy thread.
    //
    pj_status_t destroy()
    {
        if (thread_) {
            Pj_Thread_API::destroy(thread_);
            thread_ = NULL;
        }
    }

protected:
    pj_thread_t *thread_;

    static int PJ_THREAD_FUNC thread_proc(void *obj)
    {
        Pj_Thread *thread_class = (Pj_Thread*)obj;
        return thread_class->main();
    }
};


//
// External Thread
//  (threads that were started by external means, i.e. not 
//   with Pj_Thread::create).
//
// This class will normally be defined as local variable in
// external thread's stack, normally inside thread's main proc.
// But be aware that the handle will be destroyed on destructor!
//
class Pj_External_Thread : public Pj_Thread
{
public:
    Pj_External_Thread()
    {
    }

    //
    // Register external thread so that pjlib functions can work
    // in that thread.
    //
    pj_status_t register_this_thread( const char *name=NULL )
    {
        return Pj_Thread_API::register_this_thread(desc_, &thread_,name);
    }

private:
    pj_thread_desc desc_;
};


//
// Thread specific data/thread local storage/TLS.
//
class Pj_Thread_Local_API
{
public:
    //
    // Allocate thread local storage (TLS) index.
    //
    static pj_status_t alloc(long *index)
    {
        return pj_thread_local_alloc(index);
    }

    //
    // Free TLS index.
    //
    static void free(long index)
    {
        pj_thread_local_free(index);
    }

    //
    // Set thread specific data.
    //
    static pj_status_t set(long index, void *value)
    {
        return pj_thread_local_set(index, value);
    }

    //
    // Get thread specific data.
    //
    static void *get(long index)
    {
        return pj_thread_local_get(index);
    }

};

//
// Atomic variable
//
// How to use:
//   Pj_Atomic_Var var(pool, 0);
//   var.set(..);
//
class Pj_Atomic_Var : public Pj_Object
{
public:
    //
    // Default constructor, initialize variable with NULL.
    //
    Pj_Atomic_Var()
        : var_(NULL)
    {
    }

    //
    // Construct atomic variable.
    //
    Pj_Atomic_Var(Pj_Pool *pool, pj_atomic_value_t value)
        : var_(NULL)
    {
        create(pool, value);
    }

    //
    // Destructor.
    //
    ~Pj_Atomic_Var()
    {
        destroy();
    }

    //
    // Create atomic variable.
    //
    pj_status_t create( Pj_Pool *pool, pj_atomic_value_t value)
    {
        destroy();
	return pj_atomic_create(pool->pool_(), value, &var_);
    }

    //
    // Destroy.
    //
    void destroy()
    {
        if (var_) {
            pj_atomic_destroy(var_);
            var_ = NULL;
        }
    }

    //
    // Get pjlib compatible atomic variable.
    //
    pj_atomic_t *pj_atomic_t_()
    {
	return var_;
    }

    //
    // Set the value.
    //
    void set(pj_atomic_value_t val)
    {
	pj_atomic_set(var_, val);
    }

    //
    // Get the value.
    //
    pj_atomic_value_t get()
    {
	return pj_atomic_get(var_);
    }

    //
    // Increment.
    //
    void inc()
    {
	pj_atomic_inc(var_);
    }

    //
    // Increment and get the result.
    //
    pj_atomic_value_t inc_and_get()
    {
        return pj_atomic_inc_and_get(var_);
    }

    //
    // Decrement.
    //
    void dec()
    {
	pj_atomic_dec(var_);
    }

    //
    // Decrement and get the result.
    //
    pj_atomic_value_t dec_and_get()
    {
        return pj_atomic_dec_and_get(var_);
    }

    //
    // Add the variable.
    //
    void add(pj_atomic_value_t value)
    {
        pj_atomic_add(var_, value);
    }

    //
    // Add the variable and get the value.
    //
    pj_atomic_value_t add_and_get(pj_atomic_value_t value)
    {
        return pj_atomic_add_and_get(var_, value );
    }

private:
    pj_atomic_t *var_;
};


//
// Mutex
//
class Pj_Mutex : public Pj_Object
{
public:
    //
    // Mutex type.
    //
    enum Type
    {
	DEFAULT = PJ_MUTEX_DEFAULT,
	SIMPLE = PJ_MUTEX_SIMPLE,
	RECURSE = PJ_MUTEX_RECURSE,
    };

    //
    // Default constructor will create default mutex.
    //
    explicit Pj_Mutex(Pj_Pool *pool, Type type = DEFAULT,
                      const char *name = NULL)
        : mutex_(NULL)
    {
        create(pool, type, name);
    }

    //
    // Destructor.
    //
    ~Pj_Mutex()
    {
        destroy();
    }

    //
    // Create mutex.
    //
    pj_status_t create( Pj_Pool *pool, Type type, const char *name = NULL)
    {
        destroy();
	return pj_mutex_create( pool->pool_(), name, type,
                                &mutex_ );
    }

    //
    // Create simple mutex.
    //
    pj_status_t create_simple( Pj_Pool *pool,const char *name = NULL)
    {
        return create(pool, SIMPLE, name);
    }

    //
    // Create recursive mutex.
    //
    pj_status_t create_recursive( Pj_Pool *pool, const char *name = NULL )
    {
        return create(pool, RECURSE, name);
    }

    //
    // Get pjlib compatible mutex object.
    //
    pj_mutex_t *pj_mutex_t_()
    {
	return mutex_;
    }

    //
    // Destroy mutex.
    //
    void destroy()
    {
        if (mutex_) {
	    pj_mutex_destroy(mutex_);
            mutex_ = NULL;
        }
    }

    //
    // Lock mutex.
    //
    pj_status_t acquire()
    {
	return pj_mutex_lock(mutex_);
    }

    //
    // Unlock mutex.
    //
    pj_status_t release()
    {
	return pj_mutex_unlock(mutex_);
    }

    //
    // Try locking the mutex.
    //
    pj_status_t tryacquire()
    {
	return pj_mutex_trylock(mutex_);
    }

private:
    pj_mutex_t *mutex_;
};


//
// Semaphore
//
class Pj_Semaphore : public Pj_Object
{
public:
    //
    // Construct semaphore
    //
    Pj_Semaphore(Pj_Pool *pool, unsigned max,
                 unsigned initial = 0, const char *name = NULL)
    : sem_(NULL)
    {
	create(pool, max, initial, name);
    }

    //
    // Destructor.
    //
    ~Pj_Semaphore()
    {
        destroy();
    }

    //
    // Create semaphore
    //
    pj_status_t create( Pj_Pool *pool, unsigned max,
                        unsigned initial = 0, const char *name = NULL )
    {
        destroy();
	return pj_sem_create( pool->pool_(), name, initial, max, &sem_);
    }

    //
    // Destroy semaphore.
    //
    void destroy()
    {
        if (sem_) {
            pj_sem_destroy(sem_);
            sem_ = NULL;
        }
    }

    //
    // Get pjlib compatible semaphore object.
    //
    pj_sem_t *pj_sem_t_()
    {
	return (pj_sem_t*)this;
    }

    //
    // Wait semaphore.
    //
    pj_status_t wait()
    {
	return pj_sem_wait(this->pj_sem_t_());
    }

    //
    // Wait semaphore.
    //
    pj_status_t acquire()
    {
	return wait();
    }

    //
    // Try wait semaphore.
    //
    pj_status_t trywait()
    {
	return pj_sem_trywait(this->pj_sem_t_());
    }

    //
    // Try wait semaphore.
    //
    pj_status_t tryacquire()
    {
	return trywait();
    }

    //
    // Post semaphore.
    //
    pj_status_t post()
    {
	return pj_sem_post(this->pj_sem_t_());
    }

    //
    // Post semaphore.
    //
    pj_status_t release()
    {
	return post();
    }

private:
    pj_sem_t *sem_;
};


//
// Event object.
//
class Pj_Event
{
public:
    //
    // Construct event object.
    //
    Pj_Event( Pj_Pool *pool, bool manual_reset = false,
              bool initial = false, const char *name = NULL )
    : event_(NULL)
    {
        create(pool, manual_reset, initial, name);
    }

    //
    // Destructor.
    //
    ~Pj_Event()
    {
        destroy();
    }

    //
    // Create event object.
    //
    pj_status_t create( Pj_Pool *pool, bool manual_reset = false, 
                        bool initial = false, const char *name = NULL)
    {
        destroy();
	return pj_event_create(pool->pool_(), name, manual_reset, initial,
                               &event_);
    }

    //
    // Get pjlib compatible event object.
    //
    pj_event_t *pj_event_t_()
    {
	return event_;
    }

    //
    // Destroy event object.
    //
    void destroy()
    {
        if (event_) {
	    pj_event_destroy(event_);
            event_ = NULL;
        }
    }

    //
    // Wait.
    //
    pj_status_t wait()
    {
	return pj_event_wait(event_);
    }

    //
    // Try wait.
    //
    pj_status_t trywait()
    {
	return pj_event_trywait(event_);
    }

    //
    // Set event state to signalled.
    //
    pj_status_t set()
    {
	return pj_event_set(this->pj_event_t_());
    }

    //
    // Release one waiting thread.
    //
    pj_status_t pulse()
    {
	return pj_event_pulse(this->pj_event_t_());
    }

    //
    // Set a non-signalled.
    //
    pj_status_t reset()
    {
	return pj_event_reset(this->pj_event_t_());
    }

private:
    pj_event_t *event_;
};

//
// Timestamp
//
class Pj_Timestamp
{
public:
    pj_status_t get_timestamp()
    {
	return pj_get_timestamp(&ts_);
    }

    Pj_Timestamp& operator += (const Pj_Timestamp &rhs)
    {
	pj_add_timestamp(&ts_, &rhs.ts_);
	return *this;
    }

    Pj_Timestamp& operator -= (const Pj_Timestamp &rhs)
    {
	pj_sub_timestamp(&ts_, &rhs.ts_);
	return *this;
    }

    Pj_Time_Val to_time() const
    {
	Pj_Timestamp zero;
	pj_memset(&zero, 0, sizeof(zero));
	return Pj_Time_Val(pj_elapsed_time(&zero.ts_, &ts_));
    }

    pj_uint32_t to_msec() const
    {
	Pj_Timestamp zero;
	pj_memset(&zero, 0, sizeof(zero));
	return pj_elapsed_msec(&zero.ts_, &ts_);
    }

    pj_uint32_t to_usec() const
    {
	Pj_Timestamp zero;
	pj_memset(&zero, 0, sizeof(zero));
	return pj_elapsed_usec(&zero.ts_, &ts_);
    }

    pj_uint32_t to_nanosec() const
    {
	Pj_Timestamp zero;
	pj_memset(&zero, 0, sizeof(zero));
	return pj_elapsed_nanosec(&zero.ts_, &ts_);
    }

    pj_uint32_t to_cycle() const
    {
	Pj_Timestamp zero;
	pj_memset(&zero, 0, sizeof(zero));
	return pj_elapsed_cycle(&zero.ts_, &ts_);
    }

private:
    pj_timestamp    ts_;
};


//
// OS abstraction.
//
class Pj_OS_API
{
public:
    //
    // Get current time.
    //
    static pj_status_t gettimeofday( Pj_Time_Val *tv )
    {
	return pj_gettimeofday(tv);
    }

    //
    // Parse to time of day.
    //
    static pj_status_t time_decode( const Pj_Time_Val *tv, 
                                    pj_parsed_time *pt )
    {
	return pj_time_decode(tv, pt);
    }

    //
    // Parse from time of day.
    //
    static pj_status_t time_encode( const pj_parsed_time *pt, 
                                    Pj_Time_Val *tv)
    {
	return pj_time_encode(pt, tv);
    }

    //
    // Convert to GMT.
    //
    static pj_status_t time_local_to_gmt( Pj_Time_Val *tv )
    {
	return pj_time_local_to_gmt( tv );
    }

    //
    // Convert time to local.
    //
    static pj_status_t time_gmt_to_local( Pj_Time_Val *tv) 
    {
	return pj_time_gmt_to_local( tv );
    }
};

//
// Timeval inlines.
//
inline pj_status_t Pj_Time_Val::gettimeofday()
{
    return Pj_OS_API::gettimeofday(this);
}

inline pj_parsed_time Pj_Time_Val::decode()
{
    pj_parsed_time pt;
    Pj_OS_API::time_decode(this, &pt);
    return pt;
}

inline pj_status_t Pj_Time_Val::encode(const pj_parsed_time *pt)
{
    return Pj_OS_API::time_encode(pt, this);
}

inline pj_status_t Pj_Time_Val::to_gmt()
{
    return Pj_OS_API::time_local_to_gmt(this);
}

inline pj_status_t Pj_Time_Val::to_local()
{
    return Pj_OS_API::time_gmt_to_local(this);
}

#endif	/* __PJPP_OS_HPP__ */

