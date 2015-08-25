/* $Id: rpid.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_SIMPLE_RPID_H__
#define __PJSIP_SIMPLE_RPID_H__

/**
 * @file rpid.h
 * @brief RPID: Rich Presence Extensions to the PIDF (RFC 4480)
 */
#include <pjsip-simple/types.h>
#include <pjsip-simple/pidf.h>

PJ_BEGIN_DECL


/**
 * @defgroup PJSIP_SIMPLE_RPID RPID/Rich Presence Extensions to PIDF (RFC 4480)
 * @ingroup PJSIP_SIMPLE
 * @brief RPID/Rich Presence Extensions to PIDF (RFC 4480)
 * @{
 *
 * This file provides tools for managing subset of RPID elements into
 * PIDF document.
 */

/**
 * This enumeration describes subset of standard activities as 
 * described by RFC 4880, RPID: Rich Presence Extensions to the 
 * Presence Information Data Format (PIDF). 
 */
typedef enum pjrpid_activity
{
    /** Activity is unknown. The activity would then be conceived
     *  in the "note" field.
     */
    PJRPID_ACTIVITY_UNKNOWN,

    /** The person is away */
    PJRPID_ACTIVITY_AWAY,

    /** The person is busy */
    PJRPID_ACTIVITY_BUSY

} pjrpid_activity;


/**
 * This enumeration describes types of RPID element.
 */
typedef enum pjrpid_element_type
{
    /** RPID <person> element */
    PJRPID_ELEMENT_TYPE_PERSON

} pjrpid_element_type;


/**
 * This structure describes person information in RPID document.
 */
typedef struct pjrpid_element
{
    /** Element type. */
    pjrpid_element_type	    type;

    /** Optional id to set on the element. */
    pj_str_t		    id;

    /** Activity type. */
    pjrpid_activity	    activity;

    /** Optional text describing the person/element. */
    pj_str_t		    note;

} pjrpid_element;


/**
 * Duplicate RPID element.
 *
 * @param pool	    Pool.
 * @param dst	    Destination structure.
 * @param src	    Source structure.
 */
PJ_DECL(void) pjrpid_element_dup(pj_pool_t *pool, pjrpid_element *dst,
				 const pjrpid_element *src);


/**
 * Add RPID element information into existing PIDF document. This will also
 * add the appropriate XML namespace attributes into the presence's XML
 * node, if the attributes are not already present, and also a <note> element
 * to the first <tuple> element of the PIDF document.
 *
 * @param pres	    The PIDF presence document.
 * @param pool	    Pool.
 * @param options   Currently unused, and must be zero.
 * @param elem	    RPID element information to be added into the PIDF
 *		    document.
 *
 * @return PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjrpid_add_element(pjpidf_pres *pres,
				        pj_pool_t *pool,
					unsigned options,
				        const pjrpid_element *elem);

/**
 * Get RPID element information from PIDF document, if any.
 *
 * @param pres	    The PIDF document containing RPID elements.
 * @param pool	    Pool to duplicate the information.
 * @param elem	    Structure to receive the element information.
 *
 * @return PJ_SUCCESS	if the document does contain RPID element
 *			and the information has been parsed successfully.
 */
PJ_DECL(pj_status_t) pjrpid_get_element(const pjpidf_pres *pres,
				        pj_pool_t *pool,
				        pjrpid_element *elem);


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJSIP_SIMPLE_RPID_H__ */

