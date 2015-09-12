/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  User Datagram Protocol			File: net_udp.c	
    *  
    *  This module implements the User Datagram Protocol (RFCxxxx)
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

#include "cfe_error.h"

/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define UDP_HDR_LENGTH 8
#define UDP_PORTBASE 1024
#define UDP_MAX_PORTS 4

/*  *********************************************************************
    *  Types
    ********************************************************************* */

typedef struct udp_port_s udp_port_t;

/*
 * UDP port structure - describes an open UDP port.
 */

struct udp_port_s {
    uint16_t up_destport;		/* destination port number */
    uint16_t up_srcport;		/* source port number */
    queue_t up_rxqueue;			/* queue of received packets */
    int up_maxqueue;			/* max # of elements on rx queue */
    int up_inuse;			/* nonzero if port is in use */
};


/*
 * UDP stack information - describes the entire UDP layer.
 */

struct udp_info_s {
    uint16_t ui_portbase;
    void *ui_ref;
    ip_info_t *ui_ipinfo;
    udp_port_t ui_ports[UDP_MAX_PORTS];
};

/*  *********************************************************************
    *  Forward declarations
    ********************************************************************* */

static int _udp_rx_callback(void *ref,ebuf_t *buf,uint8_t *destaddr,uint8_t *srcaddr);


/*  *********************************************************************
    *  udp_find_port(info,port)
    *  
    *  Locate an open port.  Scan the list of ports looking for one
    *  that is both open and has a matching source port number.
    *  
    *  Input parameters: 
    *  	   info - UDP stack information
    *  	   port - source port # we're looking for
    *  	   
    *  Return value:
    *  	   udp_port_t pointer or NULL if no port was found
    ********************************************************************* */

static udp_port_t *_udp_find_port(udp_info_t *info,uint16_t port)
{
    int idx;
    udp_port_t *udp = info->ui_ports;

    for (idx = 0; idx < UDP_MAX_PORTS; idx++) {
	if (udp->up_inuse && (udp->up_srcport == port)) {
	    return udp;
	    }
	udp++;
	}

    return NULL;      
}

/*  *********************************************************************
    *  _udp_socket(info,port)
    *  
    *  Open a UDP socket.  This is an internal function used by
    *  the network API layer.
    *  
    *  Input parameters: 
    *  	   info - UDP stack information
    *  	   port - port number to open
    *  	   
    *  Return value:
    *  	   port number (0 based) or an error code (<0) if an error
    *  	   occured.
    ********************************************************************* */

int _udp_socket(udp_info_t *info,uint16_t port)
{
    extern int32_t _getticks(void);		/* return value of CP0 COUNT */
    int idx;
    udp_port_t *udp;
    uint16_t srcport = UDP_PORTBASE + (_getticks() & 0xFFF);

    while (_udp_find_port(info,srcport)) {	/* should always exit */
	srcport++;
	if (srcport > (UDP_PORTBASE+4096)) srcport = UDP_PORTBASE;
	}

    udp = info->ui_ports;

    for (idx = 0; idx < UDP_MAX_PORTS; idx++) {
	if (!udp->up_inuse) break;
	udp++;
	}

    if (idx == UDP_MAX_PORTS) return CFE_ERR_NOHANDLES;

    udp->up_destport = port;
    udp->up_srcport = srcport;
    udp->up_maxqueue = 2;
    udp->up_inuse = TRUE;
    q_init(&(udp->up_rxqueue));

    return idx;
}


/*  *********************************************************************
    *  _udp_close(info,s)
    *  
    *  Internal function to close an open UDP port.  This routine is 
    *  called by the high-level network API.
    *  	   
    *  Input parameters: 
    *  	   info - UDP stack information
    *  	   s - an open UDP socket handle (returned from _udp_open)
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void _udp_close(udp_info_t *info,int s)
{
    udp_port_t *udp = &(info->ui_ports[s]);
    ebuf_t *buf;

    while ((buf = (ebuf_t *) q_deqnext(&(udp->up_rxqueue)))) {
	_ip_free(info->ui_ipinfo,buf);
	}

    udp->up_srcport = 0;
    udp->up_destport = 0;
    udp->up_maxqueue = 0;
    udp->up_inuse = FALSE;
}

/*  *********************************************************************
    *  _udp_send(info,s,buf,dest)
    *  
    *  Transmit a UDP datagram.  Note that we never send fragmented
    *  datagrams, so all datagrams must be less than the MTU.
    *  
    *  Input parameters: 
    *  	   info - UDP stack information
    *  	   s - an open UDP socket handle (returned from _udp_open)
    *  	   buf - an ebuf to send
    *  	   dest - destination IP address
    *  	   
    *  Return value:
    *  	   0 if packet was sent
    *  	   else error code
    ********************************************************************* */

int _udp_send(udp_info_t *info,int s,ebuf_t *buf,uint8_t *dest)
{
    udp_port_t *udp = &(info->ui_ports[s]);
    uint8_t *udphdr;
    int udplen;
    uint8_t pseudoheader[12];
    uint16_t cksum;

    /*
     * Calculate the length of the IP datagram (includes UDP header)
     */

    udplen = ebuf_length(buf) + UDP_HDR_LENGTH;

    /*
     * Build the pseudoheader, which is part of the checksum calculation
     */

    _ip_getaddr(info->ui_ipinfo,&pseudoheader[0]);
    memcpy(&pseudoheader[4],dest,IP_ADDR_LEN);
    pseudoheader[8] = 0;
    pseudoheader[9] = IPPROTO_UDP;
    pseudoheader[10] = (udplen >> 8) & 0xFF;
    pseudoheader[11] = (udplen & 0xFF);

    /*
     * Back up and build the actual UDP header in the packet
     */

    ebuf_seek(buf,-UDP_HDR_LENGTH);
    udphdr = ebuf_ptr(buf);

    ebuf_put_u16_be(buf,udp->up_srcport);
    ebuf_put_u16_be(buf,udp->up_destport);
    ebuf_put_u16_be(buf,udplen);
    ebuf_put_u16_be(buf,0);

    ebuf_prepend(buf,UDP_HDR_LENGTH);

    /*
     * Checksum the packet and insert the checksum into the header 
     */

    cksum = ip_chksum(0,pseudoheader,sizeof(pseudoheader));
    cksum = ip_chksum(cksum,udphdr,udplen);
    cksum = ~cksum;
    if (cksum == 0) cksum = 0xFFFF;
    udphdr[6] = (cksum >> 8) & 0xFF;
    udphdr[7] = (cksum & 0xFF);

    /*
     * Off it goes!
     */

    _ip_send(info->ui_ipinfo,buf,dest,IPPROTO_UDP);

    return 0;    
}

/*  *********************************************************************
    *  _udp_bind(info,s,port)
    *  
    *  Bind a UDP socket to a particular port number.  Basically,
    *  all this means is we set the "source" port number.
    *  
    *  Input parameters: 
    *  	   info - UDP stack information
    *  	   s - an open UDP socket (from _udp_open)
    *  	   port - port number to assign to the UDP socket
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

int _udp_bind(udp_info_t *info,int s,uint16_t port)
{
    udp_port_t *udp = &(info->ui_ports[s]);

    if (_udp_find_port(info,port)) return CFE_ERR_ALREADYBOUND;

    udp->up_srcport = port;

    return 0;
}


/*  *********************************************************************
    *  _udp_connect(info,s,port)
    *  
    *  "connect" a UDP socket to a particular port number.  Basically,
    *  this just sets the "destination" port number.  It is used for
    *  protocols like TFTP where the destination port number changes
    *  after the port is open.
    *  
    *  Input parameters: 
    *  	   info - UDP stack information
    *  	   s - an open UDP socket (from _udp_open)
    *  	   port - port number to assign to the UDP socket
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int _udp_connect(udp_info_t *info,int s,uint16_t port)
{
    udp_port_t *udp = &(info->ui_ports[s]);

    udp->up_destport = port;

    return 0;
}

/*  *********************************************************************
    *  _udp_rx_callback(ref,buf,destaddr,srcaddr)
    *  
    *  Receive callback routine from the IP layer.  When an IP
    *  packet of protocol type "UDP" is received, this routine gets
    *  called.
    *  
    *  Input parameters: 
    *  	   ref - reference data (pointer to our UDP stack info)
    *  	   buf - the ebuf, currently pointing at the UDP header
    *  	   destaddr - the destination IP address (usually our IP address)
    *  	   srcaddr - the source IP address
    *  	   
    *  Return value:
    *  	   ETH_KEEP to keep (not deallocate) the packet
    *  	   ETH_DROP to deallocate the packet.
    ********************************************************************* */

static int _udp_rx_callback(void *ref,ebuf_t *buf,uint8_t *destaddr,uint8_t *srcaddr)
{
    uint8_t pseudoheader[12];
    int udplen;
    uint16_t calccksum;
    uint16_t origcksum;
    uint8_t *udphdr;
    uint16_t srcport;
    uint16_t dstport;
    uint16_t udplen2;
    udp_port_t *udp;
    udp_info_t *info = (udp_info_t *) ref;



    /*
     * get a pointer to the UDP header
     */

    udplen = ebuf_length(buf);
    udphdr = ebuf_ptr(buf);

    /* 
     * see if we are checking checksums  (cksum field != 0)
     */

    if ((udphdr[6] | udphdr[7]) != 0) {

	/* 
	 * construct the pseudoheader for the cksum calculation
	 */

	memcpy(&pseudoheader[0],srcaddr,IP_ADDR_LEN);
	memcpy(&pseudoheader[4],destaddr,IP_ADDR_LEN);
	pseudoheader[8] = 0;
	pseudoheader[9] = IPPROTO_UDP;
	pseudoheader[10] = (udplen >> 8) & 0xFF;
	pseudoheader[11] = (udplen & 0xFF);

	origcksum = ((uint16_t) udphdr[6] << 8) | (uint16_t) udphdr[7];
	udphdr[6] = udphdr[7] = 0;

	calccksum = ip_chksum(0,pseudoheader,sizeof(pseudoheader));
	calccksum = ip_chksum(calccksum,udphdr,udplen);
	if (calccksum != 0xffff) {
	    calccksum = ~calccksum;
	    }

	if (calccksum != origcksum) {
	    return ETH_DROP;
	    }
	}

    /* Read the other UDP header fields from the packet */

    ebuf_get_u16_be(buf,srcport);
    ebuf_get_u16_be(buf,dstport);
    ebuf_get_u16_be(buf,udplen2);
    ebuf_skip(buf,2);

    /*
     * It's no good if the lengths don't match.  The length
     * reported by IP should be the length in the UDP header + 8
     */

    if (udplen2 != (uint16_t) udplen) {
	return ETH_DROP;
	}

    /*
     * Okay, start looking for a matching port
     */

    udp = _udp_find_port(info,dstport);
    if (!udp) {
	return ETH_DROP;		/* drop packet if no matching port */
	}

    buf->eb_usrdata = (int) srcport;
    buf->eb_usrptr = srcaddr;

    /*
     * Drop packet if queue is full
     */

    if (q_count(&(udp->up_rxqueue)) >= udp->up_maxqueue) {
	return ETH_DROP;
	}

    /*
     * Add to receive queue
     */

    ebuf_setlength(buf,udplen2-UDP_HDR_LENGTH);
    q_enqueue(&(udp->up_rxqueue),(queue_t *) buf);

    return ETH_KEEP;
}


/*  *********************************************************************
    *  _udp_recv(info,s)
    *  
    *  Receive a packet from the specified UDP socket.
    *  
    *  Input parameters: 
    *  	   info - UDP stack information
    *  	   s - an open UDP socket handle (from _udp_open)
    *  	   
    *  Return value:
    *  	   an ebuf, or NULL if no packets have been received.
    ********************************************************************* */

ebuf_t *_udp_recv(udp_info_t *info,int s)
{
    ebuf_t *buf;
    udp_port_t *udp = &(info->ui_ports[s]);

    buf = (ebuf_t *) q_deqnext(&(udp->up_rxqueue));

    return buf;
}


/*  *********************************************************************
    *  _udp_init(ipi,ref)
    *  
    *  Initialize the UDP module.  This routine registers our
    *  protocol with the IP layer.
    *  
    *  Input parameters: 
    *  	   ipi - IP information (including our IP address, etc.)
    *  	   ref - reference data, stored in our UDP stack structure
    *  	   
    *  Return value:
    *  	   udp_info_t (allocated) or NULL if something went wrong.
    ********************************************************************* */

udp_info_t *_udp_init(ip_info_t *ipi,void *ref)
{
    udp_info_t *info;
    udp_port_t *udp;
    int idx;

    /*
     * Allocate some memory for our structure
     */

    info = KMALLOC(sizeof(udp_info_t),0);

    if (info == NULL) return NULL;

    memset(info,0,sizeof(udp_info_t));

    /*
     * Fill in the fields.
     */

    info->ui_ref = ref;
    info->ui_ipinfo = ipi;
    udp = info->ui_ports;
    for (idx = 0; idx < UDP_MAX_PORTS; idx++) {
	udp->up_inuse = FALSE;
	q_init(&(udp->up_rxqueue));
	udp++;
	}

    /*
     * Register our protocol with IP
     */

    _ip_register(ipi,IPPROTO_UDP,_udp_rx_callback,info);

    return info;
}


/*  *********************************************************************
    *  _udp_uninit(info)
    *  
    *  Uninitialize the UDP module, deregistering ourselves from the
    *  IP layer.
    *  
    *  Input parameters: 
    *  	   info - UDP stack information
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void _udp_uninit(udp_info_t *info)
{
    int idx;
    udp_port_t *udp;
    ebuf_t *buf;

    /*
     * Unregister from IP
     */

    _ip_deregister(info->ui_ipinfo,IPPROTO_UDP);

    /*
     * Free up any packets that were waiting
     */

    udp = info->ui_ports;
    for (idx = 0; idx < UDP_MAX_PORTS; idx++) {
	if (udp->up_inuse) {
	    while ((buf = (ebuf_t *) q_deqnext(&(udp->up_rxqueue)))) {
		_ip_free(info->ui_ipinfo,buf);
		}
	    }
	udp++;
	}

    /*
     * Free the stack info
     */

    KFREE(info);
}

/*  *********************************************************************
    *  _udp_alloc(info)
    *  
    *  Allocate a buffer for use with UDP.  This routine obtains an
    *  ebuf and adjusts its header to include room for the UDP
    *  header.
    *  
    *  Input parameters: 
    *  	   info - UDP stack information
    *  	   
    *  Return value:
    *  	   ebuf, or NULL if there are none left
    ********************************************************************* */

ebuf_t *_udp_alloc(udp_info_t *info)
{
    ebuf_t *buf;

    /*
     * Get an ebuf
     */

    buf = _ip_alloc(info->ui_ipinfo);

    if (!buf) return NULL;

    /*
     * make room for the udp header
     */

    ebuf_seek(buf,UDP_HDR_LENGTH);
    ebuf_setlength(buf,0);

    return buf;    
}

/*  *********************************************************************
    *  _udp_free(info,buf)
    *  
    *  Return an ebuf to the pool.
    *  
    *  Input parameters: 
    *  	   info - UDP stack info
    *  	   buf - an ebuf
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void _udp_free(udp_info_t *info,ebuf_t *buf)
{
    _ip_free(info->ui_ipinfo,buf);
}
