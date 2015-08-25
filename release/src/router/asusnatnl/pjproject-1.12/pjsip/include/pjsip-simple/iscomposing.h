/* $Id: iscomposing.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_SIMPLE_ISCOMPOSING_H__
#define __PJSIP_SIMPLE_ISCOMPOSING_H__

/**
 * @file iscomposing.h
 * @brief Support for Indication of Message Composition (RFC 3994)
 */
#include <pjsip-simple/types.h>
#include <pjlib-util/xml.h>

/**
 * @defgroup PJSIP_ISCOMPOSING Message Composition Indication (RFC 3994)
 * @ingroup PJSIP_SIMPLE
 * @brief Support for Indication of Message Composition (RFC 3994)
 * @{
 *
 * This implements message composition indication, as described in
 * RFC 3994.
 */

PJ_BEGIN_DECL


/**
 * Create XML message with MIME type "application/im-iscomposing+xml"
 * to indicate the message composition status.
 *
 * @param pool		    Pool to allocate memory.
 * @param is_composing	    Message composition indication status. Set to
 *			    PJ_TRUE (or non-zero) to indicate that application
 *			    is currently composing an instant message.
 * @param lst_actv	    Optional attribute to indicate time of last
 *			    activity. If none is to be specified, the value
 *			    MUST be set to NULL.
 * @param content_tp	    Optional attribute to indicate the content type of
 *			    message being composed. If none is to be specified, 
 *			    the value MUST be set to NULL.
 * @param refresh	    Optional attribute to indicate the interval when
 *			    next indication will be sent, only when 
 *			    is_composing is non-zero. If none is to be 
 *			    specified, the value MUST be set to -1.
 *
 * @return		    An XML message containing the message indication.
 *			    NULL will be returned when there's not enough
 *			    memory to allocate the message.
 */
PJ_DECL(pj_xml_node*) pjsip_iscomposing_create_xml(pj_pool_t *pool,
						   pj_bool_t is_composing,
						   const pj_time_val *lst_actv,
						   const pj_str_t *content_tp,
						   int refresh);


/**
 * Create message body with Content-Type "application/im-iscomposing+xml"
 * to indicate the message composition status.
 *
 * @param pool		    Pool to allocate memory.
 * @param is_composing	    Message composition indication status. Set to
 *			    PJ_TRUE (or non-zero) to indicate that application
 *			    is currently composing an instant message.
 * @param lst_actv	    Optional attribute to indicate time of last
 *			    activity. If none is to be specified, the value
 *			    MUST be set to NULL.
 * @param content_tp	    Optional attribute to indicate the content type of
 *			    message being composed. If none is to be specified, 
 *			    the value MUST be set to NULL.
 * @param refresh	    Optional attribute to indicate the interval when
 *			    next indication will be sent, only when 
 *			    is_composing is non-zero. If none is to be 
 *			    specified, the value MUST be set to -1.
 *
 * @return		    The SIP message body containing XML message 
 *			    indication. NULL will be returned when there's not
 *			    enough memory to allocate the message.
 */
PJ_DECL(pjsip_msg_body*) pjsip_iscomposing_create_body( pj_pool_t *pool,
						   pj_bool_t is_composing,
						   const pj_time_val *lst_actv,
						   const pj_str_t *content_tp,
						   int refresh);


/**
 * Parse the buffer and return message composition indication in the 
 * message.
 *
 * @param inst_id       The instance id of pjsua.
 * @param pool		    Pool to allocate memory for the parsing process.
 * @param msg		    The message to be parsed.
 * @param len		    Length of the message.
 * @param p_is_composing    Optional pointer to receive iscomposing status.
 * @param p_last_active	    Optional pointer to receive last active attribute.
 * @param p_content_type    Optional pointer to receive content type attribute.
 * @param p_refresh	    Optional pointer to receive refresh time.
 *
 * @return		    PJ_SUCCESS if message can be successfully parsed.
 */
PJ_DECL(pj_status_t) pjsip_iscomposing_parse( int inst_id,
						  pj_pool_t *pool,
					      char *msg,
					      pj_size_t len,
					      pj_bool_t *p_is_composing,
					      pj_str_t **p_last_active,
					      pj_str_t **p_content_type,
					      int *p_refresh );


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJSIP_SIMPLE_ISCOMPOSING_H__ */

