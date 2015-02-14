/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Internet Protocol			File: net_ip.c
    *  
    *  This module implements the IP layer (RFC791)
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

#include "net_ebuf.h"
#include "net_ether.h"

#include "net_ip.h"
#include "net_ip_internal.h"

#include "cfe_error.h"

/*  *********************************************************************
    *  Forward declarations
    ********************************************************************* */

static int ip_rx_callback(ebuf_t *buf,void *ref);

/*  *********************************************************************
    *  _ip_alloc(ipi)
    *  
    *  Allocate an ebuf and reserve space for the IP header in it.
    *  
    *  Input parameters: 
    *  	   ipi - IP stack information
    *  	   
    *  Return value:
    *  	   ebuf - an ebuf, or NULL if there are none left
    ********************************************************************* */

ebuf_t *_ip_alloc(ip_info_t *ipi)
{
    ebuf_t *buf;

    buf = eth_alloc(ipi->eth_info,ipi->ip_port);

    if (buf == NULL) return buf;

    ebuf_seek(buf,IPHDR_LENGTH);
    ebuf_setlength(buf,0);

    return buf;
}


/*  *********************************************************************
    *  ip_chksum(initcksum,ptr,len)
    *  
    *  Do an IP checksum for the specified buffer.  You can pass
    *  an initial checksum if you're continuing a previous checksum
    *  calculation, such as for UDP headers and pseudoheaders.
    *  
    *  Input parameters: 
    *  	   initcksum - initial checksum (usually zero)
    *  	   ptr - pointer to buffer to checksum
    *  	   len - length of data in bytes
    *  	   
    *  Return value:
    *  	   checksum (16 bits)
    ********************************************************************* */

uint16_t ip_chksum(uint16_t initcksum,uint8_t *ptr,int len)
{
    unsigned int cksum;
    int idx;
    int odd;

    cksum = (unsigned int) initcksum;

    odd = len & 1;
    len -= odd;

    for (idx = 0; idx < len; idx += 2) {
	cksum += ((unsigned long) ptr[idx] << 8) + ((unsigned long) ptr[idx+1]);
	}

    if (odd) {		/* buffer is odd length */
	cksum += ((unsigned long) ptr[idx] << 8);
	}

    /*
     * Fold in the carries
     */

    while (cksum >> 16) {
	cksum = (cksum & 0xFFFF) + (cksum >> 16);
	}

    return cksum;
}


/*  *********************************************************************
    *  _ip_send(ipi,buf,destaddr,proto)
    *  
    *  Send an IP datagram.  We only support non-fragmented datagrams
    *  at this time.
    *  
    *  Input parameters: 
    *  	   ipi - IP stack information
    *  	   buf - an ebuf
    *  	   destaddr - destination IP address
    *  	   proto - IP protocol number
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int _ip_send(ip_info_t *ipi,ebuf_t *buf,uint8_t *destaddr,uint8_t proto)
{
    uint16_t cksum;
    uint8_t masksrc[IP_ADDR_LEN];
    uint8_t maskdest[IP_ADDR_LEN];
    int pktlen;
    uint8_t *ptr;

    /* Move to the beginning of the IP hdeader */

    ebuf_seek(buf,-IPHDR_LENGTH);

    pktlen = ebuf_length(buf) + IPHDR_LENGTH;

    ipi->ip_id++;

    /* Insert the IP header */

    ebuf_put_u8(buf,IPHDR_VER_4 | IPHDR_LEN_20);
    ebuf_put_u8(buf,IPHDR_TOS_DEFAULT);
    ebuf_put_u16_be(buf,pktlen);
    ebuf_put_u16_be(buf,ipi->ip_id);
    ebuf_put_u16_be(buf,0);
    ebuf_put_u8(buf,IPHDR_TTL_DEFAULT);
    ebuf_put_u8(buf,proto);
    ebuf_put_u16_be(buf,0);		/* checksum */
    ebuf_put_bytes(buf,ipi->net_info.ip_addr,IP_ADDR_LEN);
    ebuf_put_bytes(buf,destaddr,IP_ADDR_LEN);

    /* adjust pointer and add in the header length */

    ebuf_prepend(buf,IPHDR_LENGTH);

    /* Checksum the header */

    ptr = ebuf_ptr(buf);
    cksum = ip_chksum(0,ptr,IPHDR_LENGTH);
    cksum = ~cksum;
    ptr[10] = (cksum >> 8) & 0xFF;
    ptr[11] = (cksum >> 0) & 0xFF;

    /* 
     * If sending to the IP broadcast address,
     * send to local broadcast.
     */

    if (ip_addrisbcast(destaddr)) {
	eth_send(buf,(uint8_t *) eth_broadcast);
	eth_free(buf);
	return 0;
	}

    /*
     * If the mask has not been set, don't try to
     * determine if we should use the gateway or not.
     */

    if (ip_addriszero(ipi->net_info.ip_netmask)) {
	return _arp_lookup_and_send(ipi,buf,destaddr);
	}

    /*
     * Compute (dest-addr & netmask)  and   (my-addr & netmask)
     */

    ip_mask(masksrc,destaddr,ipi->net_info.ip_netmask);
    ip_mask(maskdest,ipi->net_info.ip_addr,ipi->net_info.ip_netmask);

    /*
     * if destination and my address are on the same subnet,
     * send the packet directly.  Otherwise, send via
     * the gateway.
     */

    if (ip_compareaddr(masksrc,maskdest) == 0) {
	return _arp_lookup_and_send(ipi,buf,destaddr);
	}
    else {
	/* if no gw configured, drop packet */
	if (ip_addriszero(ipi->net_info.ip_gateway)) {
	    eth_free(buf);	/* silently drop */
	    return 0;
	    }
	else {
	    return _arp_lookup_and_send(ipi,buf,ipi->net_info.ip_gateway);
	    }
	}

}


/*  *********************************************************************
    *  ip_rx_callback(buf,ref)
    *  
    *  Receive callback for IP packets.  This routine is called
    *  by the Ethernet datalink.  We look up a suitable protocol
    *  handler and pass the packet off.
    *  
    *  Input parameters: 
    *  	   buf - ebuf we received
    *  	   ref - reference data from the ethernet datalink
    *  	   
    *  Return value:
    *  	   ETH_KEEP to keep the packet
    *  	   ETH_DROP to drop the packet
    ********************************************************************* */

static int ip_rx_callback(ebuf_t *buf,void *ref)
{
    ip_info_t *ipi = ref;
    uint8_t tmp;
    int hdrlen;
    uint8_t *hdr;
    uint16_t origchksum;
    uint16_t calcchksum;
    uint16_t length;
    uint16_t tmp16;
    uint8_t proto;
    uint8_t srcip[IP_ADDR_LEN];
    uint8_t dstip[IP_ADDR_LEN];
    ip_protodisp_t *pdisp;
    int res;
    int idx;

    hdr = ebuf_ptr(buf);		/* save current posn */

    ebuf_get_u8(buf,tmp);		/* version and header length */

    /*
     * Check IP version
     */

    if ((tmp & 0xF0) != IPHDR_VER_4) {
	goto drop;			/* not IPV4 */
	}
    hdrlen = (tmp & 0x0F) * 4;

    /*
     * Check header size
     */

    if (hdrlen < IPHDR_LENGTH) {
	goto drop;			/* header < 20 bytes */
	}

    /* 
     * Check the checksum
     */
    origchksum = ((uint16_t) hdr[10] << 8) | (uint16_t) hdr[11];
    hdr[10] = hdr[11] = 0;
    calcchksum = ~ip_chksum(0,hdr,hdrlen);
    if (calcchksum != origchksum) {
	goto drop;
	}

    /*
     * Okay, now go back and check other fields.
     */

    ebuf_skip(buf,1);			/* skip TOS field */

    ebuf_get_u16_be(buf,length);
    ebuf_skip(buf,2);			/* skip ID field */

    ebuf_get_u16_be(buf,tmp16);		/* get Fragment Offset field */

    /*
     * If the fragment offset field is nonzero, or the 
     * "more fragments" bit is set, then this is a packet
     * fragment.  Our trivial IP implementation does not
     * deal with fragments, so drop the packets.
     */
    if ((tmp16 & (IPHDR_FRAGOFFSET | IPHDR_MOREFRAGMENTS)) != 0) {
	goto drop;			/* packet is fragmented */
	}

    ebuf_skip(buf,1);			/* skip TTL */
    ebuf_get_u8(buf,proto);		/* get protocol */
    ebuf_skip(buf,2);			/* skip checksum */

    ebuf_get_bytes(buf,srcip,IP_ADDR_LEN);
    ebuf_get_bytes(buf,dstip,IP_ADDR_LEN);

    ebuf_skip(buf,hdrlen-IPHDR_LENGTH); /* skip rest of header */

    ebuf_setlength(buf,length-hdrlen);	/* set length to just data portion */

    /*
     * If our address is not set, let anybody in.  We need this to
     * properly pass up DHCP replies that get forwarde through routers.
     * Otherwise, only let in matching addresses or broadcasts.
     */
    
    if (!ip_addriszero(ipi->net_info.ip_addr)) {
	if ((ip_compareaddr(dstip,ipi->net_info.ip_addr) != 0) &&
	    !(ip_addrisbcast(dstip))) {
	    goto drop;			/* not for us */
	    }
	}

    /*
     * ebuf's pointer now starts at beginning of protocol data
     */

    /*
     * Find matching protocol dispatch
     */
     
    pdisp = ipi->ip_protocols;
    res = ETH_DROP;
    for (idx = 0; idx < IP_MAX_PROTOCOLS; idx++) {
	if (pdisp->cb && (pdisp->protocol == proto)) {
	    res = (*(pdisp->cb))(pdisp->ref,buf,dstip,srcip);
	    break;
	    }
	pdisp++;
	}


    return res;

drop:
    return ETH_DROP;
}


/*  *********************************************************************
    *  _ip_init(eth)
    *  
    *  Initialize the IP layer, attaching it to an underlying Ethernet
    *  datalink interface.
    *  
    *  Input parameters: 
    *  	   eth - Ethernet datalink information
    *  	   
    *  Return value:
    *  	   ip_info pointer (IP stack information) or NULL if error
    ********************************************************************* */

ip_info_t *_ip_init(ether_info_t *eth)
{
    ip_info_t *ipi;
    uint8_t ipproto[2];

    /*
     * Allocate IP stack info
     */

    ipi = KMALLOC(sizeof(ip_info_t),0);
    if (ipi == NULL) return NULL;

    memset(ipi,0,sizeof(ip_info_t));

    ipi->eth_info = eth;

    /*
     * Initialize ARP
     */

    if (_arp_init(ipi) < 0) {
	KFREE(ipi);
	return NULL;
	}

    /*
     * Open the Ethernet portal for IP packets
     */

    ipproto[0] = (PROTOSPACE_IP >> 8) & 0xFF; ipproto[1] = PROTOSPACE_IP & 0xFF;
    ipi->ip_port = eth_open(ipi->eth_info,ETH_PTYPE_DIX,(int8_t *)ipproto,ip_rx_callback,ipi);

    if (ipi->ip_port < 0) {
	_arp_uninit(ipi);
	KFREE(ipi);
	return NULL;
	}

    return ipi;
}


/*  *********************************************************************
    *  _ip_uninit(ipi)
    *  
    *  Un-initialize the IP layer, freeing resources
    *  
    *  Input parameters: 
    *  	   ipi - IP stack information
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void _ip_uninit(ip_info_t *ipi)
{
    /*
     * Close the IP portal
     */

    eth_close(ipi->eth_info,ipi->ip_port);

    /*
     * Turn off the ARP layer.
     */

    _arp_uninit(ipi);


    /*
     * free strings containing the domain and host names
     */

    if (ipi->net_info.ip_domain) {
	KFREE(ipi->net_info.ip_domain);
	}

    if (ipi->net_info.ip_hostname) {
	KFREE(ipi->net_info.ip_hostname);
	}

    /*
     * Free the stack information
     */

    KFREE(ipi);
}


/*  *********************************************************************
    *  _ip_timer_tick(ipi)
    *  
    *  Called once per second while the IP stack is active.
    *  
    *  Input parameters: 
    *  	   ipi - ip stack information
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */


void _ip_timer_tick(ip_info_t *ipi)
{
    _arp_timer_tick(ipi);
}


/*  *********************************************************************
    *  _ip_free(ipi,buf)
    *  
    *  Free an ebuf allocated via _ip_alloc
    *  
    *  Input parameters: 
    *  	   ipi - IP stack information
    *  	   buf - ebuf to free
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void _ip_free(ip_info_t *ipi,ebuf_t *buf)
{
    eth_free(buf);
}

/*  *********************************************************************
    *  _ip_getaddr(ipi,buf)
    *  
    *  Return our IP address (is this used?)
    *  
    *  Input parameters: 
    *  	   ipi - IP stack information
    *  	   buf - pointer to 4-byte buffer to receive IP address
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void _ip_getaddr(ip_info_t *ipi,uint8_t *buf)
{
    memcpy(buf,ipi->net_info.ip_addr,IP_ADDR_LEN);
}

/*  *********************************************************************
    *  _ip_getparam(ipi,param)
    *  
    *  Return the value of an IP parameter (address, netmask, etc.).
    *  The return value may need to be coerced if it's not normally
    *  a uint8_t* pointer.
    *  
    *  Input parameters: 
    *  	   ipi - IP stack information
    *  	   param - parameter number
    *  	   
    *  Return value:
    *  	   parameter value, or NULL if the parameter is invalid or 
    *  	   not set.
    ********************************************************************* */

uint8_t *_ip_getparam(ip_info_t *ipinfo,int param)
{
    uint8_t *ret = NULL;

    switch (param) {
	case NET_IPADDR:
	    ret = ipinfo->net_info.ip_addr;
	    break;
	case NET_NETMASK:
	    ret = ipinfo->net_info.ip_netmask;
	    break;
	case NET_GATEWAY:
	    ret = ipinfo->net_info.ip_gateway;
	    break;
	case NET_NAMESERVER:
	    ret = ipinfo->net_info.ip_nameserver;
	    break;
	case NET_HWADDR:
	    ret = ipinfo->arp_hwaddr;
	    break;
	case NET_DOMAIN:
	    ret = (uint8_t *)ipinfo->net_info.ip_domain;
	    break;
	case NET_HOSTNAME:
	    ret = (uint8_t *)ipinfo->net_info.ip_hostname;
	    break;
	case NET_SPEED:
	    return NULL;
	    break;
	case NET_LOOPBACK:
	    return NULL;
	    break;
	}

    return ret;
}

/*  *********************************************************************
    *  _ip_getparam(ipi,param,value)
    *  
    *  Set the value of an IP parameter (address, netmask, etc.).
    *  The value may need to be coerced if it's not normally
    *  a uint8_t* pointer.
    *  
    *  Input parameters: 
    *  	   ipi - IP stack information
    *  	   param - parameter number
    *	   value - parameter's new value
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

int _ip_setparam(ip_info_t *ipinfo,int param,uint8_t *ptr)
{
    int res = -1;

    switch (param) {
	case NET_IPADDR:
	    memcpy(ipinfo->net_info.ip_addr,ptr,IP_ADDR_LEN);
	    _arp_send_gratuitous(ipinfo);
	    res = 0;
	    break;
	case NET_NETMASK:
	    memcpy(ipinfo->net_info.ip_netmask,ptr,IP_ADDR_LEN);
	    res = 0;
	    break;
	case NET_GATEWAY:
	    memcpy(ipinfo->net_info.ip_gateway,ptr,IP_ADDR_LEN);
	    res = 0;
	    break;
	case NET_NAMESERVER:
	    memcpy(ipinfo->net_info.ip_nameserver,ptr,IP_ADDR_LEN);
	    res = 0;
	    break;
	case NET_DOMAIN:
	    if (ipinfo->net_info.ip_domain) {
		KFREE(ipinfo->net_info.ip_domain);
		ipinfo->net_info.ip_domain = NULL;
		}
	    if (ptr) ipinfo->net_info.ip_domain = strdup((char *) ptr);
	    break;
	case NET_HOSTNAME:
	    if (ipinfo->net_info.ip_hostname) {
		KFREE(ipinfo->net_info.ip_hostname);
		ipinfo->net_info.ip_hostname = NULL;
		}
	    if (ptr) ipinfo->net_info.ip_hostname = strdup((char *) ptr);
	    break;
	case NET_HWADDR:
	    memcpy(ipinfo->arp_hwaddr,ptr,ENET_ADDR_LEN);
	    eth_sethwaddr(ipinfo->eth_info,ptr);
	    res = 0;
	    break;
	case NET_SPEED:
	    res = eth_setspeed(ipinfo->eth_info,*(int *)ptr);
	    break;
	case NET_LOOPBACK:
	    res = eth_setloopback(ipinfo->eth_info,*(int *)ptr);
	    break;
	}

    return res;
}

/*  *********************************************************************
    *  _ip_register(ipinfo,proto,cb)
    *  
    *  Register a protocol handler with the IP layer.  IP client
    *  protocols such as UDP, ICMP, etc. call this to register their
    *  callbacks.
    *  
    *  Input parameters: 
    *  	   ipinfo - IP stack information
    *  	   proto - IP protocol number
    *  	   cb - callback routine to register
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void _ip_register(ip_info_t *ipinfo,
		  int proto,
		  int (*cb)(void *ref,ebuf_t *buf,uint8_t *dst,uint8_t *src),void *ref)
{
    int idx;

    for (idx = 0; idx < IP_MAX_PROTOCOLS; idx++) {
	if (ipinfo->ip_protocols[idx].cb == NULL) break;
	}

    if (idx == IP_MAX_PROTOCOLS) return;

    ipinfo->ip_protocols[idx].protocol = (uint8_t) proto;
    ipinfo->ip_protocols[idx].cb = cb;
    ipinfo->ip_protocols[idx].ref = ref;
}


/*  *********************************************************************
    *  _ip_deregister(ipinfo,proto)
    *  
    *  Deregister an IP protocol.  
    *  
    *  Input parameters: 
    *  	   ipinfo - IP stack information
    *  	   proto - protocol number
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void _ip_deregister(ip_info_t *ipinfo,int proto)
{
    int idx;

    for (idx = 0; idx < IP_MAX_PROTOCOLS; idx++) {
	if (ipinfo->ip_protocols[idx].protocol == (uint8_t) proto) {
	    ipinfo->ip_protocols[idx].protocol = 0;
	    ipinfo->ip_protocols[idx].ref = 0;
	    ipinfo->ip_protocols[idx].cb = NULL;
	    }
	}
}
