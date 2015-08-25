/* $Id: splitcomb.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_SPLITCOMB_H__
#define __PJMEDIA_SPLITCOMB_H__


/**
 * @file splitcomb.h
 * @brief Media channel splitter/combiner port.
 */
#include <pjmedia/port.h>


/**
 * @addtogroup PJMEDIA_SPLITCOMB Media channel splitter/combiner
 * @ingroup PJMEDIA_PORT
 * @brief Split and combine multiple mono-channel media ports into
 *  a single multiple-channels media port
 * @{
 *
 * This section describes media port to split and combine media
 * channels in the stream.
 *
 * A splitter/combiner splits a single stereo/multichannels audio frame into
 * multiple audio frames to each channel when put_frame() is called, 
 * and combines mono frames from each channel into a stereo/multichannel 
 * frame when get_frame() is called. A common application for the splitter/
 * combiner is to split frames from stereo to mono and vise versa.
 */

PJ_BEGIN_DECL


/**
 * Create a media splitter/combiner with the specified parameters.
 * When the splitter/combiner is created, it creates an instance of
 * pjmedia_port. This media port represents the stereo/multichannel side
 * of the splitter/combiner. Application needs to supply the splitter/
 * combiner with a media port for each audio channels.
 *
 * @param pool		    Pool to allocate memory to create the splitter/
 *			    combiner.
 * @param clock_rate	    Audio clock rate/sampling rate.
 * @param channel_count	    Number of channels.
 * @param samples_per_frame Number of samples per frame.
 * @param bits_per_sample   Bits per sample.
 * @param options	    Optional flags.
 * @param p_splitcomb	    Pointer to receive the splitter/combiner.
 *
 * @return		    PJ_SUCCESS on success, or the appropriate
 *			    error code.
 */
PJ_DECL(pj_status_t) pjmedia_splitcomb_create(pj_pool_t *pool,
					      unsigned clock_rate,
					      unsigned channel_count,
					      unsigned samples_per_frame,
					      unsigned bits_per_sample,
					      unsigned options,
					      pjmedia_port **p_splitcomb);

/**
 * Supply the splitter/combiner with media port for the specified channel 
 * number. The media port will be called at the
 * same phase as the splitter/combiner; which means that when application
 * calls get_frame() of the splitter/combiner, it will call get_frame()
 * for all ports that have the same phase. And similarly for put_frame().
 *
 * @param splitcomb	    The splitter/combiner.
 * @param ch_num	    Audio channel starting number (zero based).
 * @param options	    Must be zero at the moment.
 * @param port		    The media port.
 *
 * @return		    PJ_SUCCESS on success, or the appropriate error
 *			    code.
 */
PJ_DECL(pj_status_t) pjmedia_splitcomb_set_channel(pjmedia_port *splitcomb,
						   unsigned ch_num,
						   unsigned options,
						   pjmedia_port *port);

/**
 * Create a reverse phase media port for the specified channel number.
 * For channels with reversed phase, when application calls put_frame() to
 * the splitter/combiner, the splitter/combiner will only put the frame to
 * a buffer. Later on, when application calls get_frame() on the channel's
 * media port, it will return the frame that are available in the buffer.
 * The same process happens when application calls put_frame() to the
 * channel's media port, it will only put the frame to another buffer, which
 * will be returned when application calls get_frame() to the splitter's 
 * media port. So this effectively reverse the phase of the media port.
 *
 * @param pool		    The pool to allocate memory for the port and
 *			    buffers.
 * @param splitcomb	    The splitter/combiner.
 * @param ch_num	    Audio channel starting number (zero based).
 * @param options	    Normally is zero, but the lower 8-bit of the 
 *			    options can be used to specify the number of 
 *			    buffers in the circular buffer. If zero, then
 *			    default number will be used (default: 8).
 * @param p_chport	    The media port created with reverse phase for
 *			    the specified audio channel.
 *
 * @return		    PJ_SUCCESS on success, or the appropriate error
 *			    code.
 */
PJ_DECL(pj_status_t) 
pjmedia_splitcomb_create_rev_channel( pj_pool_t *pool,
				      pjmedia_port *splitcomb,
				      unsigned ch_num,
				      unsigned options,
				      pjmedia_port **p_chport);



PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJMEDIA_SPLITCOMB_H__ */


