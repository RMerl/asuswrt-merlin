/* $Id: delaybuf.h 3567 2011-05-15 12:54:28Z ming $ */
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

#ifndef __PJMEDIA_DELAYBUF_H__
#define __PJMEDIA_DELAYBUF_H__


/**
 * @file delaybuf.h
 * @brief Delay Buffer.
 */

#include <pjmedia/types.h>

/**
 * @defgroup PJMED_DELAYBUF Adaptive Delay Buffer
 * @ingroup PJMEDIA_FRAME_OP
 * @brief Adaptive delay buffer with high-quality time-scale
 * modification
 * @{
 *
 * This section describes PJMEDIA's implementation of delay buffer.
 * Delay buffer works quite similarly like a fixed jitter buffer, that
 * is it will delay the frame retrieval by some interval so that caller
 * will get continuous frame from the buffer. This can be useful when
 * the put() and get() operations are not evenly interleaved, for example
 * when caller performs burst of put() operations and then followed by
 * burst of get() operations. With using this delay buffer, the buffer
 * will put the burst frames into a buffer so that get() operations
 * will always get a frame from the buffer (assuming that the number of
 * get() and put() are matched).
 *
 * The buffer is adaptive, that is it continuously learns the optimal delay
 * to be applied to the audio flow at run-time. Once the optimal delay has 
 * been learned, the delay buffer will apply this delay to the audio flow,
 * expanding or shrinking the audio samples as necessary when the actual
 * audio samples in the buffer are too low or too high. It does this without
 * distorting the audio quality of the audio, by using \a PJMED_WSOLA.
 *
 * The delay buffer is used in \ref PJMED_SND_PORT, \ref PJMEDIA_SPLITCOMB,
 * and \ref PJMEDIA_CONF.
 */

PJ_BEGIN_DECL

/** Opaque declaration for delay buffer. */
typedef struct pjmedia_delay_buf pjmedia_delay_buf;

/**
 * Delay buffer options.
 */
typedef enum pjmedia_delay_buf_flag
{
    /**
     * Use simple FIFO mechanism for the delay buffer, i.e.
     * without WSOLA for expanding and shrinking audio samples.
     */
    PJMEDIA_DELAY_BUF_SIMPLE_FIFO = 1

} pjmedia_delay_buf_flag;

/**
 * Create the delay buffer. Once the delay buffer is created, it will
 * enter learning state unless the delay argument is specified, which
 * in this case it will directly enter the running state.
 *
 * @param pool		    Pool where the delay buffer will be allocated
 *			    from.
 * @param name		    Optional name for the buffer for log 
 *			    identification.
 * @param clock_rate	    Number of samples processed per second.
 * @param samples_per_frame Number of samples per frame.
 * @param channel_count	    Number of channel per frame.
 * @param max_delay	    Maximum number of delay to be accommodated,
 *			    in ms, if this value is negative or less than 
 *			    one frame time, default maximum delay used is
 *			    400 ms.
 * @param options	    Options. If PJMEDIA_DELAY_BUF_SIMPLE_FIFO is
 *                          specified, then a simple FIFO mechanism
 *			    will be used instead of the adaptive
 *                          implementation (which uses WSOLA to expand
 *                          or shrink audio samples).
 *			    See #pjmedia_delay_buf_flag for other options.
 * @param p_b		    Pointer to receive the delay buffer instance.
 *
 * @return		    PJ_SUCCESS if the delay buffer has been
 *			    created successfully, otherwise the appropriate
 *			    error will be returned.
 */
PJ_DECL(pj_status_t) pjmedia_delay_buf_create(pj_pool_t *pool,
					      const char *name,
					      unsigned clock_rate,
					      unsigned samples_per_frame,
					      unsigned channel_count,
					      unsigned max_delay,
					      unsigned options,
					      pjmedia_delay_buf **p_b);

/**
 * Put one frame into the buffer.
 *
 * @param b		    The delay buffer.
 * @param frame		    Frame to be put into the buffer. This frame
 *			    must have samples_per_frame length.
 *
 * @return		    PJ_SUCCESS if frames can be put successfully.
 *			    PJ_EPENDING if the buffer is still at learning
 *			    state. PJ_ETOOMANY if the number of frames
 *			    will exceed maximum delay level, which in this
 *			    case the new frame will overwrite the oldest
 *			    frame in the buffer.
 */
PJ_DECL(pj_status_t) pjmedia_delay_buf_put(pjmedia_delay_buf *b,
					   pj_int16_t frame[]);

/**
 * Get one frame from the buffer.
 *
 * @param b		    The delay buffer.
 * @param frame		    Buffer to receive the frame from the delay
 *			    buffer.
 *
 * @return		    PJ_SUCCESS if frame has been copied successfully.
 *			    PJ_EPENDING if no frame is available, either
 *			    because the buffer is still at learning state or
 *			    no buffer is available during running state.
 *			    On non-successful return, the frame will be
 *			    filled with zeroes.
 */
PJ_DECL(pj_status_t) pjmedia_delay_buf_get(pjmedia_delay_buf *b,
					   pj_int16_t frame[]);

/**
 * Reset delay buffer. This will clear the buffer's content. But keep
 * the learning result.
 *
 * @param b		    The delay buffer.
 *
 * @return		    PJ_SUCCESS on success or the appropriate error.
 */
PJ_DECL(pj_status_t) pjmedia_delay_buf_reset(pjmedia_delay_buf *b);

/**
 * Destroy delay buffer.
 *
 * @param b	    Delay buffer session.
 *
 * @return	    PJ_SUCCESS normally.
 */
PJ_DECL(pj_status_t) pjmedia_delay_buf_destroy(pjmedia_delay_buf *b);


PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJMEDIA_DELAYBUF_H__ */
