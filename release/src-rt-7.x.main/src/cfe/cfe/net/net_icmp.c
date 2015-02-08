/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  ICMP Protocol				File: net_icmp.c
    *  
    *  This module implements portions of the ICMP protocol.  Note
    *  that it is not a complete implementation, just enough to 
    *  generate and respond to ICMP echo requests.
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

#include "net_ebuf.h"
#include "net_ether.h"

#include "net_ip.h"

/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define ICMP_CODE_ECHO		0
#define ICMP_TYPE_ECHOREPLY	0
#define ICMP_TYPE_ECHOREQ	8

#define ICMPMSG(type,code) (((type)<<8)|(code))

/*  *********************************************************************
    *  Structures
    ********************************************************************* */

struct icmp_info_s {
    ip_info_t *icmp_ipinfo;
    queue_t icmp_echoreplies;
    int icmp_maxreplies;
};

/*  *********************************************************************
    *  ICMP_RX_CALLBACK(ref,buf,dst,src)
    *  
    *  This routine is called by the IP layer when we receive
    *  ICMP protocol messages.
    *  
    *  Input parameters: 
    *  	   ref - reference data (an icmp_info_t)
    *  	   buf - the ebuf containing the buffer
    *  	   dst - destination IP address (us, usually)
    *  	   src - source IP address
    *  	   
    *  Return value:
    *  	   ETH_KEEP to keep packet, ETH_DROP to cause packet to be freed
    ********************************************************************* */

static int icmp_rx_callback(void *ref,ebuf_t *buf,uint8_t *dst,uint8_t *src)
{
    icmp_info_t *icmp = (icmp_info_t *)ref;
    ip_info_t *ipi = icmp->icmp_ipinfo;
    uint16_t imsg;
    uint16_t cksum;
    ebuf_t *txbuf;
    uint8_t *icmphdr;
    int res;

    imsg = ICMPMSG(buf->eb_ptr[0],buf->eb_ptr[1]);

    res = ETH_DROP;			/* assume we're dropping the pkt */

    switch (imsg) {
	case ICMPMSG(ICMP_TYPE_ECHOREQ,ICMP_CODE_ECHO):
	    txbuf = _ip_alloc(ipi);
	    if (txbuf) {
		/* Construct reply from the original packet. */		   
		icmphdr = txbuf->eb_ptr;
		ebuf_append_bytes(txbuf,buf->eb_ptr,buf->eb_length);
		icmphdr[0] = ICMP_TYPE_ECHOREPLY;
		icmphdr[1] = ICMP_CODE_ECHO;
		icmphdr[2] = 0; icmphdr[3] = 0;
		cksum = ~ip_chksum(0,icmphdr,ebuf_length(txbuf));
		icmphdr[2] = (cksum >> 8) & 0xFF;
		icmphdr[3] = (cksum & 0xFF);
		if (_ip_send(ipi,txbuf,src,IPPROTO_ICMP) < 0) {
		    _ip_free(ipi,txbuf);
		    }
		}
	    break;

	case ICMPMSG(ICMP_TYPE_ECHOREPLY,ICMP_CODE_ECHO):
	    if (q_count(&(icmp->icmp_echoreplies)) < icmp->icmp_maxreplies) {
		/* We're keeping this packet, put it on the queue and don't
		   free it in the driver. */
		q_enqueue(&(icmp->icmp_echoreplies),(queue_t *) buf);
		res = ETH_KEEP;
		}
	    break;

	default:
	    res = ETH_DROP;
	}
    
    return res;
}


/*  *********************************************************************
    *  _ICMP_INIT(ipi)
    *  
    *  Initialize the ICMP layer.
    *  
    *  Input parameters: 
    *  	   ipi - ipinfo structure of IP layer to attach to
    *  	   
    *  Return value:
    *  	   icmp_info_t structure or NULL if error occurs
    ********************************************************************* */

icmp_info_t *_icmp_init(ip_info_t *ipi)
{
    icmp_info_t *icmp;

    icmp = (icmp_info_t *) KMALLOC(sizeof(icmp_info_t),0);
    if (!icmp) return NULL;

    icmp->icmp_ipinfo = ipi;
    q_init(&(icmp->icmp_echoreplies));
    icmp->icmp_maxreplies = 0;

    _ip_register(ipi,IPPROTO_ICMP,icmp_rx_callback,icmp);

    return icmp;
}

/*  *********************************************************************
    *  _ICMP_UNINIT(icmp)
    *  
    *  Un-initialize the ICMP layer.
    *  
    *  Input parameters: 
    *  	   icmp - icmp_info_t structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void _icmp_uninit(icmp_info_t *icmp)
{
    _ip_deregister(icmp->icmp_ipinfo,IPPROTO_ICMP);

    KFREE(icmp);
}


/*  *********************************************************************
    *  _ICMP_PING(icmp,dest,seq,len)
    *  
    *  Transmit an ICMP echo request and wait for a reply.
    *  
    *  Input parameters: 
    *  	   icmp - icmp_info_t structure
    *  	   dest - destination IP address
    *  	   seq - sequence number for ICMP packet
    *  	   len - length of data portion of ICMP packet
    *  	   
    *  Return value:
    *  	   <0 = error
    *  	   0 = timeout
    *  	   >0 = reply received
    ********************************************************************* */

int _icmp_ping(icmp_info_t *icmp,uint8_t *dest,int seq,int len)
{
    ebuf_t *buf;
    int64_t timer;
    uint16_t cksum;
    uint8_t *icmphdr;
    uint16_t id; 
    int idx;
    int result = 0;

    /*
     * Get an ebuf
     */

    buf = _ip_alloc(icmp->icmp_ipinfo);
    if (buf == NULL) return -1;

    /* 
     * Remember where the ICMP header is going to be so we can
     * calculate the checksum later.
     */

    icmphdr = buf->eb_ptr;

    id = (uint16_t) cfe_ticks;

    /*
     * Construct the ICMP header and data portion.
     */

    ebuf_append_u8(buf,8);		/* echo message */
    ebuf_append_u8(buf,0);		/* code = 0 */
    ebuf_append_u16_be(buf,0);		/* empty checksum for now */
    ebuf_append_u16_be(buf,id);		/* packet ID */
    ebuf_append_u16_be(buf,((uint16_t)seq)); /* sequence # */

    for (idx = 0; idx < len; idx++) {
	ebuf_append_u8(buf,((idx+0x40)&0xFF));	/* data */
	}

    /*
     * Calculate and install the checksum
     */

    cksum = ~ip_chksum(0,icmphdr,ebuf_length(buf));
    icmphdr[2] = (cksum >> 8) & 0xFF;
    icmphdr[3] = (cksum & 0xFF);

    /*
     * Transmit the ICMP echo 
     */ 

    icmp->icmp_maxreplies = 1;		/* allow ICMP replies */
    _ip_send(icmp->icmp_ipinfo,buf,dest,IPPROTO_ICMP);
    buf = NULL;

    /*
     * Wait for a reply
     */

    TIMER_SET(timer,2*CFE_HZ);

    while (!TIMER_EXPIRED(timer)) {

	POLL();
	buf = (ebuf_t *) q_deqnext(&(icmp->icmp_echoreplies));

	/* If we get a packet, make sure it matches. */

	if (buf) {
	    uint16_t rxid,rxseq;

	    cksum = ip_chksum(0,buf->eb_ptr,ebuf_length(buf));
	    if (cksum == 0xFFFF) {
		ebuf_skip(buf,2);
		ebuf_skip(buf,2);		/* skip checksum */
		ebuf_get_u16_be(buf,rxid);
		ebuf_get_u16_be(buf,rxseq);

		if ((id == rxid) && ((uint16_t) seq == rxseq)) {
		    result = 1;
		    break;
		    }
		}
	    _ip_free(icmp->icmp_ipinfo,buf);
	    }
	}

    /*
     * Don't accept any more replies.
     */

    icmp->icmp_maxreplies = 0;		/* allow ICMP replies */

    if (buf) _ip_free(icmp->icmp_ipinfo,buf);

    return result;

}
