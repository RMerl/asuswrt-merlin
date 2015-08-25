/* $Id: transport_tcp.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_TRANSPORT_TCP_H__
#define __PJMEDIA_TRANSPORT_TCP_H__


/**
 * @file transport_tcp.h
 * @brief Stream transport with TCP.
 */

#include <pjmedia/stream.h>


/**
 * @defgroup PJMEDIA_TRANSPORT_TCP TCP Media Transport
 * @ingroup PJMEDIA_TRANSPORT
 * @brief Implementation of media transport with TCP sockets.
 * @{
 *
 * The TCP media transport is the standard based media transport
 * as described by RFC 3550/3551. It can be used to facilitate RTP/RTCP
 * unicast or multicast communication.
 */

PJ_BEGIN_DECL


/**
 * Options that can be specified when creating TCP transport.
 */
enum pjmedia_transport_TCP_options
{
    /**
     * Normally the TCP transport will continuously check the source address
     * of incoming packets to see if it is different than the configured
     * remote address, and switch the remote address to the source address
     * of the packet if they are different after several packets are
     * received.
     * Specifying this option will disable this feature.
     */
    PJMEDIA_TCP_NO_SRC_ADDR_CHECKING = 1
};


/**
 * Create an RTP and RTCP sockets and bind the sockets to the specified
 * port to create media transport.
 *
 * @param endpt	    The media endpoint instance.
 * @param name	    Optional name to be assigned to the transport.
 * @param port	    TCP port number for the RTP socket. The RTCP port number
 *		    will be set to one above RTP port.
 * @param options   Options, bitmask of #pjmedia_transport_tcp_options.
 * @param p_tp	    Pointer to receive the transport instance.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_transport_tcp_create(pjmedia_endpt *endpt,
						  const char *name,
						  int port,
						  unsigned options,
						  pjmedia_transport **p_tp);


/**
 * Create an RTP and RTCP sockets and bind the sockets to the specified
 * address and port to create media transport.
 *
 * @param endpt	    The media endpoint instance.
 * @param name	    Optional name to be assigned to the transport.
 * @param addr	    Optional local address to bind the sockets to. If this
 *		    argument is NULL or empty, the sockets will be bound
 *		    to all interface.
 * @param port	    TCP port number for the RTP socket. The RTCP port number
 *		    will be set to one above RTP port.
 * @param options   Options, bitmask of #pjmedia_transport_tcp_options.
 * @param p_tp	    Pointer to receive the transport instance.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_transport_tcp_create2(pjmedia_endpt *endpt,
						   const char *name,
						   const pj_str_t *addr,
						   int port,
						   unsigned options,
						   pjmedia_transport **p_tp);

/**
 * Another variant of #pjmedia_transport_tcp_create() which allows
 * the creation of IPv6 transport.
 *
 * @param endpt	    The media endpoint instance.
 * @param af	    Address family, which can be pj_AF_INET() for IPv4 or
 *		    pj_AF_INET6() for IPv6.
 * @param name	    Optional name to be assigned to the transport.
 * @param addr	    Optional local address to bind the sockets to. If this
 *		    argument is NULL or empty, the sockets will be bound
 *		    to all interface.
 * @param port	    TCP port number for the RTP socket. The RTCP port number
 *		    will be set to one above RTP port.
 * @param options   Options, bitmask of #pjmedia_transport_tcp_options.
 * @param p_tp	    Pointer to receive the transport instance.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_transport_tcp_create3(pjmedia_endpt *endpt,
						   int af,
						   const char *name,
						   const pj_str_t *addr,
						   int port,
						   unsigned options,
						   pjmedia_transport **p_tp);


/**
 * Create TCP stream transport from existing sockets. Use this function when
 * the sockets have previously been created.
 *
 * @param endpt	    The media endpoint instance.
 * @param name	    Optional name to be assigned to the transport.
 * @param si	    Media socket info containing the RTP and RTCP sockets.
 * @param options   Options, bitmask of #pjmedia_transport_tcp_options.
 * @param p_tp	    Pointer to receive the transport instance.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_transport_tcp_attach(pjmedia_endpt *endpt,
						  const char *name,
						  const pjmedia_sock_info *si,
						  unsigned options,
						  pjmedia_transport **p_tp);


PJ_END_DECL


/**
 * @}
 */


#endif	/* __PJMEDIA_TRANSPORT_TCP_H__ */


