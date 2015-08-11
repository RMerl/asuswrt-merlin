/* $Id: stun_simple.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSTUN_H__
#define __PJSTUN_H__

/**
 * @file stun.h
 * @brief STUN client.
 */

#include <pjlib-util/types.h>
#include <pj/sock.h>


PJ_BEGIN_DECL

/*
 * This enumeration describes STUN message types.
 */
typedef enum pjstun_msg_type
{
    PJSTUN_BINDING_REQUEST		    = 0x0001,
    PJSTUN_BINDING_RESPONSE		    = 0x0101,
    PJSTUN_BINDING_ERROR_RESPONSE	    = 0x0111,
    PJSTUN_SHARED_SECRET_REQUEST	    = 0x0002,
    PJSTUN_SHARED_SECRET_RESPONSE	    = 0x0102,
    PJSTUN_SHARED_SECRET_ERROR_RESPONSE    = 0x0112
} pjstun_msg_type;


/*
 * This enumeration describes STUN attribute types.
 */
typedef enum pjstun_attr_type
{
    PJSTUN_ATTR_MAPPED_ADDR = 1,
    PJSTUN_ATTR_RESPONSE_ADDR,
    PJSTUN_ATTR_CHANGE_REQUEST,
    PJSTUN_ATTR_SOURCE_ADDR,
    PJSTUN_ATTR_CHANGED_ADDR,
    PJSTUN_ATTR_USERNAME,
    PJSTUN_ATTR_PASSWORD,
    PJSTUN_ATTR_MESSAGE_INTEGRITY,
    PJSTUN_ATTR_ERROR_CODE,
    PJSTUN_ATTR_UNKNOWN_ATTRIBUTES,
    PJSTUN_ATTR_REFLECTED_FROM,
    PJSTUN_ATTR_XOR_MAPPED_ADDR = 0x0020
} pjstun_attr_type;


/*
 * This structre describes STUN message header.
 */
typedef struct pjstun_msg_hdr
{
    pj_uint16_t		type;
    pj_uint16_t		length;
    pj_uint32_t		tsx[4];
} pjstun_msg_hdr;


/*
 * This structre describes STUN attribute header.
 */
typedef struct pjstun_attr_hdr
{
    pj_uint16_t		type;
    pj_uint16_t		length;
} pjstun_attr_hdr;


/*
 * This structre describes STUN MAPPED-ADDR attribute.
 */
typedef struct pjstun_mapped_addr_attr
{
    pjstun_attr_hdr	hdr;
    pj_uint8_t		ignored;
    pj_uint8_t		family;
    pj_uint16_t		port;
    pj_uint32_t		addr;
} pjstun_mapped_addr_attr;

typedef pjstun_mapped_addr_attr pjstun_response_addr_attr;
typedef pjstun_mapped_addr_attr pjstun_changed_addr_attr;
typedef pjstun_mapped_addr_attr pjstun_src_addr_attr;
typedef pjstun_mapped_addr_attr pjstun_reflected_form_attr;

typedef struct pjstun_change_request_attr
{
    pjstun_attr_hdr	hdr;
    pj_uint32_t		value;
} pjstun_change_request_attr;

typedef struct pjstun_username_attr
{
    pjstun_attr_hdr	hdr;
    pj_uint32_t		value[1];
} pjstun_username_attr;

typedef pjstun_username_attr pjstun_password_attr;

typedef struct pjstun_error_code_attr
{
    pjstun_attr_hdr	hdr;
    pj_uint16_t		ignored;
    pj_uint8_t		err_class;
    pj_uint8_t		number;
    char		reason[4];
} pjstun_error_code_attr;

typedef struct pjstun_msg
{
    pjstun_msg_hdr    *hdr;
    int			attr_count;
    pjstun_attr_hdr   *attr[PJSTUN_MAX_ATTR];
} pjstun_msg;

/* STUN message API (stun.c). */

PJ_DECL(pj_status_t) pjstun_create_bind_req( pj_pool_t *pool, 
					      void **msg, pj_size_t *len,
					      pj_uint32_t id_hi,
					      pj_uint32_t id_lo);
PJ_DECL(pj_status_t) pjstun_parse_msg( void *buf, pj_size_t len,
				        pjstun_msg *msg);
PJ_DECL(void*) pjstun_msg_find_attr( pjstun_msg *msg, pjstun_attr_type t);


/**
 * @defgroup PJLIB_UTIL_STUN_CLIENT Simple STUN Helper
 * @ingroup PJ_PROTOCOLS
 * @brief A simple and small footprint STUN resolution helper
 * @{
 *
 * This is the older implementation of STUN client, with only one function
 * provided (pjstun_get_mapped_addr()) to retrieve the public IP address
 * of multiple sockets.
 */

/**
 * This is the main function to request the mapped address of local sockets
 * to multiple STUN servers. This function is able to find the mapped 
 * addresses of multiple sockets simultaneously, and for each socket, two
 * requests will be sent to two different STUN servers to see if both servers
 * get the same public address for the same socket. (Note that application can
 * specify the same address for the two servers, but still two requests will
 * be sent for each server).
 *
 * This function will perform necessary retransmissions of the requests if
 * response is not received within a predetermined period. When all responses
 * have been received, the function will compare the mapped addresses returned
 * by the servers, and when both are equal, the address will be returned in
 * \a mapped_addr argument.
 *
 * @param pf		The pool factory where memory will be allocated from.
 * @param sock_cnt	Number of sockets in the socket array.
 * @param sock		Array of local UDP sockets which public addresses are
 *			to be queried from the STUN servers.
 * @param srv1		Host name or IP address string of the first STUN
 *			server.
 * @param port1		The port number of the first STUN server. 
 * @param srv2		Host name or IP address string of the second STUN
 *			server.
 * @param port2		The port number of the second STUN server. 
 * @param mapped_addr	Array to receive the mapped public address of the local
 *			UDP sockets, when the function returns PJ_SUCCESS.
 *
 * @return		This functions returns PJ_SUCCESS if responses are
 *			received from all servers AND all servers returned the
 *			same mapped public address. Otherwise this function may
 *			return one of the following error codes:
 *			- PJLIB_UTIL_ESTUNNOTRESPOND: no respons from servers.
 *			- PJLIB_UTIL_ESTUNSYMMETRIC: different mapped addresses
 *			  are returned by servers.
 *			- etc.
 *
 */
PJ_DECL(pj_status_t) pjstun_get_mapped_addr( pj_pool_factory *pf,
					      int sock_cnt, pj_sock_t sock[],
					      const pj_str_t *srv1, int port1,
					      const pj_str_t *srv2, int port2,
					      pj_sockaddr_in mapped_addr[]);

PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJSTUN_H__ */

