/* $Id: xpidf.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_SIMPLE_XPIDF_H__
#define __PJSIP_SIMPLE_XPIDF_H__

/**
 * @file xpidf.h
 * @brief XPIDF/Presence Information Data Format
 */
#include <pjsip-simple/types.h>
#include <pjlib-util/xml.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_SIMPLE_XPIDF XPIDF/Presence Information Data Format
 * @ingroup PJSIP_SIMPLE
 * @brief Support for XPIDF/Presence Information Data Format
 * @{
 *
 * This is an old presence data format as described in:
 * draft-rosenberg-impp-pidf-00.txt.
 *
 * We won't support this format extensively here, as it seems there's not
 * too many implementations support this anymore, as it shouldn't.
 */

/** Type definitions for XPIDF root document. */
typedef pj_xml_node pjxpidf_pres;


/**
 * Create a new XPIDF document.
 *
 * @param pool	    Pool.
 * @param uri	    URI to set in the XPIDF document.
 *
 * @return	    XPIDF document.
 */
PJ_DECL(pjxpidf_pres*) pjxpidf_create(pj_pool_t *pool, const pj_str_t *uri);


/**
 * Parse XPIDF document.
 *
 * @param inst_id   The instance id of pjsua.
 * @param pool	    Pool.
 * @param text	    Input text.
 * @param len	    Length of input text.
 *
 * @return	    XPIDF document.
 */
PJ_DECL(pjxpidf_pres*) pjxpidf_parse(int inst_id, pj_pool_t *pool, char *text, pj_size_t len);


/**
 * Print XPIDF document.
 *
 * @param pres	    The XPIDF document to print.
 * @param text	    Buffer to place the output.
 * @param len	    Length of the buffer.
 *
 * @return	    The length printed.
 */
PJ_DECL(int) pjxpidf_print( pjxpidf_pres *pres, char *text, pj_size_t len);


/**
 * Get URI in the XPIDF document
 *
 * @param pres	    XPIDF document
 *
 * @return	    The URI, or an empty string.
 */
PJ_DECL(pj_str_t*) pjxpidf_get_uri(pjxpidf_pres *pres);


/**
 * Set the URI of the XPIDF document.
 *
 * @param pool	    Pool.
 * @param pres	    The XPIDF document.
 * @param uri	    URI to set in the XPIDF document.
 *
 * @return	    Zero on success.
 */
PJ_DECL(pj_status_t) pjxpidf_set_uri(pj_pool_t *pool, pjxpidf_pres *pres, 
				     const pj_str_t *uri);


/**
 * Get presence status in the XPIDF document.
 *
 * @param pres	    XPIDF document.
 *
 * @return	    True to indicate the contact is online.
 */
PJ_DECL(pj_bool_t) pjxpidf_get_status(pjxpidf_pres *pres);


/**
 * Set presence status in the XPIDF document.
 *
 * @param pres	    XPIDF document.
 * @param status    Status to set, True for online, False for offline.
 *
 * @return	    Zero on success.
 */
PJ_DECL(pj_status_t) pjxpidf_set_status(pjxpidf_pres *pres, pj_bool_t status);


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJSIP_SIMPLE_XPIDF_H__ */
