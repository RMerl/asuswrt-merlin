/* $Id: errno.h 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_AUDIODEV_AUDIODEV_ERRNO_H__
#define __PJMEDIA_AUDIODEV_AUDIODEV_ERRNO_H__

/**
 * @file errno.h Error Codes
 * @brief Audiodev specific error codes.
 */

#include <pjmedia-audiodev/config.h>
#include <pj/errno.h>

/**
 * @defgroup error_codes Error Codes
 * @ingroup audio_device_api
 * @brief Audio devive library specific error codes.
 * @{
 */


PJ_BEGIN_DECL


/**
 * Start of error code relative to PJ_ERRNO_START_USER.
 * This value is 420000.
 */
#define PJMEDIA_AUDIODEV_ERRNO_START \
	    (PJ_ERRNO_START_USER + PJ_ERRNO_SPACE_SIZE*5)
#define PJMEDIA_AUDIODEV_ERRNO_END   \
	    (PJMEDIA_AUDIODEV_ERRNO_START + PJ_ERRNO_SPACE_SIZE - 1)


/**
 * Mapping from PortAudio error codes to pjmedia error space.
 */
#define PJMEDIA_AUDIODEV_PORTAUDIO_ERRNO_START \
	    (PJMEDIA_AUDIODEV_ERRNO_END-10000)
#define PJMEDIA_AUDIODEV_PORTAUDIO_ERRNO_END   \
	    (PJMEDIA_AUDIODEV_PORTAUDIO_ERRNO_START + 10000 -1)
/**
 * Convert PortAudio error code to PJLIB error code.
 * PortAudio error code range: 0 >= err >= -10000
 */
#define PJMEDIA_AUDIODEV_ERRNO_FROM_PORTAUDIO(err) \
	    ((int)PJMEDIA_AUDIODEV_PORTAUDIO_ERRNO_START-err)

/**
 * Mapping from Windows multimedia WaveIn error codes.
 */
#define PJMEDIA_AUDIODEV_WMME_IN_ERROR_START	\
	    (PJMEDIA_AUDIODEV_ERRNO_START + 30000)
#define PJMEDIA_AUDIODEV_WMME_IN_ERROR_END	\
	    (PJMEDIA_AUDIODEV_WMME_IN_ERROR_START + 1000 - 1)
/**
 * Convert WaveIn operation error codes to PJLIB error space.
 */
#define PJMEDIA_AUDIODEV_ERRNO_FROM_WMME_IN(err) \
	    ((int)PJMEDIA_AUDIODEV_WMME_IN_ERROR_START+err)


/**
 * Mapping from Windows multimedia WaveOut error codes.
 */
#define PJMEDIA_AUDIODEV_WMME_OUT_ERROR_START	\
	    (PJMEDIA_AUDIODEV_WMME_IN_ERROR_END + 1000)
#define PJMEDIA_AUDIODEV_WMME_OUT_ERROR_END	\
	    (PJMEDIA_AUDIODEV_WMME_OUT_ERROR_START + 1000)
/**
 * Convert WaveOut operation error codes to PJLIB error space.
 */
#define PJMEDIA_AUDIODEV_ERRNO_FROM_WMME_OUT(err) \
	    ((int)PJMEDIA_AUDIODEV_WMME_OUT_ERROR_START+err)


/**
 * Mapping from CoreAudio error codes to pjmedia error space.
 */
#define PJMEDIA_AUDIODEV_COREAUDIO_ERRNO_START \
	    (PJMEDIA_AUDIODEV_ERRNO_START+20000)
#define PJMEDIA_AUDIODEV_COREAUDIO_ERRNO_END   \
	    (PJMEDIA_AUDIODEV_COREAUDIO_ERRNO_START + 20000 -1)
/**
 * Convert CoreAudio error code to PJLIB error code.
 * CoreAudio error code range: 0 >= err >= -10000
 */
#define PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(err) \
	    ((int)PJMEDIA_AUDIODEV_COREAUDIO_ERRNO_START-err)

/************************************************************
 * Audio Device API error codes
 ***********************************************************/
/**
 * @hideinitializer
 * General/unknown error.
 */
#define PJMEDIA_EAUD_ERR	(PJMEDIA_AUDIODEV_ERRNO_START+1) /* 420001 */

/**
 * @hideinitializer
 * Unknown error from audio driver
 */
#define PJMEDIA_EAUD_SYSERR	(PJMEDIA_AUDIODEV_ERRNO_START+2) /* 420002 */

/**
 * @hideinitializer
 * Audio subsystem not initialized
 */
#define PJMEDIA_EAUD_INIT	(PJMEDIA_AUDIODEV_ERRNO_START+3) /* 420003 */

/**
 * @hideinitializer
 * Invalid audio device
 */
#define PJMEDIA_EAUD_INVDEV	(PJMEDIA_AUDIODEV_ERRNO_START+4) /* 420004 */

/**
 * @hideinitializer
 * Found no devices
 */
#define PJMEDIA_EAUD_NODEV	(PJMEDIA_AUDIODEV_ERRNO_START+5) /* 420005 */

/**
 * @hideinitializer
 * Unable to find default device
 */
#define PJMEDIA_EAUD_NODEFDEV	(PJMEDIA_AUDIODEV_ERRNO_START+6) /* 420006 */

/**
 * @hideinitializer
 * Device not ready
 */
#define PJMEDIA_EAUD_NOTREADY	(PJMEDIA_AUDIODEV_ERRNO_START+7) /* 420007 */

/**
 * @hideinitializer
 * The audio capability is invalid or not supported
 */
#define PJMEDIA_EAUD_INVCAP	(PJMEDIA_AUDIODEV_ERRNO_START+8) /* 420008 */

/**
 * @hideinitializer
 * The operation is invalid or not supported
 */
#define PJMEDIA_EAUD_INVOP	(PJMEDIA_AUDIODEV_ERRNO_START+9) /* 420009 */

/**
 * @hideinitializer
 * Bad or invalid audio device format
 */
#define PJMEDIA_EAUD_BADFORMAT	(PJMEDIA_AUDIODEV_ERRNO_START+10) /* 4200010 */

/**
 * @hideinitializer
 * Invalid audio device sample format
 */
#define PJMEDIA_EAUD_SAMPFORMAT	(PJMEDIA_AUDIODEV_ERRNO_START+11) /* 4200011 */

/**
 * @hideinitializer
 * Bad latency setting
 */
#define PJMEDIA_EAUD_BADLATENCY	(PJMEDIA_AUDIODEV_ERRNO_START+12) /* 4200012 */





/**
 * Get error message for the specified error code. Note that this
 * function is only able to decode PJMEDIA Audiodev specific error code.
 * Application should use pj_strerror(), which should be able to
 * decode all error codes belonging to all subsystems (e.g. pjlib,
 * pjmedia, pjsip, etc).
 *
 * @param status    The error code.
 * @param buffer    The buffer where to put the error message.
 * @param bufsize   Size of the buffer.
 *
 * @return	    The error message as NULL terminated string,
 *                  wrapped with pj_str_t.
 */
PJ_DECL(pj_str_t) pjmedia_audiodev_strerror(pj_status_t status, char *buffer,
					    pj_size_t bufsize);


PJ_END_DECL

/**
 * @}
 */


#endif	/* __PJMEDIA_AUDIODEV_AUDIODEV_ERRNO_H__ */

