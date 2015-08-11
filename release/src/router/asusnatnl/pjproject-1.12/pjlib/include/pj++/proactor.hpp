/* $Id: proactor.hpp 2394 2008-12-23 17:27:53Z bennylp $ */
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
#ifndef __PJPP_PROACTOR_HPP__
#define __PJPP_PROACTOR_HPP__

#include <pj/ioqueue.h>
#include <pj++/pool.hpp>
#include <pj++/sock.hpp>
#include <pj++/timer.hpp>
#include <pj/errno.h>

class Pj_Proactor;
class Pj_Event_Handler;


//////////////////////////////////////////////////////////////////////////////
// Asynchronous operation key.
//
// Applications may inheric this class to put their application
// specific data.
//
class Pj_Async_Op : public pj_ioqueue_op_key_t
{
public:
    //
    // Construct with null handler.
    // App must call set_handler() before use.
    //
    Pj_Async_Op()
        : handler_(NULL)
    {
	pj_ioqueue_op_key_init(this, sizeof(*this));
    }

    //
    // Constructor.
    //
    explicit Pj_Async_Op(Pj_Event_Handler *handler)
        : handler_(handler)
    {
	pj_ioqueue_op_key_init(this, sizeof(*this));
    }

    //
    // Set handler.
    //
    void set_handler(Pj_Event_Handler *handler)
    {
        handler_ = handler;
    }

    //
    // Check whether operation is still pending for this key.
    //
    bool is_pending();

    //
    // Cancel the operation.
    //
    bool cancel(pj_ssize_t bytes_status=-PJ_ECANCELLED);

protected:
    Pj_Event_Handler *handler_;
};


//////////////////////////////////////////////////////////////////////////////
// Event handler.
//
// Applications should inherit this class to receive various event
// notifications.
//
// Applications should implement get_socket_handle().
//
class Pj_Event_Handler : public Pj_Object
{
    friend class Pj_Proactor;
public:
    //
    // Default constructor.
    //
    Pj_Event_Handler()
        : key_(NULL)
    {
        pj_memset(&timer_, 0, sizeof(timer_));
        timer_.user_data = this;
        timer_.cb = &timer_callback;
    }
    
    //
    // Destroy.
    //
    virtual ~Pj_Event_Handler()
    {
        unregister();
    }

    //
    // Unregister this handler from the ioqueue.
    //
    void unregister()
    {
        if (key_) {
            pj_ioqueue_unregister(key_);
            key_ = NULL;
        }
    }

    //
    // Get socket handle associated with this.
    //
    virtual pj_sock_t get_socket_handle()
    {
        return PJ_INVALID_SOCKET;
    }

    //
    // Start async receive.
    //
    pj_status_t recv( Pj_Async_Op *op_key, 
                      void *buf, pj_ssize_t *len, 
                      unsigned flags)
    {
        return pj_ioqueue_recv( key_, op_key,
                                buf, len, flags);
    }

    //
    // Start async recvfrom()
    //
    pj_status_t recvfrom( Pj_Async_Op *op_key, 
                          void *buf, pj_ssize_t *len, unsigned flags,
                          Pj_Inet_Addr *addr)
    {
        addr->addrlen_ = sizeof(Pj_Inet_Addr);
        return pj_ioqueue_recvfrom( key_, op_key, buf, len, flags,
                                    addr, &addr->addrlen_ );
    }

    //
    // Start async send()
    //
    pj_status_t send( Pj_Async_Op *op_key, 
                      const void *data, pj_ssize_t *len, 
                      unsigned flags)
    {
        return pj_ioqueue_send( key_, op_key, data, len, flags);
    }

    //
    // Start async sendto()
    //
    pj_status_t sendto( Pj_Async_Op *op_key,
                        const void *data, pj_ssize_t *len, unsigned flags,
                        const Pj_Inet_Addr &addr)
    {
        return pj_ioqueue_sendto(key_, op_key, data, len, flags,
                                 &addr, sizeof(addr));
    }

#if PJ_HAS_TCP
    //
    // Start async connect()
    //
    pj_status_t connect(const Pj_Inet_Addr &addr)
    {
        return pj_ioqueue_connect(key_, &addr, sizeof(addr));
    }

    //
    // Start async accept().
    //
    pj_status_t accept( Pj_Async_Op *op_key,
                        Pj_Socket *sock, 
                        Pj_Inet_Addr *local = NULL, 
                        Pj_Inet_Addr *remote = NULL)
    {
        int *addrlen = local ? &local->addrlen_ : NULL;
        return pj_ioqueue_accept( key_, op_key, &sock->sock_,
                                  local, remote, addrlen );
    }

#endif

protected:
    //////////////////
    // Overridables
    //////////////////

    //
    // Timeout callback.
    //
    virtual void on_timeout(int) 
    {
    }

    //
    // On read complete callback.
    //
    virtual void on_read_complete( Pj_Async_Op*, pj_ssize_t) 
    {
    }

    //
    // On write complete callback.
    //
    virtual void on_write_complete( Pj_Async_Op *, pj_ssize_t) 
    {
    }

#if PJ_HAS_TCP
    //
    // On connect complete callback.
    //
    virtual void on_connect_complete(pj_status_t) 
    {
    }

    //
    // On new connection callback.
    //
    virtual void on_accept_complete( Pj_Async_Op*, pj_sock_t, pj_status_t) 
    {
    }

#endif


private:
    pj_ioqueue_key_t *key_;
    pj_timer_entry    timer_;

    friend class Pj_Proactor;
    friend class Pj_Async_Op;

    //
    // Static timer callback.
    //
    static void timer_callback( pj_timer_heap_t*, 
                                struct pj_timer_entry *entry)
    {
        Pj_Event_Handler *handler = 
            (Pj_Event_Handler*) entry->user_data;

        handler->on_timeout(entry->id);
    }
};

inline bool Pj_Async_Op::is_pending()
{
    return pj_ioqueue_is_pending(handler_->key_, this) != 0;
}

inline bool Pj_Async_Op::cancel(pj_ssize_t bytes_status)
{
    return pj_ioqueue_post_completion(handler_->key_, this, 
                                      bytes_status) == PJ_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
// Proactor
//
class Pj_Proactor : public Pj_Object
{
public:
    //
    // Default constructor, initializes to NULL.
    //
    Pj_Proactor()
        : ioq_(NULL), th_(NULL)
    {
        cb_.on_read_complete    = &read_complete_cb;
        cb_.on_write_complete   = &write_complete_cb;
        cb_.on_accept_complete  = &accept_complete_cb;
        cb_.on_connect_complete = &connect_complete_cb;
    }

    //
    // Construct proactor.
    //
    Pj_Proactor( Pj_Pool *pool, pj_size_t max_fd,
                 pj_size_t max_timer_entries )
    : ioq_(NULL), th_(NULL)
    {
        cb_.on_read_complete    = &read_complete_cb;
        cb_.on_write_complete   = &write_complete_cb;
        cb_.on_accept_complete  = &accept_complete_cb;
        cb_.on_connect_complete = &connect_complete_cb;

        create(pool, max_fd, max_timer_entries);
    }

    //
    // Destructor.
    //
    ~Pj_Proactor()
    {
        destroy();
    }

    //
    // Create proactor.
    //
    pj_status_t create( Pj_Pool *pool, pj_size_t max_fd, 
			pj_size_t timer_entry_count)
    {
        pj_status_t status;

        destroy();

        status = pj_ioqueue_create(pool->pool_(), max_fd, &ioq_);
        if (status != PJ_SUCCESS) 
            return status;
        
        status = pj_timer_heap_create(pool->pool_(), 
                                      timer_entry_count, &th_);
        if (status != PJ_SUCCESS) {
            pj_ioqueue_destroy(ioq_);
            ioq_ = NULL;
            return NULL;
        }
        
        return status;
    }

    //
    // Destroy proactor.
    //
    void destroy()
    {
        if (ioq_) {
            pj_ioqueue_destroy(ioq_);
            ioq_ = NULL;
        }
        if (th_) {
            pj_timer_heap_destroy(th_);
            th_ = NULL;
        }
    }

    //
    // Register handler.
    // This will call handler->get_socket_handle()
    //
    pj_status_t register_socket_handler(Pj_Pool *pool, 
                                        Pj_Event_Handler *handler)
    {
        return   pj_ioqueue_register_sock( pool->pool_(), ioq_,
                                           handler->get_socket_handle(),
                                           handler, &cb_, &handler->key_ );
    }

    //
    // Unregister handler.
    //
    static void unregister_handler(Pj_Event_Handler *handler)
    {
        if (handler->key_) {
            pj_ioqueue_unregister( handler->key_ );
            handler->key_ = NULL;
        }
    }

    //
    // Scheduler timer.
    //
    bool schedule_timer( Pj_Event_Handler *handler, 
                         const Pj_Time_Val &delay, 
                         int id=-1)
    {
        return schedule_timer(th_, handler, delay, id);
    }

    //
    // Cancel timer.
    //
    bool cancel_timer(Pj_Event_Handler *handler)
    {
        return pj_timer_heap_cancel(th_, &handler->timer_) == 1;
    }

    //
    // Handle events.
    //
    int handle_events(Pj_Time_Val *max_timeout)
    {
        Pj_Time_Val timeout(0, 0);
        int timer_count;

        timer_count = pj_timer_heap_poll( th_, &timeout );

        if (timeout.get_sec() < 0) 
            timeout.sec = PJ_MAXINT32;

        /* If caller specifies maximum time to wait, then compare the value 
         * with the timeout to wait from timer, and use the minimum value.
         */
        if (max_timeout && timeout >= *max_timeout) {
	    timeout = *max_timeout;
        }

        /* Poll events in ioqueue. */
        int ioqueue_count;

        ioqueue_count = pj_ioqueue_poll(ioq_, &timeout);
        if (ioqueue_count < 0)
	    return ioqueue_count;

        return ioqueue_count + timer_count;
    }

    //
    // Get the internal ioqueue object.
    //
    pj_ioqueue_t *get_io_queue()
    {
        return ioq_;
    }

    //
    // Get the internal timer heap object.
    //
    pj_timer_heap_t *get_timer_heap()
    {
        return th_;
    }

private:
    pj_ioqueue_t *ioq_;
    pj_timer_heap_t *th_;
    pj_ioqueue_callback cb_;

    static bool schedule_timer( pj_timer_heap_t *timer, 
                                Pj_Event_Handler *handler,
				const Pj_Time_Val &delay, 
                                int id=-1)
    {
        handler->timer_.id = id;
        return pj_timer_heap_schedule(timer, &handler->timer_, &delay) == 0;
    }


    //
    // Static read completion callback.
    //
    static void read_complete_cb( pj_ioqueue_key_t *key, 
                                  pj_ioqueue_op_key_t *op_key, 
                                  pj_ssize_t bytes_read)
    {
        Pj_Event_Handler *handler = 
	    (Pj_Event_Handler*) pj_ioqueue_get_user_data(key);

        handler->on_read_complete((Pj_Async_Op*)op_key, bytes_read);
    }

    //
    // Static write completion callback.
    //
    static void write_complete_cb(pj_ioqueue_key_t *key, 
                                  pj_ioqueue_op_key_t *op_key,
                                  pj_ssize_t bytes_sent)
    {
        Pj_Event_Handler *handler = 
	    (Pj_Event_Handler*) pj_ioqueue_get_user_data(key);

        handler->on_write_complete((Pj_Async_Op*)op_key, bytes_sent);
    }

    //
    // Static accept completion callback.
    //
    static void accept_complete_cb(pj_ioqueue_key_t *key, 
                                   pj_ioqueue_op_key_t *op_key,
                                   pj_sock_t new_sock,
                                   pj_status_t status)
    {
        Pj_Event_Handler *handler = 
	    (Pj_Event_Handler*) pj_ioqueue_get_user_data(key);

        handler->on_accept_complete((Pj_Async_Op*)op_key, new_sock, status);
    }

    //
    // Static connect completion callback.
    //
    static void connect_complete_cb(pj_ioqueue_key_t *key, 
                                    pj_status_t status)
    {
        Pj_Event_Handler *handler = 
	    (Pj_Event_Handler*) pj_ioqueue_get_user_data(key);

        handler->on_connect_complete(status);
    }

};

#endif	/* __PJPP_PROACTOR_HPP__ */

