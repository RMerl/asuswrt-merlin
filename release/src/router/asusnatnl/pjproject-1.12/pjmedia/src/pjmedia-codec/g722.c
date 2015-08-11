/* $Id: g722.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia-codec/g722.h>
#include <pjmedia/codec.h>
#include <pjmedia/errno.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/plc.h>
#include <pjmedia/port.h>
#include <pjmedia/silencedet.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/os.h>

#if defined(PJMEDIA_HAS_G722_CODEC) && (PJMEDIA_HAS_G722_CODEC != 0)

#include "g722/g722_enc.h"
#include "g722/g722_dec.h"

#define THIS_FILE   "g722.c"

/* Defines */
#define PTIME			(10)
#define SAMPLES_PER_FRAME	(16000 * PTIME /1000)
#define FRAME_LEN		(80)
#define PLC_DISABLED		0

/* Tracing */
#ifndef PJ_TRACE
#   define PJ_TRACE	0	
#endif

#if PJ_TRACE 
#   define TRACE_(expr)	PJ_LOG(4,expr)
#else
#   define TRACE_(expr)
#endif


/* Prototypes for G722 factory */
static pj_status_t g722_test_alloc(pjmedia_codec_factory *factory, 
				   const pjmedia_codec_info *id );
static pj_status_t g722_default_attr(pjmedia_codec_factory *factory, 
				     const pjmedia_codec_info *id, 
				     pjmedia_codec_param *attr );
static pj_status_t g722_enum_codecs(pjmedia_codec_factory *factory, 
				    unsigned *count, 
				    pjmedia_codec_info codecs[]);
static pj_status_t g722_alloc_codec(pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id, 
				    pjmedia_codec **p_codec);
static pj_status_t g722_dealloc_codec(pjmedia_codec_factory *factory, 
				      pjmedia_codec *codec );

/* Prototypes for G722 implementation. */
static pj_status_t  g722_codec_init(pjmedia_codec *codec, 
				    pj_pool_t *pool );
static pj_status_t  g722_codec_open(pjmedia_codec *codec, 
				    pjmedia_codec_param *attr );
static pj_status_t  g722_codec_close(pjmedia_codec *codec );
static pj_status_t  g722_codec_modify(pjmedia_codec *codec, 
				      const pjmedia_codec_param *attr );
static pj_status_t  g722_codec_parse(pjmedia_codec *codec,
				     void *pkt,
				     pj_size_t pkt_size,
				     const pj_timestamp *ts,
				     unsigned *frame_cnt,
				     pjmedia_frame frames[]);
static pj_status_t  g722_codec_encode(pjmedia_codec *codec, 
				      const struct pjmedia_frame *input,
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);
static pj_status_t  g722_codec_decode(pjmedia_codec *codec, 
				      const struct pjmedia_frame *input,
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);
#if !PLC_DISABLED
static pj_status_t  g722_codec_recover(pjmedia_codec *codec,
				      unsigned output_buf_len,
				      struct pjmedia_frame *output);
#endif

/* Definition for G722 codec operations. */
static pjmedia_codec_op g722_op = 
{
    &g722_codec_init,
    &g722_codec_open,
    &g722_codec_close,
    &g722_codec_modify,
    &g722_codec_parse,
    &g722_codec_encode,
    &g722_codec_decode,
#if !PLC_DISABLED
    &g722_codec_recover
#else
    NULL
#endif
};

/* Definition for G722 codec factory operations. */
static pjmedia_codec_factory_op g722_factory_op =
{
    &g722_test_alloc,
    &g722_default_attr,
    &g722_enum_codecs,
    &g722_alloc_codec,
    &g722_dealloc_codec
};

/* G722 factory */
static struct g722_codec_factory
{
    pjmedia_codec_factory    base;
    pjmedia_endpt	    *endpt;
    pj_pool_t		    *pool;
    pj_mutex_t		    *mutex;
    pjmedia_codec	     codec_list;
    unsigned		     pcm_shift;
} g722_codec_factory;


/* G722 codec private data. */
struct g722_data
{
    g722_enc_t		 encoder;
    g722_dec_t		 decoder;
    unsigned		 pcm_shift;
    pj_int16_t		 pcm_clip_mask;
    pj_bool_t		 plc_enabled;
    pj_bool_t		 vad_enabled;
    pjmedia_silence_det	*vad;
    pj_timestamp	 last_tx;
#if !PLC_DISABLED
    pjmedia_plc		*plc;
#endif
};



/*
 * Initialize and register G722 codec factory to pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_g722_init( pjmedia_endpt *endpt )
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (g722_codec_factory.pool != NULL)
	return PJ_SUCCESS;

    /* Create G722 codec factory. */
    g722_codec_factory.base.op = &g722_factory_op;
    g722_codec_factory.base.factory_data = NULL;
    g722_codec_factory.endpt = endpt;
    g722_codec_factory.pcm_shift = PJMEDIA_G722_DEFAULT_PCM_SHIFT;

    g722_codec_factory.pool = pjmedia_endpt_create_pool(endpt, "g722", 1000, 
						        1000);
    if (!g722_codec_factory.pool)
	return PJ_ENOMEM;

    pj_list_init(&g722_codec_factory.codec_list);

    /* Create mutex. */
    status = pj_mutex_create_simple(g722_codec_factory.pool, "g722", 
				    &g722_codec_factory.mutex);
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
						&g722_codec_factory.base);
    if (status != PJ_SUCCESS)
	goto on_error;

    TRACE_((THIS_FILE, "G722 codec factory initialized"));
    
    /* Done. */
    return PJ_SUCCESS;

on_error:
    pj_pool_release(g722_codec_factory.pool);
    g722_codec_factory.pool = NULL;
    return status;
}

/*
 * Unregister G722 codec factory from pjmedia endpoint and deinitialize
 * the G722 codec library.
 */
PJ_DEF(pj_status_t) pjmedia_codec_g722_deinit(void)
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (g722_codec_factory.pool == NULL)
	return PJ_SUCCESS;

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(g722_codec_factory.endpt);
    if (!codec_mgr) {
	pj_pool_release(g722_codec_factory.pool);
	g722_codec_factory.pool = NULL;
	return PJ_EINVALIDOP;
    }

    /* Unregister G722 codec factory. */
    status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
						  &g722_codec_factory.base);
    
    /* Destroy mutex. */
    pj_mutex_destroy(g722_codec_factory.mutex);

    /* Destroy pool. */
    pj_pool_release(g722_codec_factory.pool);
    g722_codec_factory.pool = NULL;
    
    TRACE_((THIS_FILE, "G722 codec factory shutdown"));
    return status;
}


/*
 * Set level adjustment.
 */
PJ_DEF(pj_status_t) pjmedia_codec_g722_set_pcm_shift(unsigned val)
{
    g722_codec_factory.pcm_shift = val;
    return PJ_SUCCESS;
}


/* 
 * Check if factory can allocate the specified codec. 
 */
static pj_status_t g722_test_alloc(pjmedia_codec_factory *factory, 
				   const pjmedia_codec_info *info )
{
    PJ_UNUSED_ARG(factory);

    /* Check payload type. */
    if (info->pt != PJMEDIA_RTP_PT_G722)
	return PJMEDIA_CODEC_EUNSUP;

    /* Ignore the rest, since it's static payload type. */

    return PJ_SUCCESS;
}

/*
 * Generate default attribute.
 */
static pj_status_t g722_default_attr( pjmedia_codec_factory *factory, 
				      const pjmedia_codec_info *id, 
				      pjmedia_codec_param *attr )
{
    PJ_UNUSED_ARG(factory);
    PJ_UNUSED_ARG(id);

    pj_bzero(attr, sizeof(pjmedia_codec_param));
    attr->info.clock_rate = 16000;
    attr->info.channel_cnt = 1;
    attr->info.avg_bps = 64000;
    attr->info.max_bps = 64000;
    attr->info.pcm_bits_per_sample = 16;
    attr->info.frm_ptime = PTIME;
    attr->info.pt = PJMEDIA_RTP_PT_G722;

    attr->setting.frm_per_pkt = 2;
    attr->setting.vad = 1;
    attr->setting.plc = 1;

    /* Default all other flag bits disabled. */

    return PJ_SUCCESS;
}

/*
 * Enum codecs supported by this factory (i.e. only G722!).
 */
static pj_status_t g722_enum_codecs(pjmedia_codec_factory *factory, 
				    unsigned *count, 
				    pjmedia_codec_info codecs[])
{
    PJ_UNUSED_ARG(factory);
    PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);

    pj_bzero(&codecs[0], sizeof(pjmedia_codec_info));
    codecs[0].encoding_name = pj_str("G722");
    codecs[0].pt = PJMEDIA_RTP_PT_G722;
    codecs[0].type = PJMEDIA_TYPE_AUDIO;
    codecs[0].clock_rate = 16000;
    codecs[0].channel_cnt = 1;

    *count = 1;

    return PJ_SUCCESS;
}

/*
 * Allocate a new G722 codec instance.
 */
static pj_status_t g722_alloc_codec(pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id,
				    pjmedia_codec **p_codec)
{
    pjmedia_codec *codec;
    struct g722_data *g722_data;

    PJ_ASSERT_RETURN(factory && id && p_codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &g722_codec_factory.base, PJ_EINVAL);

    pj_mutex_lock(g722_codec_factory.mutex);

    /* Get free nodes, if any. */
    if (!pj_list_empty(&g722_codec_factory.codec_list)) {
	codec = g722_codec_factory.codec_list.next;
	pj_list_erase(codec);
    } else {
	pj_status_t status;

	codec = PJ_POOL_ZALLOC_T(g722_codec_factory.pool, pjmedia_codec);
	PJ_ASSERT_RETURN(codec != NULL, PJ_ENOMEM);
	codec->op = &g722_op;
	codec->factory = factory;

	g722_data = PJ_POOL_ZALLOC_T(g722_codec_factory.pool, struct g722_data);
	codec->codec_data = g722_data;

#if !PLC_DISABLED
    	/* Create PLC */
    	status = pjmedia_plc_create(g722_codec_factory.pool, 16000, 
			            SAMPLES_PER_FRAME, 0, &g722_data->plc);
	if (status != PJ_SUCCESS) {
	    pj_mutex_unlock(g722_codec_factory.mutex);
	    return status;
	}
#endif

	/* Create silence detector */
	status = pjmedia_silence_det_create(g722_codec_factory.pool,
					    16000, SAMPLES_PER_FRAME,
					    &g722_data->vad);
	if (status != PJ_SUCCESS) {
	    pj_mutex_unlock(g722_codec_factory.mutex);
	    TRACE_((THIS_FILE, "Create silence detector failed (status = %d)", 
                               status));
	    return status;
	}
    }


    pj_mutex_unlock(g722_codec_factory.mutex);

    *p_codec = codec;
    return PJ_SUCCESS;
}

/*
 * Free codec.
 */
static pj_status_t g722_dealloc_codec(pjmedia_codec_factory *factory, 
				      pjmedia_codec *codec )
{
    struct g722_data *g722_data;
    int i;

    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &g722_codec_factory.base, PJ_EINVAL);

    g722_data = (struct g722_data*) codec->codec_data;

    /* Close codec, if it's not closed. */
    g722_codec_close(codec);

#if !PLC_DISABLED
    /* Clear left samples in the PLC, since codec+plc will be reused
     * next time.
     */
    for (i=0; i<2; ++i) {
	pj_int16_t frame[SAMPLES_PER_FRAME];
	pjmedia_zero_samples(frame, PJ_ARRAY_SIZE(frame));
	pjmedia_plc_save(g722_data->plc, frame);
    }
#else
    PJ_UNUSED_ARG(i);
#endif

    /* Re-init silence_period */
    pj_set_timestamp32(&g722_data->last_tx, 0, 0);

    /* Put in the free list. */
    pj_mutex_lock(g722_codec_factory.mutex);
    pj_list_push_front(&g722_codec_factory.codec_list, codec);
    pj_mutex_unlock(g722_codec_factory.mutex);

    return PJ_SUCCESS;
}

/*
 * Init codec.
 */
static pj_status_t g722_codec_init(pjmedia_codec *codec, 
				   pj_pool_t *pool )
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}

/*
 * Open codec.
 */
static pj_status_t g722_codec_open(pjmedia_codec *codec, 
				   pjmedia_codec_param *attr )
{
    struct g722_data *g722_data = (struct g722_data*) codec->codec_data;
    pj_status_t status;

    PJ_ASSERT_RETURN(codec && attr, PJ_EINVAL);
    PJ_ASSERT_RETURN(g722_data != NULL, PJ_EINVALIDOP);

    status = g722_enc_init(&g722_data->encoder);
    if (status != PJ_SUCCESS) {
	TRACE_((THIS_FILE, "g722_enc_init() failed, status=%d", status));
	pj_mutex_unlock(g722_codec_factory.mutex);
	return PJMEDIA_CODEC_EFAILED;
    }

    status = g722_dec_init(&g722_data->decoder);
    if (status != PJ_SUCCESS) {
	TRACE_((THIS_FILE, "g722_dec_init() failed, status=%d", status));
	pj_mutex_unlock(g722_codec_factory.mutex);
	return PJMEDIA_CODEC_EFAILED;
    }

    g722_data->vad_enabled = (attr->setting.vad != 0);
    g722_data->plc_enabled = (attr->setting.plc != 0);
    g722_data->pcm_shift = g722_codec_factory.pcm_shift;
    g722_data->pcm_clip_mask = (pj_int16_t)(1<<g722_codec_factory.pcm_shift)-1;
    g722_data->pcm_clip_mask <<= (16-g722_codec_factory.pcm_shift);

    TRACE_((THIS_FILE, "G722 codec opened: vad=%d, plc=%d",
			g722_data->vad_enabled, g722_data->plc_enabled));
    return PJ_SUCCESS;
}

/*
 * Close codec.
 */
static pj_status_t g722_codec_close( pjmedia_codec *codec )
{
    /* The codec, encoder, and decoder will be reused, so there's
     * nothing to do here
     */

    PJ_UNUSED_ARG(codec);
    
    TRACE_((THIS_FILE, "G722 codec closed"));
    return PJ_SUCCESS;
}


/*
 * Modify codec settings.
 */
static pj_status_t  g722_codec_modify(pjmedia_codec *codec, 
				     const pjmedia_codec_param *attr )
{
    struct g722_data *g722_data = (struct g722_data*) codec->codec_data;

    pj_assert(g722_data != NULL);

    g722_data->vad_enabled = (attr->setting.vad != 0);
    g722_data->plc_enabled = (attr->setting.plc != 0);

    TRACE_((THIS_FILE, "G722 codec modified: vad=%d, plc=%d",
			g722_data->vad_enabled, g722_data->plc_enabled));
    return PJ_SUCCESS;
}


/*
 * Get frames in the packet.
 */
static pj_status_t  g722_codec_parse(pjmedia_codec *codec,
				     void *pkt,
				     pj_size_t pkt_size,
				     const pj_timestamp *ts,
				     unsigned *frame_cnt,
				     pjmedia_frame frames[])
{
    unsigned count = 0;

    PJ_UNUSED_ARG(codec);

    PJ_ASSERT_RETURN(frame_cnt, PJ_EINVAL);

    TRACE_((THIS_FILE, "G722 parse(): input len=%d", pkt_size));

    while (pkt_size >= FRAME_LEN && count < *frame_cnt) {
	frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
	frames[count].buf = pkt;
	frames[count].size = FRAME_LEN;
	frames[count].timestamp.u64 = ts->u64 + count * SAMPLES_PER_FRAME;

	pkt = ((char*)pkt) + FRAME_LEN;
	pkt_size -= FRAME_LEN;

	++count;
    }

    TRACE_((THIS_FILE, "G722 parse(): got %d frames", count));

    *frame_cnt = count;
    return PJ_SUCCESS;
}

/*
 * Encode frame.
 */
static pj_status_t g722_codec_encode(pjmedia_codec *codec, 
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len, 
				     struct pjmedia_frame *output)
{
    struct g722_data *g722_data = (struct g722_data*) codec->codec_data;
    pj_status_t status;

    pj_assert(g722_data && input && output);

    PJ_ASSERT_RETURN((input->size >> 2) <= output_buf_len, 
                     PJMEDIA_CODEC_EFRMTOOSHORT);

    /* Detect silence */
    if (g722_data->vad_enabled) {
	pj_bool_t is_silence;
	pj_int32_t silence_duration;

	silence_duration = pj_timestamp_diff32(&g722_data->last_tx, 
					       &input->timestamp);

	is_silence = pjmedia_silence_det_detect(g722_data->vad, 
					        (const pj_int16_t*) input->buf,
						(input->size >> 1),
						NULL);
	if (is_silence &&
	    (PJMEDIA_CODEC_MAX_SILENCE_PERIOD == -1 ||
	     silence_duration < PJMEDIA_CODEC_MAX_SILENCE_PERIOD*16000/1000))
	{
	    output->type = PJMEDIA_FRAME_TYPE_NONE;
	    output->buf = NULL;
	    output->size = 0;
	    output->timestamp = input->timestamp;
	    return PJ_SUCCESS;
	} else {
	    g722_data->last_tx = input->timestamp;
	}
    }

    /* Adjust input signal level from 16-bit to 14-bit */
    if (g722_data->pcm_shift) {
	pj_int16_t *p, *end;

	p = (pj_int16_t*)input->buf;
	end = p + input->size/2;
	while (p < end) {
	    *p++ >>= g722_data->pcm_shift;
	}
    }

    /* Encode to temporary buffer */
    output->size = output_buf_len;
    status = g722_enc_encode(&g722_data->encoder, (pj_int16_t*)input->buf, 
			     (input->size >> 1), output->buf, &output->size);
    if (status != PJ_SUCCESS) {
	output->size = 0;
	output->buf = NULL;
	output->type = PJMEDIA_FRAME_TYPE_NONE;
	TRACE_((THIS_FILE, "G722 encode() status: %d", status));
	return PJMEDIA_CODEC_EFAILED;
    }

    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;
    
    TRACE_((THIS_FILE, "G722 encode(): size=%d", output->size));
    return PJ_SUCCESS;
}

/*
 * Decode frame.
 */
static pj_status_t g722_codec_decode(pjmedia_codec *codec, 
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len, 
				     struct pjmedia_frame *output)
{
    struct g722_data *g722_data = (struct g722_data*) codec->codec_data;
    pj_status_t status;

    pj_assert(g722_data != NULL);
    PJ_ASSERT_RETURN(input && output, PJ_EINVAL);

    TRACE_((THIS_FILE, "G722 decode(): inbuf=%p, insize=%d, outbuf=%p,"
		       "outsize=%d",
	               input->buf, input->size, output->buf, output_buf_len));
    
    if (output_buf_len < SAMPLES_PER_FRAME * 2) {
        TRACE_((THIS_FILE, "G722 decode() ERROR: PJMEDIA_CODEC_EPCMTOOSHORT"));
	return PJMEDIA_CODEC_EPCMTOOSHORT;
    }

    if (input->size != FRAME_LEN) {
        TRACE_((THIS_FILE, "G722 decode() ERROR: PJMEDIA_CODEC_EFRMTOOSHORT"));
	return PJMEDIA_CODEC_EFRMTOOSHORT;
    }


    /* Decode */
    output->size = SAMPLES_PER_FRAME;
    status = g722_dec_decode(&g722_data->decoder, input->buf, input->size,
			     (pj_int16_t*)output->buf, &output->size);
    if (status != PJ_SUCCESS) {
	TRACE_((THIS_FILE, "G722 decode() status: %d", status));
	return PJMEDIA_CODEC_EFAILED;
    }

    pj_assert(output->size == SAMPLES_PER_FRAME);

    /* Adjust input signal level from 14-bit to 16-bit */
    if (g722_data->pcm_shift) {
	pj_int16_t *p, *end;

	p = (pj_int16_t*)output->buf;
	end = p + output->size;
	while (p < end) {
#if PJMEDIA_G722_STOP_PCM_SHIFT_ON_CLIPPING
	    /* If there is clipping, stop the PCM shifting */
	    if (*p & g722_data->pcm_clip_mask) {
		g722_data->pcm_shift = 0;
		break;
	    }
#endif
	    *p++ <<= g722_data->pcm_shift;
	}
    }

    output->size = SAMPLES_PER_FRAME * 2;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;

#if !PLC_DISABLED
    if (g722_data->plc_enabled)
	pjmedia_plc_save(g722_data->plc, (pj_int16_t*)output->buf);
#endif

    TRACE_((THIS_FILE, "G722 decode done"));
    return PJ_SUCCESS;
}


#if !PLC_DISABLED
/*
 * Recover lost frame.
 */
static pj_status_t  g722_codec_recover(pjmedia_codec *codec,
				       unsigned output_buf_len,
				       struct pjmedia_frame *output)
{
    struct g722_data *g722_data = (struct g722_data*)codec->codec_data;

    PJ_ASSERT_RETURN(g722_data->plc_enabled, PJ_EINVALIDOP);

    PJ_ASSERT_RETURN(output_buf_len >= SAMPLES_PER_FRAME * 2, 
                     PJMEDIA_CODEC_EPCMTOOSHORT);

    pjmedia_plc_generate(g722_data->plc, (pj_int16_t*)output->buf);

    output->size = SAMPLES_PER_FRAME * 2;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    
    return PJ_SUCCESS;
}
#endif

#endif // PJMEDIA_HAS_G722_CODEC

