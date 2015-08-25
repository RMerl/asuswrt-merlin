/* $Id: pcap.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJLIB_UTIL_PCAP_H__
#define __PJLIB_UTIL_PCAP_H__

/**
 * @file pcap.h
 * @brief Simple PCAP file reader
 */

#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_PCAP Simple PCAP file reader
 * @ingroup PJ_FILE_FMT
 * @{
 * This module describes simple utility to read PCAP file. It is not intended
 * to support all PCAP features (that's what libpcap is for!), but it can
 * be useful for example to playback or stream PCAP contents.
 */

/**
 * Enumeration to describe supported data link types.
 */
typedef enum pj_pcap_link_type
{
    /** Ethernet data link */
    PJ_PCAP_LINK_TYPE_ETH   = 1

} pj_pcap_link_type;


/**
 * Enumeration to describe supported protocol types.
 */
typedef enum pj_pcap_proto_type
{
    /** UDP protocol */
    PJ_PCAP_PROTO_TYPE_UDP  = 17

} pj_pcap_proto_type;


/**
 * This describes UDP header, which may optionally be returned in
 * #pj_pcap_read_udp() function. All fields are in network byte order.
 */
typedef struct pj_pcap_udp_hdr
{
    pj_uint16_t	src_port;   /**< Source port.	    */
    pj_uint16_t	dst_port;   /**< Destination port   */
    pj_uint16_t	len;	    /**< Length.	    */
    pj_uint16_t	csum;	    /**< Checksum.	    */
} pj_pcap_udp_hdr;


/**
 * This structure describes the filter to be used when reading packets from
 * a PCAP file. When a filter is configured, only packets matching all the
 * filter specifications will be read from PCAP file.
 */
typedef struct pj_pcap_filter
{
    /**
     * Select data link type, or zero to include any supported data links.
     */
    pj_pcap_link_type	link;

    /**
     * Select protocol, or zero to include all supported protocols.
     */
    pj_pcap_proto_type	proto;

    /**
     * Specify source IP address of the packets, or zero to include packets
     * from any IP addresses. Note that IP address here must be in
     * network byte order.
     */
    pj_uint32_t		ip_src;

    /**
     * Specify destination IP address of the packets, or zero to include packets
     * destined to any IP addresses. Note that IP address here must be in
     * network byte order.
     */
    pj_uint32_t		ip_dst;

    /**
     * Specify source port of the packets, or zero to include packets with
     * any source port number. Note that the port number must be in network
     * byte order.
     */
    pj_uint16_t		src_port;

    /**
     * Specify destination port of the packets, or zero to include packets with
     * any destination port number. Note that the port number must be in network
     * byte order.
     */
    pj_uint16_t		dst_port;

} pj_pcap_filter;


/** Opaque declaration for PCAP file */
typedef struct pj_pcap_file pj_pcap_file;


/**
 * Initialize filter with default values. The default value is to allow
 * any packets.
 *
 * @param filter    Filter to be initialized.
 */
PJ_DECL(void) pj_pcap_filter_default(pj_pcap_filter *filter);

/**
 * Open PCAP file.
 *
 * @param pool	    Pool to allocate memory.
 * @param path	    File/path name.
 * @param p_file    Pointer to receive PCAP file handle.
 *
 * @return	    PJ_SUCCESS if file can be opened successfully.
 */
PJ_DECL(pj_status_t) pj_pcap_open(pj_pool_t *pool,
				  const char *path,
				  pj_pcap_file **p_file);

/**
 * Close PCAP file.
 *
 * @param file	    PCAP file handle.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_pcap_close(pj_pcap_file *file);

/**
 * Configure filter for reading the file. When filter is configured,
 * only packets matching all the filter settings will be returned.
 *
 * @param file	    PCAP file handle.
 * @param filter    The filter.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_pcap_set_filter(pj_pcap_file *file,
				        const pj_pcap_filter *filter);

/**
 * Read UDP payload from the next packet in the PCAP file. Optionally it
 * can return the UDP header, if caller supplies it.
 *
 * @param file		    PCAP file handle.
 * @param udp_hdr	    Optional buffer to receive UDP header.
 * @param udp_payload	    Buffer to receive the UDP payload.
 * @param udp_payload_size  On input, specify the size of the buffer.
 *			    On output, it will be filled with the actual size
 *			    of the payload as read from the packet.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_pcap_read_udp(pj_pcap_file *file,
				      pj_pcap_udp_hdr *udp_hdr,
				      pj_uint8_t *udp_payload,
				      pj_size_t *udp_payload_size);


/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJLIB_UTIL_PCAP_H__ */

