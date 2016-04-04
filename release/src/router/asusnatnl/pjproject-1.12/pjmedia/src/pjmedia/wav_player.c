/* $Id: wav_player.c 4116 2012-04-30 17:33:03Z nanang $ */
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


#define THIS_FILE   "wav_player.c"


#define SIGNATURE	    PJMEDIA_PORT_SIGNATURE('F', 'P', 'l', 'y')
#define BITS_PER_SAMPLE	    16

#if 1
#   define TRACE_(x)	PJ_LOG(4,x)
#else
#   define TRACE_(x)
#endif

#if defined(PJ_IS_BIG_ENDIAN) && PJ_IS_BIG_ENDIAN!=0
    static void samples_to_host(pj_int16_t *samples, unsigned count)
    {
	unsigned i;
	for (i=0; i<count; ++i) {
	    samples[i] = pj_swap16(samples[i]);
	}
    }
#else
#   define samples_to_host(samples,count)
#endif

struct file_reader_port
{
    pjmedia_port     base;
    unsigned	     options;
    pjmedia_wave_fmt_tag fmt_tag;
    pj_uint16_t	     bytes_per_sample;
    pj_bool_t	     eof;
    pj_size_t	     bufsize;
    char	    *buf;
    char	    *readpos;
    char	    *eofpos;

    pj_off_t	     fsize;
    unsigned	     start_data;
    unsigned         data_len;
    unsigned         data_left;
    pj_off_t	     fpos;
    pj_oshandle_t    fd;

    pj_status_t	   (*cb)(pjmedia_port*, void*);
};


static pj_status_t file_get_frame(pjmedia_port *this_port, 
				  pjmedia_frame *frame);
static pj_status_t file_on_destroy(pjmedia_port *this_port);

static struct file_reader_port *create_file_port(pj_pool_t *pool)
{
    const pj_str_t name = pj_str("file");
    struct file_reader_port *port;

    port = PJ_POOL_ZALLOC_T(pool, struct file_reader_port);
    if (!port)
	return NULL;

    /* Put in default values.
     * These will be overriden once the file is read.
     */
    pjmedia_port_info_init(&port->base.info, &name, SIGNATURE, 
			   8000, 1, 16, 80);

    port->base.get_frame = &file_get_frame;
    port->base.on_destroy = &file_on_destroy;


    return port;
}

/*
 * Fill buffer.
 */
static pj_status_t fill_buffer(struct file_reader_port *fport)
{
    pj_ssize_t size_left = fport->bufsize;
    unsigned size_to_read;
    pj_ssize_t size;
    pj_status_t status;

    fport->eofpos = NULL;
    
    while (size_left > 0) {

	/* Calculate how many bytes to read in this run. */
	size = size_to_read = size_left;
	status = pj_file_read(fport->fd, 
			      &fport->buf[fport->bufsize-size_left], 
			      &size);
	if (status != PJ_SUCCESS)
	    return status;
	if (size < 0) {
	    /* Should return more appropriate error code here.. */
	    return PJ_ECANCELLED;
	}

        if (size > (pj_ssize_t)fport->data_left) {
            /* We passed the end of the data chunk,
             * only count the portion read from the data chunk.
             */
            size = (pj_ssize_t)fport->data_left;
        }

	size_left -= size;
        fport->data_left -= size;
	fport->fpos += size;

	/* If size is less than size_to_read, it indicates that we've
	 * encountered EOF. Rewind the file.
	 */
        if (size < (pj_ssize_t)size_to_read) {
            fport->eof = PJ_TRUE;
            fport->eofpos = fport->buf + fport->bufsize - size_left;

            if (fport->options & PJMEDIA_FILE_NO_LOOP) {
                /* Zero remaining buffer */
                if (fport->fmt_tag == PJMEDIA_WAVE_FMT_TAG_PCM) {
                    pj_bzero(fport->eofpos, size_left);
                } else if (fport->fmt_tag == PJMEDIA_WAVE_FMT_TAG_ULAW) {
                    int val = pjmedia_linear2ulaw(0);
                    pj_memset(fport->eofpos, val, size_left);
                } else if (fport->fmt_tag == PJMEDIA_WAVE_FMT_TAG_ALAW) {
                    int val = pjmedia_linear2alaw(0);
                    pj_memset(fport->eofpos, val, size_left);
                }
		size_left = 0;
            }

	    /* Rewind file */
	    fport->fpos = fport->start_data;
	    pj_file_setpos( fport->fd, fport->fpos, PJ_SEEK_SET);
            fport->data_left = fport->data_len;
	}
    }

    /* Convert samples to host rep */
    samples_to_host((pj_int16_t*)fport->buf, 
		    fport->bufsize/fport->bytes_per_sample);

    return PJ_SUCCESS;
}


/*
 * Create WAVE player port.
 */
PJ_DEF(pj_status_t) pjmedia_wav_player_port_create( pj_pool_t *pool,
						     const char *filename,
						     unsigned ptime,
						     unsigned options,
						     pj_ssize_t buff_size,
						     pjmedia_port **p_port )
{
    pjmedia_wave_hdr wave_hdr;
    pj_ssize_t size_to_read, size_read;
    struct file_reader_port *fport;
    pj_off_t pos;
    pj_status_t status = PJ_SUCCESS;


    /* Check arguments. */
    PJ_ASSERT_RETURN(pool && filename && p_port, PJ_EINVAL);

    /* Check the file really exists. */
	if (!pj_file_exists(filename)) {
		PJ_LOG(4, ("wav_player.c", "pjmedia_wav_player_port_create() file not found."));
		return PJ_ENOTFOUND;
    }

    /* Normalize ptime */
    if (ptime == 0)
	ptime = 20;

    /* Normalize buff_size */
    if (buff_size < 1) buff_size = PJMEDIA_FILE_PORT_BUFSIZE;


    /* Create fport instance. */
    fport = create_file_port(pool);
    if (!fport) {
	return PJ_ENOMEM;
    }


    /* Get the file size. */
    fport->fsize = pj_file_size(filename);

    /* Size must be more than WAVE header size */
    if (fport->fsize <= sizeof(pjmedia_wave_hdr)) {
	return PJMEDIA_ENOTVALIDWAVE;
    }

    /* Open file. */
    status = pj_file_open( pool, filename, PJ_O_RDONLY, &fport->fd, NULL);
    if (status != PJ_SUCCESS)
	return status;

    /* Read the file header plus fmt header only. */
    size_read = size_to_read = sizeof(wave_hdr) - 8;
    status = pj_file_read( fport->fd, &wave_hdr, &size_read);
    if (status != PJ_SUCCESS) {
	pj_file_close(fport->fd);
	return status;
    }
    if (size_read != size_to_read) {
	pj_file_close(fport->fd);
	return PJMEDIA_ENOTVALIDWAVE;
    }

    /* Normalize WAVE header fields values from little-endian to host
     * byte order.
     */
    pjmedia_wave_hdr_file_to_host(&wave_hdr);
    
    /* Validate WAVE file. */
    if (wave_hdr.riff_hdr.riff != PJMEDIA_RIFF_TAG ||
	wave_hdr.riff_hdr.wave != PJMEDIA_WAVE_TAG ||
	wave_hdr.fmt_hdr.fmt != PJMEDIA_FMT_TAG)
    {
	pj_file_close(fport->fd);
	TRACE_((THIS_FILE, 
		"actual value|expected riff=%x|%x, wave=%x|%x fmt=%x|%x",
		wave_hdr.riff_hdr.riff, PJMEDIA_RIFF_TAG,
		wave_hdr.riff_hdr.wave, PJMEDIA_WAVE_TAG,
		wave_hdr.fmt_hdr.fmt, PJMEDIA_FMT_TAG));
	return PJMEDIA_ENOTVALIDWAVE;
    }

    /* Validate format and its attributes (i.e: bits per sample, block align) */
    switch (wave_hdr.fmt_hdr.fmt_tag) {
    case PJMEDIA_WAVE_FMT_TAG_PCM:
	if (wave_hdr.fmt_hdr.bits_per_sample != 16 || 
	    wave_hdr.fmt_hdr.block_align != 2 * wave_hdr.fmt_hdr.nchan)
	    status = PJMEDIA_EWAVEUNSUPP;
	break;

    case PJMEDIA_WAVE_FMT_TAG_ALAW:
    case PJMEDIA_WAVE_FMT_TAG_ULAW:
	if (wave_hdr.fmt_hdr.bits_per_sample != 8 ||
	    wave_hdr.fmt_hdr.block_align != wave_hdr.fmt_hdr.nchan)
	    status = PJMEDIA_ENOTVALIDWAVE;
	break;

    default:
	status = PJMEDIA_EWAVEUNSUPP;
	break;
    }

    if (status != PJ_SUCCESS) {
	pj_file_close(fport->fd);
	return status;
    }

    fport->fmt_tag = (pjmedia_wave_fmt_tag)wave_hdr.fmt_hdr.fmt_tag;
    fport->bytes_per_sample = (pj_uint16_t) 
			      (wave_hdr.fmt_hdr.bits_per_sample / 8);

    /* If length of fmt_header is greater than 16, skip the remaining
     * fmt header data.
     */
    if (wave_hdr.fmt_hdr.len > 16) {
	size_to_read = wave_hdr.fmt_hdr.len - 16;
	status = pj_file_setpos(fport->fd, size_to_read, PJ_SEEK_CUR);
	if (status != PJ_SUCCESS) {
	    pj_file_close(fport->fd);
	    return status;
	}
    }

    /* Repeat reading the WAVE file until we have 'data' chunk */
    for (;;) {
	pjmedia_wave_subchunk subchunk;
	size_read = 8;
	status = pj_file_read(fport->fd, &subchunk, &size_read);
	if (status != PJ_SUCCESS || size_read != 8) {
	    pj_file_close(fport->fd);
	    return PJMEDIA_EWAVETOOSHORT;
	}

	/* Normalize endianness */
	PJMEDIA_WAVE_NORMALIZE_SUBCHUNK(&subchunk);

	/* Break if this is "data" chunk */
	if (subchunk.id == PJMEDIA_DATA_TAG) {
	    wave_hdr.data_hdr.data = PJMEDIA_DATA_TAG;
	    wave_hdr.data_hdr.len = subchunk.len;
	    break;
	}

	/* Otherwise skip the chunk contents */
	size_to_read = subchunk.len;
	status = pj_file_setpos(fport->fd, size_to_read, PJ_SEEK_CUR);
	if (status != PJ_SUCCESS) {
	    pj_file_close(fport->fd);
	    return status;
	}
    }

    /* Current file position now points to start of data */
    status = pj_file_getpos(fport->fd, &pos);
    fport->start_data = (unsigned)pos;
    fport->data_len = wave_hdr.data_hdr.len;
    fport->data_left = wave_hdr.data_hdr.len;

    /* Validate length. */
    if (wave_hdr.data_hdr.len > fport->fsize - fport->start_data) {
	pj_file_close(fport->fd);
	return PJMEDIA_EWAVEUNSUPP;
    }
    if (wave_hdr.data_hdr.len < ptime * wave_hdr.fmt_hdr.sample_rate *
				wave_hdr.fmt_hdr.nchan / 1000)
    {
	pj_file_close(fport->fd);
	return PJMEDIA_EWAVETOOSHORT;
    }

    /* It seems like we have a valid WAVE file. */

    /* Initialize */
    fport->options = options;

    /* Update port info. */
    fport->base.info.channel_count = wave_hdr.fmt_hdr.nchan;
    fport->base.info.clock_rate = wave_hdr.fmt_hdr.sample_rate;
    fport->base.info.bits_per_sample = BITS_PER_SAMPLE;
    fport->base.info.samples_per_frame = fport->base.info.clock_rate *
					 wave_hdr.fmt_hdr.nchan *
					 ptime / 1000;
    fport->base.info.bytes_per_frame = 
	fport->base.info.samples_per_frame * 
	fport->base.info.bits_per_sample / 8;

    pj_strdup2(pool, &fport->base.info.name, filename);

    /* If file is shorter than buffer size, adjust buffer size to file
     * size. Otherwise EOF callback will be called multiple times when
     * fill_buffer() is called.
     */
    if (wave_hdr.data_hdr.len < (unsigned)buff_size)
	buff_size = wave_hdr.data_hdr.len;

    /* Create file buffer.
     */
    fport->bufsize = buff_size;


    /* samples_per_frame must be smaller than bufsize (because get_frame()
     * doesn't handle this case).
     */
    if (fport->base.info.samples_per_frame * fport->bytes_per_sample >=
	fport->bufsize)
    {
	pj_file_close(fport->fd);
	return PJ_EINVAL;
    }

    /* Create buffer. */
    fport->buf = (char*) pj_pool_alloc(pool, fport->bufsize);
    if (!fport->buf) {
	pj_file_close(fport->fd);
	return PJ_ENOMEM;
    }
 
    fport->readpos = fport->buf;

    /* Set initial position of the file. */
    fport->fpos = fport->start_data;

    /* Fill up the buffer. */
    status = fill_buffer(fport);
    if (status != PJ_SUCCESS) {
	pj_file_close(fport->fd);
	return status;
    }

    /* Done. */

    *p_port = &fport->base;


    PJ_LOG(4,(THIS_FILE, 
	      "File player '%.*s' created: samp.rate=%d, ch=%d, bufsize=%uKB, "
	      "filesize=%luKB",
	      (int)fport->base.info.name.slen,
	      fport->base.info.name.ptr,
	      fport->base.info.clock_rate,
	      fport->base.info.channel_count,
	      fport->bufsize / 1000,
	      (unsigned long)(fport->fsize / 1000)));

    return PJ_SUCCESS;
}


/*
 * Get the data length, in bytes.
 */
PJ_DEF(pj_ssize_t) pjmedia_wav_player_get_len(pjmedia_port *port)
{
    struct file_reader_port *fport;
    pj_ssize_t size;

    /* Sanity check */
    PJ_ASSERT_RETURN(port, -PJ_EINVAL);

    /* Check that this is really a player port */
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, -PJ_EINVALIDOP);

    fport = (struct file_reader_port*) port;

    size = (pj_ssize_t) fport->fsize;
    return size - fport->start_data;
}


/*
 * Set position.
 */
PJ_DEF(pj_status_t) pjmedia_wav_player_port_set_pos(pjmedia_port *port,
						    pj_uint32_t bytes )
{
    struct file_reader_port *fport;
    pj_status_t status;

    /* Sanity check */
    PJ_ASSERT_RETURN(port, PJ_EINVAL);

    /* Check that this is really a player port */
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, PJ_EINVALIDOP);


    fport = (struct file_reader_port*) port;

    /* Check that this offset does not pass the audio-data (in case of
     * extra chunk after audio data chunk
     */
    PJ_ASSERT_RETURN(bytes < fport->data_len, PJ_EINVAL);

    fport->fpos = fport->start_data + bytes;
    fport->data_left = fport->data_len - bytes;
    pj_file_setpos( fport->fd, fport->fpos, PJ_SEEK_SET);

    fport->eof = PJ_FALSE;
    status = fill_buffer(fport);
    if (status != PJ_SUCCESS)
	return status;

    fport->readpos = fport->buf;

    return PJ_SUCCESS;
}


/*
 * Get the file play position of WAV player.
 */
PJ_DEF(pj_ssize_t) pjmedia_wav_player_port_get_pos( pjmedia_port *port )
{
    struct file_reader_port *fport;
    pj_size_t payload_pos;

    /* Sanity check */
    PJ_ASSERT_RETURN(port, -PJ_EINVAL);

    /* Check that this is really a player port */
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, -PJ_EINVALIDOP);

    fport = (struct file_reader_port*) port;

    payload_pos = (pj_size_t)(fport->fpos - fport->start_data);
    if (payload_pos >= fport->bufsize)
	return payload_pos - fport->bufsize + (fport->readpos - fport->buf);
    else
	return (fport->readpos - fport->buf) % payload_pos;
}



/*
 * Register a callback to be called when the file reading has reached the
 * end of file.
 */
PJ_DEF(pj_status_t) pjmedia_wav_player_set_eof_cb( pjmedia_port *port,
			       void *user_data,
			       pj_status_t (*cb)(pjmedia_port *port,
						 void *usr_data))
{
    struct file_reader_port *fport;

    /* Sanity check */
    PJ_ASSERT_RETURN(port, -PJ_EINVAL);

    /* Check that this is really a player port */
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, -PJ_EINVALIDOP);

    fport = (struct file_reader_port*) port;

    fport->base.port_data.pdata = user_data;
    fport->cb = cb;

    return PJ_SUCCESS;
}


/*
 * Get frame from file.
 */
static pj_status_t file_get_frame(pjmedia_port *this_port, 
				  pjmedia_frame *frame)
{
    struct file_reader_port *fport = (struct file_reader_port*)this_port;
    unsigned frame_size;
    pj_status_t status;

    pj_assert(fport->base.info.signature == SIGNATURE);
    pj_assert(frame->size <= fport->bufsize);

    /* EOF is set and readpos already passed the eofpos */
    if (fport->eof && fport->readpos >= fport->eofpos) {
	pj_status_t status = PJ_SUCCESS;

	PJ_LOG(5,(THIS_FILE, "File port %.*s EOF",
		  (int)fport->base.info.name.slen,
		  fport->base.info.name.ptr));

	/* Call callback, if any */
	if (fport->cb)
	    status = (*fport->cb)(this_port, fport->base.port_data.pdata);

	/* If callback returns non PJ_SUCCESS or 'no loop' is specified,
	 * return immediately (and don't try to access player port since
	 * it might have been destroyed by the callback).
	 */
	if ((status != PJ_SUCCESS) || (fport->options & PJMEDIA_FILE_NO_LOOP)) {
	    frame->type = PJMEDIA_FRAME_TYPE_NONE;
	    frame->size = 0;
	    return PJ_EEOF;
	}
    	
	PJ_LOG(5,(THIS_FILE, "File port %.*s rewinding..",
		  (int)fport->base.info.name.slen,
		  fport->base.info.name.ptr));
	
	fport->eof = PJ_FALSE;
    }

    //pj_assert(frame->size == fport->base.info.bytes_per_frame);
    if (fport->fmt_tag == PJMEDIA_WAVE_FMT_TAG_PCM) {
	frame_size = frame->size;
	//frame->size = frame_size;
    } else {
	/* Must be ULAW or ALAW */
	pj_assert(fport->fmt_tag == PJMEDIA_WAVE_FMT_TAG_ULAW || 
		  fport->fmt_tag == PJMEDIA_WAVE_FMT_TAG_ALAW);

	frame_size = frame->size >> 1;
	frame->size = frame_size << 1;
    }

    /* Copy frame from buffer. */
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame->timestamp.u64 = 0;

    if ((fport->readpos + frame_size) <= (fport->buf + fport->bufsize))
    {
	/* Read contiguous buffer. */
	pj_memcpy(frame->buf, fport->readpos, frame_size);

	/* Fill up the buffer if all has been read. */
	fport->readpos += frame_size;
	if (fport->readpos == fport->buf + fport->bufsize) {
	    fport->readpos = fport->buf;

	    status = fill_buffer(fport);
	    if (status != PJ_SUCCESS) {
		frame->type = PJMEDIA_FRAME_TYPE_NONE;
		frame->size = 0;
		fport->readpos = fport->buf + fport->bufsize;
		return status;
	    }
	}
    } else {
	unsigned endread;

	/* Split read.
	 * First stage: read until end of buffer. 
	 */
	endread = (fport->buf+fport->bufsize) - fport->readpos;
	pj_memcpy(frame->buf, fport->readpos, endread);

	/* End Of Buffer and EOF and NO LOOP */
	if (fport->eof && (fport->options & PJMEDIA_FILE_NO_LOOP)) {
	    fport->readpos += endread;

            if (fport->fmt_tag == PJMEDIA_WAVE_FMT_TAG_PCM) {
                pj_bzero((char*)frame->buf + endread, frame_size - endread);
            } else if (fport->fmt_tag == PJMEDIA_WAVE_FMT_TAG_ULAW) {
                int val = pjmedia_linear2ulaw(0);
                pj_memset((char*)frame->buf + endread, val,
                          frame_size - endread);
            } else if (fport->fmt_tag == PJMEDIA_WAVE_FMT_TAG_ALAW) {
                int val = pjmedia_linear2alaw(0);
                pj_memset((char*)frame->buf + endread, val,
                          frame_size - endread);
            }

	    return PJ_SUCCESS;
	}

	/* Second stage: fill up buffer, and read from the start of buffer. */
	status = fill_buffer(fport);
	if (status != PJ_SUCCESS) {
	    frame->type = PJMEDIA_FRAME_TYPE_NONE;
	    frame->size = 0;
	    fport->readpos = fport->buf + fport->bufsize;
	    return status;
	}

	pj_memcpy(((char*)frame->buf)+endread, fport->buf, frame_size-endread);
	fport->readpos = fport->buf + (frame_size - endread);
    }

    if (fport->fmt_tag == PJMEDIA_WAVE_FMT_TAG_ULAW ||
	fport->fmt_tag == PJMEDIA_WAVE_FMT_TAG_ALAW)
    {
	unsigned i;
	pj_uint16_t *dst;
	pj_uint8_t *src;

	dst = (pj_uint16_t*)frame->buf + frame_size - 1;
	src = (pj_uint8_t*)frame->buf + frame_size - 1;

	if (fport->fmt_tag == PJMEDIA_WAVE_FMT_TAG_ULAW) {
	    for (i = 0; i < frame_size; ++i) {
		*dst-- = (pj_uint16_t) pjmedia_ulaw2linear(*src--);
	    }
	} else {
	    for (i = 0; i < frame_size; ++i) {
		*dst-- = (pj_uint16_t) pjmedia_alaw2linear(*src--);
	    }
	}
    }

    return PJ_SUCCESS;
}

/*
 * Destroy port.
 */
static pj_status_t file_on_destroy(pjmedia_port *this_port)
{
    struct file_reader_port *fport = (struct file_reader_port*) this_port;

    pj_assert(this_port->info.signature == SIGNATURE);

    pj_file_close(fport->fd);
    return PJ_SUCCESS;
}

