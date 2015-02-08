/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  High-level network API defs		File: net_api.h
    *  
    *  This module contains prototypes and constants for the 
    *  network (TCP/IP) interface.
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
    *  Constants
    ********************************************************************* */

#ifndef IP_ADDR_LEN
#define IP_ADDR_LEN 4
#endif

/*  *********************************************************************
    *  DHCP Protocol
    ********************************************************************* */

typedef struct dhcpreply_s {
    uint8_t dr_ipaddr[IP_ADDR_LEN];
    uint8_t dr_netmask[IP_ADDR_LEN];
    uint8_t dr_gateway[IP_ADDR_LEN];
    uint8_t dr_nameserver[IP_ADDR_LEN];
    uint8_t dr_dhcpserver[IP_ADDR_LEN];
    uint8_t dr_bootserver[IP_ADDR_LEN];
    char *dr_hostname;
    char *dr_domainname;
    char *dr_bootfile;
    char *dr_rootpath;
    char *dr_swapserver;
    char *dr_script;
    char *dr_options;
} dhcpreply_t;


int dhcp_bootrequest(dhcpreply_t **reply);
void dhcp_free_reply(dhcpreply_t *reply);
void dhcp_set_envvars(dhcpreply_t *reply);

/*  *********************************************************************
    *  IP Layer
    ********************************************************************* */

void ip_uninit(void);
int ip_init(char *,uint8_t *);
ebuf_t *ip_alloc(void);
void ip_free(ebuf_t *buf);
int ip_send(ebuf_t *buf,uint8_t *destaddr,uint8_t proto);
uint16_t ip_chksum(uint16_t initchksum,uint8_t *ptr,int len);

/*  *********************************************************************
    *  UDP Layer
    ********************************************************************* */

ebuf_t *udp_alloc(void);
void udp_free(ebuf_t *buf);

int udp_socket(uint16_t port);
int udp_bind(int portnum,uint16_t port);
int udp_connect(int portnum,uint16_t port);
void udp_close(int portnum);
int udp_send(int portnum,ebuf_t *buf,uint8_t *dest);
ebuf_t *udp_recv_with_timeout(int portnum,int seconds);
ebuf_t *udp_recv(int portnum);

/*  *********************************************************************
    *  TCP Layer
    ********************************************************************* */

#if CFG_TCP  
#ifndef TCPFLG_NODELAY
#define TCPFLG_NODELAY	1		/* disable nagle */
#define TCPFLG_NBIO	2		/* non-blocking I/O */

#define TCPSTATUS_NOTCONN	0
#define TCPSTATUS_CONNECTING	1
#define TCPSTATUS_CONNECTED	2
#endif
int tcp_socket(void);
void tcp_destroy(int portnum);
int tcp_connect(int s,uint8_t *dest,uint16_t port);
int tcp_close(int s);
int tcp_send(int s,uint8_t *buf,int len);
int tcp_recv(int s,uint8_t *buf,int len);
int tcp_bind(int s,uint16_t port);
int tcp_peeraddr(int s,uint8_t *addr,uint16_t *port);
int tcp_listen(int s,uint16_t port);
int tcp_status(int s,int *connflag,int *rxready,int *rxeof);
int tcp_debug(int s,int arg);
int tcp_setflags(int s,unsigned int flags);
int tcp_getflags(int s,unsigned int *flags);
#endif

/*  *********************************************************************
    *  ARP Layer
    ********************************************************************* */

uint8_t *arp_lookup(uint8_t *destip);
void arp_add(uint8_t *destip,uint8_t *desthw);
int arp_enumerate(int entrynum,uint8_t *ipaddr,uint8_t *hwaddr);
int arp_delete(uint8_t *ipaddr);

/*  *********************************************************************
    *  Network Configuration
    ********************************************************************* */

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
#define NET_DEVNAME	10

uint8_t *net_getparam(int param);
int net_setparam(int param,uint8_t *ptr);
int net_init(char *devname);
void net_uninit(void);
void net_setnetvars(void);

/*  *********************************************************************
    *  DNS
    ********************************************************************* */

int dns_lookup(char *hostname,uint8_t *ipaddr);

/*  *********************************************************************
    *  ICMP
    ********************************************************************* */

int icmp_ping(uint8_t *dest,int seq,int len);

/*  *********************************************************************
    *  TFTP
    ********************************************************************* */

extern int tftp_max_retries;
extern int tftp_rrq_timeout;
extern int tftp_recv_timeout;
