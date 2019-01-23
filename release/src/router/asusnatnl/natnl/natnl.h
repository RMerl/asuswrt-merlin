/* $Id: natnl.h,v 1.2 2011/12/27 07:34:26 andrew Exp $ */
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
#ifndef __PJMEDIA_NATNL_H__
#define __PJMEDIA_NATNL_H__

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * @file natnl.h
	 * @brief NAT tunnel system.
	 */
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>

#include <natnl_lib.h>

#define NULL_SND_DEV_ID          -99

#define RTP_HDR_SIZE              12
	 /**
	  * @defgroup PJMEDIA_NATNL NAT tunnel system
	  * @ingroup PJSUA.H
	  * @brief The simplest type of NAT tunnel system.
	  * @{
	  */

	extern char userid[64];

	/**
	 * Set maximum number of the instance of pjsua
	 */
	PJ_DECL(void) set_max_instances(unsigned max_insts);

	/**
	 * Get maximum number of the instance of pjsua
	 */
	PJ_DECL(int) get_max_instances();

	extern void pjsua_media_config_dup(pj_pool_t *pool,
		pjsua_media_config *dst,
		const pjsua_media_config *src);

	//#if  defined(NATNL_LIB) && !defined(PJ_ANDROID) && PJ_CONFIG_IPHONE!=1
	//extern char callee[128], registrar_uri[128];
	//#endif

	PJ_DECL(pj_status_t) natnl_logging_endpt_create(pjsua_inst_id inst_id,
		const pjsua_logging_config *log_cfg);

	//PJ_DECL(pj_status_t) natnl_logging_endpt_destroy(pjsua_inst_id inst_id);

	PJ_DECL(pj_status_t) natnl_create(pjsua_inst_id inst_id);

	/**
	 * Initialize natnl with the specified settings. All the settings are
	 * optional, and the default values will be used when the config is not
	 * specified.
	 *
	 * Note that #pjsua_create() MUST be called before calling this function.
	 *
	 * @param ua_cfg    User agent configuration.
	 * @param log_cfg   Optional logging configuration.
	 * @param media_cfg Optional media configuration.
	 *
	 * @return      PJ_SUCCESS on success, or the appropriate error code.
	 */
	PJ_DECL(pj_status_t) natnl_init(pjsua_inst_id inst_id,
		const pjsua_config *ua_cfg,
		const pjsua_logging_config *log_cfg,
		const pjsua_media_config *media_cfg);

	PJ_DECL(pjsua_call *) pjsua_get_call(pjsua_inst_id inst_id, pjsua_call_id call_id);
	PJ_DECL(pjmedia_transport *) pjsua_get_media_transport(pjsua_inst_id inst_id, pjsua_call_id call_id);

	extern struct natnl_callback natnl_callback;
	PJ_DECL(void) natnl_call_callback(struct natnl_tnl_event *tnl_event);
	PJ_DECL(void) natnl_call_callback2(struct natnl_tnl_event *tnl_event, int use_pj_log);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */


#endif	/* __PJMEDIA_NATNL_H__ */
