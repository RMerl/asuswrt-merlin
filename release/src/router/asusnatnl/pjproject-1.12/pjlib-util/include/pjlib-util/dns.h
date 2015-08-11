/* $Id: dns.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJLIB_UTIL_DNS_H__
#define __PJLIB_UTIL_DNS_H__


/**
 * @file dns.h
 * @brief Low level DNS message parsing and packetization.
 */
#include <pjlib-util/types.h>
#include <pj/sock.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_DNS DNS and Asynchronous DNS Resolver
 * @ingroup PJ_PROTOCOLS
 */

/**
 * @defgroup PJ_DNS_PARSING Low-level DNS Message Parsing and Packetization
 * @ingroup PJ_DNS
 * @{
 *
 * This module provides low-level services to parse and packetize DNS queries
 * and responses. The functions support building a DNS query packet and parse
 * the data in the DNS response. This implementation conforms to the 
 * following specifications:
 *  - RFC 1035: DOMAIN NAMES - IMPLEMENTATION AND SPECIFICATION
 *  - RFC 1886: DNS Extensions to support IP version 6
 *
 * To create a DNS query packet, application should call #pj_dns_make_query()
 * function, specifying the desired DNS query type, the name to be resolved,
 * and the buffer where the DNS packet will be built into. 
 *
 * When incoming DNS query or response packet arrives, application can use
 * #pj_dns_parse_packet() to parse the TCP/UDP payload into parsed DNS packet
 * structure.
 *
 * This module does not provide any networking functionalities to send or
 * receive DNS packets. This functionality should be provided by higher layer
 * modules such as @ref PJ_DNS_RESOLVER.
 */

enum
{
    PJ_DNS_CLASS_IN	= 1	/**< DNS class IN.			    */
};

/**
 * This enumeration describes standard DNS record types as described by
 * RFC 1035, RFC 2782, and others.
 */
typedef enum pj_dns_type
{
    PJ_DNS_TYPE_A	= 1,    /**< Host address (A) record.		    */
    PJ_DNS_TYPE_NS	= 2,    /**< Authoritative name server (NS)	    */
    PJ_DNS_TYPE_MD	= 3,    /**< Mail destination (MD) record.	    */
    PJ_DNS_TYPE_MF	= 4,    /**< Mail forwarder (MF) record.	    */
    PJ_DNS_TYPE_CNAME	= 5,	/**< Canonical name (CNAME) record.	    */
    PJ_DNS_TYPE_SOA	= 6,    /**< Marks start of zone authority.	    */
    PJ_DNS_TYPE_MB	= 7,    /**< Mailbox domain name (MB).		    */
    PJ_DNS_TYPE_MG	= 8,    /**< Mail group member (MG).		    */
    PJ_DNS_TYPE_MR	= 9,    /**< Mail rename domain name.		    */
    PJ_DNS_TYPE_NULL	= 10,	/**< NULL RR.				    */
    PJ_DNS_TYPE_WKS	= 11,	/**< Well known service description	    */
    PJ_DNS_TYPE_PTR	= 12,	/**< Domain name pointer.		    */
    PJ_DNS_TYPE_HINFO	= 13,	/**< Host information.			    */
    PJ_DNS_TYPE_MINFO	= 14,	/**< Mailbox or mail list information.	    */
    PJ_DNS_TYPE_MX	= 15,	/**< Mail exchange record.		    */
    PJ_DNS_TYPE_TXT	= 16,	/**< Text string.			    */
    PJ_DNS_TYPE_RP	= 17,	/**< Responsible person.		    */
    PJ_DNS_TYPE_AFSB	= 18,	/**< AFS cell database.			    */
    PJ_DNS_TYPE_X25	= 19,	/**< X.25 calling address.		    */
    PJ_DNS_TYPE_ISDN	= 20,	/**< ISDN calling address.		    */
    PJ_DNS_TYPE_RT	= 21,	/**< Router.				    */
    PJ_DNS_TYPE_NSAP	= 22,	/**< NSAP address.			    */
    PJ_DNS_TYPE_NSAP_PTR= 23,	/**< NSAP reverse address.		    */
    PJ_DNS_TYPE_SIG	= 24,	/**< Signature.				    */
    PJ_DNS_TYPE_KEY	= 25,	/**< Key.				    */
    PJ_DNS_TYPE_PX	= 26,	/**< X.400 mail mapping.		    */
    PJ_DNS_TYPE_GPOS	= 27,	/**< Geographical position (withdrawn)	    */
    PJ_DNS_TYPE_AAAA	= 28,	/**< IPv6 address.			    */
    PJ_DNS_TYPE_LOC	= 29,	/**< Location.				    */
    PJ_DNS_TYPE_NXT	= 30,	/**< Next valid name in the zone.	    */
    PJ_DNS_TYPE_EID	= 31,	/**< Endpoint idenfitier.		    */
    PJ_DNS_TYPE_NIMLOC	= 32,	/**< Nimrod locator.			    */
    PJ_DNS_TYPE_SRV	= 33,	/**< Server selection (SRV) record.	    */
    PJ_DNS_TYPE_ATMA	= 34,	/**< DNS ATM address record.		    */
    PJ_DNS_TYPE_NAPTR	= 35,	/**< DNS Naming authority pointer record.   */
    PJ_DNS_TYPE_KX	= 36,	/**< DNS key exchange record.		    */
    PJ_DNS_TYPE_CERT	= 37,	/**< DNS certificate record.		    */
    PJ_DNS_TYPE_A6	= 38,	/**< DNS IPv6 address (experimental)	    */
    PJ_DNS_TYPE_DNAME	= 39,	/**< DNS non-terminal name redirection rec. */

    PJ_DNS_TYPE_OPT	= 41,	/**< DNS options - contains EDNS metadata.  */
    PJ_DNS_TYPE_APL	= 42,	/**< DNS Address Prefix List (APL) record.  */
    PJ_DNS_TYPE_DS	= 43,	/**< DNS Delegation Signer (DS)		    */
    PJ_DNS_TYPE_SSHFP	= 44,	/**< DNS SSH Key Fingerprint		    */
    PJ_DNS_TYPE_IPSECKEY= 45,	/**< DNS IPSEC Key.			    */
    PJ_DNS_TYPE_RRSIG	= 46,	/**< DNS Resource Record signature.	    */
    PJ_DNS_TYPE_NSEC	= 47,	/**< DNS Next Secure Name.		    */
    PJ_DNS_TYPE_DNSKEY	= 48	/**< DNSSEC Key.			    */
} pj_dns_type;



/**
 * Standard DNS header, according to RFC 1035, which will be present in
 * both DNS query and DNS response. 
 *
 * Note that all values seen by application would be in
 * host by order. The library would convert them to network
 * byte order as necessary.
 */
typedef struct pj_dns_hdr
{
    pj_uint16_t  id;	    /**< Transaction ID.	    */
    pj_uint16_t  flags;	    /**< Flags.			    */
    pj_uint16_t  qdcount;   /**< Nb. of queries.	    */
    pj_uint16_t  anscount;  /**< Nb. of res records	    */
    pj_uint16_t  nscount;   /**< Nb. of NS records.	    */
    pj_uint16_t  arcount;   /**< Nb. of additional records  */
} pj_dns_hdr;

/** Create RCODE flag */
#define PJ_DNS_SET_RCODE(c)	((pj_uint16_t)((c) & 0x0F))

/** Create RA (Recursion Available) bit */
#define PJ_DNS_SET_RA(on)	((pj_uint16_t)((on) << 7))

/** Create RD (Recursion Desired) bit */
#define PJ_DNS_SET_RD(on)	((pj_uint16_t)((on) << 8))

/** Create TC (Truncated) bit */
#define PJ_DNS_SET_TC(on)	((pj_uint16_t)((on) << 9))

/** Create AA (Authoritative Answer) bit */
#define PJ_DNS_SET_AA(on)	((pj_uint16_t)((on) << 10))

/** Create four bits opcode */
#define PJ_DNS_SET_OPCODE(o)	((pj_uint16_t)((o)  << 11))

/** Create query/response bit */
#define PJ_DNS_SET_QR(on)	((pj_uint16_t)((on) << 15))


/** Get RCODE value */
#define PJ_DNS_GET_RCODE(val)	(((val) & PJ_DNS_SET_RCODE(0x0F)) >> 0)

/** Get RA bit */
#define PJ_DNS_GET_RA(val)	(((val) & PJ_DNS_SET_RA(1)) >> 7)

/** Get RD bit */
#define PJ_DNS_GET_RD(val)	(((val) & PJ_DNS_SET_RD(1)) >> 8)

/** Get TC bit */
#define PJ_DNS_GET_TC(val)	(((val) & PJ_DNS_SET_TC(1)) >> 9)

/** Get AA bit */
#define PJ_DNS_GET_AA(val)	(((val) & PJ_DNS_SET_AA(1)) >> 10)

/** Get OPCODE value */
#define PJ_DNS_GET_OPCODE(val)	(((val) & PJ_DNS_SET_OPCODE(0x0F)) >> 11)

/** Get QR bit */
#define PJ_DNS_GET_QR(val)	(((val) & PJ_DNS_SET_QR(1)) >> 15)


/** 
 * These constants describe DNS RCODEs. Application can fold these constants
 * into PJLIB pj_status_t namespace by calling #PJ_STATUS_FROM_DNS_RCODE()
 * macro.
 */
typedef enum pj_dns_rcode
{
    PJ_DNS_RCODE_FORMERR    = 1,    /**< Format error.			    */
    PJ_DNS_RCODE_SERVFAIL   = 2,    /**< Server failure.		    */
    PJ_DNS_RCODE_NXDOMAIN   = 3,    /**< Name Error.			    */
    PJ_DNS_RCODE_NOTIMPL    = 4,    /**< Not Implemented.		    */
    PJ_DNS_RCODE_REFUSED    = 5,    /**< Refused.			    */
    PJ_DNS_RCODE_YXDOMAIN   = 6,    /**< The name exists.		    */
    PJ_DNS_RCODE_YXRRSET    = 7,    /**< The RRset (name, type) exists.	    */
    PJ_DNS_RCODE_NXRRSET    = 8,    /**< The RRset (name, type) doesn't exist*/
    PJ_DNS_RCODE_NOTAUTH    = 9,    /**< Not authorized.		    */
    PJ_DNS_RCODE_NOTZONE    = 10    /**< The zone specified is not a zone.  */

} pj_dns_rcode;


/**
 * This structure describes a DNS query record.
 */
typedef struct pj_dns_parsed_query
{
    pj_str_t	name;	    /**< The domain in the query.		    */
    pj_uint16_t	type;	    /**< Type of the query (pj_dns_type)	    */
    pj_uint16_t	dnsclass;   /**< Network class (PJ_DNS_CLASS_IN=1)	    */
} pj_dns_parsed_query;


/**
 * This structure describes a Resource Record parsed from the DNS packet.
 * All integral values are in host byte order.
 */
typedef struct pj_dns_parsed_rr
{
    pj_str_t	 name;	    /**< The domain name which this rec pertains.   */
    pj_uint16_t	 type;	    /**< RR type code.				    */
    pj_uint16_t	 dnsclass;  /**< Class of data (PJ_DNS_CLASS_IN=1).	    */
    pj_uint32_t	 ttl;	    /**< Time to live.				    */
    pj_uint16_t	 rdlength;  /**< Resource data length.			    */
    void	*data;	    /**< Pointer to the raw resource data, only
				 when the type is not known. If it is known,
				 the data will be put in rdata below.	    */
    
    /** For resource types that are recognized/supported by this library,
     *  the parsed resource data will be placed in this rdata union.
     */
    union rdata
    {
	/** SRV Resource Data (PJ_DNS_TYPE_SRV, 33) */
	struct srv {
	    pj_uint16_t	prio;	/**< Target priority (lower is higher).	    */
	    pj_uint16_t weight;	/**< Weight/proportion			    */
	    pj_uint16_t port;	/**< Port number of the service		    */
	    pj_str_t	target;	/**< Target name.			    */
	} srv;

	/** CNAME Resource Data (PJ_DNS_TYPE_CNAME, 5) */
	struct cname {
	    pj_str_t	name;	/**< Primary canonical name for an alias.   */
	} cname;

	/** NS Resource Data (PJ_DNS_TYPE_NS, 2) */
	struct ns {
	    pj_str_t	name;	/**< Primary name server.		    */
	} ns;

	/** PTR Resource Data (PJ_DNS_TYPE_PTR, 12) */
	struct ptr {
	    pj_str_t	name;	/**< PTR name.				    */
	} ptr;

	/** A Resource Data (PJ_DNS_TYPE_A, 1) */
	struct a {
	    pj_in_addr	ip_addr;/**< IPv4 address in network byte order.    */
	} a;

	/** AAAA Resource Data (PJ_DNS_TYPE_AAAA, 28) */
	struct aaaa {
	    pj_in6_addr	ip_addr;/**< IPv6 address in network byte order.    */
	} aaaa;

    } rdata;

} pj_dns_parsed_rr;


/**
 * This structure describes the parsed repersentation of the raw DNS packet.
 * Note that all integral values in the parsed packet are represented in
 * host byte order.
 */
typedef struct pj_dns_parsed_packet
{
    pj_dns_hdr		 hdr;	/**< Pointer to DNS hdr, in host byte order */
    pj_dns_parsed_query	*q;	/**< Array of DNS queries.		    */
    pj_dns_parsed_rr	*ans;	/**< Array of DNS RR answer.		    */
    pj_dns_parsed_rr	*ns;	/**< Array of NS record in the answer.	    */
    pj_dns_parsed_rr	*arr;	/**< Array of additional RR answer.	    */
} pj_dns_parsed_packet;


/**
 * Option flags to be specified when calling #pj_dns_packet_dup() function.
 * These flags can be combined with bitwise OR operation.
 */
enum pj_dns_dup_options
{
    PJ_DNS_NO_QD    = 1, /**< Do not duplicate the query section.	    */
    PJ_DNS_NO_ANS   = 2, /**< Do not duplicate the answer section.	    */
    PJ_DNS_NO_NS    = 4, /**< Do not duplicate the NS section.		    */
    PJ_DNS_NO_AR    = 8  /**< Do not duplicate the additional rec section   */
};


/**
 * Create DNS query packet to resolve the specified names. This function
 * can be used to build any types of DNS query, such as A record or DNS SRV
 * record.
 *
 * Application specifies the type of record and the name to be queried,
 * and the function will build the DNS query packet into the buffer
 * specified. Once the packet is successfully built, application can send
 * the packet via TCP or UDP connection.
 *
 * @param packet	The buffer to put the DNS query packet.
 * @param size		On input, it specifies the size of the buffer.
 *			On output, it will be filled with the actual size of
 *			the DNS query packet.
 * @param id		DNS query ID to associate DNS response with the
 *			query.
 * @param qtype		DNS type of record to be queried (see #pj_dns_type).
 * @param name		Name to be queried from the DNS server.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_dns_make_query(void *packet,
				       unsigned *size,
				       pj_uint16_t id,
				       int qtype,
				       const pj_str_t *name);

/**
 * Parse raw DNS packet into parsed DNS packet structure. This function is
 * able to parse few DNS resource records such as A record, PTR record,
 * CNAME record, NS record, and SRV record.
 *
 * @param pool		Pool to allocate memory for the parsed packet.
 * @param packet	Pointer to the DNS packet (the TCP/UDP payload of 
 *			the raw packet).
 * @param size		The size of the DNS packet.
 * @param p_res		Pointer to store the resulting parsed packet.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_dns_parse_packet(pj_pool_t *pool,
					 const void *packet,
					 unsigned size,
					 pj_dns_parsed_packet **p_res);

/**
 * Duplicate DNS packet.
 *
 * @param pool		The pool to allocate memory for the duplicated packet.
 * @param p		The DNS packet to be cloned.
 * @param options	Option flags, from pj_dns_dup_options.
 * @param p_dst		Pointer to store the cloned DNS packet.
 */
PJ_DECL(void) pj_dns_packet_dup(pj_pool_t *pool,
				const pj_dns_parsed_packet*p,
				unsigned options,
				pj_dns_parsed_packet **p_dst);


/**
 * Utility function to get the type name string of the specified DNS type.
 *
 * @param type		DNS type (see #pj_dns_type).
 *
 * @return		String name of the type (e.g. "A", "SRV", etc.).
 */
PJ_DECL(const char *) pj_dns_get_type_name(int type);


/**
 * Initialize DNS record as DNS SRV record.
 *
 * @param rec		The DNS resource record to be initialized as DNS
 *			SRV record.
 * @param res_name	Resource name.
 * @param dnsclass	DNS class.
 * @param ttl		Resource TTL value.
 * @param prio		DNS SRV priority.
 * @param weight	DNS SRV weight.
 * @param port		Target port.
 * @param target	Target name.
 */
PJ_DECL(void) pj_dns_init_srv_rr(pj_dns_parsed_rr *rec,
				 const pj_str_t *res_name,
				 unsigned dnsclass,
				 unsigned ttl,
				 unsigned prio,
				 unsigned weight,
				 unsigned port,
				 const pj_str_t *target);

/**
 * Initialize DNS record as DNS CNAME record.
 *
 * @param rec		The DNS resource record to be initialized as DNS
 *			CNAME record.
 * @param res_name	Resource name.
 * @param dnsclass	DNS class.
 * @param ttl		Resource TTL value.
 * @param name		Host name.
 */
PJ_DECL(void) pj_dns_init_cname_rr(pj_dns_parsed_rr *rec,
				   const pj_str_t *res_name,
				   unsigned dnsclass,
				   unsigned ttl,
				   const pj_str_t *name);

/**
 * Initialize DNS record as DNS A record.
 *
 * @param rec		The DNS resource record to be initialized as DNS
 *			A record.
 * @param res_name	Resource name.
 * @param dnsclass	DNS class.
 * @param ttl		Resource TTL value.
 * @param ip_addr	Host address.
 */
PJ_DECL(void) pj_dns_init_a_rr(pj_dns_parsed_rr *rec,
			       const pj_str_t *res_name,
			       unsigned dnsclass,
			       unsigned ttl,
			       const pj_in_addr *ip_addr);

/**
 * Dump DNS packet to standard log.
 *
 * @param res		The DNS packet.
 */
PJ_DECL(void) pj_dns_dump_packet(const pj_dns_parsed_packet *res);


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJLIB_UTIL_DNS_H__ */

