/* $Id: natnl_codec.h,v 1.1.1.1 2011/12/27 06:17:38 andrew Exp $ */
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
#ifndef __NAT_TUNNEL_CODEC_H__
#define __NAT_TUNNEL_CODEC_H__

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * @file ntc.h
	 * @brief ASUS proprietary NAT tunnel Codec
	 */

#include <pjmedia-codec/types.h>

	 /**
	  * @defgroup PJMED_NAT tunnel Codec
	  * @ingroup PJMEDIA_CODEC_CODECS
	  * @brief ASUS proprietary tunnel codec.
	  * @{
	  *
	  * This section describes functions to initialize and register NAT tunnel codec
	  * factory to the codec manager. After the codec factory has been registered,
	  * application can use @ref PJMEDIA_CODEC API to manipulate the codec.
	  *
	  * \section codec_setting Codec Settings
	  *
	  * \subsection general_setting General Settings
	  *
	  * General codec settings for this codec such as VAD and PLC can be
	  * manipulated through the <tt>setting</tt> field in #pjmedia_codec_param.
	  * Please see the documentation of #pjmedia_codec_param for more info.
	  *
	  * \subsection specific_setting Codec Specific Settings
	  *
	  * Currently none.
	  */


	  /**
	   * Initialize and register NAT tunnel codec factory to pjmedia endpoint.
	   * This will register PCMU and PCMA codec, in that order.
	   *
	   * @param inst_id	The the instance id of pjsua.
	   * @param endpt		The pjmedia endpoint.
	   *
	   * @return		PJ_SUCCESS on success.
	   */
	PJ_DECL(pj_status_t) pjmedia_codec_ntc_init(int inst_id, pjmedia_endpt *endpt);



	/**
	 * Unregister NAT tunnel codec factory from pjmedia endpoint.
	 *
	 * @param inst_id	The the instance id of pjsua.
	 * @return	    PJ_SUCCESS on success.
	 */
	PJ_DECL(pj_status_t) pjmedia_codec_ntc_deinit(int inst_id);


#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif	/* __NAT_TUNNEL_CODEC_H__ */

