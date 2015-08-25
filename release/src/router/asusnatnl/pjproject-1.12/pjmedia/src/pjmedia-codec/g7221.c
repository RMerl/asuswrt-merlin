/* $Id: g7221.c 3880 2011-11-02 07:50:44Z nanang $ */
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
#include <pjmedia-codec/g7221.h>
#include <pjmedia/codec.h>
#include <pjmedia/errno.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/port.h>
#include <pjmedia/silencedet.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/math.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/os.h>

/*
 * Only build this file if PJMEDIA_HAS_G7221_CODEC != 0
 */
#if defined(PJMEDIA_HAS_G7221_CODEC) && PJMEDIA_HAS_G7221_CODEC!=0

#include "../../../third_party/g7221/common/defs.h"

#define THIS_FILE	    "g7221.c"

/* Codec tag, it is the SDP encoding name and also MIME subtype name */
#define CODEC_TAG	    "G7221"

/* Sampling rates definition */
#define WB_SAMPLE_RATE	    16000
#define UWB_SAMPLE_RATE	    32000

/* Maximum number of samples per frame. */
#define MAX_SAMPLES_PER_FRAME (UWB_SAMPLE_RATE * 20 / 1000)

/* Maximum number of codec params. */
#define MAX_CODEC_MODES	    8
#define START_RSV_MODES_IDX 6


/* Prototypes for G722.1 codec factory */
static pj_status_t test_alloc( pjmedia_codec_factory *factory, 
			       const pjmedia_codec_info *id );
static pj_status_t default_attr( pjmedia_codec_factory *factory, 
				 const pjmedia_codec_info *id, 
				 pjmedia_codec_param *attr );
static pj_status_t enum_codecs( pjmedia_codec_factory *factory, 
				unsigned *count, 
				pjmedia_codec_info codecs[]);
static pj_status_t alloc_codec( pjmedia_codec_factory *factory, 
				const pjmedia_codec_info *id, 
				pjmedia_codec **p_codec);
static pj_status_t dealloc_codec( pjmedia_codec_factory *factory, 
				  pjmedia_codec *codec );

/* Prototypes for G722.1 codec implementation. */
static pj_status_t codec_init( pjmedia_codec *codec, 
			       pj_pool_t *pool );
static pj_status_t codec_open( pjmedia_codec *codec, 
			       pjmedia_codec_param *attr );
static pj_status_t codec_close( pjmedia_codec *codec );
static pj_status_t codec_modify(pjmedia_codec *codec, 
			        const pjmedia_codec_param *attr );
static pj_status_t codec_parse( pjmedia_codec *codec,
			        void *pkt,
				pj_size_t pkt_size,
				const pj_timestamp *ts,
				unsigned *frame_cnt,
				pjmedia_frame frames[]);
static pj_status_t codec_encode( pjmedia_codec *codec, 
				 const struct pjmedia_frame *input,
				 unsigned output_buf_len,
				 struct pjmedia_frame *output);
static pj_status_t codec_decode( pjmedia_codec *codec,
				 const struct pjmedia_frame *input,
				 unsigned output_buf_len, 
				 struct pjmedia_frame *output);
static pj_status_t codec_recover( pjmedia_codec *codec, 
				  unsigned output_buf_len, 
				  struct pjmedia_frame *output);

/* Definition for G722.1 codec operations. */
static pjmedia_codec_op codec_op = 
{
    &codec_init,
    &codec_open,
    &codec_close,
    &codec_modify,
    &codec_parse,
    &codec_encode,
    &codec_decode,
    &codec_recover
};

/* Definition for G722.1 codec factory operations. */
static pjmedia_codec_factory_op codec_factory_op =
{
    &test_alloc,
    &default_attr,
    &enum_codecs,
    &alloc_codec,
    &dealloc_codec
};


/* Structure of G722.1 mode */
typedef struct codec_mode
{
    pj_bool_t	     enabled;		/* Is this mode enabled?	    */
    pj_uint8_t	     pt;		/* Payload type.		    */
    unsigned	     sample_rate;	/* Default sampling rate to be used.*/
    unsigned	     bitrate;		/* Bitrate.			    */
    char	     bitrate_str[8];	/* Bitrate in string.		    */
} codec_mode;


/* G722.1 codec factory */
static struct codec_factory {
    pjmedia_codec_factory    base;	    /**< Base class.		    */
    pjmedia_endpt	    *endpt;	    /**< PJMEDIA endpoint instance. */
    pj_pool_t		    *pool;	    /**< Codec factory pool.	    */
    pj_mutex_t		    *mutex;	    /**< Codec factory mutex.	    */

    int			     pcm_shift;	    /**< Level adjustment	    */
    unsigned		     mode_count;    /**< Number of G722.1 modes.    */
    codec_mode		     modes[MAX_CODEC_MODES]; /**< The G722.1 modes. */
    unsigned		     mode_rsv_start;/**< Start index of G722.1 non-
						 standard modes, currently
						 there can only be up to two 
						 non-standard modes enabled
						 at the same time.	    */
} codec_factory;

/* G722.1 codec private data. */
typedef struct codec_private {
    pj_pool_t		*pool;		    /**< Pool for each instance.    */
    pj_bool_t		 plc_enabled;	    /**< PLC enabled?		    */
    pj_bool_t		 vad_enabled;	    /**< VAD enabled?		    */
    pjmedia_silence_det	*vad;		    /**< PJMEDIA VAD instance.	    */
    pj_timestamp	 last_tx;	    /**< Timestamp of last transmit.*/

    /* ITU ref implementation seems to leave the codec engine states to be
     * managed by the application, so here we go.
     */

    /* Common engine state */
    pj_uint16_t		 samples_per_frame; /**< Samples per frame.	    */
    pj_uint16_t		 bitrate;	    /**< Coded stream bitrate.	    */
    pj_uint16_t		 frame_size;	    /**< Coded frame size.	    */
    pj_uint16_t		 frame_size_bits;   /**< Coded frame size in bits.  */
    pj_uint16_t		 number_of_regions; /**< Number of regions.	    */
    int			 pcm_shift;	    /**< Adjustment for PCM in/out  */
    
    /* Encoder specific state */
    Word16		*enc_frame;	    /**< 16bit to 14bit buffer	    */
    Word16		*enc_old_frame;
    
    /* Decoder specific state */
    Word16		*dec_old_frame;
    Rand_Obj		 dec_randobj;
    Word16		 dec_old_mag_shift;
    Word16		*dec_old_mlt_coefs;
} codec_private_t;

/* 
 * Helper function for looking up mode based on payload type.
 */
static codec_mode* lookup_mode(unsigned pt)
{
    codec_mode* mode = NULL;
    unsigned i;

    for (i = 0; i < codec_factory.mode_count; ++i) {
	mode = &codec_factory.modes[i];
	if (mode->pt == pt)
	    break;
    }

    return mode;
}

/* 
 * Helper function to validate G722.1 mode. Valid modes are defined as:
 * 1. sample rate must be 16kHz or 32kHz,
 * 2. bitrate:
 *    - for sampling rate 16kHz: 16000 to 32000 bps, it must be a multiple 
 *      of 400 (to keep RTP payload octed-aligned)
 *    - for sampling rate 32kHz: 24000 to 48000 bps, it must be a multiple 
 *      of 400 (to keep RTP payload octed-aligned)
 */
static pj_bool_t validate_mode(unsigned sample_rate, unsigned bitrate)
{
    if (sample_rate == WB_SAMPLE_RATE) {
	if (bitrate < 16000 || bitrate > 32000 ||
	    ((bitrate-16000) % 400 != 0))
	{
	    return PJ_FALSE;
	}
    } else if (sample_rate == UWB_SAMPLE_RATE) {
	if (bitrate < 24000 || bitrate > 48000 ||
	    ((bitrate-24000) % 400 != 0))
	{
	    return PJ_FALSE;
	}
    } else {
	return PJ_FALSE;
    }

    return PJ_TRUE;
}

#if defined(PJ_IS_LITTLE_ENDIAN) && PJ_IS_LITTLE_ENDIAN!=0
PJ_INLINE(void) swap_bytes(pj_uint16_t *buf, unsigned count)
{
    pj_uint16_t *end = buf + count;
    while (buf != end) {
	*buf = (pj_uint16_t)((*buf << 8) | (*buf >> 8));
	++buf;
    }
}
#else
#define swap_bytes(buf, count)
#endif

/*
 * Initialize and register G722.1 codec factory to pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_g7221_init( pjmedia_endpt *endpt )
{
    pjmedia_codec_mgr *codec_mgr;
    codec_mode *mode;
    pj_status_t status;

    if (codec_factory.pool != NULL) {
	/* Already initialized. */
	return PJ_SUCCESS;
    }

    /* Initialize codec modes, by default all standard bitrates are enabled */
    codec_factory.mode_count = 0;
    codec_factory.pcm_shift = PJMEDIA_G7221_DEFAULT_PCM_SHIFT;

    mode = &codec_factory.modes[codec_factory.mode_count++];
    mode->enabled = PJ_TRUE;
    mode->pt = PJMEDIA_RTP_PT_G722_1_24;
    mode->sample_rate = WB_SAMPLE_RATE;
    mode->bitrate = 24000;
    pj_utoa(mode->bitrate, mode->bitrate_str);

    mode = &codec_factory.modes[codec_factory.mode_count++];
    mode->enabled = PJ_TRUE;
    mode->pt = PJMEDIA_RTP_PT_G722_1_32;
    mode->sample_rate = WB_SAMPLE_RATE;
    mode->bitrate = 32000;
    pj_utoa(mode->bitrate, mode->bitrate_str);

    mode = &codec_factory.modes[codec_factory.mode_count++];
    mode->enabled = PJ_TRUE;
    mode->pt = PJMEDIA_RTP_PT_G7221C_24;
    mode->sample_rate = UWB_SAMPLE_RATE;
    mode->bitrate = 24000;
    pj_utoa(mode->bitrate, mode->bitrate_str);

    mode = &codec_factory.modes[codec_factory.mode_count++];
    mode->enabled = PJ_TRUE;
    mode->pt = PJMEDIA_RTP_PT_G7221C_32;
    mode->sample_rate = UWB_SAMPLE_RATE;
    mode->bitrate = 32000;
    pj_utoa(mode->bitrate, mode->bitrate_str);

    mode = &codec_factory.modes[codec_factory.mode_count++];
    mode->enabled = PJ_TRUE;
    mode->pt = PJMEDIA_RTP_PT_G7221C_48;
    mode->sample_rate = UWB_SAMPLE_RATE;
    mode->bitrate = 48000;
    pj_utoa(mode->bitrate, mode->bitrate_str);

    /* Non-standard bitrates */

    /* Bitrate 16kbps is non-standard but rather commonly used. */
    mode = &codec_factory.modes[codec_factory.mode_count++];
    mode->enabled = PJ_FALSE;
    mode->pt = PJMEDIA_RTP_PT_G722_1_16;
    mode->sample_rate = WB_SAMPLE_RATE;
    mode->bitrate = 16000;
    pj_utoa(mode->bitrate, mode->bitrate_str);

    /* Reserved two modes for non-standard bitrates */
    codec_factory.mode_rsv_start = codec_factory.mode_count;
    mode = &codec_factory.modes[codec_factory.mode_count++];
    mode->enabled = PJ_FALSE;
    mode->pt = PJMEDIA_RTP_PT_G7221_RSV1;

    mode = &codec_factory.modes[codec_factory.mode_count++];
    mode->enabled = PJ_FALSE;
    mode->pt = PJMEDIA_RTP_PT_G7221_RSV2;

    pj_assert(codec_factory.mode_count <= MAX_CODEC_MODES);

    /* Create G722.1 codec factory. */
    codec_factory.base.op = &codec_factory_op;
    codec_factory.base.factory_data = NULL;
    codec_factory.endpt = endpt;

    codec_factory.pool = pjmedia_endpt_create_pool(endpt, "G722.1 codec",
						   4000, 4000);
    if (!codec_factory.pool)
	return PJ_ENOMEM;

    /* Create mutex. */
    status = pj_mutex_create_simple(codec_factory.pool, "G722.1 codec",
				    &codec_factory.mutex);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
    if (!codec_mgr) {
	status = PJ_EINVALIDOP;
	goto on_error;
    }

    /* Register codec factory to endpoint. */
    status = pjmedia_codec_mgr_register_factory(codec_mgr, 
						&codec_factory.base);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Done. */
    return PJ_SUCCESS;

on_error:
    if (codec_factory.mutex) {
	pj_mutex_destroy(codec_factory.mutex);
	codec_factory.mutex = NULL;
    }

    pj_pool_release(codec_factory.pool);
    codec_factory.pool = NULL;
    return status;
}


/**
 * Enable and disable G722.1 modes, including non-standard modes.
 */
PJ_DEF(pj_status_t) pjmedia_codec_g7221_set_mode(unsigned sample_rate, 
						 unsigned bitrate, 
						 pj_bool_t enabled)
{
    unsigned i;

    /* Validate mode */
    if (!validate_mode(sample_rate, bitrate))
	return PJMEDIA_CODEC_EINMODE;

    /* Look up in factory modes table */
    for (i = 0; i < codec_factory.mode_count; ++i) {
	if (codec_factory.modes[i].sample_rate == sample_rate &&
	    codec_factory.modes[i].bitrate == bitrate)
	{
	    codec_factory.modes[i].enabled = enabled;
	    return PJ_SUCCESS;
	}
    }

    /* Mode not found in modes table, this may be a request to enable
     * a non-standard G722.1 mode.
     */

    /* Non-standard mode need to be initialized first before user 
     * can disable it.
     */
    if (!enabled)
	{
		PJ_LOG(4, ("g7221.c", "pjmedia_codec_g7221_set_mode() mod not found."));
		return PJ_ENOTFOUND;
	}

    /* Initialize a non-standard mode, look for available space. */
    for (i = codec_factory.mode_rsv_start; 
	 i < codec_factory.mode_count; ++i) 
    {
	if (!codec_factory.modes[i].enabled)
	{
	    codec_mode *mode = &codec_factory.modes[i];
	    mode->enabled = PJ_TRUE;
	    mode->sample_rate = sample_rate;
	    mode->bitrate = bitrate;
	    pj_utoa(mode->bitrate, mode->bitrate_str);

	    return PJ_SUCCESS;
	}
    }
    
    /* No space for non-standard mode. */
    return PJ_ETOOMANY;
}


/*
 * Set level adjustment.
 */
PJ_DEF(pj_status_t)  pjmedia_codec_g7221_set_pcm_shift(int val)
{
    codec_factory.pcm_shift = val;
    return PJ_SUCCESS;
}

/*
 * Unregister G722.1 codec factory from pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_g7221_deinit(void)
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (codec_factory.pool == NULL) {
	/* Already deinitialized */
	return PJ_SUCCESS;
    }

    pj_mutex_lock(codec_factory.mutex);

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(codec_factory.endpt);
    if (!codec_mgr) {
	pj_pool_release(codec_factory.pool);
	codec_factory.pool = NULL;
	return PJ_EINVALIDOP;
    }

    /* Unregister G722.1 codec factory. */
    status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
						  &codec_factory.base);
    
    /* Destroy mutex. */
    pj_mutex_destroy(codec_factory.mutex);

    /* Destroy pool. */
    pj_pool_release(codec_factory.pool);
    codec_factory.pool = NULL;

    return status;
}

/* 
 * Check if factory can allocate the specified codec. 
 */
static pj_status_t test_alloc( pjmedia_codec_factory *factory, 
			       const pjmedia_codec_info *info )
{
    PJ_UNUSED_ARG(factory);

    /* Type MUST be audio. */
    if (info->type != PJMEDIA_TYPE_AUDIO)
	return PJMEDIA_CODEC_EUNSUP;

    /* Check encoding name. */
    if (pj_stricmp2(&info->encoding_name, CODEC_TAG) != 0)
	return PJMEDIA_CODEC_EUNSUP;

    /* Check clock-rate */
    if (info->clock_rate != WB_SAMPLE_RATE && 
	info->clock_rate != UWB_SAMPLE_RATE)
    {
	return PJMEDIA_CODEC_EUNSUP;
    }

    return PJ_SUCCESS;
}

/*
 * Generate default attribute.
 */
static pj_status_t default_attr ( pjmedia_codec_factory *factory, 
				  const pjmedia_codec_info *id, 
				  pjmedia_codec_param *attr )
{
    codec_mode *mode;

    PJ_ASSERT_RETURN(factory==&codec_factory.base, PJ_EINVAL);

    pj_bzero(attr, sizeof(pjmedia_codec_param));

    mode = lookup_mode(id->pt);
    if (mode == NULL || !mode->enabled)
	return PJMEDIA_CODEC_EUNSUP;

    attr->info.pt = (pj_uint8_t)id->pt;
    attr->info.channel_cnt = 1;
    attr->info.clock_rate = mode->sample_rate;
    attr->info.max_bps = mode->bitrate;
    attr->info.avg_bps = mode->bitrate;
    attr->info.pcm_bits_per_sample = 16;
    attr->info.frm_ptime = 20;

    /* Default flags. */
    attr->setting.plc = 1;
    attr->setting.vad = 0;
    attr->setting.frm_per_pkt = 1;

    /* Default FMTP setting */
    attr->setting.dec_fmtp.cnt = 1;
    attr->setting.dec_fmtp.param[0].name = pj_str("bitrate");
    attr->setting.dec_fmtp.param[0].val = pj_str(mode->bitrate_str);

    return PJ_SUCCESS;
}

/*
 * Enum codecs supported by this factory.
 */
static pj_status_t enum_codecs( pjmedia_codec_factory *factory, 
				unsigned *count, 
				pjmedia_codec_info codecs[])
{
    unsigned i, max_cnt;

    PJ_ASSERT_RETURN(factory==&codec_factory.base, PJ_EINVAL);
    PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);

    max_cnt = *count;
    *count = 0;
    
    for (i=0; (i < codec_factory.mode_count) && (*count < max_cnt); ++i)
    {
	if (!codec_factory.modes[i].enabled)
	    continue;

	pj_bzero(&codecs[*count], sizeof(pjmedia_codec_info));
	codecs[*count].encoding_name = pj_str((char*)CODEC_TAG);
	codecs[*count].pt = codec_factory.modes[i].pt;
	codecs[*count].type = PJMEDIA_TYPE_AUDIO;
	codecs[*count].clock_rate = codec_factory.modes[i].sample_rate;
	codecs[*count].channel_cnt = 1;

	++ *count;
    }

    return PJ_SUCCESS;
}

/*
 * Allocate a new codec instance.
 */
static pj_status_t alloc_codec( pjmedia_codec_factory *factory, 
				const pjmedia_codec_info *id,
				pjmedia_codec **p_codec)
{
    codec_private_t *codec_data;
    pjmedia_codec *codec;
    pj_pool_t *pool;
    pj_status_t status;

    PJ_ASSERT_RETURN(factory && id && p_codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &codec_factory.base, PJ_EINVAL);

    pj_mutex_lock(codec_factory.mutex);

    /* Create pool for codec instance */
    pool = pjmedia_endpt_create_pool(codec_factory.endpt, "G7221", 512, 512);
    codec = PJ_POOL_ZALLOC_T(pool, pjmedia_codec);
    codec->op = &codec_op;
    codec->factory = factory;
    codec->codec_data = PJ_POOL_ZALLOC_T(pool, codec_private_t);
    codec_data = (codec_private_t*) codec->codec_data;
    codec_data->pool = pool;

    /* Create silence detector */
    status = pjmedia_silence_det_create(pool, id->clock_rate, 
					id->clock_rate * 20 / 1000,
					&codec_data->vad);
    if (status != PJ_SUCCESS) {
	pj_mutex_unlock(codec_factory.mutex);
	return status;
    }

    pj_mutex_unlock(codec_factory.mutex);

    *p_codec = codec;
    return PJ_SUCCESS;
}

/*
 * Free codec.
 */
static pj_status_t dealloc_codec( pjmedia_codec_factory *factory, 
				  pjmedia_codec *codec )
{
    codec_private_t *codec_data;
    pj_pool_t *pool;

    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &codec_factory.base, PJ_EINVAL);

    /* Close codec, if it's not closed. */
    codec_data = (codec_private_t*) codec->codec_data;
    pool = codec_data->pool;
    codec_close(codec);

    /* Release codec pool */
    pj_pool_release(pool);

    return PJ_SUCCESS;
}

/*
 * Init codec.
 */
static pj_status_t codec_init( pjmedia_codec *codec, 
			       pj_pool_t *pool )
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}

/*
 * Open codec.
 */
static pj_status_t codec_open( pjmedia_codec *codec, 
			       pjmedia_codec_param *attr )
{
    codec_private_t *codec_data = (codec_private_t*) codec->codec_data;
    pj_pool_t *pool;
    unsigned tmp;

    /* Validation mode first! */
    if (!validate_mode(attr->info.clock_rate, attr->info.avg_bps))
	return PJMEDIA_CODEC_EINMODE;

    pool = codec_data->pool;

    /* Initialize common state */
    codec_data->vad_enabled = (attr->setting.vad != 0);
    codec_data->plc_enabled = (attr->setting.plc != 0);

    codec_data->bitrate = (pj_uint16_t)attr->info.avg_bps;
    codec_data->frame_size_bits = (pj_uint16_t)(attr->info.avg_bps*20/1000);
    codec_data->frame_size = (pj_uint16_t)(codec_data->frame_size_bits>>3);
    codec_data->samples_per_frame = (pj_uint16_t)
				    (attr->info.clock_rate*20/1000);
    codec_data->number_of_regions = (pj_uint16_t)
				    (attr->info.clock_rate <= WB_SAMPLE_RATE?
				     NUMBER_OF_REGIONS:MAX_NUMBER_OF_REGIONS);
    codec_data->pcm_shift = codec_factory.pcm_shift;

    /* Initialize encoder state */
    tmp = codec_data->samples_per_frame << 1;
    codec_data->enc_old_frame = (Word16*)pj_pool_zalloc(pool, tmp);
    codec_data->enc_frame = (Word16*)pj_pool_alloc(pool, tmp);

    /* Initialize decoder state */
    tmp = codec_data->samples_per_frame;
    codec_data->dec_old_frame = (Word16*)pj_pool_zalloc(pool, tmp);

    tmp = codec_data->samples_per_frame << 1;
    codec_data->dec_old_mlt_coefs = (Word16*)pj_pool_zalloc(pool, tmp);

    codec_data->dec_randobj.seed0 = 1;
    codec_data->dec_randobj.seed1 = 1;
    codec_data->dec_randobj.seed2 = 1;
    codec_data->dec_randobj.seed3 = 1;

    return PJ_SUCCESS;
}

/*
 * Close codec.
 */
static pj_status_t codec_close( pjmedia_codec *codec )
{
    PJ_UNUSED_ARG(codec);

    return PJ_SUCCESS;
}


/*
 * Modify codec settings.
 */
static pj_status_t codec_modify( pjmedia_codec *codec, 
				 const pjmedia_codec_param *attr )
{
    codec_private_t *codec_data = (codec_private_t*) codec->codec_data;

    codec_data->vad_enabled = (attr->setting.vad != 0);
    codec_data->plc_enabled = (attr->setting.plc != 0);

    return PJ_SUCCESS;
}

/*
 * Get frames in the packet.
 */
static pj_status_t codec_parse( pjmedia_codec *codec,
				void *pkt,
				pj_size_t pkt_size,
				const pj_timestamp *ts,
				unsigned *frame_cnt,
				pjmedia_frame frames[])
{
    codec_private_t *codec_data = (codec_private_t*) codec->codec_data;
    unsigned count = 0;

    PJ_ASSERT_RETURN(frame_cnt, PJ_EINVAL);

    /* Parse based on fixed frame size. */
    while (pkt_size >= codec_data->frame_size && count < *frame_cnt) {
	frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
	frames[count].buf = pkt;
	frames[count].size = codec_data->frame_size;
	frames[count].timestamp.u64 = ts->u64 + 
				      count * codec_data->samples_per_frame;

	pkt = (pj_uint8_t*)pkt + codec_data->frame_size;
	pkt_size -= codec_data->frame_size;

	++count;
    }

    pj_assert(pkt_size == 0);
    *frame_cnt = count;

    return PJ_SUCCESS;
}

/*
 * Encode frames.
 */
static pj_status_t codec_encode( pjmedia_codec *codec, 
				 const struct pjmedia_frame *input,
				 unsigned output_buf_len, 
				 struct pjmedia_frame *output)
{
    codec_private_t *codec_data = (codec_private_t*) codec->codec_data;
    unsigned nsamples, processed;

    /* Check frame in & out size */
    nsamples = input->size >> 1;
    PJ_ASSERT_RETURN(nsamples % codec_data->samples_per_frame == 0, 
		     PJMEDIA_CODEC_EPCMFRMINLEN);
    PJ_ASSERT_RETURN(output_buf_len >= codec_data->frame_size * nsamples /
		     codec_data->samples_per_frame,
		     PJMEDIA_CODEC_EFRMTOOSHORT);

    /* Apply silence detection if VAD is enabled */
    if (codec_data->vad_enabled) {
	pj_bool_t is_silence;
	pj_int32_t silence_duration;

	pj_assert(codec_data->vad);

	silence_duration = pj_timestamp_diff32(&codec_data->last_tx, 
					       &input->timestamp);

	is_silence = pjmedia_silence_det_detect(codec_data->vad, 
					        (const pj_int16_t*) input->buf,
						(input->size >> 1),
						NULL);
	if (is_silence &&
	    (PJMEDIA_CODEC_MAX_SILENCE_PERIOD == -1 ||
	     silence_duration < (PJMEDIA_CODEC_MAX_SILENCE_PERIOD *
			         (int)codec_data->samples_per_frame / 20)))
	{
	    output->type = PJMEDIA_FRAME_TYPE_NONE;
	    output->buf = NULL;
	    output->size = 0;
	    output->timestamp = input->timestamp;
	    return PJ_SUCCESS;
	} else {
	    codec_data->last_tx = input->timestamp;
	}
    }

    processed = 0;
    output->size = 0;
    while (processed < nsamples) {
	Word16 mlt_coefs[MAX_SAMPLES_PER_FRAME];
	Word16 mag_shift;
	const Word16 *pcm_input;
	pj_int8_t *out_bits;
	
	pcm_input = (const Word16*)input->buf + processed;
	out_bits = (pj_int8_t*)output->buf + output->size;

	/* Encoder adjust the input signal level */
	if (codec_data->pcm_shift) {
	    unsigned i;
	    for (i=0; i<codec_data->samples_per_frame; ++i) {
		codec_data->enc_frame[i] = 
			(Word16)(pcm_input[i] >> codec_data->pcm_shift);
	    }
	    pcm_input = codec_data->enc_frame;
	}

	/* Convert input samples to rmlt coefs */
	mag_shift = samples_to_rmlt_coefs(pcm_input,
					  codec_data->enc_old_frame, 
					  mlt_coefs, 
					  codec_data->samples_per_frame);

	/* Encode the mlt coefs. Note that encoder output stream is
	 * 16 bit array, so we need to take care about endianness.
	 */
	encoder(codec_data->frame_size_bits,
		codec_data->number_of_regions,
		mlt_coefs,
		mag_shift,
		(Word16*)out_bits);

	/* Encoder output are in native host byte order, while ITU says
	 * it must be in network byte order (MSB first).
	 */
	swap_bytes((pj_uint16_t*)out_bits, codec_data->frame_size/2);

	processed += codec_data->samples_per_frame;
	output->size += codec_data->frame_size;
    }

    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;

    return PJ_SUCCESS;
}

/*
 * Decode frame.
 */
static pj_status_t codec_decode( pjmedia_codec *codec, 
				 const struct pjmedia_frame *input,
				 unsigned output_buf_len, 
				 struct pjmedia_frame *output)
{
    codec_private_t *codec_data = (codec_private_t*) codec->codec_data;
    Word16 mlt_coefs[MAX_SAMPLES_PER_FRAME];
    Word16 mag_shift;
    Bit_Obj bitobj;
    Word16 frame_error_flag = 0;

    /* Check frame out length size */
    PJ_ASSERT_RETURN(output_buf_len >= 
		    (unsigned)(codec_data->samples_per_frame<<1),
		     PJMEDIA_CODEC_EPCMTOOSHORT);

    /* If input is NULL, perform PLC by settting frame_error_flag to 1 */
    if (input) {
	/* Check frame in length size */
	PJ_ASSERT_RETURN((pj_uint16_t)input->size == codec_data->frame_size,
			 PJMEDIA_CODEC_EFRMINLEN);

	/* Decoder requires input of 16-bits array in native host byte
	 * order, while the frame received from the network are in
	 * network byte order (MSB first).
	 */
	swap_bytes((pj_uint16_t*)input->buf, codec_data->frame_size/2);

	bitobj.code_word_ptr = (Word16*)input->buf;
	bitobj.current_word =  *bitobj.code_word_ptr;
	bitobj.code_bit_count = 0;
	bitobj.number_of_bits_left = codec_data->frame_size_bits;

	output->timestamp = input->timestamp;
    } else {
	pj_bzero(&bitobj, sizeof(bitobj));
	frame_error_flag = 1;
    }

    /* Process the input frame to get mlt coefs */
    decoder(&bitobj,
	    &codec_data->dec_randobj,
            codec_data->number_of_regions,
	    mlt_coefs,
            &mag_shift,
	    &codec_data->dec_old_mag_shift,
            codec_data->dec_old_mlt_coefs,
            frame_error_flag);

    /* Convert the mlt_coefs to PCM samples */
    rmlt_coefs_to_samples(mlt_coefs, 
			  codec_data->dec_old_frame, 
			  (Word16*)output->buf, 
			  codec_data->samples_per_frame, 
			  mag_shift);

    /* Decoder adjust PCM signal */
    if (codec_data->pcm_shift) {
	unsigned i;
	pj_int16_t *buf = (Word16*)output->buf;

	for (i=0; i<codec_data->samples_per_frame; ++i) {
	    buf[i] <<= codec_data->pcm_shift;
	}
    }

    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->size = codec_data->samples_per_frame << 1;

    return PJ_SUCCESS;
}

/* 
 * Recover lost frame.
 */
static pj_status_t codec_recover( pjmedia_codec *codec, 
				  unsigned output_buf_len, 
				  struct pjmedia_frame *output)
{
    codec_private_t *codec_data = (codec_private_t*) codec->codec_data;

    /* Use native PLC when PLC is enabled. */
    if (codec_data->plc_enabled)
	return codec_decode(codec, NULL, output_buf_len, output);

    /* Otherwise just return zero-fill frame. */
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->size = codec_data->samples_per_frame << 1;

    pjmedia_zero_samples((pj_int16_t*)output->buf, 
			 codec_data->samples_per_frame);

    return PJ_SUCCESS;
}

#endif	/* PJMEDIA_HAS_G7221_CODEC */
