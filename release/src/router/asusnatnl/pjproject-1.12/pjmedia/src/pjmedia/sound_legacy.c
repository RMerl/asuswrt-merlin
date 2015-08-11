/* $Id: sound_legacy.c 3553 2011-05-05 06:14:19Z nanang $ */
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

/*
 * This is implementation of legacy sound device API, for applications
 * that still use the old/deprecated sound device API. This implementation
 * uses the new Audio Device API.
 *
 * Please see http://trac.pjsip.org/repos/wiki/Audio_Dev_API for more
 * information.
 */

#include <pjmedia/sound.h>
#include <pjmedia-audiodev/errno.h>
#include <pj/assert.h>

#if PJMEDIA_HAS_LEGACY_SOUND_API

static struct legacy_subsys
{
    pjmedia_snd_dev_info     info[4];
    unsigned		     info_counter;
    unsigned		     user_rec_latency;
    unsigned		     user_play_latency;
} g_sys;

struct pjmedia_snd_stream
{
    pj_pool_t		*pool;
    pjmedia_aud_stream	*aud_strm;
    pjmedia_snd_rec_cb	 user_rec_cb;
    pjmedia_snd_play_cb  user_play_cb;
    void		*user_user_data;
};

PJ_DEF(pj_status_t) pjmedia_snd_init(pj_pool_factory *factory)
{
    return pjmedia_aud_subsys_init(factory);
}

PJ_DEF(pj_status_t) pjmedia_snd_deinit()
{
    return pjmedia_aud_subsys_shutdown();
}

PJ_DEF(int) pjmedia_snd_get_dev_count()
{
    return pjmedia_aud_dev_count();
}

PJ_DEF(const pjmedia_snd_dev_info*) pjmedia_snd_get_dev_info(unsigned index)
{
    pjmedia_snd_dev_info *oi = &g_sys.info[g_sys.info_counter];
    pjmedia_aud_dev_info di;

    g_sys.info_counter = (g_sys.info_counter+1) % PJ_ARRAY_SIZE(g_sys.info);

    if (pjmedia_aud_dev_get_info(index, &di) != PJ_SUCCESS)
	return NULL;

    pj_bzero(oi, sizeof(*oi));
    pj_ansi_strncpy(oi->name, di.name, sizeof(oi->name));
    oi->name[sizeof(oi->name)-1] = '\0';
    oi->input_count = di.input_count;
    oi->output_count = di.output_count;
    oi->default_samples_per_sec = di.default_samples_per_sec;

    return oi;
}


static pj_status_t snd_play_cb(void *user_data,
			       pjmedia_frame *frame)
{
    pjmedia_snd_stream *strm = (pjmedia_snd_stream*)user_data;

    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
    return strm->user_play_cb(strm->user_user_data, 
			      frame->timestamp.u32.lo,
			      frame->buf,
			      frame->size);
}

static pj_status_t snd_rec_cb(void *user_data,
			      pjmedia_frame *frame)
{
    pjmedia_snd_stream *strm = (pjmedia_snd_stream*)user_data;
    return strm->user_rec_cb(strm->user_user_data, 
			     frame->timestamp.u32.lo,
			     frame->buf,
			     frame->size);
}

static pj_status_t open_stream( pjmedia_dir dir,
			        int rec_id,
				int play_id,
				unsigned clock_rate,
				unsigned channel_count,
				unsigned samples_per_frame,
				unsigned bits_per_sample,
				pjmedia_snd_rec_cb rec_cb,
				pjmedia_snd_play_cb play_cb,
				void *user_data,
				pjmedia_snd_stream **p_snd_strm)
{
    pj_pool_t *pool;
    pjmedia_snd_stream *snd_strm;
    pjmedia_aud_param param;
    pj_status_t status;

    /* Initialize parameters */
    if (dir & PJMEDIA_DIR_CAPTURE) {
	status = pjmedia_aud_dev_default_param(rec_id, &param);
    } else {
	status = pjmedia_aud_dev_default_param(play_id, &param);
    }
    if (status != PJ_SUCCESS)
	return status;

    param.dir = dir;
    param.rec_id = rec_id;
    param.play_id = play_id;
    param.clock_rate = clock_rate;
    param.channel_count = channel_count;
    param.samples_per_frame = samples_per_frame;
    param.bits_per_sample = bits_per_sample;

    /* Latencies setting */
    if ((dir & PJMEDIA_DIR_CAPTURE) && g_sys.user_rec_latency) {
	param.flags |= PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY;
	param.input_latency_ms = g_sys.user_rec_latency;
    }
    if ((dir & PJMEDIA_DIR_PLAYBACK) && g_sys.user_play_latency) {
	param.flags |= PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY;
	param.output_latency_ms = g_sys.user_play_latency;
    }

    /* Create sound wrapper */
    pool = pj_pool_create(pjmedia_aud_subsys_get_pool_factory(),
			  "legacy-snd", 512, 512, NULL);
    snd_strm = PJ_POOL_ZALLOC_T(pool, pjmedia_snd_stream);
    snd_strm->pool = pool;
    snd_strm->user_rec_cb = rec_cb;
    snd_strm->user_play_cb = play_cb;
    snd_strm->user_user_data = user_data;

    /* Create the stream */
    status = pjmedia_aud_stream_create(&param, &snd_rec_cb,
				       &snd_play_cb, snd_strm,
				       &snd_strm->aud_strm);
    if (status != PJ_SUCCESS) {
	pj_pool_release(pool);
	return status;
    }

    *p_snd_strm = snd_strm;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_snd_open_rec( int index,
					  unsigned clock_rate,
					  unsigned channel_count,
					  unsigned samples_per_frame,
					  unsigned bits_per_sample,
					  pjmedia_snd_rec_cb rec_cb,
					  void *user_data,
					  pjmedia_snd_stream **p_snd_strm)
{
    return open_stream(PJMEDIA_DIR_CAPTURE, index, PJMEDIA_AUD_INVALID_DEV,
		       clock_rate, channel_count, samples_per_frame,
		       bits_per_sample, rec_cb, NULL,
		       user_data, p_snd_strm);
}

PJ_DEF(pj_status_t) pjmedia_snd_open_player( int index,
					unsigned clock_rate,
					unsigned channel_count,
					unsigned samples_per_frame,
					unsigned bits_per_sample,
					pjmedia_snd_play_cb play_cb,
					void *user_data,
					pjmedia_snd_stream **p_snd_strm )
{
    return open_stream(PJMEDIA_DIR_PLAYBACK, PJMEDIA_AUD_INVALID_DEV, index,
		       clock_rate, channel_count, samples_per_frame,
		       bits_per_sample, NULL, play_cb,
		       user_data, p_snd_strm);
}

PJ_DEF(pj_status_t) pjmedia_snd_open( int rec_id,
				      int play_id,
				      unsigned clock_rate,
				      unsigned channel_count,
				      unsigned samples_per_frame,
				      unsigned bits_per_sample,
				      pjmedia_snd_rec_cb rec_cb,
				      pjmedia_snd_play_cb play_cb,
				      void *user_data,
				      pjmedia_snd_stream **p_snd_strm)
{
    return open_stream(PJMEDIA_DIR_CAPTURE_PLAYBACK, rec_id, play_id,
		       clock_rate, channel_count, samples_per_frame,
		       bits_per_sample, rec_cb, play_cb, 
		       user_data, p_snd_strm);
}

PJ_DEF(pj_status_t) pjmedia_snd_stream_start(pjmedia_snd_stream *stream)
{
    return pjmedia_aud_stream_start(stream->aud_strm);
}

PJ_DEF(pj_status_t) pjmedia_snd_stream_stop(pjmedia_snd_stream *stream)
{
    return pjmedia_aud_stream_stop(stream->aud_strm);
}

PJ_DEF(pj_status_t) pjmedia_snd_stream_get_info(pjmedia_snd_stream *strm,
						pjmedia_snd_stream_info *pi)
{
    pjmedia_aud_param param;
    pj_status_t status;

    status = pjmedia_aud_stream_get_param(strm->aud_strm, &param);
    if (status != PJ_SUCCESS)
	return status;

    pj_bzero(pi, sizeof(*pi));
    pi->dir = param.dir;
    pi->play_id = param.play_id;
    pi->rec_id = param.rec_id;
    pi->clock_rate = param.clock_rate;
    pi->channel_count = param.channel_count;
    pi->samples_per_frame = param.samples_per_frame;
    pi->bits_per_sample = param.bits_per_sample;

    if (param.flags & PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY) {
	pi->rec_latency = param.input_latency_ms;
    }
    if (param.flags & PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY) {
	pi->play_latency = param.output_latency_ms;
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_snd_stream_close(pjmedia_snd_stream *stream)
{
    pj_status_t status;

    status = pjmedia_aud_stream_destroy(stream->aud_strm);
    if (status != PJ_SUCCESS)
	return status;

    pj_pool_release(stream->pool);
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_snd_set_latency(unsigned input_latency, 
					    unsigned output_latency)
{
    g_sys.user_rec_latency = input_latency;
    g_sys.user_play_latency = output_latency;
    return PJ_SUCCESS;
}

#endif	/* PJMEDIA_HAS_LEGACY_SOUND_API */

