/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Top-level API to network			File: net_api.c
    *  
    *  This routine contains the highest-level API to the network 
    *  routines.  The global handle to the network state is right here.
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


#include "bsp_config.h"

#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_devfuncs.h"
#include "cfe_ioctl.h"
#include "cfe_timer.h"

#include "cfe_error.h"

#include "net_ebuf.h"
#include "net_ether.h"

#include "cfe_timer.h"

#include "net_ip.h"
#include "net_ip_internal.h"
#include "net_api.h"

#include "env_subr.h"

#if CFG_TCP
#include "net_tcp.h"
#endif

#if CFG_HTTP
#include "net_http.h"
#endif

/*  *********************************************************************
    *  Structures
    ********************************************************************* */

/*
 * Net context.  All the soft context structures of all the
 * layers of the network stack are bundled here.  There's only one 
 * of these in the system when the network is active.
 */

typedef struct net_ctx_s {
    /* Global info */
    int64_t timer;

    /* device name */
    char *devname;

    /* Run-time info for IP interface */
    ip_info_t *ipinfo;

    /* Info for Ethernet interface */
    ether_info_t *ethinfo;

    /* Info specific to UDP */
    udp_info_t *udpinfo;

    /* Info specific to ICMP */
    icmp_info_t *icmpinfo;

#if CFG_TCP
    /* Info specific to TCP */
    tcp_info_t *tcpinfo;
#endif
#if CFG_HTTP
    /* Info specific to HTTP */
    http_info_t *httpinfo;
#endif
} net_ctx_t;


/*  *********************************************************************
    *  Globals
    ********************************************************************* */

static net_ctx_t *netctx = NULL;


/*  *********************************************************************
    *  UDP INTERFACE
    ********************************************************************* */

/*  *********************************************************************
    *  udp_alloc()
    *  
    *  Allocate an ebuf with fields reserved for the UDP layer.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   pointer to ebuf, or NULL if no EBUFs are available
    ********************************************************************* */

ebuf_t *udp_alloc(void)
{
    if (!netctx) return NULL;
    return _udp_alloc(netctx->udpinfo);
}

/*  *********************************************************************
    *  udp_free(buf)
    *  
    *  Return an ebuf to the pool.  The ebuf was presumably allocated
    *  via udp_alloc() first.
    *  
    *  Input parameters: 
    *  	   buf - ebuf to return to the pool
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
void udp_free(ebuf_t *buf)
{
    if (!netctx) return;
    _udp_free(netctx->udpinfo,buf);
}

/*  *********************************************************************
    *  udp_socket(port)
    *  
    *  Open a UDP socket.  Once open, datagrams sent on the socket will
    *  go to the specified port number.  You can change the port later
    *  using the "udp_connect" function.
    *  
    *  Input parameters: 
    *  	   port - port number
    *  	   
    *  Return value:
    *  	   UDP port handle, or -1 if no ports are available.
    ********************************************************************* */

int udp_socket(uint16_t port)
{
    if (!netctx) return -1;

    return _udp_socket(netctx->udpinfo,port);
}

/*  *********************************************************************
    *  udp_close(sock)
    *  
    *  Close a udp socket.  You pass this handle returned from a previous
    *  call to udp_open.
    *  
    *  Input parameters: 
    *  	   handle - UDP port handle, from udp_open()
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void udp_close(int portnum)
{
    if (!netctx) return;

    _udp_close(netctx->udpinfo,portnum);
}


/*  *********************************************************************
    *  udp_send(s,buf,dest)
    *  
    *  Send a datagram to the specified destination address.  The
    *  source and destination UDP port numbers are taken from the 
    *  values passed to earlier calls to udp_open, udp_bind, and
    *  udp_connect.
    *  
    *  Input parameters: 
    *  	   s - socket handle, from udp_open
    *  	   buf - ebuf to send (allocated via udp_alloc)
    *  	   dest - pointer to 4-byte destination IP address
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   <0 if an error occured.
    ********************************************************************* */

int udp_send(int s,ebuf_t *buf,uint8_t *dest)
{
    if (!netctx) return -1;

    return _udp_send(netctx->udpinfo,s,buf,dest);
}

/*  *********************************************************************
    *  udp_bind(s,port)
    *  
    *  Re-"bind" the specified udp socket to a new source port.
    *  This changes the source port number that will be transmitted
    *  in subsequent calls to udp_send()
    *  
    *  Input parameters: 
    *  	   s - socket handle
    *  	   port - new port number
    *
    *  Return value:
    *      0 if ok, else error code
    ********************************************************************* */

int udp_bind(int s,uint16_t port)
{
    if (!netctx) return -1;

    return _udp_bind(netctx->udpinfo,s,port);
}


/*  *********************************************************************
    *  udp_connect(s,port)
    *  
    *  Set the port number to be used in the destination port field
    *  for subsequent calls to udp_send().
    *  
    *  Input parameters: 
    *  	   s - udp socket handle
    *  	   port - new destination port number
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

int udp_connect(int s,uint16_t port)
{
    if (!netctx) return -1;

    return _udp_connect(netctx->udpinfo,s,port);
}

/*  *********************************************************************
    *  udp_recv(s)
    *  
    *  Return the next packet from the receive queue for this port.
    *  If no packets are available, NULL is returned.
    *  
    *  Input parameters: 
    *  	   s - udp port handle
    *  	   
    *  Return value:
    *  	   ebuf (if a packet is available)
    *  	   NULL (no packet available)
    ********************************************************************* */

ebuf_t *udp_recv(int s)
{
    if (!netctx) return NULL;

    return _udp_recv(netctx->udpinfo,s);
}


/*  *********************************************************************
    *  udp_recv_with_timeout(s,ticks)
    *  
    *  Return the next packet from the receive queue for this socket,
    *  waiting for one to arrive if there are none available.
    *  
    *  Input parameters: 
    *  	   s - udp socket handle
    *  	   ticks - number of ticks to wait
    *  	   
    *  Return value:
    *  	   ebuf (if a packet is available)
    *  	   NULL (no packet available after timeout)
    ********************************************************************* */

ebuf_t *udp_recv_with_timeout(int s,int ticks)
{
    ebuf_t *buf = NULL;
    int64_t timer;

    if (!netctx) return NULL;

    TIMER_SET(timer,ticks);

    while (!TIMER_EXPIRED(timer)) {
	POLL();
	buf = _udp_recv(netctx->udpinfo,s);
	if (buf) break;
	}

    return buf;
}



#if CFG_TCP
/*  *********************************************************************
    *  TCP INTERFACE
    ********************************************************************* */


/*  *********************************************************************
    *  tcp_socket()
    *  
    *  Create a new TCP port.
    *  
    *  Input parameters: 
    *  	   nothing.
    *  	   
    *  Return value:
    *  	   TCP port handle, or <0 if no ports are available.
    ********************************************************************* */

int tcp_socket(void)
{
    if (!netctx) return -1;

    return _tcp_socket(netctx->tcpinfo);
}

/*  *********************************************************************
    *  tcp_connect(handle,dest,port)
    *  
    *  Connect to a remote TCP destination.
    *  
    *  Input parameters: 
    *  	   handle - returned from tcp_create
    *  	   dest - destination IP address
    *  	   port - destination port number
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int tcp_connect(int s,uint8_t *dest,uint16_t port)
{
    int res;
    unsigned int flags;
    int connflag;

    if (!netctx) return -1;

    /* 
     * Get socket's blocking status 
     * If nonblocking, just call the tcp stack
     * and return what it returns.
     */

    res = _tcp_getflags(netctx->tcpinfo,s,&flags);	
    if (res < 0) return res;

    if (flags & TCPFLG_NBIO) {
	return _tcp_connect(netctx->tcpinfo,s,dest,port);
	}

    /*
     * Otherwise, call connect and poll till the status
     * changes.  We want to see a transition to the
     * CONNECTED state, so we loop while we see "CONNECTING"
     * and return a status based on what it changes to.
     */

    res = _tcp_connect(netctx->tcpinfo,s,dest,port);    
    if (res < 0) return res;
    connflag = TCPSTATUS_NOTCONN;

    for (;;) {
	POLL();

	res = _tcp_status(netctx->tcpinfo,s,&connflag,NULL,NULL);
	if (res < 0) break;

	if (connflag == TCPSTATUS_CONNECTING) continue;
	break;
	}

    if (connflag != TCPSTATUS_CONNECTED) return CFE_ERR_NOTCONN;

    return res;
}

/*  *********************************************************************
    *  tcp_close(s)
    *  
    *  Disconnect a connection (cleanly)
    *  
    *  Input parameters: 
    *  	   s - handle from tcp_create
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error
    ********************************************************************* */

int tcp_close(int s)
{
    if (!netctx) return -1;

    return _tcp_close(netctx->tcpinfo,s);
}



/*  *********************************************************************
    *  tcp_send(s,buf,len)
    *  
    *  Send a buffer to the other TCP, buffering as much data as
    *  will fit in the send buffer.
    *  
    *  Input parameters: 
    *  	   s - port handle, from tcp_open
    *  	   buf - buffer pointer
    *  	   len - length of buffer to send
    *  	   
    *  Return value:
    *  	   >=0 if ok (number of bytes sent)
    *  	   <0 if an error occured.
    ********************************************************************* */

int tcp_send(int s,uint8_t *buf,int len)
{
    int flags;
    int res;
    int total = 0;

    if (!netctx) return -1;

    /* 
     * Get socket's blocking status 
     * If nonblocking, just call the tcp stack
     * and return what it returns.
     */

    res = _tcp_getflags(netctx->tcpinfo,s,&flags);	
    if (res < 0) return res;

    if (flags & TCPFLG_NBIO) {
	return _tcp_send(netctx->tcpinfo,s,buf,len);
	}

    /* 
     * The first time we'll check the return code for an
     * error so we can pass up the failure.	
     */

    res = _tcp_send(netctx->tcpinfo,s,buf,len);
    if (res < 0) return res;

    buf += res;
    len -= res;
    total += res;

    while (len > 0) {
	/*
	 * Give the TCP stack and devices a chance to run
	 */

	POLL();

	/*
	 * Try to send some more.  If we get an error, get out.
	 * otherwise, keep going till all the data is gone.
	 */

	res = _tcp_send(netctx->tcpinfo,s,buf,len);
	if (res < 0) break;
	buf += res;
	len -= res;
	total += res;
	}

    /*
     * If we sent nothing and have an error, return the error.
     * Otherwise return the amount of data we sent.
     */
    if ((total == 0) && (res < 0)) return res;
    else return total;
}

/*  *********************************************************************
    *  tcp_recv(s,buf,len)
    *  
    *  Receive data from the remote TCP session
    *  
    *  Input parameters: 
    *  	   s - port handle, from tcp_open
    *  	   buf - buffer pointer
    *  	   len - length of buffer to send
    *  	   
    *  Return value:
    *  	   >=0 if ok (number of bytes received)
    *  	   <0 if an error occured.
    ********************************************************************* */

int tcp_recv(int s,uint8_t *buf,int len)
{
    int flags;
    int res;
    int total = 0;

    if (!netctx) return -1;

    /* 
     * Get socket's blocking status 
     * If nonblocking, just call the tcp stack
     * and return what it returns.
     */

    res = _tcp_getflags(netctx->tcpinfo,s,&flags);	
    if (res < 0) return res;

    if (flags & TCPFLG_NBIO) {
	return _tcp_recv(netctx->tcpinfo,s,buf,len);
	}

    /* 
     * The first time we'll check the return code for an
     * error so we can pass up the failure.	
     */

    res = _tcp_recv(netctx->tcpinfo,s,buf,len);
    if (res < 0) return res;

    buf += res;
    len -= res;
    total += res;

    while (len > 0) {
	/*
	 * Give the TCP stack and devices a chance to run
	 */

	POLL();

	/*
	 * Try to receive some more.  If we get an error, get out.
	 * otherwise, keep going till all the data is gone.
	 */

	res = _tcp_recv(netctx->tcpinfo,s,buf,len);
	if (res < 0) break;

	if (res == 0) {
	    _tcp_status(netctx->tcpinfo,s,&flags,NULL,NULL);
	    if (flags != TCPSTATUS_CONNECTED) {
		res = CFE_ERR_NOTCONN;
		break;
		}
	    }

	buf += res;
	len -= res;
	total += res;
	}

    /*
     * If we sent received and have an error, return the error.
     * Otherwise return the amount of data we sent.
     */
    if ((total == 0) && (res < 0)) return res;
    else return total;

}

/*  *********************************************************************
    *  tcp_bind(s,port)
    *  
    *  Re-"bind" the specified tcp port handle to a new source port.
    * 
    *  Used for listening sockets.
    *
    *  Input parameters: 
    *  	   s - port handle
    *  	   port - new port number
    *
    *  Return value:
    *      0 if ok, else error code
    ********************************************************************* */

int tcp_bind(int s,uint16_t port)
{
    if (!netctx) return -1;

    return _tcp_bind(netctx->tcpinfo,s,port);
}

/*  *********************************************************************
    *  tcp_peeraddr(s,addr,port)
    *  
    *  Return the address of the remote peer.
    *
    *  Input parameters: 
    *  	   s - port handle
    *      addr - points to 4-byte buffer to receive IP address
    *  	   port - points to uint16 to receive port number
    *
    *  Return value:
    *      0 if ok, else error code
    ********************************************************************* */

int tcp_peeraddr(int s,uint8_t *addr,uint16_t *port)
{
    if (!netctx) return -1;

    return _tcp_peeraddr(netctx->tcpinfo,s,addr,port);
}

/*  *********************************************************************
    *  tcp_setflags(s,addr,flags)
    *  
    *  Set per-socket flags (nodelay, etc.)
    *
    *  Input parameters: 
    *  	   s - port handle
    *      flags - flags for this socket
    *
    *  Return value:
    *      0 if ok, else error code
    ********************************************************************* */

int tcp_setflags(int s,unsigned int flags)
{
    if (!netctx) return -1;

    return _tcp_setflags(netctx->tcpinfo,s,flags);
}

/*  *********************************************************************
    *  tcp_getflags(s,addr,flags)
    *  
    *  Get per-socket flags (nodelay, etc.)
    *
    *  Input parameters: 
    *  	   s - port handle
    *      flags - flags for this socket
    *
    *  Return value:
    *      0 if ok, else error code
    ********************************************************************* */

int tcp_getflags(int s,unsigned int *flags)
{
    if (!netctx) return -1;

    return _tcp_getflags(netctx->tcpinfo,s,flags);
}


/*  *********************************************************************
    *  tcp_listen(s)
    *  
    *  Set the socket into "listen" mode.
    *  
    *  Input parameters: 
    *  	   s - port handle
    *	   port - port # to listen on
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error
    ********************************************************************* */

int tcp_listen(int s,uint16_t port)
{
    if (!netctx) return -1;

    return _tcp_listen(netctx->tcpinfo,s,port);
}

/*  *********************************************************************
    *  tcp_status(s,connflag,rxready,rxeof)
    *  
    *  Return the TCP connection's status
    *  
    *  Input parameters: 
    *  	   s - port handle
    *      connflag - points to flag to receive connected status
    *      rxready - returns # of bytes ready to receive
    *	   rxeof - returns TRUE if we've been FINed.
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error
    ********************************************************************* */

int tcp_status(int s,int *connflag,int *rxready,int *rxeof)
{
    if (!netctx) return -1;

    return _tcp_status(netctx->tcpinfo,s,connflag,rxready,rxeof);
}

/*  *********************************************************************
    *  tcp_debug(s,arg)
    *  
    *  Call the debug routine in the tcp stack.
    *  
    *  Input parameters: 
    *  	   s - socket handle
    *  	   arg - passed to debug routine
    *  	   
    *  Return value:
    *  	   return value from debug routine
    ********************************************************************* */

int tcp_debug(int s,int arg)
{
    if (!netctx) return -1;
    return _tcp_debug(netctx->tcpinfo,s,arg);
}

#endif

/*  *********************************************************************
    *  ARP FUNCTIONS
    ********************************************************************* */


/*  *********************************************************************
    *  arp_add(destip,desthw)
    *  
    *  Add a permanent entry to the ARP table.  This entry will
    *  persist until deleted or the interface is deactivated.
    *  This may cause a stale entry to be deleted if the table is full
    *  
    *  Input parameters: 
    *  	   destip - pointer to 4-byte destination IP address
    *  	   desthw - pointer to 6-byte destination hardware address
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void arp_add(uint8_t *destip,uint8_t *desthw)
{
    if (netctx) _arp_add(netctx->ipinfo,destip,desthw);
}

/*  *********************************************************************
    *  arp_lookup(destip)
    *  
    *  Look up the hardware address for an IP address.
    *  
    *  Input parameters: 
    *  	   destip - pointer to 4-byte IP address
    *  	   
    *  Return value:
    *  	   pointer to 6-byte hardware address, or NULL if there are
    *  	   no matching entries in the table.
    ********************************************************************* */

uint8_t *arp_lookup(uint8_t *destip)
{
    if (!netctx) return NULL;
    return _arp_lookup(netctx->ipinfo,destip);
}


/*  *********************************************************************
    *  arp_enumerate(entrynum,ipaddr,hwaddr)
    *  
    *  Return an entry from the ARP table.
    *  
    *  Input parameters: 
    *  	   entrynum - entry number to return, starting with zero
    *  	   ipaddr - pointer to 4 bytes to receive IP address
    *  	   hwaddr - pointer to 6 bytes to receive hardware address
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int arp_enumerate(int entrynum,uint8_t *ipaddr,uint8_t *hwaddr)
{
    if (!netctx) return -1;
    return _arp_enumerate(netctx->ipinfo,entrynum,ipaddr,hwaddr);
}

/*  *********************************************************************
    *  arp_delete(ipaddr)
    *  
    *  Delete an entry from the ARP table.
    *  
    *  Input parameters: 
    *  	   ipaddr - pointer to 4-byte IP address
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int arp_delete(uint8_t *ipaddr)
{
    if (!netctx) return -1;
    return _arp_delete(netctx->ipinfo,ipaddr);
}

/*  *********************************************************************
    *  ICMP FUNCTIONS
    ********************************************************************* */

/*  *********************************************************************
    *  icmp_ping(dest,seq,len)
    *  
    *  Ping a remote host, transmitting the ICMP_ECHO message and
    *  waiting for the corresponding ICMP_ECHO_REPLY.
    *  
    *  Input parameters: 
    *  	   dest - pointer to 4-byte destination IP address
    *  	   seq - sequence number to put in to the ICMP packet
    *  	   len - length of data to place in ICMP packet
    *  	   
    *  Return value:
    *  	   0 if ok (remote host responded)
    *  	   else error code
    ********************************************************************* */

int icmp_ping(uint8_t *dest,int seq,int len)
{
    if (!netctx) return -1;
    return _icmp_ping(netctx->icmpinfo,dest,seq,len);
}

/*  *********************************************************************
    *  INIT/CONFIG FUNCTIONS
    ********************************************************************* */

/*  *********************************************************************
    *  net_getparam(param)
    *  
    *  Return a parameter from the current IP configuration.  This is
    *  the main call to set the IP address, netmask, gateway, 
    *  name server, host name, etc.
    *  
    *  Input parameters: 
    *  	   param - parameter number (see net_api.h)
    *  	   
    *  Return value:
    *  	   pointer to value of parameter, or NULL if parameter
    *  	   ID is invalid
    ********************************************************************* */

uint8_t *net_getparam(int param)
{
    if (!netctx) return NULL;
    if (param == NET_DEVNAME) return (uint8_t *) netctx->devname;
    return _ip_getparam(netctx->ipinfo,param);

}

/*  *********************************************************************
    *  net_setparam(param,ptr)
    *  
    *  Set the value of an IP configuration parameter
    *  
    *  Input parameters: 
    *  	   param - parameter number (see net_api.h)
    *  	   ptr - pointer to parameter's new value
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int net_setparam(int param,uint8_t *ptr)
{
    if (!netctx) return NULL;
    return _ip_setparam(netctx->ipinfo,param,ptr);
}

/*  *********************************************************************
    *  net_poll()
    *  
    *  Process background tasks for the network stack, maintaining
    *  the ARP table, receive queues, etc.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void net_poll(void *arg)
{
    if (netctx) {
	eth_poll(netctx->ethinfo);
	if (TIMER_EXPIRED(netctx->timer)) {
	    _ip_timer_tick(netctx->ipinfo);
#if CFG_SIM
	    TIMER_SET(netctx->timer,CFE_HZ/10);
#else
	    TIMER_SET(netctx->timer,CFE_HZ);
#endif /* CFG_SIM */
	    }
	}
}

/*  *********************************************************************
    *  net_init(devname)
    *  
    *  Initialize the network interface.  This is the main call, once
    *  completed you should call net_setparam to set up the network
    *  addresses and stuff.
    *  
    *  Input parameters: 
    *  	   devname - CFE device name for network device
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int net_init(char *devname)
{
    net_ctx_t *ctx;

    if (netctx) net_uninit();

    ctx = KMALLOC(sizeof(net_ctx_t),0);

    if (!ctx) return -1;

    ctx->devname = strdup(devname);

    ctx->ethinfo = eth_init(devname);
    if (ctx->ethinfo == NULL) {
	return -1;
	}

    ctx->ipinfo = _ip_init(ctx->ethinfo);
    if (ctx->ipinfo == NULL) {
	eth_uninit(ctx->ethinfo);
	return -1;
	}

    ctx->udpinfo = _udp_init(ctx->ipinfo,ctx->ipinfo);
    if (ctx->udpinfo == NULL) {
	_ip_uninit(ctx->ipinfo);
	eth_uninit(ctx->ethinfo);
	return -1;
	}
	
    ctx->icmpinfo = _icmp_init(ctx->ipinfo);
    if (ctx->icmpinfo == NULL) {
	_udp_uninit(ctx->udpinfo);
	_ip_uninit(ctx->ipinfo);
	eth_uninit(ctx->ethinfo);
	return -1;
	}

    cfe_bg_add(net_poll,ctx);
#if CFG_SIM
    TIMER_SET(ctx->timer,CFE_HZ/10);
#else
    TIMER_SET(ctx->timer,CFE_HZ);
#endif /* CFG_SIM */

#if CFG_TCP
    ctx->tcpinfo = _tcp_init(ctx->ipinfo,ctx->ipinfo);
    cfe_bg_add(_tcp_poll,ctx->tcpinfo);
#endif

#if CFG_HTTP
    ctx->httpinfo = _tcphttp_init(ctx->ipinfo,ctx->ipinfo);
#endif
    netctx = ctx;

    return 0;
}


/*  *********************************************************************
    *  net_uninit()
    *  
    *  Uninitialize the network, deallocating all resources allocated
    *  to the network and closing all open device handles
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void net_uninit(void)
{
    if (netctx) {
#if CFG_TCP
	cfe_bg_remove(_tcp_poll);
	_tcp_uninit(netctx->tcpinfo);
#endif
#if CFG_HTTP
	_tcphttp_uninit(netctx->httpinfo);
#endif
	TIMER_CLEAR(netctx->timer);
	_icmp_uninit(netctx->icmpinfo);
	_udp_uninit(netctx->udpinfo);
	_ip_uninit(netctx->ipinfo);
	eth_uninit(netctx->ethinfo);
	KFREE(netctx->devname);
	KFREE(netctx);
	netctx = NULL;
	cfe_bg_remove(net_poll);
	}
}


/*  *********************************************************************
    *  net_setnetvars()
    *  
    *  Set environment variables related to the network.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void net_setnetvars(void)
{
    char *x;
    uint8_t *addr;
    char str[60];

    /* Clear out all the environment variables */
    env_delenv("NET_DEVICE");
    env_delenv("NET_IPADDR");
    env_delenv("NET_NETMASK");
    env_delenv("NET_GATEWAY");
    env_delenv("NET_NAMESERVER");
    env_delenv("NET_DOMAIN");

    x = (char *) net_getparam(NET_DEVNAME);
    if (!x) {
	return;
	}

    x = (char *) net_getparam(NET_DEVNAME);
    if (x) env_setenv("NET_DEVICE",x,ENV_FLG_BUILTIN);

    x = (char *) net_getparam(NET_DOMAIN);
    if (x) env_setenv("NET_DOMAIN",x,ENV_FLG_BUILTIN);

    addr = net_getparam(NET_IPADDR);
    if (addr) {
	xsprintf(str,"%I",addr);
	env_setenv("NET_IPADDR",str,ENV_FLG_BUILTIN);
	}

    addr = net_getparam(NET_NETMASK);
    if (addr) {
	xsprintf(str,"%I",addr);
	env_setenv("NET_NETMASK",str,ENV_FLG_BUILTIN);
	}

    addr = net_getparam(NET_GATEWAY);
    if (addr) {
	xsprintf(str,"%I",addr);
	env_setenv("NET_GATEWAY",str,ENV_FLG_BUILTIN);
	}

    addr = net_getparam(NET_NAMESERVER);
    if (addr) {
	xsprintf(str,"%I",addr);
	env_setenv("NET_NAMESERVER",str,ENV_FLG_BUILTIN);
	}

}
