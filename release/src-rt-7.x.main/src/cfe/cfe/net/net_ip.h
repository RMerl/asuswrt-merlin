/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  IP Protocol Definitions			File: net_ip.h
    *  
    *  This is the main include file for the CFE network stack.
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



/*  *********************************************************************
    *  Global info
    ********************************************************************* */

#ifndef IP_ADDR_LEN
#define IP_ADDR_LEN 4
#endif

typedef struct ip_info_s ip_info_t;

typedef struct net_info_s {
    /* Configuration info for IP interface */
    uint8_t ip_addr[IP_ADDR_LEN];
    uint8_t ip_netmask[IP_ADDR_LEN];
    uint8_t ip_gateway[IP_ADDR_LEN];
    uint8_t ip_nameserver[IP_ADDR_LEN];
    char *ip_domain;
    char *ip_hostname;
} net_info_t;

/*  *********************************************************************
    *  ARP Information
    ********************************************************************* */

int _arp_lookup_and_send(ip_info_t *ipi,ebuf_t *buf,uint8_t *dest);
void _arp_timer_tick(ip_info_t *ipi);
int _arp_init(ip_info_t *ipi);
void _arp_uninit(ip_info_t *ipi);
void _arp_add(ip_info_t *ipi,uint8_t *destip,uint8_t *desthw);
void _arp_send_gratuitous(ip_info_t *ipi);
uint8_t *_arp_lookup(ip_info_t *ipi,uint8_t *destip);
int _arp_enumerate(ip_info_t *ipi,int entrynum,uint8_t *ipaddr,uint8_t *hwaddr);
int _arp_delete(ip_info_t *ipi,uint8_t *ipaddr);


/*  *********************************************************************
    *  IP protocol
    ********************************************************************* */


#define IPPROTO_TCP	6
#define IPPROTO_UDP	17
#define IPPROTO_ICMP	1

int _ip_send(ip_info_t *ipi,ebuf_t *buf,uint8_t *destaddr,uint8_t proto);
ip_info_t *_ip_init(ether_info_t *);
void _ip_timer_tick(ip_info_t *ipi);
void _ip_uninit(ip_info_t *ipi);
ebuf_t *_ip_alloc(ip_info_t *ipi);
void _ip_free(ip_info_t *ipi,ebuf_t *buf);
void _ip_getaddr(ip_info_t *ipi,uint8_t *buf);
uint8_t *_ip_getparam(ip_info_t *ipinfo,int param);
int _ip_setparam(ip_info_t *ipinfo,int param,uint8_t *ptr);
uint16_t ip_chksum(uint16_t initcksum,uint8_t *ptr,int len);
void _ip_deregister(ip_info_t *ipinfo,int proto);
void _ip_register(ip_info_t *ipinfo,
		  int proto,
		  int (*cb)(void *ref,ebuf_t *buf,uint8_t *dst,uint8_t *src),void *ref);


#define ip_mask(dest,a,b) (dest)[0] = (a)[0] & (b)[0] ;  \
                          (dest)[1] = (a)[1] & (b)[1] ; \
                          (dest)[2] = (a)[2] & (b)[2] ; \
                          (dest)[3] = (a)[3] & (b)[3] 

#define ip_compareaddr(a,b) memcmp(a,b,IP_ADDR_LEN)

#define ip_addriszero(a) (((a)[0]|(a)[1]|(a)[2]|(a)[3]) == 0)
#define ip_addrisbcast(a) ((a[0] == 0xFF) && (a[1] == 0xFF) && (a[2] == 0xFF) && (a[3] == 0xFF))

#ifndef NET_IPADDR
#define NET_IPADDR	0
#define NET_NETMASK	1
#define NET_GATEWAY	2
#define NET_NAMESERVER	3
#define NET_HWADDR	4
#define NET_DOMAIN	5
#define NET_HOSTNAME	6
#define NET_SPEED	7
#define NET_LOOPBACK	8
#endif

/*  *********************************************************************
    *  UDP Protocol
    ********************************************************************* */

typedef struct udp_info_s udp_info_t;

int _udp_socket(udp_info_t *info,uint16_t port);
void _udp_close(udp_info_t *info,int s);
int _udp_send(udp_info_t *info,int s,ebuf_t *buf,uint8_t *dest);
udp_info_t *_udp_init(ip_info_t *ipi,void *ref);
void _udp_uninit(udp_info_t *info);
int _udp_bind(udp_info_t *info,int s,uint16_t port);
int _udp_connect(udp_info_t *info,int s,uint16_t port);
ebuf_t *_udp_recv(udp_info_t *info,int s);
ebuf_t *_udp_alloc(udp_info_t *info);
void _udp_free(udp_info_t *info,ebuf_t *buf);

/*  *********************************************************************
    *  ICMP protocol
    ********************************************************************* */

typedef struct icmp_info_s icmp_info_t;

icmp_info_t *_icmp_init(ip_info_t *ipi);
void _icmp_uninit(icmp_info_t *icmp);

int _icmp_ping(icmp_info_t *icmp,uint8_t *ipaddr,int seq,int len);
