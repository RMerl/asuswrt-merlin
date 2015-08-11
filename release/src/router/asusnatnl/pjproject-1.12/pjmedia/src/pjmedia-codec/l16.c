/* $Id: l16.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia-codec/l16.h>
#include <pjmedia/codec.h>
#include <pjmedia/errno.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/plc.h>
#include <pjmedia/silencedet.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/sock.h>
#include <pj/string.h>


/*
 * Only build this file if PJMEDIA_HAS_L16_CODEC != 0
 */
#if defined(PJMEDIA_HAS_L16_CODEC) && PJMEDIA_HAS_L16_CODEC != 0

#define PLC_DISABLED	0


static const pj_str_t STR_L16 = { "L16", 3 };

/* To keep frame size below 1400 MTU, set ptime to 10ms for
 * sampling rate > 35 KHz
 */
#define GET_PTIME(clock_rate)	((pj_uint16_t)(clock_rate > 35000 ? 10 : 20))


/* Prototypes for L16 factory */
static pj_status_t l16_test_alloc( pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id );
static pj_status_t l16_default_attr( pjmedia_codec_factory *factory, 
				      const pjmedia_codec_info *id, 
				      pjmedia_codec_param *attr );
static pj_status_t l16_enum_codecs (pjmedia_codec_factory *factory, 
				     unsigned *count, 
				     pjmedia_codec_info codecs[]);
static pj_status_t l16_alloc_codec( pjmedia_codec_factory *factory, 
				     const pjmedia_codec_info *id, 
				     pjmedia_codec **p_codec);
static pj_status_t l16_dealloc_codec( pjmedia_codec_factory *factory, 
				       pjmedia_codec *codec );

/* Prototypes for L16 implementation. */
static pj_status_t  l16_init( pjmedia_codec *codec, 
			       pj_pool_t *pool );
static pj_status_t  l16_open( pjmedia_codec *codec, 
			      pjmedia_codec_param *attr );
static pj_status_t  l16_close( pjmedia_codec *codec );
static pj_status_t  l16_modify(pjmedia_codec *codec, 
			       const pjmedia_codec_param *attr );
static pj_status_t  l16_parse(pjmedia_codec *codec,
			      void *pkt,
			      pj_size_t pkt_size,
			      const pj_timestamp *ts,
			      unsigned *frame_cnt,
			      pjmedia_frame frames[]);
static pj_status_t  l16_encode( pjmedia_codec *codec, 
				 const struct pjmedia_frame *input,
				 unsigned output_buf_len, 
				 struct pjmedia_frame *output);
static pj_status_t  l16_decode( pjmedia_codec *codec, 
				 const struct pjmedia_frame *input,
				 unsigned output_buf_len, 
				 struct pjmedia_frame *output);
#if !PLC_DISABLED
static pj_status_t  l16_recover(pjmedia_codec *codec,
				unsigned output_buf_len,
				struct pjmedia_frame *output);
#endif

/* Definition for L16 codec operations. */
static pjmedia_codec_op l16_op = 
{
    &l16_init,
    &l16_open,
    &l16_close,
    &l16_modify,
    &l16_parse,
    &l16_encode,
    &l16_decode,
#if !PLC_DISABLED
    &l16_recover
#else
    NULL
#endif
};

/* Definition for L16 codec factory operations. */
static pjmedia_codec_factory_op l16_factory_op =
{
    &l16_test_alloc,
    &l16_default_attr,
    &l16_enum_codecs,
    &l16_alloc_codec,
    &l16_dealloc_codec
};

/* L16 factory private data */
static struct l16_factory
{
    pjmedia_codec_factory	base;
    pjmedia_endpt	       *endpt;
    pj_pool_t		       *pool;
    pj_mutex_t		       *mutex;
} l16_factory;


/* L16 codec private data. */
struct l16_data
{
    pj_pool_t		*pool;
    unsigned		 frame_size;    /* Frame size, in bytes */
    unsigned		 clock_rate;    /* Clock rate */

#if !PLC_DISABLED
    pj_bool_t		 plc_enabled;
    pjmedia_plc		*plc;
#endif
    pj_bool_t		 vad_enabled;
    pjmedia_silence_det	*vad;
    pj_timestamp	 last_tx;
};



PJ_DEF(pj_status_t) pjmedia_codec_l16_init(pjmedia_endpt *endpt,
					   unsigned options)
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    PJ_UNUSED_ARG(options);


    if (l16_factory.endpt != NULL) {
	/* Already initialized. */
	return PJ_SUCCESS;
    }

    /* Init factory */
    l16_factory.base.op = &l16_factory_op;
    l16_factory.base.factory_data = NULL;
    l16_factory.endpt = endpt;

    /* Create pool */
    l16_factory.pool = pjmedia_endpt_create_pool(endpt, "l16", 4000, 4000);
    if (!l16_factory.pool)
	return PJ_ENOMEM;

    /* Create mutex. */
    status = pj_mutex_create_simple(l16_factory.pool, "l16", 
				    &l16_factory.mutex);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
    if (!codec_mgr) {
	return PJ_EINVALIDOP;
    }

    /* Register codec factory to endpoint. */
    status = pjmedia_codec_mgr_register_factory(codec_mgr, 
						&l16_factory.base);
    if (status != PJ_SUCCESS)
	return status;


    return PJ_SUCCESS;

on_error:
    if (l16_factory.mutex) {
	pj_mutex_destroy(l16_factory.mutex);
	l16_factory.mutex = NULL;
    }
    if (l16_factory.pool) {
	pj_pool_release(l16_factory.pool);
	l16_factory.pool = NULL;
    }
    return status;
}

PJ_DEF(pj_status_t) pjmedia_codec_l16_deinit(void)
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (l16_factory.endpt == NULL) {
	/* Not registered. */
	return PJ_SUCCESS;
    }

    /* Lock mutex. */
    pj_mutex_lock(l16_factory.mutex);

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(l16_factory.endpt);
    if (!codec_mgr) {
	l16_factory.endpt = NULL;
	pj_mutex_unlock(l16_factory.mutex);
	return PJ_EINVALIDOP;
    }

    /* Unregister L16 codec factory. */
    status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
						  &l16_factory.base);
    l16_factory.endpt = NULL;

    /* Destroy mutex. */
    pj_mutex_destroy(l16_factory.mutex);
    l16_factory.mutex = NULL;


    /* Release pool. */
    pj_pool_release(l16_factory.pool);
    l16_factory.pool = NULL;


    return status;
}

static pj_status_t l16_test_alloc(pjmedia_codec_factory *factory, 
				  const pjmedia_codec_info *id )
{
    PJ_UNUSED_ARG(factory);

    if (pj_stricmp(&id->encoding_name, &STR_L16)==0) {
	/* Match! */
	return PJ_SUCCESS;
    }

    return -1;
}

static pj_status_t l16_default_attr( pjmedia_codec_factory *factory, 
				     const pjmedia_codec_info *id, 
				     pjmedia_codec_param *attr )
{
    PJ_UNUSED_ARG(factory);

    pj_bzero(attr, sizeof(pjmedia_codec_param));
    attr->info.pt = (pj_uint8_t)id->pt;
    attr->info.clock_rate = id->clock_rate;
    attr->info.channel_cnt = id->channel_cnt;
    attr->info.avg_bps = id->clock_rate * id->channel_cnt * 16;
    attr->info.max_bps = attr->info.avg_bps;
    attr->info.pcm_bits_per_sample = 16;

    /* To keep frame size below 1400 MTU, set ptime to 10ms for
     * sampling rate > 35 KHz
     */
    attr->info.frm_ptime = GET_PTIME(id->clock_rate);

    attr->setting.frm_per_pkt = 1;

    attr->setting.vad = 1;
#if !PLC_DISABLED
    attr->setting.plc = 1;
#endif

    return PJ_SUCCESS;
}

static pj_status_t l16_enum_codecs( pjmedia_codec_factory *factory, 
				    unsigned *max_count, 
				    pjmedia_codec_info codecs[])
{
    unsigned count = 0;

    PJ_UNUSED_ARG(factory);

    if (count < *max_count) {
	/* Register 44100Hz 1 channel L16 codec */
	codecs[count].type = PJMEDIA_TYPE_AUDIO;
	codecs[count].pt = PJMEDIA_RTP_PT_L16_1;
	codecs[count].encoding_name = STR_L16;
	codecs[count].clock_rate = 44100;
	codecs[count].channel_cnt = 1;
	++count;
    }

    if (count < *max_count) {
	/* Register 44100Hz 2 channels L16 codec */
	codecs[count].type = PJMEDIA_TYPE_AUDIO;
	codecs[count].pt = PJMEDIA_RTP_PT_L16_2;
	codecs[count].encoding_name = STR_L16;
	codecs[count].clock_rate = 44100;
	codecs[count].channel_cnt = 2;
	++count;
    }

    if (count < *max_count) {
	/* 8KHz mono */
	codecs[count].type = PJMEDIA_TYPE_AUDIO;
	codecs[count].pt = PJMEDIA_RTP_PT_L16_8KHZ_MONO;
	codecs[count].encoding_name = STR_L16;
	codecs[count].clock_rate = 8000;
	codecs[count].channel_cnt = 1;
	++count;
    }

    if (count < *max_count) {
	/* 8KHz stereo */
	codecs[count].type = PJMEDIA_TYPE_AUDIO;
	codecs[count].pt = PJMEDIA_RTP_PT_L16_8KHZ_STEREO;
	codecs[count].encoding_name = STR_L16;
	codecs[count].clock_rate = 8000;
	codecs[count].channel_cnt = 2;
	++count;
    }

// disable some L16 modes
#if 0
    if (count < *max_count) {
	/* 11025 Hz mono */
	codecs[count].type = PJMEDIA_TYPE_AUDIO;
	codecs[count].pt = PJMEDIA_RTP_PT_L16_11KHZ_MONO;
	codecs[count].encoding_name = STR_L16;
	codecs[count].clock_rate = 11025;
	codecs[count].channel_cnt = 1;
	++count;
    }

    if (count < *max_count) {
	/* 11025 Hz stereo */
	codecs[count].type = PJMEDIA_TYPE_AUDIO;
	codecs[count].pt = PJMEDIA_RTP_PT_L16_11KHZ_STEREO;
	codecs[count].encoding_name = STR_L16;
	codecs[count].clock_rate = 11025;
	codecs[count].channel_cnt = 2;
	++count;
    }
#endif

    if (count < *max_count) {
	/* 16000 Hz mono */
	codecs[count].type = PJMEDIA_TYPE_AUDIO;
	codecs[count].pt = PJMEDIA_RTP_PT_L16_16KHZ_MONO;
	codecs[count].encoding_name = STR_L16;
	codecs[count].clock_rate = 16000;
	codecs[count].channel_cnt = 1;
	++count;
    }


    if (count < *max_count) {
	/* 16000 Hz stereo */
	codecs[count].type = PJMEDIA_TYPE_AUDIO;
	codecs[count].pt = PJMEDIA_RTP_PT_L16_16KHZ_STEREO;
	codecs[count].encoding_name = STR_L16;
	codecs[count].clock_rate = 16000;
	codecs[count].channel_cnt = 2;
	++count;
    }

// disable some L16 modes
#if 0
    if (count < *max_count) {
	/* 22050 Hz mono */
	codecs[count].type = PJMEDIA_TYPE_AUDIO;
	codecs[count].pt = PJMEDIA_RTP_PT_L16_22KHZ_MONO;
	codecs[count].encoding_name = STR_L16;
	codecs[count].clock_rate = 22050;
	codecs[count].channel_cnt = 1;
	++count;
    }


    if (count < *max_count) {
	/* 22050 Hz stereo */
	codecs[count].type = PJMEDIA_TYPE_AUDIO;
	codecs[count].pt = PJMEDIA_RTP_PT_L16_22KHZ_STEREO;
	codecs[count].encoding_name = STR_L16;
	codecs[count].clock_rate = 22050;
	codecs[count].channel_cnt = 2;
	++count;
    }

    if (count < *max_count) {
	/* 32000 Hz mono */
	codecs[count].type = PJMEDIA_TYPE_AUDIO;
	codecs[count].pt = PJMEDIA_RTP_PT_L16_32KHZ_MONO;
	codecs[count].encoding_name = STR_L16;
	codecs[count].clock_rate = 32000;
	codecs[count].channel_cnt = 1;
	++count;
    }

    if (count < *max_count) {
	/* 32000 Hz stereo */
	codecs[count].type = PJMEDIA_TYPE_AUDIO;
	codecs[count].pt = PJMEDIA_RTP_PT_L16_32KHZ_STEREO;
	codecs[count].encoding_name = STR_L16;
	codecs[count].clock_rate = 32000;
	codecs[count].channel_cnt = 2;
	++count;
    }

    if (count < *max_count) {
	/* 48KHz mono */
	codecs[count].type = PJMEDIA_TYPE_AUDIO;
	codecs[count].pt = PJMEDIA_RTP_PT_L16_48KHZ_MONO;
	codecs[count].encoding_name = STR_L16;
	codecs[count].clock_rate = 48000;
	codecs[count].channel_cnt = 1;
	++count;
    }

    if (count < *max_count) {
	/* 48KHz stereo */
	codecs[count].type = PJMEDIA_TYPE_AUDIO;
	codecs[count].pt = PJMEDIA_RTP_PT_L16_48KHZ_STEREO;
	codecs[count].encoding_name = STR_L16;
	codecs[count].clock_rate = 48000;
	codecs[count].channel_cnt = 2;
	++count;
    }
#endif


    *max_count = count;

    return PJ_SUCCESS;
}

static pj_status_t l16_alloc_codec( pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id,
				    pjmedia_codec **p_codec)
{
    pjmedia_codec *codec = NULL;
    struct l16_data *data;
    unsigned ptime;
    pj_pool_t *pool;

    pj_status_t status;

    PJ_ASSERT_RETURN(factory==&l16_factory.base, PJ_EINVAL);

    /* Lock mutex. */
    pj_mutex_lock(l16_factory.mutex);


    pool = pjmedia_endpt_create_pool(l16_factory.endpt, "l16", 4000, 4000);
    codec = PJ_POOL_ZALLOC_T(pool, pjmedia_codec);
    codec->codec_data = pj_pool_alloc(pool, sizeof(struct l16_data));
    codec->factory = factory;
    codec->op = &l16_op;

    /* Init private data */
    ptime = GET_PTIME(id->clock_rate);
    data = (struct l16_data*) codec->codec_data;
    data->frame_size = ptime * id->clock_rate * id->channel_cnt * 2 / 1000;
    data->clock_rate = id->clock_rate;
    data->pool = pool;

#if !PLC_DISABLED
    /* Create PLC */
    status = pjmedia_plc_create(pool, id->clock_rate, 
				data->frame_size >> 1, 0, 
				&data->plc);
    if (status != PJ_SUCCESS) {
	pj_mutex_unlock(l16_factory.mutex);
	return status;
    }
#endif

    /* Create silence detector */
    status = pjmedia_silence_det_create(pool, id->clock_rate, 
					data->frame_size >> 1,
					&data->vad);
    if (status != PJ_SUCCESS) {
	pj_mutex_unlock(l16_factory.mutex);
	return status;
    }

    *p_codec = codec;

    /* Unlock mutex. */
    pj_mutex_unlock(l16_factory.mutex);

    return PJ_SUCCESS;
}

static pj_status_t l16_dealloc_codec(pjmedia_codec_factory *factory, 
				     pjmedia_codec *codec )
{
    struct l16_data *data;

    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory==&l16_factory.base, PJ_EINVAL);

    /* Lock mutex. */
    pj_mutex_lock(l16_factory.mutex);

    /* Just release codec data pool */
    data = (struct l16_data*) codec->codec_data;
    pj_assert(data);
    pj_pool_release(data->pool);

    /* Unlock mutex. */
    pj_mutex_unlock(l16_factory.mutex);

    return PJ_SUCCESS;
}

static pj_status_t l16_init( pjmedia_codec *codec, pj_pool_t *pool )
{
    /* There's nothing to do here really */
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);

    return PJ_SUCCESS;
}

static pj_status_t l16_open(pjmedia_codec *codec, 
			    pjmedia_codec_param *attr )
{
    struct l16_data *data = NULL;
    
    PJ_ASSERT_RETURN(codec && codec->codec_data && attr, PJ_EINVAL);

    data = (struct l16_data*) codec->codec_data;

    data->vad_enabled = (attr->setting.vad != 0);
#if !PLC_DISABLED
    data->plc_enabled = (attr->setting.plc != 0);
#endif

    return PJ_SUCCESS;
}

static pj_status_t l16_close( pjmedia_codec *codec )
{
    PJ_UNUSED_ARG(codec);
    /* Nothing to do */
    return PJ_SUCCESS;
}

static pj_status_t  l16_modify(pjmedia_codec *codec, 
			       const pjmedia_codec_param *attr )
{
    struct l16_data *data = (struct l16_data*) codec->codec_data;

    pj_assert(data != NULL);

    data->vad_enabled = (attr->setting.vad != 0);
#if !PLC_DISABLED
    data->plc_enabled = (attr->setting.plc != 0);
#endif

    return PJ_SUCCESS;
}

static pj_status_t  l16_parse( pjmedia_codec *codec,
			       void *pkt,
			       pj_size_t pkt_size,
			       const pj_timestamp *ts,
			       unsigned *frame_cnt,
			       pjmedia_frame frames[])
{
    unsigned count = 0;
    struct l16_data *data = (struct l16_data*) codec->codec_data;

    PJ_UNUSED_ARG(codec);
    PJ_ASSERT_RETURN(frame_cnt, PJ_EINVAL);

    while (pkt_size >= data->frame_size && count < *frame_cnt) {
	frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
	frames[count].buf = pkt;
	frames[count].size = data->frame_size;
	frames[count].timestamp.u64 = ts->u64 + (count * data->frame_size);

	pkt = ((char*)pkt) + data->frame_size;
	pkt_size -= data->frame_size;

	++count;
    }

    *frame_cnt = count;
    return PJ_SUCCESS;
}

static pj_status_t l16_encode(pjmedia_codec *codec, 
			      const struct pjmedia_frame *input,
			      unsigned output_buf_len, 
			      struct pjmedia_frame *output)
{
    struct l16_data *data = (struct l16_data*) codec->codec_data;
    const pj_int16_t *samp = (const pj_int16_t*) input->buf;
    const pj_int16_t *samp_end = samp + input->size/sizeof(pj_int16_t);
    pj_int16_t *samp_out = (pj_int16_t*) output->buf;    

    pj_assert(data && input && output);

    /* Check output buffer length */
    if (output_buf_len < input->size)
	return PJMEDIA_CODEC_EFRMTOOSHORT;

    /* Detect silence */
    if (data->vad_enabled) {
	pj_bool_t is_silence;
	pj_int32_t silence_duration;

	silence_duration = pj_timestamp_diff32(&data->last_tx, 
					       &input->timestamp);

	is_silence = pjmedia_silence_det_detect(data->vad, 
					        (const pj_int16_t*) input->buf,
						(input->size >> 1),
						NULL);
	if (is_silence &&
	    (PJMEDIA_CODEC_MAX_SILENCE_PERIOD == -1 ||
	     silence_duration < PJMEDIA_CODEC_MAX_SILENCE_PERIOD*
			        (int)data->clock_rate/1000))
	{
	    output->type = PJMEDIA_FRAME_TYPE_NONE;
	    output->buf = NULL;
	    output->size = 0;
	    output->timestamp = input->timestamp;
	    return PJ_SUCCESS;
	} else {
	    data->last_tx = input->timestamp;
	}
    }

    /* Encode */
#if defined(PJ_IS_LITTLE_ENDIAN) && PJ_IS_LITTLE_ENDIAN!=0
    while (samp!=samp_end)
	*samp_out++ = pj_htons(*samp++);
#else
    pjmedia_copy_samples(samp_out, samp, input->size >> 1);
#endif


    /* Done */
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->size = input->size;
    output->timestamp = input->timestamp;

    return PJ_SUCCESS;
}

static pj_status_t l16_decode(pjmedia_codec *codec, 
			      const struct pjmedia_frame *input,
			      unsigned output_buf_len, 
			      struct pjmedia_frame *output)
{
    struct l16_data *l16_data = (struct l16_data*) codec->codec_data;
    const pj_int16_t *samp = (const pj_int16_t*) input->buf;
    const pj_int16_t *samp_end = samp + input->size/sizeof(pj_int16_t);
    pj_int16_t *samp_out = (pj_int16_t*) output->buf;    

    pj_assert(l16_data != NULL);
    PJ_ASSERT_RETURN(input && output, PJ_EINVAL);


    /* Check output buffer length */
    if (output_buf_len < input->size)
	return PJMEDIA_CODEC_EPCMTOOSHORT;


    /* Decode */
#if defined(PJ_IS_LITTLE_ENDIAN) && PJ_IS_LITTLE_ENDIAN!=0
    while (samp!=samp_end)
	*samp_out++ = pj_htons(*samp++);
#else
    pjmedia_copy_samples(samp_out, samp, input->size >> 1);
#endif


    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->size = input->size;
    output->timestamp = input->timestamp;

#if !PLC_DISABLED
    if (l16_data->plc_enabled)
	pjmedia_plc_save( l16_data->plc, (pj_int16_t*)output->buf);
#endif

    return PJ_SUCCESS;
}

#if !PLC_DISABLED
/*
 * Recover lost frame.
 */
static pj_status_t  l16_recover(pjmedia_codec *codec,
				      unsigned output_buf_len,
				      struct pjmedia_frame *output)
{
    struct l16_data *data = (struct l16_data*) codec->codec_data;

    PJ_ASSERT_RETURN(data->plc_enabled, PJ_EINVALIDOP);

    PJ_ASSERT_RETURN(output_buf_len >= data->frame_size, 
		     PJMEDIA_CODEC_EPCMTOOSHORT);

    pjmedia_plc_generate(data->plc, (pj_int16_t*)output->buf);
    output->size = data->frame_size;

    return PJ_SUCCESS;
}
#endif

#endif	/* PJMEDIA_HAS_L16_CODEC */


