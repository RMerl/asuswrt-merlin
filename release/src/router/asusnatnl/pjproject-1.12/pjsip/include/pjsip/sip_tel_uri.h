/* $Id: sip_tel_uri.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_TEL_URI_H__
#define __PJSIP_TEL_URI_H__

/**
 * @file sip_tel_uri.h
 * @brief Tel: URI
 */

#include <pjsip/sip_uri.h>

/**
 * @addtogroup PJSIP_TEL_URI tel URI Scheme
 * @ingroup PJSIP_URI
 * @brief Support for "tel:" URI scheme.
 * @{
 */


PJ_BEGIN_DECL

/**
 * tel: URI.
 */
typedef struct pjsip_tel_uri
{
    pjsip_uri_vptr *vptr;	/**< Pointer to virtual function table.	*/
    pj_str_t	    number;	/**< Global or local phone number	*/
    pj_str_t	    context;	/**< Phone context (for local number).	*/
    pj_str_t	    ext_param;	/**< Extension param.			*/
    pj_str_t	    isub_param;	/**< ISDN sub-address param.		*/
    pjsip_param	    other_param;/**< Other parameter.			*/
} pjsip_tel_uri;


/**
 * Create a new tel: URI.
 *
 * @param pool	    The pool.
 *
 * @return	    New instance of tel: URI.
 */
PJ_DECL(pjsip_tel_uri*) pjsip_tel_uri_create(pj_pool_t *pool);

/**
 * This function compares two numbers for equality, according to rules as
 * specified in RFC 3966.
 *
 * @param nb1	    The first number.
 * @param nb2	    The second number.
 *
 * @return	    Zero if equal, -1 if nb1 is less than nb2, or +1 if
 *		    nb1 is greater than nb2.
 */
PJ_DECL(int) pjsip_tel_nb_cmp(const pj_str_t *nb1, const pj_str_t *nb2);


PJ_END_DECL


/**
 * @}
 */


#endif	/* __PJSIP_TEL_URI_H__ */
