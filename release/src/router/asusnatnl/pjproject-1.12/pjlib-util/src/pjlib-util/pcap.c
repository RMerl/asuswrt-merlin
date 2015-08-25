/* $Id: pcap.c 3588 2011-06-20 03:54:49Z nanang $ */
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
#include <pjlib-util/pcap.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/file_io.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/sock.h>
#include <pj/string.h>

#if 0
#   define TRACE_(x)	PJ_LOG(5,x)
#else
#   define TRACE_(x)
#endif


#pragma pack(1)

typedef struct pj_pcap_hdr 
{
    pj_uint32_t magic_number;   /* magic number */
    pj_uint16_t version_major;  /* major version number */
    pj_uint16_t version_minor;  /* minor version number */
    pj_int32_t  thiszone;       /* GMT to local correction */
    pj_uint32_t sigfigs;        /* accuracy of timestamps */
    pj_uint32_t snaplen;        /* max length of captured packets, in octets */
    pj_uint32_t network;        /* data link type */
} pj_pcap_hdr;

typedef struct pj_pcap_rec_hdr 
{
    pj_uint32_t ts_sec;         /* timestamp seconds */
    pj_uint32_t ts_usec;        /* timestamp microseconds */
    pj_uint32_t incl_len;       /* number of octets of packet saved in file */
    pj_uint32_t orig_len;       /* actual length of packet */
} pj_pcap_rec_hdr;

#if 0
/* gcc insisted on aligning this struct to 32bit on ARM */
typedef struct pj_pcap_eth_hdr 
{
    pj_uint8_t  dest[6];
    pj_uint8_t  src[6];
    pj_uint8_t  len[2];
} pj_pcap_eth_hdr;
#else
typedef pj_uint8_t pj_pcap_eth_hdr[14];
#endif

typedef struct pj_pcap_ip_hdr 
{
    pj_uint8_t	v_ihl;
    pj_uint8_t	tos;
    pj_uint16_t	len;
    pj_uint16_t	id;
    pj_uint16_t	flags_fragment;
    pj_uint8_t	ttl;
    pj_uint8_t	proto;
    pj_uint16_t	csum;
    pj_uint32_t	ip_src;
    pj_uint32_t	ip_dst;
} pj_pcap_ip_hdr;

/* Implementation of pcap file */
struct pj_pcap_file
{
    char	    obj_name[PJ_MAX_OBJ_NAME];
    pj_oshandle_t   fd;
    pj_bool_t	    swap;
    pj_pcap_hdr	    hdr;
    pj_pcap_filter  filter;
};

/* Init default filter */
PJ_DEF(void) pj_pcap_filter_default(pj_pcap_filter *filter)
{
    pj_bzero(filter, sizeof(*filter));
}

/* Open pcap file */
PJ_DEF(pj_status_t) pj_pcap_open(pj_pool_t *pool,
				 const char *path,
				 pj_pcap_file **p_file)
{
    pj_pcap_file *file;
    pj_ssize_t sz;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && path && p_file, PJ_EINVAL);

    /* More sanity checks */
    TRACE_(("pcap", "sizeof(pj_pcap_eth_hdr)=%d",
	    sizeof(pj_pcap_eth_hdr)));
    PJ_ASSERT_RETURN(sizeof(pj_pcap_eth_hdr)==14, PJ_EBUG);
    TRACE_(("pcap", "sizeof(pj_pcap_ip_hdr)=%d",
	    sizeof(pj_pcap_ip_hdr)));
    PJ_ASSERT_RETURN(sizeof(pj_pcap_ip_hdr)==20, PJ_EBUG);
    TRACE_(("pcap", "sizeof(pj_pcap_udp_hdr)=%d",
	    sizeof(pj_pcap_udp_hdr)));
    PJ_ASSERT_RETURN(sizeof(pj_pcap_udp_hdr)==8, PJ_EBUG);
    
    file = PJ_POOL_ZALLOC_T(pool, pj_pcap_file);

    pj_ansi_strcpy(file->obj_name, "pcap");

    status = pj_file_open(pool, path, PJ_O_RDONLY, &file->fd);
    if (status != PJ_SUCCESS)
	return status;

    /* Read file pcap header */
    sz = sizeof(file->hdr);
    status = pj_file_read(file->fd, &file->hdr, &sz);
    if (status != PJ_SUCCESS) {
	pj_file_close(file->fd);
	return status;
    }

    /* Check magic number */
    if (file->hdr.magic_number == 0xa1b2c3d4) {
	file->swap = PJ_FALSE;
    } else if (file->hdr.magic_number == 0xd4c3b2a1) {
	file->swap = PJ_TRUE;
	file->hdr.network = pj_ntohl(file->hdr.network);
    } else {
	/* Not PCAP file */
	pj_file_close(file->fd);
	return PJ_EINVALIDOP;
    }

    TRACE_((file->obj_name, "PCAP file %s opened", path));
    
    *p_file = file;
    return PJ_SUCCESS;
}

/* Close pcap file */
PJ_DEF(pj_status_t) pj_pcap_close(pj_pcap_file *file)
{
    PJ_ASSERT_RETURN(file, PJ_EINVAL);
    TRACE_((file->obj_name, "PCAP file closed"));
    return pj_file_close(file->fd);
}

/* Setup filter */
PJ_DEF(pj_status_t) pj_pcap_set_filter(pj_pcap_file *file,
				       const pj_pcap_filter *fil)
{
    PJ_ASSERT_RETURN(file && fil, PJ_EINVAL);
    pj_memcpy(&file->filter, fil, sizeof(pj_pcap_filter));
    return PJ_SUCCESS;
}

/* Read file */
static pj_status_t read_file(pj_pcap_file *file,
			     void *buf,
			     pj_ssize_t *sz)
{
    pj_status_t status;
    status = pj_file_read(file->fd, buf, sz);
    if (status != PJ_SUCCESS)
	return status;
    if (*sz == 0)
	return PJ_EEOF;
    return PJ_SUCCESS;
}

static pj_status_t skip(pj_oshandle_t fd, pj_off_t bytes)
{
    pj_status_t status;
    status = pj_file_setpos(fd, bytes, PJ_SEEK_CUR);
    if (status != PJ_SUCCESS)
	return status; 
    return PJ_SUCCESS;
}


#define SKIP_PKT()  \
	if (rec_incl > sz_read) { \
	    status = skip(file->fd, rec_incl-sz_read);\
	    if (status != PJ_SUCCESS) \
		return status; \
	}

/* Read UDP packet */
PJ_DEF(pj_status_t) pj_pcap_read_udp(pj_pcap_file *file,
				     pj_pcap_udp_hdr *udp_hdr,
				     pj_uint8_t *udp_payload,
				     pj_size_t *udp_payload_size)
{
    PJ_ASSERT_RETURN(file && udp_payload && udp_payload_size, PJ_EINVAL);
    PJ_ASSERT_RETURN(*udp_payload_size, PJ_EINVAL);

    /* Check data link type in PCAP file header */
    if ((file->filter.link && 
	    file->hdr.network != (pj_uint32_t)file->filter.link) ||
	file->hdr.network != PJ_PCAP_LINK_TYPE_ETH)
    {
	/* Link header other than Ethernet is not supported for now */
	return PJ_ENOTSUP;
    }

    /* Loop until we have the packet */
    for (;;) {
	union {
	    pj_pcap_rec_hdr rec;
	    pj_pcap_eth_hdr eth;
	    pj_pcap_ip_hdr ip;
	    pj_pcap_udp_hdr udp;
	} tmp;
	unsigned rec_incl;
	pj_ssize_t sz;
	unsigned sz_read = 0;
	pj_status_t status;

	TRACE_((file->obj_name, "Reading packet.."));

	/* Read PCAP packet header */
	sz = sizeof(tmp.rec);
	status = read_file(file, &tmp.rec, &sz);
	if (status != PJ_SUCCESS) {
	    TRACE_((file->obj_name, "read_file() error: %d", status));
	    return status;
	}

	rec_incl = tmp.rec.incl_len;

	/* Swap byte ordering */
	if (file->swap) {
	    tmp.rec.incl_len = pj_ntohl(tmp.rec.incl_len);
	    tmp.rec.orig_len = pj_ntohl(tmp.rec.orig_len);
	    tmp.rec.ts_sec = pj_ntohl(tmp.rec.ts_sec);
	    tmp.rec.ts_usec = pj_ntohl(tmp.rec.ts_usec);
	}

	/* Read link layer header */
	switch (file->hdr.network) {
	case PJ_PCAP_LINK_TYPE_ETH:
	    sz = sizeof(tmp.eth);
	    status = read_file(file, &tmp.eth, &sz);
	    break;
	default:
	    TRACE_((file->obj_name, "Error: link layer not Ethernet"));
	    return PJ_ENOTSUP;
	}

	if (status != PJ_SUCCESS) {
	    TRACE_((file->obj_name, "Error reading Eth header: %d", status));
	    return status;
	}

	sz_read += sz;
	    
	/* Read IP header */
	sz = sizeof(tmp.ip);
	status = read_file(file, &tmp.ip, &sz);
	if (status != PJ_SUCCESS) {
	    TRACE_((file->obj_name, "Error reading IP header: %d", status));
	    return status;
	}

	sz_read += sz;

	/* Skip if IP source mismatch */
	if (file->filter.ip_src && tmp.ip.ip_src != file->filter.ip_src) {
	    TRACE_((file->obj_name, "IP source %s mismatch, skipping", 
		    pj_inet_ntoa(*(pj_in_addr*)&tmp.ip.ip_src)));
	    SKIP_PKT();
	    continue;
	}

	/* Skip if IP destination mismatch */
	if (file->filter.ip_dst && tmp.ip.ip_dst != file->filter.ip_dst) {
	    TRACE_((file->obj_name, "IP detination %s mismatch, skipping", 
		    pj_inet_ntoa(*(pj_in_addr*)&tmp.ip.ip_dst)));
	    SKIP_PKT();
	    continue;
	}

	/* Skip if proto mismatch */
	if (file->filter.proto && tmp.ip.proto != file->filter.proto) {
	    TRACE_((file->obj_name, "IP proto %d mismatch, skipping", 
		    tmp.ip.proto));
	    SKIP_PKT();
	    continue;
	}

	/* Read transport layer header */
	switch (tmp.ip.proto) {
	case PJ_PCAP_PROTO_TYPE_UDP:
	    sz = sizeof(tmp.udp);
	    status = read_file(file, &tmp.udp, &sz);
	    if (status != PJ_SUCCESS) {
		TRACE_((file->obj_name, "Error reading UDP header: %d",status));
		return status;
	    }

	    sz_read += sz;

	    /* Skip if source port mismatch */
	    if (file->filter.src_port && 
	        tmp.udp.src_port != file->filter.src_port) 
	    {
		TRACE_((file->obj_name, "UDP src port %d mismatch, skipping", 
			pj_ntohs(tmp.udp.src_port)));
		SKIP_PKT();
		continue;
	    }

	    /* Skip if destination port mismatch */
	    if (file->filter.dst_port && 
		tmp.udp.dst_port != file->filter.dst_port) 
	    {
		TRACE_((file->obj_name, "UDP dst port %d mismatch, skipping", 
			pj_ntohs(tmp.udp.dst_port)));
		SKIP_PKT();
		continue;
	    }

	    /* Copy UDP header if caller wants it */
	    if (udp_hdr) {
		pj_memcpy(udp_hdr, &tmp.udp, sizeof(*udp_hdr));
	    }

	    /* Calculate payload size */
	    sz = pj_ntohs(tmp.udp.len) - sizeof(tmp.udp);
	    break;
	default:
	    TRACE_((file->obj_name, "Not UDP, skipping"));
	    SKIP_PKT();
	    continue;
	}

	/* Check if payload fits the buffer */
	if (sz > (pj_ssize_t)*udp_payload_size) {
	    TRACE_((file->obj_name, 
		    "Error: packet too large (%d bytes required)", sz));
	    SKIP_PKT();
	    return PJ_ETOOSMALL;
	}

	/* Read the payload */
	status = read_file(file, udp_payload, &sz);
	if (status != PJ_SUCCESS) {
	    TRACE_((file->obj_name, "Error reading payload: %d", status));
	    return status;
	}

	sz_read += sz;

	*udp_payload_size = sz;

	// Some layers may have trailer, e.g: link eth2.
	/* Check that we've read all the packets */
	//PJ_ASSERT_RETURN(sz_read == rec_incl, PJ_EBUG);

	/* Skip trailer */
	while (sz_read < rec_incl) {
	    sz = rec_incl - sz_read;
	    status = read_file(file, &tmp.eth, &sz);
	    if (status != PJ_SUCCESS) {
		TRACE_((file->obj_name, "Error reading trailer: %d", status));
		return status;
	    }
	    sz_read += sz;
	}

	return PJ_SUCCESS;
    }

    /* Does not reach here */
}


