/* $Id: sip_transport_loop.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_TRANSPORT_LOOP_H__
#define __PJSIP_TRANSPORT_LOOP_H__


/**
 * @file sip_transport_loop.h
 * @brief 
 * Loopback transport (for debugging)
 */


#include <pjsip/sip_transport.h>

/**
 * @defgroup PJSIP_TRANSPORT_LOOP Loop Transport
 * @ingroup PJSIP_TRANSPORT
 * @brief Loopback transport (for testing purposes).
 * @{
 * The loopback transport simply bounce back outgoing messages as
 * incoming messages. This feature is used mostly during automated
 * testing, to provide controlled behavior.
 */

PJ_BEGIN_DECL

/**
 * Create and start datagram loop transport.
 *
 * @param endpt		The endpoint instance.
 * @param transport	Pointer to receive the transport instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_loop_start( pjsip_endpoint *endpt,
				       pjsip_transport **transport);


/**
 * Enable/disable flag to discard any packets sent using the specified
 * loop transport.
 *
 * @param tp		The loop transport.
 * @param discard	If non-zero, any outgoing packets will be discarded.
 * @param prev_value	Optional argument to receive previous value of
 *			the discard flag.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_loop_set_discard( pjsip_transport *tp,
					     pj_bool_t discard,
					     pj_bool_t *prev_value );


/**
 * Enable/disable flag to simulate network error. When this flag is set,
 * outgoing transmission will return either immediate error or error via
 * callback. If error is to be notified via callback, then the notification
 * will occur after some delay, which is controlled by #pjsip_loop_set_delay().
 *
 * @param tp		The loop transport.
 * @param fail_flag	If set to 1, the transport will return fail to deliver
 *			the message. If delay is zero, failure will occur
 *			immediately; otherwise it will be reported in callback.
 *			If set to zero, the transport will successfully deliver
 *			the packet.
 * @param prev_value	Optional argument to receive previous value of
 *			the failure flag.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_loop_set_failure( pjsip_transport *tp,
					     int fail_flag,
					     int *prev_value );


/**
 * Set delay (in miliseconds) before packet is received by the other end
 * of the loop transport. This will also 
 * control the delay for error notification callback.
 *
 * @param tp		The loop transport.
 * @param delay		Delay, in miliseconds.
 * @param prev_value	Optional argument to receive previous value of the
 *			delay.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_loop_set_recv_delay( pjsip_transport *tp,
						unsigned delay,
						unsigned *prev_value);


/**
 * Set delay (in miliseconds) before send notification is delivered to sender.
 * This will also control the delay for error notification callback.
 *
 * @param tp		The loop transport.
 * @param delay		Delay, in miliseconds.
 * @param prev_value	Optional argument to receive previous value of the
 *			delay.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_loop_set_send_callback_delay( pjsip_transport *tp,
							 unsigned delay,
							 unsigned *prev_value);


/**
 * Set both receive and send notification delay.
 *
 * @param tp		The loop transport.
 * @param delay		Delay, in miliseconds.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_loop_set_delay( pjsip_transport *tp,
					   unsigned delay );


PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJSIP_TRANSPORT_LOOP_H__ */

