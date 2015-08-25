/* $Id: g711.c 4387 2013-02-27 10:16:08Z ming $ */
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
/* This file contains file from Sun Microsystems, Inc, with the complete 
 * notice in the second half of this file.
 */
#include <pjmedia/g711.h>
#include <pjmedia/codec.h>
#include <pjmedia/alaw_ulaw.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/errno.h>
#include <pjmedia/port.h>
#include <pjmedia/plc.h>
#include <pjmedia/silencedet.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/assert.h>

#if defined(PJMEDIA_HAS_G711_CODEC) && PJMEDIA_HAS_G711_CODEC!=0

/* We removed PLC in 0.6 (and re-enabled it again in 0.9!) */
#define PLC_DISABLED	0


#define G711_BPS	    64000
#define G711_CODEC_CNT	    0	/* number of codec to preallocate in memory */
#define PTIME		    10	/* basic frame size is 10 msec	    */
#define FRAME_SIZE	    (8000 * PTIME / 1000)   /* 80 bytes	    */
#define SAMPLES_PER_FRAME   (8000 * PTIME / 1000)   /* 80 samples   */

/* Prototypes for G711 factory */
static pj_status_t g711_test_alloc( pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id );
static pj_status_t g711_default_attr( pjmedia_codec_factory *factory, 
				      const pjmedia_codec_info *id, 
				      pjmedia_codec_param *attr );
static pj_status_t g711_enum_codecs (pjmedia_codec_factory *factory, 
				     unsigned *count, 
				     pjmedia_codec_info codecs[]);
static pj_status_t g711_alloc_codec( pjmedia_codec_factory *factory, 
				     const pjmedia_codec_info *id, 
				     pjmedia_codec **p_codec);
static pj_status_t g711_dealloc_codec( pjmedia_codec_factory *factory, 
				       pjmedia_codec *codec );

/* Prototypes for G711 implementation. */
static pj_status_t  g711_init( pjmedia_codec *codec, 
			       pj_pool_t *pool );
static pj_status_t  g711_open( pjmedia_codec *codec, 
			       pjmedia_codec_param *attr );
static pj_status_t  g711_close( pjmedia_codec *codec );
static pj_status_t  g711_modify(pjmedia_codec *codec, 
			        const pjmedia_codec_param *attr );
static pj_status_t  g711_parse(pjmedia_codec *codec,
			       void *pkt,
			       pj_size_t pkt_size,
			       const pj_timestamp *timestamp,
			       unsigned *frame_cnt,
			       pjmedia_frame frames[]);
static pj_status_t  g711_encode( pjmedia_codec *codec, 
				 const struct pjmedia_frame *input,
				 unsigned output_buf_len, 
				 struct pjmedia_frame *output);
static pj_status_t  g711_decode( pjmedia_codec *codec, 
				 const struct pjmedia_frame *input,
				 unsigned output_buf_len, 
				 struct pjmedia_frame *output);
#if !PLC_DISABLED
static pj_status_t  g711_recover( pjmedia_codec *codec,
				  unsigned output_buf_len,
				  struct pjmedia_frame *output);
#endif

/* Definition for G711 codec operations. */
static pjmedia_codec_op g711_op = 
{
    &g711_init,
    &g711_open,
    &g711_close,
    &g711_modify,
    &g711_parse,
    &g711_encode,
    &g711_decode,
#if !PLC_DISABLED
    &g711_recover
#else
    NULL
#endif
};

/* Definition for G711 codec factory operations. */
static pjmedia_codec_factory_op g711_factory_op =
{
    &g711_test_alloc,
    &g711_default_attr,
    &g711_enum_codecs,
    &g711_alloc_codec,
    &g711_dealloc_codec
};

/* G711 factory private data */
static struct g711_factory
{
    pjmedia_codec_factory	base;
    pjmedia_endpt	       *endpt;
    pj_pool_t		       *pool;
    pj_mutex_t		       *mutex;
    pjmedia_codec		codec_list;
} g711_factory;

/* G711 codec private data. */
struct g711_private
{
    unsigned		 pt;
#if !PLC_DISABLED
    pj_bool_t		 plc_enabled;
    pjmedia_plc		*plc;
#endif
    pj_bool_t		 vad_enabled;
    pjmedia_silence_det *vad;
    pj_timestamp	 last_tx;
};


PJ_DEF(pj_status_t) pjmedia_codec_g711_init(pjmedia_endpt *endpt)
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (g711_factory.endpt != NULL) {
	/* Already initialized. */
	return PJ_SUCCESS;
    }

    /* Init factory */
    g711_factory.base.op = &g711_factory_op;
    g711_factory.base.factory_data = NULL;
    g711_factory.endpt = endpt;

    pj_list_init(&g711_factory.codec_list);

    /* Create pool */
    g711_factory.pool = pjmedia_endpt_create_pool(endpt, "g711", 4000, 4000);
    if (!g711_factory.pool)
	return PJ_ENOMEM;

    /* Create mutex. */
    status = pj_mutex_create_simple(g711_factory.pool, "g611", 
				    &g711_factory.mutex);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
    if (!codec_mgr) {
	return PJ_EINVALIDOP;
    }

    /* Register codec factory to endpoint. */
    status = pjmedia_codec_mgr_register_factory(codec_mgr, 
						&g711_factory.base);
    if (status != PJ_SUCCESS)
	return status;


    return PJ_SUCCESS;

on_error:
    if (g711_factory.mutex) {
	pj_mutex_destroy(g711_factory.mutex);
	g711_factory.mutex = NULL;
    }
    if (g711_factory.pool) {
	pj_pool_release(g711_factory.pool);
	g711_factory.pool = NULL;
    }
    return status;
}

PJ_DEF(pj_status_t) pjmedia_codec_g711_deinit(void)
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (g711_factory.endpt == NULL) {
	/* Not registered. */
	return PJ_SUCCESS;
    }

    /* Lock mutex. */
    pj_mutex_lock(g711_factory.mutex);

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(g711_factory.endpt);
    if (!codec_mgr) {
	g711_factory.endpt = NULL;
	pj_mutex_unlock(g711_factory.mutex);
	return PJ_EINVALIDOP;
    }

    /* Unregister G711 codec factory. */
    status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
						  &g711_factory.base);
    g711_factory.endpt = NULL;

    /* Destroy mutex. */
    pj_mutex_destroy(g711_factory.mutex);
    g711_factory.mutex = NULL;


    /* Release pool. */
    pj_pool_release(g711_factory.pool);
    g711_factory.pool = NULL;


    return status;
}

static pj_status_t g711_test_alloc(pjmedia_codec_factory *factory, 
				   const pjmedia_codec_info *id )
{
    PJ_UNUSED_ARG(factory);

    /* It's sufficient to check payload type only. */
    return (id->pt==PJMEDIA_RTP_PT_PCMU || id->pt==PJMEDIA_RTP_PT_PCMA)? 0:-1;
}

static pj_status_t g711_default_attr (pjmedia_codec_factory *factory, 
				      const pjmedia_codec_info *id, 
				      pjmedia_codec_param *attr )
{
    PJ_UNUSED_ARG(factory);

    pj_bzero(attr, sizeof(pjmedia_codec_param));
    attr->info.clock_rate = 8000;
    attr->info.channel_cnt = 1;
    attr->info.avg_bps = G711_BPS;
    attr->info.max_bps = G711_BPS;
    attr->info.pcm_bits_per_sample = 16;
    attr->info.frm_ptime = PTIME;
    attr->info.pt = (pj_uint8_t)id->pt;

    /* Set default frames per packet to 2 (or 20ms) */
    attr->setting.frm_per_pkt = 2;

#if !PLC_DISABLED
    /* Enable plc by default. */
    attr->setting.plc = 1;
#endif

    /* Enable VAD by default. */
    attr->setting.vad = 1;

    /* Default all other flag bits disabled. */

    return PJ_SUCCESS;
}

static pj_status_t g711_enum_codecs(pjmedia_codec_factory *factory, 
				    unsigned *max_count, 
				    pjmedia_codec_info codecs[])
{
    unsigned count = 0;

    PJ_UNUSED_ARG(factory);

    if (count < *max_count) {
	codecs[count].type = PJMEDIA_TYPE_AUDIO;
	codecs[count].pt = PJMEDIA_RTP_PT_PCMU;
	codecs[count].encoding_name = pj_str("PCMU");
	codecs[count].clock_rate = 8000;
	codecs[count].channel_cnt = 1;
	++count;
    }
    if (count < *max_count) {
	codecs[count].type = PJMEDIA_TYPE_AUDIO;
	codecs[count].pt = PJMEDIA_RTP_PT_PCMA;
	codecs[count].encoding_name = pj_str("PCMA");
	codecs[count].clock_rate = 8000;
	codecs[count].channel_cnt = 1;
	++count;
    }

    *max_count = count;

    return PJ_SUCCESS;
}

static pj_status_t g711_alloc_codec( pjmedia_codec_factory *factory, 
				     const pjmedia_codec_info *id,
				     pjmedia_codec **p_codec)
{
    pjmedia_codec *codec = NULL;
    pj_status_t status;

    PJ_ASSERT_RETURN(factory==&g711_factory.base, PJ_EINVAL);

    /* Lock mutex. */
    pj_mutex_lock(g711_factory.mutex);

    /* Allocate new codec if no more is available */
    if (pj_list_empty(&g711_factory.codec_list)) {
	struct g711_private *codec_priv;

	codec = PJ_POOL_ALLOC_T(g711_factory.pool, pjmedia_codec);
	codec_priv = PJ_POOL_ZALLOC_T(g711_factory.pool, struct g711_private);
	if (!codec || !codec_priv) {
	    pj_mutex_unlock(g711_factory.mutex);
	    return PJ_ENOMEM;
	}

	/* Set the payload type */
	codec_priv->pt = id->pt;

#if !PLC_DISABLED
	/* Create PLC, always with 10ms ptime */
	status = pjmedia_plc_create(g711_factory.pool, 8000,
				    SAMPLES_PER_FRAME,
				    0, &codec_priv->plc);
	if (status != PJ_SUCCESS) {
	    pj_mutex_unlock(g711_factory.mutex);
	    return status;
	}
#endif

	/* Create VAD */
	status = pjmedia_silence_det_create(g711_factory.pool,
					    8000, SAMPLES_PER_FRAME,
					    &codec_priv->vad);
	if (status != PJ_SUCCESS) {
	    pj_mutex_unlock(g711_factory.mutex);
	    return status;
	}

	codec->factory = factory;
	codec->op = &g711_op;
	codec->codec_data = codec_priv;
    } else {
	codec = g711_factory.codec_list.next;
	pj_list_erase(codec);
    }

    /* Zero the list, for error detection in g711_dealloc_codec */
    codec->next = codec->prev = NULL;

    *p_codec = codec;

    /* Unlock mutex. */
    pj_mutex_unlock(g711_factory.mutex);

    return PJ_SUCCESS;
}

static pj_status_t g711_dealloc_codec(pjmedia_codec_factory *factory, 
				      pjmedia_codec *codec )
{
    struct g711_private *priv = (struct g711_private*) codec->codec_data;
    int i = 0;

    PJ_ASSERT_RETURN(factory==&g711_factory.base, PJ_EINVAL);

    /* Check that this node has not been deallocated before */
    pj_assert (codec->next==NULL && codec->prev==NULL);
    if (codec->next!=NULL || codec->prev!=NULL) {
	return PJ_EINVALIDOP;
    }

#if !PLC_DISABLED
    /* Clear left samples in the PLC, since codec+plc will be reused
     * next time.
     */
    for (i=0; i<2; ++i) {
	pj_int16_t frame[SAMPLES_PER_FRAME];
	pjmedia_zero_samples(frame, PJ_ARRAY_SIZE(frame));
	pjmedia_plc_save(priv->plc, frame);
    }
#else
    PJ_UNUSED_ARG(i);
    PJ_UNUSED_ARG(priv);
#endif

    /* Lock mutex. */
    pj_mutex_lock(g711_factory.mutex);

    /* Insert at the back of the list */
    pj_list_insert_before(&g711_factory.codec_list, codec);

    /* Unlock mutex. */
    pj_mutex_unlock(g711_factory.mutex);

    return PJ_SUCCESS;
}

static pj_status_t g711_init( pjmedia_codec *codec, pj_pool_t *pool )
{
    /* There's nothing to do here really */
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);

    return PJ_SUCCESS;
}

static pj_status_t g711_open(pjmedia_codec *codec, 
			     pjmedia_codec_param *attr )
{
    struct g711_private *priv = (struct g711_private*) codec->codec_data;
    priv->pt = attr->info.pt;
#if !PLC_DISABLED
    priv->plc_enabled = (attr->setting.plc != 0);
#endif
    priv->vad_enabled = (attr->setting.vad != 0);
    return PJ_SUCCESS;
}

static pj_status_t g711_close( pjmedia_codec *codec )
{
    PJ_UNUSED_ARG(codec);
    /* Nothing to do */
    return PJ_SUCCESS;
}

static pj_status_t  g711_modify(pjmedia_codec *codec, 
			        const pjmedia_codec_param *attr )
{
    struct g711_private *priv = (struct g711_private*) codec->codec_data;

    if (attr->info.pt != priv->pt)
	return PJMEDIA_EINVALIDPT;

#if !PLC_DISABLED
    priv->plc_enabled = (attr->setting.plc != 0);
#endif
    priv->vad_enabled = (attr->setting.vad != 0);

    return PJ_SUCCESS;
}

static pj_status_t  g711_parse( pjmedia_codec *codec,
				void *pkt,
				pj_size_t pkt_size,
				const pj_timestamp *ts,
				unsigned *frame_cnt,
				pjmedia_frame frames[])
{
    unsigned count = 0;

    PJ_UNUSED_ARG(codec);

    PJ_ASSERT_RETURN(ts && frame_cnt && frames, PJ_EINVAL);

    while (pkt_size >= FRAME_SIZE && count < *frame_cnt) {
	frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
	frames[count].buf = pkt;
	frames[count].size = FRAME_SIZE;
	frames[count].timestamp.u64 = ts->u64 + SAMPLES_PER_FRAME * count;

	pkt = ((char*)pkt) + FRAME_SIZE;
	pkt_size -= FRAME_SIZE;

	++count;
    }

    *frame_cnt = count;
    return PJ_SUCCESS;
}

static pj_status_t  g711_encode(pjmedia_codec *codec, 
				const struct pjmedia_frame *input,
				unsigned output_buf_len, 
				struct pjmedia_frame *output)
{
    pj_int16_t *samples = (pj_int16_t*) input->buf;
    struct g711_private *priv = (struct g711_private*) codec->codec_data;

    /* Check output buffer length */
    if (output_buf_len < (input->size >> 1))
	return PJMEDIA_CODEC_EFRMTOOSHORT;

    /* Detect silence if VAD is enabled */
    if (priv->vad_enabled) {
	pj_bool_t is_silence;
	pj_int32_t silence_period;

	silence_period = pj_timestamp_diff32(&priv->last_tx,
					     &input->timestamp);

	is_silence = pjmedia_silence_det_detect(priv->vad, 
						(const pj_int16_t*) input->buf, 
						(input->size >> 1), NULL);
	if (is_silence && 
	    (PJMEDIA_CODEC_MAX_SILENCE_PERIOD == -1 ||
	     silence_period < PJMEDIA_CODEC_MAX_SILENCE_PERIOD*8000/1000))
	{
	    output->type = PJMEDIA_FRAME_TYPE_NONE;
	    output->buf = NULL;
	    output->size = 0;
	    output->timestamp = input->timestamp;
	    return PJ_SUCCESS;
	} else {
	    priv->last_tx = input->timestamp;
	}
    }

    /* Encode */
    if (priv->pt == PJMEDIA_RTP_PT_PCMA) {
	unsigned i, n;
	pj_uint8_t *dst = (pj_uint8_t*) output->buf;

	n = (input->size >> 1);
	for (i=0; i!=n; ++i, ++dst) {
	    *dst = pjmedia_linear2alaw(samples[i]);
	}
    } else if (priv->pt == PJMEDIA_RTP_PT_PCMU) {
	unsigned i, n;
	pj_uint8_t *dst = (pj_uint8_t*) output->buf;

	n = (input->size >> 1);
	for (i=0; i!=n; ++i, ++dst) {
	    *dst = pjmedia_linear2ulaw(samples[i]);
	}

    } else {
	return PJMEDIA_EINVALIDPT;
    }

    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->size = (input->size >> 1);
    output->timestamp = input->timestamp;

    return PJ_SUCCESS;
}

static pj_status_t  g711_decode(pjmedia_codec *codec, 
				const struct pjmedia_frame *input,
				unsigned output_buf_len, 
				struct pjmedia_frame *output)
{
    struct g711_private *priv = (struct g711_private*) codec->codec_data;

    /* Check output buffer length */
    PJ_ASSERT_RETURN(output_buf_len >= (input->size << 1),
		     PJMEDIA_CODEC_EPCMTOOSHORT);

    /* Input buffer MUST have exactly 80 bytes long */
    PJ_ASSERT_RETURN(input->size == FRAME_SIZE, 
		     PJMEDIA_CODEC_EFRMINLEN);

    /* Decode */
    if (priv->pt == PJMEDIA_RTP_PT_PCMA) {
	unsigned i;
	pj_uint8_t *src = (pj_uint8_t*) input->buf;
	pj_uint16_t *dst = (pj_uint16_t*) output->buf;

	for (i=0; i!=input->size; ++i) {
	    *dst++ = (pj_uint16_t) pjmedia_alaw2linear(*src++);
	}
    } else if (priv->pt == PJMEDIA_RTP_PT_PCMU) {
	unsigned i;
	pj_uint8_t *src = (pj_uint8_t*) input->buf;
	pj_uint16_t *dst = (pj_uint16_t*) output->buf;

	for (i=0; i!=input->size; ++i) {
	    *dst++ = (pj_uint16_t) pjmedia_ulaw2linear(*src++);
	}

    } else {
	return PJMEDIA_EINVALIDPT;
    }

    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->size = (input->size << 1);
    output->timestamp = input->timestamp;

#if !PLC_DISABLED
    if (priv->plc_enabled)
	pjmedia_plc_save( priv->plc, (pj_int16_t*)output->buf);
#endif

    return PJ_SUCCESS;
}

#if !PLC_DISABLED
static pj_status_t  g711_recover( pjmedia_codec *codec,
				  unsigned output_buf_len,
				  struct pjmedia_frame *output)
{
    struct g711_private *priv = (struct g711_private*) codec->codec_data;

    if (!priv->plc_enabled)
	return PJ_EINVALIDOP;

    PJ_ASSERT_RETURN(output_buf_len >= SAMPLES_PER_FRAME * 2, 
		     PJMEDIA_CODEC_EPCMTOOSHORT);

    pjmedia_plc_generate(priv->plc, (pj_int16_t*)output->buf);
    output->size = SAMPLES_PER_FRAME * 2;

    return PJ_SUCCESS;
}
#endif

#endif	/* PJMEDIA_HAS_G711_CODEC */



