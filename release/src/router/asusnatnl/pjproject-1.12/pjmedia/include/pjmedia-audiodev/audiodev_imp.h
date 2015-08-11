/* $Id: audiodev_imp.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __AUDIODEV_IMP_H__
#define __AUDIODEV_IMP_H__

#include <pjmedia-audiodev/audiodev.h>

/**
 * @defgroup s8_audio_device_implementors_api Audio Device Implementors API
 * @ingroup audio_device_api
 * @brief API for audio device implementors
 * @{
 */

/**
 * Sound device factory operations.
 */
typedef struct pjmedia_aud_dev_factory_op
{
    /**
     * Initialize the audio device factory.
     *
     * @param f		The audio device factory.
     */
    pj_status_t (*init)(pjmedia_aud_dev_factory *f);

    /**
     * Close this audio device factory and release all resources back to the
     * operating system.
     *
     * @param f		The audio device factory.
     */
    pj_status_t (*destroy)(pjmedia_aud_dev_factory *f);

    /**
     * Get the number of audio devices installed in the system.
     *
     * @param f		The audio device factory.
     */
    unsigned (*get_dev_count)(pjmedia_aud_dev_factory *f);

    /**
     * Get the audio device information and capabilities.
     *
     * @param f		The audio device factory.
     * @param index	Device index.
     * @param info	The audio device information structure which will be
     *			initialized by this function once it returns 
     *			successfully.
     */
    pj_status_t	(*get_dev_info)(pjmedia_aud_dev_factory *f, 
				unsigned index,
				pjmedia_aud_dev_info *info);

    /**
     * Initialize the specified audio device parameter with the default
     * values for the specified device.
     *
     * @param f		The audio device factory.
     * @param index	Device index.
     * @param param	The audio device parameter.
     */
    pj_status_t (*default_param)(pjmedia_aud_dev_factory *f,
				 unsigned index,
				 pjmedia_aud_param *param);

    /**
     * Open the audio device and create audio stream. See
     * #pjmedia_aud_stream_create()
     */
    pj_status_t (*create_stream)(pjmedia_aud_dev_factory *f,
				 const pjmedia_aud_param *param,
				 pjmedia_aud_rec_cb rec_cb,
				 pjmedia_aud_play_cb play_cb,
				 void *user_data,
				 pjmedia_aud_stream **p_aud_strm);

    /**
     * Refresh the list of audio devices installed in the system.
     *
     * @param f		The audio device factory.
     */
    pj_status_t (*refresh)(pjmedia_aud_dev_factory *f);

} pjmedia_aud_dev_factory_op;


/**
 * This structure describes an audio device factory. 
 */
struct pjmedia_aud_dev_factory
{
    /** Internal data to be initialized by audio subsystem. */
    struct {
	/** Driver index */
	unsigned drv_idx;
    } sys;

    /** Operations */
    pjmedia_aud_dev_factory_op *op;
};


/**
 * Sound stream operations.
 */
typedef struct pjmedia_aud_stream_op
{
    /**
     * See #pjmedia_aud_stream_get_param()
     */
    pj_status_t (*get_param)(pjmedia_aud_stream *strm,
			     pjmedia_aud_param *param);

    /**
     * See #pjmedia_aud_stream_get_cap()
     */
    pj_status_t (*get_cap)(pjmedia_aud_stream *strm,
			   pjmedia_aud_dev_cap cap,
			   void *value);

    /**
     * See #pjmedia_aud_stream_set_cap()
     */
    pj_status_t (*set_cap)(pjmedia_aud_stream *strm,
			   pjmedia_aud_dev_cap cap,
			   const void *value);

    /**
     * See #pjmedia_aud_stream_start()
     */
    pj_status_t (*start)(pjmedia_aud_stream *strm);

    /**
     * See #pjmedia_aud_stream_stop().
     */
    pj_status_t (*stop)(pjmedia_aud_stream *strm);

    /**
     * See #pjmedia_aud_stream_destroy().
     */
    pj_status_t (*destroy)(pjmedia_aud_stream *strm);

} pjmedia_aud_stream_op;


/**
 * This structure describes the audio device stream.
 */
struct pjmedia_aud_stream
{
    /** Internal data to be initialized by audio subsystem */
    struct {
	/** Driver index */
	unsigned drv_idx;
    } sys;

    /** Operations */
    pjmedia_aud_stream_op *op;
};




/**
 * @}
 */



#endif /* __AUDIODEV_IMP_H__ */
