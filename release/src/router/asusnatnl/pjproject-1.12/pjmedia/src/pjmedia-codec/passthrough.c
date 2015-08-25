/* $Id: passthrough.c 4063 2012-04-20 03:19:42Z nanang $ */
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
#include <pjmedia-codec/passthrough.h>
#include <pjmedia/codec.h>
#include <pjmedia/errno.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/port.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/math.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/os.h>

/*
 * Only build this file if PJMEDIA_HAS_PASSTHROUGH_CODECS != 0
 */
#if defined(PJMEDIA_HAS_PASSTHROUGH_CODECS) && PJMEDIA_HAS_PASSTHROUGH_CODECS!=0

#define THIS_FILE   "passthrough.c"


/* Prototypes for passthrough codecs factory */
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

/* Prototypes for passthrough codecs implementation. */
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

/* Definition for passthrough codecs operations. */
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

/* Definition for passthrough codecs factory operations. */
static pjmedia_codec_factory_op codec_factory_op =
{
    &test_alloc,
    &default_attr,
    &enum_codecs,
    &alloc_codec,
    &dealloc_codec
};

/* Passthrough codecs factory */
static struct codec_factory {
    pjmedia_codec_factory    base;
    pjmedia_endpt	    *endpt;
    pj_pool_t		    *pool;
    pj_mutex_t		    *mutex;
} codec_factory;

/* Passthrough codecs private data. */
typedef struct codec_private {
    pj_pool_t		*pool;		    /**< Pool for each instance.    */
    int			 codec_idx;	    /**< Codec index.		    */
    void		*codec_setting;	    /**< Specific codec setting.    */
    pj_uint16_t		 avg_frame_size;    /**< Average of frame size.	    */
    unsigned		 samples_per_frame; /**< Samples per frame, for iLBC
						 this can be 240 or 160, can
						 only be known after codec is
						 opened.		    */
} codec_private_t;



/* CUSTOM CALLBACKS */

/* Parse frames from a packet. Default behaviour of frame parsing is 
 * just separating frames based on calculating frame length derived 
 * from bitrate. Implement this callback when the default behaviour is 
 * unapplicable.
 */
typedef pj_status_t (*parse_cb)(codec_private_t *codec_data, void *pkt, 
				pj_size_t pkt_size, const pj_timestamp *ts,
				unsigned *frame_cnt, pjmedia_frame frames[]);

/* Pack frames into a packet. Default behaviour of packing frames is 
 * just stacking the frames with octet aligned without adding any 
 * payload header. Implement this callback when the default behaviour is
 * unapplicable.
 */
typedef pj_status_t (*pack_cb)(codec_private_t *codec_data, 
			       const struct pjmedia_frame_ext *input,
			       unsigned output_buf_len, 
			       struct pjmedia_frame *output);


/* Custom callback implementations. */
static pj_status_t parse_amr( codec_private_t *codec_data, void *pkt, 
			      pj_size_t pkt_size, const pj_timestamp *ts,
			      unsigned *frame_cnt, pjmedia_frame frames[]);
static pj_status_t pack_amr ( codec_private_t *codec_data,
			      const struct pjmedia_frame_ext *input,
			      unsigned output_buf_len, 
			      struct pjmedia_frame *output);


/* Passthrough codec implementation descriptions. */
static struct codec_desc {
    int		     enabled;		/* Is this codec enabled?	    */
    const char	    *name;		/* Codec name.			    */
    pj_uint8_t	     pt;		/* Payload type.		    */
    pjmedia_format_id fmt_id;		/* Source format.		    */
    unsigned	     clock_rate;	/* Codec's clock rate.		    */
    unsigned	     channel_count;	/* Codec's channel count.	    */
    unsigned	     samples_per_frame;	/* Codec's samples count.	    */
    unsigned	     def_bitrate;	/* Default bitrate of this codec.   */
    unsigned	     max_bitrate;	/* Maximum bitrate of this codec.   */
    pj_uint8_t	     frm_per_pkt;	/* Default num of frames per packet.*/
    pj_uint8_t	     vad;		/* VAD enabled/disabled.	    */
    pj_uint8_t	     plc;		/* PLC enabled/disabled.	    */
    parse_cb	     parse;		/* Callback to parse bitstream.	    */
    pack_cb	     pack;		/* Callback to pack bitstream.	    */
    pjmedia_codec_fmtp dec_fmtp;	/* Decoder's fmtp params.	    */
}

codec_desc[] = 
{
#   if PJMEDIA_HAS_PASSTHROUGH_CODEC_AMR
    {1, "AMR",	    PJMEDIA_RTP_PT_AMR,       PJMEDIA_FORMAT_AMR,
		    8000, 1, 160, 
		    7400, 12200, 2, 1, 1,
		    &parse_amr, &pack_amr
		    /*, {1, {{{"octet-align", 11}, {"1", 1}}} } */
    },
#   endif

#   if PJMEDIA_HAS_PASSTHROUGH_CODEC_G729
    {1, "G729",	    PJMEDIA_RTP_PT_G729,      PJMEDIA_FORMAT_G729,
		    8000, 1,  80,
		    8000, 8000, 2, 1, 1
    },
#   endif

#   if PJMEDIA_HAS_PASSTHROUGH_CODEC_ILBC
    {1, "iLBC",	    PJMEDIA_RTP_PT_ILBC,      PJMEDIA_FORMAT_ILBC,
		    8000, 1,  240,
		    13333, 15200, 1, 1, 1,
		    NULL, NULL,
		    {1, {{{"mode", 4}, {"30", 2}}} }
    },
#   endif

#   if PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMU
    {1, "PCMU",	    PJMEDIA_RTP_PT_PCMU,      PJMEDIA_FORMAT_PCMU,
		    8000, 1,  80,
		    64000, 64000, 2, 1, 1
    },
#   endif

#   if PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMA
    {1, "PCMA",	    PJMEDIA_RTP_PT_PCMA,      PJMEDIA_FORMAT_PCMA,
		    8000, 1,  80,
		    64000, 64000, 2, 1, 1
    },
#   endif
};


#if PJMEDIA_HAS_PASSTHROUGH_CODEC_AMR

#include <pjmedia-codec/amr_helper.h>

typedef struct amr_settings_t {
    pjmedia_codec_amr_pack_setting enc_setting;
    pjmedia_codec_amr_pack_setting dec_setting;
    pj_int8_t enc_mode;
} amr_settings_t;


/* Pack AMR payload */
static pj_status_t pack_amr ( codec_private_t *codec_data,
			      const struct pjmedia_frame_ext *input,
			      unsigned output_buf_len, 
			      struct pjmedia_frame *output)
{
    enum {MAX_FRAMES_PER_PACKET = PJMEDIA_MAX_FRAME_DURATION_MS / 20};

    pjmedia_frame frames[MAX_FRAMES_PER_PACKET];
    amr_settings_t* setting = (amr_settings_t*)codec_data->codec_setting;
    pjmedia_codec_amr_pack_setting *enc_setting = &setting->enc_setting;
    pj_uint8_t SID_FT;
    unsigned i;

    pj_assert(input->subframe_cnt <= MAX_FRAMES_PER_PACKET);

    SID_FT = (pj_uint8_t)(enc_setting->amr_nb? 8 : 9);

    /* Get frames */
    for (i = 0; i < input->subframe_cnt; ++i) {
	pjmedia_frame_ext_subframe *sf;
	pjmedia_codec_amr_bit_info *info;
	unsigned len;
	
	sf = pjmedia_frame_ext_get_subframe(input, i);
	len = (sf->bitlen + 7) >> 3;
	
	info = (pjmedia_codec_amr_bit_info*) &frames[i].bit_info;
	pj_bzero(info, sizeof(*info));
	
	if (len == 0) {
	    /* DTX */
	    info->frame_type = 15;
	} else {
	    info->frame_type = pjmedia_codec_amr_get_mode2(enc_setting->amr_nb, 
							   len);
	}
	info->good_quality = 1;
	info->mode = setting->enc_mode;
	if (info->frame_type == SID_FT)
	    info->STI = (sf->data[4] >> 4) & 1;

	frames[i].buf = sf->data;
	frames[i].size = len;
    }

    output->size = output_buf_len;

    return pjmedia_codec_amr_pack(frames, input->subframe_cnt, enc_setting, 
				  output->buf, &output->size);
}


/* Parse AMR payload into frames. */
static pj_status_t parse_amr(codec_private_t *codec_data, void *pkt, 
			     pj_size_t pkt_size, const pj_timestamp *ts,
			     unsigned *frame_cnt, pjmedia_frame frames[])
{
    amr_settings_t* s = (amr_settings_t*)codec_data->codec_setting;
    pjmedia_codec_amr_pack_setting *setting;
    pj_status_t status;
    pj_uint8_t cmr;

    setting = &s->dec_setting;

    status = pjmedia_codec_amr_parse(pkt, pkt_size, ts, setting, frames, 
				     frame_cnt, &cmr);
    if (status != PJ_SUCCESS)
	return status;

    // CMR is not supported for now. 
    /* Check Change Mode Request. */
    //if ((setting->amr_nb && cmr <= 7) || (!setting->amr_nb && cmr <= 8)) {
    //	s->enc_mode = cmr;
    //}

    return PJ_SUCCESS;
}

#endif /* PJMEDIA_HAS_PASSTROUGH_CODEC_AMR */


/*
 * Initialize and register passthrough codec factory to pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_passthrough_init( pjmedia_endpt *endpt )
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (codec_factory.pool != NULL) {
	/* Already initialized. */
	return PJ_EEXISTS;
    }

    /* Create passthrough codec factory. */
    codec_factory.base.op = &codec_factory_op;
    codec_factory.base.factory_data = NULL;
    codec_factory.endpt = endpt;

    codec_factory.pool = pjmedia_endpt_create_pool(endpt, "Passthrough codecs",
						   4000, 4000);
    if (!codec_factory.pool)
	return PJ_ENOMEM;

    /* Create mutex. */
    status = pj_mutex_create_simple(codec_factory.pool, "Passthrough codecs",
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
    pj_pool_release(codec_factory.pool);
    codec_factory.pool = NULL;
    return status;
}

/*
 * Initialize and register passthrough codec factory to pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_passthrough_init2( 
		      pjmedia_endpt *endpt,
		      const pjmedia_codec_passthrough_setting *setting)
{
    if (codec_factory.pool != NULL) {
	/* Already initialized. */
	return PJ_EEXISTS;
    }

    if (setting != NULL) {
	unsigned i;

	/* Enable/disable codecs based on the specified encoding formats */
	for (i = 0; i < PJ_ARRAY_SIZE(codec_desc); ++i) {
	    pj_bool_t enabled = PJ_FALSE;
	    unsigned j;

	    for (j = 0; j < setting->fmt_cnt && !enabled; ++j) {
		if (codec_desc[i].fmt_id == setting->fmts[j].id)
		    enabled = PJ_TRUE;
	    }

	    codec_desc[i].enabled = enabled;
	}

#if PJMEDIA_HAS_PASSTHROUGH_CODEC_ILBC
	/* Update iLBC codec description based on default mode setting. */
	for (i = 0; i < PJ_ARRAY_SIZE(codec_desc); ++i) {
	    if (codec_desc[i].enabled && 
		codec_desc[i].fmt_id == PJMEDIA_FORMAT_ILBC)
	    {
		codec_desc[i].samples_per_frame = 
				(setting->ilbc_mode == 20? 160 : 240);
		codec_desc[i].def_bitrate = 
				(setting->ilbc_mode == 20? 15200 : 13333);
		pj_strset2(&codec_desc[i].dec_fmtp.param[0].val, 
				(setting->ilbc_mode == 20? "20" : "30"));
		break;
	    }
	}
#endif
    }

    return pjmedia_codec_passthrough_init(endpt);
}

/*
 * Unregister passthrough codecs factory from pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_passthrough_deinit(void)
{
    pjmedia_codec_mgr *codec_mgr;
    unsigned i;
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

    /* Unregister passthrough codecs factory. */
    status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
						  &codec_factory.base);
    
    /* Destroy mutex. */
    pj_mutex_destroy(codec_factory.mutex);

    /* Destroy pool. */
    pj_pool_release(codec_factory.pool);
    codec_factory.pool = NULL;

    /* Re-enable all codecs in the codec_desc. */
    for (i = 0; i < PJ_ARRAY_SIZE(codec_desc); ++i) {
	codec_desc[i].enabled = PJ_TRUE;
    }

    return status;
}

/* 
 * Check if factory can allocate the specified codec. 
 */
static pj_status_t test_alloc( pjmedia_codec_factory *factory, 
			       const pjmedia_codec_info *info )
{
    unsigned i;

    PJ_UNUSED_ARG(factory);

    /* Type MUST be audio. */
    if (info->type != PJMEDIA_TYPE_AUDIO)
	return PJMEDIA_CODEC_EUNSUP;

    for (i = 0; i < PJ_ARRAY_SIZE(codec_desc); ++i) {
	pj_str_t name = pj_str((char*)codec_desc[i].name);
	if ((pj_stricmp(&info->encoding_name, &name) == 0) &&
	    (info->clock_rate == (unsigned)codec_desc[i].clock_rate) &&
	    (info->channel_cnt == (unsigned)codec_desc[i].channel_count) &&
	    (codec_desc[i].enabled))
	{
	    return PJ_SUCCESS;
	}
    }
    
    /* Unsupported, or mode is disabled. */
    return PJMEDIA_CODEC_EUNSUP;
}

/*
 * Generate default attribute.
 */
static pj_status_t default_attr ( pjmedia_codec_factory *factory, 
				  const pjmedia_codec_info *id, 
				  pjmedia_codec_param *attr )
{
    unsigned i;

    PJ_ASSERT_RETURN(factory==&codec_factory.base, PJ_EINVAL);

    pj_bzero(attr, sizeof(pjmedia_codec_param));

    for (i = 0; i < PJ_ARRAY_SIZE(codec_desc); ++i) {
	pj_str_t name = pj_str((char*)codec_desc[i].name);
	if ((pj_stricmp(&id->encoding_name, &name) == 0) &&
	    (id->clock_rate == (unsigned)codec_desc[i].clock_rate) &&
	    (id->channel_cnt == (unsigned)codec_desc[i].channel_count) &&
	    (id->pt == (unsigned)codec_desc[i].pt))
	{
	    attr->info.pt = (pj_uint8_t)id->pt;
	    attr->info.channel_cnt = codec_desc[i].channel_count;
	    attr->info.clock_rate = codec_desc[i].clock_rate;
	    attr->info.avg_bps = codec_desc[i].def_bitrate;
	    attr->info.max_bps = codec_desc[i].max_bitrate;
	    attr->info.pcm_bits_per_sample = 16;
	    attr->info.frm_ptime =  (pj_uint16_t)
				    (codec_desc[i].samples_per_frame * 1000 / 
				    codec_desc[i].channel_count / 
				    codec_desc[i].clock_rate);
	    attr->info.fmt_id = codec_desc[i].fmt_id;

	    /* Default flags. */
	    attr->setting.frm_per_pkt = codec_desc[i].frm_per_pkt;
	    attr->setting.plc = codec_desc[i].plc;
	    attr->setting.penh= 0;
	    attr->setting.vad = codec_desc[i].vad;
	    attr->setting.cng = attr->setting.vad;
	    attr->setting.dec_fmtp = codec_desc[i].dec_fmtp;

	    if (attr->setting.vad == 0) {
#if PJMEDIA_HAS_PASSTHROUGH_CODEC_G729
		if (id->pt == PJMEDIA_RTP_PT_G729) {
		    /* Signal G729 Annex B is being disabled */
		    attr->setting.dec_fmtp.cnt = 1;
		    pj_strset2(&attr->setting.dec_fmtp.param[0].name, "annexb");
		    pj_strset2(&attr->setting.dec_fmtp.param[0].val, "no");
		}
#endif
	    }

	    return PJ_SUCCESS;
	}
    }

    return PJMEDIA_CODEC_EUNSUP;
}

/*
 * Enum codecs supported by this factory.
 */
static pj_status_t enum_codecs( pjmedia_codec_factory *factory, 
				unsigned *count, 
				pjmedia_codec_info codecs[])
{
    unsigned max;
    unsigned i;

    PJ_UNUSED_ARG(factory);
    PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);

    max = *count;
    
    for (i = 0, *count = 0; i < PJ_ARRAY_SIZE(codec_desc) && *count < max; ++i) 
    {
	if (!codec_desc[i].enabled)
	    continue;

	pj_bzero(&codecs[*count], sizeof(pjmedia_codec_info));
	codecs[*count].encoding_name = pj_str((char*)codec_desc[i].name);
	codecs[*count].pt = codec_desc[i].pt;
	codecs[*count].type = PJMEDIA_TYPE_AUDIO;
	codecs[*count].clock_rate = codec_desc[i].clock_rate;
	codecs[*count].channel_cnt = codec_desc[i].channel_count;

	++*count;
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
    int idx;
    pj_pool_t *pool;
    unsigned i;

    PJ_ASSERT_RETURN(factory && id && p_codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &codec_factory.base, PJ_EINVAL);

    pj_mutex_lock(codec_factory.mutex);

    /* Find codec's index */
    idx = -1;
    for (i = 0; i < PJ_ARRAY_SIZE(codec_desc); ++i) {
	pj_str_t name = pj_str((char*)codec_desc[i].name);
	if ((pj_stricmp(&id->encoding_name, &name) == 0) &&
	    (id->clock_rate == (unsigned)codec_desc[i].clock_rate) &&
	    (id->channel_cnt == (unsigned)codec_desc[i].channel_count) &&
	    (codec_desc[i].enabled))
	{
	    idx = i;
	    break;
	}
    }
    if (idx == -1) {
	*p_codec = NULL;
	return PJMEDIA_CODEC_EUNSUP;
    }

    /* Create pool for codec instance */
    pool = pjmedia_endpt_create_pool(codec_factory.endpt, "passthroughcodec",
				     512, 512);
    codec = PJ_POOL_ZALLOC_T(pool, pjmedia_codec);
    codec->op = &codec_op;
    codec->factory = factory;
    codec->codec_data = PJ_POOL_ZALLOC_T(pool, codec_private_t);
    codec_data = (codec_private_t*) codec->codec_data;
    codec_data->pool = pool;
    codec_data->codec_idx = idx;

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

    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &codec_factory.base, PJ_EINVAL);

    /* Close codec, if it's not closed. */
    codec_data = (codec_private_t*) codec->codec_data;
    codec_close(codec);

    pj_pool_release(codec_data->pool);

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
    struct codec_desc *desc = &codec_desc[codec_data->codec_idx];
    pj_pool_t *pool;
    int i, j;

    pool = codec_data->pool;

    /* Cache samples per frame value */
    codec_data->samples_per_frame = desc->samples_per_frame;

    /* Calculate bitstream size */
    i = attr->info.avg_bps * codec_data->samples_per_frame;
    j = desc->clock_rate << 3;
    codec_data->avg_frame_size = (pj_uint16_t)(i / j);
    if (i % j) ++codec_data->avg_frame_size;

#if PJMEDIA_HAS_PASSTHROUGH_CODEC_AMR
    /* Init AMR settings */
    if (desc->pt == PJMEDIA_RTP_PT_AMR || desc->pt == PJMEDIA_RTP_PT_AMRWB) {
	amr_settings_t *s;
	pj_uint8_t octet_align = 0;
	pj_int8_t enc_mode;
	
	enc_mode = pjmedia_codec_amr_get_mode(attr->info.avg_bps);
	pj_assert(enc_mode >= 0 && enc_mode <= 8);

	for (i = 0; i < attr->setting.dec_fmtp.cnt; ++i) {
	    const pj_str_t STR_FMTP_OCTET_ALIGN = {"octet-align", 11};
	    
	    /* Fetch octet-align setting. It should be fine to fetch only 
	     * the decoder, since encoder & decoder must use the same setting 
	     * (RFC 4867 section 8.3.1).
	     */
	    if (pj_stricmp(&attr->setting.dec_fmtp.param[i].name, 
			   &STR_FMTP_OCTET_ALIGN) == 0)
	    {
		octet_align=(pj_uint8_t)
			    (pj_strtoul(&attr->setting.dec_fmtp.param[i].val));
		break;
	    }
	}

	for (i = 0; i < attr->setting.enc_fmtp.cnt; ++i) {
	    const pj_str_t STR_FMTP_MODE_SET = {"mode-set", 8};

	    /* mode-set, encoding mode is chosen based on local default mode 
	     * setting:
	     * - if local default mode is included in the mode-set, use it
	     * - otherwise, find the closest mode to local default mode;
	     *   if there are two closest modes, prefer to use the higher
	     *   one, e.g: local default mode is 4, the mode-set param
	     *   contains '2,3,5,6', then 5 will be chosen.
	     */
	    if (pj_stricmp(&attr->setting.enc_fmtp.param[i].name, 
			   &STR_FMTP_MODE_SET) == 0)
	    {
		const char *p;
		pj_size_t l;
		pj_int8_t diff = 99;
		
		p = pj_strbuf(&attr->setting.enc_fmtp.param[i].val);
		l = pj_strlen(&attr->setting.enc_fmtp.param[i].val);

		while (l--) {
		    if ((desc->pt==PJMEDIA_RTP_PT_AMR && *p>='0' && *p<='7') ||
		        (desc->pt==PJMEDIA_RTP_PT_AMRWB && *p>='0' && *p<='8'))
		    {
			pj_int8_t tmp = (pj_int8_t)(*p - '0' - enc_mode);

			if (PJ_ABS(diff) > PJ_ABS(tmp) || 
			    (PJ_ABS(diff) == PJ_ABS(tmp) && tmp > diff))
			{
			    diff = tmp;
			    if (diff == 0) break;
			}
		    }
		    ++p;
		}

		if (diff == 99)
		    return PJMEDIA_CODEC_EFAILED;

		enc_mode = (pj_int8_t)(enc_mode + diff);

		break;
	    }
	}

	s = PJ_POOL_ZALLOC_T(pool, amr_settings_t);
	codec_data->codec_setting = s;

	s->enc_mode = enc_mode;
	if (s->enc_mode < 0)
	    return PJMEDIA_CODEC_EINMODE;

	s->enc_setting.amr_nb = (pj_uint8_t)(desc->pt == PJMEDIA_RTP_PT_AMR);
	s->enc_setting.octet_aligned = octet_align;
	s->enc_setting.reorder = PJ_FALSE; /* Note this! passthrough codec
					      doesn't do sensitivity bits 
					      reordering */
	s->enc_setting.cmr = 15;
	
	s->dec_setting.amr_nb = (pj_uint8_t)(desc->pt == PJMEDIA_RTP_PT_AMR);
	s->dec_setting.octet_aligned = octet_align;
	s->dec_setting.reorder = PJ_FALSE; /* Note this! passthrough codec
					      doesn't do sensitivity bits 
					      reordering */
	
	/* Return back bitrate info to application */
	attr->info.avg_bps = s->enc_setting.amr_nb?
			     pjmedia_codec_amrnb_bitrates[s->enc_mode]:
			     pjmedia_codec_amrwb_bitrates[s->enc_mode];
    }
#endif

#if PJMEDIA_HAS_PASSTHROUGH_CODEC_ILBC
    /* Init iLBC settings */
    if (desc->pt == PJMEDIA_RTP_PT_ILBC)
    {
	enum { DEFAULT_MODE = 30 };
	static pj_str_t STR_MODE = {"mode", 4};
	pj_uint16_t dec_fmtp_mode = DEFAULT_MODE, 
		    enc_fmtp_mode = DEFAULT_MODE;

	/* Get decoder mode */
	for (i = 0; i < attr->setting.dec_fmtp.cnt; ++i) {
	    if (pj_stricmp(&attr->setting.dec_fmtp.param[i].name, &STR_MODE) == 0)
	    {
		dec_fmtp_mode = (pj_uint16_t)
				pj_strtoul(&attr->setting.dec_fmtp.param[i].val);
		break;
	    }
	}

	/* Decoder mode must be set */
	PJ_ASSERT_RETURN(dec_fmtp_mode == 20 || dec_fmtp_mode == 30, 
			 PJMEDIA_CODEC_EINMODE);

	/* Get encoder mode */
	for (i = 0; i < attr->setting.enc_fmtp.cnt; ++i) {
	    if (pj_stricmp(&attr->setting.enc_fmtp.param[i].name, &STR_MODE) == 0)
	    {
		enc_fmtp_mode = (pj_uint16_t)
				pj_strtoul(&attr->setting.enc_fmtp.param[i].val);
		break;
	    }
	}

	PJ_ASSERT_RETURN(enc_fmtp_mode==20 || enc_fmtp_mode==30, 
			 PJMEDIA_CODEC_EINMODE);

	/* Both sides of a bi-directional session MUST use the same "mode" value.
	 * In this point, possible values are only 20 or 30, so when encoder and
	 * decoder modes are not same, just use the default mode, it is 30.
	 */
	if (enc_fmtp_mode != dec_fmtp_mode) {
	    enc_fmtp_mode = dec_fmtp_mode = DEFAULT_MODE;
	    PJ_LOG(4,(pool->obj_name, 
		      "Normalized iLBC encoder and decoder modes to %d", 
		      DEFAULT_MODE));
	}

	/* Update some attributes based on negotiated mode. */
	attr->info.avg_bps = (dec_fmtp_mode == 30? 13333 : 15200);
	attr->info.frm_ptime = dec_fmtp_mode;

	/* Override average frame size */
	codec_data->avg_frame_size = (dec_fmtp_mode == 30? 50 : 38);

	/* Override samples per frame */
	codec_data->samples_per_frame = (dec_fmtp_mode == 30? 240 : 160);
    }
#endif

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
    /* Not supported yet. */
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(attr);

    return PJ_ENOTSUP;
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
    struct codec_desc *desc = &codec_desc[codec_data->codec_idx];
    unsigned count = 0;

    PJ_ASSERT_RETURN(frame_cnt, PJ_EINVAL);

    if (desc->parse != NULL) {
	return desc->parse(codec_data, pkt,  pkt_size, ts, frame_cnt, frames);
    }

    while (pkt_size >= codec_data->avg_frame_size && count < *frame_cnt) {
	frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
	frames[count].buf = pkt;
	frames[count].size = codec_data->avg_frame_size;
	frames[count].timestamp.u64 = ts->u64 + 
				      count * codec_data->samples_per_frame;

	pkt = (pj_uint8_t*)pkt + codec_data->avg_frame_size;
	pkt_size -= codec_data->avg_frame_size;

	++count;
    }

    if (pkt_size && count < *frame_cnt) {
	frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
	frames[count].buf = pkt;
	frames[count].size = pkt_size;
	frames[count].timestamp.u64 = ts->u64 + 
				       count * codec_data->samples_per_frame;
	++count;
    }

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
    struct codec_desc *desc = &codec_desc[codec_data->codec_idx];
    const pjmedia_frame_ext *input_ = (const pjmedia_frame_ext*) input;

    pj_assert(input && input->type == PJMEDIA_FRAME_TYPE_EXTENDED);

    if (desc->pack != NULL) {
	desc->pack(codec_data, input_, output_buf_len, output);
    } else {
	if (input_->subframe_cnt == 0) {
	    /* DTX */
	    output->buf = NULL;
	    output->size = 0;
	    output->type = PJMEDIA_FRAME_TYPE_NONE;
	} else {
	    unsigned i;
	    pj_uint8_t *p = output->buf;

	    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
	    output->size = 0;
	    
	    for (i = 0; i < input_->subframe_cnt; ++i) {
		pjmedia_frame_ext_subframe *sf;
		unsigned sf_len;

		sf = pjmedia_frame_ext_get_subframe(input_, i);
		pj_assert(sf);

		sf_len = (sf->bitlen + 7) >> 3;

		pj_memcpy(p, sf->data, sf_len);
		p += sf_len;
		output->size += sf_len;

		/* If there is SID or DTX frame, break the loop. */
		if (desc->pt == PJMEDIA_RTP_PT_G729 && 
		    sf_len < codec_data->avg_frame_size)
		{
		    break;
		}
		
	    }
	}
    }

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
#if PJMEDIA_HAS_PASSTHROUGH_CODEC_AMR
    struct codec_desc *desc = &codec_desc[codec_data->codec_idx];
#endif
    pjmedia_frame_ext *output_ = (pjmedia_frame_ext*) output;

    pj_assert(input);
    PJ_UNUSED_ARG(output_buf_len);

#if PJMEDIA_HAS_PASSTHROUGH_CODEC_AMR
    /* Need to rearrange the AMR bitstream, since the bitstream may not be 
     * started from bit 0 or may need to be reordered from sensitivity order 
     * into encoder bits order.
     */
    if (desc->pt == PJMEDIA_RTP_PT_AMR || desc->pt == PJMEDIA_RTP_PT_AMRWB) {
	pjmedia_frame input_;
	pjmedia_codec_amr_pack_setting *setting;

	setting = &((amr_settings_t*)codec_data->codec_setting)->dec_setting;

	input_ = *input;
	pjmedia_codec_amr_predecode(input, setting, &input_);
	
	pjmedia_frame_ext_append_subframe(output_, input_.buf, 
					  (pj_uint16_t)(input_.size << 3),
					  (pj_uint16_t)codec_data->samples_per_frame);
	output->timestamp = input->timestamp;
	
	return PJ_SUCCESS;
    }
#endif
    
    pjmedia_frame_ext_append_subframe(output_, input->buf, 
				      (pj_uint16_t)(input->size << 3),
				      (pj_uint16_t)codec_data->samples_per_frame);
    output->timestamp = input->timestamp;

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
    pjmedia_frame_ext *output_ = (pjmedia_frame_ext*) output;

    PJ_UNUSED_ARG(output_buf_len);

    pjmedia_frame_ext_append_subframe(output_, NULL, 0,
				      (pj_uint16_t)codec_data->samples_per_frame);

    return PJ_SUCCESS;
}

#endif	/* PJMEDIA_HAS_PASSTHROUGH_CODECS */

