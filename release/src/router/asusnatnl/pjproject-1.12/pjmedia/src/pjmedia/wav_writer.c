/* $Id: wav_writer.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/wav_port.h>
#include <pjmedia/alaw_ulaw.h>
#include <pjmedia/errno.h>
#include <pjmedia/wave.h>
#include <pj/assert.h>
#include <pj/file_access.h>
#include <pj/file_io.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>


#define THIS_FILE	    "wav_writer.c"
#define SIGNATURE	    PJMEDIA_PORT_SIGNATURE('F', 'W', 'R', 'T')


struct file_port
{
    pjmedia_port     base;
    pjmedia_wave_fmt_tag fmt_tag;
    pj_uint16_t	     bytes_per_sample;

    pj_size_t	     bufsize;
    char	    *buf;
    char	    *writepos;
    pj_size_t	     total;

    pj_oshandle_t    fd;

    pj_size_t	     cb_size;
    pj_status_t	   (*cb)(pjmedia_port*, void*);
};

static pj_status_t file_put_frame(pjmedia_port *this_port, 
				  const pjmedia_frame *frame);
static pj_status_t file_get_frame(pjmedia_port *this_port, 
				  pjmedia_frame *frame);
static pj_status_t file_on_destroy(pjmedia_port *this_port);


/*
 * Create file writer port.
 */
PJ_DEF(pj_status_t) pjmedia_wav_writer_port_create( pj_pool_t *pool,
						     const char *filename,
						     unsigned sampling_rate,
						     unsigned channel_count,
						     unsigned samples_per_frame,
						     unsigned bits_per_sample,
						     unsigned flags,
						     pj_ssize_t buff_size,
						     pjmedia_port **p_port )
{
    struct file_port *fport;
    pjmedia_wave_hdr wave_hdr;
    pj_ssize_t size;
    pj_str_t name;
    pj_status_t status;

    /* Check arguments. */
    PJ_ASSERT_RETURN(pool && filename && p_port, PJ_EINVAL);

    /* Only supports 16bits per sample for now.
     * See flush_buffer().
     */
    PJ_ASSERT_RETURN(bits_per_sample == 16, PJ_EINVAL);

    /* Create file port instance. */
    fport = PJ_POOL_ZALLOC_T(pool, struct file_port);
    PJ_ASSERT_RETURN(fport != NULL, PJ_ENOMEM);

    /* Initialize port info. */
    pj_strdup2(pool, &name, filename);
    pjmedia_port_info_init(&fport->base.info, &name, SIGNATURE,
			   sampling_rate, channel_count, bits_per_sample,
			   samples_per_frame);

    fport->base.get_frame = &file_get_frame;
    fport->base.put_frame = &file_put_frame;
    fport->base.on_destroy = &file_on_destroy;

    if (flags == PJMEDIA_FILE_WRITE_ALAW) {
	fport->fmt_tag = PJMEDIA_WAVE_FMT_TAG_ALAW;
	fport->bytes_per_sample = 1;
    } else if (flags == PJMEDIA_FILE_WRITE_ULAW) {
	fport->fmt_tag = PJMEDIA_WAVE_FMT_TAG_ULAW;
	fport->bytes_per_sample = 1;
    } else {
	fport->fmt_tag = PJMEDIA_WAVE_FMT_TAG_PCM;
	fport->bytes_per_sample = 2;
    }

    /* Open file in write and read mode.
     * We need the read mode because we'll modify the WAVE header once
     * the recording has completed.
     */
    status = pj_file_open(pool, filename, PJ_O_WRONLY, &fport->fd);
    if (status != PJ_SUCCESS)
	return status;

    /* Initialize WAVE header */
    pj_bzero(&wave_hdr, sizeof(pjmedia_wave_hdr));
    wave_hdr.riff_hdr.riff = PJMEDIA_RIFF_TAG;
    wave_hdr.riff_hdr.file_len = 0; /* will be filled later */
    wave_hdr.riff_hdr.wave = PJMEDIA_WAVE_TAG;

    wave_hdr.fmt_hdr.fmt = PJMEDIA_FMT_TAG;
    wave_hdr.fmt_hdr.len = 16;
    wave_hdr.fmt_hdr.fmt_tag = (pj_uint16_t)fport->fmt_tag;
    wave_hdr.fmt_hdr.nchan = (pj_int16_t)channel_count;
    wave_hdr.fmt_hdr.sample_rate = sampling_rate;
    wave_hdr.fmt_hdr.bytes_per_sec = sampling_rate * channel_count * 
				     fport->bytes_per_sample;
    wave_hdr.fmt_hdr.block_align = (pj_uint16_t)
				   (fport->bytes_per_sample * channel_count);
    wave_hdr.fmt_hdr.bits_per_sample = (pj_uint16_t)
				       (fport->bytes_per_sample * 8);

    wave_hdr.data_hdr.data = PJMEDIA_DATA_TAG;
    wave_hdr.data_hdr.len = 0;	    /* will be filled later */


    /* Convert WAVE header from host byte order to little endian
     * before writing the header.
     */
    pjmedia_wave_hdr_host_to_file(&wave_hdr);


    /* Write WAVE header */
    if (fport->fmt_tag != PJMEDIA_WAVE_FMT_TAG_PCM) {
	pjmedia_wave_subchunk fact_chunk;
	pj_uint32_t tmp = 0;

	fact_chunk.id = PJMEDIA_FACT_TAG;
	fact_chunk.len = 4;

	PJMEDIA_WAVE_NORMALIZE_SUBCHUNK(&fact_chunk);

	/* Write WAVE header without DATA chunk header */
	size = sizeof(pjmedia_wave_hdr) - sizeof(wave_hdr.data_hdr);
	status = pj_file_write(fport->fd, &wave_hdr, &size);
	if (status != PJ_SUCCESS) {
	    pj_file_close(fport->fd);
	    return status;
	}

	/* Write FACT chunk if it stores compressed data */
	size = sizeof(fact_chunk);
	status = pj_file_write(fport->fd, &fact_chunk, &size);
	if (status != PJ_SUCCESS) {
	    pj_file_close(fport->fd);
	    return status;
	}
	size = 4;
	status = pj_file_write(fport->fd, &tmp, &size);
	if (status != PJ_SUCCESS) {
	    pj_file_close(fport->fd);
	    return status;
	}

	/* Write DATA chunk header */
	size = sizeof(wave_hdr.data_hdr);
	status = pj_file_write(fport->fd, &wave_hdr.data_hdr, &size);
	if (status != PJ_SUCCESS) {
	    pj_file_close(fport->fd);
	    return status;
	}
    } else {
	size = sizeof(pjmedia_wave_hdr);
	status = pj_file_write(fport->fd, &wave_hdr, &size);
	if (status != PJ_SUCCESS) {
	    pj_file_close(fport->fd);
	    return status;
	}
    }

    /* Set buffer size. */
    if (buff_size < 1) buff_size = PJMEDIA_FILE_PORT_BUFSIZE;
    fport->bufsize = buff_size;

    /* Check that buffer size is greater than bytes per frame */
    pj_assert(fport->bufsize >= fport->base.info.bytes_per_frame);


    /* Allocate buffer and set initial write position */
    fport->buf = (char*) pj_pool_alloc(pool, fport->bufsize);
    if (fport->buf == NULL) {
	pj_file_close(fport->fd);
	return PJ_ENOMEM;
    }
    fport->writepos = fport->buf;

    /* Done. */
    *p_port = &fport->base;

    PJ_LOG(4,(THIS_FILE, 
	      "File writer '%.*s' created: samp.rate=%d, bufsize=%uKB",
	      (int)fport->base.info.name.slen,
	      fport->base.info.name.ptr,
	      fport->base.info.clock_rate,
	      fport->bufsize / 1000));


    return PJ_SUCCESS;
}



/*
 * Get current writing position. 
 */
PJ_DEF(pj_ssize_t) pjmedia_wav_writer_port_get_pos( pjmedia_port *port )
{
    struct file_port *fport;

    /* Sanity check */
    PJ_ASSERT_RETURN(port, -PJ_EINVAL);

    /* Check that this is really a writer port */
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, -PJ_EINVALIDOP);

    fport = (struct file_port*) port;

    return fport->total;
}


/*
 * Register callback.
 */
PJ_DEF(pj_status_t) pjmedia_wav_writer_port_set_cb( pjmedia_port *port,
				pj_size_t pos,
				void *user_data,
			        pj_status_t (*cb)(pjmedia_port *port,
						  void *usr_data))
{
    struct file_port *fport;

    /* Sanity check */
    PJ_ASSERT_RETURN(port && cb, PJ_EINVAL);

    /* Check that this is really a writer port */
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, PJ_EINVALIDOP);

    fport = (struct file_port*) port;

    fport->cb_size = pos;
    fport->base.port_data.pdata = user_data;
    fport->cb = cb;

    return PJ_SUCCESS;
}


#if defined(PJ_IS_BIG_ENDIAN) && PJ_IS_BIG_ENDIAN!=0
    static void swap_samples(pj_int16_t *samples, unsigned count)
    {
	unsigned i;
	for (i=0; i<count; ++i) {
	    samples[i] = pj_swap16(samples[i]);
	}
    }
#else
#   define swap_samples(samples,count)
#endif

/*
 * Flush the contents of the buffer to the file.
 */
static pj_status_t flush_buffer(struct file_port *fport)
{
    pj_ssize_t bytes = fport->writepos - fport->buf;
    pj_status_t status;

    /* Convert samples to little endian */
    swap_samples((pj_int16_t*)fport->buf, bytes/fport->bytes_per_sample);

    /* Write to file. */
    status = pj_file_write(fport->fd, fport->buf, &bytes);

    /* Reset writepos */
    fport->writepos = fport->buf;

    return status;
}

/*
 * Put a frame into the buffer. When the buffer is full, flush the buffer
 * to the file.
 */
static pj_status_t file_put_frame(pjmedia_port *this_port, 
				  const pjmedia_frame *frame)
{
    struct file_port *fport = (struct file_port *)this_port;
    unsigned frame_size;

    if (fport->fmt_tag == PJMEDIA_WAVE_FMT_TAG_PCM)
	frame_size = frame->size;
    else
	frame_size = frame->size >> 1;

    /* Flush buffer if we don't have enough room for the frame. */
    if (fport->writepos + frame_size > fport->buf + fport->bufsize) {
	pj_status_t status;
	status = flush_buffer(fport);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Check if frame is not too large. */
    PJ_ASSERT_RETURN(fport->writepos+frame_size <= fport->buf+fport->bufsize,
		     PJMEDIA_EFRMFILETOOBIG);

    /* Copy frame to buffer. */
    if (fport->fmt_tag == PJMEDIA_WAVE_FMT_TAG_PCM) {
	pj_memcpy(fport->writepos, frame->buf, frame->size);
    } else {
	unsigned i;
	pj_int16_t *src = (pj_int16_t*)frame->buf;
	pj_uint8_t *dst = (pj_uint8_t*)fport->writepos;

	if (fport->fmt_tag == PJMEDIA_WAVE_FMT_TAG_ULAW) {
	    for (i = 0; i < frame_size; ++i) {
		*dst++ = pjmedia_linear2ulaw(*src++);
	    }
	} else {
	    for (i = 0; i < frame_size; ++i) {
		*dst++ = pjmedia_linear2alaw(*src++);
	    }
	}

    }
    fport->writepos += frame_size;

    /* Increment total written, and check if we need to call callback */
    fport->total += frame_size;
    if (fport->cb && fport->total >= fport->cb_size) {
	pj_status_t (*cb)(pjmedia_port*, void*);
	pj_status_t status;

	cb = fport->cb;
	fport->cb = NULL;

	status = (*cb)(this_port, this_port->port_data.pdata);
	return status;
    }

    return PJ_SUCCESS;
}

/*
 * Get frame, basicy is a no-op operation.
 */
static pj_status_t file_get_frame(pjmedia_port *this_port, 
				  pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(this_port);
    PJ_UNUSED_ARG(frame);
    return PJ_EINVALIDOP;
}

/*
 * Close the port, modify file header with updated file length.
 */
static pj_status_t file_on_destroy(pjmedia_port *this_port)
{
    enum { FILE_LEN_POS = 4, DATA_LEN_POS = 40 };
    struct file_port *fport = (struct file_port *)this_port;
    pj_off_t file_size;
    pj_ssize_t bytes;
    pj_uint32_t wave_file_len;
    pj_uint32_t wave_data_len;
    pj_status_t status;
    pj_uint32_t data_len_pos = DATA_LEN_POS;

    /* Flush remaining buffers. */
    if (fport->writepos != fport->buf) 
	flush_buffer(fport);

    /* Get file size. */
    status = pj_file_getpos(fport->fd, &file_size);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Calculate wave fields */
    wave_file_len = (pj_uint32_t)(file_size - 8);
    wave_data_len = (pj_uint32_t)(file_size - sizeof(pjmedia_wave_hdr));

#if defined(PJ_IS_BIG_ENDIAN) && PJ_IS_BIG_ENDIAN!=0
    wave_file_len = pj_swap32(wave_file_len);
    wave_data_len = pj_swap32(wave_data_len);
#endif

    /* Seek to the file_len field. */
    status = pj_file_setpos(fport->fd, FILE_LEN_POS, PJ_SEEK_SET);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Write file_len */
    bytes = sizeof(wave_file_len);
    status = pj_file_write(fport->fd, &wave_file_len, &bytes);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Write samples_len in FACT chunk */
    if (fport->fmt_tag != PJMEDIA_WAVE_FMT_TAG_PCM) {
	enum { SAMPLES_LEN_POS = 44};
	pj_uint32_t wav_samples_len;

	/* Adjust wave_data_len & data_len_pos since there is FACT chunk */
	wave_data_len -= 12;
	data_len_pos += 12;
	wav_samples_len = wave_data_len;

	/* Seek to samples_len field. */
	status = pj_file_setpos(fport->fd, SAMPLES_LEN_POS, PJ_SEEK_SET);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

	/* Write samples_len */
	bytes = sizeof(wav_samples_len);
	status = pj_file_write(fport->fd, &wav_samples_len, &bytes);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    }

    /* Seek to data_len field. */
    status = pj_file_setpos(fport->fd, data_len_pos, PJ_SEEK_SET);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Write file_len */
    bytes = sizeof(wave_data_len);
    status = pj_file_write(fport->fd, &wave_data_len, &bytes);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Close file */
    status = pj_file_close(fport->fd);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Done. */
    return PJ_SUCCESS;
}

