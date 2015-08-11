/* $Id: rtcp_xr.h 3969 2012-03-08 08:34:30Z nanang $ */
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
#ifndef __PJMEDIA_RTCP_XR_H__
#define __PJMEDIA_RTCP_XR_H__

/**
 * @file rtcp_xr.h
 * @brief RTCP XR implementation.
 */

#include <pjmedia/types.h>
#include <pj/math.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJMED_RTCP_XR RTCP Extended Report (XR) - RFC 3611
 * @ingroup PJMEDIA_SESSION
 * @brief RTCP XR extension to RTCP session
 * @{
 *
 * PJMEDIA implements subsets of RTCP XR specification (RFC 3611) to monitor
 * the quality of the real-time media (audio/video) transmission.
 */

/**
 * Enumeration of report types of RTCP XR. Useful for user to enable varying
 * combinations of RTCP XR report blocks.
 */
typedef enum {
    PJMEDIA_RTCP_XR_LOSS_RLE	    = (1 << 0),
    PJMEDIA_RTCP_XR_DUP_RLE	    = (1 << 1),
    PJMEDIA_RTCP_XR_RCPT_TIMES	    = (1 << 2),
    PJMEDIA_RTCP_XR_RR_TIME	    = (1 << 3),
    PJMEDIA_RTCP_XR_DLRR	    = (1 << 4),
    PJMEDIA_RTCP_XR_STATS	    = (1 << 5),
    PJMEDIA_RTCP_XR_VOIP_METRICS    = (1 << 6)
} pjmedia_rtcp_xr_type;

/**
 * Enumeration of info need to be updated manually to RTCP XR. Most info
 * could be updated automatically each time RTP received.
 */
typedef enum {
    PJMEDIA_RTCP_XR_INFO_SIGNAL_LVL = 1,
    PJMEDIA_RTCP_XR_INFO_NOISE_LVL  = 2,
    PJMEDIA_RTCP_XR_INFO_RERL	    = 3,
    PJMEDIA_RTCP_XR_INFO_R_FACTOR   = 4,
    PJMEDIA_RTCP_XR_INFO_MOS_LQ	    = 5,
    PJMEDIA_RTCP_XR_INFO_MOS_CQ	    = 6,
    PJMEDIA_RTCP_XR_INFO_CONF_PLC   = 7,
    PJMEDIA_RTCP_XR_INFO_CONF_JBA   = 8,
    PJMEDIA_RTCP_XR_INFO_CONF_JBR   = 9,
    PJMEDIA_RTCP_XR_INFO_JB_NOM	    = 10,
    PJMEDIA_RTCP_XR_INFO_JB_MAX	    = 11,
    PJMEDIA_RTCP_XR_INFO_JB_ABS_MAX = 12
} pjmedia_rtcp_xr_info;

/**
 * Enumeration of PLC types definitions for RTCP XR report.
 */
typedef enum {
    PJMEDIA_RTCP_XR_PLC_UNK	    = 0,
    PJMEDIA_RTCP_XR_PLC_DIS	    = 1,
    PJMEDIA_RTCP_XR_PLC_ENH	    = 2,
    PJMEDIA_RTCP_XR_PLC_STD	    = 3
} pjmedia_rtcp_xr_plc_type;

/**
 * Enumeration of jitter buffer types definitions for RTCP XR report.
 */
typedef enum {
    PJMEDIA_RTCP_XR_JB_UNKNOWN      = 0,
    PJMEDIA_RTCP_XR_JB_FIXED        = 2,
    PJMEDIA_RTCP_XR_JB_ADAPTIVE     = 3
} pjmedia_rtcp_xr_jb_type;


#pragma pack(1)

/**
 * This type declares RTCP XR Report Header.
 */
typedef struct pjmedia_rtcp_xr_rb_header
{
    pj_uint8_t		 bt;		/**< Block type.		*/
    pj_uint8_t		 specific;	/**< Block specific data.	*/
    pj_uint16_t		 length;	/**< Block length.		*/
} pjmedia_rtcp_xr_rb_header;

/**
 * This type declares RTCP XR Receiver Reference Time Report Block.
 */
typedef struct pjmedia_rtcp_xr_rb_rr_time
{
    pjmedia_rtcp_xr_rb_header header;	/**< Block header.		*/
    pj_uint32_t		 ntp_sec;	/**< NTP time, seconds part.	*/
    pj_uint32_t		 ntp_frac;	/**< NTP time, fractions part.	*/
} pjmedia_rtcp_xr_rb_rr_time;


/**
 * This type declares RTCP XR DLRR Report Sub-block
 */
typedef struct pjmedia_rtcp_xr_rb_dlrr_item
{
    pj_uint32_t		 ssrc;		/**< receiver SSRC		*/
    pj_uint32_t		 lrr;		/**< last receiver report	*/
    pj_uint32_t		 dlrr;		/**< delay since last receiver
					     report			*/
} pjmedia_rtcp_xr_rb_dlrr_item;

/**
 * This type declares RTCP XR DLRR Report Block
 */
typedef struct pjmedia_rtcp_xr_rb_dlrr
{
    pjmedia_rtcp_xr_rb_header header;	/**< Block header.		*/
    pjmedia_rtcp_xr_rb_dlrr_item item;	/**< Block contents, 
					     variable length list	*/
} pjmedia_rtcp_xr_rb_dlrr;

/**
 * This type declares RTCP XR Statistics Summary Report Block
 */
typedef struct pjmedia_rtcp_xr_rb_stats
{
    pjmedia_rtcp_xr_rb_header header;	/**< Block header.		     */
    pj_uint32_t		 ssrc;		/**< Receiver SSRC		     */
    pj_uint16_t		 begin_seq;	/**< Begin RTP sequence reported     */
    pj_uint16_t		 end_seq;	/**< End RTP sequence reported       */
    pj_uint32_t		 lost;		/**< Number of packet lost in this 
					     interval  */
    pj_uint32_t		 dup;		/**< Number of duplicated packet in 
					     this interval */
    pj_uint32_t		 jitter_min;	/**< Minimum jitter in this interval */
    pj_uint32_t		 jitter_max;	/**< Maximum jitter in this interval */
    pj_uint32_t		 jitter_mean;	/**< Average jitter in this interval */
    pj_uint32_t		 jitter_dev;	/**< Jitter deviation in this 
					     interval */
    pj_uint32_t		 toh_min:8;	/**< Minimum ToH in this interval    */
    pj_uint32_t		 toh_max:8;	/**< Maximum ToH in this interval    */
    pj_uint32_t		 toh_mean:8;	/**< Average ToH in this interval    */
    pj_uint32_t		 toh_dev:8;	/**< ToH deviation in this interval  */
} pjmedia_rtcp_xr_rb_stats;

/**
 * This type declares RTCP XR VoIP Metrics Report Block
 */
typedef struct pjmedia_rtcp_xr_rb_voip_mtc
{
    pjmedia_rtcp_xr_rb_header header;	/**< Block header.		*/
    pj_uint32_t		 ssrc;		/**< Receiver SSRC		*/
    pj_uint8_t		 loss_rate;	/**< Packet loss rate		*/
    pj_uint8_t		 discard_rate;	/**< Packet discarded rate	*/
    pj_uint8_t		 burst_den;	/**< Burst density		*/
    pj_uint8_t		 gap_den;	/**< Gap density		*/
    pj_uint16_t		 burst_dur;	/**< Burst duration		*/
    pj_uint16_t		 gap_dur;	/**< Gap duration		*/
    pj_uint16_t		 rnd_trip_delay;/**< Round trip delay		*/
    pj_uint16_t		 end_sys_delay; /**< End system delay		*/
    pj_uint8_t		 signal_lvl;	/**< Signal level		*/
    pj_uint8_t		 noise_lvl;	/**< Noise level		*/
    pj_uint8_t		 rerl;		/**< Residual Echo Return Loss	*/
    pj_uint8_t		 gmin;		/**< The gap threshold		*/
    pj_uint8_t		 r_factor;	/**< Voice quality metric carried
					     over this RTP session	*/
    pj_uint8_t		 ext_r_factor;  /**< Voice quality metric carried 
					     outside of this RTP session*/
    pj_uint8_t		 mos_lq;	/**< Mean Opinion Score for 
					     Listening Quality          */
    pj_uint8_t		 mos_cq;	/**< Mean Opinion Score for 
					     Conversation Quality       */
    pj_uint8_t		 rx_config;	/**< Receiver configuration	*/
    pj_uint8_t		 reserved2;	/**< Not used			*/
    pj_uint16_t		 jb_nom;	/**< Current delay by jitter
					     buffer			*/
    pj_uint16_t		 jb_max;	/**< Maximum delay by jitter
					     buffer			*/
    pj_uint16_t		 jb_abs_max;	/**< Maximum possible delay by
					     jitter buffer		*/
} pjmedia_rtcp_xr_rb_voip_mtc;


/**
 * Constant of RTCP-XR content size.
 */
#define PJMEDIA_RTCP_XR_BUF_SIZE \
    sizeof(pjmedia_rtcp_xr_rb_rr_time) + \
    sizeof(pjmedia_rtcp_xr_rb_dlrr) + \
    sizeof(pjmedia_rtcp_xr_rb_stats) + \
    sizeof(pjmedia_rtcp_xr_rb_voip_mtc)


/**
 * This structure declares RTCP XR (Extended Report) packet.
 */
typedef struct pjmedia_rtcp_xr_pkt
{
    struct {
#if defined(PJ_IS_BIG_ENDIAN) && PJ_IS_BIG_ENDIAN!=0
	unsigned	 version:2;	/**< packet type            */
	unsigned	 p:1;		/**< padding flag           */
	unsigned	 count:5;	/**< varies by payload type */
	unsigned	 pt:8;		/**< payload type           */
#else
	unsigned	 count:5;	/**< varies by payload type */
	unsigned	 p:1;		/**< padding flag           */
	unsigned	 version:2;	/**< packet type            */
	unsigned	 pt:8;		/**< payload type           */
#endif
	unsigned	 length:16;	/**< packet length          */
	pj_uint32_t	 ssrc;		/**< SSRC identification    */
    } common;

    pj_int8_t		 buf[PJMEDIA_RTCP_XR_BUF_SIZE];
					/**< Content buffer   */
} pjmedia_rtcp_xr_pkt;

#pragma pack()


/**
 * This structure describes RTCP XR statitic.
 */
typedef struct pjmedia_rtcp_xr_stream_stat
{
    struct {
	pj_time_val	    update;	/**< Time of last update.	    */

	pj_uint32_t	    begin_seq;	/**< Begin # seq of this interval.  */
	pj_uint32_t	    end_seq;	/**< End # seq of this interval.    */
	unsigned	    count;	/**< Number of packets.		    */

	/**
	 * Flags represent whether the such report is valid/updated
	 */
	unsigned	    l:1;	/**< Lost flag			    */
	unsigned	    d:1;	/**< Duplicated flag		    */
	unsigned	    j:1;	/**< Jitter flag		    */
	unsigned	    t:2;	/**< TTL or Hop Limit, 
					     0=none, 1=TTL, 2=HL	    */

	unsigned	    lost;	/**< Number of packets lost	    */
	unsigned	    dup;	/**< Number of duplicated packets   */
	pj_math_stat	    jitter;	/**< Jitter statistics (in usec)    */
	pj_math_stat	    toh;	/**< TTL of hop limit statistics.   */
    } stat_sum;

    struct {
	pj_time_val	    update;	    /**< Time of last update.	    */

	pj_uint8_t	    loss_rate;	    /**< Packet loss rate	    */
	pj_uint8_t	    discard_rate;   /**< Packet discarded rate	    */
	pj_uint8_t	    burst_den;	    /**< Burst density		    */
	pj_uint8_t	    gap_den;	    /**< Gap density		    */
	pj_uint16_t	    burst_dur;	    /**< Burst duration		    */
	pj_uint16_t	    gap_dur;	    /**< Gap duration		    */
	pj_uint16_t	    rnd_trip_delay; /**< Round trip delay	    */
	pj_uint16_t	    end_sys_delay;  /**< End system delay	    */
	pj_int8_t	    signal_lvl;	    /**< Signal level		    */
	pj_int8_t	    noise_lvl;	    /**< Noise level		    */
	pj_uint8_t	    rerl;	    /**< Residual Echo Return Loss  */
	pj_uint8_t	    gmin;	    /**< The gap threshold	    */
	pj_uint8_t	    r_factor;	    /**< Voice quality metric carried
						 over this RTP session	    */
	pj_uint8_t	    ext_r_factor;   /**< Voice quality metric carried 
						 outside of this RTP session*/
	pj_uint8_t	    mos_lq;	    /**< Mean Opinion Score for 
						 Listening Quality          */
	pj_uint8_t	    mos_cq;	    /**< Mean Opinion Score for 
						 Conversation Quality       */
	pj_uint8_t	    rx_config;	    /**< Receiver configuration	    */
	pj_uint16_t	    jb_nom;	    /**< Current delay by jitter
						 buffer			    */
	pj_uint16_t	    jb_max;	    /**< Maximum delay by jitter
						 buffer			    */
	pj_uint16_t	    jb_abs_max;	    /**< Maximum possible delay by
						 jitter buffer		    */
    } voip_mtc;

} pjmedia_rtcp_xr_stream_stat;

typedef struct pjmedia_rtcp_xr_stat
{
    pjmedia_rtcp_xr_stream_stat	 rx;  /**< Decoding direction statistics.   */
    pjmedia_rtcp_xr_stream_stat	 tx;  /**< Encoding direction statistics.   */
    pj_math_stat		 rtt; /**< Round-trip delay stat (in usec) 
					   the value is calculated from 
					   receiver side.		    */
} pjmedia_rtcp_xr_stat;

/**
 * Forward declaration of RTCP session
 */
struct pjmedia_rtcp_session;

/**
 * RTCP session is used to monitor the RTP session of one endpoint. There
 * should only be one RTCP session for a bidirectional RTP streams.
 */
struct pjmedia_rtcp_xr_session
{
    char		   *name;	/**< Name identification.	    */
    pjmedia_rtcp_xr_pkt	    pkt;	/**< Cached RTCP XR packet.	    */

    pj_uint32_t		    rx_lrr;	/**< NTP ts in last RR received.    */
    pj_timestamp	    rx_lrr_time;/**< Time when last RR is received. */
    pj_uint32_t		    rx_last_rr; /**< # pkt received since last 
				             sending RR time.		    */

    pjmedia_rtcp_xr_stat    stat;	/**< RTCP XR statistics.	    */

    /* The reference sequence number is an extended sequence number
     * that serves as the basis for determining whether a new 16 bit
     * sequence number comes earlier or later in the 32 bit sequence
     * space.
     */
    pj_uint32_t		    src_ref_seq;
    pj_bool_t		    uninitialized_src_ref_seq;

    /* This structure contains variables needed for calculating 
     * burst metrics.
     */
    struct {
	pj_uint32_t	    pkt;
	pj_uint32_t	    lost;
	pj_uint32_t	    loss_count;
	pj_uint32_t	    discard_count;
	pj_uint32_t	    c11;
	pj_uint32_t	    c13;
	pj_uint32_t	    c14;
	pj_uint32_t	    c22;
	pj_uint32_t	    c23;
	pj_uint32_t	    c33;
    } voip_mtc_stat;

    unsigned ptime;			/**< Packet time.		    */
    unsigned frames_per_packet;		/**< # frames per packet.	    */

    struct pjmedia_rtcp_session *rtcp_session;
					/**< Parent/RTCP session.	    */
};

typedef struct pjmedia_rtcp_xr_session pjmedia_rtcp_xr_session;

/**
 * Build an RTCP XR packet which contains one or more RTCP XR report blocks.
 * There are seven report types as defined in RFC 3611.
 *
 * @param session   The RTCP XR session.
 * @param rpt_types Report types to be included in the packet, report types
 *		    are defined in pjmedia_rtcp_xr_type, set this to zero
 *		    will make this function build all reports appropriately.
 * @param rtcp_pkt  Upon return, it will contain pointer to the RTCP XR packet.
 * @param len	    Upon return, it will indicate the size of the generated 
 *		    RTCP XR packet.
 */
PJ_DECL(void) pjmedia_rtcp_build_rtcp_xr( pjmedia_rtcp_xr_session *session,
					  unsigned rpt_types,
					  void **rtcp_pkt, int *len);

/**
 * Call this function to manually update some info needed by RTCP XR to 
 * generate report which could not be populated directly when receiving
 * RTP.
 *
 * @param session   The RTCP XR session.
 * @param info	    Info type to be updated, @see pjmedia_rtcp_xr_info.
 * @param val	    Value.
 */
PJ_DECL(pj_status_t) pjmedia_rtcp_xr_update_info(
					  pjmedia_rtcp_xr_session *session,
					  unsigned info,
					  pj_int32_t val);

/*
 * Private APIs:
 */

/**
 * This function is called internally by RTCP session when RTCP XR is enabled
 * to initialize the RTCP XR session.
 *
 * @param session   RTCP XR session.
 * @param r_session RTCP session.
 * @param gmin      Gmin value (defined in RFC 3611), set to 0 for default (16).
 * @param frames_per_packet
		    Number of frames per packet.
 */
void pjmedia_rtcp_xr_init( pjmedia_rtcp_xr_session *session, 
			   struct pjmedia_rtcp_session *r_session,
			   pj_uint8_t gmin,
			   unsigned frames_per_packet);

/**
 * This function is called internally by RTCP session to destroy 
 * the RTCP XR session.
 *
 * @param session   RTCP XR session.
 */
void pjmedia_rtcp_xr_fini( pjmedia_rtcp_xr_session *session );

/**
 * This function is called internally by RTCP session when it receives 
 * incoming RTCP XR packets.
 *
 * @param session   RTCP XR session.
 * @param rtcp_pkt  The received RTCP XR packet.
 * @param size	    Size of the incoming packet.
 */
void pjmedia_rtcp_xr_rx_rtcp_xr( pjmedia_rtcp_xr_session *session,
				 const void *rtcp_pkt,
				 pj_size_t size);

/**
 * This function is called internally by RTCP session whenever an RTP packet
 * is received or lost to let the RTCP XR session update its statistics.
 * Data passed to this function is a result of analyzation by RTCP and the
 * jitter buffer. Whenever some info is available, the value should be zero
 * or more (no negative info), otherwise if info is not available the info
 * should be -1 so no update will be done for this info in the RTCP XR session.
 *
 * @param session   RTCP XR session.
 * @param seq	    Sequence number of RTP packet.
 * @param lost	    Info if this packet is lost. 
 * @param dup	    Info if this packet is a duplication. 
 * @param discarded Info if this packet is discarded 
 *		    (not because of duplication).
 * @param jitter    Info jitter of this packet.
 * @param toh	    Info Time To Live or Hops Limit of this packet.
 * @param toh_ipv4  Set PJ_TRUE if packet is transported over IPv4.
 */
void pjmedia_rtcp_xr_rx_rtp( pjmedia_rtcp_xr_session *session,
			     unsigned seq, 
			     int lost,
			     int dup,
			     int discarded,
			     int jitter,
			     int toh, pj_bool_t toh_ipv4);

/**
 * This function is called internally by RTCP session whenever an RTP 
 * packet is sent to let the RTCP XR session do its internal calculations.
 *
 * @param session   RTCP XR session.
 * @param ptsize    Size of RTP payload being sent.
 */
void pjmedia_rtcp_xr_tx_rtp( pjmedia_rtcp_xr_session *session, 
			     unsigned ptsize );

/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_RTCP_XR_H__ */
