/* $Id: stun_transaction.c 4407 2013-02-27 15:02:03Z riza $ */
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
#include <pjnath/stun_transaction.h>
#include <pjnath/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/timer.h>


#define THIS_FILE		"stun_transaction.c"
#define TIMER_INACTIVE		0
#define TIMER_ACTIVE		1


struct pj_stun_client_tsx
{
    char		 obj_name[PJ_MAX_OBJ_NAME];
    pj_stun_tsx_cb	 cb;
    void		*user_data;
    pj_grp_lock_t	*grp_lock;

    pj_bool_t		 complete;

    pj_bool_t		 require_retransmit;
    unsigned		 rto_msec;
    pj_timer_entry	 retransmit_timer;
    unsigned		 transmit_count;
    pj_time_val		 retransmit_time;
    pj_timer_heap_t	*timer_heap;

    pj_timer_entry	 destroy_timer;

    void		*last_pkt;
    unsigned		 last_pkt_size;
};


#if 1
#   define TRACE_(expr)		    PJ_LOG(5,expr)
#else
#   define TRACE_(expr)
#endif


static void retransmit_timer_callback(pj_timer_heap_t *timer_heap, 
				      pj_timer_entry *timer);
static void destroy_timer_callback(pj_timer_heap_t *timer_heap, 
				   pj_timer_entry *timer);

#define stun_perror(tsx,msg,rc) pjnath_perror(tsx->obj_name, msg, rc)

/*
 * Create a STUN client transaction.
 */
PJ_DEF(pj_status_t) pj_stun_client_tsx_create(pj_stun_config *cfg,
					      pj_pool_t *pool,
					      pj_grp_lock_t *grp_lock,
					      const pj_stun_tsx_cb *cb,
					      pj_stun_client_tsx **p_tsx)
{
    pj_stun_client_tsx *tsx;

    PJ_ASSERT_RETURN(cfg && cb && p_tsx, PJ_EINVAL);
    PJ_ASSERT_RETURN(cb->on_send_msg, PJ_EINVAL);

    tsx = PJ_POOL_ZALLOC_T(pool, pj_stun_client_tsx);
    tsx->rto_msec = cfg->rto_msec;
    tsx->timer_heap = cfg->timer_heap;
    tsx->grp_lock = grp_lock;
    pj_memcpy(&tsx->cb, cb, sizeof(*cb));

    tsx->retransmit_timer.cb = &retransmit_timer_callback;
    tsx->retransmit_timer.user_data = tsx;

    tsx->destroy_timer.cb = &destroy_timer_callback;
    tsx->destroy_timer.user_data = tsx;

    pj_ansi_snprintf(tsx->obj_name, sizeof(tsx->obj_name), "stuntsx%p", tsx);

    *p_tsx = tsx;

    PJ_LOG(5,(tsx->obj_name, "STUN client transaction created"));
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_stun_client_tsx_schedule_destroy(
				    pj_stun_client_tsx *tsx,
				    const pj_time_val *delay)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(tsx && delay, PJ_EINVAL);
    PJ_ASSERT_RETURN(tsx->cb.on_destroy, PJ_EINVAL);

    pj_grp_lock_acquire(tsx->grp_lock);

    /* Cancel previously registered timer */
    pj_timer_heap_cancel_if_active(tsx->timer_heap, &tsx->destroy_timer,
                                   TIMER_INACTIVE);

    /* Stop retransmission, just in case */
    pj_timer_heap_cancel_if_active(tsx->timer_heap, &tsx->retransmit_timer,
                                   TIMER_INACTIVE);

    status = pj_timer_heap_schedule_w_grp_lock(tsx->timer_heap,
                                               &tsx->destroy_timer, delay,
                                               TIMER_ACTIVE, tsx->grp_lock);
    if (status != PJ_SUCCESS) {
	pj_grp_lock_release(tsx->grp_lock);
	return status;
    }

    tsx->cb.on_complete = NULL;

    pj_grp_lock_release(tsx->grp_lock);

    TRACE_((tsx->obj_name, "STUN transaction %p schedule destroy", tsx));

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_stun_client_tsx_destroy(pj_stun_client_tsx *tsx)
{
    /*
     * Currently tsx has no objects to destroy so we don't need to do anything
     * here.
     */
    /* pj_stun_client_tsx_stop(tsx); */
    PJ_UNUSED_ARG(tsx);
    return PJ_SUCCESS;
}

/*
 * Destroy transaction immediately.
 */
PJ_DEF(pj_status_t) pj_stun_client_tsx_stop(pj_stun_client_tsx *tsx)
{
    PJ_ASSERT_RETURN(tsx, PJ_EINVAL);

    /* Don't call grp_lock_acquire() because we might be called on
     * group lock's destructor.
     */
    pj_timer_heap_cancel_if_active(tsx->timer_heap, &tsx->retransmit_timer,
                                   TIMER_INACTIVE);
    pj_timer_heap_cancel_if_active(tsx->timer_heap, &tsx->destroy_timer,
                                   TIMER_INACTIVE);

    PJ_LOG(5,(tsx->obj_name, "STUN client transaction %p stopped, ref_cnt=%d",
	      tsx, pj_grp_lock_get_ref(tsx->grp_lock)));

    return PJ_SUCCESS;
}


/*
 * Check if transaction has completed.
 */
PJ_DEF(pj_bool_t) pj_stun_client_tsx_is_complete(pj_stun_client_tsx *tsx)
{
    PJ_ASSERT_RETURN(tsx, PJ_FALSE);
    return tsx->complete;
}


/*
 * Set user data.
 */
PJ_DEF(pj_status_t) pj_stun_client_tsx_set_data(pj_stun_client_tsx *tsx,
						void *data)
{
    PJ_ASSERT_RETURN(tsx, PJ_EINVAL);
    tsx->user_data = data;
    return PJ_SUCCESS;
}


/*
 * Get the user data
 */
PJ_DEF(void*) pj_stun_client_tsx_get_data(pj_stun_client_tsx *tsx)
{
    PJ_ASSERT_RETURN(tsx, NULL);
    return tsx->user_data;
}


/*
 * Transmit message.
 */
static pj_status_t tsx_transmit_msg(pj_stun_client_tsx *tsx,
                                    pj_bool_t mod_count)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(tsx->retransmit_timer.id == TIMER_INACTIVE ||
		     !tsx->require_retransmit || !mod_count, PJ_EBUSY);

    if (tsx->require_retransmit && mod_count) {
	/* Calculate retransmit/timeout delay */
	if (tsx->transmit_count == 0) {
	    tsx->retransmit_time.sec = 0;
	    tsx->retransmit_time.msec = tsx->rto_msec;

	} else if (tsx->transmit_count < PJ_STUN_MAX_TRANSMIT_COUNT-1) {
	    unsigned msec;

	    msec = PJ_TIME_VAL_MSEC(tsx->retransmit_time);
	    msec <<= 1;
	    tsx->retransmit_time.sec = msec / 1000;
	    tsx->retransmit_time.msec = msec % 1000;

	} else {
	    tsx->retransmit_time.sec = PJ_STUN_TIMEOUT_VALUE / 1000;
	    tsx->retransmit_time.msec = PJ_STUN_TIMEOUT_VALUE % 1000;
	}

	/* Schedule timer first because when send_msg() failed we can
	 * cancel it (as opposed to when schedule_timer() failed we cannot
	 * cancel transmission).
	 */;
	status = pj_timer_heap_schedule_w_grp_lock(tsx->timer_heap,
						   &tsx->retransmit_timer,
						   &tsx->retransmit_time,
						   TIMER_ACTIVE,
						   tsx->grp_lock);
	if (status != PJ_SUCCESS) {
	    tsx->retransmit_timer.id = TIMER_INACTIVE;
	    return status;
	}
    }


    if (mod_count)
        tsx->transmit_count++;

    PJ_LOG(4,(tsx->obj_name, "[%s] STUN sending message (transmit count=%d)",
	      tsx->obj_name, tsx->transmit_count));

    /* Send message */
    status = tsx->cb.on_send_msg(tsx, tsx->last_pkt, tsx->last_pkt_size);

    if (status == PJNATH_ESTUNDESTROYED) {
	/* We've been destroyed, don't access the object. */
    } else if (status != PJ_SUCCESS) {
	if (mod_count || status == PJ_EINVALIDOP) {
		pj_timer_heap_cancel_if_active( tsx->timer_heap,
	                               		&tsx->retransmit_timer,
	                               		TIMER_INACTIVE);
	}
	PJ_PERROR(4, (tsx->obj_name, status, "STUN error sending message"));
    }

    return status;
}


/*
 * Send outgoing message and start STUN transaction.
 */
PJ_DEF(pj_status_t) pj_stun_client_tsx_send_msg(pj_stun_client_tsx *tsx,
						pj_bool_t retransmit,
						void *pkt,
						unsigned pkt_len)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(tsx && pkt && pkt_len, PJ_EINVAL);
    PJ_ASSERT_RETURN(tsx->retransmit_timer.id == 0, PJ_EBUSY);

    pj_grp_lock_acquire(tsx->grp_lock);

    /* Encode message */
    tsx->last_pkt = pkt;
    tsx->last_pkt_size = pkt_len;

    /* Update STUN retransmit flag */
    tsx->require_retransmit = retransmit;

    /* For TCP, schedule timeout timer after PJ_STUN_TIMEOUT_VALUE.
     * Since we don't have timeout timer, simulate this by using
     * retransmit timer.
     */
    if (!retransmit) {
	unsigned timeout;

	pj_assert(tsx->retransmit_timer.id == 0);
	tsx->transmit_count = PJ_STUN_MAX_TRANSMIT_COUNT;

	timeout = tsx->rto_msec * 16;
	tsx->retransmit_time.sec = timeout / 1000;
	tsx->retransmit_time.msec = timeout % 1000;

	/* Schedule timer first because when send_msg() failed we can
	 * cancel it (as opposed to when schedule_timer() failed we cannot
	 * cancel transmission).
	 */;
	status = pj_timer_heap_schedule_w_grp_lock(tsx->timer_heap,
	                                           &tsx->retransmit_timer,
	                                           &tsx->retransmit_time,
	                                           TIMER_ACTIVE,
	                                           tsx->grp_lock);
	if (status != PJ_SUCCESS) {
	    tsx->retransmit_timer.id = TIMER_INACTIVE;
	    pj_grp_lock_release(tsx->grp_lock);
	    return status;
	}
    }

    /* Send the message */
    status = tsx_transmit_msg(tsx, PJ_TRUE);
    if (status != PJ_SUCCESS) {
	pj_timer_heap_cancel_if_active(tsx->timer_heap,
	                               &tsx->retransmit_timer,
	                               TIMER_INACTIVE);
	pj_grp_lock_release(tsx->grp_lock);
	return status;
    }

    pj_grp_lock_release(tsx->grp_lock);
    return PJ_SUCCESS;
}


/* Retransmit timer callback */
static void retransmit_timer_callback(pj_timer_heap_t *timer_heap, 
				      pj_timer_entry *timer)
{
    pj_stun_client_tsx *tsx = (pj_stun_client_tsx *) timer->user_data;
    pj_status_t status;

    PJ_UNUSED_ARG(timer_heap);
    pj_grp_lock_acquire(tsx->grp_lock);

    if (tsx->transmit_count >= PJ_STUN_MAX_TRANSMIT_COUNT) {
        /* tsx may be destroyed when calling the callback below */
        pj_grp_lock_t *grp_lock = tsx->grp_lock;

	/* Retransmission count exceeded. Transaction has failed */
	tsx->retransmit_timer.id = 0;
	PJ_LOG(4,(tsx->obj_name, "STUN timeout waiting for response"));
	if (!tsx->complete) {
	    tsx->complete = PJ_TRUE;
	    if (tsx->cb.on_complete) {
		tsx->cb.on_complete(tsx, PJNATH_ESTUNTIMEDOUT, NULL, NULL, 0);
	    }
	}
	pj_grp_lock_release(grp_lock);
	/* We might have been destroyed, don't try to access the object */
	return;
    }

    tsx->retransmit_timer.id = 0;
    status = tsx_transmit_msg(tsx, PJ_TRUE);
    if (status != PJ_SUCCESS) {
	tsx->retransmit_timer.id = 0;
	if (!tsx->complete) {
	    tsx->complete = PJ_TRUE;
	    if (tsx->cb.on_complete) {
		tsx->cb.on_complete(tsx, status, NULL, NULL, 0);
	    }
	}
    }

    pj_grp_lock_release(tsx->grp_lock);
    /* We might have been destroyed, don't try to access the object */
}

/*
 * Request to retransmit the request.
 */
PJ_DEF(pj_status_t) pj_stun_client_tsx_retransmit(pj_stun_client_tsx *tsx,
                                                  pj_bool_t mod_count)
{
    if (tsx->destroy_timer.id != 0) {
	return PJ_SUCCESS;
    }

    if (mod_count) {
        pj_timer_heap_cancel_if_active(tsx->timer_heap, &tsx->retransmit_timer,
                                       TIMER_INACTIVE);
    }

    return tsx_transmit_msg(tsx, mod_count);
}

/* Timer callback to destroy transaction */
static void destroy_timer_callback(pj_timer_heap_t *timer_heap, 
				   pj_timer_entry *timer)
{
    pj_stun_client_tsx *tsx = (pj_stun_client_tsx *) timer->user_data;

    PJ_UNUSED_ARG(timer_heap);

    tsx->destroy_timer.id = PJ_FALSE;

    tsx->cb.on_destroy(tsx);
    /* Don't access transaction after this */
}


/*
 * Notify the STUN transaction about the arrival of STUN response.
 */
PJ_DEF(pj_status_t) pj_stun_client_tsx_on_rx_msg(pj_stun_client_tsx *tsx,
						 const pj_stun_msg *msg,
						 const pj_sockaddr_t *src_addr,
						 unsigned src_addr_len)
{
    pj_stun_errcode_attr *err_attr;
    pj_status_t status;

    /* Must be STUN response message */
    if (!PJ_STUN_IS_SUCCESS_RESPONSE(msg->hdr.type) && 
	!PJ_STUN_IS_ERROR_RESPONSE(msg->hdr.type))
    {
	PJ_LOG(4,(tsx->obj_name, 
		  "STUN rx_msg() error: not response message"));
	return PJNATH_EINSTUNMSGTYPE;
    }


    /* We have a response with matching transaction ID. 
     * We can cancel retransmit timer now.
     */
    pj_timer_heap_cancel_if_active(tsx->timer_heap, &tsx->retransmit_timer,
                                   TIMER_INACTIVE);

    /* Find STUN error code attribute */
    err_attr = (pj_stun_errcode_attr*) 
		pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_ERROR_CODE, 0);

    if (err_attr && err_attr->err_code <= 200) {
	/* draft-ietf-behave-rfc3489bis-05.txt Section 8.3.2:
	 * Any response between 100 and 299 MUST result in the cessation
	 * of request retransmissions, but otherwise is discarded.
	 */
	PJ_LOG(4,(tsx->obj_name, 
		  "STUN rx_msg() error: received provisional %d code (%.*s)",
		  err_attr->err_code,
		  (int)err_attr->reason.slen,
		  err_attr->reason.ptr));
	return PJ_SUCCESS;
    }

    if (err_attr == NULL) {
	status = PJ_SUCCESS;
    } else {
	status = PJ_STATUS_FROM_STUN_CODE(err_attr->err_code);
    }

    /* Call callback */
    if (!tsx->complete) {
	tsx->complete = PJ_TRUE;
	if (tsx->cb.on_complete) {
	    tsx->cb.on_complete(tsx, status, msg, src_addr, src_addr_len);
	}
	/* We might have been destroyed, don't try to access the object */
    }

    return PJ_SUCCESS;

}

PJ_DEF(void)pj_stun_tsx_cancel_timer(pj_stun_client_tsx *tsx)
{
    pj_timer_heap_cancel_if_active(tsx->timer_heap, &tsx->retransmit_timer,
                                   TIMER_INACTIVE);
}

PJ_DEF(char *)pj_stun_tsx_get_obj_name(pj_stun_client_tsx *tsx)
{
	if (tsx) {
		return tsx->obj_name;
	}
	return "";
}

PJ_DEF(int)pj_stun_tsx_tansmit_count(pj_stun_client_tsx *tsx)
{
	if (tsx) {
		return tsx->transmit_count;
	}
	return 0;
}