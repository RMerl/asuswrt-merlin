/* $Id: circbuf.h 3553 2011-05-05 06:14:19Z nanang $ */
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

#ifndef __PJMEDIA_CIRC_BUF_H__
#define __PJMEDIA_CIRC_BUF_H__

/**
 * @file circbuf.h
 * @brief Circular Buffer.
 */

#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/pool.h>

/**
 * @defgroup PJMED_CIRCBUF Circular Buffer
 * @ingroup PJMEDIA_FRAME_OP
 * @brief Circular buffer manages read and write contiguous audio samples in a 
 * non-contiguous buffer as if the buffer were contiguous. This should give
 * better performance than keeping contiguous samples in a contiguous buffer,
 * since read/write operations will only update the pointers, instead of 
 * shifting audio samples.
 *
 * @{
 *
 * This section describes PJMEDIA's implementation of circular buffer.
 */

/* Algorithm checkings, for development purpose only */
#if 0
#   define PJMEDIA_CIRC_BUF_CHECK(x) pj_assert(x)
#else
#   define PJMEDIA_CIRC_BUF_CHECK(x)
#endif

PJ_BEGIN_DECL

/** 
 * Circular buffer structure
 */
typedef struct pjmedia_circ_buf {
    pj_int16_t	    *buf;	    /**< The buffer			*/
    unsigned	     capacity;	    /**< Buffer capacity, in samples	*/

    pj_int16_t	    *start;	    /**< Pointer to the first sample	*/
    unsigned	     len;	    /**< Audio samples length, 
					 in samples			*/
} pjmedia_circ_buf;


/**
 * Create the circular buffer.
 *
 * @param pool		    Pool where the circular buffer will be allocated
 *			    from.
 * @param capacity	    Capacity of the buffer, in samples.
 * @param p_cb		    Pointer to receive the circular buffer instance.
 *
 * @return		    PJ_SUCCESS if the circular buffer has been
 *			    created successfully, otherwise the appropriate
 *			    error will be returned.
 */
PJ_INLINE(pj_status_t) pjmedia_circ_buf_create(pj_pool_t *pool, 
					       unsigned capacity, 
					       pjmedia_circ_buf **p_cb)
{
    pjmedia_circ_buf *cbuf;

    cbuf = PJ_POOL_ZALLOC_T(pool, pjmedia_circ_buf);
    cbuf->buf = (pj_int16_t*) pj_pool_calloc(pool, capacity, 
					     sizeof(pj_int16_t));
    cbuf->capacity = capacity;
    cbuf->start = cbuf->buf;
    cbuf->len = 0;

    *p_cb = cbuf;

    return PJ_SUCCESS;
}


/**
 * Reset the circular buffer.
 *
 * @param circbuf	    The circular buffer.
 *
 * @return		    PJ_SUCCESS when successful.
 */
PJ_INLINE(pj_status_t) pjmedia_circ_buf_reset(pjmedia_circ_buf *circbuf)
{
    circbuf->start = circbuf->buf;
    circbuf->len = 0;

    return PJ_SUCCESS;
}


/**
 * Get the circular buffer length, it is number of samples buffered in the 
 * circular buffer.
 *
 * @param circbuf	    The circular buffer.
 *
 * @return		    The buffer length.
 */
PJ_INLINE(unsigned) pjmedia_circ_buf_get_len(pjmedia_circ_buf *circbuf)
{
    return circbuf->len;
}


/**
 * Set circular buffer length. This is useful when audio buffer is manually 
 * manipulated by the user, e.g: shrinked, expanded.
 *
 * @param circbuf	    The circular buffer.
 * @param len		    The new buffer length.
 */
PJ_INLINE(void) pjmedia_circ_buf_set_len(pjmedia_circ_buf *circbuf,
					 unsigned len)
{
    PJMEDIA_CIRC_BUF_CHECK(len <= circbuf->capacity);
    circbuf->len = len;
}


/**
 * Advance the read pointer of circular buffer. This function will discard
 * the skipped samples while advancing the read pointer, thus reducing 
 * the buffer length.
 *
 * @param circbuf	    The circular buffer.
 * @param count		    Distance from current read pointer, can only be
 *			    possitive number, in samples.
 *
 * @return		    PJ_SUCCESS when successful, otherwise 
 *			    the appropriate error will be returned.
 */
PJ_INLINE(pj_status_t) pjmedia_circ_buf_adv_read_ptr(pjmedia_circ_buf *circbuf, 
						     unsigned count)
{
    if (count >= circbuf->len)
	return pjmedia_circ_buf_reset(circbuf);

    PJMEDIA_CIRC_BUF_CHECK(count <= circbuf->len);

    circbuf->start += count;
    if (circbuf->start >= circbuf->buf + circbuf->capacity) 
	circbuf->start -= circbuf->capacity;
    circbuf->len -= count;

    return PJ_SUCCESS;
}


/**
 * Advance the write pointer of circular buffer. Since write pointer is always
 * pointing to a sample after the end of sample, so this function also means
 * increasing the buffer length.
 *
 * @param circbuf	    The circular buffer.
 * @param count		    Distance from current write pointer, can only be
 *			    possitive number, in samples.
 *
 * @return		    PJ_SUCCESS when successful, otherwise 
 *			    the appropriate error will be returned.
 */
PJ_INLINE(pj_status_t) pjmedia_circ_buf_adv_write_ptr(pjmedia_circ_buf *circbuf,
						      unsigned count)
{
    if (count + circbuf->len > circbuf->capacity)
	return PJ_ETOOBIG;

    circbuf->len += count;

    return PJ_SUCCESS;
}


/**
 * Get the real buffer addresses containing the audio samples.
 *
 * @param circbuf	    The circular buffer.
 * @param reg1		    Pointer to store the first buffer address.
 * @param reg1_len	    Pointer to store the length of the first buffer, 
 *			    in samples.
 * @param reg2		    Pointer to store the second buffer address.
 * @param reg2_len	    Pointer to store the length of the second buffer, 
 *			    in samples.
 */
PJ_INLINE(void) pjmedia_circ_buf_get_read_regions(pjmedia_circ_buf *circbuf, 
						  pj_int16_t **reg1, 
						  unsigned *reg1_len, 
						  pj_int16_t **reg2, 
						  unsigned *reg2_len)
{
    *reg1 = circbuf->start;
    *reg1_len = circbuf->len;
    if (*reg1 + *reg1_len > circbuf->buf + circbuf->capacity) {
	*reg1_len = circbuf->buf + circbuf->capacity - circbuf->start;
	*reg2 = circbuf->buf;
	*reg2_len = circbuf->len - *reg1_len;
    } else {
	*reg2 = NULL;
	*reg2_len = 0;
    }

    PJMEDIA_CIRC_BUF_CHECK(*reg1_len != 0 || (*reg1_len == 0 && 
					      circbuf->len == 0));
    PJMEDIA_CIRC_BUF_CHECK(*reg1_len + *reg2_len == circbuf->len);
}


/**
 * Get the real buffer addresses that is empty or writeable.
 *
 * @param circbuf	    The circular buffer.
 * @param reg1		    Pointer to store the first buffer address.
 * @param reg1_len	    Pointer to store the length of the first buffer, 
 *			    in samples.
 * @param reg2		    Pointer to store the second buffer address.
 * @param reg2_len	    Pointer to store the length of the second buffer, 
 *			    in samples.
 */
PJ_INLINE(void) pjmedia_circ_buf_get_write_regions(pjmedia_circ_buf *circbuf, 
						   pj_int16_t **reg1, 
						   unsigned *reg1_len, 
						   pj_int16_t **reg2, 
						   unsigned *reg2_len)
{
    *reg1 = circbuf->start + circbuf->len;
    if (*reg1 >= circbuf->buf + circbuf->capacity)
	*reg1 -= circbuf->capacity;
    *reg1_len = circbuf->capacity - circbuf->len;
    if (*reg1 + *reg1_len > circbuf->buf + circbuf->capacity) {
	*reg1_len = circbuf->buf + circbuf->capacity - *reg1;
	*reg2 = circbuf->buf;
	*reg2_len = circbuf->start - circbuf->buf;
    } else {
	*reg2 = NULL;
	*reg2_len = 0;
    }

    PJMEDIA_CIRC_BUF_CHECK(*reg1_len != 0 || (*reg1_len == 0 && 
					      circbuf->len == 0));
    PJMEDIA_CIRC_BUF_CHECK(*reg1_len + *reg2_len == circbuf->capacity - 
			   circbuf->len);
}


/**
 * Read audio samples from the circular buffer.
 *
 * @param circbuf	    The circular buffer.
 * @param data		    Buffer to store the read audio samples.
 * @param count		    Number of samples being read.
 *
 * @return		    PJ_SUCCESS when successful, otherwise 
 *			    the appropriate error will be returned.
 */
PJ_INLINE(pj_status_t) pjmedia_circ_buf_read(pjmedia_circ_buf *circbuf, 
					     pj_int16_t *data, 
					     unsigned count)
{
    pj_int16_t *reg1, *reg2;
    unsigned reg1cnt, reg2cnt;

    /* Data in the buffer is less than requested */
    if (count > circbuf->len)
	return PJ_ETOOBIG;

    pjmedia_circ_buf_get_read_regions(circbuf, &reg1, &reg1cnt, 
				      &reg2, &reg2cnt);
    if (reg1cnt >= count) {
	pjmedia_copy_samples(data, reg1, count);
    } else {
	pjmedia_copy_samples(data, reg1, reg1cnt);
	pjmedia_copy_samples(data + reg1cnt, reg2, count - reg1cnt);
    }

    return pjmedia_circ_buf_adv_read_ptr(circbuf, count);
}


/**
 * Write audio samples to the circular buffer.
 *
 * @param circbuf	    The circular buffer.
 * @param data		    Audio samples to be written.
 * @param count		    Number of samples being written.
 *
 * @return		    PJ_SUCCESS when successful, otherwise
 *			    the appropriate error will be returned.
 */
PJ_INLINE(pj_status_t) pjmedia_circ_buf_write(pjmedia_circ_buf *circbuf, 
					      pj_int16_t *data, 
					      unsigned count)
{
    pj_int16_t *reg1, *reg2;
    unsigned reg1cnt, reg2cnt;

    /* Data to write is larger than buffer can store */
    if (count > circbuf->capacity - circbuf->len)
	return PJ_ETOOBIG;

    pjmedia_circ_buf_get_write_regions(circbuf, &reg1, &reg1cnt, 
				       &reg2, &reg2cnt);
    if (reg1cnt >= count) {
	pjmedia_copy_samples(reg1, data, count);
    } else {
	pjmedia_copy_samples(reg1, data, reg1cnt);
	pjmedia_copy_samples(reg2, data + reg1cnt, count - reg1cnt);
    }

    return pjmedia_circ_buf_adv_write_ptr(circbuf, count);
}


/**
 * Copy audio samples from the circular buffer without changing its state. 
 *
 * @param circbuf	    The circular buffer.
 * @param start_idx	    Starting sample index to be copied.
 * @param data		    Buffer to store the read audio samples.
 * @param count		    Number of samples being read.
 *
 * @return		    PJ_SUCCESS when successful, otherwise 
 *			    the appropriate error will be returned.
 */
PJ_INLINE(pj_status_t) pjmedia_circ_buf_copy(pjmedia_circ_buf *circbuf, 
					     unsigned start_idx,
					     pj_int16_t *data, 
					     unsigned count)
{
    pj_int16_t *reg1, *reg2;
    unsigned reg1cnt, reg2cnt;

    /* Data in the buffer is less than requested */
    if (count + start_idx > circbuf->len)
	return PJ_ETOOBIG;

    pjmedia_circ_buf_get_read_regions(circbuf, &reg1, &reg1cnt, 
				      &reg2, &reg2cnt);
    if (reg1cnt > start_idx) {
	unsigned tmp_len;
	tmp_len = reg1cnt - start_idx;
	if (tmp_len > count)
	    tmp_len = count;
	pjmedia_copy_samples(data, reg1 + start_idx, tmp_len);
	if (tmp_len < count)
	    pjmedia_copy_samples(data + tmp_len, reg2, count - tmp_len);
    } else {
	pjmedia_copy_samples(data, reg2 + start_idx - reg1cnt, count);
    }

    return PJ_SUCCESS;
}


/**
 * Pack the buffer so the first sample will be in the beginning of the buffer.
 * This will also make the buffer contiguous.
 *
 * @param circbuf	    The circular buffer.
 *
 * @return		    PJ_SUCCESS when successful, otherwise 
 *			    the appropriate error will be returned.
 */
PJ_INLINE(pj_status_t) pjmedia_circ_buf_pack_buffer(pjmedia_circ_buf *circbuf)
{
    pj_int16_t *reg1, *reg2;
    unsigned reg1cnt, reg2cnt;
    unsigned gap;

    pjmedia_circ_buf_get_read_regions(circbuf, &reg1, &reg1cnt, 
				      &reg2, &reg2cnt);

    /* Check if not contigue */
    if (reg2cnt != 0) {
	/* Check if no space left to roll the buffer 
	 * (or should this function provide temporary buffer?)
	 */
	gap = circbuf->capacity - pjmedia_circ_buf_get_len(circbuf);
	if (gap == 0)
	    return PJ_ETOOBIG;

	/* Roll buffer left using the gap until reg2cnt == 0 */
	do {
	    if (gap > reg2cnt)
		gap = reg2cnt;
	    pjmedia_move_samples(reg1 - gap, reg1, reg1cnt);
	    pjmedia_copy_samples(reg1 + reg1cnt - gap, reg2, gap);
	    if (gap < reg2cnt)
		pjmedia_move_samples(reg2, reg2 + gap, reg2cnt - gap);
	    reg1 -= gap;
	    reg1cnt += gap;
	    reg2cnt -= gap;
	} while (reg2cnt > 0);
    }

    /* Finally, Shift samples to the left edge */
    if (reg1 != circbuf->buf)
	pjmedia_move_samples(circbuf->buf, reg1, 
			     pjmedia_circ_buf_get_len(circbuf));
    circbuf->start = circbuf->buf;

    return PJ_SUCCESS;
}


PJ_END_DECL

/**
 * @}
 */

#endif
