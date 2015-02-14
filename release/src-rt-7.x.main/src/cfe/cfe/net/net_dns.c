/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Domain Name System Resolver		File: net_dns.c
    *  
    *  This module provides minimal support for looking up IP addresses
    *  from DNS name servers (RFCxxxx)
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */


#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_timer.h"
#include "cfe_error.h"

#include "net_ebuf.h"
#include "net_ether.h"

#include "cfe.h"

#include "net_api.h"

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define ip_addriszero(a) (((a)[0]|(a)[1]|(a)[2]|(a)[3]) == 0)

/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define UDP_PORT_DNS	53

#define DNS_FLG_QUERY	 0x0000
#define DNS_FLG_RESPONSE 0x8000
#define DNS_OPCODE_QUERY 0x0000
#define DNS_FLG_AA       0x0400
#define DNS_FLG_TC       0x0200
#define DNS_FLG_RD       0x0100
#define DNS_FLG_RA       0x0080
#define DNS_RCODE_MASK   0x000F
#define DNS_RCODE_OK	 0x0000
#define DNS_RCODE_NAMEERR 0x0003


#define QTYPE_HOSTADDR	1
#define QCLASS_INTERNET	1

#define DNS_QUERY_TIMEOUT  1 /* seconds */
#define DNS_RETRY_COUNT    8


/*  *********************************************************************
    *  dns_dolookup(s,hostname,ipaddr)
    *  
    *  Look up a host name and return its IP address.
    *  
    *  Input parameters: 
    * 	   s - udp socket
    *      server - name server to query
    *  	   hostname - host name to find
    *  	   ipaddr - buffer to place IP address 
    *  	   
    *  Return value:
    *  	   0 if no responses found
    *      >0 to indicate number of response records (usually 1)
    *  	   else error code
    ********************************************************************* */

static int dns_dolookup(int s,uint8_t *server,char *hostname,uint8_t *ipaddr)
{
    ebuf_t *buf;
    uint16_t id;
    uint16_t tmp;
    char *tok;
    char *ptr;
    int64_t timer;
    int nres = 0;
    uint16_t nqr,nar,nns,nxr;
    uint8_t tmpb;
    int expired;


    /*
     * Use the current time for our request ID 
     */

    id = (uint16_t) cfe_ticks;

    /*
     * Get a buffer and fill it in.
     */

    buf = udp_alloc();

    ebuf_append_u16_be(buf,id);
    ebuf_append_u16_be(buf,(DNS_FLG_QUERY | DNS_OPCODE_QUERY | DNS_FLG_RD));
    ebuf_append_u16_be(buf,1);		/* one question */
    ebuf_append_u16_be(buf,0);		/* no answers */
    ebuf_append_u16_be(buf,0);		/* no server resource records */
    ebuf_append_u16_be(buf,0);		/* no additional records */

    /*
     * Chop up the hostname into pieces, basically replacing 
     * the dots with length indicators.
     */

    ptr = hostname;

    while ((tok = strchr(ptr,'.'))) {
	ebuf_append_u8(buf,(tok-ptr));
	ebuf_append_bytes(buf,ptr,(tok-ptr));
	ptr = tok + 1;
	}

    ebuf_append_u8(buf,strlen(ptr));
    ebuf_append_bytes(buf,ptr,strlen(ptr));
    ebuf_append_u8(buf,0);
    ebuf_append_u16_be(buf,QTYPE_HOSTADDR);
    ebuf_append_u16_be(buf,QCLASS_INTERNET);

    /*
     * Send the request to the name server
     */

    udp_send(s,buf,server);

    /*
     * Set a timer for the response
     */

    TIMER_SET(timer,DNS_QUERY_TIMEOUT*CFE_HZ);

    /*
     * Start waiting...
     */

    while (!(expired = TIMER_EXPIRED(timer))) {
	POLL();

	buf = udp_recv(s);
	if (!buf) continue;

	/* IDs must match */

	ebuf_get_u16_be(buf,tmp);
	if (id != tmp) goto drop;

	/* It must be a response */

	ebuf_get_u16_be(buf,tmp);

	if ((tmp & DNS_FLG_RESPONSE) == 0)  goto drop;

	if ((tmp & DNS_RCODE_MASK) != DNS_RCODE_OK) {
	    udp_free(buf);
	    /* name error */
	    break;
	    }

	ebuf_get_u16_be(buf,nqr);
	ebuf_get_u16_be(buf,nar);
	ebuf_get_u16_be(buf,nns);
	ebuf_get_u16_be(buf,nxr);

	if (nar == 0) {
	    goto drop;
	    }

	while (nqr > 0) {
	    if (ebuf_length(buf) <= 0) {
		goto drop;
		}
	    for (;;) {
		ebuf_get_u8(buf,tmpb);
		if (tmpb == 0) break;
		ebuf_skip(buf,tmpb);
		if (ebuf_length(buf) <= 0) {
		    goto drop;
		    }
		}
	    ebuf_skip(buf,2);	/* skip QTYPE */
	    ebuf_skip(buf,2);	/* skip QCLASS */
	    nqr--;		/* next question record */
	    }

	/*
	 * Loop through the answer records to find
	 * a HOSTADDR record.  Ignore any other records
	 * we find.
	 */

	while (nar > 0) {
	    uint16_t rname,rtype,rclass,dlen;

	    ebuf_get_u16_be(buf,rname);	/* resource name */

	    ebuf_get_u16_be(buf,rtype);	/* resource type */

	    ebuf_get_u16_be(buf,rclass);	/* resource class */

	    ebuf_skip(buf,4);		/* time to live */

	    ebuf_get_u16_be(buf,dlen);	/* length of data */

	    if (rtype != QTYPE_HOSTADDR) {
		ebuf_skip(buf,dlen);
		nar--;
		continue;
		}
	    if (rclass != QCLASS_INTERNET) {
		ebuf_skip(buf,dlen);
		nar--;
		continue;
		}

	    if (dlen != IP_ADDR_LEN) {
		ebuf_skip(buf,dlen);
		nar--;
		continue;
		}

	    ebuf_get_bytes(buf,ipaddr,IP_ADDR_LEN);
	    break;
	    }

	if (nar == 0) goto drop;

	udp_free(buf);
	nres++;
	break;

    drop:
	udp_free(buf);
	}

    if (expired) return CFE_ERR_TIMEOUT;
    if (nres == 0) return CFE_ERR_HOSTUNKNOWN;
    return nres;
}


/*  *********************************************************************
    *  dns_lookup(hostname,ipaddr)
    *  
    *  Look up a host name and return its IP address.
    *  
    *  Input parameters: 
    *  	   hostname - host name to find
    *  	   ipaddr - buffer to place IP address 
    *  	   
    *  Return value:
    *  	   0 if no responses found
    *      >0 to indicate number of response records (usually 1)
    *  	   else error code
    ********************************************************************* */

int dns_lookup(char *hostname,uint8_t *ipaddr)
{
    int s;
    int nres = 0;
    int retries;
    char temphostname[100];
    uint8_t *server;
    char *tok;

    /*
     * If it's a valid IP address, don't look it up.
     */

    if (parseipaddr(hostname,ipaddr) == 0) return 1;

    /*
     * Add default domain if no domain was specified
     */

    if (strchr(hostname,'.') == NULL) {
	tok = (char *)net_getparam(NET_DOMAIN);
	if (tok) {
	    xsprintf(temphostname,"%s.%s",hostname,tok);
	    hostname = temphostname;
	    }
	}

    /*
     * Figure out who the name server is
     */

    server = net_getparam(NET_NAMESERVER);

    if (!server) return CFE_ERR_NETDOWN;
    if (ip_addriszero(server)) return CFE_ERR_NONAMESERVER;

    /*
     * Go do the name server lookup
     */

    s = udp_socket(UDP_PORT_DNS);
    if (s < 0) return CFE_ERR_NOHANDLES;

    for (retries = 0; retries < DNS_RETRY_COUNT; retries++) {
	nres = dns_dolookup(s,server,hostname,ipaddr);
	if (nres == CFE_ERR_TIMEOUT) continue;
	if (nres >= 0) break;
	}

    udp_close(s);

    return nres;

}
