/* $Id: stereo.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_STEREO_H__
#define __PJMEDIA_STEREO_H__

/**
 * @file stereo.h
 * @brief Monochannel and multichannel converter.
 */

#include <pjmedia/errno.h>
#include <pjmedia/port.h>
#include <pjmedia/types.h>
#include <pj/assert.h>


/**
 * @defgroup PJMEDIA_STEREO Monochannel and multichannel audio frame converter
 * @ingroup PJMEDIA_FRAME_OP
 * @brief Mono - multi-channels audio conversion
 * @{
 *
 */

PJ_BEGIN_DECL


/**
 * Multichannel to monochannel conversion mixes samples from all channels
 * into the monochannel.
 */
#define PJMEDIA_STEREO_MIX  PJ_TRUE



/**
 * Multichannel to monochannel conversion, it has two operation mode specified
 * by param options, @see pjmedia_stereo_options. This function can work safely
 * using the same buffer (in place conversion).
 *
 * @param mono		    Output buffer to store the mono frame extracted 
 *			    from the multichannels frame.
 * @param multi		    Input frame containing multichannels audio.
 * @param channel_count	    Number of channels in the input frame.
 * @param samples_per_frame Number of samples in the input frame.
 * @param mix		    If the value is PJ_TRUE then the input channels 
 *			    will be mixed to produce output frame, otherwise
 *			    only frame from channel_src will be copied to the
 *			    output frame.
 * @param channel_src	    When mixing is disabled, the mono output frame
 *			    will be copied from this channel number.
 *
 * @return		    PJ_SUCCESS on success;
 */
PJ_INLINE(pj_status_t) pjmedia_convert_channel_nto1(pj_int16_t mono[],
						    const pj_int16_t multi[],
						    unsigned channel_count,
						    unsigned samples_per_frame,
						    pj_bool_t mix,
						    unsigned channel_src)
{
    unsigned i;

    PJ_ASSERT_RETURN(mono && multi && channel_count && samples_per_frame &&
		     channel_src < channel_count,  PJ_EINVAL);

    if (mix==PJ_FALSE) {
	for (i = channel_src; i < samples_per_frame; i += channel_count) {
	    *mono = multi[i];
	    ++mono;
	}
    } else {
	unsigned j;
	for (i = 0; i < samples_per_frame; i += channel_count) {
	    int tmp = 0;
	    for(j = 0; j < channel_count; ++j)
		tmp += multi[i+j];

	    if (tmp > 32767) tmp = 32767;
	    else if (tmp < -32768) tmp = -32768;
	    *mono = (pj_int16_t) tmp;
	    ++mono;
	}
    }

    return PJ_SUCCESS;
}


/**
 * Monochannel to multichannel conversion, it will just duplicate the samples
 * from monochannel frame to all channels in the multichannel frame. 
 * This function can work safely using the same buffer (in place conversion)
 * as long as the buffer is big enough for the multichannel samples.
 *
 * @param multi		    Output buffer to store the multichannels frame 
 *			    mixed from the mono frame.
 * @param mono		    The input monochannel audio frame.
 * @param channel_count	    Desired number of channels in the output frame.
 * @param samples_per_frame Number of samples in the input frame.
 * @param options	    Options for conversion, currently must be zero.
 *
 * @return		    PJ_SUCCESS on success;
 */
PJ_INLINE(pj_status_t) pjmedia_convert_channel_1ton(pj_int16_t multi[],
						    const pj_int16_t mono[],
						    unsigned channel_count,
						    unsigned samples_per_frame,
						    unsigned options)
{
    const pj_int16_t *src;

    PJ_ASSERT_RETURN(mono && multi && channel_count && samples_per_frame, 
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(options == 0, PJ_EINVAL);

    PJ_UNUSED_ARG(options);

    src = mono + samples_per_frame - 1;
    samples_per_frame *= channel_count;
    while (samples_per_frame) {
	unsigned i;
	for (i=1; i<=channel_count; ++i)
	    multi[samples_per_frame-i] = *src;
	samples_per_frame -= channel_count;
	--src;
    }

    return PJ_SUCCESS;
}


/** 
 * Options for channel converter port. The #pjmedia_stereo_options is also
 * valid for this port options.
 */
typedef enum pjmedia_stereo_port_options
{
    /**
     * Specifies whether this port should not destroy downstream port when 
     * this port is destroyed.
     */
    PJMEDIA_STEREO_DONT_DESTROY_DN  = 4
} pjmedia_stereo_port_options;


/**
 * Create a mono-multi channel converter port. This creates a converter session,
 * which will adjust the samples of audio frame to a different channel count
 * when the port's get_frame() and put_frame() is called.
 *
 * When the port's get_frame() is called, this port will get a frame from 
 * the downstream port and convert the frame to the target channel count before
 * returning it to the caller.
 *
 * When the port's put_frame() is called, this port will convert the frame
 * to the downstream port's channel count before giving the frame to the 
 * downstream port.
 *
 * @param pool			Pool to allocate the structure and buffers.
 * @param dn_port		The downstream port, which channel count is to
 *				be converted to the target channel count.
 * @param channel_count		This port channel count.
 * @param options		Bitmask flags from #pjmedia_stereo_port_options
 *				and also application may add PJMEDIA_STEREO_MIX
 *				to mix channels.
 *				When this flag is zero, the default behavior
 *				is to use simple N-to-1 channel converter and 
 *				to destroy downstream port when this port is 
 *				destroyed.
 * @param p_port		Pointer to receive the stereo port instance.
 *
 * @return PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stereo_port_create( pj_pool_t *pool,
						 pjmedia_port *dn_port,
						 unsigned channel_count,
						 unsigned options,
						 pjmedia_port **p_port );

PJ_END_DECL

/**
 * @}
 */


#endif	/* __PJMEDIA_STEREO_H__ */

