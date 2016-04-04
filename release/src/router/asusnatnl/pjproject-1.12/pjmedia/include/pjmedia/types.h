/* $Id: types.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_TYPES_H__
#define __PJMEDIA_TYPES_H__

/**
 * @file pjmedia/types.h Basic Types
 * @brief Basic PJMEDIA types.
 */

#include <pjmedia/config.h>
#include <pj/sock.h>	    /* pjmedia_sock_info	*/
#include <pj/string.h>	    /* pj_memcpy(), pj_memset() */

/**
 * @defgroup PJMEDIA_PORT Media Ports Framework
 * @brief Extensible framework for media terminations
 */


/**
 * @defgroup PJMEDIA_FRAME_OP Audio Manipulation Algorithms
 * @brief Algorithms to manipulate audio frames
 */

/**
 * @defgroup PJMEDIA_TYPES Basic Types
 * @ingroup PJMEDIA_BASE
 * @brief Basic PJMEDIA types and operations.
 * @{
 */

/**
 * Top most media type.
 */
typedef enum pjmedia_type
{
    /** No type. */
    PJMEDIA_TYPE_NONE = 0,

    /** The media is audio */
    PJMEDIA_TYPE_AUDIO = 1,

    /** The media is video. */
    PJMEDIA_TYPE_VIDEO = 2,

    /** Unknown media type, in this case the name will be specified in
     *  encoding_name.
     */
    PJMEDIA_TYPE_UNKNOWN = 3,

    /** The media is application. */
    PJMEDIA_TYPE_APPLICATION = 4

} pjmedia_type;


/**
 * Media transport protocol.
 */
typedef enum pjmedia_tp_proto
{
    /** No transport type */
    PJMEDIA_TP_PROTO_NONE = 0,

    /** RTP using A/V profile */
	PJMEDIA_TP_PROTO_RTP_AVP,

	/** Secure RTP */
	PJMEDIA_TP_PROTO_RTP_SAVP,

	/** WebRTC Data Channel DTLS_SCTP */
	PJMEDIA_TP_PROTO_DTLS_SCTP,

	/** RTP and SCTP */
	PJMEDIA_TP_PROTO_RTP_SCTP,

    /** Unknown */
    PJMEDIA_TP_PROTO_UNKNOWN

} pjmedia_tp_proto;


/**
 * Media direction.
 */
typedef enum pjmedia_dir
{
    /** None */
    PJMEDIA_DIR_NONE = 0,

    /** Encoding (outgoing to network) stream */
    PJMEDIA_DIR_ENCODING = 1,

    /** Decoding (incoming from network) stream. */
    PJMEDIA_DIR_DECODING = 2,

    /** Incoming and outgoing stream. */
    PJMEDIA_DIR_ENCODING_DECODING = 3

} pjmedia_dir;



/* Alternate names for media direction: */

/**
 * Direction is capturing audio frames.
 */
#define PJMEDIA_DIR_CAPTURE	PJMEDIA_DIR_ENCODING

/**
 * Direction is playback of audio frames.
 */
#define PJMEDIA_DIR_PLAYBACK	PJMEDIA_DIR_DECODING

/**
 * Direction is both capture and playback.
 */
#define PJMEDIA_DIR_CAPTURE_PLAYBACK	PJMEDIA_DIR_ENCODING_DECODING


/**
 * Create 32bit port signature from ASCII characters.
 */
#define PJMEDIA_PORT_SIGNATURE(a,b,c,d)	    \
	    (a<<24 | b<<16 | c<<8 | d)


/**
 * Opaque declaration of media endpoint.
 */
typedef struct pjmedia_endpt pjmedia_endpt;


/*
 * Forward declaration for stream (needed by transport).
 */
typedef struct pjmedia_stream pjmedia_stream;


/**
 * Media socket info is used to describe the underlying sockets
 * to be used as media transport.
 */
typedef struct pjmedia_sock_info
{
    /** The RTP socket handle */
	pj_sock_t	    rtp_sock;

    /** Address to be advertised as the local address for the RTP
     *  socket, which does not need to be equal as the bound
     *  address (for example, this address can be the address resolved
     *  with STUN).
     */
    pj_sockaddr	    rtp_addr_name;

    /** The RTCP socket handle. */
	pj_sock_t	    rtcp_sock;

    /** Address to be advertised as the local address for the RTCP
     *  socket, which does not need to be equal as the bound
     *  address (for example, this address can be the address resolved
     *  with STUN).
     */
    pj_sockaddr	    rtcp_addr_name;

    /** The TCP socket handle. */
    pj_sock_t	    tcp_rtp_sock;

    /** Address to be advertised as the local address for the TCP
     *  socket, which does not need to be equal as the bound
     *  address
     */
    pj_sockaddr	    tcp_rtp_addr_name;

} pjmedia_sock_info;


/**
 * Macro for packing format.
 */
#define PJMEDIA_FORMAT_PACK(C1, C2, C3, C4) ( C4<<24 | C3<<16 | C2<<8 | C1 )

/**
 * This enumeration describes format ID. 
 */
typedef enum pjmedia_format_id
{
    /**
     * 16bit linear
     */
    PJMEDIA_FORMAT_L16	    = 0,
    
    /**
     * Alias for PJMEDIA_FORMAT_L16
     */
    PJMEDIA_FORMAT_PCM	    = PJMEDIA_FORMAT_L16,

    /**
     * G.711 ALAW
     */
    PJMEDIA_FORMAT_PCMA	    = PJMEDIA_FORMAT_PACK('A', 'L', 'A', 'W'),

    /**
     * Alias for PJMEDIA_FORMAT_PCMA
     */
    PJMEDIA_FORMAT_ALAW	    = PJMEDIA_FORMAT_PCMA,

    /**
     * G.711 ULAW
     */
    PJMEDIA_FORMAT_PCMU	    = PJMEDIA_FORMAT_PACK('u', 'L', 'A', 'W'),

    /**
     * Aliaw for PJMEDIA_FORMAT_PCMU
     */
    PJMEDIA_FORMAT_ULAW	    = PJMEDIA_FORMAT_PCMU,

    /**
     * AMR narrowband
     */
    PJMEDIA_FORMAT_AMR	    = PJMEDIA_FORMAT_PACK(' ', 'A', 'M', 'R'),

    /**
     * ITU G.729
     */
    PJMEDIA_FORMAT_G729	    = PJMEDIA_FORMAT_PACK('G', '7', '2', '9'),

    /**
     * Internet Low Bit-Rate Codec (ILBC)
     */
    PJMEDIA_FORMAT_ILBC	    = PJMEDIA_FORMAT_PACK('I', 'L', 'B', 'C')

} pjmedia_format_id;


/**
 * Media format information.
 */
typedef struct pjmedia_format
{
    /** Format ID */
    pjmedia_format_id	id;

    /** Bitrate. */
    pj_uint32_t		bitrate;

    /** Flag to indicate whether VAD is enabled */
    pj_bool_t		vad;

} pjmedia_format;



/**
 * This is a general purpose function set PCM samples to zero.
 * Since this function is needed by many parts of the library,
 * by putting this functionality in one place, it enables some.
 * clever people to optimize this function.
 *
 * @param samples	The 16bit PCM samples.
 * @param count		Number of samples.
 */
PJ_INLINE(void) pjmedia_zero_samples(pj_int16_t *samples, unsigned count)
{
#if 1
    pj_bzero(samples, (count<<1));
#elif 0
    unsigned i;
    for (i=0; i<count; ++i) samples[i] = 0;
#else
    unsigned i;
    count >>= 1;
    for (i=0; i<count; ++i) ((pj_int32_t*)samples)[i] = (pj_int32_t)0;
#endif
}


/**
 * This is a general purpose function to copy samples from/to buffers with
 * equal size. Since this function is needed by many parts of the library,
 * by putting this functionality in one place, it enables some.
 * clever people to optimize this function.
 */
PJ_INLINE(void) pjmedia_copy_samples(pj_int16_t *dst, const pj_int16_t *src,
				     unsigned count)
{
#if 1
    pj_memcpy(dst, src, (count<<1));
#elif 0
    unsigned i;
    for (i=0; i<count; ++i) dst[i] = src[i];
#else
    unsigned i;
    count >>= 1;
    for (i=0; i<count; ++i)
	((pj_int32_t*)dst)[i] = ((pj_int32_t*)src)[i];
#endif
}


/**
 * This is a general purpose function to copy samples from/to buffers with
 * equal size. Since this function is needed by many parts of the library,
 * by putting this functionality in one place, it enables some.
 * clever people to optimize this function.
 */
PJ_INLINE(void) pjmedia_move_samples(pj_int16_t *dst, const pj_int16_t *src,
				     unsigned count)
{
#if 1
    pj_memmove(dst, src, (count<<1));
#elif 0
    unsigned i;
    for (i=0; i<count; ++i) dst[i] = src[i];
#else
    unsigned i;
    count >>= 1;
    for (i=0; i<count; ++i)
	((pj_int32_t*)dst)[i] = ((pj_int32_t*)src)[i];
#endif
}

/** 
 * Types of media frame. 
 */
typedef enum pjmedia_frame_type
{
    PJMEDIA_FRAME_TYPE_NONE,	    /**< No frame.		*/
    PJMEDIA_FRAME_TYPE_AUDIO,	    /**< Normal audio frame.	*/
    PJMEDIA_FRAME_TYPE_EXTENDED	    /**< Extended audio frame.	*/

} pjmedia_frame_type;


/** 
 * This structure describes a media frame. 
 */
typedef struct pjmedia_frame
{
    pjmedia_frame_type	 type;	    /**< Frame type.			    */
    void		*buf;	    /**< Pointer to buffer.		    */
    pj_size_t		 size;	    /**< Frame size in bytes.		    */
    pj_timestamp	 timestamp; /**< Frame timestamp.		    */
    pj_uint32_t		 bit_info;  /**< Bit info of the frame, sample case:
					 a frame may not exactly start and end
					 at the octet boundary, so this field 
					 may be used for specifying start & 
					 end bit offset.		    */
} pjmedia_frame;


/**
 * The pjmedia_frame_ext is used to carry a more complex audio frames than
 * the typical PCM audio frames, and it is signaled by setting the "type"
 * field of a pjmedia_frame to PJMEDIA_FRAME_TYPE_EXTENDED. With this set,
 * application may typecast pjmedia_frame to pjmedia_frame_ext.
 *
 * This structure may contain more than one audio frames, which subsequently
 * will be called subframes in this structure. The subframes section
 * immediately follows the end of this structure, and each subframe is
 * represented by pjmedia_frame_ext_subframe structure. Every next
 * subframe immediately follows the previous subframe, and all subframes
 * are byte-aligned although its payload may not be byte-aligned.
 */

#pragma pack(1)
typedef struct pjmedia_frame_ext {
    pjmedia_frame   base;	    /**< Base frame info */
    pj_uint16_t     samples_cnt;    /**< Number of samples in this frame */
    pj_uint16_t     subframe_cnt;   /**< Number of (sub)frames in this frame */

    /* Zero or more (sub)frames follows immediately after this,
     * each will be represented by pjmedia_frame_ext_subframe
     */
} pjmedia_frame_ext;
#pragma pack()

/**
 * This structure represents the individual subframes in the
 * pjmedia_frame_ext structure.
 */
#pragma pack(1)
typedef struct pjmedia_frame_ext_subframe {
    pj_uint16_t     bitlen;	    /**< Number of bits in the data */
    pj_uint8_t      data[1];	    /**< Start of encoded data */
} pjmedia_frame_ext_subframe;

#pragma pack()


/**
 * Append one subframe to #pjmedia_frame_ext.
 *
 * @param frm		    The #pjmedia_frame_ext.
 * @param src		    Subframe data.
 * @param bitlen	    Lenght of subframe, in bits.
 * @param samples_cnt	    Number of audio samples in subframe.
 */
PJ_INLINE(void) pjmedia_frame_ext_append_subframe(pjmedia_frame_ext *frm,
						  const void *src,
					          unsigned bitlen,
						  unsigned samples_cnt)
{
    pjmedia_frame_ext_subframe *fsub;
    pj_uint8_t *p;
    unsigned i;

    p = (pj_uint8_t*)frm + sizeof(pjmedia_frame_ext);
    for (i = 0; i < frm->subframe_cnt; ++i) {
	fsub = (pjmedia_frame_ext_subframe*) p;
	p += sizeof(fsub->bitlen) + ((fsub->bitlen+7) >> 3);
    }

    fsub = (pjmedia_frame_ext_subframe*) p;
    fsub->bitlen = (pj_uint16_t)bitlen;
    if (bitlen)
	pj_memcpy(fsub->data, src, (bitlen+7) >> 3);

    frm->subframe_cnt++;
    frm->samples_cnt = (pj_uint16_t)(frm->samples_cnt + samples_cnt);
}

/**
 * Get a subframe from #pjmedia_frame_ext.
 *
 * @param frm		    The #pjmedia_frame_ext.
 * @param n		    Subframe index, zero based.
 *
 * @return		    The n-th subframe, or NULL if n is out-of-range.
 */
PJ_INLINE(pjmedia_frame_ext_subframe*) 
pjmedia_frame_ext_get_subframe(const pjmedia_frame_ext *frm, unsigned n)
{
    pjmedia_frame_ext_subframe *sf = NULL;

    if (n < frm->subframe_cnt) {
	pj_uint8_t *p;
	unsigned i;

	p = (pj_uint8_t*)frm + sizeof(pjmedia_frame_ext);
	for (i = 0; i < n; ++i) {	
	    sf = (pjmedia_frame_ext_subframe*) p;
	    p += sizeof(sf->bitlen) + ((sf->bitlen+7) >> 3);
	}
        
	sf = (pjmedia_frame_ext_subframe*) p;
    }

    return sf;
}
	
/**
 * Extract all frame payload to the specified buffer. 
 *
 * @param frm		    The frame.
 * @param dst		    Destination buffer.
 * @param maxlen	    Maximum size to copy (i.e. the size of the
 *			    destination buffer).
 *
 * @return		    Total size of payload copied.
 */
PJ_INLINE(unsigned) 
pjmedia_frame_ext_copy_payload(const pjmedia_frame_ext *frm,
			       void *dst, 
			       unsigned maxlen)
{
    unsigned i, copied=0;
    for (i=0; i<frm->subframe_cnt; ++i) {
	pjmedia_frame_ext_subframe *sf;
	unsigned sz;

	sf = pjmedia_frame_ext_get_subframe(frm, i);
	if (!sf)
	    continue;

	sz = ((sf->bitlen + 7) >> 3);
	if (sz + copied > maxlen)
	    break;

	pj_memcpy(((pj_uint8_t*)dst) + copied, sf->data, sz);
	copied += sz;
    }
    return copied;
}


/**
 * Pop out first n subframes from #pjmedia_frame_ext.
 *
 * @param frm		    The #pjmedia_frame_ext.
 * @param n		    Number of first subframes to be popped out.
 *
 * @return		    PJ_SUCCESS when successful.
 */
PJ_INLINE(pj_status_t) 
pjmedia_frame_ext_pop_subframes(pjmedia_frame_ext *frm, unsigned n)
{
    pjmedia_frame_ext_subframe *sf;
    pj_uint8_t *move_src;
    unsigned move_len;

    if (frm->subframe_cnt <= n) {
	frm->subframe_cnt = 0;
	frm->samples_cnt = 0;
	return PJ_SUCCESS;
    }

    move_src = (pj_uint8_t*)pjmedia_frame_ext_get_subframe(frm, n);
    sf = pjmedia_frame_ext_get_subframe(frm, frm->subframe_cnt-1);
    move_len = (pj_uint8_t*)sf - move_src + sizeof(sf->bitlen) + 
	       ((sf->bitlen+7) >> 3);
    pj_memmove((pj_uint8_t*)frm+sizeof(pjmedia_frame_ext), 
	       move_src, move_len);
	    
    frm->samples_cnt = (pj_uint16_t)
		   (frm->samples_cnt - n*frm->samples_cnt/frm->subframe_cnt);
    frm->subframe_cnt = (pj_uint16_t) (frm->subframe_cnt - n);

    return PJ_SUCCESS;
}


/**
 * @}
 */


#endif	/* __PJMEDIA_TYPES_H__ */

