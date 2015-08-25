/* $Id: errno.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia-audiodev/errno.h>
#include <pj/string.h>
#include <pj/unicode.h>
#if PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO
#   include <portaudio.h>
#endif
#if PJMEDIA_AUDIO_DEV_HAS_WMME
#   ifdef _MSC_VER
#	pragma warning(push, 3)
#   endif
#   include <windows.h>
#   include <mmsystem.h>
#   ifdef _MSC_VER
#	pragma warning(pop)
#   endif
#endif

/* PJMEDIA-Audiodev's own error codes/messages 
 * MUST KEEP THIS ARRAY SORTED!!
 * Message must be limited to 64 chars!
 */

#if defined(PJ_HAS_ERROR_STRING) && (PJ_HAS_ERROR_STRING != 0)

static const struct 
{
    int code;
    const char *msg;
} err_str[] = 
{
    PJ_BUILD_ERR( PJMEDIA_EAUD_ERR,	    "Unspecified audio device error" ),
    PJ_BUILD_ERR( PJMEDIA_EAUD_SYSERR,	    "Unknown error from audio driver" ),
    PJ_BUILD_ERR( PJMEDIA_EAUD_INIT,	    "Audio subsystem not initialized" ),
    PJ_BUILD_ERR( PJMEDIA_EAUD_INVDEV,	    "Invalid audio device" ),
    PJ_BUILD_ERR( PJMEDIA_EAUD_NODEV,	    "Found no audio devices" ),
    PJ_BUILD_ERR( PJMEDIA_EAUD_NODEFDEV,    "Unable to find default audio device" ),
    PJ_BUILD_ERR( PJMEDIA_EAUD_NOTREADY,    "Audio device not ready" ),
    PJ_BUILD_ERR( PJMEDIA_EAUD_INVCAP,	    "Invalid or unsupported audio capability" ),
    PJ_BUILD_ERR( PJMEDIA_EAUD_INVOP,	    "Invalid or unsupported audio device operation" ),
    PJ_BUILD_ERR( PJMEDIA_EAUD_BADFORMAT,   "Bad or invalid audio device format" ),
    PJ_BUILD_ERR( PJMEDIA_EAUD_SAMPFORMAT,  "Invalid audio device sample format"),
    PJ_BUILD_ERR( PJMEDIA_EAUD_BADLATENCY,  "Bad audio latency setting")

};

#endif	/* PJ_HAS_ERROR_STRING */



/*
 * pjmedia_audiodev_strerror()
 */
PJ_DEF(pj_str_t) pjmedia_audiodev_strerror(pj_status_t statcode, 
					   char *buf, pj_size_t bufsize )
{
    pj_str_t errstr;

#if defined(PJ_HAS_ERROR_STRING) && (PJ_HAS_ERROR_STRING != 0)


    /* See if the error comes from Core Audio. */
#if PJMEDIA_AUDIO_DEV_HAS_COREAUDIO
    if (statcode >= PJMEDIA_AUDIODEV_COREAUDIO_ERRNO_START &&
	statcode <= PJMEDIA_AUDIODEV_COREAUDIO_ERRNO_END)
    {
	int ca_err = PJMEDIA_AUDIODEV_COREAUDIO_ERRNO_START - statcode;

	PJ_UNUSED_ARG(ca_err);
	// TODO: create more helpful error messages
	errstr.ptr = buf;
	pj_strcpy2(&errstr, "Core audio error");
	return errstr;
    } else
#endif

    /* See if the error comes from PortAudio. */
#if PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO
    if (statcode >= PJMEDIA_AUDIODEV_PORTAUDIO_ERRNO_START &&
	statcode <= PJMEDIA_AUDIODEV_PORTAUDIO_ERRNO_END)
    {

	//int pa_err = statcode - PJMEDIA_ERRNO_FROM_PORTAUDIO(0);
	int pa_err = PJMEDIA_AUDIODEV_PORTAUDIO_ERRNO_START - statcode;
	pj_str_t msg;
	
	msg.ptr = (char*)Pa_GetErrorText(pa_err);
	msg.slen = pj_ansi_strlen(msg.ptr);

	errstr.ptr = buf;
	pj_strncpy_with_null(&errstr, &msg, bufsize);
	return errstr;

    } else 
#endif	/* PJMEDIA_SOUND_IMPLEMENTATION */

    /* See if the error comes from WMME */
#if PJMEDIA_AUDIO_DEV_HAS_WMME
    if ((statcode >= PJMEDIA_AUDIODEV_WMME_IN_ERROR_START &&
	 statcode < PJMEDIA_AUDIODEV_WMME_IN_ERROR_END) ||
	(statcode >= PJMEDIA_AUDIODEV_WMME_OUT_ERROR_START &&
	 statcode < PJMEDIA_AUDIODEV_WMME_OUT_ERROR_END))
    {
	MMRESULT native_err, mr;
	MMRESULT (WINAPI *waveGetErrText)(UINT mmrError, LPTSTR pszText, UINT cchText);
	PJ_DECL_UNICODE_TEMP_BUF(wbuf, 80)

	if (statcode >= PJMEDIA_AUDIODEV_WMME_IN_ERROR_START &&
	    statcode <= PJMEDIA_AUDIODEV_WMME_IN_ERROR_END)
	{
	    native_err = statcode - PJMEDIA_AUDIODEV_WMME_IN_ERROR_START;
	    waveGetErrText = &waveInGetErrorText;
	} else {
	    native_err = statcode - PJMEDIA_AUDIODEV_WMME_OUT_ERROR_START;
	    waveGetErrText = &waveOutGetErrorText;
	}

#if PJ_NATIVE_STRING_IS_UNICODE
	mr = (*waveGetErrText)(native_err, wbuf, PJ_ARRAY_SIZE(wbuf));
	if (mr == MMSYSERR_NOERROR) {
	    int len = wcslen(wbuf);
	    pj_unicode_to_ansi(wbuf, len, buf, bufsize);
	}
#else
	mr = (*waveGetErrText)(native_err, buf, bufsize);
#endif

	if (mr==MMSYSERR_NOERROR) {
	    errstr.ptr = buf;
	    errstr.slen = pj_ansi_strlen(buf);
	    return errstr;
	} else {
	    pj_ansi_snprintf(buf, bufsize, "MMSYSTEM native error %d", 
			     native_err);
	    return pj_str(buf);
	}

    } else
#endif

    /* Audiodev error */
    if (statcode >= PJMEDIA_AUDIODEV_ERRNO_START && 
	statcode < PJMEDIA_AUDIODEV_ERRNO_END)
    {
	/* Find the error in the table.
	 * Use binary search!
	 */
	int first = 0;
	int n = PJ_ARRAY_SIZE(err_str);

	while (n > 0) {
	    int half = n/2;
	    int mid = first + half;

	    if (err_str[mid].code < statcode) {
		first = mid+1;
		n -= (half+1);
	    } else if (err_str[mid].code > statcode) {
		n = half;
	    } else {
		first = mid;
		break;
	    }
	}


	if (PJ_ARRAY_SIZE(err_str) && err_str[first].code == statcode) {
	    pj_str_t msg;
	    
	    msg.ptr = (char*)err_str[first].msg;
	    msg.slen = pj_ansi_strlen(err_str[first].msg);

	    errstr.ptr = buf;
	    pj_strncpy_with_null(&errstr, &msg, bufsize);
	    return errstr;

	} 
    } 
#endif	/* PJ_HAS_ERROR_STRING */

    /* Error not found. */
    errstr.ptr = buf;
    errstr.slen = pj_ansi_snprintf(buf, bufsize, 
				   "Unknown pjmedia-audiodev error %d",
				   statcode);

    return errstr;
}

