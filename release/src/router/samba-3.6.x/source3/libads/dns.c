/*
   Unix SMB/CIFS implementation.
   DNS utility library
   Copyright (C) Gerald (Jerry) Carter           2006.
   Copyright (C) Jeremy Allison                  2007.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "libads/dns.h"
#include "../librpc/ndr/libndr.h"

/* AIX resolv.h uses 'class' in struct ns_rr */

#if defined(AIX)
#  if defined(class)
#    undef class
#  endif
#endif	/* AIX */

/* resolver headers */

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <netdb.h>

#define MAX_DNS_PACKET_SIZE 0xffff

#ifdef NS_HFIXEDSZ	/* Bind 8/9 interface */
#if !defined(C_IN)	/* AIX 5.3 already defines C_IN */
#  define C_IN		ns_c_in
#endif
#if !defined(T_A)	/* AIX 5.3 already defines T_A */
#  define T_A   	ns_t_a
#endif

#if defined(HAVE_IPV6)
#if !defined(T_AAAA)
#  define T_AAAA	ns_t_aaaa
#endif
#endif

#  define T_SRV 	ns_t_srv
#if !defined(T_NS)	/* AIX 5.3 already defines T_NS */
#  define T_NS 		ns_t_ns
#endif
#else
#  ifdef HFIXEDSZ
#    define NS_HFIXEDSZ HFIXEDSZ
#  else
#    define NS_HFIXEDSZ sizeof(HEADER)
#  endif	/* HFIXEDSZ */
#  ifdef PACKETSZ
#    define NS_PACKETSZ	PACKETSZ
#  else	/* 512 is usually the default */
#    define NS_PACKETSZ	512
#  endif	/* PACKETSZ */
#  define T_SRV 	33
#endif

/*********************************************************************
*********************************************************************/

static bool ads_dns_parse_query( TALLOC_CTX *ctx, uint8 *start, uint8 *end,
                          uint8 **ptr, struct dns_query *q )
{
	uint8 *p = *ptr;
	char hostname[MAX_DNS_NAME_LENGTH];
	int namelen;

	ZERO_STRUCTP( q );

	if ( !start || !end || !q || !*ptr)
		return False;

	/* See RFC 1035 for details. If this fails, then return. */

	namelen = dn_expand( start, end, p, hostname, sizeof(hostname) );
	if ( namelen < 0 ) {
		return False;
	}
	p += namelen;
	q->hostname = talloc_strdup( ctx, hostname );

	/* check that we have space remaining */

	if ( PTR_DIFF(p+4, end) > 0 )
		return False;

	q->type     = RSVAL( p, 0 );
	q->in_class = RSVAL( p, 2 );
	p += 4;

	*ptr = p;

	return True;
}

/*********************************************************************
*********************************************************************/

static bool ads_dns_parse_rr( TALLOC_CTX *ctx, uint8 *start, uint8 *end,
                       uint8 **ptr, struct dns_rr *rr )
{
	uint8 *p = *ptr;
	char hostname[MAX_DNS_NAME_LENGTH];
	int namelen;

	if ( !start || !end || !rr || !*ptr)
		return -1;

	ZERO_STRUCTP( rr );
	/* pull the name from the answer */

	namelen = dn_expand( start, end, p, hostname, sizeof(hostname) );
	if ( namelen < 0 ) {
		return -1;
	}
	p += namelen;
	rr->hostname = talloc_strdup( ctx, hostname );

	/* check that we have space remaining */

	if ( PTR_DIFF(p+10, end) > 0 )
		return False;

	/* pull some values and then skip onto the string */

	rr->type     = RSVAL(p, 0);
	rr->in_class = RSVAL(p, 2);
	rr->ttl      = RIVAL(p, 4);
	rr->rdatalen = RSVAL(p, 8);

	p += 10;

	/* sanity check the available space */

	if ( PTR_DIFF(p+rr->rdatalen, end ) > 0 ) {
		return False;

	}

	/* save a point to the rdata for this section */

	rr->rdata = p;
	p += rr->rdatalen;

	*ptr = p;

	return True;
}

/*********************************************************************
*********************************************************************/

static bool ads_dns_parse_rr_srv( TALLOC_CTX *ctx, uint8 *start, uint8 *end,
                       uint8 **ptr, struct dns_rr_srv *srv )
{
	struct dns_rr rr;
	uint8 *p;
	char dcname[MAX_DNS_NAME_LENGTH];
	int namelen;

	if ( !start || !end || !srv || !*ptr)
		return -1;

	/* Parse the RR entry.  Coming out of the this, ptr is at the beginning
	   of the next record */

	if ( !ads_dns_parse_rr( ctx, start, end, ptr, &rr ) ) {
		DEBUG(1,("ads_dns_parse_rr_srv: Failed to parse RR record\n"));
		return False;
	}

	if ( rr.type != T_SRV ) {
		DEBUG(1,("ads_dns_parse_rr_srv: Bad answer type (%d)\n",
					rr.type));
		return False;
	}

	p = rr.rdata;

	srv->priority = RSVAL(p, 0);
	srv->weight   = RSVAL(p, 2);
	srv->port     = RSVAL(p, 4);

	p += 6;

	namelen = dn_expand( start, end, p, dcname, sizeof(dcname) );
	if ( namelen < 0 ) {
		DEBUG(1,("ads_dns_parse_rr_srv: Failed to uncompress name!\n"));
		return False;
	}

	srv->hostname = talloc_strdup( ctx, dcname );

	DEBUG(10,("ads_dns_parse_rr_srv: Parsed %s [%u, %u, %u]\n", 
		  srv->hostname, 
		  srv->priority,
		  srv->weight,
		  srv->port));

	return True;
}

/*********************************************************************
*********************************************************************/

static bool ads_dns_parse_rr_ns( TALLOC_CTX *ctx, uint8 *start, uint8 *end,
                       uint8 **ptr, struct dns_rr_ns *nsrec )
{
	struct dns_rr rr;
	uint8 *p;
	char nsname[MAX_DNS_NAME_LENGTH];
	int namelen;

	if ( !start || !end || !nsrec || !*ptr)
		return -1;

	/* Parse the RR entry.  Coming out of the this, ptr is at the beginning
	   of the next record */

	if ( !ads_dns_parse_rr( ctx, start, end, ptr, &rr ) ) {
		DEBUG(1,("ads_dns_parse_rr_ns: Failed to parse RR record\n"));
		return False;
	}

	if ( rr.type != T_NS ) {
		DEBUG(1,("ads_dns_parse_rr_ns: Bad answer type (%d)\n",
					rr.type));
		return False;
	}

	p = rr.rdata;

	/* ame server hostname */

	namelen = dn_expand( start, end, p, nsname, sizeof(nsname) );
	if ( namelen < 0 ) {
		DEBUG(1,("ads_dns_parse_rr_ns: Failed to uncompress name!\n"));
		return False;
	}
	nsrec->hostname = talloc_strdup( ctx, nsname );

	return True;
}

/*********************************************************************
 Sort SRV record list based on weight and priority.  See RFC 2782.
*********************************************************************/

static int dnssrvcmp( struct dns_rr_srv *a, struct dns_rr_srv *b )
{
	if ( a->priority == b->priority ) {

		/* randomize entries with an equal weight and priority */
		if ( a->weight == b->weight )
			return 0;

		/* higher weights should be sorted lower */
		if ( a->weight > b->weight )
			return -1;
		else
			return 1;
	}

	if ( a->priority < b->priority )
		return -1;

	return 1;
}

/*********************************************************************
 Simple wrapper for a DNS query
*********************************************************************/

#define DNS_FAILED_WAITTIME          30

static NTSTATUS dns_send_req( TALLOC_CTX *ctx, const char *name, int q_type,
                              uint8 **buf, int *resp_length )
{
	uint8 *buffer = NULL;
	size_t buf_len = 0;
	int resp_len = NS_PACKETSZ;
	static time_t last_dns_check = 0;
	static NTSTATUS last_dns_status = NT_STATUS_OK;
	time_t now = time_mono(NULL);

	/* Try to prevent bursts of DNS lookups if the server is down */

	/* Protect against large clock changes */

	if ( last_dns_check > now )
		last_dns_check = 0;

	/* IF we had a DNS timeout or a bad server and we are still
	   in the 30 second cache window, just return the previous
	   status and save the network timeout. */

	if ( (NT_STATUS_EQUAL(last_dns_status,NT_STATUS_IO_TIMEOUT) ||
	      NT_STATUS_EQUAL(last_dns_status,NT_STATUS_CONNECTION_REFUSED)) &&
	     (last_dns_check+DNS_FAILED_WAITTIME) > now )
	{
		DEBUG(10,("last_dns_check: Returning cached status (%s)\n",
			  nt_errstr(last_dns_status) ));
		return last_dns_status;
	}

	/* Send the Query */
	do {
		if ( buffer )
			TALLOC_FREE( buffer );

		buf_len = resp_len * sizeof(uint8);

		if (buf_len) {
			if ((buffer = TALLOC_ARRAY(ctx, uint8, buf_len))
					== NULL ) {
				DEBUG(0,("ads_dns_lookup_srv: "
					"talloc() failed!\n"));
				last_dns_status = NT_STATUS_NO_MEMORY;
				last_dns_check = time_mono(NULL);
				return last_dns_status;
			}
		}

		if ((resp_len = res_query(name, C_IN, q_type, buffer, buf_len))
				< 0 ) {
			DEBUG(3,("ads_dns_lookup_srv: "
				"Failed to resolve %s (%s)\n",
				name, strerror(errno)));
			TALLOC_FREE( buffer );
			last_dns_status = NT_STATUS_UNSUCCESSFUL;

			if (errno == ETIMEDOUT) {
				last_dns_status = NT_STATUS_IO_TIMEOUT;
			}
			if (errno == ECONNREFUSED) {
				last_dns_status = NT_STATUS_CONNECTION_REFUSED;
			}
			last_dns_check = time_mono(NULL);
			return last_dns_status;
		}

		/* On AIX, Solaris, and possibly some older glibc systems (e.g. SLES8)
		   truncated replies never give back a resp_len > buflen
		   which ends up causing DNS resolve failures on large tcp DNS replies */

		if (buf_len == resp_len) {
			if (resp_len == MAX_DNS_PACKET_SIZE) {
				DEBUG(1,("dns_send_req: DNS reply too large when resolving %s\n",
					name));
				TALLOC_FREE( buffer );
				last_dns_status = NT_STATUS_BUFFER_TOO_SMALL;
				last_dns_check = time_mono(NULL);
				return last_dns_status;
			}

			resp_len = MIN(resp_len*2, MAX_DNS_PACKET_SIZE);
		}


	} while ( buf_len < resp_len && resp_len <= MAX_DNS_PACKET_SIZE );

	*buf = buffer;
	*resp_length = resp_len;

	last_dns_check = time_mono(NULL);
	last_dns_status = NT_STATUS_OK;
	return last_dns_status;
}

/*********************************************************************
 Simple wrapper for a DNS SRV query
*********************************************************************/

static NTSTATUS ads_dns_lookup_srv( TALLOC_CTX *ctx,
				const char *name,
				struct dns_rr_srv **dclist,
				int *numdcs)
{
	uint8 *buffer = NULL;
	int resp_len = 0;
	struct dns_rr_srv *dcs = NULL;
	int query_count, answer_count, auth_count, additional_count;
	uint8 *p = buffer;
	int rrnum;
	int idx = 0;
	NTSTATUS status;

	if ( !ctx || !name || !dclist ) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* Send the request.  May have to loop several times in case
	   of large replies */

	status = dns_send_req( ctx, name, T_SRV, &buffer, &resp_len );
	if ( !NT_STATUS_IS_OK(status) ) {
		DEBUG(3,("ads_dns_lookup_srv: Failed to send DNS query (%s)\n",
			nt_errstr(status)));
		return status;
	}
	p = buffer;

	/* For some insane reason, the ns_initparse() et. al. routines are only
	   available in libresolv.a, and not the shared lib.  Who knows why....
	   So we have to parse the DNS reply ourselves */

	/* Pull the answer RR's count from the header.
	 * Use the NMB ordering macros */

	query_count      = RSVAL( p, 4 );
	answer_count     = RSVAL( p, 6 );
	auth_count       = RSVAL( p, 8 );
	additional_count = RSVAL( p, 10 );

	DEBUG(4,("ads_dns_lookup_srv: "
		"%d records returned in the answer section.\n",
		answer_count));

	if (answer_count) {
		if ((dcs = TALLOC_ZERO_ARRAY(ctx, struct dns_rr_srv,
						answer_count)) == NULL ) {
			DEBUG(0,("ads_dns_lookup_srv: "
				"talloc() failure for %d char*'s\n",
				answer_count));
			return NT_STATUS_NO_MEMORY;
		}
	} else {
		dcs = NULL;
	}

	/* now skip the header */

	p += NS_HFIXEDSZ;

	/* parse the query section */

	for ( rrnum=0; rrnum<query_count; rrnum++ ) {
		struct dns_query q;

		if (!ads_dns_parse_query(ctx, buffer,
					buffer+resp_len, &p, &q)) {
			DEBUG(1,("ads_dns_lookup_srv: "
				 "Failed to parse query record [%d]!\n", rrnum));
			return NT_STATUS_UNSUCCESSFUL;
		}
	}

	/* now we are at the answer section */

	for ( rrnum=0; rrnum<answer_count; rrnum++ ) {
		if (!ads_dns_parse_rr_srv(ctx, buffer, buffer+resp_len,
					&p, &dcs[rrnum])) {
			DEBUG(1,("ads_dns_lookup_srv: "
				 "Failed to parse answer recordi [%d]!\n", rrnum));
			return NT_STATUS_UNSUCCESSFUL;
		}
	}
	idx = rrnum;

	/* Parse the authority section */
	/* just skip these for now */

	for ( rrnum=0; rrnum<auth_count; rrnum++ ) {
		struct dns_rr rr;

		if (!ads_dns_parse_rr( ctx, buffer,
					buffer+resp_len, &p, &rr)) {
			DEBUG(1,("ads_dns_lookup_srv: "
				 "Failed to parse authority record! [%d]\n", rrnum));
			return NT_STATUS_UNSUCCESSFUL;
		}
	}

	/* Parse the additional records section */

	for ( rrnum=0; rrnum<additional_count; rrnum++ ) {
		struct dns_rr rr;
		int i;

		if (!ads_dns_parse_rr(ctx, buffer, buffer+resp_len,
					&p, &rr)) {
			DEBUG(1,("ads_dns_lookup_srv: Failed "
				 "to parse additional records section! [%d]\n", rrnum));
			return NT_STATUS_UNSUCCESSFUL;
		}

		/* Only interested in A or AAAA records as a shortcut for having
		 * to come back later and lookup the name. For multi-homed
		 * hosts, the number of additional records and exceed the
		 * number of answer records. */

		if (rr.type != T_A || rr.rdatalen != 4) {
#if defined(HAVE_IPV6)
			/* RFC2874 defines A6 records. This
			 * requires recusive and horribly complex lookups.
			 * Bastards. Ignore this for now.... JRA.
			 * Luckily RFC3363 reprecates A6 records.
			 */
			if (rr.type != T_AAAA || rr.rdatalen != 16)
#endif
				continue;
		}

		for ( i=0; i<idx; i++ ) {
			if ( strcmp( rr.hostname, dcs[i].hostname ) == 0 ) {
				int num_ips = dcs[i].num_ips;
				struct sockaddr_storage *tmp_ss_s;

				/* allocate new memory */

				if (dcs[i].num_ips == 0) {
					if ((dcs[i].ss_s = TALLOC_ARRAY(dcs,
						struct sockaddr_storage, 1 ))
							== NULL ) {
						return NT_STATUS_NO_MEMORY;
					}
				} else {
					if ((tmp_ss_s = TALLOC_REALLOC_ARRAY(dcs,
							dcs[i].ss_s,
							struct sockaddr_storage,
							dcs[i].num_ips+1))
								== NULL ) {
						return NT_STATUS_NO_MEMORY;
					}

					dcs[i].ss_s = tmp_ss_s;
				}
				dcs[i].num_ips++;

				/* copy the new IP address */
				if (rr.type == T_A) {
					struct in_addr ip;
					memcpy(&ip, rr.rdata, 4);
					in_addr_to_sockaddr_storage(
							&dcs[i].ss_s[num_ips],
							ip);
				}
#if defined(HAVE_IPV6)
				if (rr.type == T_AAAA) {
					struct in6_addr ip6;
					memcpy(&ip6, rr.rdata, rr.rdatalen);
					in6_addr_to_sockaddr_storage(
							&dcs[i].ss_s[num_ips],
							ip6);
				}
#endif
			}
		}
	}

	TYPESAFE_QSORT(dcs, idx, dnssrvcmp );

	*dclist = dcs;
	*numdcs = idx;

	return NT_STATUS_OK;
}

/*********************************************************************
 Simple wrapper for a DNS NS query
*********************************************************************/

NTSTATUS ads_dns_lookup_ns(TALLOC_CTX *ctx,
				const char *dnsdomain,
				struct dns_rr_ns **nslist,
				int *numns)
{
	uint8 *buffer = NULL;
	int resp_len = 0;
	struct dns_rr_ns *nsarray = NULL;
	int query_count, answer_count, auth_count, additional_count;
	uint8 *p;
	int rrnum;
	int idx = 0;
	NTSTATUS status;

	if ( !ctx || !dnsdomain || !nslist ) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* Send the request.  May have to loop several times in case
	   of large replies */

	status = dns_send_req( ctx, dnsdomain, T_NS, &buffer, &resp_len );
	if ( !NT_STATUS_IS_OK(status) ) {
		DEBUG(3,("ads_dns_lookup_ns: Failed to send DNS query (%s)\n",
			nt_errstr(status)));
		return status;
	}
	p = buffer;

	/* For some insane reason, the ns_initparse() et. al. routines are only
	   available in libresolv.a, and not the shared lib.  Who knows why....
	   So we have to parse the DNS reply ourselves */

	/* Pull the answer RR's count from the header.
	 * Use the NMB ordering macros */

	query_count      = RSVAL( p, 4 );
	answer_count     = RSVAL( p, 6 );
	auth_count       = RSVAL( p, 8 );
	additional_count = RSVAL( p, 10 );

	DEBUG(4,("ads_dns_lookup_ns: "
		"%d records returned in the answer section.\n",
		answer_count));

	if (answer_count) {
		if ((nsarray = TALLOC_ARRAY(ctx, struct dns_rr_ns,
						answer_count)) == NULL ) {
			DEBUG(0,("ads_dns_lookup_ns: "
				"talloc() failure for %d char*'s\n",
				answer_count));
			return NT_STATUS_NO_MEMORY;
		}
	} else {
		nsarray = NULL;
	}

	/* now skip the header */

	p += NS_HFIXEDSZ;

	/* parse the query section */

	for ( rrnum=0; rrnum<query_count; rrnum++ ) {
		struct dns_query q;

		if (!ads_dns_parse_query(ctx, buffer, buffer+resp_len,
					&p, &q)) {
			DEBUG(1,("ads_dns_lookup_ns: "
				" Failed to parse query record!\n"));
			return NT_STATUS_UNSUCCESSFUL;
		}
	}

	/* now we are at the answer section */

	for ( rrnum=0; rrnum<answer_count; rrnum++ ) {
		if (!ads_dns_parse_rr_ns(ctx, buffer, buffer+resp_len,
					&p, &nsarray[rrnum])) {
			DEBUG(1,("ads_dns_lookup_ns: "
				"Failed to parse answer record!\n"));
			return NT_STATUS_UNSUCCESSFUL;
		}
	}
	idx = rrnum;

	/* Parse the authority section */
	/* just skip these for now */

	for ( rrnum=0; rrnum<auth_count; rrnum++ ) {
		struct dns_rr rr;

		if ( !ads_dns_parse_rr(ctx, buffer, buffer+resp_len,
					&p, &rr)) {
			DEBUG(1,("ads_dns_lookup_ns: "
				"Failed to parse authority record!\n"));
			return NT_STATUS_UNSUCCESSFUL;
		}
	}

	/* Parse the additional records section */

	for ( rrnum=0; rrnum<additional_count; rrnum++ ) {
		struct dns_rr rr;
		int i;

		if (!ads_dns_parse_rr(ctx, buffer, buffer+resp_len,
					&p, &rr)) {
			DEBUG(1,("ads_dns_lookup_ns: Failed "
				"to parse additional records section!\n"));
			return NT_STATUS_UNSUCCESSFUL;
		}

		/* only interested in A records as a shortcut for having to come
		   back later and lookup the name */

		if (rr.type != T_A || rr.rdatalen != 4) {
#if defined(HAVE_IPV6)
			if (rr.type != T_AAAA || rr.rdatalen != 16)
#endif
				continue;
		}

		for ( i=0; i<idx; i++ ) {
			if (strcmp(rr.hostname, nsarray[i].hostname) == 0) {
				if (rr.type == T_A) {
					struct in_addr ip;
					memcpy(&ip, rr.rdata, 4);
					in_addr_to_sockaddr_storage(
							&nsarray[i].ss,
							ip);
				}
#if defined(HAVE_IPV6)
				if (rr.type == T_AAAA) {
					struct in6_addr ip6;
					memcpy(&ip6, rr.rdata, rr.rdatalen);
					in6_addr_to_sockaddr_storage(
							&nsarray[i].ss,
							ip6);
				}
#endif
			}
		}
	}

	*nslist = nsarray;
	*numns = idx;

	return NT_STATUS_OK;
}

/********************************************************************
 Query with optional sitename.
********************************************************************/

static NTSTATUS ads_dns_query_internal(TALLOC_CTX *ctx,
				       const char *servicename,
				       const char *dc_pdc_gc_domains,
				       const char *realm,
				       const char *sitename,
				       struct dns_rr_srv **dclist,
				       int *numdcs )
{
	char *name;
	if (sitename) {
		name = talloc_asprintf(ctx, "%s._tcp.%s._sites.%s._msdcs.%s",
				       servicename, sitename,
				       dc_pdc_gc_domains, realm);
  	} else {
		name = talloc_asprintf(ctx, "%s._tcp.%s._msdcs.%s",
				servicename, dc_pdc_gc_domains, realm);
  	}
	if (!name) {
		return NT_STATUS_NO_MEMORY;
	}
	return ads_dns_lookup_srv( ctx, name, dclist, numdcs );
}

/********************************************************************
 Query for AD DC's.
********************************************************************/

NTSTATUS ads_dns_query_dcs(TALLOC_CTX *ctx,
			   const char *realm,
			   const char *sitename,
			   struct dns_rr_srv **dclist,
			   int *numdcs )
{
	NTSTATUS status;

	status = ads_dns_query_internal(ctx, "_ldap", "dc", realm, sitename,
					dclist, numdcs);

	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_CONNECTION_REFUSED)) {
		return status;
	}

	if (sitename &&
	    ((!NT_STATUS_IS_OK(status)) ||
	     (NT_STATUS_IS_OK(status) && (numdcs == 0)))) {
		/* Sitename DNS query may have failed. Try without. */
		status = ads_dns_query_internal(ctx, "_ldap", "dc", realm,
						NULL, dclist, numdcs);
	}
	return status;
}

/********************************************************************
 Query for AD GC's.
********************************************************************/

NTSTATUS ads_dns_query_gcs(TALLOC_CTX *ctx,
			   const char *realm,
			   const char *sitename,
			   struct dns_rr_srv **dclist,
			   int *numdcs )
{
	NTSTATUS status;

	status = ads_dns_query_internal(ctx, "_ldap", "gc", realm, sitename,
					dclist, numdcs);

	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_CONNECTION_REFUSED)) {
		return status;
	}

	if (sitename &&
	    ((!NT_STATUS_IS_OK(status)) ||
	     (NT_STATUS_IS_OK(status) && (numdcs == 0)))) {
		/* Sitename DNS query may have failed. Try without. */
		status = ads_dns_query_internal(ctx, "_ldap", "gc", realm,
						NULL, dclist, numdcs);
	}
	return status;
}

/********************************************************************
 Query for AD KDC's.
 Even if our underlying kerberos libraries are UDP only, this
 is pretty safe as it's unlikely that a KDC supports TCP and not UDP.
********************************************************************/

NTSTATUS ads_dns_query_kdcs(TALLOC_CTX *ctx,
			    const char *dns_forest_name,
			    const char *sitename,
			    struct dns_rr_srv **dclist,
			    int *numdcs )
{
	NTSTATUS status;

	status = ads_dns_query_internal(ctx, "_kerberos", "dc",
					dns_forest_name, sitename, dclist,
					numdcs);

	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_CONNECTION_REFUSED)) {
		return status;
	}

	if (sitename &&
	    ((!NT_STATUS_IS_OK(status)) ||
	     (NT_STATUS_IS_OK(status) && (numdcs == 0)))) {
		/* Sitename DNS query may have failed. Try without. */
		status = ads_dns_query_internal(ctx, "_kerberos", "dc",
						dns_forest_name, NULL,
						dclist, numdcs);
	}
	return status;
}

/********************************************************************
 Query for AD PDC. Sitename is obsolete here.
********************************************************************/

NTSTATUS ads_dns_query_pdc(TALLOC_CTX *ctx,
			   const char *dns_domain_name,
			   struct dns_rr_srv **dclist,
			   int *numdcs )
{
	return ads_dns_query_internal(ctx, "_ldap", "pdc", dns_domain_name,
				      NULL, dclist, numdcs);
}

/********************************************************************
 Query for AD DC by guid. Sitename is obsolete here.
********************************************************************/

NTSTATUS ads_dns_query_dcs_guid(TALLOC_CTX *ctx,
				const char *dns_forest_name,
				const struct GUID *domain_guid,
				struct dns_rr_srv **dclist,
				int *numdcs )
{
	/*_ldap._tcp.DomainGuid.domains._msdcs.DnsForestName */

	const char *domains;
	char *guid_string;

	guid_string = GUID_string(ctx, domain_guid);
	if (!guid_string) {
		return NT_STATUS_NO_MEMORY;
	}

	/* little hack */
	domains = talloc_asprintf(ctx, "%s.domains", guid_string);
	if (!domains) {
		return NT_STATUS_NO_MEMORY;
	}
	TALLOC_FREE(guid_string);

	return ads_dns_query_internal(ctx, "_ldap", domains, dns_forest_name,
				      NULL, dclist, numdcs);
}
