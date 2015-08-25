/* $Id: sound_port.c 4079 2012-04-24 10:26:07Z ming $ */
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
#include <pjmedia/sound_port.h>
#include <pjmedia/alaw_ulaw.h>
#include <pjmedia/delaybuf.h>
#include <pjmedia/echo.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/rand.h>
#include <pj/string.h>	    /* pj_memset() */

#define AEC_TAIL	    128	    /* default AEC length in ms */
#define AEC_SUSPEND_LIMIT   5	    /* seconds of no activity	*/

#define THIS_FILE	    "sound_port.c"

//#define TEST_OVERFLOW_UNDERFLOW

struct pjmedia_snd_port
{
	int inst_id;

    int			 rec_id;
    int			 play_id;
    pj_uint32_t		 aud_caps;
    pjmedia_aud_param	 aud_param;
    pjmedia_aud_stream	*aud_stream;
    pjmedia_dir		 dir;
    pjmedia_port	*port;

    unsigned		 clock_rate;
    unsigned		 channel_count;
    unsigned		 samples_per_frame;
    unsigned		 bits_per_sample;
    unsigned		 options;
    unsigned		 prm_ec_options;

    /* software ec */
    pjmedia_echo_state	*ec_state;
    unsigned		 ec_options;
    unsigned		 ec_tail_len;
    pj_bool_t		 ec_suspended;
    unsigned		 ec_suspend_count;
    unsigned		 ec_suspend_limit;
};

/*
 * The callback called by sound player when it needs more samples to be
 * played.
 */
static pj_status_t play_cb(void *user_data, pjmedia_frame *frame)
{
    pjmedia_snd_port *snd_port = (pjmedia_snd_port*) user_data;
    pjmedia_port *port;
    const unsigned required_size = frame->size;
    pj_status_t status;

    port = snd_port->port;
    if (port == NULL)
	goto no_frame;

    status = pjmedia_port_get_frame(port, frame);
    if (status != PJ_SUCCESS)
	goto no_frame;

    if (frame->type != PJMEDIA_FRAME_TYPE_AUDIO)
	goto no_frame;

    /* Must supply the required samples */
    pj_assert(frame->size == required_size);

    if (snd_port->ec_state) {
	if (snd_port->ec_suspended) {
	    snd_port->ec_suspended = PJ_FALSE;
	    //pjmedia_echo_state_reset(snd_port->ec_state);
	    PJ_LOG(4,(THIS_FILE, "EC activated"));
	}
	snd_port->ec_suspend_count = 0;
	pjmedia_echo_playback(snd_port->ec_state, (pj_int16_t*)frame->buf);
    }


    return PJ_SUCCESS;

no_frame:
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame->size = required_size;
    pj_bzero(frame->buf, frame->size);

    if (snd_port->ec_state && !snd_port->ec_suspended) {
	++snd_port->ec_suspend_count;
	if (snd_port->ec_suspend_count > snd_port->ec_suspend_limit) {
	    snd_port->ec_suspended = PJ_TRUE;
	    PJ_LOG(4,(THIS_FILE, "EC suspended because of inactivity"));
	}
	if (snd_port->ec_state) {
	    /* To maintain correct delay in EC */
	    pjmedia_echo_playback(snd_port->ec_state, (pj_int16_t*)frame->buf);
	}
    }

    return PJ_SUCCESS;
}


/*
 * The callback called by sound recorder when it has finished capturing a
 * frame.
 */
static pj_status_t rec_cb(void *user_data, pjmedia_frame *frame)
{
    pjmedia_snd_port *snd_port = (pjmedia_snd_port*) user_data;
    pjmedia_port *port;

    port = snd_port->port;
    if (port == NULL)
	return PJ_SUCCESS;

    /* Cancel echo */
    if (snd_port->ec_state && !snd_port->ec_suspended) {
	pjmedia_echo_capture(snd_port->ec_state, (pj_int16_t*) frame->buf, 0);
    }

    pjmedia_port_put_frame(port, frame);

    return PJ_SUCCESS;
}

/*
 * The callback called by sound player when it needs more samples to be
 * played. This version is for non-PCM data.
 */
static pj_status_t play_cb_ext(void *user_data, pjmedia_frame *frame)
{
    pjmedia_snd_port *snd_port = (pjmedia_snd_port*) user_data;
    pjmedia_port *port = snd_port->port;

    if (port == NULL) {
	frame->type = PJMEDIA_FRAME_TYPE_NONE;
	return PJ_SUCCESS;
    }

    pjmedia_port_get_frame(port, frame);

    return PJ_SUCCESS;
}


/*
 * The callback called by sound recorder when it has finished capturing a
 * frame. This version is for non-PCM data.
 */
static pj_status_t rec_cb_ext(void *user_data, pjmedia_frame *frame)
{
    pjmedia_snd_port *snd_port = (pjmedia_snd_port*) user_data;
    pjmedia_port *port;

    port = snd_port->port;
    if (port == NULL)
	return PJ_SUCCESS;

    pjmedia_port_put_frame(port, frame);

    return PJ_SUCCESS;
}

/* Initialize with default values (zero) */
PJ_DEF(void) pjmedia_snd_port_param_default(pjmedia_snd_port_param *prm)
{
    pj_bzero(prm, sizeof(*prm));
}

/*
 * Start the sound stream.
 * This may be called even when the sound stream has already been started.
 */
static pj_status_t start_sound_device( pj_pool_t *pool,
				       pjmedia_snd_port *snd_port )
{
    pjmedia_aud_rec_cb snd_rec_cb;
    pjmedia_aud_play_cb snd_play_cb;
    pjmedia_aud_param param_copy;
    pj_status_t status;

    /* Check if sound has been started. */
    if (snd_port->aud_stream != NULL)
	return PJ_SUCCESS;

    PJ_ASSERT_RETURN(snd_port->dir == PJMEDIA_DIR_CAPTURE ||
		     snd_port->dir == PJMEDIA_DIR_PLAYBACK ||
		     snd_port->dir == PJMEDIA_DIR_CAPTURE_PLAYBACK,
		     PJ_EBUG);

    /* Get device caps */
    if (snd_port->aud_param.dir & PJMEDIA_DIR_CAPTURE) {
	pjmedia_aud_dev_info dev_info;

	status = pjmedia_aud_dev_get_info(snd_port->aud_param.rec_id,
					  &dev_info);
	if (status != PJ_SUCCESS)
	    return status;

	snd_port->aud_caps = dev_info.caps;
    } else {
	snd_port->aud_caps = 0;
    }

    /* Process EC settings */
    pj_memcpy(&param_copy, &snd_port->aud_param, sizeof(param_copy));
    if (param_copy.flags & PJMEDIA_AUD_DEV_CAP_EC) {
	/* EC is wanted */
	if ((snd_port->prm_ec_options & PJMEDIA_ECHO_USE_SW_ECHO) == 0 &&
            snd_port->aud_caps & PJMEDIA_AUD_DEV_CAP_EC)
        {
	    /* Device supports EC */
	    /* Nothing to do */
	} else {
	    /* Application wants to use software EC or device
             * doesn't support EC, remove EC settings from
	     * device parameters
	     */
	    param_copy.flags &= ~(PJMEDIA_AUD_DEV_CAP_EC |
				  PJMEDIA_AUD_DEV_CAP_EC_TAIL);
	}
    }

    /* Use different callback if format is not PCM */
    if (snd_port->aud_param.ext_fmt.id == PJMEDIA_FORMAT_L16) {
	snd_rec_cb = &rec_cb;
	snd_play_cb = &play_cb;
    } else {
	snd_rec_cb = &rec_cb_ext;
	snd_play_cb = &play_cb_ext;
    }

    /* Open the device */
    status = pjmedia_aud_stream_create(&param_copy,
				       snd_rec_cb,
				       snd_play_cb,
				       snd_port,
				       &snd_port->aud_stream);

    if (status != PJ_SUCCESS)
	return status;

    /* Inactivity limit before EC is suspended. */
    snd_port->ec_suspend_limit = AEC_SUSPEND_LIMIT *
				 (snd_port->clock_rate / 
				  snd_port->samples_per_frame);

    /* Create software EC if parameter specifies EC and
     * (app specifically requests software EC or device
     * doesn't support EC). Only do this if the format is PCM!
     */
    if ((snd_port->aud_param.flags & PJMEDIA_AUD_DEV_CAP_EC) &&
	((snd_port->aud_caps & PJMEDIA_AUD_DEV_CAP_EC)==0 ||
         (snd_port->prm_ec_options & PJMEDIA_ECHO_USE_SW_ECHO) != 0) &&
	param_copy.ext_fmt.id == PJMEDIA_FORMAT_PCM)
    {
	if ((snd_port->aud_param.flags & PJMEDIA_AUD_DEV_CAP_EC_TAIL)==0) {
	    snd_port->aud_param.flags |= PJMEDIA_AUD_DEV_CAP_EC_TAIL;
	    snd_port->aud_param.ec_tail_ms = AEC_TAIL;
	    PJ_LOG(4,(THIS_FILE, "AEC tail is set to default %u ms",
				 snd_port->aud_param.ec_tail_ms));
	}
	    
	status = pjmedia_snd_port_set_ec(snd_port, pool, 
					 snd_port->aud_param.ec_tail_ms,
					 snd_port->prm_ec_options);
	if (status != PJ_SUCCESS) {
	    pjmedia_aud_stream_destroy(snd_port->aud_stream);
	    snd_port->aud_stream = NULL;
	    return status;
	}
    }

    /* Start sound stream. */
    if (!(snd_port->options & PJMEDIA_SND_PORT_NO_AUTO_START)) {
	status = pjmedia_aud_stream_start(snd_port->aud_stream);
    }
    if (status != PJ_SUCCESS) {
	pjmedia_aud_stream_destroy(snd_port->aud_stream);
	snd_port->aud_stream = NULL;
	return status;
    }

    return PJ_SUCCESS;
}


/*
 * Stop the sound device.
 * This may be called even when there's no sound device in the port.
 */
static pj_status_t stop_sound_device( pjmedia_snd_port *snd_port )
{
    /* Check if we have sound stream device. */
    if (snd_port->aud_stream) {
	pjmedia_aud_stream_stop(snd_port->aud_stream);
	pjmedia_aud_stream_destroy(snd_port->aud_stream);
	snd_port->aud_stream = NULL;
    }

    /* Destroy AEC */
    if (snd_port->ec_state) {
	pjmedia_echo_destroy(snd_port->ec_state);
	snd_port->ec_state = NULL;
    }

    return PJ_SUCCESS;
}


/*
 * Create bidirectional port.
 */
PJ_DEF(pj_status_t) pjmedia_snd_port_create( pj_pool_t *pool,
					     int rec_id,
					     int play_id,
					     unsigned clock_rate,
					     unsigned channel_count,
					     unsigned samples_per_frame,
					     unsigned bits_per_sample,
					     unsigned options,
					     pjmedia_snd_port **p_port)
{
    pjmedia_snd_port_param param;
    pj_status_t status;

    pjmedia_snd_port_param_default(&param);

    status = pjmedia_aud_dev_default_param(rec_id, &param.base);
    if (status != PJ_SUCCESS)
	return status;

    param.base.dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
    param.base.rec_id = rec_id;
    param.base.play_id = play_id;
    param.base.clock_rate = clock_rate;
    param.base.channel_count = channel_count;
    param.base.samples_per_frame = samples_per_frame;
    param.base.bits_per_sample = bits_per_sample;
    param.options = options;
    param.ec_options = 0;

    return pjmedia_snd_port_create2(pool, &param, p_port);
}

/*
 * Create sound recorder AEC.
 */
PJ_DEF(pj_status_t) pjmedia_snd_port_create_rec( pj_pool_t *pool,
						 int dev_id,
						 unsigned clock_rate,
						 unsigned channel_count,
						 unsigned samples_per_frame,
						 unsigned bits_per_sample,
						 unsigned options,
						 pjmedia_snd_port **p_port)
{
    pjmedia_snd_port_param param;
    pj_status_t status;

    pjmedia_snd_port_param_default(&param);

    status = pjmedia_aud_dev_default_param(dev_id, &param.base);
    if (status != PJ_SUCCESS)
	return status;

    param.base.dir = PJMEDIA_DIR_CAPTURE;
    param.base.rec_id = dev_id;
    param.base.clock_rate = clock_rate;
    param.base.channel_count = channel_count;
    param.base.samples_per_frame = samples_per_frame;
    param.base.bits_per_sample = bits_per_sample;
    param.options = options;
    param.ec_options = 0;

    return pjmedia_snd_port_create2(pool, &param, p_port);
}


/*
 * Create sound player port.
 */
PJ_DEF(pj_status_t) pjmedia_snd_port_create_player( pj_pool_t *pool,
						    int dev_id,
						    unsigned clock_rate,
						    unsigned channel_count,
						    unsigned samples_per_frame,
						    unsigned bits_per_sample,
						    unsigned options,
						    pjmedia_snd_port **p_port)
{
    pjmedia_snd_port_param param;
    pj_status_t status;

    pjmedia_snd_port_param_default(&param);

    status = pjmedia_aud_dev_default_param(dev_id, &param.base);
    if (status != PJ_SUCCESS)
	return status;

    param.base.dir = PJMEDIA_DIR_PLAYBACK;
    param.base.play_id = dev_id;
    param.base.clock_rate = clock_rate;
    param.base.channel_count = channel_count;
    param.base.samples_per_frame = samples_per_frame;
    param.base.bits_per_sample = bits_per_sample;
    param.options = options;
    param.ec_options = 0;

    return pjmedia_snd_port_create2(pool, &param, p_port);
}


/*
 * Create sound port.
 */
PJ_DEF(pj_status_t) pjmedia_snd_port_create2(pj_pool_t *pool,
					     const pjmedia_snd_port_param *prm,
					     pjmedia_snd_port **p_port)
{
    pjmedia_snd_port *snd_port;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && prm && p_port, PJ_EINVAL);

    snd_port = PJ_POOL_ZALLOC_T(pool, pjmedia_snd_port);
    PJ_ASSERT_RETURN(snd_port, PJ_ENOMEM);

    snd_port->dir = prm->base.dir;
    snd_port->rec_id = prm->base.rec_id;
    snd_port->play_id = prm->base.play_id;
    snd_port->clock_rate = prm->base.clock_rate;
    snd_port->channel_count = prm->base.channel_count;
    snd_port->samples_per_frame = prm->base.samples_per_frame;
    snd_port->bits_per_sample = prm->base.bits_per_sample;
    pj_memcpy(&snd_port->aud_param, &prm->base, sizeof(snd_port->aud_param));
    snd_port->options = prm->options;
    snd_port->prm_ec_options = prm->ec_options;
	snd_port->inst_id = pool->factory->inst_id;
    
    /* Start sound device immediately.
     * If there's no port connected, the sound callback will return
     * empty signal.
     */
    status = start_sound_device( pool, snd_port );
    if (status != PJ_SUCCESS) {
	pjmedia_snd_port_destroy(snd_port);
	return status;
    }

    *p_port = snd_port;
    return PJ_SUCCESS;
}


/*
 * Destroy port (also destroys the sound device).
 */
PJ_DEF(pj_status_t) pjmedia_snd_port_destroy(pjmedia_snd_port *snd_port)
{
    PJ_ASSERT_RETURN(snd_port, PJ_EINVAL);

    return stop_sound_device(snd_port);
}


/*
 * Retrieve the sound stream associated by this sound device port.
 */
PJ_DEF(pjmedia_aud_stream*) pjmedia_snd_port_get_snd_stream(
						pjmedia_snd_port *snd_port)
{
    PJ_ASSERT_RETURN(snd_port, NULL);
    return snd_port->aud_stream;
}


/*
 * Change EC settings.
 */
PJ_DEF(pj_status_t) pjmedia_snd_port_set_ec( pjmedia_snd_port *snd_port,
					     pj_pool_t *pool,
					     unsigned tail_ms,
					     unsigned options)
{
    pjmedia_aud_param prm;
    pj_status_t status;

    /* Sound must be opened in full-duplex mode */
    PJ_ASSERT_RETURN(snd_port && 
		     snd_port->dir == PJMEDIA_DIR_CAPTURE_PLAYBACK,
		     PJ_EINVALIDOP);

    /* Determine whether we use device or software EC */
    if ((snd_port->prm_ec_options & PJMEDIA_ECHO_USE_SW_ECHO) == 0 &&
        snd_port->aud_caps & PJMEDIA_AUD_DEV_CAP_EC)
    {
	/* We use device EC */
	pj_bool_t ec_enabled;

	/* Query EC status */
	status = pjmedia_aud_stream_get_cap(snd_port->aud_stream,
					    PJMEDIA_AUD_DEV_CAP_EC,
					    &ec_enabled);
	if (status != PJ_SUCCESS)
	    return status;

	if (tail_ms != 0) {
	    /* Change EC setting */

	    if (!ec_enabled) {
		/* Enable EC first */
		pj_bool_t value = PJ_TRUE;
		status = pjmedia_aud_stream_set_cap(snd_port->aud_stream, 
						    PJMEDIA_AUD_DEV_CAP_EC,
						    &value);
		if (status != PJ_SUCCESS)
		    return status;
	    }

	    if ((snd_port->aud_caps & PJMEDIA_AUD_DEV_CAP_EC_TAIL)==0) {
		/* Device does not support setting EC tail */
		return PJMEDIA_EAUD_INVCAP;
	    }

	    return pjmedia_aud_stream_set_cap(snd_port->aud_stream,
					      PJMEDIA_AUD_DEV_CAP_EC_TAIL,
					      &tail_ms);

	} else if (ec_enabled) {
	    /* Disable EC */
	    pj_bool_t value = PJ_FALSE;
	    return pjmedia_aud_stream_set_cap(snd_port->aud_stream, 
					      PJMEDIA_AUD_DEV_CAP_EC,
					      &value);
	} else {
	    /* Request to disable EC but EC has been disabled */
	    /* Do nothing */
	    return PJ_SUCCESS;
	}

    } else {
	/* We use software EC */

	/* Check if there is change in parameters */
	if (tail_ms==snd_port->ec_tail_len && options==snd_port->ec_options) {
	    PJ_LOG(5,(THIS_FILE, "pjmedia_snd_port_set_ec() ignored, no "
				 "change in settings"));
	    return PJ_SUCCESS;
	}

	status = pjmedia_aud_stream_get_param(snd_port->aud_stream, &prm);
	if (status != PJ_SUCCESS)
	    return status;

	/* Audio stream must be in PCM format */
	PJ_ASSERT_RETURN(prm.ext_fmt.id == PJMEDIA_FORMAT_PCM,
			 PJ_EINVALIDOP);

	/* Destroy AEC */
	if (snd_port->ec_state) {
	    pjmedia_echo_destroy(snd_port->ec_state);
	    snd_port->ec_state = NULL;
	}

	if (tail_ms != 0) {
	    unsigned delay_ms;

	    //No need to add input latency in the latency calculation,
	    //since actual input latency should be zero.
	    //delay_ms = (si.rec_latency + si.play_latency) * 1000 /
	    //	   snd_port->clock_rate;
	    /* Set EC latency to 3/4 of output latency to reduce the
	     * possibility of missing/late reference frame.
	     */
	    delay_ms = prm.output_latency_ms * 3/4;
	    status = pjmedia_echo_create2(pool, snd_port->clock_rate, 
					  snd_port->channel_count,
					  snd_port->samples_per_frame, 
					  tail_ms, delay_ms,
					  options, &snd_port->ec_state);
	    if (status != PJ_SUCCESS)
		snd_port->ec_state = NULL;
	    else
		snd_port->ec_suspended = PJ_FALSE;
	} else {
	    PJ_LOG(4,(THIS_FILE, "Echo canceller is now disabled in the "
				 "sound port"));
	    status = PJ_SUCCESS;
	}

	snd_port->ec_options = options;
	snd_port->ec_tail_len = tail_ms;
    }

    return status;
}


/* Get AEC tail length */
PJ_DEF(pj_status_t) pjmedia_snd_port_get_ec_tail( pjmedia_snd_port *snd_port,
						  unsigned *p_length)
{
    PJ_ASSERT_RETURN(snd_port && p_length, PJ_EINVAL);

    /* Determine whether we use device or software EC */
    if (snd_port->aud_caps & PJMEDIA_AUD_DEV_CAP_EC) {
	/* We use device EC */
	pj_bool_t ec_enabled;
	pj_status_t status;

	/* Query EC status */
	status = pjmedia_aud_stream_get_cap(snd_port->aud_stream,
					    PJMEDIA_AUD_DEV_CAP_EC,
					    &ec_enabled);
	if (status != PJ_SUCCESS)
	    return status;

	if (!ec_enabled) {
	    *p_length = 0;
	} else if (snd_port->aud_caps & PJMEDIA_AUD_DEV_CAP_EC_TAIL) {
	    /* Get device EC tail */
	    status = pjmedia_aud_stream_get_cap(snd_port->aud_stream,
						PJMEDIA_AUD_DEV_CAP_EC_TAIL,
						p_length);
	    if (status != PJ_SUCCESS)
		return status;
	} else {
	    /* Just use default */
	    *p_length = AEC_TAIL;
	}

    } else {
	/* We use software EC */
	*p_length =  snd_port->ec_state ? snd_port->ec_tail_len : 0;
    }
    return PJ_SUCCESS;
}


/*
 * Connect a port.
 */
PJ_DEF(pj_status_t) pjmedia_snd_port_connect( pjmedia_snd_port *snd_port,
					      pjmedia_port *port)
{
    pjmedia_port_info *pinfo;

    PJ_ASSERT_RETURN(snd_port && port, PJ_EINVAL);

    /* Check that port has the same configuration as the sound device
     * port.
     */
    pinfo = &port->info;
    if (pinfo->clock_rate != snd_port->clock_rate)
	return PJMEDIA_ENCCLOCKRATE;

    if (pinfo->samples_per_frame != snd_port->samples_per_frame)
	return PJMEDIA_ENCSAMPLESPFRAME;

    if (pinfo->channel_count != snd_port->channel_count)
	return PJMEDIA_ENCCHANNEL;

    if (pinfo->bits_per_sample != snd_port->bits_per_sample)
	return PJMEDIA_ENCBITS;

    /* Port is okay. */
    snd_port->port = port;
    return PJ_SUCCESS;
}


/*
 * Get the connected port.
 */
PJ_DEF(pjmedia_port*) pjmedia_snd_port_get_port(pjmedia_snd_port *snd_port)
{
    PJ_ASSERT_RETURN(snd_port, NULL);
    return snd_port->port;
}


/*
 * Disconnect port.
 */
PJ_DEF(pj_status_t) pjmedia_snd_port_disconnect(pjmedia_snd_port *snd_port)
{
    PJ_ASSERT_RETURN(snd_port, PJ_EINVAL);

    snd_port->port = NULL;

    return PJ_SUCCESS;
}


