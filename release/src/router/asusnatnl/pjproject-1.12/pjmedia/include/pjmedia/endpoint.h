/* $Id: endpoint.h 3988 2012-03-28 07:32:42Z nanang $ */
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
#ifndef __PJMEDIA_MEDIAMGR_H__
#define __PJMEDIA_MEDIAMGR_H__


/**
 * @file endpoint.h
 * @brief Media endpoint.
 */
/**
 * @defgroup PJMED_ENDPT The Endpoint
 * @{
 *
 * The media endpoint acts as placeholder for endpoint capabilities. Each 
 * media endpoint will have a codec manager to manage list of codecs installed
 * in the endpoint and a sound device factory.
 *
 * A reference to media endpoint instance is required when application wants
 * to create a media session (#pjmedia_session_create()).
 */

#include <pjmedia/codec.h>
#include <pjmedia/sdp.h>


PJ_BEGIN_DECL

/**
 * This enumeration describes various flags that can be set or retrieved in
 * the media endpoint, by using pjmedia_endpt_set_flag() and
 * pjmedia_endpt_get_flag() respectively.
 */
typedef enum pjmedia_endpt_flag
{
    /**
     * This flag controls whether telephony-event should be offered in SDP.
     * Value is boolean.
     */
    PJMEDIA_ENDPT_HAS_TELEPHONE_EVENT_FLAG

} pjmedia_endpt_flag;


/**
 * Type of callback to register to pjmedia_endpt_atexit().
 */
typedef void (*pjmedia_endpt_exit_callback)(pjmedia_endpt *endpt);


/**
 * Create an instance of media endpoint.
 *
 * @param pf		Pool factory, which will be used by the media endpoint
 *			throughout its lifetime.
 * @param ioqueue	Optional ioqueue instance to be registered to the 
 *			endpoint. The ioqueue instance is used to poll all RTP
 *			and RTCP sockets. If this argument is NULL, the 
 *			endpoint will create an internal ioqueue instance.
 * @param worker_cnt	Specify the number of worker threads to be created
 *			to poll the ioqueue.
 * @param disable_sdp_compress	disable sdp compress or not.
 * @param inst_id	The instance id of pjsua.
 * @param p_endpt	Pointer to receive the endpoint instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_endpt_create( pj_pool_factory *pf,
					   pj_ioqueue_t *ioqueue,
					   unsigned worker_cnt,
					   int disable_sdp_compress,
					   int inst_id,
					   pjmedia_endpt **p_endpt);

/**
 * Destroy media endpoint instance.
 *
 * @param endpt		Media endpoint instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_endpt_destroy(pjmedia_endpt *endpt);

/**
 * Change the value of a flag.
 *
 * @param endpt		Media endpoint.
 * @param flag		The flag.
 * @param value		Pointer to the value to be set.
 *
 * @reurn		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_endpt_set_flag(pjmedia_endpt *endpt,
					    pjmedia_endpt_flag flag,
					    const void *value);

/**
 *  Retrieve the value of a flag.
 *
 *  @param endpt	Media endpoint.
 *  @param flag		The flag.
 *  @param value	Pointer to store the result.
 *
 *  @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_endpt_get_flag(pjmedia_endpt *endpt,
					    pjmedia_endpt_flag flag,
					    void *value);

/**
 * Get the ioqueue instance of the media endpoint.
 *
 * @param endpt		The media endpoint instance.
 *
 * @return		The ioqueue instance of the media endpoint.
 */
PJ_DECL(pj_ioqueue_t*) pjmedia_endpt_get_ioqueue(pjmedia_endpt *endpt);


/**
 * Get the number of worker threads on the media endpoint
 *
 * @param endpt		The media endpoint instance.
 * @return		The number of worker threads on the media endpoint
 */
PJ_DECL(unsigned) pjmedia_endpt_get_thread_count(pjmedia_endpt *endpt);

/**
 * Get a reference to one of the worker threads of the media endpoint 
 *
 * @param endpt		The media endpoint instance.
 * @param index		The index of the thread: 0<= index < thread_cnt
 *
 * @return		pj_thread_t or NULL
 */
PJ_DECL(pj_thread_t*) pjmedia_endpt_get_thread(pjmedia_endpt *endpt, 
					       unsigned index);

PJ_DECL(int) pjmedia_endpt_get_inst_id(pjmedia_endpt *endpt);


/**
 * Request the media endpoint to create pool.
 *
 * @param endpt		The media endpoint instance.
 * @param name		Name to be assigned to the pool.
 * @param initial	Initial pool size, in bytes.
 * @param increment	Increment size, in bytes.
 *
 * @return		Memory pool.
 */
PJ_DECL(pj_pool_t*) pjmedia_endpt_create_pool( pjmedia_endpt *endpt,
					       const char *name,
					       pj_size_t initial,
					       pj_size_t increment);

/**
 * Get the codec manager instance of the media endpoint.
 *
 * @param endpt		The media endpoint instance.
 *
 * @return		The instance of codec manager belonging to
 *			this media endpoint.
 */
PJ_DECL(pjmedia_codec_mgr*) pjmedia_endpt_get_codec_mgr(pjmedia_endpt *endpt);


/**
 * Create a SDP session description that describes the endpoint
 * capability.
 *
 * @param endpt		The media endpoint.
 * @param pool		Pool to use to create the SDP descriptor.
 * @param stream_cnt	Number of elements in the sock_info array. This
 *			also denotes the maximum number of streams (i.e.
 *			the "m=" lines) that will be created in the SDP.
 * @param sock_info	Array of socket transport information. One 
 *			transport is needed for each media stream, and
 *			each transport consists of an RTP and RTCP socket
 *			pair.
 * @param p_sdp		Pointer to receive SDP session descriptor.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_endpt_create_sdp( pjmedia_endpt *endpt,
					       pj_pool_t *pool,
					       unsigned stream_cnt,
					       const pjmedia_sock_info sock_info[],
					       pjmedia_sdp_session **p_sdp );

PJ_DECL(pj_status_t) pjmedia_endpt_create_application_sdp( pjmedia_endpt *endpt,
														 pj_pool_t *pool,
														 unsigned stream_cnt,
														 const pjmedia_sock_info sock_info[],
														 pjmedia_sdp_session **p_sdp );


/**
 * Dump media endpoint capabilities.
 *
 * @param endpt		The media endpoint.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_endpt_dump(pjmedia_endpt *endpt);


/**
 * Register cleanup function to be called by media endpoint when 
 * #pjmedia_endpt_destroy() is called. Note that application should not
 * use or access any endpoint resource (such as pool, ioqueue) from within
 * the callback as such resource may have been released when the callback
 * function is invoked.
 *
 * @param endpt		The media endpoint.
 * @param func		The function to be registered.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_endpt_atexit(pjmedia_endpt *endpt,
					  pjmedia_endpt_exit_callback func);


PJ_DECL(pjmedia_type) pjmedia_get_meida_type( const pjmedia_sdp_session *sdp, int media_index);

PJ_DECL(pj_pool_t *) pjmedia_get_pool( pjmedia_endpt *endpt);



PJ_END_DECL


/**
 * @}
 */



#endif	/* __PJMEDIA_MEDIAMGR_H__ */
