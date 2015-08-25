/* $Id: legacy_dev.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia-audiodev/audiodev_imp.h>
#include <pjmedia/sound.h>
#include <pj/assert.h>

#if PJMEDIA_AUDIO_DEV_HAS_LEGACY_DEVICE

#define THIS_FILE			"legacy_dev.c"

/* Legacy devices factory */
struct legacy_factory
{
    pjmedia_aud_dev_factory	 base;
    pj_pool_t			*pool;
    pj_pool_factory		*pf;
};


struct legacy_stream
{
    pjmedia_aud_stream	 base;

    pj_pool_t		*pool;
    pjmedia_aud_param    param;
    pjmedia_snd_stream	*snd_strm;
    pjmedia_aud_play_cb	 user_play_cb;
    pjmedia_aud_rec_cb   user_rec_cb;
    void		*user_user_data;
    unsigned		 input_latency;
    unsigned		 output_latency;
};


/* Prototypes */
static pj_status_t factory_init(pjmedia_aud_dev_factory *f);
static pj_status_t factory_destroy(pjmedia_aud_dev_factory *f);
static pj_status_t factory_refresh(pjmedia_aud_dev_factory *f);
static unsigned    factory_get_dev_count(pjmedia_aud_dev_factory *f);
static pj_status_t factory_get_dev_info(pjmedia_aud_dev_factory *f, 
					unsigned index,
					pjmedia_aud_dev_info *info);
static pj_status_t factory_default_param(pjmedia_aud_dev_factory *f,
					 unsigned index,
					 pjmedia_aud_param *param);
static pj_status_t factory_create_stream(pjmedia_aud_dev_factory *f,
					 const pjmedia_aud_param *param,
					 pjmedia_aud_rec_cb rec_cb,
					 pjmedia_aud_play_cb play_cb,
					 void *user_data,
					 pjmedia_aud_stream **p_aud_strm);

static pj_status_t stream_get_param(pjmedia_aud_stream *strm,
				    pjmedia_aud_param *param);
static pj_status_t stream_get_cap(pjmedia_aud_stream *strm,
				  pjmedia_aud_dev_cap cap,
				  void *value);
static pj_status_t stream_set_cap(pjmedia_aud_stream *strm,
				  pjmedia_aud_dev_cap cap,
				  const void *value);
static pj_status_t stream_start(pjmedia_aud_stream *strm);
static pj_status_t stream_stop(pjmedia_aud_stream *strm);
static pj_status_t stream_destroy(pjmedia_aud_stream *strm);


/* Operations */
static pjmedia_aud_dev_factory_op factory_op =
{
    &factory_init,
    &factory_destroy,
    &factory_get_dev_count,
    &factory_get_dev_info,
    &factory_default_param,
    &factory_create_stream,
    &factory_refresh
};

static pjmedia_aud_stream_op stream_op = 
{
    &stream_get_param,
    &stream_get_cap,
    &stream_set_cap,
    &stream_start,
    &stream_stop,
    &stream_destroy
};


/****************************************************************************
 * Factory operations
 */

/*
 * Init legacy audio driver.
 */
pjmedia_aud_dev_factory* pjmedia_legacy_factory(pj_pool_factory *pf)
{
    struct legacy_factory *f;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "legacy-snd", 512, 512, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct legacy_factory);
    f->pf = pf;
    f->pool = pool;
    f->base.op = &factory_op;

    return &f->base;
}


/* API: init factory */
static pj_status_t factory_init(pjmedia_aud_dev_factory *f)
{
    struct legacy_factory *wf = (struct legacy_factory*)f;

    return pjmedia_snd_init(wf->pf);
}

/* API: destroy factory */
static pj_status_t factory_destroy(pjmedia_aud_dev_factory *f)
{
    struct legacy_factory *wf = (struct legacy_factory*)f;
    pj_status_t status;

    status = pjmedia_snd_deinit();

    if (status == PJ_SUCCESS) {
	pj_pool_t *pool = wf->pool;
	wf->pool = NULL;
	pj_pool_release(pool);
    }

    return status;
}

/* API: refresh the list of devices */
static pj_status_t factory_refresh(pjmedia_aud_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return PJ_ENOTSUP;
}

/* API: get number of devices */
static unsigned factory_get_dev_count(pjmedia_aud_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return pjmedia_snd_get_dev_count();
}

/* API: get device info */
static pj_status_t factory_get_dev_info(pjmedia_aud_dev_factory *f, 
					unsigned index,
					pjmedia_aud_dev_info *info)
{
    const pjmedia_snd_dev_info *si = 
	pjmedia_snd_get_dev_info(index);;

    PJ_UNUSED_ARG(f);

    if (si == NULL)
	return PJMEDIA_EAUD_INVDEV;

    pj_bzero(info, sizeof(*info));
    pj_ansi_strncpy(info->name, si->name, sizeof(info->name));
    info->name[sizeof(info->name)-1] = '\0';
    info->input_count = si->input_count;
    info->output_count = si->output_count;
    info->default_samples_per_sec = si->default_samples_per_sec;
    pj_ansi_strcpy(info->driver, "legacy");
    info->caps = PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY | 
		 PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY;

    return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t factory_default_param(pjmedia_aud_dev_factory *f,
					 unsigned index,
					 pjmedia_aud_param *param)
{
    pjmedia_aud_dev_info di;
    pj_status_t status;

    status = factory_get_dev_info(f, index, &di);
    if (status != PJ_SUCCESS)
	return status;

    pj_bzero(param, sizeof(*param));
    if (di.input_count && di.output_count) {
	param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
	param->rec_id = index;
	param->play_id = index;
    } else if (di.input_count) {
	param->dir = PJMEDIA_DIR_CAPTURE;
	param->rec_id = index;
	param->play_id = PJMEDIA_AUD_INVALID_DEV;
    } else if (di.output_count) {
	param->dir = PJMEDIA_DIR_PLAYBACK;
	param->play_id = index;
	param->rec_id = PJMEDIA_AUD_INVALID_DEV;
    } else {
	return PJMEDIA_EAUD_INVDEV;
    }

    param->clock_rate = di.default_samples_per_sec;
    param->channel_count = 1;
    param->samples_per_frame = di.default_samples_per_sec * 20 / 1000;
    param->bits_per_sample = 16;
    param->flags = di.caps;
    param->input_latency_ms = PJMEDIA_SND_DEFAULT_REC_LATENCY;
    param->output_latency_ms = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;

    return PJ_SUCCESS;
}

/* Callback from legacy sound device */
static pj_status_t snd_play_cb(/* in */   void *user_data,
			       /* in */   pj_uint32_t timestamp,
			       /* out */  void *output,
			       /* out */  unsigned size)
{
    struct legacy_stream *strm = (struct legacy_stream*)user_data;
    pjmedia_frame frame;
    pj_status_t status;

    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame.buf = output;
    frame.size = size;
    frame.timestamp.u64 = timestamp;

    status = strm->user_play_cb(strm->user_user_data, &frame);
    if (status != PJ_SUCCESS)
	return status;

    if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO) {
	pj_bzero(output, size);
    }

    return PJ_SUCCESS;
}

/* Callback from legacy sound device */
static pj_status_t snd_rec_cb( /* in */   void *user_data,
			       /* in */   pj_uint32_t timestamp,
			       /* in */   void *input,
			       /* in*/    unsigned size)
{
    struct legacy_stream *strm = (struct legacy_stream*)user_data;
    pjmedia_frame frame;
    
    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame.buf = input;
    frame.size = size;
    frame.timestamp.u64 = timestamp;

    return strm->user_rec_cb(strm->user_user_data, &frame);
}

/* API: create stream */
static pj_status_t factory_create_stream(pjmedia_aud_dev_factory *f,
					 const pjmedia_aud_param *param,
					 pjmedia_aud_rec_cb rec_cb,
					 pjmedia_aud_play_cb play_cb,
					 void *user_data,
					 pjmedia_aud_stream **p_aud_strm)
{
    struct legacy_factory *wf = (struct legacy_factory*)f;
    pj_pool_t *pool;
    struct legacy_stream *strm;
    pj_status_t status;

    /* Initialize our stream data */
    pool = pj_pool_create(wf->pf, "legacy-snd", 512, 512, NULL);
    strm = PJ_POOL_ZALLOC_T(pool, struct legacy_stream);
    strm->pool = pool;
    strm->user_rec_cb = rec_cb;
    strm->user_play_cb = play_cb;
    strm->user_user_data = user_data;
    pj_memcpy(&strm->param, param, sizeof(*param));

    /* Set the latency if wanted */
    if (param->dir==PJMEDIA_DIR_CAPTURE_PLAYBACK &&
	param->flags & (PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY |
			PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY))
    {
	PJ_ASSERT_RETURN(param->input_latency_ms &&
			 param->output_latency_ms,
			 PJMEDIA_EAUD_BADLATENCY);

	strm->input_latency = param->input_latency_ms;
	strm->output_latency = param->output_latency_ms;

	status = pjmedia_snd_set_latency(param->input_latency_ms,
					 param->output_latency_ms);
	if (status != PJ_SUCCESS) {
	    pj_pool_release(pool);
	    return status;
	}
    }

    /* Open the stream */
    if (param->dir == PJMEDIA_DIR_CAPTURE) {
	status = pjmedia_snd_open_rec(param->rec_id,
				      param->clock_rate,
				      param->channel_count,
				      param->samples_per_frame,
				      param->bits_per_sample,
				      &snd_rec_cb,
				      strm,
				      &strm->snd_strm);
    } else if (param->dir == PJMEDIA_DIR_PLAYBACK) {
	status = pjmedia_snd_open_player(param->play_id,
					 param->clock_rate,
					 param->channel_count,
					 param->samples_per_frame,
					 param->bits_per_sample,
					 &snd_play_cb,
					 strm,
					 &strm->snd_strm);

    } else if (param->dir == PJMEDIA_DIR_CAPTURE_PLAYBACK) {
	status = pjmedia_snd_open(param->rec_id,
				  param->play_id,
				  param->clock_rate,
				  param->channel_count,
				  param->samples_per_frame,
				  param->bits_per_sample,
				  &snd_rec_cb,
				  &snd_play_cb,
				  strm,
				  &strm->snd_strm);
    } else {
	pj_assert(!"Invalid direction!");
	return PJ_EINVAL;
    }

    if (status != PJ_SUCCESS) {
	pj_pool_release(pool);
	return status;
    }

    *p_aud_strm = &strm->base;
    return PJ_SUCCESS;
}

/* API: Get stream info. */
static pj_status_t stream_get_param(pjmedia_aud_stream *s,
				    pjmedia_aud_param *pi)
{
    struct legacy_stream *strm = (struct legacy_stream*)s;
    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

    pj_memcpy(pi, &strm->param, sizeof(*pi));

    if (strm->input_latency) {
	pi->flags |= PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY;
	pi->input_latency_ms = strm->input_latency;
    } else {
	pi->flags &= ~PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY;
    }

    if (strm->output_latency) {
	pi->flags |= PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY;
	pi->output_latency_ms = strm->output_latency;
    } else {
	pi->flags &= ~PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY;
    }

    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t stream_get_cap(pjmedia_aud_stream *s,
				  pjmedia_aud_dev_cap cap,
				  void *pval)
{
    struct legacy_stream *strm = (struct legacy_stream*)s;

    PJ_ASSERT_RETURN(strm && pval, PJ_EINVAL);

    if (cap==PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY && 
	(strm->param.dir & PJMEDIA_DIR_CAPTURE)) 
    {
	/* Recording latency */
	if (strm->input_latency) {
	    *(unsigned*)pval = strm->input_latency;
	    return PJ_SUCCESS;
	} else {
	    return PJMEDIA_EAUD_INVCAP;
	}

    } else if (cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY  && 
	       (strm->param.dir & PJMEDIA_DIR_PLAYBACK))
    {
	/* Playback latency */
	if (strm->output_latency) {
	    *(unsigned*)pval = strm->output_latency;
	    return PJ_SUCCESS;
	} else {
	    return PJMEDIA_EAUD_INVCAP;
	}
    } else {
	return PJMEDIA_EAUD_INVCAP;
    }
}

/* API: set capability */
static pj_status_t stream_set_cap(pjmedia_aud_stream *s,
				  pjmedia_aud_dev_cap cap,
				  const void *pval)
{
    PJ_UNUSED_ARG(s);
    PJ_UNUSED_ARG(cap);
    PJ_UNUSED_ARG(pval);
    return PJMEDIA_EAUD_INVCAP;
}

/* API: Start stream. */
static pj_status_t stream_start(pjmedia_aud_stream *s)
{
    struct legacy_stream *strm = (struct legacy_stream*)s;
    return pjmedia_snd_stream_start(strm->snd_strm);
}

/* API: Stop stream. */
static pj_status_t stream_stop(pjmedia_aud_stream *s)
{
    struct legacy_stream *strm = (struct legacy_stream*)s;
    return pjmedia_snd_stream_stop(strm->snd_strm);
}


/* API: Destroy stream. */
static pj_status_t stream_destroy(pjmedia_aud_stream *s)
{
    struct legacy_stream *strm = (struct legacy_stream*)s;
    pj_status_t status;

    status = pjmedia_snd_stream_close(strm->snd_strm);

    if (status == PJ_SUCCESS) {
	pj_pool_t *pool = strm->pool;

	strm->pool = NULL;
	pj_pool_release(pool);
    }

    return status;
}

#endif	/* PJMEDIA_AUDIO_DEV_HAS_LEGACY_DEVICE */

