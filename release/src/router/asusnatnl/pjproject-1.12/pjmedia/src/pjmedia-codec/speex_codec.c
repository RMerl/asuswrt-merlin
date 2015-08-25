/* $Id: speex_codec.c 3553 2011-05-05 06:14:19Z nanang $ */
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

#include <pjmedia-codec/speex.h>
#include <pjmedia/codec.h>
#include <pjmedia/errno.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/port.h>
#include <speex/speex.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/os.h>

/*
 * Only build this file if PJMEDIA_HAS_SPEEX_CODEC != 0
 */
#if defined(PJMEDIA_HAS_SPEEX_CODEC) && PJMEDIA_HAS_SPEEX_CODEC!=0


#define THIS_FILE   "speex_codec.c"

/* Prototypes for Speex factory */
static pj_status_t spx_test_alloc( pjmedia_codec_factory *factory, 
				   const pjmedia_codec_info *id );
static pj_status_t spx_default_attr( pjmedia_codec_factory *factory, 
				     const pjmedia_codec_info *id, 
				     pjmedia_codec_param *attr );
static pj_status_t spx_enum_codecs( pjmedia_codec_factory *factory, 
				    unsigned *count, 
				    pjmedia_codec_info codecs[]);
static pj_status_t spx_alloc_codec( pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id, 
				    pjmedia_codec **p_codec);
static pj_status_t spx_dealloc_codec( pjmedia_codec_factory *factory, 
				      pjmedia_codec *codec );

/* Prototypes for Speex implementation. */
static pj_status_t  spx_codec_init( pjmedia_codec *codec, 
				    pj_pool_t *pool );
static pj_status_t  spx_codec_open( pjmedia_codec *codec, 
				    pjmedia_codec_param *attr );
static pj_status_t  spx_codec_close( pjmedia_codec *codec );
static pj_status_t  spx_codec_modify(pjmedia_codec *codec, 
				     const pjmedia_codec_param *attr );
static pj_status_t  spx_codec_parse( pjmedia_codec *codec,
				     void *pkt,
				     pj_size_t pkt_size,
				     const pj_timestamp *ts,
				     unsigned *frame_cnt,
				     pjmedia_frame frames[]);
static pj_status_t  spx_codec_encode( pjmedia_codec *codec, 
				      const struct pjmedia_frame *input,
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);
static pj_status_t  spx_codec_decode( pjmedia_codec *codec, 
				      const struct pjmedia_frame *input,
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);
static pj_status_t  spx_codec_recover(pjmedia_codec *codec, 
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);

/* Definition for Speex codec operations. */
static pjmedia_codec_op spx_op = 
{
    &spx_codec_init,
    &spx_codec_open,
    &spx_codec_close,
    &spx_codec_modify,
    &spx_codec_parse,
    &spx_codec_encode,
    &spx_codec_decode,
    &spx_codec_recover
};

/* Definition for Speex codec factory operations. */
static pjmedia_codec_factory_op spx_factory_op =
{
    &spx_test_alloc,
    &spx_default_attr,
    &spx_enum_codecs,
    &spx_alloc_codec,
    &spx_dealloc_codec
};

/* Index to Speex parameter. */
enum
{
    PARAM_NB,	/* Index for narrowband parameter.	*/
    PARAM_WB,	/* Index for wideband parameter.	*/
    PARAM_UWB,	/* Index for ultra-wideband parameter	*/
};

/* Speex default parameter */
struct speex_param
{
    int		     enabled;		/* Is this mode enabled?	    */
    const SpeexMode *mode;		/* Speex mode.			    */
    int		     pt;		/* Payload type.		    */
    unsigned	     clock_rate;	/* Default sampling rate to be used.*/
    int		     quality;		/* Default encoder quality.	    */
    int		     complexity;	/* Default encoder complexity.	    */
    int		     samples_per_frame;	/* Samples per frame.		    */
    int		     framesize;		/* Frame size for current mode.	    */
    int		     bitrate;		/* Bit rate for current mode.	    */
    int		     max_bitrate;	/* Max bit rate for current mode.   */
};

/* Speex factory */
static struct spx_factory
{
    pjmedia_codec_factory    base;
    pjmedia_endpt	    *endpt;
    pj_pool_t		    *pool;
    pj_mutex_t		    *mutex;
    pjmedia_codec	     codec_list;
    struct speex_param	     speex_param[3];

} spx_factory;

/* Speex codec private data. */
struct spx_private
{
    int			 param_id;	    /**< Index to speex param.	*/

    void		*enc;		    /**< Encoder state.		*/
    SpeexBits		 enc_bits;	    /**< Encoder bits.		*/
    void		*dec;		    /**< Decoder state.		*/
    SpeexBits		 dec_bits;	    /**< Decoder bits.		*/
};


/*
 * Get codec bitrate and frame size.
 */
static pj_status_t get_speex_info( struct speex_param *p )
{
    void *state;
    int tmp;

    /* Create temporary encoder */
    state = speex_encoder_init(p->mode);
    if (!state)
	return PJMEDIA_CODEC_EFAILED;

    /* Set the quality */
    if (p->quality != -1)
	speex_encoder_ctl(state, SPEEX_SET_QUALITY, &p->quality);

    /* Sampling rate. */
    speex_encoder_ctl(state, SPEEX_SET_SAMPLING_RATE, &p->clock_rate);

    /* VAD off to have max bitrate */
    tmp = 0;
    speex_encoder_ctl(state, SPEEX_SET_VAD, &tmp);

    /* Complexity. */
    if (p->complexity != -1)
	speex_encoder_ctl(state, SPEEX_SET_COMPLEXITY, &p->complexity);

    /* Now get the frame size */
    speex_encoder_ctl(state, SPEEX_GET_FRAME_SIZE, &p->samples_per_frame);

    /* Now get the average bitrate */
    speex_encoder_ctl(state, SPEEX_GET_BITRATE, &p->bitrate);

    /* Calculate framesize. */
    p->framesize = p->bitrate * 20 / 1000;

    /* Now get the maximum bitrate by using maximum quality (=10) */
    tmp = 10;
    speex_encoder_ctl(state, SPEEX_SET_QUALITY, &tmp);
    speex_encoder_ctl(state, SPEEX_GET_BITRATE, &p->max_bitrate);

    /* Destroy encoder. */
    speex_encoder_destroy(state);

    return PJ_SUCCESS;
}

/*
 * Initialize and register Speex codec factory to pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_speex_init( pjmedia_endpt *endpt,
					      unsigned options,
					      int quality,
					      int complexity )
{
    pjmedia_codec_mgr *codec_mgr;
    unsigned i;
    pj_status_t status;

    if (spx_factory.pool != NULL) {
	/* Already initialized. */
	return PJ_SUCCESS;
    }

    /* Get defaults */
    if (quality < 0) quality = PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY;
    if (complexity < 0) complexity = PJMEDIA_CODEC_SPEEX_DEFAULT_COMPLEXITY;

    /* Validate quality & complexity */
    PJ_ASSERT_RETURN(quality >= 0 && quality <= 10, PJ_EINVAL);
    PJ_ASSERT_RETURN(complexity >= 1 && complexity <= 10, PJ_EINVAL);

    /* Create Speex codec factory. */
    spx_factory.base.op = &spx_factory_op;
    spx_factory.base.factory_data = NULL;
    spx_factory.endpt = endpt;

    spx_factory.pool = pjmedia_endpt_create_pool(endpt, "speex", 
						       4000, 4000);
    if (!spx_factory.pool)
	return PJ_ENOMEM;

    pj_list_init(&spx_factory.codec_list);

    /* Create mutex. */
    status = pj_mutex_create_simple(spx_factory.pool, "speex", 
				    &spx_factory.mutex);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Initialize default Speex parameter. */
    spx_factory.speex_param[PARAM_NB].enabled = 
	((options & PJMEDIA_SPEEX_NO_NB) == 0);
    spx_factory.speex_param[PARAM_NB].pt = PJMEDIA_RTP_PT_SPEEX_NB;
    spx_factory.speex_param[PARAM_NB].mode = speex_lib_get_mode(SPEEX_MODEID_NB);
    spx_factory.speex_param[PARAM_NB].clock_rate = 8000;
    spx_factory.speex_param[PARAM_NB].quality = quality;
    spx_factory.speex_param[PARAM_NB].complexity = complexity;

    spx_factory.speex_param[PARAM_WB].enabled = 
	((options & PJMEDIA_SPEEX_NO_WB) == 0);
    spx_factory.speex_param[PARAM_WB].pt = PJMEDIA_RTP_PT_SPEEX_WB;
    spx_factory.speex_param[PARAM_WB].mode = speex_lib_get_mode(SPEEX_MODEID_WB);
    spx_factory.speex_param[PARAM_WB].clock_rate = 16000;
    spx_factory.speex_param[PARAM_WB].quality = quality;
    spx_factory.speex_param[PARAM_WB].complexity = complexity;

    spx_factory.speex_param[PARAM_UWB].enabled = 
	((options & PJMEDIA_SPEEX_NO_UWB) == 0);
    spx_factory.speex_param[PARAM_UWB].pt = PJMEDIA_RTP_PT_SPEEX_UWB;
    spx_factory.speex_param[PARAM_UWB].mode = speex_lib_get_mode(SPEEX_MODEID_UWB);
    spx_factory.speex_param[PARAM_UWB].clock_rate = 32000;
    spx_factory.speex_param[PARAM_UWB].quality = quality;
    spx_factory.speex_param[PARAM_UWB].complexity = complexity;

    /* Somehow quality <=4 is broken in linux. */
    if (quality <= 4 && quality >= 0) {
	PJ_LOG(5,(THIS_FILE, "Adjusting quality to 5 for uwb"));
	spx_factory.speex_param[PARAM_UWB].quality = 5;
    }

    /* Get codec framesize and avg bitrate for each mode. */
    for (i=0; i<PJ_ARRAY_SIZE(spx_factory.speex_param); ++i) {
	status = get_speex_info(&spx_factory.speex_param[i]);
    }

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
    if (!codec_mgr) {
	status = PJ_EINVALIDOP;
	goto on_error;
    }

    /* Register codec factory to endpoint. */
    status = pjmedia_codec_mgr_register_factory(codec_mgr, 
						&spx_factory.base);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Done. */
    return PJ_SUCCESS;

on_error:
    pj_pool_release(spx_factory.pool);
    spx_factory.pool = NULL;
    return status;
}


/*
 * Initialize with default settings.
 */
PJ_DEF(pj_status_t) pjmedia_codec_speex_init_default(pjmedia_endpt *endpt)
{
    return pjmedia_codec_speex_init(endpt, 0, -1, -1);
}

/*
 * Change the settings of Speex codec.
 */
PJ_DEF(pj_status_t) pjmedia_codec_speex_set_param(unsigned clock_rate,
						  int quality,
						  int complexity)
{
    unsigned i;

    /* Get defaults */
    if (quality < 0) quality = PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY;
    if (complexity < 0) complexity = PJMEDIA_CODEC_SPEEX_DEFAULT_COMPLEXITY;

    /* Validate quality & complexity */
    PJ_ASSERT_RETURN(quality >= 0 && quality <= 10, PJ_EINVAL);
    PJ_ASSERT_RETURN(complexity >= 1 && complexity <= 10, PJ_EINVAL);

    /* Apply the settings */
    for (i=0; i<PJ_ARRAY_SIZE(spx_factory.speex_param); ++i) {
	if (spx_factory.speex_param[i].clock_rate == clock_rate) {
	    pj_status_t status;

	    spx_factory.speex_param[i].quality = quality;
	    spx_factory.speex_param[i].complexity = complexity;

	    /* Somehow quality<=4 is broken in linux. */
	    if (i == PARAM_UWB && quality <= 4 && quality >= 0) {
		PJ_LOG(5,(THIS_FILE, "Adjusting quality to 5 for uwb"));
		spx_factory.speex_param[PARAM_UWB].quality = 5;
	    }

	    status = get_speex_info(&spx_factory.speex_param[i]);

	    return status;
	}
    }

    return PJ_EINVAL;
}

/*
 * Unregister Speex codec factory from pjmedia endpoint and deinitialize
 * the Speex codec library.
 */
PJ_DEF(pj_status_t) pjmedia_codec_speex_deinit(void)
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (spx_factory.pool == NULL) {
	/* Already deinitialized */
	return PJ_SUCCESS;
    }

    pj_mutex_lock(spx_factory.mutex);

    /* We don't want to deinit if there's outstanding codec. */
    /* This is silly, as we'll always have codec in the list if
       we ever allocate a codec! A better behavior maybe is to 
       deallocate all codecs in the list.
    if (!pj_list_empty(&spx_factory.codec_list)) {
	pj_mutex_unlock(spx_factory.mutex);
	return PJ_EBUSY;
    }
    */

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(spx_factory.endpt);
    if (!codec_mgr) {
	pj_pool_release(spx_factory.pool);
	spx_factory.pool = NULL;
	return PJ_EINVALIDOP;
    }

    /* Unregister Speex codec factory. */
    status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
						  &spx_factory.base);
    
    /* Destroy mutex. */
    pj_mutex_destroy(spx_factory.mutex);

    /* Destroy pool. */
    pj_pool_release(spx_factory.pool);
    spx_factory.pool = NULL;

    return status;
}

/* 
 * Check if factory can allocate the specified codec. 
 */
static pj_status_t spx_test_alloc( pjmedia_codec_factory *factory, 
				   const pjmedia_codec_info *info )
{
    const pj_str_t speex_tag = { "speex", 5};
    unsigned i;

    PJ_UNUSED_ARG(factory);

    /* Type MUST be audio. */
    if (info->type != PJMEDIA_TYPE_AUDIO)
	return PJMEDIA_CODEC_EUNSUP;

    /* Check encoding name. */
    if (pj_stricmp(&info->encoding_name, &speex_tag) != 0)
	return PJMEDIA_CODEC_EUNSUP;

    /* Check clock-rate */
    for (i=0; i<PJ_ARRAY_SIZE(spx_factory.speex_param); ++i) {
	if (info->clock_rate == spx_factory.speex_param[i].clock_rate) {
	    /* Okay, let's Speex! */
	    return PJ_SUCCESS;
	}
    }

    
    /* Unsupported, or mode is disabled. */
    return PJMEDIA_CODEC_EUNSUP;
}

/*
 * Generate default attribute.
 */
static pj_status_t spx_default_attr (pjmedia_codec_factory *factory, 
				      const pjmedia_codec_info *id, 
				      pjmedia_codec_param *attr )
{

    PJ_ASSERT_RETURN(factory==&spx_factory.base, PJ_EINVAL);

    pj_bzero(attr, sizeof(pjmedia_codec_param));
    attr->info.pt = (pj_uint8_t)id->pt;
    attr->info.channel_cnt = 1;

    if (id->clock_rate <= 8000) {
	attr->info.clock_rate = spx_factory.speex_param[PARAM_NB].clock_rate;
	attr->info.avg_bps = spx_factory.speex_param[PARAM_NB].bitrate;
	attr->info.max_bps = spx_factory.speex_param[PARAM_NB].max_bitrate;

    } else if (id->clock_rate <= 16000) {
	attr->info.clock_rate = spx_factory.speex_param[PARAM_WB].clock_rate;
	attr->info.avg_bps = spx_factory.speex_param[PARAM_WB].bitrate;
	attr->info.max_bps = spx_factory.speex_param[PARAM_WB].max_bitrate;

    } else {
	/* Wow.. somebody is doing ultra-wideband. Cool...! */
	attr->info.clock_rate = spx_factory.speex_param[PARAM_UWB].clock_rate;
	attr->info.avg_bps = spx_factory.speex_param[PARAM_UWB].bitrate;
	attr->info.max_bps = spx_factory.speex_param[PARAM_UWB].max_bitrate;
    }

    attr->info.pcm_bits_per_sample = 16;
    attr->info.frm_ptime = 20;
    attr->info.pt = (pj_uint8_t)id->pt;

    attr->setting.frm_per_pkt = 1;

    /* Default flags. */
    attr->setting.cng = 1;
    attr->setting.plc = 1;
    attr->setting.penh =1 ;
    attr->setting.vad = 1;

    return PJ_SUCCESS;
}

/*
 * Enum codecs supported by this factory (i.e. only Speex!).
 */
static pj_status_t spx_enum_codecs(pjmedia_codec_factory *factory, 
				    unsigned *count, 
				    pjmedia_codec_info codecs[])
{
    unsigned max;
    int i;  /* Must be signed */

    PJ_UNUSED_ARG(factory);
    PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);

    max = *count;
    *count = 0;

    /*
     * We return three codecs here, and in this order:
     *	- ultra-wideband, wideband, and narrowband.
     */
    for (i=PJ_ARRAY_SIZE(spx_factory.speex_param)-1; i>=0 && *count<max; --i) {

	if (!spx_factory.speex_param[i].enabled)
	    continue;

	pj_bzero(&codecs[*count], sizeof(pjmedia_codec_info));
	codecs[*count].encoding_name = pj_str("speex");
	codecs[*count].pt = spx_factory.speex_param[i].pt;
	codecs[*count].type = PJMEDIA_TYPE_AUDIO;
	codecs[*count].clock_rate = spx_factory.speex_param[i].clock_rate;
	codecs[*count].channel_cnt = 1;

	++*count;
    }

    return PJ_SUCCESS;
}

/*
 * Allocate a new Speex codec instance.
 */
static pj_status_t spx_alloc_codec( pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id,
				    pjmedia_codec **p_codec)
{
    pjmedia_codec *codec;
    struct spx_private *spx;

    PJ_ASSERT_RETURN(factory && id && p_codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &spx_factory.base, PJ_EINVAL);


    pj_mutex_lock(spx_factory.mutex);

    /* Get free nodes, if any. */
    if (!pj_list_empty(&spx_factory.codec_list)) {
	codec = spx_factory.codec_list.next;
	pj_list_erase(codec);
    } else {
	codec = PJ_POOL_ZALLOC_T(spx_factory.pool, pjmedia_codec);
	PJ_ASSERT_RETURN(codec != NULL, PJ_ENOMEM);
	codec->op = &spx_op;
	codec->factory = factory;
	codec->codec_data = pj_pool_alloc(spx_factory.pool,
					  sizeof(struct spx_private));
    }

    pj_mutex_unlock(spx_factory.mutex);

    spx = (struct spx_private*) codec->codec_data;
    spx->enc = NULL;
    spx->dec = NULL;

    if (id->clock_rate <= 8000)
	spx->param_id = PARAM_NB;
    else if (id->clock_rate <= 16000)
	spx->param_id = PARAM_WB;
    else
	spx->param_id = PARAM_UWB;

    *p_codec = codec;
    return PJ_SUCCESS;
}

/*
 * Free codec.
 */
static pj_status_t spx_dealloc_codec( pjmedia_codec_factory *factory, 
				      pjmedia_codec *codec )
{
    struct spx_private *spx;

    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &spx_factory.base, PJ_EINVAL);

    /* Close codec, if it's not closed. */
    spx = (struct spx_private*) codec->codec_data;
    if (spx->enc != NULL || spx->dec != NULL) {
	spx_codec_close(codec);
    }

    /* Put in the free list. */
    pj_mutex_lock(spx_factory.mutex);
    pj_list_push_front(&spx_factory.codec_list, codec);
    pj_mutex_unlock(spx_factory.mutex);

    return PJ_SUCCESS;
}

/*
 * Init codec.
 */
static pj_status_t spx_codec_init( pjmedia_codec *codec, 
				   pj_pool_t *pool )
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}

/*
 * Open codec.
 */
static pj_status_t spx_codec_open( pjmedia_codec *codec, 
				   pjmedia_codec_param *attr )
{
    struct spx_private *spx;
    int id, tmp;

    spx = (struct spx_private*) codec->codec_data;
    id = spx->param_id;

    /* 
     * Create and initialize encoder. 
     */
    spx->enc = speex_encoder_init(spx_factory.speex_param[id].mode);
    if (!spx->enc)
	return PJMEDIA_CODEC_EFAILED;
    speex_bits_init(&spx->enc_bits);

    /* Set the quality*/
    if (spx_factory.speex_param[id].quality != -1) {
	speex_encoder_ctl(spx->enc, SPEEX_SET_QUALITY, 
			  &spx_factory.speex_param[id].quality);
    }

    /* Sampling rate. */
    tmp = attr->info.clock_rate;
    speex_encoder_ctl(spx->enc, SPEEX_SET_SAMPLING_RATE, 
		      &spx_factory.speex_param[id].clock_rate);

    /* VAD */
    tmp = (attr->setting.vad != 0);
    speex_encoder_ctl(spx->enc, SPEEX_SET_VAD, &tmp);
    speex_encoder_ctl(spx->enc, SPEEX_SET_DTX, &tmp);

    /* Complexity */
    if (spx_factory.speex_param[id].complexity != -1) {
	speex_encoder_ctl(spx->enc, SPEEX_SET_COMPLEXITY, 
			  &spx_factory.speex_param[id].complexity);
    }

    /* 
     * Create and initialize decoder. 
     */
    spx->dec = speex_decoder_init(spx_factory.speex_param[id].mode);
    if (!spx->dec) {
	spx_codec_close(codec);
	return PJMEDIA_CODEC_EFAILED;
    }
    speex_bits_init(&spx->dec_bits);

    /* Sampling rate. */
    speex_decoder_ctl(spx->dec, SPEEX_SET_SAMPLING_RATE, 
		      &spx_factory.speex_param[id].clock_rate);

    /* PENH */
    tmp = attr->setting.penh;
    speex_decoder_ctl(spx->dec, SPEEX_SET_ENH, &tmp);

    return PJ_SUCCESS;
}

/*
 * Close codec.
 */
static pj_status_t spx_codec_close( pjmedia_codec *codec )
{
    struct spx_private *spx;

    spx = (struct spx_private*) codec->codec_data;

    /* Destroy encoder*/
    if (spx->enc) {
	speex_encoder_destroy( spx->enc );
	spx->enc = NULL;
	speex_bits_destroy( &spx->enc_bits );
    }

    /* Destroy decoder */
    if (spx->dec) {
	speex_decoder_destroy( spx->dec);
	spx->dec = NULL;
	speex_bits_destroy( &spx->dec_bits );
    }

    return PJ_SUCCESS;
}


/*
 * Modify codec settings.
 */
static pj_status_t  spx_codec_modify(pjmedia_codec *codec, 
				     const pjmedia_codec_param *attr )
{
    struct spx_private *spx;
    int tmp;

    spx = (struct spx_private*) codec->codec_data;

    /* VAD */
    tmp = (attr->setting.vad != 0);
    speex_encoder_ctl(spx->enc, SPEEX_SET_VAD, &tmp);
    speex_encoder_ctl(spx->enc, SPEEX_SET_DTX, &tmp);

    /* PENH */
    tmp = attr->setting.penh;
    speex_decoder_ctl(spx->dec, SPEEX_SET_ENH, &tmp);

    return PJ_SUCCESS;
}

#if 0
#  define TRACE__(args)	    PJ_LOG(5,args)
#else
#  define TRACE__(args)
#endif

#undef THIS_FUNC
#define THIS_FUNC "speex_get_next_frame"

#define NB_SUBMODES 16
#define NB_SUBMODE_BITS 4

#define SB_SUBMODES 8
#define SB_SUBMODE_BITS 3

/* This function will iterate frames & submodes in the Speex bits.
 * Returns 0 if a frame found, otherwise returns -1.
 */
int speex_get_next_frame(SpeexBits *bits)
{
    static const int inband_skip_table[NB_SUBMODES] =
       {1, 1, 4, 4, 4, 4, 4, 4, 8, 8, 16, 16, 32, 32, 64, 64 };
    static const int wb_skip_table[SB_SUBMODES] =
       {SB_SUBMODE_BITS+1, 36, 112, 192, 352, -1, -1, -1};

    unsigned submode;
    unsigned nb_count = 0;

    while (speex_bits_remaining(bits) >= 5) {
	unsigned wb_count = 0;
	unsigned bit_ptr = bits->bitPtr;
	unsigned char_ptr = bits->charPtr;

	/* WB frame */
	while ((speex_bits_remaining(bits) >= 4)
	    && speex_bits_unpack_unsigned(bits, 1))
	{
	    int advance;

	    submode = speex_bits_unpack_unsigned(bits, 3);
	    advance = wb_skip_table[submode];
	    if (advance < 0) {
		TRACE__((THIS_FUNC, "Invalid mode encountered. "
			 "The stream is corrupted."));
		return -1;
	    } 
	    TRACE__((THIS_FUNC, "WB layer skipped: %d bits", advance));
	    advance -= (SB_SUBMODE_BITS+1);
	    speex_bits_advance(bits, advance);

	    bit_ptr = bits->bitPtr;
	    char_ptr = bits->charPtr;

	    /* Consecutive subband frames may not exceed 2 frames */
	    if (++wb_count > 2)
		return -1;
	}

	/* End of bits, return the frame */
	if (speex_bits_remaining(bits) < 4) {
	    TRACE__((THIS_FUNC, "End of stream"));
	    return 0;
	}

	/* Stop iteration, return the frame */
	if (nb_count > 0) {
	    bits->bitPtr = bit_ptr;
	    bits->charPtr = char_ptr;
	    return 0;
	}

	/* Get control bits */
	submode = speex_bits_unpack_unsigned(bits, 4);
	TRACE__((THIS_FUNC, "Control bits: %d at %d", 
		 submode, bits->charPtr*8+bits->bitPtr));

	if (submode == 15) {
	    TRACE__((THIS_FUNC, "Found submode: terminator"));
	    return -1;
	} else if (submode == 14) {
	    /* in-band signal; next 4 bits contain signal id */
	    submode = speex_bits_unpack_unsigned(bits, 4);
	    TRACE__((THIS_FUNC, "Found submode: in-band %d bits", 
		     inband_skip_table[submode]));
	    speex_bits_advance(bits, inband_skip_table[submode]);
	} else if (submode == 13) {
	    /* user in-band; next 5 bits contain msg len */
	    submode = speex_bits_unpack_unsigned(bits, 5);
	    TRACE__((THIS_FUNC, "Found submode: user-band %d bytes", submode));
	    speex_bits_advance(bits, submode * 8);
	} else if (submode > 8) {
	    TRACE__((THIS_FUNC, "Unknown sub-mode %d", submode));
	    return -1;
	} else {
	    /* NB frame */
	    unsigned int advance = submode;
	    speex_mode_query(&speex_nb_mode, SPEEX_SUBMODE_BITS_PER_FRAME, &advance);
	    if (advance < 0) {
		TRACE__((THIS_FUNC, "Invalid mode encountered. "
			 "The stream is corrupted."));
		return -1;
	    }
	    TRACE__((THIS_FUNC, "Submode %d: %d bits", submode, advance));
	    advance -= (NB_SUBMODE_BITS+1);
	    speex_bits_advance(bits, advance);

	    ++nb_count;
	}
    }

    return 0;
}


/*
 * Get frames in the packet.
 */
static pj_status_t  spx_codec_parse( pjmedia_codec *codec,
				     void *pkt,
				     pj_size_t pkt_size,
				     const pj_timestamp *ts,
				     unsigned *frame_cnt,
				     pjmedia_frame frames[])
{
    struct spx_private *spx = (struct spx_private*) codec->codec_data;
    unsigned samples_per_frame;
    unsigned count = 0;
    int char_ptr = 0;
    int bit_ptr = 0;

    samples_per_frame=spx_factory.speex_param[spx->param_id].samples_per_frame;

    /* Copy the data into the speex bit-stream */
    speex_bits_read_from(&spx->dec_bits, (char*)pkt, pkt_size);

    while (speex_get_next_frame(&spx->dec_bits) == 0 && 
	   spx->dec_bits.charPtr != char_ptr)
    {
	frames[count].buf = (char*)pkt + char_ptr;
	/* Bit info contains start bit offset of the frame */
	frames[count].bit_info = bit_ptr;
	frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
	frames[count].timestamp.u64 = ts->u64 + count * samples_per_frame;
	frames[count].size = spx->dec_bits.charPtr - char_ptr;
	if (spx->dec_bits.bitPtr)
	    ++frames[count].size;

	bit_ptr = spx->dec_bits.bitPtr;
	char_ptr = spx->dec_bits.charPtr;

	++count;
    }

    *frame_cnt = count;

    return PJ_SUCCESS;
}

/*
 * Encode frames.
 */
static pj_status_t spx_codec_encode( pjmedia_codec *codec, 
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len, 
				     struct pjmedia_frame *output)
{
    struct spx_private *spx;
    unsigned samples_per_frame;
    int tx = 0;
    spx_int16_t *pcm_in = (spx_int16_t*)input->buf;
    unsigned nsamples;

    spx = (struct spx_private*) codec->codec_data;

    if (input->type != PJMEDIA_FRAME_TYPE_AUDIO) {
	output->size = 0;
	output->buf = NULL;
	output->timestamp = input->timestamp;
	output->type = input->type;
	return PJ_SUCCESS;
    }

    nsamples = input->size >> 1;
    samples_per_frame=spx_factory.speex_param[spx->param_id].samples_per_frame;

    PJ_ASSERT_RETURN(nsamples % samples_per_frame == 0, 
		     PJMEDIA_CODEC_EPCMFRMINLEN);

    /* Flush all the bits in the struct so we can encode a new frame */
    speex_bits_reset(&spx->enc_bits);

    /* Encode the frames */
    while (nsamples >= samples_per_frame) {
	tx += speex_encode_int(spx->enc, pcm_in, &spx->enc_bits);
	pcm_in += samples_per_frame;
	nsamples -= samples_per_frame;
    }

    /* Check if we need not to transmit the frame (DTX) */
    if (tx == 0) {
	output->buf = NULL;
	output->size = 0;
	output->timestamp.u64 = input->timestamp.u64;
	output->type = PJMEDIA_FRAME_TYPE_NONE;
	return PJ_SUCCESS;
    }

    /* Check size. */
    pj_assert(speex_bits_nbytes(&spx->enc_bits) <= (int)output_buf_len);

    /* Copy the bits to an array of char that can be written */
    output->size = speex_bits_write(&spx->enc_bits, 
				    (char*)output->buf, output_buf_len);
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;

    return PJ_SUCCESS;
}

/*
 * Decode frame.
 */
static pj_status_t spx_codec_decode( pjmedia_codec *codec, 
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len, 
				     struct pjmedia_frame *output)
{
    struct spx_private *spx;
    unsigned samples_per_frame;

    spx = (struct spx_private*) codec->codec_data;
    samples_per_frame=spx_factory.speex_param[spx->param_id].samples_per_frame;

    PJ_ASSERT_RETURN(output_buf_len >= samples_per_frame << 1,
		     PJMEDIA_CODEC_EPCMTOOSHORT);

    if (input->type != PJMEDIA_FRAME_TYPE_AUDIO) {
	pjmedia_zero_samples((pj_int16_t*)output->buf, samples_per_frame);
	output->size = samples_per_frame << 1;
	output->timestamp.u64 = input->timestamp.u64;
	output->type = PJMEDIA_FRAME_TYPE_AUDIO;
	return PJ_SUCCESS;
    }

    /* Copy the data into the bit-stream struct */
    speex_bits_read_from(&spx->dec_bits, (char*)input->buf, input->size);
    
    /* Set Speex dec_bits pointer to the start bit of the frame */
    speex_bits_advance(&spx->dec_bits, input->bit_info);

    /* Decode the data */
    speex_decode_int(spx->dec, &spx->dec_bits, (spx_int16_t*)output->buf);

    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->size = samples_per_frame << 1;
    output->timestamp.u64 = input->timestamp.u64;

    return PJ_SUCCESS;
}

/* 
 * Recover lost frame.
 */
static pj_status_t  spx_codec_recover(pjmedia_codec *codec, 
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output)
{
    struct spx_private *spx;
    unsigned count;

    /* output_buf_len is unreferenced when building in Release mode */
    PJ_UNUSED_ARG(output_buf_len);

    spx = (struct spx_private*) codec->codec_data;

    count = spx_factory.speex_param[spx->param_id].clock_rate * 20 / 1000;
    pj_assert(count <= output_buf_len/2);

    /* Recover packet loss */
    speex_decode_int(spx->dec, NULL, (spx_int16_t*) output->buf);

    output->size = count * 2;

    return PJ_SUCCESS;
}


#endif	/* PJMEDIA_HAS_SPEEX_CODEC */
