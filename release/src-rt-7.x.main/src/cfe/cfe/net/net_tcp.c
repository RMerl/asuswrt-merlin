/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  TCP Protocol 				File: net_tcp.c
    *  
    *  This file contains a very simple TCP.  The basic goals of this
    *  tcp are to be "good enough for firmware."  We try to be 
    *  correct in our protocol implementation, but not very fancy.
    *  In particular, we don't deal with out-of-order segments,
    *  we don't hesitate to copy data more then necessary, etc.
    *  We strive to implement important protocol features
    *  like slow start, nagle, etc., but even these things are
    *  subsetted and simplified as much as possible.
    *
    *  Current "todo" list:  
    *	slow start
    *	good testing of retransmissions,
    *   round-trip delay calculations
    *   Process received TCP options, particularly segment size
    *   Ignore urgent data (remove from datastream)
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

#include "cfe_timer.h"

#include "cfe_error.h"

#include "net_tcpbuf.h"
#include "net_tcp_internal.h"
#include "net_tcp.h"

/*  *********************************************************************
    *  Config
    ********************************************************************* */

//#define _TCP_DEBUG_

/*  *********************************************************************
    *  Structures
    ********************************************************************* */

struct tcp_info_s {
    void *ti_ref;				/* ref data for IP layer */
    ip_info_t *ti_ipinfo;			/* IP layer handle */
    cfe_timer_t ti_fasttimer;			/* 200ms timer */
    queue_t ti_tcblist;				/* list of known TCBs */
    int ti_onqueue;				/* number of TCBs on queue */
    uint32_t ti_iss;				/* initial sequence number */
    tcb_t *ti_ports[TCP_MAX_PORTS];		/* table of active sockets */
};

/*  *********************************************************************
    *  Forward Declarations
    ********************************************************************* */

static void _tcp_protosend(tcp_info_t *ti,tcb_t *tcb);
static int _tcp_rx_callback(void *ref,ebuf_t *buf,uint8_t *destaddr,uint8_t *srcaddr);
static void _tcp_output(tcp_info_t *ti,tcb_t *tcb);
static void _tcp_aborttcb(tcp_info_t *ti,tcb_t *tcb);
static void _tcp_closetcb(tcp_info_t *ti,tcb_t *tcb);
static tcb_t *_tcp_find_lclport(tcp_info_t *ti,uint16_t port);
static void _tcp_freetcb(tcp_info_t *ti,tcb_t *tcb);
static void _tcp_sendctlmsg(tcp_info_t *ti,tcb_t *tcb,uint16_t flags,int timeout);


/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#ifdef _TCP_DEBUG_
#define _tcp_setstate(tcb,state) (tcb)->tcb_state = (state); \
                                 printf("tcp state = " #state "\n");
#define DEBUGMSG(x) printf x
#else
#define _tcp_setstate(tcb,state) (tcb)->tcb_state = (state);
#define DEBUGMSG(x)
#endif

#define _tcp_preparectlmsg(tcb,flags) (tcb)->tcb_txflags = (flags) ; \
               (tcb)->tcb_flags |= TCB_FLG_SENDMSG; 

#define _tcp_canceltimers(tcb) \
          TIMER_CLEAR((tcb)->tcb_timer_retx); \
          TIMER_CLEAR((tcb)->tcb_timer_keep); \
          TIMER_CLEAR((tcb)->tcb_timer_2msl); \
          TIMER_CLEAR((tcb)->tcb_timer_pers);
         

/*  *********************************************************************
    *  Globals
    ********************************************************************* */

#ifdef _TCP_DEBUG_
int _tcp_dumpflags = 1;
#else
int _tcp_dumpflags = 0;
#endif

/*  *********************************************************************
    *  _tcp_init(ipi,ref)
    *  
    *  Initialize the TCP module.  We set up our data structures
    *  and register ourselves with the IP layer.
    *  
    *  Input parameters: 
    *  	   ipi - IP information
    *  	   ref - will be passed back to IP as needed
    *  	   
    *  Return value:
    *  	   tcp_info_t structure, or NULL if problems
    ********************************************************************* */

tcp_info_t *_tcp_init(ip_info_t *ipi,void *ref)
{
    tcp_info_t *ti;
    int idx;

    ti = (tcp_info_t *) KMALLOC(sizeof(tcp_info_t),0);
    if (!ti) return NULL;

    ti->ti_ref = ref;
    ti->ti_ipinfo = ipi;

    /*
     * Start the "fast" timer
     */

    TIMER_SET(ti->ti_fasttimer,TCP_FAST_TIMER);

    /*
     * Initialize the TCB list
     */

    q_init(&(ti->ti_tcblist));

    for (idx = 0; idx < TCP_MAX_PORTS; idx++) {
	ti->ti_ports[idx] = NULL;
	}

    ti->ti_onqueue = 0;

    /*
     * Set up the initial sequence number
     */

    extern int32_t _getticks(void);		/* return value of CP0 COUNT */
    ti->ti_iss = (uint32_t) _getticks();

    /*
     * Register our protocol with IP
     */

    _ip_register(ipi,IPPROTO_TCP,_tcp_rx_callback,ti);

    return ti;
}


/*  *********************************************************************
    *  _tcp_uninit(info)
    *  
    *  De-initialize the TCP layer, unregistering from the IP layer.
    *  
    *  Input parameters: 
    *  	   info - our tcp_info_t, from _tcp_init()
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void _tcp_uninit(tcp_info_t *info)
{
    tcb_t *tcb;

    /*
     * Destroy all allocated TCBs, forcefully.
     */   

    while (!q_isempty(&(info->ti_tcblist))) {
	tcb = (tcb_t *) q_getfirst(&(info->ti_tcblist));
	/* tcp_freetcb removes tcb from the queue */
	_tcp_freetcb(info,tcb);
	}

    /*
     * Deregister with IP
     */

    _ip_deregister(info->ti_ipinfo,IPPROTO_TCP);

    /*
     * Free up the info structure
     */

    KFREE(info);
}


/*  *********************************************************************
    *  _tcp_freetcb(ti,tcb)
    *  
    *  Called when the TIME_WAIT timer expires, we use this to
    *  free up the TCB for good.
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   tcb - tcb to free
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void _tcp_freetcb(tcp_info_t *ti,tcb_t *tcb)
{
    /*
     * Undo socket number
     */

    if (tcb->tcb_socknum >= 0) ti->ti_ports[tcb->tcb_socknum] = NULL;
    tcb->tcb_socknum = -1;

    /*
     * Remove from queue
     */

    ti->ti_onqueue--;
    q_dequeue(&(tcb->tcb_qb));

    /*
     * Free buffers (could probably be done in tcb_destroy)
     */

    tmb_free(&(tcb->tcb_txbuf));
    tmb_free(&(tcb->tcb_rxbuf));

    /*
     * Free the TCB
     */

    KFREE(tcb);

}

/*  *********************************************************************
    *  _tcp_socket(info)
    *  
    *  Create a new tcp socket (a new tcb structure is allocated
    *  and entered into the socket table)
    *  
    *  Input parameters: 
    *  	   info - tcp information
    *  	   
    *  Return value:
    *  	   new socket number, or <0 if an error occured
    ********************************************************************* */

int _tcp_socket(tcp_info_t *info)
{
    int idx;
    tcb_t *tcb;

    /*
     * Find an empty slot.
     */

    for (idx = 0; idx < TCP_MAX_PORTS; idx++) {
	if (!info->ti_ports[idx]) break;
	}

    if (idx == TCP_MAX_PORTS) {
	return CFE_ERR_NOHANDLES;
	}

    /*
     * See if we can create another TCB 
     */

    if (info->ti_onqueue >= TCP_MAX_TCBS) return CFE_ERR_NOMEM;

    /*
     * Allocate data structures
     */

    tcb = KMALLOC(sizeof(tcb_t),0);
    if (!tcb) return CFE_ERR_NOMEM;

    memset(tcb,0,sizeof(tcb_t));

    if (tmb_alloc(&tcb->tcb_txbuf,TCP_BUF_SIZE) < 0) goto error;
    if (tmb_alloc(&tcb->tcb_rxbuf,TCP_BUF_SIZE) < 0) goto error;


    tcb->tcb_mtu = 1400;

    /*
     * Default socket flags
     */

    tcb->tcb_sockflags = TCPFLG_NBIO;

    /*
     * Set up initial state.  Find an empty port number.
     * note that the way we do this is pretty gruesome, but it will
     * work for our small TCP, where the number of TCBs outstanding
     * will be very small compared to the port number space.
     * 
     * Try to look up the port number we want - if we find it, increment
     * it and try again until we find an unused one.
     * Stay away from ports 0..1023.
     */

    _tcp_setstate(tcb,TCPSTATE_CLOSED);
    tcb->tcb_lclport = (uint16_t) (((uint16_t) cfe_ticks) + 1024);

    while (_tcp_find_lclport(info,tcb->tcb_lclport) != NULL) {
	tcb->tcb_lclport++;
	if (tcb->tcb_lclport == 0) tcb->tcb_lclport = 1024;
	}

    /*
     * Remember this socket in the table
     */

    info->ti_ports[idx] = tcb;
    tcb->tcb_socknum = idx;

    info->ti_onqueue++;
    q_enqueue(&(info->ti_tcblist),&(tcb->tcb_qb));

    return idx;

error:
    tmb_free(&(tcb->tcb_txbuf));
    tmb_free(&(tcb->tcb_rxbuf));
    KFREE(tcb);
    return CFE_ERR_NOMEM;
}


/*  *********************************************************************
    *  _tcp_connect(ti,s,dest,port)
    *  
    *  Connect a socket to a remote port.  
    *  
    *  Input parameters: 
    *  	   ti - TCP information
    *  	   s - socket number, allocated via _tcp_create
    *  	   dest - destination IP address
    *  	   port - destination port number
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int _tcp_connect(tcp_info_t *ti,int s,uint8_t *dest,uint16_t port)
{
    tcb_t *tcb;

    tcb = ti->ti_ports[s];
    if (!tcb) return CFE_ERR_INV_PARAM;

    memcpy(tcb->tcb_peeraddr,dest,IP_ADDR_LEN);
    tcb->tcb_peerport = port;

    tcb->tcb_rcvnext = 0;
    tcb->tcb_rcvack = 0;

    tcb->tcb_sendnext = ti->ti_iss;
    tcb->tcb_sendunack = ti->ti_iss;
    tcb->tcb_sendwindow = 0;

    tmb_init(&tcb->tcb_txbuf);
    tmb_init(&tcb->tcb_rxbuf);

    TIMER_SET(tcb->tcb_timer_keep,TCP_KEEPALIVE_TIMER);
    _tcp_setstate(tcb,TCPSTATE_SYN_SENT);

    _tcp_sendctlmsg(ti,tcb,TCPFLG_SYN,TCP_RETX_TIMER);

    return 0;
}


/*  *********************************************************************
    *  _tcp_close(ti,s)
    *  
    *  Disconnect a TCP socket nicely.  Sends a FIN packet to get 
    *  us into the disconnect state.
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   s - socket number
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int _tcp_close(tcp_info_t *ti,int s)
{
    tcb_t *tcb;

    tcb = ti->ti_ports[s];
    if (!tcb) return CFE_ERR_INV_PARAM;

    /*
     * Reclaim this socket number for future use
     */

    ti->ti_ports[s] = NULL;
    tcb->tcb_socknum = -1;

    /*
     * Decide what action to take based on current state
     */

    switch (tcb->tcb_state) {
	case TCPSTATE_SYN_RECEIVED:
	case TCPSTATE_ESTABLISHED:
	    _tcp_setstate(tcb,TCPSTATE_FINWAIT_1);
	    _tcp_sendctlmsg(ti,tcb,TCPFLG_ACK | TCPFLG_FIN,TCP_RETX_TIMER);
	    break;

	case TCPSTATE_CLOSED:
	case TCPSTATE_LISTEN:
	case TCPSTATE_SYN_SENT:
	    /*
	     * Disconnect during our attempt, or from some
	     * idle state that does not require sending anything.
	     * Go back to CLOSED.
	     */
	     _tcp_closetcb(ti,tcb);
	     _tcp_freetcb(ti,tcb);
	    break;

	case TCPSTATE_CLOSE_WAIT:
	    _tcp_setstate(tcb,TCPSTATE_LAST_ACK);
	    _tcp_sendctlmsg(ti,tcb,TCPFLG_ACK | TCPFLG_FIN,TCP_RETX_TIMER);
	    break;
	    
	case TCPSTATE_TIME_WAIT:
	case TCPSTATE_FINWAIT_1:
	case TCPSTATE_FINWAIT_2:
	case TCPSTATE_CLOSING:
	case TCPSTATE_LAST_ACK:
	default:
	    break;
	}

    return 0;
}



/*  *********************************************************************
    *  _tcp_aborttcb(ti,tcb)
    *  
    *  Forcefully terminate a TCP connection.  Sends an RST packet
    *  to nuke the other end.  The socket is forced into the CLOSED
    *  state.
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   tcb -tcb to abort
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void _tcp_aborttcb(tcp_info_t *ti,tcb_t *tcb)
{
    DEBUGMSG(("tcp_abort from state %d\n",tcb->tcb_state));

    /*
     * Decide what action to take based on current state
     * If we're in SYN_SENT, RECEIVED, ESTABLISHED, FINWAIT_1,
     * FINWAIT_2, CLOSING, LAST_ACK, or CLOSE_WAIT we've sent
     * some traffic on this TCB, so send an RST to kill off
     * the remote TCB.
     */

    if (TCPSTATE_IN_SET(tcb->tcb_state,M_TCPSTATE_ABORTSTATES)) {
	/* Send RST with no timeout, don't retransmit it. */
	_tcp_canceltimers(tcb);
	_tcp_sendctlmsg(ti,tcb,TCPFLG_ACK | TCPFLG_RST,0);
	}

    /*
     * No matter what, it's now CLOSED.
     */

    _tcp_closetcb(ti,tcb);

}


/*  *********************************************************************
    *  _tcp_closetcb(ti,tcb)
    *  
    *  Close a TCB, switching the state to "closed" and resetting
    *  internal variables.  The TCB is *not* freed.
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   tcb - tcb to close
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void _tcp_closetcb(tcp_info_t *ti,tcb_t *tcb)
{
    /*
     * Set state to "closed" and reset timers
     */

    _tcp_setstate(tcb,TCPSTATE_CLOSED);
    tcb->tcb_flags = 0;
    _tcp_canceltimers(tcb);

    /*
     * Reinitialize the buffers to waste the stored send data and
     * clear out any receive data
     */

    tmb_init(&tcb->tcb_txbuf);
    tmb_init(&tcb->tcb_rxbuf);
}

/*  *********************************************************************
    *  _tcp_send(ti,s,buf,len)
    *  
    *  Queue some data to be transmitted via a TCP socket
    *  
    *  Input parameters: 
    *  	   ti - TCP information
    *  	   s - socket number
    *  	   buf - buffer of data to send
    *  	   len - size of buffer to send
    *  	   
    *  Return value:
    *  	   number of bytes queued
    *  	   <0 if an error occured
    ********************************************************************* */

int _tcp_send(tcp_info_t *ti,int s,uint8_t *buf,int len)
{
    tcb_t *tcb;
    int retlen;
    int curlen;

    tcb = ti->ti_ports[s];
    if (!tcb) return CFE_ERR_INV_PARAM;

    /*
     * State must be ESTABLISHED or CLOSE_WAIT.  CLOSE_WAIT
     * means we've received a fin, but we can still send in
     * the outbound direction.
     */

    if (!TCPSTATE_IN_SET(tcb->tcb_state,M_TCPSTATE_SEND_OK)) {
	return CFE_ERR_NOTCONN;
	}

    /*
     * Copy the user data into the transmit buffer.
     */

    curlen = tmb_curlen(&(tcb->tcb_txbuf));
    retlen = tmb_copyin(&(tcb->tcb_txbuf),buf,len,TRUE);

    /*
     * Cause some output on the device.
     */

    /*
     * Nagle: Call _tcp_output only if there is no outstanding
     * unacknowledged data.  The way our transmit buffer
     * works, it only holds unacknowledged data, so this
     * test is easy.  It isn't really 100% correct to
     * do it this way, but the effect is the same; we will
     * not transmit tinygrams.
     */

    if ((curlen == 0) || (tcb->tcb_sockflags & TCPFLG_NODELAY)) {
	_tcp_output(ti,tcb);	
	}

    return retlen;
}

/*  *********************************************************************
    *  _tcp_recv(ti,s,buf,len)
    *  
    *  Get buffered receive data from the TCP socket
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   s - socket number
    *  	   buf - pointer to receive buffer area
    *  	   len - size of receive buffer area
    *  	   
    *  Return value:
    *  	   number of bytes received
    *  	   <0 if an error occured
    *  	   ==0 if no data available (or tcp session is closed)
    ********************************************************************* */

int _tcp_recv(tcp_info_t *ti,int s,uint8_t *buf,int len)
{
    tcb_t *tcb;
    int retlen;

    tcb = ti->ti_ports[s];
    if (!tcb) return CFE_ERR_INV_PARAM;

    if (!TCPSTATE_IN_SET(tcb->tcb_state,M_TCPSTATE_RECV_OK)) {
	return CFE_ERR_NOTCONN;
	}

    retlen = tmb_copyout(&(tcb->tcb_rxbuf),buf,len,TRUE);

    /*
     * If we've drained all of the data out of the buffer
     * send an ack.  This isn't ideal, but it will
     * prevent us from keeping the window closed.
     */

//    if (retlen && (tmb_curlen(&(tcb->tcb_rxbuf)) == 0)) {
//	_tcp_preparectlmsg(tcb,TCPFLG_ACK);
//	_tcp_protosend(ti,tcb);
//	tcb->tcb_flags &= ~TCB_FLG_DLYACK;
//	}

    return retlen;
}


/*  *********************************************************************
    *  _tcp_bind(ti,s,port)
    *  
    *  "bind" a TCP socket to a particular port - this sets the 
    *  outbound local (source) port number.
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   s - socket number
    *  	   port - port number
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int _tcp_bind(tcp_info_t *ti,int s,uint16_t port)
{
    tcb_t *tcb;

    tcb = ti->ti_ports[s];
    if (!tcb) return CFE_ERR_INV_PARAM;


    if (_tcp_find_lclport(ti,port)) return CFE_ERR_ADDRINUSE;

    tcb->tcb_lclport = port;

    return 0;
}

/*  *********************************************************************
    *  _tcp_setflags(ti,s,flags)
    *  
    *  Set per-socket flags.
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   s - socket number
    *  	   flags - flags to set
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int _tcp_setflags(tcp_info_t *ti,int s,unsigned int flags)
{
    tcb_t *tcb;

    tcb = ti->ti_ports[s];
    if (!tcb) return CFE_ERR_INV_PARAM;

    tcb->tcb_sockflags = flags;

    return 0;
}

/*  *********************************************************************
    *  _tcp_getflags(ti,s,flags)
    *  
    *  Get per-socket flags.
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   s - socket number
    *  	   flags - pointer to int to receive flags
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int _tcp_getflags(tcp_info_t *ti,int s,unsigned int *flags)
{
    tcb_t *tcb;

    tcb = ti->ti_ports[s];
    if (!tcb) return CFE_ERR_INV_PARAM;

    *flags = tcb->tcb_sockflags;

    return 0;
}


/*  *********************************************************************
    *  _tcp_peeraddr(ti,s,addr,port)
    *  
    *  Return the address of the computer on the other end of this
    *  connection.
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   s - socket number
    *      addr - receives IP address of remote
    *  	   port - port number
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int _tcp_peeraddr(tcp_info_t *ti,int s,uint8_t *addr,uint16_t *port)
{
    tcb_t *tcb;

    tcb = ti->ti_ports[s];
    if (!tcb) return CFE_ERR_INV_PARAM;

    /* 
     * Test for any of the states where we believe the peeraddr in the tcb
     * is valid.
     */

    if (!TCPSTATE_IN_SET(tcb->tcb_state,M_TCPSTATE_PEERADDR_OK)) {
	return CFE_ERR_NOTCONN;
	}

    if (addr) memcpy(addr,tcb->tcb_peeraddr,IP_ADDR_LEN);
    if (port) *port = tcb->tcb_peerport;

    return 0;
}

/*  *********************************************************************
    *  _tcp_listen(ti,s,port)
    *  
    *  Set a socket for listening mode, allowing inbound connections
    *  to occur.
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   s - socket nunber
    *  	   port - port number to listen for (implicit tcp_bind)
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int _tcp_listen(tcp_info_t *ti,int s,uint16_t port)
{
    tcb_t *tcb;
    queue_t *qb;

    /*
     * See if another TCB is listening to this port
     */

    for (qb = ti->ti_tcblist.q_next; qb != &(ti->ti_tcblist); qb = qb->q_next) {
	tcb = (tcb_t *) qb;

	if ((tcb->tcb_lclport == port) &&
	    (tcb->tcb_state == TCPSTATE_LISTEN) && 
	    (tcb->tcb_peerport == 0)) return CFE_ERR_ADDRINUSE;

	}

    /*
     * Otherwise, we're good to go.
     */

    tcb = ti->ti_ports[s];
    if (!tcb) return CFE_ERR_INV_PARAM;

    tcb->tcb_lclport = port;
    _tcp_setstate(tcb,TCPSTATE_LISTEN);

    tcb->tcb_sendnext = 0;
    tcb->tcb_sendunack = 0;
    tcb->tcb_sendwindow = 0;
    _tcp_canceltimers(tcb);

    tcb->tcb_rcvnext = 0;
    tcb->tcb_rcvack = 0;

    tmb_init(&tcb->tcb_txbuf);
    tmb_init(&tcb->tcb_rxbuf);

    tcb->tcb_txflags = 0;

    return 0;
}

/*  *********************************************************************
    *  _tcp_status(ti,s,connflag,rxready,rxeof)
    *  
    *  Get status information about the TCP session
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   s - socket nunber
    *  	   connflag - points to an int to receive connection status
    *  	             (1=connected,0=not connected)
    *  	   rxready - number of bytes in receive queue
    *      rxeof - returns 1 if we've been FINed.
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int _tcp_status(tcp_info_t *ti,int s,int *connflag,int *rxready,int *rxeof)
{
    tcb_t *tcb;
    int rxlen;

    tcb = ti->ti_ports[s];
    if (!tcb) return CFE_ERR_INV_PARAM;

    rxlen = tmb_curlen(&(tcb->tcb_rxbuf));

    /*
     * We consider the connection "connected" if it's established
     * or it's in CLOSE_WAIT (FIN received) but we have not drained
     * all of the receive data.
     * 
     * Otherwise: If it's not in one of the connection establishment states,
     * it's not connected.
     */

    if (connflag) {
	if ((tcb->tcb_state == TCPSTATE_ESTABLISHED) ||
	    ((rxlen != 0) && (tcb->tcb_state == TCPSTATE_CLOSE_WAIT))) {
	    *connflag = TCPSTATUS_CONNECTED;
	    }
	else if (TCPSTATE_IN_SET(tcb->tcb_state,M_TCPSTATE_CONNINPROGRESS)) {
	    *connflag = TCPSTATUS_CONNECTING;
	    }
	else {
	    *connflag = TCPSTATUS_NOTCONN;
	    }
	}

    if (rxready) {
	*rxready = rxlen;
	}

    if (rxeof) {
	*rxeof = 0;
	if ((tcb->tcb_state == TCPSTATE_CLOSE_WAIT) ||
	    (tcb->tcb_state == TCPSTATE_LAST_ACK)) {
	    *rxeof = 1;
	    }
	}

    return 0;
}

/*  *********************************************************************
    *  _tcp_debug(ti,s,arg)
    *  
    *  Perform debug functions on a socket
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   s - socket number
    *  	   arg - argument
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int _tcp_debug(tcp_info_t *ti,int s,int arg)
{
    tcb_t *tcb;

    tcb = ti->ti_ports[s];
    if (!tcb) return CFE_ERR_INV_PARAM;

    printf("State=%d SendNext=%u SendUnack=%u ",
	   tcb->tcb_state,tcb->tcb_sendnext,tcb->tcb_sendunack);
    printf("SendWin=%d Ack=%u Rxlen=%d\n",
	   tcb->tcb_sendwindow,
	   tcb->tcb_rcvack,tmb_curlen(&(tcb->tcb_rxbuf)));

    return 0;
}


#ifdef _TCP_DEBUG_
/*  *********************************************************************
    *  _tcp_dumppktstate(label,flags,ack,seq,len)
    *  
    *  A debug function.
    *  
    *  Input parameters: 
    *  	   label - printed before packet state
    *  	   flags - transmit flags for packet
    *  	   ack,seq,len - from the packet
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
static void _tcp_dumppktstate(char *label,uint16_t flags,
			      uint32_t ack,uint32_t seq,int len)
{
    printf("%s: ",label);
    if (flags & TCPFLG_FIN) printf("FIN ");
    if (flags & TCPFLG_SYN) printf("SYN ");
    if (flags & TCPFLG_RST) printf("RST ");
    if (flags & TCPFLG_PSH) printf("PSH ");
    if (flags & TCPFLG_ACK) printf("ACK ");
    if (flags & TCPFLG_URG) printf("URG ");
    printf("Ack=%u Seq=%u Data=%d\n",ack,seq,len);
}
#endif


/*  *********************************************************************
    *  _tcp_output(ti,tcb)
    *  
    *  Try to perform some output on this TCB.  If there is
    *  data to send and we can transmit it (i.e., the remote's
    *  receive window will allow it), segment the data from the
    *  buffer and transmit it, updating local state variables.
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   tcb - the tcb to send data on
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void _tcp_output(tcp_info_t *ti,tcb_t *tcb)
{
    ebuf_t *b;
    int tcplen;
    int window;
    uint16_t flags;
    uint8_t *cksumptr;
    uint8_t *tcphdr;
    uint16_t cksum;
    int hdrlen;
    uint8_t pseudoheader[12];
    int offset;
    uint32_t sendmax;
    uint32_t windowmax;
    uint32_t cwndmax;

    /*
     * sendmax is (one plus) the highest sequence number we have
     * data for.
     *
     * windowmax is (one plus) the highest sequence number in the
     * send window.
     *
     * cwndmax is (one plus) the highest sequence number in the
     * congestion window.
     */

    sendmax = tcb->tcb_sendunack + tmb_curlen(&(tcb->tcb_txbuf));
    windowmax = tcb->tcb_sendunack + tcb->tcb_sendwindow;
    cwndmax = tcb->tcb_sendunack + 5*1400;

    /*
     * We'll send up to 'sendmax', 'cwndmax', or 'windowmax' bytes, whichever
     * is sooner.  Set 'sendmax' to the earliest sequence number.
     */

    if (TCPSEQ_GT(sendmax,windowmax)) sendmax = windowmax;
    if (TCPSEQ_GT(sendmax,cwndmax)) sendmax = cwndmax;

    /* 
     * The (receive) window is whatever it takes to fill the buffer.
     */

    window = tmb_remlen(&(tcb->tcb_rxbuf));
    if (window > 65000) window = 65000;

    /*
     * Spit out packets until "sendnext" either catches up
     * or exceeds the remote window
     */

    while (TCPSEQ_LT(tcb->tcb_sendnext,sendmax)) {

	/*
	 * This is the offset into the transmit buffer to start with.
	 * Offset 0 in the transmit buffer corresponds to "send unack"
	 * received acks move this pointer.
	 */

	offset = tcb->tcb_sendnext - tcb->tcb_sendunack;

	/*
	 * Allocate a buffer and remember the pointer to where
	 * the header starts.
	 */

	b = _ip_alloc(ti->ti_ipinfo);
	if (!b) break;

	tcphdr = ebuf_ptr(b) + ebuf_length(b);

	flags = TCPFLG_ACK | TCPHDRFLG(TCP_HDR_LENGTH);
	hdrlen = TCP_HDR_LENGTH;

	/*
	 * Fill in the fields according to the RFC.
	 */

	tcb->tcb_rcvack = tcb->tcb_rcvnext;	/* Update our "most recent ack" */

	ebuf_append_u16_be(b,tcb->tcb_lclport);	/* Local and remote ports */
	ebuf_append_u16_be(b,tcb->tcb_peerport);
	ebuf_append_u32_be(b,tcb->tcb_sendnext); /* sequence and ack numbers */
	ebuf_append_u32_be(b,tcb->tcb_rcvack);
	ebuf_append_u16_be(b,flags);		/* Flags */
	ebuf_append_u16_be(b,window);		/* Window size */
	cksumptr = ebuf_ptr(b) + ebuf_length(b);
	ebuf_append_u16_be(b,0);		/* dummy checksum for calculation */
	ebuf_append_u16_be(b,0);		/* Urgent Pointer (not used) */

	/*
	 * Append the data, copying pieces out of the transmit buffer
	 * without adjusting its pointers.
	 */

	tcplen = tmb_copyout2(&(tcb->tcb_txbuf),
			      b->eb_ptr + b->eb_length,
			      offset,
			      tcb->tcb_mtu);

	b->eb_length += tcplen;

#ifdef _TCP_DEBUG_
	if (_tcp_dumpflags) _tcp_dumppktstate("TX_DATA",flags,
					      tcb->tcb_sendnext,
					      tcb->tcb_rcvack,tcplen);
#endif

	/*
	 * Adjust the "send next" sequence number to account for what
	 * we're about to send.
	 */

	tcb->tcb_sendnext += tcplen;

    
	/*
	 * Build the pseudoheader, which is part of the checksum calculation
	 */

	_ip_getaddr(ti->ti_ipinfo,&pseudoheader[0]);
	memcpy(&pseudoheader[4],tcb->tcb_peeraddr,IP_ADDR_LEN);
	pseudoheader[8] = 0;
	pseudoheader[9] = IPPROTO_TCP;
	pseudoheader[10] = ((tcplen+hdrlen) >> 8) & 0xFF;
	pseudoheader[11] = ((tcplen+hdrlen) & 0xFF);

	/*
	 * Checksum the packet and insert the checksum into the header 
	 */

	cksum = ip_chksum(0,pseudoheader,sizeof(pseudoheader));
	cksum = ip_chksum(cksum,tcphdr,tcplen + hdrlen);
	cksum = ~cksum;
	cksumptr[0] = (cksum >> 8) & 0xFF;
	cksumptr[1] = (cksum & 0xFF);

	/*
	 * Transmit the packet.  The layer below us will free it.
	 */

	_ip_send(ti->ti_ipinfo,b,tcb->tcb_peeraddr,IPPROTO_TCP);

	/*
	 * Set the timer that we'll use to wait for an acknowledgement.
	 * If this timer goes off, we'll rewind "sendnext" back to
	 * "sendunack" and transmit stuff again.
	 */

	TIMER_SET(tcb->tcb_timer_retx,TCP_RETX_TIMER);

	/*
	 * We just sent an ack, so cancel the delayed ack timer.
	 */

	tcb->tcb_flags &= ~TCB_FLG_DLYACK;

	}

}

/*  *********************************************************************
    *  _tcp_protosend(ti,tcb)
    *  
    *  Transmit "protocol messages" on the tcb.  This is used for
    *  sending SYN, FIN, ACK, and other control packets.
    *  
    *  Input parameters: 
    *  	   ti - tcp infomration
    *  	   tcb - tcb we're interested in
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void _tcp_protosend(tcp_info_t *ti,tcb_t *tcb)
{
    ebuf_t *b;
    int tcplen;
    int window;
    uint16_t flags;
    uint8_t *cksumptr;
    uint8_t *tcphdr;
    uint16_t cksum;
    int hdrlen;
    uint8_t pseudoheader[12];

    /*
     * Allocate a buffer and remember the pointer to where
     * the header starts.
     */

    b = _ip_alloc(ti->ti_ipinfo);
    if (!b) return;

    tcphdr = ebuf_ptr(b) + ebuf_length(b);

    /*
     * We're going to send something, so we can clear the flag.
     * Also clear the delay-ack flag since everything's an ack.
     */

    tcb->tcb_flags &= ~(TCB_FLG_SENDMSG | TCB_FLG_DLYACK);

    /*
     * Build the TCP header
     */

    /* The flags are the current flags + the header length */
    if (tcb->tcb_txflags & TCPFLG_SYN) {
	flags = tcb->tcb_txflags | (TCPHDRFLG(TCP_HDR_LENGTH + 4));
	hdrlen = TCP_HDR_LENGTH + 4;
	}
    else {
	flags = tcb->tcb_txflags | (TCPHDRFLG(TCP_HDR_LENGTH));
	hdrlen = TCP_HDR_LENGTH;
	}


#ifdef _TCP_DEBUG_
	if (_tcp_dumpflags) _tcp_dumppktstate("TX_CTL",flags,
					      tcb->tcb_sendnext,
					      tcb->tcb_rcvack,0);
#endif

    /* 
     * The (receive) window is whatever it takes to fill the buffer.
     */

    window = tmb_remlen(&(tcb->tcb_rxbuf));
    if (window > 65000) window = 65000;

    /*
     * Fill in the fields according to the RFC.
     */

    tcb->tcb_rcvack = tcb->tcb_rcvnext;		/* update last tx ack */

    ebuf_append_u16_be(b,tcb->tcb_lclport);	/* Local and remote ports */
    ebuf_append_u16_be(b,tcb->tcb_peerport);
    ebuf_append_u32_be(b,tcb->tcb_sendnext);	/* sequence and ack numbers */
    ebuf_append_u32_be(b,tcb->tcb_rcvack);
    ebuf_append_u16_be(b,flags);		/* Flags */
    ebuf_append_u16_be(b,window);		/* Window size */
    cksumptr = ebuf_ptr(b) + ebuf_length(b);
    ebuf_append_u16_be(b,0);		/* dummy checksum for calculation */
    ebuf_append_u16_be(b,0);		/* Urgent Pointer (not used) */

    /*
     * Append TCP options on SYN packet
     */

    if (flags & TCPFLG_SYN) {
	ebuf_append_u16_be(b,TCP_MAX_SEG_OPT);
	ebuf_append_u16_be(b,TCP_MAX_SEG_SIZE);
	}

    tcplen = 0;				/* don't transmit data here */


    if (flags & (TCPFLG_SYN | TCPFLG_FIN)) tcb->tcb_sendnext++;
    
    /*
     * Build the pseudoheader, which is part of the checksum calculation
     */

    _ip_getaddr(ti->ti_ipinfo,&pseudoheader[0]);
    memcpy(&pseudoheader[4],tcb->tcb_peeraddr,IP_ADDR_LEN);
    pseudoheader[8] = 0;
    pseudoheader[9] = IPPROTO_TCP;
    pseudoheader[10] = ((tcplen+hdrlen) >> 8) & 0xFF;
    pseudoheader[11] = ((tcplen+hdrlen) & 0xFF);

    /*
     * Checksum the packet and insert the checksum into the header 
     */

    cksum = ip_chksum(0,pseudoheader,sizeof(pseudoheader));
    cksum = ip_chksum(cksum,tcphdr,tcplen + hdrlen);
    cksum = ~cksum;
    cksumptr[0] = (cksum >> 8) & 0xFF;
    cksumptr[1] = (cksum & 0xFF);

    /*
     * Transmit the packet.  The layer below us will free it.
     */

    _ip_send(ti->ti_ipinfo,b,tcb->tcb_peeraddr,IPPROTO_TCP);
}

/*  *********************************************************************
    *  _tcp_sendreset(ti,dstipaddr,ack,srcport,dstport)
    *  
    *  Transmit "protocol messages" on the tcb.  This is used for
    *  sending SYN, FIN, ACK, and other control packets.
    *  
    *  Input parameters: 
    *  	   ti - tcp infomration
    *  	   tcb - tcb we're interested in
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void _tcp_sendreset(tcp_info_t *ti,uint8_t *dstipaddr,uint32_t ack,
			   uint16_t srcport,uint16_t dstport)
{
    tcb_t tcb;		/* fake TCB so we can use _tcp_protosend */

    memset(&tcb,0,sizeof(tcb));
    memcpy(tcb.tcb_peeraddr,dstipaddr,IP_ADDR_LEN);
    tcb.tcb_peerport = dstport;
    tcb.tcb_lclport = srcport;
    tcb.tcb_txflags = TCPFLG_RST|TCPFLG_ACK;
    tcb.tcb_rcvnext = ack;

    _tcp_protosend(ti,&tcb);
}


/*  *********************************************************************
    *  _tcp_sendctlmsg(ti,tcb,flags,timeout)
    *  
    *  Set up for and transmit a control message.  This is usually
    *  called on a state transition where we need to send a control
    *  message like a SYN or FIN, with a timeout.  We reset the 
    *  retry counter, set the retransmit timer, and fire off the message
    *  right here.
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   tcb - the tcb for this TCP session
    *  	   flags - packet flags (TCPFLG_xxx)
    *  	   timeout - timeout value , in ticks
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void _tcp_sendctlmsg(tcp_info_t *ti,tcb_t *tcb,uint16_t flags,int timeout)
{
    tcb->tcb_txflags = flags;
    tcb->tcb_retrycnt = 0;

    if (timeout >= 0) {
	if (timeout) {
	    TIMER_SET(tcb->tcb_timer_retx,timeout);
	    }
	else {
	    TIMER_CLEAR(tcb->tcb_timer_retx);
	    }
	}

    _tcp_protosend(ti,tcb);
}


/*  *********************************************************************
    *  _tcp_find_lclport(ti,port)
    *  
    *  Find the specified local port - mostly to see if it is
    *  already in use.
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   port - local port
    *  	   
    *  Return value:
    *  	   tcb owning this port, or NULL if none found
    ********************************************************************* */
static tcb_t *_tcp_find_lclport(tcp_info_t *ti,uint16_t port)
{
    tcb_t *tcb = NULL;
    queue_t *qb;

    for (qb = ti->ti_tcblist.q_next; qb != &(ti->ti_tcblist); qb = qb->q_next) {
	tcb = (tcb_t *) qb;

	if (tcb->tcb_lclport == port) return tcb;

	}

    return NULL;

}

/*  *********************************************************************
    *  _tcp_find_tcb(ti,srcport,dstport,saddr)
    *  
    *  Locate the TCB in the active TCBs.  This is used to match
    *  an inbound TCP packet to a TCB.
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   srcport,dstport - source and dest ports from TCP header
    *  	   saddr - source IP address of sending host
    *  	   
    *  Return value:
    *  	   tcb pointer or NULL if no TCB was found
    ********************************************************************* */

static tcb_t *_tcp_find_tcb(tcp_info_t *ti,uint16_t srcport,uint16_t dstport,uint8_t *saddr)
{
    tcb_t *tcb = NULL;
    queue_t *qb;

    for (qb = ti->ti_tcblist.q_next; qb != &(ti->ti_tcblist); qb = qb->q_next) {
	tcb = (tcb_t *) qb;

	/* Try active TCBs */
	if ((tcb->tcb_peerport != 0) &&
	    (tcb->tcb_lclport == dstport) &&
	    (tcb->tcb_peerport == srcport) &&
	    (memcmp(saddr,tcb->tcb_peeraddr,IP_ADDR_LEN) == 0)) break;
	}

    /* Found it on our active list. */
    if (qb != &(ti->ti_tcblist)) return tcb;

    /* no dice, try listening ports */
    for (qb = ti->ti_tcblist.q_next; qb != &(ti->ti_tcblist); qb = qb->q_next) {
	tcb = (tcb_t *) qb;

	/* Try active TCBs */
	if ((tcb->tcb_peerport == 0) &&
	    (tcb->tcb_lclport == dstport)) break;
	}

    /* Found it on our passive list. */
    if (qb != &(ti->ti_tcblist)) return tcb;

    return NULL;
}

/*  *********************************************************************
    *  _tcp_procack(ti,tcb,ack,flags)
    *  
    *  Process a received acknowledgement.
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   tcb - tcb for this tcb session
    *  	   ack - acknum from received packet
    *  	   flags - flags from received packet
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void _tcp_procack(tcp_info_t *ti,tcb_t *tcb,uint32_t ack,uint16_t flags)
{
    int ackamt;
    int unacklen;

    /* This is the number of unacknowledged bytes we have */
    unacklen = tmb_curlen(&(tcb->tcb_txbuf));

    switch (tcb->tcb_state) {

	case TCPSTATE_ESTABLISHED:
	    /*
	     * The ack is valid if it's in the range of
	     * unacknowledged data we're holding onto.
	     *
	     *     sendunack < ack <= (sendunack+datalen)
	     *
	     * Do the first comparison as "<=" instead of just "<"
	     * so we can catch duplicate acks.
	     */

	    if (!(TCPSEQ_LEQ(tcb->tcb_sendunack,ack) &&
		  TCPSEQ_LEQ(ack,(tcb->tcb_sendunack+unacklen)))) {
		DEBUGMSG(("Ack is out of range: %u\n",ack));
		return;
		}

	    /* 
	     * Compute the # of bytes spanned by this ack 
	     */

	    ackamt = TCPSEQ_DIFF(ack,tcb->tcb_sendunack);

	    /* 
	     * If we actually acked something, adjust the tx buffer
	     * to remove the bytes covered by the ack, then update
	     * the 'next' sequence number so we'll start there next
	     * time.
	     */

	    if (ackamt > 0) {
		tmb_adjust(&tcb->tcb_txbuf,ackamt);
		tcb->tcb_txflags = TCPFLG_ACK;
		tcb->tcb_sendunack = ack;
		tcb->tcb_dup_ackcnt = 0;     	/* not a duplicate */
		}
	    else {
		tcb->tcb_dup_ackcnt++;
		//DEBUGMSG(("Duplicate ack received\n"));
		}

	    /*
	     * If we're all caught up, we can cancel the 
	     * retransmit timer.  Otherwise, try to do 
	     * some output on the interface.  This will
	     * reset the retransmit timer if we did anything.
	     */

	    if (tmb_curlen(&(tcb->tcb_txbuf)) != 0) {
		tcb->tcb_flags |= TCB_FLG_OUTPUT;
		}
	    else {
		TIMER_CLEAR(tcb->tcb_timer_retx);
		}
		
	    break;

	case TCPSTATE_FINWAIT_1:
	    ackamt = TCPSEQ_DIFF(ack,tcb->tcb_sendunack);
	    if (ackamt == 1) {
		tcb->tcb_sendunack = ack;
		if (flags & TCPFLG_FIN) {
		    _tcp_setstate(tcb,TCPSTATE_TIME_WAIT);
		    _tcp_canceltimers(tcb);
		    _tcp_preparectlmsg(tcb,TCPFLG_ACK);
		    TIMER_SET(tcb->tcb_timer_2msl,TCP_TIMEWAIT_TIMER);
		    }
		else {
		    _tcp_setstate(tcb,TCPSTATE_FINWAIT_2);
		    }
		}
	    break;

	}
}

/*  *********************************************************************
    *  _tcp_procdata(ti,tcb,seqnum,flags,buf)
    *  
    *  Process data in a received TCP packet - we'll put the
    *  data into the receive ring buffer, process the seqnum
    *  field, and prepare to send an ack.
    *  
    *  Input parameters: 
    *  	   ti - tcp information
    *  	   tcb - tcb describing TCP session
    *  	   seqnum,flags - from the header of the received TCP packet
    *  	   buf - ebuf contianing data
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void _tcp_procdata(tcp_info_t *ti,tcb_t *tcb,uint32_t seqnum,
			  uint16_t flags,ebuf_t *buf)
{
    int datalen;
    uint8_t *bp;
    int rxwin,rwin;
    uint32_t endseqnum;
    uint32_t endrxwindow;
    int reallen;

    /*
     * Figure out how much we're going to take.  This should not
     * exceed our buffer size because we advertised a window 
     * only big enough to fill our buffer.
     */
     
    bp = ebuf_ptr(buf);				/* pointer to TCP data */
    datalen = ebuf_remlen(buf);			/* Size of TCP data */

    /*
     * If there's no data in the buffer, we can skip the data-related
     * stuff and check for a possible FIN.
     */

    if (datalen != 0) {

	/*
	 * Figure out if the data fits within the window.  We don't deal
	 * with out-of-order segments, so the test is relatively
	 * simple: The received segment must contain (or match)
	 * the sequence number from "rcvnext" to the end of the receive
	 * window.
	 *
	 * Therefore:   seqnum <= rcvnext < seqnum+datalen
	 */

	rxwin = tmb_remlen(&(tcb->tcb_rxbuf));	/* receive window size */
	endseqnum = (seqnum + datalen);		/* seq # of end of segment */
	endrxwindow = tcb->tcb_rcvack + rxwin;	/* end of receive window */

	/*
	 * The segment might include data outside the receive window
	 * (it's not supposed to, but it can).  Crop the 'endseqnum'
	 * value at the rx window if necessary.  Keep the earlier seqnum.
	 */
//
//	if (TCPSEQ_LT(endrxwindow,endseqnum)) {
//	    endseqnum = endrxwindow;
//	    }

	/*
	 * Test to see if the data is in sequence.
	 */

	rwin = (TCPSEQ_LEQ(seqnum,tcb->tcb_rcvnext) &&
		TCPSEQ_LT(tcb->tcb_rcvnext,endseqnum));

	if (!rwin) {
	    DEBUGMSG(("Dropping out-of-order packet %u %u %u\n",seqnum,
		      tcb->tcb_rcvnext,endseqnum));
	    return;
	    }

	/*
	 * The actual amount of new data is the distance from
	 * the "rcvnext" pointer to the end sequence number.
	 * typically this will be the entire packet, but we
	 * handle the case of receiving a partial duplicate segment.
	 * Do this by shortening the data length we're going to
	 * copy and adjusting the buffer pointer to point past the
	 * duplicate data.  Normally this won't do anything.
	 */

	reallen = endseqnum - tcb->tcb_rcvnext;
	bp += (tcb->tcb_rcvnext - seqnum);

	if (reallen != datalen) {
	    DEBUGMSG(("newdata(%d) does not match real length(%d)\n",reallen,datalen));
	    }

	if (reallen > 0) {

	    /*
	     * Copy the data into the receive buffer.  In the
	     * unlikely event that it doesn't fit, update the
	     * length so we'll only ack what we took.
	     */

	    reallen = tmb_copyin(&(tcb->tcb_rxbuf),bp,reallen,TRUE);

	    tcb->tcb_rcvnext += reallen;

	    /*
	     * Set the delayed-ack flag.  If it's already set,
	     * then we've already received some data without acking
	     * it, so send the ack now, to encourage us to
	     * ack every other segment at least.
	     */

	    if (tcb->tcb_flags & TCB_FLG_DLYACK) {
		_tcp_preparectlmsg(tcb,TCPFLG_ACK);
		}
	    else {
		tcb->tcb_flags |= TCB_FLG_DLYACK;
		}
	    }
	}

    /*
     * Handle the case of data in a FIN packet.
     */

    if (flags & TCPFLG_FIN) {

	tcb->tcb_rcvnext++;		/* Consume the FIN */

	DEBUGMSG(("FIN rcvd in %d ack %u\n",tcb->tcb_state,tcb->tcb_rcvnext));

	switch (tcb->tcb_state) {
	    case TCPSTATE_ESTABLISHED:

		/*
		 * If a FIN is received in the ESTABLISHED state,
		 * go to CLOSE_WAIT.  
		 */

		tcb->tcb_flags &= ~TCB_FLG_DLYACK;
		_tcp_setstate(tcb,TCPSTATE_CLOSE_WAIT);
		_tcp_sendctlmsg(ti,tcb,TCPFLG_ACK,-1);
		break;

	    case TCPSTATE_FINWAIT_1:

		/*
		 * Two choices: Either we got a FIN or a FIN ACK.
		 * In either case, send an ack.  The difference
		 * is what state we end up in.
		 */

		if (flags & TCPFLG_ACK) {
		    /* Received: FIN ACK - go directly to TIME_WAIT */
		    _tcp_canceltimers(tcb);
		    _tcp_preparectlmsg(tcb,TCPFLG_ACK);
		    _tcp_setstate(tcb,TCPSTATE_TIME_WAIT);
		    TIMER_SET(tcb->tcb_timer_2msl,TCP_TIMEWAIT_TIMER);	
		    }
		else {
		    /* Received: FIN - simultaneous close, go to CLOSING */
		    _tcp_setstate(tcb,TCPSTATE_CLOSING);
		    _tcp_preparectlmsg(tcb,TCPFLG_ACK);
		    }
		break;

	    case TCPSTATE_FINWAIT_2:

		/*
		 * Received a FIN in FINWAIT_2 - just transmit
		 * an ack and go to TIME_WAIT.
		 */

		_tcp_canceltimers(tcb);
		_tcp_preparectlmsg(tcb,TCPFLG_ACK);
		_tcp_setstate(tcb,TCPSTATE_TIME_WAIT);
		TIMER_SET(tcb->tcb_timer_2msl,TCP_TIMEWAIT_TIMER);
		break;
	    }
	}

}

/*  *********************************************************************
    *  _tcp_rx_callback(ref,buf,destaddr,srcaddr)
    *  
    *  The IP layer calls this routine when a TCP packet is received.
    *  We dispatch the packet to appropriate handlers from here.
    *  
    *  Input parameters: 
    *  	   ref - our tcp information (held by the ip stack for us)
    *  	   buf - the ebuf that we received
    *  	   destaddr,srcaddr - destination and source IP addresses
    *  	   
    *  Return value:
    *  	   ETH_DROP or ETH_KEEP, depending if we keep the packet or not
    ********************************************************************* */

static int _tcp_rx_callback(void *ref,ebuf_t *buf,uint8_t *destaddr,uint8_t *srcaddr)
{
    uint8_t pseudoheader[12];
    int tcplen;
    uint16_t calccksum;
    uint16_t origcksum;
    uint8_t *tcphdr;
    uint16_t srcport;
    uint16_t dstport;
    uint32_t seqnum;
    uint32_t acknum;
    uint16_t window;
    uint16_t flags;
    tcb_t *tcb;
    tcp_info_t *ti = (tcp_info_t *) ref;

    /*
     * get a pointer to the TCP header
     */

    tcplen = ebuf_length(buf);
    tcphdr = ebuf_ptr(buf);

    /* 
     * construct the pseudoheader for the cksum calculation
     */

    memcpy(&pseudoheader[0],srcaddr,IP_ADDR_LEN);
    memcpy(&pseudoheader[4],destaddr,IP_ADDR_LEN);
    pseudoheader[8] = 0;
    pseudoheader[9] = IPPROTO_TCP;
    pseudoheader[10] = (tcplen >> 8) & 0xFF;
    pseudoheader[11] = (tcplen & 0xFF);

    origcksum  = ((uint16_t) tcphdr[16] << 8) | (uint16_t) tcphdr[17];
    tcphdr[16] = tcphdr[17] = 0;

    calccksum = ip_chksum(0,pseudoheader,sizeof(pseudoheader));
    calccksum = ip_chksum(calccksum,tcphdr,tcplen);
    calccksum = ~calccksum;

    if (calccksum != origcksum) {
	return ETH_DROP;
	}

    /* Read the other TCP header fields from the packet */

    ebuf_get_u16_be(buf,srcport);
    ebuf_get_u16_be(buf,dstport);
    ebuf_get_u32_be(buf,seqnum);
    ebuf_get_u32_be(buf,acknum);
    ebuf_get_u16_be(buf,flags);
    ebuf_get_u16_be(buf,window);
    ebuf_skip(buf,4);		/* skip checksum and urgent pointer */

    /* Skip options in header */
    if (TCPHDRSIZE(flags) < TCP_HDR_LENGTH) {
	return ETH_DROP;
	}
    ebuf_skip(buf,(TCPHDRSIZE(flags) - TCP_HDR_LENGTH));


    tcb = _tcp_find_tcb(ti,srcport,dstport,srcaddr);
    if (!tcb) {
	DEBUGMSG(("Invalid TCB from %I, srcport=%u dstport=%u\n",srcaddr,srcport,dstport));
	_tcp_sendreset(ti,srcaddr,seqnum+1,dstport,srcport);
	return ETH_DROP;		/* drop packet if no matching port */
	}

    /*
     * Any activity on a TCB is enough to reset the keepalive timer
     */

    if (TCPSTATE_IN_SET(tcb->tcb_state,M_TCPSTATE_RESETKEEPALIVE)) {
	TIMER_SET(tcb->tcb_timer_keep,TCP_KEEPALIVE_TIMER);
	}

    /*
     * Some debugging 
     */

#ifdef _TCP_DEBUG_
	if (_tcp_dumpflags) _tcp_dumppktstate("Received",flags,
					      acknum,seqnum,ebuf_length(buf));
#endif

    /*
     * If someone tries to reset us, just kill off the port.
     * (except: if we were in SYN_RECEIVED, go back to LISTEN)
     */

    if (flags & TCPFLG_RST) {
	if (tcb->tcb_state == TCPSTATE_SYN_RECEIVED) {
	    _tcp_setstate(tcb,TCPSTATE_LISTEN);
	    }
	else {
	    _tcp_closetcb(ti,tcb);
	    }
	return ETH_DROP;
	}

    /*
     * Remember the window we got.
     */

    tcb->tcb_sendwindow = window;

    /*
     * What we do here depends on the current connection state
     * Most of this is just from the connection state diagram we've
     * all see way too often.
     */

    switch ( tcb->tcb_state ) {

	case TCPSTATE_LISTEN:
	    if (flags & TCPFLG_SYN) {
		tcb->tcb_rcvnext = seqnum + 1;
		tcb->tcb_peerport = srcport;
		memcpy(tcb->tcb_peeraddr,srcaddr,IP_ADDR_LEN);
		TIMER_SET(tcb->tcb_timer_keep,TCP_KEEPALIVE_TIMER);
		_tcp_setstate(tcb,TCPSTATE_SYN_RECEIVED);
		_tcp_sendctlmsg(ti,tcb,TCPFLG_SYN | TCPFLG_ACK,TCP_RETX_TIMER);
		}
	    return ETH_DROP;		/* we ignore everything else */
	    break;

	case TCPSTATE_SYN_SENT:

	    /*
	     * The only packets we pay attention to in SYN_SENT
	     * are packets with SYN flags.
	     */
	    if (flags & TCPFLG_SYN) {

		/*
		 * Two choices: Either we got a SYN ACK (normal)
		 * or just a SYN (simultaneous open, rare)
		 */

		if ((flags & TCPFLG_ACK) && 
		    (acknum == (tcb->tcb_sendunack + 1))) {
		    /*
		     * If we received a SYN ACK and the acknum
		     * matches our SYN's seqnum + 1, we want 
		     * to transition from SYN_SENT to ESTABLISHED.
		     */
		    TIMER_SET(tcb->tcb_timer_keep,TCP_KEEPALIVE_TIMER);
		    _tcp_setstate(tcb,TCPSTATE_ESTABLISHED);
		    tcb->tcb_sendunack = acknum;
		    tcb->tcb_sendnext = tcb->tcb_sendunack;
		    tcb->tcb_rcvnext = seqnum + 1;
		    /* 
		     * Send an ack, but don't bother with the timer.
		     * If the remote does not get our ack, it will
		     * retransmit the SYN ACK and we'll ack it from
		     * the "ESTABLISHED" state.
		     * Actually, is that really true?
		     */
		    _tcp_sendctlmsg(ti,tcb,TCPFLG_ACK,0); 
		    } 
		else {
		    DEBUGMSG(("Simultaneous open: SYN received in SYN_SENT state\n"));
		    tcb->tcb_rcvnext++;		   /* consume the SYN */
		    _tcp_setstate(tcb,TCPSTATE_SYN_RECEIVED);
		    _tcp_sendctlmsg(ti,tcb,TCPFLG_SYN|TCPFLG_ACK,TCP_RETX_TIMER); 
		    }
		}

	    return ETH_DROP;
	    break;

	case TCPSTATE_SYN_RECEIVED:
	    /*
	     * If we've received a SYN and someone sends us a SYN,
	     * and the sequence numbers don't match,
	     * send back a SYN ACK.  (this doesn't sound right, 
	     * does it?  It's sort of what we do from LISTEN)
	     */

	    if (flags & TCPFLG_SYN) {
		_tcp_sendctlmsg(ti,tcb,TCPFLG_SYN | TCPFLG_ACK,TCP_RETX_TIMER);
		}

	    /*
	     * If we got an ack and the acknum is correct,
	     * switch the state to 'ESTABLISHED' and cancel
	     * the timer.  We're ready to rock.
	     */

	    if ((flags & TCPFLG_ACK) && 
		(acknum == (tcb->tcb_sendunack + 1))) {
		_tcp_sendctlmsg(ti,tcb,TCPFLG_ACK,0);
		tcb->tcb_sendunack = acknum;	
		tcb->tcb_sendnext = tcb->tcb_sendunack;
		TIMER_SET(tcb->tcb_timer_keep,TCP_KEEPALIVE_TIMER);
		_tcp_setstate(tcb,TCPSTATE_ESTABLISHED);
		}

	    return ETH_DROP;
	    break;

	case TCPSTATE_ESTABLISHED:
	case TCPSTATE_FINWAIT_1:
	case TCPSTATE_FINWAIT_2:

	    /*
	     * ESTABLISHED, FINWAIT_1, and FINWAIT_2 can all
	     * carry data.  Process the data here.  If the
	     * segment also contains a FIN, these routines
	     * will handle that.
	     */

	    if (flags & TCPFLG_ACK) {
		_tcp_procack(ti,tcb,acknum,flags);
		}
	    _tcp_procdata(ti,tcb,seqnum,flags,buf);
	    break;

	case TCPSTATE_CLOSING:
	    if (acknum == (tcb->tcb_sendunack + 1)) {
		_tcp_setstate(tcb,TCPSTATE_TIME_WAIT);
		_tcp_canceltimers(tcb);
		TIMER_SET(tcb->tcb_timer_2msl,TCP_TIMEWAIT_TIMER);
		}
	    break;

	case TCPSTATE_LAST_ACK:
	    if (acknum == (tcb->tcb_sendunack + 1)) {
		/* Ack matches, just close the TCB here. */
		_tcp_closetcb(ti,tcb);
		/* and free it. */
		_tcp_freetcb(ti,tcb);
		} 
	    else {
		_tcp_sendctlmsg(ti,tcb,TCPFLG_ACK | TCPFLG_FIN,TCP_RETX_TIMER);
		}
	    break;

	case TCPSTATE_TIME_WAIT:	
	    if (!(flags & TCPFLG_RST)) {
		tcb->tcb_txflags = TCPFLG_ACK;
		TIMER_SET(tcb->tcb_timer_2msl,TCP_TIMEWAIT_TIMER);
		_tcp_protosend(ti,tcb);
		}
	    break;
	}

    /*
     * If we're expected to transmit something, do it now.
     */

    if (tcb->tcb_flags & TCB_FLG_OUTPUT) {
	_tcp_output(ti,tcb);
	tcb->tcb_flags &= ~(TCB_FLG_OUTPUT | TCB_FLG_SENDMSG);
	}

    if (tcb->tcb_flags & TCB_FLG_SENDMSG) {
	_tcp_protosend(ti,tcb);
	}

    /* We always make a copy of the data, so drop the packet. */
    return ETH_DROP;
}

/*  *********************************************************************
    *  _tcp_fasttimo(ti)
    *  
    *  Handle "fast timeout" for active TCP sockets.
    *  
    *  Input parameters: 
    *  	   ti - tcp_info structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
static void _tcp_fasttimo(tcp_info_t *ti)
{
    tcb_t *tcb;
    queue_t *qb;

    /*
     * First, reset the timer so we'll end up here
     * again in another 200ms
     */

    TIMER_SET(ti->ti_fasttimer,TCP_FAST_TIMER);		/* 200ms */

    /*
     * Now, walk down the list of TCBs and handle
     * all the timers.
     */

    for (qb = ti->ti_tcblist.q_next; qb != &(ti->ti_tcblist); qb = qb->q_next) {
	tcb = (tcb_t *) qb;
	if (tcb->tcb_flags & TCB_FLG_DLYACK) {
	    tcb->tcb_flags &= ~TCB_FLG_DLYACK;
	    _tcp_sendctlmsg(ti,tcb,TCPFLG_ACK,-1);
	    }
	}

    /*
     * While we're here, increment TCP's initial sequence number.
     * BSD suggests 128000 every second, so we'll add 25600 every 200ms.
     */

    ti->ti_iss += 25600;

}


/*  *********************************************************************
    *  _tcp_poll(arg)
    *  
    *  CFE calls this routine periodically to allow us to check
    *  and update timers, etc.
    *  
    *  Input parameters: 
    *  	   arg - our tcp information (held by the timer module for us)
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void _tcp_poll(void *arg)
{
    tcb_t *tcb;
    queue_t *qb;
    tcp_info_t *ti = (tcp_info_t *) arg;

    /*
     * Handle the "fast" timer.  We do this every 200ms
     */

    if (TIMER_EXPIRED(ti->ti_fasttimer)) {
	_tcp_fasttimo(ti);
	/* timer will be reset by the _tcp_fasttimo routine */
	}

    /*
     * Check the TCBs for the "slow" timers.  
     */

    for (qb = ti->ti_tcblist.q_next; qb != &(ti->ti_tcblist); qb = qb->q_next) {
	tcb = (tcb_t *) qb;


	if (TIMER_EXPIRED(tcb->tcb_timer_retx)) {
	    TIMER_CLEAR(tcb->tcb_timer_retx);		/* unless it is reset */

	    DEBUGMSG(("Retransmit timer expired, retrycnt=%d\n",tcb->tcb_retrycnt));

	    switch (tcb->tcb_state) {
		case TCPSTATE_ESTABLISHED:
		    /* 
		     * wind the send seqnum back to the last unacknowledged data,
		     * and send it out again.
		     */
		    TIMER_SET(tcb->tcb_timer_retx,TCP_RETX_TIMER);
		    tcb->tcb_retrycnt++;
		    tcb->tcb_sendnext = tcb->tcb_sendunack;
		    _tcp_output(ti,tcb);
		    break;

		case TCPSTATE_SYN_SENT:
		    TIMER_SET(tcb->tcb_timer_retx,(TCP_RETX_TIMER << tcb->tcb_retrycnt));
		    tcb->tcb_retrycnt++;
		    tcb->tcb_sendnext = tcb->tcb_sendunack;
		    _tcp_protosend(ti,tcb);
		    break;

		}
	    }

	/*
	 * Check the keepalive timer.  This is used during connection
	 * attempts to time out the connection, and _can_ be used for
	 * keepalives during established sessions.
	 *
	 * Our TCP does not bother with keepalive messages.
	 */

	if (TIMER_EXPIRED(tcb->tcb_timer_keep)) {
	    TIMER_CLEAR(tcb->tcb_timer_keep);		/* unless it is reset */
	    DEBUGMSG(("Keepalive timer expired in state %d\n",tcb->tcb_state));
	    if (TCPSTATE_IN_SET(tcb->tcb_state,M_TCPSTATE_CONNINPROGRESS)) {
		DEBUGMSG(("Canceling pending connection\n"));
		_tcp_aborttcb(ti,tcb);
		}
	    }

	/*
	 * Check the 2MSL timer.  This is used in the TIME_WAIT state
	 * to return the tcb to the free list after the time wait
	 * period elapses.
	 */

	if (TIMER_EXPIRED(tcb->tcb_timer_2msl)) {
	    TIMER_CLEAR(tcb->tcb_timer_2msl);		/* will not be reset */
	    DEBUGMSG(("2MSL timer expired in state %d\n",tcb->tcb_state));
	    if (tcb->tcb_state == TCPSTATE_TIME_WAIT) {
		DEBUGMSG(("Freeing TCB\n"));
		_tcp_closetcb(ti,tcb);
		_tcp_freetcb(ti,tcb);
		}
	    }

	}

}
