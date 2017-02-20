/* $Id: stream.h 3571 2011-05-19 08:05:23Z ming $ */
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
#ifndef __PJMEDIA_STREAM_H__
#define __PJMEDIA_STREAM_H__


/**
 * @file stream.h
 * @brief Media Stream.
 */

#include <pjmedia/codec.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/jbuf.h>
#include <pjmedia/port.h>
#include <pjmedia/rtcp.h>
#include <pjmedia/transport.h>
#include <pj/sock.h>

PJ_BEGIN_DECL


/**
 * @defgroup PJMED_STRM Streams
 * @ingroup PJMEDIA_PORT
 * @brief Communicating with remote peer via the network
 * @{
 *
 * A media stream is a bidirectional multimedia communication between two
 * endpoints. It corresponds to a media description (m= line) in SDP
 * session descriptor.
 *
 * A media stream consists of two unidirectional channels:
 *  - encoding channel, which transmits unidirectional media to remote, and
 *  - decoding channel, which receives unidirectional media from remote.
 *
 * A media stream exports media port interface (see @ref PJMEDIA_PORT)
 * and application normally uses this interface to interconnect the stream
 * to other PJMEDIA components.
 *
 * A media stream internally manages the following objects:
 *  - an instance of media codec (see @ref PJMEDIA_CODEC),
 *  - an @ref PJMED_JBUF,
 *  - two instances of RTP sessions (#pjmedia_rtp_session, one for each
 *    direction),
 *  - one instance of RTCP session (#pjmedia_rtcp_session),
 *  - and a reference to media transport to send and receive packets
 *    to/from the network (see @ref PJMEDIA_TRANSPORT).
 *
 * Streams are created by calling #pjmedia_stream_create(), specifying
 * #pjmedia_stream_info structure in the parameter. Application can construct
 * the #pjmedia_stream_info structure manually, or use 
 * #pjmedia_stream_info_from_sdp() or #pjmedia_session_info_from_sdp() 
 * functions to construct the #pjmedia_stream_info from local and remote 
 * SDP session descriptors.
 *
 * Application can also use @ref PJMEDIA_SESSION to indirectly create the
 * streams.
 */

/**
 * Opaque declaration for media channel.
 * Media channel is unidirectional flow of media from sender to
 * receiver.
 */
typedef struct pjmedia_channel pjmedia_channel;

/** 
 * This structure describes media stream information. Each media stream
 * corresponds to one "m=" line in SDP session descriptor, and it has
 * its own RTP/RTCP socket pair.
 */
struct pjmedia_stream_info
{
    pjmedia_type	type;	    /**< Media type (audio, video)	    */
    pjmedia_tp_proto	proto;	    /**< Transport protocol (RTP/AVP, etc.) */
    pjmedia_dir		dir;	    /**< Media direction.		    */
    pj_sockaddr		rem_addr;   /**< Remote RTP address		    */
    pj_sockaddr		rem_rtcp;   /**< Optional remote RTCP address. If
					 sin_family is zero, the RTP address
					 will be calculated from RTP.	    */
#if defined(PJMEDIA_HAS_RTCP_XR) && (PJMEDIA_HAS_RTCP_XR != 0)
    pj_bool_t		rtcp_xr_enabled;
				    /**< Specify whether RTCP XR is enabled.*/
    pj_uint32_t		rtcp_xr_interval; /**< RTCP XR interval.            */
    pj_sockaddr		rtcp_xr_dest;/**<Additional remote RTCP XR address.
				         This is useful for third-party (e.g:
					 network monitor) to monitor the 
					 stream. If sin_family is zero, 
					 this will be ignored.		    */
#endif
    pjmedia_codec_info	fmt;	    /**< Incoming codec format info.	    */
    pjmedia_codec_param *param;	    /**< Optional codec param.		    */
    unsigned		tx_pt;	    /**< Outgoing codec paylaod type.	    */
    unsigned		tx_maxptime;/**< Outgoing codec max ptime.	    */
    int		        tx_event_pt;/**< Outgoing pt for telephone-events.  */
    int			rx_event_pt;/**< Incoming pt for telephone-events.  */
    pj_uint32_t		ssrc;	    /**< RTP SSRC.			    */
    pj_uint32_t		rtp_ts;	    /**< Initial RTP timestamp.		    */
    pj_uint16_t		rtp_seq;    /**< Initial RTP sequence number.	    */
    pj_uint8_t		rtp_seq_ts_set;
				    /**< Bitmask flags if initial RTP sequence 
				         and/or timestamp for sender are set.
					 bit 0/LSB : sequence flag 
					 bit 1     : timestamp flag 	    */
    int			jb_init;    /**< Jitter buffer init delay in msec.  
					 (-1 for default).		    */
    int			jb_min_pre; /**< Jitter buffer minimum prefetch
					 delay in msec (-1 for default).    */
    int			jb_max_pre; /**< Jitter buffer maximum prefetch
					 delay in msec (-1 for default).    */
    int			jb_max;	    /**< Jitter buffer max delay in msec.   */

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
    pj_bool_t		use_ka;	    /**< Stream keep-alive and NAT hole punch
					 (see #PJMEDIA_STREAM_ENABLE_KA)
					 is enabled?			    */
#endif
    pj_bool_t           rtcp_sdes_bye_disabled; 
                                    /**< Disable automatic sending of RTCP
                                         SDES and BYE.                      */
};


/**
 * @see pjmedia_stream_info.
 */
typedef struct pjmedia_stream_info pjmedia_stream_info;



/**
 * Create a media stream based on the specified parameter. After the stream
 * has been created, application normally would want to get the media port 
 * interface of the streams, by calling pjmedia_stream_get_port(). The 
 * media port interface exports put_frame() and get_frame() function, used
 * to transmit and receive media frames from the stream.
 *
 * Without application calling put_frame() and get_frame(), there will be 
 * no media frames transmitted or received by the stream.
 *
 * @param endpt		Media endpoint.
 * @param pool		Pool to allocate memory for the stream. A large
 *			number of memory may be needed because jitter
 *			buffer needs to preallocate some storage.
 * @param info		Stream information.
 * @param tp		Stream transport instance used to transmit 
 *			and receive RTP/RTCP packets to/from the underlying 
 *			transport. 
 * @param user_data	Arbitrary user data (for future callback feature).
 * @param p_stream	Pointer to receive the media stream.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stream_create(pjmedia_endpt *endpt,
					   pj_pool_t *pool,
					   const pjmedia_stream_info *info,
					   pjmedia_transport *tp,
					   void *user_data,
					   pjmedia_stream **p_stream);

/**
 * Destroy the media stream.
 *
 * @param stream	The media stream.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stream_destroy(pjmedia_stream *stream);


/**
 * Get the last frame type retreived from the jitter buffer.
 *
 * @param stream	The media stream.
 *
 * @return		Jitter buffer frame type.
 */
PJ_DEF(char) pjmedia_stream_get_last_jb_frame_type(pjmedia_stream *stream);


/**
 * Get the media port interface of the stream. The media port interface
 * declares put_frame() and get_frame() function, which is the only 
 * way for application to transmit and receive media frames from the
 * stream.
 *
 * @param stream	The media stream.
 * @param p_port	Pointer to receive the port interface.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stream_get_port(pjmedia_stream *stream,
					     pjmedia_port **p_port );


/**
 * Get the media transport object associated with this stream.
 *
 * @param st		The media stream.
 *
 * @return		The transport object being used by the stream.
 */
PJ_DECL(pjmedia_transport*) pjmedia_stream_get_transport(pjmedia_stream *st);


/**
 * Start the media stream. This will start the appropriate channels
 * in the media stream, depending on the media direction that was set
 * when the stream was created.
 *
 * @param stream	The media stream.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stream_start(pjmedia_stream *stream);


/**
 * Get the stream statistics. See also
 * #pjmedia_stream_get_stat_jbuf()
 *
 * @param stream	The media stream.
 * @param stat		Media stream statistics.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stream_get_stat( const pjmedia_stream *stream,
					      pjmedia_rtcp_stat *stat);


/**
 * Reset the stream statistics.
 *
 * @param stream	The media stream.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stream_reset_stat(pjmedia_stream *stream);


#if defined(PJMEDIA_HAS_RTCP_XR) && (PJMEDIA_HAS_RTCP_XR != 0)
/**
 * Get the stream extended report statistics (RTCP XR).
 *
 * @param stream	The media stream.
 * @param stat		Media stream extended report statistics.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stream_get_stat_xr( const pjmedia_stream *stream,
					         pjmedia_rtcp_xr_stat *stat);
#endif

/**
 * Get current jitter buffer state. See also
 * #pjmedia_stream_get_stat()
 *
 * @param stream	The media stream.
 * @param state		Jitter buffer state.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stream_get_stat_jbuf(const pjmedia_stream *stream,
						  pjmedia_jb_state *state);


/**
 * Pause the individual channel in the stream.
 *
 * @param stream	The media channel.
 * @param dir		Which direction to pause.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stream_pause( pjmedia_stream *stream,
					   pjmedia_dir dir);

/**
 * Resume the individual channel in the stream.
 *
 * @param stream	The media channel.
 * @param dir		Which direction to resume.
 *
 * @return		PJ_SUCCESS on success;
 */
PJ_DECL(pj_status_t) pjmedia_stream_resume(pjmedia_stream *stream,
					   pjmedia_dir dir);

/**
 * Transmit DTMF to this stream. The DTMF will be transmitted uisng
 * RTP telephone-events as described in RFC 2833. This operation is
 * only valid for audio stream.
 *
 * @param stream	The media stream.
 * @param ascii_digit	String containing digits to be sent to remote.
 *			Currently the maximum number of digits are 32.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stream_dial_dtmf(pjmedia_stream *stream,
					      const pj_str_t *ascii_digit);


/**
 * Check if the stream has incoming DTMF digits in the incoming DTMF
 * queue. Incoming DTMF digits received via RFC 2833 mechanism are
 * saved in the incoming digits queue.
 *
 * @param stream	The media stream.
 *
 * @return		Non-zero (PJ_TRUE) if the stream has received DTMF
 *			digits in the .
 */
PJ_DECL(pj_bool_t) pjmedia_stream_check_dtmf(pjmedia_stream *stream);


/**
 * Retrieve the incoming DTMF digits from the stream, and remove the digits
 * from stream's DTMF buffer. Note that the digits buffer will not be NULL 
 * terminated.
 *
 * @param stream	The media stream.
 * @param ascii_digits	Buffer to receive the digits. The length of this
 *			buffer is indicated in the "size" argument.
 * @param size		On input, contains the maximum digits to be copied
 *			to the buffer.
 *			On output, it contains the actual digits that has
 *			been copied to the buffer.
 *
 * @return		Non-zero (PJ_TRUE) if the stream has received DTMF
 *			digits in the .
 */
PJ_DECL(pj_status_t) pjmedia_stream_get_dtmf( pjmedia_stream *stream,
					      char *ascii_digits,
					      unsigned *size);


/**
 * Set callback to be called upon receiving DTMF digits. If callback is
 * registered, the stream will not buffer incoming DTMF but rather call
 * the callback as soon as DTMF digit is received completely.
 *
 * @param stream	The media stream.
 * @param cb		Callback to be called upon receiving DTMF digits.
 *			The DTMF digits will be given to the callback as
 *			ASCII digits.
 * @param user_data	User data to be returned back when the callback
 *			is called.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_stream_set_dtmf_callback(pjmedia_stream *stream,
				 void (*cb)(pjmedia_stream*, 
					    void *user_data, 
					    int digit), 
				 void *user_data);

/**
 * Send RTCP SDES for the media stream.
 *
 * @param stream	The media stream.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_stream_send_rtcp_sdes( pjmedia_stream *stream );

/**
 * Send RTCP BYE for the media stream.
 *
 * @param stream	The media stream.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_stream_send_rtcp_bye( pjmedia_stream *stream );

#if 0 // 2013-10-20 DEAN, deprecated
PJ_DECL(pj_status_t) pjmedia_stream_set_nsmd_callback(pjmedia_stream *stream, 
													 void (*cb)(int call_id));
#endif

/**
 * Check if the recv buffer is full or not.
 *
 * @param stream	The media stream.
 *
 * @return		PJ_TRUE : The recv buffer is full, can't hold data any more.
 */
PJ_DECL(pj_bool_t) pjmedia_stream_rbuff_full(pjmedia_stream *stream);

/**
 * Check if the speed limit reached.
 *
 * @param stream	The media stream.
 * @param data_len	The current length of data.
 *
 * @return		PJ_TRUE : The speed limit reached.
 */
PJ_DECL(pj_bool_t) pjmedia_stream_speed_limit_reached(pjmedia_stream *stream, int data_len);


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_STREAM_H__ */
