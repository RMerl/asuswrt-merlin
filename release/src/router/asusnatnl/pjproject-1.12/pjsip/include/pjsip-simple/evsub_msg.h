/* $Id: evsub_msg.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_SIMPLE_EVENT_NOTIFY_MSG_H__
#define __PJSIP_SIMPLE_EVENT_NOTIFY_MSG_H__

/**
 * @file evsub_msg.h
 * @brief SIP Event Notification Headers (RFC 3265)
 */
#include <pjsip/sip_msg.h>

/**
 * @defgroup PJSIP_EVENT_HDRS Additional Header Fields
 * @ingroup PJSIP_EVENT_NOT
 * @{
 */

PJ_BEGIN_DECL


/** Max events in Allow-Events header. */
#define PJSIP_MAX_ALLOW_EVENTS	16

/**
 * This structure describes Event header.
 */
typedef struct pjsip_event_hdr
{
    /** Standard header fields. */
    PJSIP_DECL_HDR_MEMBER(struct pjsip_event_hdr);

    pj_str_t	    event_type;	    /**< Event name. */
    pj_str_t	    id_param;	    /**< Optional event ID parameter. */
    pjsip_param	    other_param;    /**< Other parameter. */
} pjsip_event_hdr;

/**
 * Create an Event header.
 *
 * @param pool	    The pool.
 *
 * @return	    New Event header instance.
 */
PJ_DECL(pjsip_event_hdr*) pjsip_event_hdr_create(pj_pool_t *pool);


/**
 * This structure describes Allow-Events header.
 */
typedef pjsip_generic_array_hdr pjsip_allow_events_hdr;


/**
 * Create a new Allow-Events header.
 *
 * @param pool	    The pool.
 *
 * @return	    Allow-Events header.
 */
PJ_DECL(pjsip_allow_events_hdr*) 
pjsip_allow_events_hdr_create(pj_pool_t *pool);


/**
 * This structure describes Subscription-State header.
 */
typedef struct pjsip_sub_state_hdr
{
    /** Standard header fields. */
    PJSIP_DECL_HDR_MEMBER(struct pjsip_sub_state_hdr);

    pj_str_t	    sub_state;		/**< Subscription state. */
    pj_str_t	    reason_param;	/**< Optional termination reason. */
    int		    expires_param;	/**< Expires param, or -1. */
    int		    retry_after;	/**< Retry after param, or -1. */
    pjsip_param	    other_param;	/**< Other parameters. */
} pjsip_sub_state_hdr;

/**
 * Create new Subscription-State header.
 *
 * @param pool	    The pool.
 *
 * @return	    Subscription-State header.
 */
PJ_DECL(pjsip_sub_state_hdr*) pjsip_sub_state_hdr_create(pj_pool_t *pool);

/**
 * Initialize parser for event notify module.
 */
PJ_DECL(void) pjsip_evsub_init_parser(int inst_id);


PJ_END_DECL


/**
 * @}
 */

#endif	/* __PJSIP_SIMPLE_EVENT_NOTIFY_MSG_H__ */

