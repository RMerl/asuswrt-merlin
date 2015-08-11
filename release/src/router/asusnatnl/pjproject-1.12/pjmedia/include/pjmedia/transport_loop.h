/* $Id: transport_loop.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_TRANSPORT_LOOP_H__
#define __PJMEDIA_TRANSPORT_LOOP_H__


/**
 * @file transport_loop.h
 * @brief Loopback transport
 */

#include <pjmedia/stream.h>


/**
 * @defgroup PJMEDIA_TRANSPORT_LOOP Loopback Media Transport
 * @ingroup PJMEDIA_TRANSPORT
 * @brief Loopback transport for testing.
 * @{
 *
 * This is the loopback media transport, where packets sent to this transport
 * will be sent back to the streams attached to this transport. Unlike the
 * other PJMEDIA transports, the loop transport may be attached to multiple
 * streams (in other words, application should specify the same loop transport
 * instance when calling #pjmedia_stream_create()). Any RTP or RTCP packets
 * sent by one stream to this transport by default will be sent back to all 
 * streams that are attached to this transport, including to the stream that
 * sends the packet. Application may individually select which stream to
 * receive packets by calling #pjmedia_transport_loop_disable_rx().
 */

PJ_BEGIN_DECL


/**
 * Create the loopback transport.
 *
 * @param endpt	    The media endpoint instance.
 * @param p_tp	    Pointer to receive the transport instance.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_transport_loop_create(pjmedia_endpt *endpt,
						   pjmedia_transport **p_tp);


/**
 * Set this stream as the receiver of incoming packets.
 */
PJ_DECL(pj_status_t) pjmedia_transport_loop_disable_rx(pjmedia_transport *tp,
						       void *user,
						       pj_bool_t disabled);


PJ_END_DECL


/**
 * @}
 */


#endif	/* __PJMEDIA_TRANSPORT_LOOP_H__ */


