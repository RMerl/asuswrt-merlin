/* $Id: sip_transport.c 4092 2012-04-26 09:24:50Z bennylp $ */
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
#include <pjsip/sip_transport.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_parser.h>
#include <pjsip/sip_msg.h>
#include <pjsip/sip_private.h>
#include <pjsip/sip_errno.h>
#include <pjsip/sip_module.h>
#include <pj/except.h>
#include <pj/os.h>
#include <pj/log.h>
#include <pj/ioqueue.h>
#include <pj/hash.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/assert.h>
#include <pj/lock.h>
#include <pj/list.h>


#define THIS_FILE    "sip_transport.c"

#if 1 //DEAN for debugging.
#   define TRACE_(x)	PJ_LOG(5,x)

static const char *addr_string(const pj_sockaddr_t *addr)
{
    static char str[PJ_INET6_ADDRSTRLEN];
    pj_inet_ntop(((const pj_sockaddr*)addr)->addr.sa_family, 
		 pj_sockaddr_get_addr(addr),
		 str, sizeof(str));
    return str;
}
#else
#   define TRACE_(x)
#endif

/* Prototype. */
static pj_status_t mod_on_tx_msg(pjsip_tx_data *tdata);

/* This module has sole purpose to print transmit data to contigous buffer
 * before actually transmitted to the wire. 
 */
static pjsip_module mod_msg_print_initializer = 
{
    NULL, NULL,				/* prev and next		    */
    { "mod-msg-print", 13},		/* Name.			    */
    -1,					/* Id				    */
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER,	/* Priority			    */
    NULL,				/* load()			    */
    NULL,				/* start()			    */
    NULL,				/* stop()			    */
    NULL,				/* unload()			    */
    NULL,				/* on_rx_request()		    */
    NULL,				/* on_rx_response()		    */
    &mod_on_tx_msg,			/* on_tx_request()		    */
    &mod_on_tx_msg,			/* on_tx_response()		    */
    NULL,				/* on_tsx_state()		    */
};

static pjsip_module mod_msg_print[PJSUA_MAX_INSTANCES];
static int is_initialized;

static void mod_msg_print_initialize()
{
	int i;
	if(is_initialized)
		return;

	for (i=0; i < PJ_ARRAY_SIZE(mod_msg_print); i++)
	{
		mod_msg_print[i] = mod_msg_print_initializer;
	}

	is_initialized = 1;
}

/*
 * Transport manager.
 */
struct pjsip_tpmgr 
{
    pj_hash_table_t *table;
    pj_lock_t	    *lock;
    pjsip_endpoint  *endpt;
    pjsip_tpfactory  factory_list;
#if defined(PJ_DEBUG) && PJ_DEBUG!=0
    pj_atomic_t	    *tdata_counter;
#endif
    void           (*on_rx_msg)(pjsip_endpoint*, pj_status_t, pjsip_rx_data*);
    pj_status_t	   (*on_tx_msg)(pjsip_endpoint*, pjsip_tx_data*);
    pjsip_tp_state_callback tp_state_cb;
};


/* Transport state listener list type */
typedef struct tp_state_listener
{
    PJ_DECL_LIST_MEMBER(struct tp_state_listener);

    pjsip_tp_state_callback  cb;
    void *user_data;
} tp_state_listener;


/*
 * Transport data.
 */
typedef struct transport_data
{
    /* Transport listeners */
    tp_state_listener	    st_listeners;
    tp_state_listener	    st_listeners_empty;
} transport_data;


/*****************************************************************************
 *
 * GENERAL TRANSPORT (NAMES, TYPES, ETC.)
 *
 *****************************************************************************/

/*
 * Transport names.
 */
struct transport_names_t
{
    pjsip_transport_type_e type;	    /* Transport type	    */
    pj_uint16_t		   port;	    /* Default port number  */
    pj_str_t		   name;	    /* Id tag		    */
    const char		  *description;	    /* Longer description   */
    unsigned		   flag;	    /* Flags		    */
    char		   name_buf[16];    /* For user's transport */
} transport_names[16] = 
{
    { 
	PJSIP_TRANSPORT_UNSPECIFIED, 
	0, 
	{"Unspecified", 11}, 
	"Unspecified", 
	0
    },
    { 
	PJSIP_TRANSPORT_UDP, 
	5060, 
	{"UDP", 3}, 
	"UDP transport", 
	PJSIP_TRANSPORT_DATAGRAM
    },
    { 
	PJSIP_TRANSPORT_TCP, 
	5060, 
	{"TCP", 3}, 
	"TCP transport", 
	PJSIP_TRANSPORT_RELIABLE
    },
    { 
	PJSIP_TRANSPORT_TLS, 
	5061, 
	{"TLS", 3}, 
	"TLS transport", 
	PJSIP_TRANSPORT_RELIABLE | PJSIP_TRANSPORT_SECURE
    },
    { 
	PJSIP_TRANSPORT_SCTP, 
	5060, 
	{"SCTP", 4}, 
	"SCTP transport", 
	PJSIP_TRANSPORT_RELIABLE
    },
    { 
	PJSIP_TRANSPORT_LOOP, 
	15060, 
	{"LOOP", 4}, 
	"Loopback transport", 
	PJSIP_TRANSPORT_RELIABLE
    }, 
    { 
	PJSIP_TRANSPORT_LOOP_DGRAM, 
	15060, 
	{"LOOP-DGRAM", 10}, 
	"Loopback datagram transport", 
	PJSIP_TRANSPORT_DATAGRAM
    },
    { 
	PJSIP_TRANSPORT_UDP6, 
	5060, 
	{"UDP", 3}, 
	"UDP IPv6 transport", 
	PJSIP_TRANSPORT_DATAGRAM
    },
    { 
	PJSIP_TRANSPORT_TCP6, 
	5060, 
	{"TCP", 3}, 
	"TCP IPv6 transport", 
	PJSIP_TRANSPORT_RELIABLE
    },
};

static void tp_state_callback(pjsip_transport *tp,
			      pjsip_transport_state state,
			      const pjsip_transport_state_info *info);


struct transport_names_t *get_tpname(pjsip_transport_type_e type)
{
    unsigned i;
    for (i=0; i<PJ_ARRAY_SIZE(transport_names); ++i) {
	if (transport_names[i].type == type)
	    return &transport_names[i];
    }
    pj_assert(!"Invalid transport type!");
    return NULL;
}



/*
 * Register new transport type to PJSIP.
 */
PJ_DEF(pj_status_t) pjsip_transport_register_type( unsigned tp_flag,
						   const char *tp_name,
						   int def_port,
						   int *p_tp_type)
{
    unsigned i;

    PJ_ASSERT_RETURN(tp_flag && tp_name && def_port, PJ_EINVAL);
    PJ_ASSERT_RETURN(pj_ansi_strlen(tp_name) < 
			PJ_ARRAY_SIZE(transport_names[0].name_buf), 
		     PJ_ENAMETOOLONG);

    for (i=1; i<PJ_ARRAY_SIZE(transport_names); ++i) {
	if (transport_names[i].type == 0)
	    break;
    }

    if (i == PJ_ARRAY_SIZE(transport_names))
	return PJ_ETOOMANY;

    transport_names[i].type = (pjsip_transport_type_e)i;
    transport_names[i].port = (pj_uint16_t)def_port;
    pj_ansi_strcpy(transport_names[i].name_buf, tp_name);
    transport_names[i].name = pj_str(transport_names[i].name_buf);
    transport_names[i].flag = tp_flag;

    if (p_tp_type)
	*p_tp_type = i;

    return PJ_SUCCESS;
}


/*
 * Get transport type from name.
 */
PJ_DEF(pjsip_transport_type_e) pjsip_transport_get_type_from_name(const pj_str_t *name)
{
    unsigned i;

    if (name->slen == 0)
	return PJSIP_TRANSPORT_UNSPECIFIED;

    /* Get transport type from name. */
    for (i=0; i<PJ_ARRAY_SIZE(transport_names); ++i) {
	if (pj_stricmp(name, &transport_names[i].name) == 0) {
	    return transport_names[i].type;
	}
    }

    pj_assert(!"Invalid transport name");
    return PJSIP_TRANSPORT_UNSPECIFIED;
}


/*
 * Get the transport type for the specified flags.
 */
PJ_DEF(pjsip_transport_type_e) pjsip_transport_get_type_from_flag(unsigned flag)
{
    unsigned i;

    /* Get the transport type for the specified flags. */
    for (i=0; i<PJ_ARRAY_SIZE(transport_names); ++i) {
	if (transport_names[i].flag == flag) {
	    return transport_names[i].type;
	}
    }

    pj_assert(!"Invalid transport type");
    return PJSIP_TRANSPORT_UNSPECIFIED;
}

/*
 * Get the socket address family of a given transport type.
 */
PJ_DEF(int) pjsip_transport_type_get_af(pjsip_transport_type_e type)
{
    if (type & PJSIP_TRANSPORT_IPV6)
	return pj_AF_INET6();
    else
	return pj_AF_INET();
}

PJ_DEF(unsigned) pjsip_transport_get_flag_from_type(pjsip_transport_type_e type)
{
    /* Return transport flag. */
    return get_tpname(type)->flag;
}

/*
 * Get the default SIP port number for the specified type.
 */
PJ_DEF(int) pjsip_transport_get_default_port_for_type(pjsip_transport_type_e type)
{
    /* Return the port. */
    return get_tpname(type)->port;
}

/*
 * Get transport name.
 */
PJ_DEF(const char*) pjsip_transport_get_type_name(pjsip_transport_type_e type)
{
    /* Return the name. */
    return get_tpname(type)->name.ptr;
}

/*
 * Get transport description.
 */
PJ_DEF(const char*) pjsip_transport_get_type_desc(pjsip_transport_type_e type)
{
    /* Return the description. */
    return get_tpname(type)->description;
}


/*****************************************************************************
 *
 * TRANSPORT SELECTOR
 *
 *****************************************************************************/

/*
 * Add transport/listener reference in the selector.
 */
PJ_DEF(void) pjsip_tpselector_add_ref(pjsip_tpselector *sel)
{
    if (sel->type == PJSIP_TPSELECTOR_TRANSPORT && sel->u.transport != NULL)
	pjsip_transport_add_ref(sel->u.transport);
    else if (sel->type == PJSIP_TPSELECTOR_LISTENER && sel->u.listener != NULL)
	; /* Hmm.. looks like we don't have reference counter for listener */
}


/*
 * Decrement transport/listener reference in the selector.
 */
PJ_DEF(void) pjsip_tpselector_dec_ref(pjsip_tpselector *sel)
{
    if (sel->type == PJSIP_TPSELECTOR_TRANSPORT && sel->u.transport != NULL)
	pjsip_transport_dec_ref(sel->u.transport);
    else if (sel->type == PJSIP_TPSELECTOR_LISTENER && sel->u.listener != NULL)
	; /* Hmm.. looks like we don't have reference counter for listener */
}


/*****************************************************************************
 *
 * TRANSMIT DATA BUFFER MANIPULATION.
 *
 *****************************************************************************/

/*
 * Create new transmit buffer.
 */
PJ_DEF(pj_status_t) pjsip_tx_data_create( pjsip_tpmgr *mgr,
					  pjsip_tx_data **p_tdata )
{
    pj_pool_t *pool;
    pjsip_tx_data *tdata;
    pj_status_t status;

    PJ_ASSERT_RETURN(mgr && p_tdata, PJ_EINVAL);

    pool = pjsip_endpt_create_pool( mgr->endpt, "tdta%p",
				    PJSIP_POOL_LEN_TDATA,
				    PJSIP_POOL_INC_TDATA );
    if (!pool)
	return PJ_ENOMEM;

    tdata = PJ_POOL_ZALLOC_T(pool, pjsip_tx_data);
    tdata->pool = pool;
    tdata->mgr = mgr;
	tdata->inst_id = pjsip_endpt_get_inst_id(mgr->endpt);
    pj_memcpy(tdata->obj_name, pool->obj_name, PJ_MAX_OBJ_NAME);

    status = pj_atomic_create(tdata->pool, 0, &tdata->ref_cnt);
    if (status != PJ_SUCCESS) {
	pjsip_endpt_release_pool( mgr->endpt, tdata->pool );
	return status;
    }
    
    //status = pj_lock_create_simple_mutex(pool, "tdta%p", &tdata->lock);
    status = pj_lock_create_null_mutex(pool, "tdta%p", &tdata->lock);
    if (status != PJ_SUCCESS) {
	pjsip_endpt_release_pool( mgr->endpt, tdata->pool );
	return status;
    }

    pj_ioqueue_op_key_init(&tdata->op_key.key, sizeof(tdata->op_key.key));

#if defined(PJ_DEBUG) && PJ_DEBUG!=0
    pj_atomic_inc( tdata->mgr->tdata_counter );
#endif

    *p_tdata = tdata;
    return PJ_SUCCESS;
}


/*
 * Add reference to tx buffer.
 */
PJ_DEF(void) pjsip_tx_data_add_ref( pjsip_tx_data *tdata )
{
    pj_atomic_inc(tdata->ref_cnt);
}

/*
 * Decrease transport data reference, destroy it when the reference count
 * reaches zero.
 */
PJ_DEF(pj_status_t) pjsip_tx_data_dec_ref( pjsip_tx_data *tdata )
{
    pj_assert( pj_atomic_get(tdata->ref_cnt) > 0);
    if (pj_atomic_dec_and_get(tdata->ref_cnt) <= 0) {
	PJ_LOG(5,(tdata->obj_name, "Destroying txdata %s",
		  pjsip_tx_data_get_info(tdata)));
	pjsip_tpselector_dec_ref(&tdata->tp_sel);
#if defined(PJ_DEBUG) && PJ_DEBUG!=0
	pj_atomic_dec( tdata->mgr->tdata_counter );
#endif
	pj_atomic_destroy( tdata->ref_cnt );
	pj_lock_destroy( tdata->lock );
	pjsip_endpt_release_pool( tdata->mgr->endpt, tdata->pool );
	return PJSIP_EBUFDESTROYED;
    } else {
	return PJ_SUCCESS;
    }
}

/*
 * Invalidate the content of the print buffer to force the message to be
 * re-printed when sent.
 */
PJ_DEF(void) pjsip_tx_data_invalidate_msg( pjsip_tx_data *tdata )
{
    tdata->buf.cur = tdata->buf.start;
    tdata->info = NULL;
}

/*
 * Print the SIP message to transmit data buffer's internal buffer.
 */
PJ_DEF(pj_status_t) pjsip_tx_data_encode(pjsip_tx_data *tdata)
{
    /* Allocate buffer if necessary. */
	if (tdata->buf.start == NULL) {
		PJ_USE_EXCEPTION;

		PJ_TRY(tdata->inst_id) {
			tdata->buf.start = (char*) 
					   pj_pool_alloc(tdata->pool, PJSIP_MAX_PKT_LEN);
		}
		PJ_CATCH_ANY {
			return PJ_ENOMEM;
		}
		PJ_END(tdata->inst_id)

		tdata->buf.cur = tdata->buf.start;
		tdata->buf.end = tdata->buf.start + PJSIP_MAX_PKT_LEN;
    }

    /* Do we need to reprint? */
    if (!pjsip_tx_data_is_valid(tdata)) {
	pj_ssize_t size;

	size = pjsip_msg_print( tdata->msg, tdata->buf.start, 
			        tdata->buf.end - tdata->buf.start);
	if (size < 0) {
	    return PJSIP_EMSGTOOLONG;
	}
	pj_assert(size != 0);
	tdata->buf.cur[size] = '\0';
	tdata->buf.cur += size;
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_bool_t) pjsip_tx_data_is_valid( pjsip_tx_data *tdata )
{
    return tdata->buf.cur != tdata->buf.start;
}

static char *get_msg_info(pj_pool_t *pool, const char *obj_name,
			  const pjsip_msg *msg)
{
    char info_buf[128], *info;
    const pjsip_cseq_hdr *cseq;
    int len;

    cseq = (const pjsip_cseq_hdr*) pjsip_msg_find_hdr(msg, PJSIP_H_CSEQ, NULL);
    PJ_ASSERT_RETURN(cseq != NULL, "INVALID MSG");

    if (msg->type == PJSIP_REQUEST_MSG) {
	len = pj_ansi_snprintf(info_buf, sizeof(info_buf), 
			       "Request msg %.*s/cseq=%d (%s)",
			       (int)msg->line.req.method.name.slen,
			       msg->line.req.method.name.ptr,
			       cseq->cseq, obj_name);
    } else {
	len = pj_ansi_snprintf(info_buf, sizeof(info_buf),
			       "Response msg %d/%.*s/cseq=%d (%s)",
			       msg->line.status.code,
			       (int)cseq->method.name.slen,
			       cseq->method.name.ptr,
			       cseq->cseq, obj_name);
    }

    if (len < 1 || len >= (int)sizeof(info_buf)) {
	return (char*)obj_name;
    }

    info = (char*) pj_pool_alloc(pool, len+1);
    pj_memcpy(info, info_buf, len+1);

    return info;
}

PJ_DEF(char*) pjsip_tx_data_get_info( pjsip_tx_data *tdata )
{
    /* tdata->info may be assigned by application so if it exists
     * just return it.
     */
    if (tdata->info)
	return tdata->info;

    if (tdata==NULL || tdata->msg==NULL)
	return "NULL";

    pj_lock_acquire(tdata->lock);
    tdata->info = get_msg_info(tdata->pool, tdata->obj_name, tdata->msg);
    pj_lock_release(tdata->lock);

    return tdata->info;
}

PJ_DEF(pj_status_t) pjsip_tx_data_set_transport(pjsip_tx_data *tdata,
						const pjsip_tpselector *sel)
{
    PJ_ASSERT_RETURN(tdata && sel, PJ_EINVAL);

    pj_lock_acquire(tdata->lock);

    pjsip_tpselector_dec_ref(&tdata->tp_sel);

    pj_memcpy(&tdata->tp_sel, sel, sizeof(*sel));
    pjsip_tpselector_add_ref(&tdata->tp_sel);

    pj_lock_release(tdata->lock);

    return PJ_SUCCESS;
}


PJ_DEF(char*) pjsip_rx_data_get_info(pjsip_rx_data *rdata)
{
    char obj_name[PJ_MAX_OBJ_NAME];

    PJ_ASSERT_RETURN(rdata->msg_info.msg, "INVALID MSG");

    if (rdata->msg_info.info)
	return rdata->msg_info.info;

    pj_ansi_strcpy(obj_name, "rdata");
    pj_ansi_snprintf(obj_name+5, sizeof(obj_name)-5, "%p", rdata);

    rdata->msg_info.info = get_msg_info(rdata->tp_info.pool, obj_name,
					rdata->msg_info.msg);
    return rdata->msg_info.info;
}

/*****************************************************************************
 *
 * TRANSPORT KEY
 *
 *****************************************************************************/


/*****************************************************************************
 *
 * TRANSPORT
 *
 *****************************************************************************/

static void transport_send_callback(pjsip_transport *transport,
				    void *token,
				    pj_ssize_t size)
{
    pjsip_tx_data *tdata = (pjsip_tx_data*) token;

    PJ_UNUSED_ARG(transport);

    /* Mark pending off so that app can resend/reuse txdata from inside
     * the callback.
     */
    tdata->is_pending = 0;

    /* Call callback, if any. */
    if (tdata->cb) {
	(*tdata->cb)(tdata->token, tdata, size);
    }

    /* Decrement reference count. */
    pjsip_tx_data_dec_ref(tdata);
}

/* This function is called by endpoint for on_tx_request() and on_tx_response()
 * notification.
 */
static pj_status_t mod_on_tx_msg(pjsip_tx_data *tdata)
{
    return pjsip_tx_data_encode(tdata);
}

/*
 * Send a SIP message using the specified transport.
 */
PJ_DEF(pj_status_t) pjsip_transport_send(  pjsip_transport *tr, 
					   pjsip_tx_data *tdata,
					   const pj_sockaddr_t *addr,
					   int addr_len,
					   void *token,
					   pjsip_tp_send_callback cb)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(tr && tdata && addr, PJ_EINVAL);

    /* Is it currently being sent? */
    if (tdata->is_pending) {
	pj_assert(!"Invalid operation step!");
	PJ_LOG(2,(THIS_FILE, "Unable to send %s: message is pending", 
			     pjsip_tx_data_get_info(tdata)));
	return PJSIP_EPENDINGTX;
    }

    /* Add reference to prevent deletion, and to cancel idle timer if
     * it's running.
     */
    pjsip_transport_add_ref(tr);

    /* Fill in tp_info. */
    tdata->tp_info.transport = tr;
    pj_memcpy(&tdata->tp_info.dst_addr, addr, addr_len);
    tdata->tp_info.dst_addr_len = addr_len;

    pj_inet_ntop(((pj_sockaddr*)addr)->addr.sa_family,
		 pj_sockaddr_get_addr(addr),
		 tdata->tp_info.dst_name,
		 sizeof(tdata->tp_info.dst_name));
    tdata->tp_info.dst_port = pj_sockaddr_get_port(addr);

    /* Distribute to modules. 
     * When the message reach mod_msg_print, the contents of the message will
     * be "printed" to contiguous buffer.
     */
    if (tr->tpmgr->on_tx_msg) {
	status = (*tr->tpmgr->on_tx_msg)(tr->endpt, tdata);
	if (status != PJ_SUCCESS) {
	    pjsip_transport_dec_ref(tr);
	    return status;
    }
    }

    /* Save callback data. */
    tdata->token = token;
    tdata->cb = cb;

    /* Add reference counter. */
    pjsip_tx_data_add_ref(tdata);

    /* Mark as pending. */
    tdata->is_pending = 1;

    /* Send to transport. */
    status = (*tr->send_msg)(tr, tdata,  addr, addr_len, (void*)tdata, 
			     &transport_send_callback);

    if (status != PJ_EPENDING) {
	tdata->is_pending = 0;
	pjsip_tx_data_dec_ref(tdata);
    }

    pjsip_transport_dec_ref(tr);
    return status;
}


/* send_raw() callback */
static void send_raw_callback(pjsip_transport *transport,
			      void *token,
			      pj_ssize_t size)
{
    pjsip_tx_data *tdata = (pjsip_tx_data*) token;

    /* Mark pending off so that app can resend/reuse txdata from inside
     * the callback.
     */
    tdata->is_pending = 0;

    /* Call callback, if any. */
    if (tdata->cb) {
	(*tdata->cb)(tdata->token, tdata, size);
    }

    /* Decrement tdata reference count. */
    pjsip_tx_data_dec_ref(tdata);

    /* Decrement transport reference count */
    pjsip_transport_dec_ref(transport);
}


/* Send raw data */
PJ_DEF(pj_status_t) pjsip_tpmgr_send_raw(pjsip_tpmgr *mgr,
					 pjsip_transport_type_e tp_type,
					 const pjsip_tpselector *sel,
					 pjsip_tx_data *tdata,
					 const void *raw_data,
					 pj_size_t data_len,
					 const pj_sockaddr_t *addr,
					 int addr_len,
					 void *token,
					 pjsip_tp_send_callback cb)
{
    pjsip_transport *tr;
    pj_status_t status;
 
    /* Acquire the transport */
    status = pjsip_tpmgr_acquire_transport(mgr, tp_type, addr, addr_len,
					   sel, &tr);
    if (status != PJ_SUCCESS)
	return status;

    /* Create transmit data buffer if one is not specified */
    if (tdata == NULL) {
	status = pjsip_endpt_create_tdata(tr->endpt, &tdata);
	if (status != PJ_SUCCESS) {
	    pjsip_transport_dec_ref(tr);
	    return status;
	}

	tdata->info = "raw";

	/* Add reference counter. */
	pjsip_tx_data_add_ref(tdata);
    }

    /* Allocate buffer */
    if (tdata->buf.start == NULL ||
	(tdata->buf.end - tdata->buf.start) < (int)data_len)
    {
	/* Note: data_len may be zero, so allocate +1 */
	tdata->buf.start = (char*) pj_pool_alloc(tdata->pool, data_len+1);
	tdata->buf.end = tdata->buf.start + data_len + 1;
    }
 
    /* Copy data, if any! (application may send zero len packet) */
    if (data_len) {
	pj_memcpy(tdata->buf.start, raw_data, data_len);
    }
    tdata->buf.cur = tdata->buf.start + data_len;
 
    /* Save callback data. */
    tdata->token = token;
    tdata->cb = cb;

    /* Mark as pending. */
    tdata->is_pending = 1;

    /* Send to transport */
    status = tr->send_msg(tr, tdata, addr, addr_len,
			  tdata, &send_raw_callback);
 
    if (status != PJ_EPENDING) {
	/* callback will not be called, so destroy tdata now. */
	pjsip_tx_data_dec_ref(tdata);
	pjsip_transport_dec_ref(tr);
    }

    return status;
}


static void transport_idle_callback(pj_timer_heap_t *timer_heap,
				    struct pj_timer_entry *entry)
{
    pjsip_transport *tp = (pjsip_transport*) entry->user_data;
    pj_assert(tp != NULL);

    PJ_UNUSED_ARG(timer_heap);

    entry->id = PJ_FALSE;
    pjsip_transport_destroy(tp);
}

/*
 * Add ref.
 */
PJ_DEF(pj_status_t) pjsip_transport_add_ref( pjsip_transport *tp )
{
    PJ_ASSERT_RETURN(tp != NULL, PJ_EINVAL);

    if (pj_atomic_inc_and_get(tp->ref_cnt) == 1) {
	pj_lock_acquire(tp->tpmgr->lock);
	/* Verify again. */
	if (pj_atomic_get(tp->ref_cnt) == 1) {
	    if (tp->idle_timer.id != PJ_FALSE) {
		pjsip_endpt_cancel_timer(tp->tpmgr->endpt, &tp->idle_timer);
		tp->idle_timer.id = PJ_FALSE;
	    }
	}
	pj_lock_release(tp->tpmgr->lock);
    }

    return PJ_SUCCESS;
}

/*
 * Dec ref.
 */
PJ_DEF(pj_status_t) pjsip_transport_dec_ref( pjsip_transport *tp )
{
    PJ_ASSERT_RETURN(tp != NULL, PJ_EINVAL);

    pj_assert(pj_atomic_get(tp->ref_cnt) > 0);

    if (pj_atomic_dec_and_get(tp->ref_cnt) == 0) {
	pj_lock_acquire(tp->tpmgr->lock);
	/* Verify again. Do not register timer if the transport is
	 * being destroyed.
	 */
	if (pj_atomic_get(tp->ref_cnt) == 0 && !tp->is_destroying) {
	    pj_time_val delay;
	    
	    /* If transport is in graceful shutdown, then this is the
	     * last user who uses the transport. Schedule to destroy the
	     * transport immediately. Otherwise schedule idle timer.
	     */
	    if (tp->is_shutdown) {
		delay.sec = delay.msec = 0;
	    } else {
		delay.sec = (tp->dir==PJSIP_TP_DIR_OUTGOING) ?
				PJSIP_TRANSPORT_IDLE_TIME :
				PJSIP_TRANSPORT_SERVER_IDLE_TIME;
		delay.msec = 0;
	    }

	    pj_assert(tp->idle_timer.id == 0);
	    tp->idle_timer.id = PJ_TRUE;
	    pjsip_endpt_schedule_timer(tp->tpmgr->endpt, &tp->idle_timer, 
				       &delay);
	}
	pj_lock_release(tp->tpmgr->lock);
    }

    return PJ_SUCCESS;
}


/**
 * Register a transport.
 */
PJ_DEF(pj_status_t) pjsip_transport_register( pjsip_tpmgr *mgr,
					      pjsip_transport *tp )
{
    int key_len;
    pj_uint32_t hval;
    void *entry;
	pjsip_transport_key2 key;

    /* Init. */
    tp->tpmgr = mgr;
    pj_bzero(&tp->idle_timer, sizeof(tp->idle_timer));
    tp->idle_timer.user_data = tp;
    tp->idle_timer.cb = &transport_idle_callback;

    /* 
     * Register to hash table (see Trac ticket #42).
     */
	// DEAN 2013-05-07 prepare remote ip:port for hash-table key calculating.
	pj_bzero(&key, sizeof(pjsip_transport_key2));
	key.type = tp->key.type;
	pj_sockaddr_print(&tp->key.rem_addr, key.rem_addr_str, sizeof(key.rem_addr_str), 3);
	key_len = sizeof(key.type) + strlen(key.rem_addr_str);
    pj_lock_acquire(mgr->lock);

    /* If entry already occupied, unregister previous entry */
    hval = 0;
    entry = pj_hash_get(mgr->table, &key, key_len, &hval);
    if (entry != NULL)
	pj_hash_set(NULL, mgr->table, &key, key_len, hval, NULL);

    /* Register new entry */
	pj_hash_set(tp->pool, mgr->table, &key, key_len, hval, tp);
	pjsip_tpmgr_dump_transports(mgr);

    pj_lock_release(mgr->lock);

    TRACE_((THIS_FILE,"Transport %s registered: type=%s, remote=%s, hval=%d",
		       tp->obj_name,
		       pjsip_transport_get_type_name(key.type),
		       //addr_string(&tp->key.rem_addr),
		       //pj_sockaddr_get_port(&tp->key.rem_addr),
			   key.rem_addr_str,
			   hval));

    return PJ_SUCCESS;
}

/* Force destroy transport (e.g. during transport manager shutdown. */
static pj_status_t destroy_transport( pjsip_tpmgr *mgr,
				      pjsip_transport *tp )
{
	pjsip_transport_key2 key2;
    int key_len;
    pj_uint32_t hval;
    void *entry;

    TRACE_((THIS_FILE, "Transport %s is being destroyed", tp->obj_name));

    pj_lock_acquire(tp->lock);
    pj_lock_acquire(mgr->lock);

    tp->is_destroying = PJ_TRUE;

    /*
     * Unregister timer, if any.
     */
    //pj_assert(tp->idle_timer.id == PJ_FALSE);
    if (tp->idle_timer.id != PJ_FALSE) {
	pjsip_endpt_cancel_timer(mgr->endpt, &tp->idle_timer);
	tp->idle_timer.id = PJ_FALSE;
    }

    /*
     * Unregister from hash table (see Trac ticket #42).
     */
	pj_bzero(&key2, sizeof(pjsip_transport_key2));
	key2.type = tp->key.type;
	pj_sockaddr_print(&tp->key.rem_addr, key2.rem_addr_str, sizeof(key2.rem_addr_str), 3);
	key_len = sizeof(key2.type) + strlen(key2.rem_addr_str);	

    //key_len = sizeof(tp->key.type) + tp->addr_len;
    hval = 0;
    entry = pj_hash_get(mgr->table, &key2, key_len, &hval);
    if (entry == (void*)tp)
	pj_hash_set(NULL, mgr->table, &key2, key_len, hval, NULL);

    pj_lock_release(mgr->lock);

    /* Destroy. */
    return tp->destroy(tp);
}


/*
 * Start graceful shutdown procedure for this transport. 
 */
PJ_DEF(pj_status_t) pjsip_transport_shutdown(pjsip_transport *tp)
{
    pjsip_tpmgr *mgr;
    pj_status_t status;

    TRACE_((THIS_FILE, "Transport %s shutting down", tp->obj_name));

    pj_lock_acquire(tp->lock);

    mgr = tp->tpmgr;
    pj_lock_acquire(mgr->lock);

    /* Do nothing if transport is being shutdown already */
    if (tp->is_shutdown) {
	pj_lock_release(tp->lock);
	pj_lock_release(mgr->lock);
	return PJ_SUCCESS;
    }

    status = PJ_SUCCESS;

    /* Instruct transport to shutdown itself */
    if (tp->do_shutdown)
	status = tp->do_shutdown(tp);
    
    if (status == PJ_SUCCESS)
	tp->is_shutdown = PJ_TRUE;

    /* If transport reference count is zero, start timer count-down */
    if (pj_atomic_get(tp->ref_cnt) == 0) {
	pjsip_transport_add_ref(tp);
	pjsip_transport_dec_ref(tp);
    }

    pj_lock_release(tp->lock);
    pj_lock_release(mgr->lock);

    return status;
}


/**
 * Unregister transport.
 */
PJ_DEF(pj_status_t) pjsip_transport_destroy( pjsip_transport *tp)
{
    /* Must have no user. */
    PJ_ASSERT_RETURN(pj_atomic_get(tp->ref_cnt) == 0, PJSIP_EBUSY);

    /* Destroy. */
    return destroy_transport(tp->tpmgr, tp);
}



/*****************************************************************************
 *
 * TRANSPORT FACTORY
 *
 *****************************************************************************/


PJ_DEF(pj_status_t) pjsip_tpmgr_register_tpfactory( pjsip_tpmgr *mgr,
						    pjsip_tpfactory *tpf)
{
    pjsip_tpfactory *p;
    pj_status_t status;

    pj_lock_acquire(mgr->lock);

    /* Check that no factory with the same type has been registered. */
    status = PJ_SUCCESS;
    for (p=mgr->factory_list.next; p!=&mgr->factory_list; p=p->next) {
	if (p->type == tpf->type) {
	    status = PJSIP_ETYPEEXISTS;
	    break;
	}
	if (p == tpf) {
	    status = PJ_EEXISTS;
	    break;
	}
    }

    if (status != PJ_SUCCESS) {
	pj_lock_release(mgr->lock);
	return status;
    }

    pj_list_insert_before(&mgr->factory_list, tpf);

    pj_lock_release(mgr->lock);

    return PJ_SUCCESS;
}


/**
 * Unregister factory.
 */
PJ_DEF(pj_status_t) pjsip_tpmgr_unregister_tpfactory( pjsip_tpmgr *mgr,
						      pjsip_tpfactory *tpf)
{
    pj_lock_acquire(mgr->lock);

    pj_assert(pj_list_find_node(&mgr->factory_list, tpf) == tpf);
    pj_list_erase(tpf);

    pj_lock_release(mgr->lock);

    return PJ_SUCCESS;
}


/*****************************************************************************
 *
 * TRANSPORT MANAGER
 *
 *****************************************************************************/

/*
 * Create a new transport manager.
 */
PJ_DEF(pj_status_t) pjsip_tpmgr_create( pj_pool_t *pool,
					pjsip_endpoint *endpt,
					pjsip_rx_callback rx_cb,
					pjsip_tx_callback tx_cb,
					pjsip_tpmgr **p_mgr)
{
    pjsip_tpmgr *mgr;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && endpt && rx_cb && p_mgr, PJ_EINVAL);
	
	mod_msg_print_initialize(); // DEAN added

    /* Register mod_msg_print module. */
    status = pjsip_endpt_register_module(endpt, &mod_msg_print[pool->factory->inst_id]);
    if (status != PJ_SUCCESS)
	return status;

    /* Create and initialize transport manager. */
    mgr = PJ_POOL_ZALLOC_T(pool, pjsip_tpmgr);
    mgr->endpt = endpt;
    mgr->on_rx_msg = rx_cb;
    mgr->on_tx_msg = tx_cb;
    pj_list_init(&mgr->factory_list);

    mgr->table = pj_hash_create(pool, PJSIP_TPMGR_HTABLE_SIZE);
    if (!mgr->table)
	return PJ_ENOMEM;

    status = pj_lock_create_recursive_mutex(pool, "tmgr%p", &mgr->lock);
    if (status != PJ_SUCCESS)
	return status;

#if defined(PJ_DEBUG) && PJ_DEBUG!=0
    status = pj_atomic_create(pool, 0, &mgr->tdata_counter);
    if (status != PJ_SUCCESS)
	return status;
#endif

    /* Set transport state callback */
    status = pjsip_tpmgr_set_state_cb(mgr, &tp_state_callback);
    if (status != PJ_SUCCESS)
	return status;

    PJ_LOG(5, (THIS_FILE, "Transport manager created."));

    *p_mgr = mgr;
    return PJ_SUCCESS;
}



/*
 * Find out the appropriate local address info (IP address and port) to
 * advertise in Contact header based on the remote address to be 
 * contacted. The local address info would be the address name of the
 * transport or listener which will be used to send the request.
 *
 * In this implementation, it will only select the transport based on
 * the transport type in the request.
 */
PJ_DEF(pj_status_t) pjsip_tpmgr_find_local_addr( pjsip_tpmgr *tpmgr,
						 pj_pool_t *pool,
						 pjsip_transport_type_e type,
						 const pjsip_tpselector *sel,
						 pj_str_t *ip_addr,
						 int *port)
{
    pj_status_t status = PJSIP_EUNSUPTRANSPORT;
    unsigned flag;

    /* Sanity checks */
    PJ_ASSERT_RETURN(tpmgr && pool && ip_addr && port, PJ_EINVAL);

    ip_addr->slen = 0;
    *port = 0;

    flag = pjsip_transport_get_flag_from_type(type);

    if (sel && sel->type == PJSIP_TPSELECTOR_TRANSPORT &&
	sel->u.transport)
    {
	pj_strdup(pool, ip_addr, &sel->u.transport->local_name.host);
	*port = sel->u.transport->local_name.port;
	status = PJ_SUCCESS;

    } else if (sel && sel->type == PJSIP_TPSELECTOR_LISTENER &&
	       sel->u.listener)
    {
	pj_strdup(pool, ip_addr, &sel->u.listener->addr_name.host);
	*port = sel->u.listener->addr_name.port;
	status = PJ_SUCCESS;

    } else if ((flag & PJSIP_TRANSPORT_DATAGRAM) != 0) {
	
	pj_sockaddr remote;
	int addr_len;
	pjsip_transport *tp;

	pj_bzero(&remote, sizeof(remote));
	if (type & PJSIP_TRANSPORT_IPV6) {
	    addr_len = sizeof(pj_sockaddr_in6);
	    remote.addr.sa_family = pj_AF_INET6();
	} else {
	    addr_len = sizeof(pj_sockaddr_in);
	    remote.addr.sa_family = pj_AF_INET();
	}

	status = pjsip_tpmgr_acquire_transport(tpmgr, type, &remote,
					       addr_len, NULL, &tp);

	if (status == PJ_SUCCESS) {
	    pj_strdup(pool, ip_addr, &tp->local_name.host);
	    *port = tp->local_name.port;
	    status = PJ_SUCCESS;

	    pjsip_transport_dec_ref(tp);
	}

    } else {
	/* For connection oriented transport, enum the factories */
	pjsip_tpfactory *f;

	pj_lock_acquire(tpmgr->lock);

	f = tpmgr->factory_list.next;
	while (f != &tpmgr->factory_list) {
	    if (f->type == type)
		break;
	    f = f->next;
	}

	if (f != &tpmgr->factory_list) {
	    pj_strdup(pool, ip_addr, &f->addr_name.host);
	    *port = f->addr_name.port;
	    status = PJ_SUCCESS;
	}
	pj_lock_release(tpmgr->lock);
    }

    return status;
}

/*
 * Return number of transports currently registered to the transport
 * manager.
 */
PJ_DEF(unsigned) pjsip_tpmgr_get_transport_count(pjsip_tpmgr *mgr)
{
    pj_hash_iterator_t itr_val;
    pj_hash_iterator_t *itr;
    int nr_of_transports = 0;
    
    pj_lock_acquire(mgr->lock);
    
    itr = pj_hash_first(mgr->table, &itr_val);
    while (itr) {
	nr_of_transports++;
	itr = pj_hash_next(mgr->table, itr);
    }
    
    pj_lock_release(mgr->lock);

    return nr_of_transports;
}

/*
 * pjsip_tpmgr_destroy()
 *
 * Destroy transport manager.
 */
PJ_DEF(pj_status_t) pjsip_tpmgr_destroy( pjsip_tpmgr *mgr )
{
    pj_hash_iterator_t itr_val;
    pj_hash_iterator_t *itr;
    pjsip_tpfactory *factory;
    pjsip_endpoint *endpt = mgr->endpt;

	int inst_id = pjsip_endpt_get_inst_id(endpt);
    
    PJ_LOG(5, (THIS_FILE, "Destroying transport manager"));

    pj_lock_acquire(mgr->lock);

    /*
     * Destroy all transports.
     */
    itr = pj_hash_first(mgr->table, &itr_val);
    while (itr != NULL) {
	pj_hash_iterator_t *next;
	pjsip_transport *transport;
	
	transport = (pjsip_transport*) pj_hash_this(mgr->table, itr);

	next = pj_hash_next(mgr->table, itr);

	destroy_transport(mgr, transport);

	itr = next;
    }

    /*
     * Destroy all factories/listeners.
     */
    factory = mgr->factory_list.next;
    while (factory != &mgr->factory_list) {
	pjsip_tpfactory *next = factory->next;
	
	factory->destroy(factory);

	factory = next;
    }

    pj_lock_release(mgr->lock);
    pj_lock_destroy(mgr->lock);

    /* Unregister mod_msg_print. */
    if (mod_msg_print[inst_id].id != -1) {
	pjsip_endpt_unregister_module(endpt, &mod_msg_print[inst_id]);
    }

#if defined(PJ_DEBUG) && PJ_DEBUG!=0
    /* If you encounter assert error on this line, it means there are
     * leakings in transmit data (i.e. some transmit data have not been
     * destroyed).
     */
    //pj_assert(pj_atomic_get(mgr->tdata_counter) == 0);
    if (pj_atomic_get(mgr->tdata_counter) != 0) {
	PJ_LOG(3,(THIS_FILE, "Warning: %d transmit buffer(s) not freed!",
		  pj_atomic_get(mgr->tdata_counter)));
    }
#endif

    return PJ_SUCCESS;
}


/*
 * pjsip_tpmgr_receive_packet()
 *
 * Called by tranports when they receive a new packet.
 */
PJ_DEF(pj_ssize_t) pjsip_tpmgr_receive_packet( pjsip_tpmgr *mgr,
					       pjsip_rx_data *rdata)
{
    pjsip_transport *tr = rdata->tp_info.transport;

    char *current_pkt;
    pj_size_t remaining_len;
    pj_size_t total_processed = 0;

	int inst_id = rdata->tp_info.pool->factory->inst_id;

    /* Check size. */
    pj_assert(rdata->pkt_info.len > 0);
    if (rdata->pkt_info.len <= 0)
	return -1;

    current_pkt = rdata->pkt_info.packet;
    remaining_len = rdata->pkt_info.len;
    
    /* Must NULL terminate buffer. This is the requirement of the 
     * parser etc. 
     */
    current_pkt[remaining_len] = '\0';

    /* Process all message fragments. */
    while (remaining_len > 0) {

	pjsip_msg *msg;
	char *p, *end;
	char saved;
	pj_size_t msg_fragment_size;

	/* Skip leading newlines as pjsip_find_msg() currently can't
	 * handle leading newlines.
	 */
	for (p=current_pkt, end=p+remaining_len; p!=end; ++p) {
	    if (*p != '\r' && *p != '\n')
		break;
	}
	if (p!=current_pkt) {
	    remaining_len -= (p - current_pkt);
	    total_processed += (p - current_pkt);
	    current_pkt = p;
	    if (remaining_len == 0) {
		return total_processed;
	    }
	}

	/* Initialize default fragment size. */
	msg_fragment_size = remaining_len;

	/* Clear and init msg_info in rdata. 
	 * Endpoint might inspect the values there when we call the callback
	 * to report some errors.
	 */
	pj_bzero(&rdata->msg_info, sizeof(rdata->msg_info));
	pj_list_init(&rdata->msg_info.parse_err);
	rdata->msg_info.msg_buf = current_pkt;
	rdata->msg_info.len = remaining_len;

	/* For TCP transport, check if the whole message has been received. */
	if ((tr->flag & PJSIP_TRANSPORT_DATAGRAM) == 0) {
	    pj_status_t msg_status;
	    msg_status = pjsip_find_msg(inst_id, current_pkt, remaining_len, PJ_FALSE, 
                                        &msg_fragment_size);
	    if (msg_status != PJ_SUCCESS) {
		if (remaining_len == PJSIP_MAX_PKT_LEN) {
		    mgr->on_rx_msg(mgr->endpt, PJSIP_ERXOVERFLOW, rdata);
		    /* Exhaust all data. */
		    return rdata->pkt_info.len;
		} else {
		    /* Not enough data in packet. */
		    return total_processed;
		}
	    }
	}

	/* Update msg_info. */
	rdata->msg_info.len = msg_fragment_size;

	/* Null terminate packet */
	saved = current_pkt[msg_fragment_size];
	current_pkt[msg_fragment_size] = '\0';

	/* Parse the message. */
	rdata->msg_info.msg = msg = 
	    pjsip_parse_rdata( inst_id, current_pkt, msg_fragment_size, rdata);

	/* Restore null termination */
	current_pkt[msg_fragment_size] = saved;

	/* Check for parsing syntax error */
	if (msg==NULL || !pj_list_empty(&rdata->msg_info.parse_err)) {
	    pjsip_parser_err_report *err;
	    char buf[128];
	    pj_str_t tmp;

	    /* Gather syntax error information */
	    tmp.ptr = buf; tmp.slen = 0;
	    err = rdata->msg_info.parse_err.next;
	    while (err != &rdata->msg_info.parse_err) {
		int len;
		len = pj_ansi_snprintf(tmp.ptr+tmp.slen, sizeof(buf)-tmp.slen,
				       ": %s exception when parsing '%.*s' "
				       "header on line %d col %d",
				       pj_exception_id_name(inst_id, err->except_code),
				       (int)err->hname.slen, err->hname.ptr,
				       err->line, err->col);
		if (len > 0 && len < (int) (sizeof(buf)-tmp.slen)) {
		    tmp.slen += len;
		}
		err = err->next;
	    }

	    /* Only print error message if there's error.
	     * Sometimes we receive blank packets (packets with only CRLF)
	     * which were sent to keep NAT bindings.
	     */
	    if (tmp.slen) {
		PJ_LOG(1, (THIS_FILE, 
		      "Error processing %d bytes packet from %s %s:%d %.*s:\n"
		      "%.*s\n"
		      "-- end of packet.",
		      msg_fragment_size,
		      rdata->tp_info.transport->type_name,
		      rdata->pkt_info.src_name, 
		      rdata->pkt_info.src_port,
		      (int)tmp.slen, tmp.ptr,
		      (int)msg_fragment_size,
		      rdata->msg_info.msg_buf));
	    }

	    goto finish_process_fragment;
	}

	/* Perform basic header checking. */
	if (rdata->msg_info.cid == NULL ||
	    rdata->msg_info.cid->id.slen == 0 || 
	    rdata->msg_info.from == NULL || 
	    rdata->msg_info.to == NULL || 
	    rdata->msg_info.via == NULL || 
	    rdata->msg_info.cseq == NULL) 
	{
	    mgr->on_rx_msg(mgr->endpt, PJSIP_EMISSINGHDR, rdata);
	    goto finish_process_fragment;
	}

	/* For request: */
	if (rdata->msg_info.msg->type == PJSIP_REQUEST_MSG) {
	    /* always add received parameter to the via. */
	    pj_strdup2(rdata->tp_info.pool, 
		       &rdata->msg_info.via->recvd_param, 
		       rdata->pkt_info.src_name);

	    /* RFC 3581:
	     * If message contains "rport" param, put the received port there.
	     */
	    if (rdata->msg_info.via->rport_param == 0) {
		rdata->msg_info.via->rport_param = rdata->pkt_info.src_port;
	    }
	} else {
	    /* Drop malformed responses */
	    if (rdata->msg_info.msg->line.status.code < 100 ||
		rdata->msg_info.msg->line.status.code >= 700)
	    {
		mgr->on_rx_msg(mgr->endpt, PJSIP_EINVALIDSTATUS, rdata);
		goto finish_process_fragment;
	    }
	}

	/* Drop response message if it has more than one Via.
	*/
	/* This is wrong. Proxy DOES receive responses with multiple
	 * Via headers! Thanks Aldo <acampi at deis.unibo.it> for pointing
	 * this out.

	if (msg->type == PJSIP_RESPONSE_MSG) {
	    pjsip_hdr *hdr;
	    hdr = (pjsip_hdr*)rdata->msg_info.via->next;
	    if (hdr != &msg->hdr) {
		hdr = pjsip_msg_find_hdr(msg, PJSIP_H_VIA, hdr);
		if (hdr) {
		    mgr->on_rx_msg(mgr->endpt, PJSIP_EMULTIPLEVIA, rdata);
		    goto finish_process_fragment;
		}
	    }
	}
	*/

	/* Call the transport manager's upstream message callback.
	 */
	mgr->on_rx_msg(mgr->endpt, PJ_SUCCESS, rdata);


finish_process_fragment:
	total_processed += msg_fragment_size;
	current_pkt += msg_fragment_size;
	remaining_len -= msg_fragment_size;

    }	/* while (rdata->pkt_info.len > 0) */


    return total_processed;
}


/*
 * pjsip_tpmgr_acquire_transport()
 *
 * Get transport suitable to communicate to remote. Create a new one
 * if necessary.
 */
PJ_DEF(pj_status_t) pjsip_tpmgr_acquire_transport(pjsip_tpmgr *mgr,
						  pjsip_transport_type_e type,
						  const pj_sockaddr_t *remote,
						  int addr_len,
						  const pjsip_tpselector *sel,
						  pjsip_transport **tp)
{
    return pjsip_tpmgr_acquire_transport2(mgr, type, remote, addr_len, sel,
					  NULL, tp);
}

static void dump_bin(const char *buf, unsigned len)
{
	unsigned i;

	PJ_LOG(3,(THIS_FILE, "begin dump"));
	for (i=0; i<len; ++i) {
		int j;
		char bits[9];
		unsigned val = buf[i] & 0xFF;

		bits[8] = '\0';
		for (j=0; j<8; ++j) {
			if (val & (1 << (7-j)))
				bits[j] = '1';
			else
				bits[j] = '0';
		}

		PJ_LOG(3,(THIS_FILE, "%2d %s [%d]", i, bits, val));
	}
	PJ_LOG(3,(THIS_FILE, "end dump"));
}

/*
 * pjsip_tpmgr_acquire_transport2()
 *
 * Get transport suitable to communicate to remote. Create a new one
 * if necessary.
 */
PJ_DEF(pj_status_t) pjsip_tpmgr_acquire_transport2(pjsip_tpmgr *mgr,
						   pjsip_transport_type_e type,
						   const pj_sockaddr_t *remote,
						   int addr_len,
						   const pjsip_tpselector *sel,
						   pjsip_tx_data *tdata,
						   pjsip_transport **tp)
{
    pjsip_tpfactory *factory;
    pj_status_t status;

    TRACE_((THIS_FILE,"Acquiring transport type=%s, remote=%s:%d, sel=%d",
		       pjsip_transport_get_type_name(type),
		       addr_string(remote),
		       pj_sockaddr_get_port(remote),
			   (sel == NULL ? -1 : sel->type)));

    pj_lock_acquire(mgr->lock);

    /* If transport is specified, then just use it if it is suitable
     * for the destination.
     */
    if (sel && sel->type == PJSIP_TPSELECTOR_TRANSPORT &&
	sel->u.transport) 
    {
	pjsip_transport *seltp = sel->u.transport;

	/* See if the transport is (not) suitable */
	if (seltp->key.type != type) {
	    pj_lock_release(mgr->lock);
	    return PJSIP_ETPNOTSUITABLE;
	}

	/* We could also verify that the destination address is reachable
	 * from this transport (i.e. both are equal), but if application
	 * has requested a specific transport to be used, assume that
	 * it knows what to do.
	 *
	 * In other words, I don't think destination verification is a good
	 * idea for now.
	 */

	/* Transport looks to be suitable to use, so just use it. */
	pjsip_transport_add_ref(seltp);
	pj_lock_release(mgr->lock);
	*tp = seltp;

	TRACE_((THIS_FILE, "Transport %s acquired", seltp->obj_name));
	return PJ_SUCCESS;


    } else if (sel && sel->type == PJSIP_TPSELECTOR_LISTENER &&
	       sel->u.listener)
    {
	/* Application has requested that a specific listener is to
	 * be used. In this case, skip transport hash table lookup.
	 */

	/* Verify that the listener type matches the destination type */
	if (sel->u.listener->type != type) {
	    pj_lock_release(mgr->lock);
	    return PJSIP_ETPNOTSUITABLE;
	}

	/* We'll use this listener to create transport */
	factory = sel->u.listener;

    } else {

	/*
	 * This is the "normal" flow, where application doesn't specify
	 * specific transport/listener to be used to send message to.
	 * In this case, lookup the transport from the hash table.
	 */
	pjsip_transport_key key;
	pjsip_transport_key2 key2;
	int key_len;
	pjsip_transport *transport;
	pj_uint32_t hval = 0;

	pj_bzero(&key, sizeof(key));
	//key_len = sizeof(key.type) + addr_len;
	/* First try to get exact destination. */
	key.type = type;
	pj_memcpy(&key.rem_addr, remote, addr_len);

	// DEAN 2013-05-07 prepare remote ip:port for hash-table key calculating.
	pj_bzero(&key2, sizeof(key2));
	/* First try to get exact destination. */
	key2.type = type;
	pj_bzero(key2.rem_addr_str, sizeof(key2.rem_addr_str));
	pj_sockaddr_print(remote, key2.rem_addr_str, sizeof(key2.rem_addr_str), 3);
	key_len = sizeof(key2.type) + strlen(key2.rem_addr_str);
	pjsip_tpmgr_dump_transports(mgr);
	
	//dump_bin((char *)&key2, key_len);
	transport = (pjsip_transport*)
		    pj_hash_get(mgr->table, &key2, key_len, &hval);

	if (transport == NULL) {
	    unsigned flag = pjsip_transport_get_flag_from_type(type);
	    const pj_sockaddr *remote_addr = (const pj_sockaddr*)remote;


	    /* Ignore address for loop transports. */
	    if (type == PJSIP_TRANSPORT_LOOP ||
		     type == PJSIP_TRANSPORT_LOOP_DGRAM)
	    {
		pj_sockaddr *addr = &key.rem_addr;

		pj_bzero(addr, addr_len);
		key_len = sizeof(key2.type) + addr_len;
		transport = (pjsip_transport*) 
			    pj_hash_get(mgr->table, &key2, key_len, NULL);
	    }
	    /* For datagram transports, try lookup with zero address.
	     */
	    else if (flag & PJSIP_TRANSPORT_DATAGRAM)
	    {
		pj_sockaddr *addr = &key.rem_addr;

		pj_bzero(addr, addr_len);
		addr->addr.sa_family = remote_addr->addr.sa_family;

		key_len = sizeof(key2.type) + addr_len;
		transport = (pjsip_transport*)
			    pj_hash_get(mgr->table, &key2, key_len, NULL);
	    }
	}

	if (transport!=NULL)
		TRACE_((THIS_FILE, "Transport %s got with hval=%d", transport->obj_name, hval));

	if (transport!=NULL && !transport->is_shutdown) {
	    /*
	     * Transport found!
	     */
	    pjsip_transport_add_ref(transport);
	    pj_lock_release(mgr->lock);
	    *tp = transport;

	    TRACE_((THIS_FILE, "Transport %s acquired", transport->obj_name));
	    return PJ_SUCCESS;
	}

	/*
	 * Transport not found!
	 * Find factory that can create such transport.
	 */
	factory = mgr->factory_list.next;
	while (factory != &mgr->factory_list) {
	    if (factory->type == type)
		break;
	    factory = factory->next;
	}

	if (factory == &mgr->factory_list) {
	    /* No factory can create the transport! */
	    pj_lock_release(mgr->lock);
	    TRACE_((THIS_FILE, "No suitable factory was found either"));
	    return PJSIP_EUNSUPTRANSPORT;
	}
    }

    TRACE_((THIS_FILE, "Creating new transport from factory"));

    /* Request factory to create transport. */
    if (factory->create_transport2) {
	status = factory->create_transport2(factory, mgr, mgr->endpt,
					    (const pj_sockaddr*) remote, 
					    addr_len, tdata, tp);
    } else {
	status = factory->create_transport(factory, mgr, mgr->endpt,
					   (const pj_sockaddr*) remote, 
					   addr_len, tp);
    }
    if (status == PJ_SUCCESS) {
	PJ_ASSERT_ON_FAIL(tp!=NULL, 
	    {pj_lock_release(mgr->lock); return PJ_EBUG;});
	pjsip_transport_add_ref(*tp);
    }
    pj_lock_release(mgr->lock);
    return status;
}

/**
 * Dump transport info.
 */
PJ_DEF(void) pjsip_tpmgr_dump_transports(pjsip_tpmgr *mgr)
{
#if PJ_LOG_MAX_LEVEL >= 3
    pj_hash_iterator_t itr_val;
    pj_hash_iterator_t *itr;
    pjsip_tpfactory *factory;

    pj_lock_acquire(mgr->lock);

#if defined(PJ_DEBUG) && PJ_DEBUG!=0
    PJ_LOG(3,(THIS_FILE, " Outstanding transmit buffers: %d",
	      pj_atomic_get(mgr->tdata_counter)));
#endif

    PJ_LOG(3, (THIS_FILE, " Dumping listeners:"));
    factory = mgr->factory_list.next;
    while (factory != &mgr->factory_list) {
	PJ_LOG(3, (THIS_FILE, "  %s %s:%.*s:%d", 
		   factory->obj_name,
		   factory->type_name,
		   (int)factory->addr_name.host.slen,
		   factory->addr_name.host.ptr,
		   (int)factory->addr_name.port));
	factory = factory->next;
    }

    itr = pj_hash_first(mgr->table, &itr_val);
    if (itr) {
	PJ_LOG(3, (THIS_FILE, " Dumping transports:"));

	do {
	    pjsip_transport *t = (pjsip_transport*) 
	    			 pj_hash_this(mgr->table, itr);

	    PJ_LOG(3, (THIS_FILE, "  %s %s (refcnt=%d%s %s %s)", 
		       t->obj_name,
		       t->info,
		       pj_atomic_get(t->ref_cnt),
			   (t->idle_timer.id ? " [idle]" : ""),
			   (t->is_shutdown ? " [shutdown]" : ""),
			   (t->is_destroying ? " [destroying]" : "")));

	    itr = pj_hash_next(mgr->table, itr);
	} while (itr);
    }

    pj_lock_release(mgr->lock);
#else
    PJ_UNUSED_ARG(mgr);
#endif
}

/**
 * Set callback of global transport state notification.
 */
PJ_DEF(pj_status_t) pjsip_tpmgr_set_state_cb(pjsip_tpmgr *mgr,
					     pjsip_tp_state_callback cb)
{
    PJ_ASSERT_RETURN(mgr, PJ_EINVAL);

    mgr->tp_state_cb = cb;

    return PJ_SUCCESS;
}

/**
 * Get callback of global transport state notification.
 */
PJ_DEF(pjsip_tp_state_callback) pjsip_tpmgr_get_state_cb(
					     const pjsip_tpmgr *mgr)
{
    PJ_ASSERT_RETURN(mgr, NULL);

    return mgr->tp_state_cb;
}


/**
 * Allocate and init transport data.
 */
static void init_tp_data(pjsip_transport *tp)
{
    transport_data *tp_data;

    pj_assert(tp && !tp->data);

    tp_data = PJ_POOL_ZALLOC_T(tp->pool, transport_data);
    pj_list_init(&tp_data->st_listeners);
    pj_list_init(&tp_data->st_listeners_empty);
    tp->data = tp_data;
}


static void tp_state_callback(pjsip_transport *tp,
			      pjsip_transport_state state,
			      const pjsip_transport_state_info *info)
{
    transport_data *tp_data;

    pj_lock_acquire(tp->lock);

    tp_data = (transport_data*)tp->data;

    /* Notify the transport state listeners, if any. */
    if (!tp_data || pj_list_empty(&tp_data->st_listeners)) {
	goto on_return;
    } else {
	pjsip_transport_state_info st_info;
	tp_state_listener *st_listener = tp_data->st_listeners.next;

	/* As we need to put the user data into the transport state info,
	 * let's use a copy of transport state info.
	 */
	pj_memcpy(&st_info, info, sizeof(st_info));
	while (st_listener != &tp_data->st_listeners) {
	    st_info.user_data = st_listener->user_data;
	    (*st_listener->cb)(tp, state, &st_info);

	    st_listener = st_listener->next;
	}
    }

on_return:
    pj_lock_release(tp->lock);
}


/**
 * Add a listener to the specified transport for transport state notification.
 */
PJ_DEF(pj_status_t) pjsip_transport_add_state_listener (
					    pjsip_transport *tp,
					    pjsip_tp_state_callback cb,
					    void *user_data,
					    pjsip_tp_state_listener_key **key)
{
    transport_data *tp_data;
    tp_state_listener *entry;

    PJ_ASSERT_RETURN(tp && cb && key, PJ_EINVAL);

    pj_lock_acquire(tp->lock);

    /* Init transport data, if it hasn't */
    if (!tp->data)
	init_tp_data(tp);

    tp_data = (transport_data*)tp->data;

    /* Init the new listener entry. Use available empty slot, if any,
     * otherwise allocate it using the transport pool.
     */
    if (!pj_list_empty(&tp_data->st_listeners_empty)) {
	entry = tp_data->st_listeners_empty.next;
	pj_list_erase(entry);
    } else {
	entry = PJ_POOL_ZALLOC_T(tp->pool, tp_state_listener);
    }
    entry->cb = cb;
    entry->user_data = user_data;

    /* Add the new listener entry to the listeners list */
    pj_list_push_back(&tp_data->st_listeners, entry);

    *key = entry;

    pj_lock_release(tp->lock);

    return PJ_SUCCESS;
}

/**
 * Remove a listener from the specified transport for transport state 
 * notification.
 */
PJ_DEF(pj_status_t) pjsip_transport_remove_state_listener (
				    pjsip_transport *tp,
				    pjsip_tp_state_listener_key *key,
				    const void *user_data)
{
    transport_data *tp_data;
    tp_state_listener *entry;

    PJ_ASSERT_RETURN(tp && key, PJ_EINVAL);

    pj_lock_acquire(tp->lock);

    tp_data = (transport_data*)tp->data;

    /* Transport data is NULL or no registered listener? */
    if (!tp_data || pj_list_empty(&tp_data->st_listeners)) {
		pj_lock_release(tp->lock);
		PJ_LOG(4, ("sip_transport.c", "pjsip_transport_remove_state_listener() tp_data not found."));
		return PJ_ENOTFOUND;
    }

    entry = (tp_state_listener*)key;

    /* Validate the user data */
    if (entry->user_data != user_data) {
	pj_assert(!"Invalid transport state listener key");
	pj_lock_release(tp->lock);
	return PJ_EBUG;
    }

    /* Reset the entry and move it to the empty list */
    entry->cb = NULL;
    entry->user_data = NULL;
    pj_list_erase(entry);
    pj_list_push_back(&tp_data->st_listeners_empty, entry);

    pj_lock_release(tp->lock);

    return PJ_SUCCESS;
}
