/* $Id: mp3_writer.c 1233 2007-04-30 11:05:23Z bennylp $ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
 * Contributed by:
 *  Toni < buldozer at aufbix dot org >
 */
#include "mp3_port.h"
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/file_access.h>
#include <pj/file_io.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/unicode.h>


/* Include BladeDLL declarations */
#include "BladeMP3EncDLL.h"


#define THIS_FILE	    "mp3_writer.c"
#define SIGNATURE	    PJMEDIA_PORT_SIGNATURE('F', 'W', 'M', '3')
#define BYTES_PER_SAMPLE    2

static struct BladeDLL
{
    void		*hModule;
    int			 refCount;
    BEINITSTREAM	 beInitStream;
    BEENCODECHUNK	 beEncodeChunk;
    BEDEINITSTREAM	 beDeinitStream;
    BECLOSESTREAM	 beCloseStream;
    BEVERSION		 beVersion;
    BEWRITEVBRHEADER	 beWriteVBRHeader;
    BEWRITEINFOTAG	 beWriteInfoTag;
} BladeDLL;


struct mp3_file_port
{
    pjmedia_port    base;
    pj_size_t	    total;
    pj_oshandle_t   fd;
    pj_size_t	    cb_size;
    pj_status_t	   (*cb)(pjmedia_port*, void*);

    unsigned	    silence_duration;

    pj_str_t			mp3_filename;
    pjmedia_mp3_encoder_option  mp3_option;
    unsigned		        mp3_samples_per_frame;
    pj_int16_t		       *mp3_sample_buf;
    unsigned			mp3_sample_pos;
    HBE_STREAM		        mp3_stream;
    unsigned char	       *mp3_buf;
};


static pj_status_t file_put_frame(pjmedia_port *this_port, 
				  const pjmedia_frame *frame);
static pj_status_t file_get_frame(pjmedia_port *this_port, 
				  pjmedia_frame *frame);
static pj_status_t file_on_destroy(pjmedia_port *this_port);


#if defined(PJ_WIN32) || defined(_WIN32) || defined(WIN32)

#include <windows.h>
#define DLL_NAME    PJ_T("LAME_ENC.DLL")

/*
 * Load BladeEncoder DLL.
 */
static pj_status_t init_blade_dll(void)
{
    if (BladeDLL.refCount == 0) {
	#define GET_PROC(type, name)  \
	    BladeDLL.name = (type)GetProcAddress(BladeDLL.hModule, PJ_T(#name)); \
	    if (BladeDLL.name == NULL) { \
		PJ_LOG(1,(THIS_FILE, "Unable to find %s in %s", #name, DLL_NAME)); \
		return PJ_RETURN_OS_ERROR(GetLastError()); \
	    }

	BE_VERSION beVersion;
	BladeDLL.hModule = (void*)LoadLibrary(DLL_NAME);
	if (BladeDLL.hModule == NULL) {
	    pj_status_t status = PJ_RETURN_OS_ERROR(GetLastError());
	    char errmsg[PJ_ERR_MSG_SIZE];

	    pj_strerror(status, errmsg, sizeof(errmsg));
	    PJ_LOG(1,(THIS_FILE, "Unable to load %s: %s", DLL_NAME, errmsg));
	    return status;
	}

	GET_PROC(BEINITSTREAM, beInitStream);
	GET_PROC(BEENCODECHUNK, beEncodeChunk);
	GET_PROC(BEDEINITSTREAM, beDeinitStream);
	GET_PROC(BECLOSESTREAM, beCloseStream);
	GET_PROC(BEVERSION, beVersion);
	GET_PROC(BEWRITEVBRHEADER, beWriteVBRHeader);
	GET_PROC(BEWRITEINFOTAG, beWriteInfoTag);

	#undef GET_PROC

	BladeDLL.beVersion(&beVersion);
	PJ_LOG(4,(THIS_FILE, "%s encoder v%d.%d loaded (%s)", DLL_NAME,
		  beVersion.byMajorVersion, beVersion.byMinorVersion,
		  beVersion.zHomepage));
    }
    ++BladeDLL.refCount;
    return PJ_SUCCESS;
}

/*
 * Decrement the reference counter of the DLL.
 */
static void deinit_blade_dll()
{
    --BladeDLL.refCount;
    if (BladeDLL.refCount == 0 && BladeDLL.hModule) {
	FreeLibrary(BladeDLL.hModule);
	BladeDLL.hModule = NULL;
	PJ_LOG(4,(THIS_FILE, "%s unloaded", DLL_NAME));
    }
}

#else

static pj_status_t init_blade_dll(void)
{
    PJ_LOG(1,(THIS_FILE, "Error: MP3 writer port only works on Windows for now"));
    return PJ_ENOTSUP;
}

static void deinit_blade_dll()
{
}
#endif



/*
 * Initialize MP3 encoder.
 */
static pj_status_t init_mp3_encoder(struct mp3_file_port *fport,
				    pj_pool_t *pool)
{
    BE_CONFIG LConfig;
    unsigned  long InSamples;
    unsigned  long OutBuffSize;
    long MP3Err;

    /*
     * Initialize encoder configuration.
     */
    pj_bzero(&LConfig, sizeof(BE_CONFIG));
    LConfig.dwConfig = BE_CONFIG_LAME;
    LConfig.format.LHV1.dwStructVersion = 1;
    LConfig.format.LHV1.dwStructSize = sizeof(BE_CONFIG);
    LConfig.format.LHV1.dwSampleRate = fport->base.info.clock_rate;
    LConfig.format.LHV1.dwReSampleRate = 0;

    if (fport->base.info.channel_count==1)
	LConfig.format.LHV1.nMode = BE_MP3_MODE_MONO;
    else if (fport->base.info.channel_count==2)
	LConfig.format.LHV1.nMode = BE_MP3_MODE_STEREO;
    else
	return PJMEDIA_ENCCHANNEL;

    LConfig.format.LHV1.dwBitrate = fport->mp3_option.bit_rate / 1000;
    LConfig.format.LHV1.nPreset = LQP_NOPRESET;
    LConfig.format.LHV1.bCopyright = 0;
    LConfig.format.LHV1.bCRC = 1;
    LConfig.format.LHV1.bOriginal = 1;
    LConfig.format.LHV1.bPrivate = 0;

    if (!fport->mp3_option.vbr) {
	LConfig.format.LHV1.nVbrMethod = VBR_METHOD_NONE;
	LConfig.format.LHV1.bWriteVBRHeader = 0;
	LConfig.format.LHV1.bEnableVBR = 0;
    } else {
	LConfig.format.LHV1.nVbrMethod = VBR_METHOD_DEFAULT;
	LConfig.format.LHV1.bWriteVBRHeader = 1;
	LConfig.format.LHV1.dwVbrAbr_bps = fport->mp3_option.bit_rate;
	LConfig.format.LHV1.nVBRQuality =  (pj_uint16_t)
					   fport->mp3_option.quality;
	LConfig.format.LHV1.bEnableVBR = 1;
    }

    LConfig.format.LHV1.nQuality = (pj_uint16_t) 
				   (((0-fport->mp3_option.quality-1)<<8) | 
				    fport->mp3_option.quality);

    /*
     * Init MP3 stream.
     */
    InSamples = 0;
    MP3Err = BladeDLL.beInitStream(&LConfig, &InSamples, &OutBuffSize,
				   &fport->mp3_stream);
    if (MP3Err != BE_ERR_SUCCESSFUL) 
	return PJMEDIA_ERROR;

    /*
     * Allocate sample buffer.
     */
    fport->mp3_samples_per_frame = (unsigned)InSamples;
    fport->mp3_sample_buf = pj_pool_alloc(pool, fport->mp3_samples_per_frame * 2);
    if (!fport->mp3_sample_buf)
	return PJ_ENOMEM;

    /*
     * Allocate encoded MP3 buffer.
     */
    fport->mp3_buf = pj_pool_alloc(pool, (pj_size_t)OutBuffSize);
    if (fport->mp3_buf == NULL)
	return PJ_ENOMEM;

    
    return PJ_SUCCESS;
}


/*
 * Create MP3 file writer port.
 */
PJ_DEF(pj_status_t) 
pjmedia_mp3_writer_port_create( pj_pool_t *pool,
				const char *filename,
				unsigned sampling_rate,
				unsigned channel_count,
				unsigned samples_per_frame,
				unsigned bits_per_sample,
				const pjmedia_mp3_encoder_option *param_option,
				pjmedia_port **p_port )
{
    struct mp3_file_port *fport;
    pj_status_t status;

    status = init_blade_dll();
    if (status != PJ_SUCCESS)
	return status;

    /* Check arguments. */
    PJ_ASSERT_RETURN(pool && filename && p_port, PJ_EINVAL);

    /* Only supports 16bits per sample for now. */
    PJ_ASSERT_RETURN(bits_per_sample == 16, PJ_EINVAL);

    /* Create file port instance. */
    fport = pj_pool_zalloc(pool, sizeof(struct mp3_file_port));
    PJ_ASSERT_RETURN(fport != NULL, PJ_ENOMEM);

    /* Initialize port info. */
    pj_strdup2_with_null(pool, &fport->mp3_filename, filename);
    pjmedia_port_info_init(&fport->base.info, &fport->mp3_filename, SIGNATURE,
			   sampling_rate, channel_count, bits_per_sample,
			   samples_per_frame);

    fport->base.get_frame = &file_get_frame;
    fport->base.put_frame = &file_put_frame;
    fport->base.on_destroy = &file_on_destroy;


    /* Open file in write and read mode.
     * We need the read mode because we'll modify the WAVE header once
     * the recording has completed.
     */
    status = pj_file_open(pool, filename, PJ_O_WRONLY, &fport->fd);
    if (status != PJ_SUCCESS) {
	deinit_blade_dll();
	return status;
    }

    /* Copy and initialize option with default settings */
    if (param_option) {
	pj_memcpy(&fport->mp3_option, param_option, 
		   sizeof(pjmedia_mp3_encoder_option));
    } else {
	pj_bzero(&fport->mp3_option, sizeof(pjmedia_mp3_encoder_option));
	fport->mp3_option.vbr = PJ_TRUE;
    }

    /* Calculate bitrate if it's not specified, only if it's not VBR. */
    if (fport->mp3_option.bit_rate == 0 && !fport->mp3_option.vbr) 
	fport->mp3_option.bit_rate = sampling_rate * channel_count;

    /* Set default quality if it's not specified */
    if (fport->mp3_option.quality == 0) 
	fport->mp3_option.quality = 2;

    /* Init mp3 encoder */
    status = init_mp3_encoder(fport, pool);
    if (status != PJ_SUCCESS) {
	pj_file_close(fport->fd);
	deinit_blade_dll();
	return status;
    }

    /* Done. */
    *p_port = &fport->base;

    PJ_LOG(4,(THIS_FILE, 
	      "MP3 file writer '%.*s' created: samp.rate=%dKHz, "
	      "bitrate=%dkbps%s, quality=%d",
	      (int)fport->base.info.name.slen,
	      fport->base.info.name.ptr,
	      fport->base.info.clock_rate/1000,
	      fport->mp3_option.bit_rate/1000,
	      (fport->mp3_option.vbr ? " (VBR)" : ""),
	      fport->mp3_option.quality));

    return PJ_SUCCESS;
}



/*
 * Register callback.
 */
PJ_DEF(pj_status_t) 
pjmedia_mp3_writer_port_set_cb( pjmedia_port *port,
				pj_size_t pos,
				void *user_data,
			        pj_status_t (*cb)(pjmedia_port *port,
						  void *usr_data))
{
    struct mp3_file_port *fport;

    /* Sanity check */
    PJ_ASSERT_RETURN(port && cb, PJ_EINVAL);

    /* Check that this is really a writer port */
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, PJ_EINVALIDOP);

    fport = (struct mp3_file_port*) port;

    fport->cb_size = pos;
    fport->base.port_data.pdata = user_data;
    fport->cb = cb;

    return PJ_SUCCESS;

}


/*
 * Put a frame into the buffer. When the buffer is full, flush the buffer
 * to the file.
 */
static pj_status_t file_put_frame(pjmedia_port *this_port, 
				  const pjmedia_frame *frame)
{
    struct mp3_file_port *fport = (struct mp3_file_port *)this_port;
    unsigned long MP3Err;
    pj_ssize_t	bytes;
    pj_status_t status;
    unsigned long WriteSize;

    /* Record silence if input is no-frame */
    if (frame->type == PJMEDIA_FRAME_TYPE_NONE || frame->size == 0) {
	unsigned samples_left = fport->base.info.samples_per_frame;
	unsigned samples_copied = 0;

	/* Only want to record at most 1 second of silence */
	if (fport->silence_duration >= fport->base.info.clock_rate)
	    return PJ_SUCCESS;

	while (samples_left) {
	    unsigned samples_needed = fport->mp3_samples_per_frame -
				      fport->mp3_sample_pos;
	    if (samples_needed > samples_left)
		samples_needed = samples_left;

	    pjmedia_zero_samples(fport->mp3_sample_buf + fport->mp3_sample_pos,
				 samples_needed);
	    fport->mp3_sample_pos += samples_needed;
	    samples_left -= samples_needed;
	    samples_copied += samples_needed;

	    /* Encode if we have full frame */
	    if (fport->mp3_sample_pos == fport->mp3_samples_per_frame) {
		
		/* Clear position */
		fport->mp3_sample_pos = 0;

		/* Encode ! */
		MP3Err = BladeDLL.beEncodeChunk(fport->mp3_stream,
						fport->mp3_samples_per_frame,
						fport->mp3_sample_buf, 
						fport->mp3_buf, 
						&WriteSize);
		if (MP3Err != BE_ERR_SUCCESSFUL)
		    return PJMEDIA_ERROR;

		/* Write the chunk */
		bytes = WriteSize;
		status = pj_file_write(fport->fd, fport->mp3_buf, &bytes);
		if (status != PJ_SUCCESS)
		    return status;

		/* Increment total written. */
		fport->total += bytes;
	    }
	}

	fport->silence_duration += fport->base.info.samples_per_frame;

    }
    /* If encoder is expecting different sample size, then we need to
     * buffer the samples.
     */
    else if (fport->mp3_samples_per_frame != 
	     fport->base.info.samples_per_frame) 
    {
	unsigned samples_left = frame->size / 2;
	unsigned samples_copied = 0;
	const pj_int16_t *src_samples = frame->buf;

	fport->silence_duration = 0;

	while (samples_left) {
	    unsigned samples_needed = fport->mp3_samples_per_frame -
				      fport->mp3_sample_pos;
	    if (samples_needed > samples_left)
		samples_needed = samples_left;

	    pjmedia_copy_samples(fport->mp3_sample_buf + fport->mp3_sample_pos,
				 src_samples + samples_copied,
				 samples_needed);
	    fport->mp3_sample_pos += samples_needed;
	    samples_left -= samples_needed;
	    samples_copied += samples_needed;

	    /* Encode if we have full frame */
	    if (fport->mp3_sample_pos == fport->mp3_samples_per_frame) {
		
		/* Clear position */
		fport->mp3_sample_pos = 0;

		/* Encode ! */
		MP3Err = BladeDLL.beEncodeChunk(fport->mp3_stream,
						fport->mp3_samples_per_frame,
						fport->mp3_sample_buf, 
						fport->mp3_buf, 
						&WriteSize);
		if (MP3Err != BE_ERR_SUCCESSFUL)
		    return PJMEDIA_ERROR;

		/* Write the chunk */
		bytes = WriteSize;
		status = pj_file_write(fport->fd, fport->mp3_buf, &bytes);
		if (status != PJ_SUCCESS)
		    return status;

		/* Increment total written. */
		fport->total += bytes;
	    }
	}

    } else {

	fport->silence_duration = 0;

	/* Encode ! */
	MP3Err = BladeDLL.beEncodeChunk(fport->mp3_stream,
					fport->mp3_samples_per_frame,
					frame->buf, 
					fport->mp3_buf, 
					&WriteSize);
	if (MP3Err != BE_ERR_SUCCESSFUL)
	    return PJMEDIA_ERROR;

	/* Write the chunk */
	bytes = WriteSize;
	status = pj_file_write(fport->fd, fport->mp3_buf, &bytes);
	if (status != PJ_SUCCESS)
	    return status;

	/* Increment total written. */
	fport->total += bytes;
    }

    /* Increment total written, and check if we need to call callback */
    
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
    struct mp3_file_port *fport = (struct mp3_file_port*)this_port;
    pj_status_t status;
    unsigned long WriteSize;
    unsigned long MP3Err;


    /* Close encoder */
    MP3Err = BladeDLL.beDeinitStream(fport->mp3_stream, fport->mp3_buf, 
				     &WriteSize);
    if (MP3Err == BE_ERR_SUCCESSFUL) {
	pj_ssize_t bytes = WriteSize;
	status = pj_file_write(fport->fd, fport->mp3_buf, &bytes);
    }

    /* Close file */
    status = pj_file_close(fport->fd);

    /* Write additional VBR header */
    if (fport->mp3_option.vbr) {
	MP3Err = BladeDLL.beWriteVBRHeader(fport->mp3_filename.ptr);
    }


    /* Decrement DLL reference counter */
    deinit_blade_dll();

    /* Done. */
    return PJ_SUCCESS;
}

