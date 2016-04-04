/* $Id: session.h 3571 2011-05-19 08:05:23Z ming $ */
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
#ifndef __PJMEDIA_SESSION_H__
#define __PJMEDIA_SESSION_H__


/**
 * @file session.h
 * @brief Media Session.
 */

#include <pjmedia/endpoint.h>
#include <pjmedia/stream.h>
#include <pjmedia/sdp.h>

PJ_BEGIN_DECL 

/**
 * @defgroup PJMEDIA_SESSION Media Sessions
 * @brief Management of media sessions
 * @{
 *
 * A media session represents multimedia communication between two
 * parties. A media session represents the multimedia session that
 * is described by SDP session descriptor. A media session consists 
 * of one or more media streams (pjmedia_stream), where each stream 
 * represents one media line (m= line) in SDP.
 *
 * This module provides functions to create and manage multimedia
 * sessions.
 *
 * Application creates the media session by calling #pjmedia_session_create(),
 * normally after it has completed negotiating both SDP offer and answer.
 * The session creation function creates the media session (including
 * media streams) based on the content of local and remote SDP.
 */


/**
 * Session info, retrieved from a session by calling
 * #pjmedia_session_get_info().
 */
struct pjmedia_session_info
{
    /** Number of streams. */
    unsigned		stream_cnt;

    /** Individual stream info. */
    pjmedia_stream_info	stream_info[PJMEDIA_MAX_SDP_MEDIA];
};


/** 
 * Opaque declaration of media session. 
 */
typedef struct pjmedia_session pjmedia_session;


/**
 * @see pjmedia_session_info.
 */
typedef struct pjmedia_session_info pjmedia_session_info;


/**
 * This function will initialize the session info based on information
 * in both SDP session descriptors. The remaining information will be
 * taken from default codec parameters. If socket info array is specified,
 * the socket will be copied to the session info as well.
 *
 * @param pool		Pool to allocate memory.
 * @param endpt		Pjmedia endpoint.
 * @param max_streams	Maximum number of stream infos to be created.
 * @param si		Session info structure to be initialized.
 * @param local		Local SDP session descriptor.
 * @param remote	Remote SDP session descriptor.
 *
 * @return		PJ_SUCCESS if stream info is successfully initialized.
 */
PJ_DECL(pj_status_t)
pjmedia_session_info_from_sdp( pj_pool_t *pool,
			       pjmedia_endpt *endpt,
			       unsigned max_streams,
			       pjmedia_session_info *si,
			       const pjmedia_sdp_session *local,
			       const pjmedia_sdp_session *remote);


/**
 * This function will initialize the stream info based on information
 * in both SDP session descriptors for the specified stream index. 
 * The remaining information will be taken from default codec parameters. 
 * If socket info array is specified, the socket will be copied to the 
 * session info as well.
 *
 * @param si		Stream info structure to be initialized.
 * @param pool		Pool to allocate memory.
 * @param endpt		PJMEDIA endpoint instance.
 * @param local		Local SDP session descriptor.
 * @param remote	Remote SDP session descriptor.
 * @param stream_idx	Media stream index in the session descriptor.
 *
 * @return		PJ_SUCCESS if stream info is successfully initialized.
 */
PJ_DECL(pj_status_t)
pjmedia_stream_info_from_sdp( pjmedia_stream_info *si,
			      pj_pool_t *pool,
			      pjmedia_endpt *endpt,
			      const pjmedia_sdp_session *local,
			      const pjmedia_sdp_session *remote,
			      unsigned stream_idx);

/**
 * Create media session based on the local and remote SDP. After the session
 * has been created, application normally would want to get the media port 
 * interface of each streams, by calling #pjmedia_session_get_port(). The 
 * media port interface exports put_frame() and get_frame() function, used
 * to transmit and receive media frames from the stream.
 *
 * Without application calling put_frame() and get_frame(), there will be 
 * no media frames transmitted or received by the session.
 * 
 * @param endpt		The PJMEDIA endpoint instance.
 * @param si		Session info containing stream count and array of
 *			stream info. The stream count indicates how many
 *			streams to be created in the session.
 * @param transports	Array of media stream transports, with 
 *			sufficient number of elements (one for each stream).
 * @param user_data	Arbitrary user data to be kept in the session.
 * @param p_session	Pointer to receive the media session.
 *
 * @return		PJ_SUCCESS if media session can be created 
 *			successfully.
 */
PJ_DECL(pj_status_t) 
pjmedia_session_create( pjmedia_endpt *endpt, 
			const pjmedia_session_info *si,
			pjmedia_transport *transports[],
			void *user_data,
			pjmedia_session **p_session );


/**
 * Get media session info of the session.
 *
 * @param session	The session which info is being queried.
 * @param info		Pointer to receive session info.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_session_get_info( pjmedia_session *session,
					       pjmedia_session_info *info );

/**
 * Get user data of the session.
 *
 * @param session	The session being queried.
 *
 * @return		User data of the session.
 */
PJ_DECL(void*) pjmedia_session_get_user_data( pjmedia_session *session);


/**
 * Activate all streams in media session for the specified direction.
 * Application only needs to call this function if it previously paused
 * the session.
 *
 * @param session	The media session.
 * @param dir		The direction to activate.
 *
 * @return		PJ_SUCCESS if success.
 */
PJ_DECL(pj_status_t) pjmedia_session_resume(pjmedia_session *session,
					    pjmedia_dir dir);


/**
 * Suspend receipt and transmission of all streams in media session
 * for the specified direction.
 *
 * @param session	The media session.
 * @param dir		The media direction to suspend.
 *
 * @return		PJ_SUCCESS if success.
 */
PJ_DECL(pj_status_t) pjmedia_session_pause(pjmedia_session *session,
					   pjmedia_dir dir);

/**
 * Suspend receipt and transmission of individual stream in media session
 * for the specified direction.
 *
 * @param session	The media session.
 * @param index		The stream index.
 * @param dir		The media direction to pause.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_session_pause_stream( pjmedia_session *session,
						   unsigned index,
						   pjmedia_dir dir);

/**
 * Activate individual stream in media session for the specified direction.
 *
 * @param session	The media session.
 * @param index		The stream index.
 * @param dir		The media direction to activate.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_session_resume_stream(pjmedia_session *session,
						   unsigned index,
						   pjmedia_dir dir);

/**
 * Send RTCP SDES for the session.
 *
 * @param session	The media session.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_session_send_rtcp_sdes( const pjmedia_session *session );

/**
 * Send RTCP BYE for the session.
 *
 * @param session	The media session.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_session_send_rtcp_bye( const pjmedia_session *session );

/**
 * Enumerate media streams in the session.
 *
 * @param session	The media session.
 * @param count		On input, specifies the number of elements in
 *			the array. On output, the number will be filled
 *			with number of streams in the session.
 * @param strm_info	Array of stream info.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_session_enum_streams( const pjmedia_session *session,
			      unsigned *count, 
			      pjmedia_stream_info strm_info[]);


/**
 * Get the media port interface of the specified stream. The media port
 * interface declares put_frame() and get_frame() function, which is the 
 * only  way for application to transmit and receive media frames from the
 * stream.
 *
 * @param session	The media session.
 * @param index		Stream index.
 * @param p_port	Pointer to receive the media port interface for
 *			the specified stream.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_session_get_port( pjmedia_session *session,
					       unsigned index,
					       pjmedia_port **p_port);

PJ_DECL(pj_status_t) pjmedia_session_get_stream(  pjmedia_session *session,
											   unsigned index,
											   pjmedia_stream **p_stream);


/**
 * Get session statistics. The stream statistic shows various
 * indicators such as packet count, packet lost, jitter, delay, etc.
 * See also #pjmedia_session_get_stream_stat_jbuf()
 *
 * @param session	The media session.
 * @param index		Stream index.
 * @param stat		Stream statistic.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_session_get_stream_stat(pjmedia_session *session,
						     unsigned index,
						     pjmedia_rtcp_stat *stat);


/**
 * Reset session statistics.
 *
 * @param session	The media session.
 * @param index		Stream index.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_session_reset_stream_stat(pjmedia_session *session,
						       unsigned index);


#if defined(PJMEDIA_HAS_RTCP_XR) && (PJMEDIA_HAS_RTCP_XR != 0)
/**
 * Get extended session statistics. The extended statistic shows reports
 * from RTCP XR, such as per interval statistics summary (packet count, 
 * packet lost, jitter, etc), VoIP metrics (delay, quality, etc)
 *
 * @param session	The media session.
 * @param index		Stream index.
 * @param stat_xr	Stream extended statistics.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_session_get_stream_stat_xr(
					     pjmedia_session *session,
					     unsigned index,
					     pjmedia_rtcp_xr_stat *stat_xr);
#endif


/**
 * Get current jitter buffer state for the specified stream.
 * See also #pjmedia_session_get_stream_stat()
 *
 * @param session	The media session.
 * @param index		Stream index.
 * @param state		Jitter buffer state.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_session_get_stream_stat_jbuf(
					    pjmedia_session *session,
					    unsigned index,
					    pjmedia_jb_state *state);

/**
 * Dial DTMF digit to the stream, using RFC 2833 mechanism.
 *
 * @param session	The media session.
 * @param index		The stream index.
 * @param ascii_digits	String of ASCII digits (i.e. 0-9*##A-B).
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_session_dial_dtmf( pjmedia_session *session,
					        unsigned index,
						const pj_str_t *ascii_digits );


/**
 * Check if the specified stream has received DTMF digits.
 *
 * @param session	The media session.
 * @param index		The stream index.
 *
 * @return		Non-zero (PJ_TRUE) if the stream has DTMF digits.
 */
PJ_DECL(pj_status_t) pjmedia_session_check_dtmf( pjmedia_session *session,
					         unsigned index);


/**
 * Retrieve DTMF digits from the specified stream.
 *
 * @param session	The media session.
 * @param index		The stream index.
 * @param ascii_digits	Buffer to receive the digits. The length of this
 *			buffer is indicated in the "size" argument.
 * @param size		On input, contains the maximum digits to be copied
 *			to the buffer.
 *			On output, it contains the actual digits that has
 *			been copied to the buffer.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_session_get_dtmf( pjmedia_session *session,
					       unsigned index,
					       char *ascii_digits,
					       unsigned *size );

/**
 * Set callback to be called upon receiving DTMF digits. If callback is
 * registered, the stream will not buffer incoming DTMF but rather call
 * the callback as soon as DTMF digit is received completely.
 *
 * @param session	The media session.
 * @param index		The stream index.
 * @param cb		Callback to be called upon receiving DTMF digits.
 *			The DTMF digits will be given to the callback as
 *			ASCII digits.
 * @param user_data	User data to be returned back when the callback
 *			is called.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_session_set_dtmf_callback(pjmedia_session *session,
                                  unsigned index,
                                  void (*cb)(pjmedia_stream*, 
                                             void *user_data, 
                                             int digit), 
                                  void *user_data);

/**
 * Destroy media session.
 *
 * @param session	The media session.
 *
 * @return		PJ_SUCCESS if success.
 */
PJ_DECL(pj_status_t) pjmedia_session_destroy(pjmedia_session *session);

PJ_DECL(pj_status_t) pjmedia_session_get_stream_info(pjmedia_session *session,
													 unsigned index,
													 pjmedia_stream_info **p_stream_info);

#if 0 // 2013-10-20 DEAN, deprecated
PJ_DECL(pj_status_t)
pjmedia_session_set_nsmd_callback(pjmedia_session *session,
								  unsigned index,
								  void (*cb)(int call_id));
#endif



/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJMEDIA_SESSION_H__ */
