/* $Id: wav_playlist.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * Original author:
 *  David Clark <vdc1048 @ tx.rr.com>
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
#include <pjmedia/wav_playlist.h>
#include <pjmedia/errno.h>
#include <pjmedia/wave.h>
#include <pj/assert.h>
#include <pj/file_access.h>
#include <pj/file_io.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

#define THIS_FILE	    "wav_playlist.c"

#define SIGNATURE	    PJMEDIA_PORT_SIGNATURE('P', 'l', 's', 't')
#define BYTES_PER_SAMPLE    2


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


struct playlist_port
{
    pjmedia_port     base;
    unsigned	     options;
    pj_bool_t	     eof;
    pj_size_t	     bufsize;
    char	    *buf;
    char	    *readpos;

    pj_off_t        *fsize_list;
    unsigned        *start_data_list;
    pj_off_t        *fpos_list;
    pj_oshandle_t   *fd_list;	    /* list of file descriptors	*/
    int              current_file;  /* index of current file.	*/
    int              max_file;	    /* how many files.		*/

    pj_status_t	   (*cb)(pjmedia_port*, void*);
};


static pj_status_t file_list_get_frame(pjmedia_port *this_port,
				       pjmedia_frame *frame);
static pj_status_t file_list_on_destroy(pjmedia_port *this_port);


static struct playlist_port *create_file_list_port(pj_pool_t *pool,
						   const pj_str_t *name)
{
    struct playlist_port *port;

    port = PJ_POOL_ZALLOC_T(pool, struct playlist_port);
    if (!port)
	return NULL;

    /* Put in default values.
     * These will be overriden once the file is read.
     */
    pjmedia_port_info_init(&port->base.info, name, SIGNATURE,
			   8000, 1, 16, 80);

    port->base.get_frame = &file_list_get_frame;
    port->base.on_destroy = &file_list_on_destroy;

    return port;
}


/*
 * Fill buffer for file_list operations.
 */
static pj_status_t file_fill_buffer(struct playlist_port *fport)
{
    pj_ssize_t size_left = fport->bufsize;
    unsigned size_to_read;
    pj_ssize_t size;
    pj_status_t status;
    int current_file = fport->current_file;

    /* Can't read file if EOF and loop flag is disabled */
    if (fport->eof)
	return PJ_EEOF;

    while (size_left > 0)
    {
	/* Calculate how many bytes to read in this run. */
	size = size_to_read = size_left;
	status = pj_file_read(fport->fd_list[current_file],
			      &fport->buf[fport->bufsize-size_left],
			      &size);
	if (status != PJ_SUCCESS)
	    return status;
	
	if (size < 0)
	{
	    /* Should return more appropriate error code here.. */
	    return PJ_ECANCELLED;
	}
	
	size_left -= size;
	fport->fpos_list[current_file] += size;
	
	/* If size is less than size_to_read, it indicates that we've
	 * encountered EOF. Rewind the file.
	 */
	if (size < (pj_ssize_t)size_to_read)
	{
	    /* Rewind the file for the next iteration */
	    fport->fpos_list[current_file] = 
		fport->start_data_list[current_file];
	    pj_file_setpos(fport->fd_list[current_file], 
			   fport->fpos_list[current_file], PJ_SEEK_SET);

	    /* Move to next file */
	    current_file++;
	    fport->current_file = current_file;

	    if (fport->current_file == fport->max_file)
	    {
		/* Clear the remaining part of the buffer first, to prevent
		 * old samples from being played. If the playback restarts,
		 * this will be overwritten by new reading.
		 */
		if (size_left > 0) {
		    pj_bzero(&fport->buf[fport->bufsize-size_left], 
			     size_left);
		}

		/* All files have been played. Call callback, if any. */
		if (fport->cb)
		{
		    PJ_LOG(5,(THIS_FILE,
			      "File port %.*s EOF, calling callback",
			      (int)fport->base.info.name.slen,
			      fport->base.info.name.ptr));
		    
		    fport->eof = PJ_TRUE;

		    status = (*fport->cb)(&fport->base,
					  fport->base.port_data.pdata);

		    if (status != PJ_SUCCESS)
		    {
			/* This will crash if file port is destroyed in the
			 * callback, that's why we set the eof flag before
			 * calling the callback:
			 fport->eof = PJ_TRUE;
			 */
			return status;
		    }

		    fport->eof = PJ_FALSE;
		}


		if (fport->options & PJMEDIA_FILE_NO_LOOP)
		{
		    PJ_LOG(5,(THIS_FILE, "File port %.*s EOF, stopping..",
			      (int)fport->base.info.name.slen,
			      fport->base.info.name.ptr));
		    fport->eof = PJ_TRUE;
		    return PJ_EEOF;
		}
		else
		{
		    PJ_LOG(5,(THIS_FILE, "File port %.*s EOF, rewinding..",
			      (int)fport->base.info.name.slen,
			      fport->base.info.name.ptr));
		    
		    /* start with first file again. */
		    fport->current_file = current_file = 0;
		    fport->fpos_list[0] = fport->start_data_list[0];
		    pj_file_setpos(fport->fd_list[0], fport->fpos_list[0],
				   PJ_SEEK_SET);
		}		
		
	    } /* if current_file == max_file */

	} /* size < size_to_read */

    } /* while () */
    
    /* Convert samples to host rep */
    samples_to_host((pj_int16_t*)fport->buf, fport->bufsize/BYTES_PER_SAMPLE);
    
    return PJ_SUCCESS;
}


/*
 * Create wave list player.
 */
PJ_DEF(pj_status_t) pjmedia_wav_playlist_create(pj_pool_t *pool,
						const pj_str_t *port_label,
						const pj_str_t file_list[],
						int file_count,
						unsigned ptime,
						unsigned options,
						pj_ssize_t buff_size,
						pjmedia_port **p_port)
{
    struct playlist_port *fport;
    pj_off_t pos;
    pj_status_t status;
    int index;
    pj_bool_t has_wave_info = PJ_FALSE;
    pj_str_t tmp_port_label;
    char filename[PJ_MAXPATH];	/* filename for open operations.    */


    /* Check arguments. */
    PJ_ASSERT_RETURN(pool && file_list && file_count && p_port, PJ_EINVAL);

    /* Normalize port_label */
    if (port_label == NULL || port_label->slen == 0) {
	tmp_port_label = pj_str("WAV playlist");
	port_label = &tmp_port_label;
    }

    /* Be sure all files exist	*/
    for (index=0; index<file_count; index++) {

	PJ_ASSERT_RETURN(file_list[index].slen < PJ_MAXPATH, PJ_ENAMETOOLONG);

	pj_memcpy(filename, file_list[index].ptr, file_list[index].slen);
	filename[file_list[index].slen] = '\0';

    	/* Check the file really exists. */
    	if (!pj_file_exists(filename)) {
	    PJ_LOG(4,(THIS_FILE,
		      "WAV playlist error: file '%s' not found",
	      	      filename));
	    return PJ_ENOTFOUND;
    	}
    }

    /* Normalize ptime */
    if (ptime == 0)
	ptime = 20;

    /* Create fport instance. */
    fport = create_file_list_port(pool, port_label);
    if (!fport) {
	return PJ_ENOMEM;
    }

    /* start with the first file. */
    fport->current_file = 0;
    fport->max_file = file_count;

    /* Create file descriptor list */
    fport->fd_list = (pj_oshandle_t*)
		     pj_pool_zalloc(pool, sizeof(pj_oshandle_t)*file_count);
    if (!fport->fd_list) {
	return PJ_ENOMEM;
    }

    /* Create file size list */
    fport->fsize_list = (pj_off_t*)
			pj_pool_alloc(pool, sizeof(pj_off_t)*file_count);
    if (!fport->fsize_list) {
	return PJ_ENOMEM;
    }

    /* Create start of WAVE data list */
    fport->start_data_list = (unsigned*)
			     pj_pool_alloc(pool, sizeof(unsigned)*file_count);
    if (!fport->start_data_list) {
	return PJ_ENOMEM;
    }

    /* Create file position list */
    fport->fpos_list = (pj_off_t*)
		       pj_pool_alloc(pool, sizeof(pj_off_t)*file_count);
    if (!fport->fpos_list) {
	return PJ_ENOMEM;
    }

    /* Create file buffer once for this operation.
     */
    if (buff_size < 1) buff_size = PJMEDIA_FILE_PORT_BUFSIZE;
    fport->bufsize = buff_size;


    /* Create buffer. */
    fport->buf = (char*) pj_pool_alloc(pool, fport->bufsize);
    if (!fport->buf) {
	return PJ_ENOMEM;
    }

    /* Initialize port */
    fport->options = options;
    fport->readpos = fport->buf;


    /* ok run this for all files to be sure all are good for playback. */
    for (index=file_count-1; index>=0; index--) {

	pjmedia_wave_hdr wavehdr;
	pj_ssize_t size_to_read, size_read;

	/* we end with the last one so we are good to go if still in function*/
	pj_memcpy(filename, file_list[index].ptr, file_list[index].slen);
	filename[file_list[index].slen] = '\0';

	/* Get the file size. */
	fport->current_file = index;
	fport->fsize_list[index] = pj_file_size(filename);
	
	/* Size must be more than WAVE header size */
	if (fport->fsize_list[index] <= sizeof(pjmedia_wave_hdr)) {
	    status = PJMEDIA_ENOTVALIDWAVE;
	    goto on_error;
	}
	
	/* Open file. */
	status = pj_file_open( pool, filename, PJ_O_RDONLY, 
			       &fport->fd_list[index]);
	if (status != PJ_SUCCESS)
	    goto on_error;
	
	/* Read the file header plus fmt header only. */
	size_read = size_to_read = sizeof(wavehdr) - 8;
	status = pj_file_read( fport->fd_list[index], &wavehdr, &size_read);
	if (status != PJ_SUCCESS) {
	    goto on_error;
	}

	if (size_read != size_to_read) {
	    status = PJMEDIA_ENOTVALIDWAVE;
	    goto on_error;
	}
	
	/* Normalize WAVE header fields values from little-endian to host
	 * byte order.
	 */
	pjmedia_wave_hdr_file_to_host(&wavehdr);
	
	/* Validate WAVE file. */
	if (wavehdr.riff_hdr.riff != PJMEDIA_RIFF_TAG ||
	    wavehdr.riff_hdr.wave != PJMEDIA_WAVE_TAG ||
	    wavehdr.fmt_hdr.fmt != PJMEDIA_FMT_TAG)
	{
	    TRACE_((THIS_FILE,
		"actual value|expected riff=%x|%x, wave=%x|%x fmt=%x|%x",
		wavehdr.riff_hdr.riff, PJMEDIA_RIFF_TAG,
		wavehdr.riff_hdr.wave, PJMEDIA_WAVE_TAG,
		wavehdr.fmt_hdr.fmt, PJMEDIA_FMT_TAG));
	    status = PJMEDIA_ENOTVALIDWAVE;
	    goto on_error;
	}
	
	/* Must be PCM with 16bits per sample */
	if (wavehdr.fmt_hdr.fmt_tag != 1 ||
	    wavehdr.fmt_hdr.bits_per_sample != 16)
	{
	    status = PJMEDIA_EWAVEUNSUPP;
	    goto on_error;
	}
	
	/* Block align must be 2*nchannels */
	if (wavehdr.fmt_hdr.block_align != 
		wavehdr.fmt_hdr.nchan * BYTES_PER_SAMPLE)
	{
	    status = PJMEDIA_EWAVEUNSUPP;
	    goto on_error;
	}
	
	/* If length of fmt_header is greater than 16, skip the remaining
	 * fmt header data.
	 */
	if (wavehdr.fmt_hdr.len > 16) {
	    size_to_read = wavehdr.fmt_hdr.len - 16;
	    status = pj_file_setpos(fport->fd_list[index], size_to_read, 
				    PJ_SEEK_CUR);
	    if (status != PJ_SUCCESS) {
		goto on_error;
	    }
	}
	
	/* Repeat reading the WAVE file until we have 'data' chunk */
	for (;;) {
	    pjmedia_wave_subchunk subchunk;
	    size_read = 8;
	    status = pj_file_read(fport->fd_list[index], &subchunk, 
				  &size_read);
	    if (status != PJ_SUCCESS || size_read != 8) {
		status = PJMEDIA_EWAVETOOSHORT;
		goto on_error;
	    }
	    
	    /* Normalize endianness */
	    PJMEDIA_WAVE_NORMALIZE_SUBCHUNK(&subchunk);
	    
	    /* Break if this is "data" chunk */
	    if (subchunk.id == PJMEDIA_DATA_TAG) {
		wavehdr.data_hdr.data = PJMEDIA_DATA_TAG;
		wavehdr.data_hdr.len = subchunk.len;
		break;
	    }
	    
	    /* Otherwise skip the chunk contents */
	    size_to_read = subchunk.len;
	    status = pj_file_setpos(fport->fd_list[index], size_to_read, 
				    PJ_SEEK_CUR);
	    if (status != PJ_SUCCESS) {
		goto on_error;
	    }
	}
	
	/* Current file position now points to start of data */
	status = pj_file_getpos(fport->fd_list[index], &pos);
	fport->start_data_list[index] = (unsigned)pos;
	
	/* Validate length. */
	if (wavehdr.data_hdr.len != fport->fsize_list[index] - 
				       fport->start_data_list[index]) 
	{
	    status = PJMEDIA_EWAVEUNSUPP;
	    goto on_error;
	}
	if (wavehdr.data_hdr.len < 400) {
	    status = PJMEDIA_EWAVETOOSHORT;
	    goto on_error;
	}
	
	/* It seems like we have a valid WAVE file. */
	
	/* Update port info if we don't have one, otherwise check
	 * that the WAV file has the same attributes as previous files. 
	 */
	if (!has_wave_info) {
	    fport->base.info.channel_count = wavehdr.fmt_hdr.nchan;
	    fport->base.info.clock_rate = wavehdr.fmt_hdr.sample_rate;
	    fport->base.info.bits_per_sample = wavehdr.fmt_hdr.bits_per_sample;
	    fport->base.info.samples_per_frame = fport->base.info.clock_rate *
						 wavehdr.fmt_hdr.nchan *
						 ptime / 1000;
	    fport->base.info.bytes_per_frame =
		fport->base.info.samples_per_frame *
		fport->base.info.bits_per_sample / 8;

	    has_wave_info = PJ_TRUE;

	} else {

	    /* Check that this file has the same characteristics as the other
	     * files.
	     */
	    if (wavehdr.fmt_hdr.nchan != fport->base.info.channel_count ||
		wavehdr.fmt_hdr.sample_rate != fport->base.info.clock_rate ||
		wavehdr.fmt_hdr.bits_per_sample != fport->base.info.bits_per_sample)
	    {
		/* This file has different characteristics than the other 
		 * files. 
		 */
		PJ_LOG(4,(THIS_FILE,
		          "WAV playlist error: file '%s' has differrent number"
			  " of channels, sample rate, or bits per sample",
	      		  filename));
		status = PJMEDIA_EWAVEUNSUPP;
		goto on_error;
	    }

	}
	
	
	/* Set initial position of the file. */
	fport->fpos_list[index] = fport->start_data_list[index];
    }

    /* Fill up the buffer. */
    status = file_fill_buffer(fport);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }
    
    /* Done. */
    
    *p_port = &fport->base;
    
    PJ_LOG(4,(THIS_FILE,
	     "WAV playlist '%.*s' created: samp.rate=%d, ch=%d, bufsize=%uKB",
	     (int)port_label->slen,
	     port_label->ptr,
	     fport->base.info.clock_rate,
	     fport->base.info.channel_count,
	     fport->bufsize / 1000));
    
    return PJ_SUCCESS;

on_error:
    for (index=0; index<file_count; ++index) {
	if (fport->fd_list[index] != 0)
	    pj_file_close(fport->fd_list[index]);
    }

    return status;
}


/*
 * Register a callback to be called when the file reading has reached the
 * end of the last file.
 */
PJ_DEF(pj_status_t) pjmedia_wav_playlist_set_eof_cb(pjmedia_port *port,
			        void *user_data,
			        pj_status_t (*cb)(pjmedia_port *port,
						  void *usr_data))
{
    struct playlist_port *fport;

    /* Sanity check */
    PJ_ASSERT_RETURN(port, PJ_EINVAL);

    /* Check that this is really a playlist port */
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, PJ_EINVALIDOP);

    fport = (struct playlist_port*) port;

    fport->base.port_data.pdata = user_data;
    fport->cb = cb;

    return PJ_SUCCESS;
}


/*
 * Get frame from file for file_list operation
 */
static pj_status_t file_list_get_frame(pjmedia_port *this_port,
				       pjmedia_frame *frame)
{
    struct playlist_port *fport = (struct playlist_port*)this_port;
    unsigned frame_size;
    pj_status_t status;

    pj_assert(fport->base.info.signature == SIGNATURE);

    //frame_size = fport->base.info.bytes_per_frame;
    //pj_assert(frame->size == frame_size);
    frame_size = frame->size;

    /* Copy frame from buffer. */
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame->size = frame_size;
    frame->timestamp.u64 = 0;

    if (fport->readpos + frame_size <= fport->buf + fport->bufsize) {

	/* Read contiguous buffer. */
	pj_memcpy(frame->buf, fport->readpos, frame_size);

	/* Fill up the buffer if all has been read. */
	fport->readpos += frame_size;
	if (fport->readpos == fport->buf + fport->bufsize) {
	    fport->readpos = fport->buf;

	    status = file_fill_buffer(fport);
	    if (status != PJ_SUCCESS) {
		frame->type = PJMEDIA_FRAME_TYPE_NONE;
		frame->size = 0;
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

	/* Second stage: fill up buffer, and read from the start of buffer. */
	status = file_fill_buffer(fport);
	if (status != PJ_SUCCESS) {
	    pj_bzero(((char*)frame->buf)+endread, frame_size-endread);
	    return status;
	}

	pj_memcpy(((char*)frame->buf)+endread, fport->buf, frame_size-endread);
	fport->readpos = fport->buf + (frame_size - endread);
    }

    return PJ_SUCCESS;
}


/*
 * Destroy port.
 */
static pj_status_t file_list_on_destroy(pjmedia_port *this_port)
{
    struct playlist_port *fport = (struct playlist_port*) this_port;
    int index;

    pj_assert(this_port->info.signature == SIGNATURE);

    for (index=0; index<fport->max_file; index++)
	pj_file_close(fport->fd_list[index]);

    return PJ_SUCCESS;
}

