/* $Id: opencore_amrnb.h 3816 2011-10-14 04:15:15Z bennylp $ */
/*
 * Copyright (C) 2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2011 Dan Arrhenius <dan@keystream.se>
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
#ifndef __PJMEDIA_CODEC_OPENCORE_AMRNB_H__
#define __PJMEDIA_CODEC_OPENCORE_AMRNB_H__

#include <pjmedia-codec/types.h>

/**
 * @defgroup PJMED_OC_AMRNB OpenCORE AMR-NB Codec
 * @ingroup PJMEDIA_CODEC_CODECS
 * @brief AMRCodec wrapper for OpenCORE AMR-NB codec
 * @{
 */

PJ_BEGIN_DECL

/**
 * Settings. Use #pjmedia_codec_opencore_amrnb_set_config() to
 * activate.
 */
typedef struct pjmedia_codec_amrnb_config
{
    /**
     * Control whether to use octent align.
     */
    pj_bool_t octet_align;

    /**
     * Set the bitrate.
     */
    unsigned bitrate;

} pjmedia_codec_amrnb_config;


/**
 * Initialize and register AMR-NB codec factory to pjmedia endpoint.
 *
 * @param endpt	The pjmedia endpoint.
 *
 * @return	PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_opencore_amrnb_init(pjmedia_endpt* endpt);

/**
 * Unregister AMR-NB codec factory from pjmedia endpoint and deinitialize
 * the OpenCORE codec library.
 *
 * @return	PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_opencore_amrnb_deinit(void);


/**
 * Set AMR-NB parameters.
 *
 * @param cfg	The settings;
 *
 * @return	PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_opencore_amrnb_set_config(
				const pjmedia_codec_amrnb_config* cfg);

PJ_END_DECL


/**
 * @}
 */

#endif	/* __PJMEDIA_CODEC_OPENCORE_AMRNB_H__ */

