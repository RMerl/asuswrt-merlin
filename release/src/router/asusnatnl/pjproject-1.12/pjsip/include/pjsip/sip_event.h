/* $Id: sip_event.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_SIP_EVENT_H__
#define __PJSIP_SIP_EVENT_H__

/**
 * @file sip_event.h
 * @brief SIP Event
 */

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_EVENT Event
 * @ingroup PJSIP_CORE_CORE
 * @brief Representation of events as they are distributed among modules.
 * @{
 */
#include <pj/types.h>


/** 
 * Event IDs.
 */
typedef enum pjsip_event_id_e
{
    /** Unidentified event. */
    PJSIP_EVENT_UNKNOWN,

    /** Timer event, normally only used internally in transaction. */
    PJSIP_EVENT_TIMER,

    /** Message transmission event. */
    PJSIP_EVENT_TX_MSG,

    /** Message received event. */
    PJSIP_EVENT_RX_MSG,

    /** Transport error event. */
    PJSIP_EVENT_TRANSPORT_ERROR,

    /** Transaction state changed event. */
    PJSIP_EVENT_TSX_STATE,

    /** Indicates that the event was triggered by user action. */
    PJSIP_EVENT_USER

} pjsip_event_id_e;


/**
 * This structure describe event descriptor to fully identify a SIP event.
 *
 * Events are the only way for a lower layer object to inform something
 * to higher layer objects. Normally this is achieved by means of callback,
 * i.e. the higher layer objects register a callback to handle the event on
 * the lower layer objects.
 *
 * This event descriptor is used for example by transactions, to inform
 * endpoint about events, and by transports, to inform endpoint about
 * unexpected transport error.
 */
struct pjsip_event
{
    /** This is necessary so that we can put events as a list. */
    PJ_DECL_LIST_MEMBER(struct pjsip_event);

    /** The event type, can be any value of \b pjsip_event_id_e.
     */
    pjsip_event_id_e type;

    /**
     * The event body as union, which fields depends on the event type.
     * By convention, the first member of each struct in the union must be
     * the pointer which is relevant to the event.
     */
    union
    {
        /** Timer event. */
        struct
        {
            struct pj_timer_entry *entry;      /**< The timer entry.           */
        } timer;

        /** Transaction state has changed event. */
        struct
        {
            union
            {
                pjsip_rx_data   *rdata; /**< The incoming message.      */
                pjsip_tx_data   *tdata; /**< The outgoing message.      */
                struct pj_timer_entry  *timer; /**< The timer.                 */
                pj_status_t      status;/**< Transport error status.    */
                void            *data;  /**< Generic data.              */
            } src;
            pjsip_transaction   *tsx;   /**< The transaction.           */
	    int			 prev_state; /**< Previous state.	*/
            pjsip_event_id_e     type;  /**< Type of event source:      
                                         *      - PJSIP_EVENT_TX_MSG
                                         *      - PJSIP_EVENT_RX_MSG,
                                         *      - PJSIP_EVENT_TRANSPORT_ERROR
                                         *      - PJSIP_EVENT_TIMER
                                         *      - PJSIP_EVENT_USER
                                         */
        } tsx_state;

        /** Message transmission event. */
        struct
        {
            pjsip_tx_data       *tdata; /**< The transmit data buffer.  */

        } tx_msg;

        /** Transmission error event. */
        struct
        {
            pjsip_tx_data       *tdata; /**< The transmit data.         */
            pjsip_transaction   *tsx;   /**< The transaction.           */
        } tx_error;

        /** Message arrival event. */
        struct
        {
            pjsip_rx_data       *rdata; /**< The receive data buffer.   */
        } rx_msg;

        /** User event. */
        struct
        {
            void                *user1; /**< User data 1.               */
            void                *user2; /**< User data 2.               */
            void                *user3; /**< User data 3.               */
            void                *user4; /**< User data 4.               */
        } user;

	} body;

	int natnl_flag; // 1: the flag for uas ip changed
};

/**
 * Init timer event.
 */
#define PJSIP_EVENT_INIT_TIMER(event,pentry)            \
        do { \
            (event).type = PJSIP_EVENT_TIMER;           \
            (event).body.timer.entry = pentry;          \
        } while (0)

/**
 * Init tsx state event.
 */
#define PJSIP_EVENT_INIT_TSX_STATE(event,ptsx,ptype,pdata,prev)   \
        do { \
            (event).type = PJSIP_EVENT_TSX_STATE;           \
            (event).body.tsx_state.tsx = ptsx;				\
            (event).body.tsx_state.type = ptype;            \
            (event).body.tsx_state.src.data = pdata;        \
			(event).body.tsx_state.prev_state = prev;	    \
			(event).natnl_flag = 0;							\
        } while (0)

/**
 * Init tx msg event.
 */
#define PJSIP_EVENT_INIT_TX_MSG(event,ptdata)	\
        do { \
            (event).type = PJSIP_EVENT_TX_MSG;          \
            (event).body.tx_msg.tdata = ptdata;		\
        } while (0)

/**
 * Init rx msg event.
 */
#define PJSIP_EVENT_INIT_RX_MSG(event,prdata)	\
        do { \
            (event).type = PJSIP_EVENT_RX_MSG;		\
            (event).body.rx_msg.rdata = prdata;		\
        } while (0)

/**
 * Init transport error event.
 */
#define PJSIP_EVENT_INIT_TRANSPORT_ERROR(event,ptsx,ptdata)   \
        do { \
            (event).type = PJSIP_EVENT_TRANSPORT_ERROR; \
            (event).body.tx_error.tsx = ptsx;		\
            (event).body.tx_error.tdata = ptdata;	\
        } while (0)

/**
 * Init user event.
 */
#define PJSIP_EVENT_INIT_USER(event,u1,u2,u3,u4)    \
        do { \
            (event).type = PJSIP_EVENT_USER;        \
            (event).body.user.user1 = (void*)u1;     \
            (event).body.user.user2 = (void*)u2;     \
            (event).body.user.user3 = (void*)u3;     \
            (event).body.user.user4 = (void*)u4;     \
        } while (0)

/**
 * Get the event string from the event ID.
 * @param e the event ID.
 * @note defined in sip_util.c
 */
PJ_DECL(const char *) pjsip_event_str(pjsip_event_id_e e);

/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJSIP_SIP_EVENT_H__ */
