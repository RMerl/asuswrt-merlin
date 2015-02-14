/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  TCP and HTTP Protocol Definitions		File: net_http.h
    *  
    *  This file contains TCP and HTTP protocol-specific definitions
    *  
    *  Author:  
    *  
    *********************************************************************  
    *
    *  Copyright 2000~2008
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

#ifndef	_NET_HTTP_H
#define	_NET_HTTP_H

/*  *********************************************************************
    *  TCP Constants
    ********************************************************************* */


#define TCP_MAX_PORTS	1
#define TCP_MAX_TCBS	16
#define TCP_BUF_SIZE	65536

#define TCP_MAX_SEG_SIZE 1400

/*  *********************************************************************
    *  TCP Protocol
    ********************************************************************* */

#define TCPFLG_FIN	0x0001
#define TCPFLG_SYN	0x0002
#define TCPFLG_RST	0x0004
#define TCPFLG_PSH	0x0008
#define TCPFLG_ACK	0x0010
#define TCPFLG_URG	0x0020
#define TCPHDRSIZE(flg)	(((flg) >> 12)*4)
#define TCPHDRFLG(size) (((size)/4)<<12)

#define TCP_HDR_LENGTH	20

#define TCP_MAX_SEG_OPT	0x0204

/*  *********************************************************************
    *  TCP State machine
    ********************************************************************* */

/* 
 * TCB states, see RFC 
 */

#define TCPSTATE_CLOSED		0
#define TCPSTATE_LISTEN		1
#define TCPSTATE_SYN_SENT	2
#define TCPSTATE_SYN_RECEIVED	3
#define TCPSTATE_ESTABLISHED	4
#define TCPSTATE_CLOSE_WAIT	5
#define TCPSTATE_FINWAIT_1	6
#define TCPSTATE_FINWAIT_2	7
#define TCPSTATE_CLOSING	8
#define TCPSTATE_LAST_ACK	9
#define TCPSTATE_TIME_WAIT	10

/*  *********************************************************************
    *  TCP Control Block
    ********************************************************************* */

typedef struct tcb_s {
    int	tcb_state;			/* current connection state */

    uint8_t tcb_peeraddr[IP_ADDR_LEN];	/* Peer's IP Address */
    uint16_t tcb_peerport;		/* Peer's port address */
    uint16_t tcb_lclport;		/* My port address */

    uint16_t tcb_txflags;		/* packet flags for next tx packet */
    uint16_t rxflag;		        /* packet flags for receive packet */
    char *tcb_txbuf;			/* Transmit buffer */
    uint32_t txlen;
    char *tcb_rxbuf;			/* Receive buffer */
    uint32_t rxlen;

    unsigned int tcb_flags;		/* Misc protocol flags */

    /*
     * Send sequence variables
     */
    uint32_t tcb_sendnext;		/* Next seqnum to send */
    uint32_t tcb_sendwindow;		/* Last advertised send window */

    /*
     * Receive sequence variables
     */
    uint32_t tcb_rcvnext;		/* next in-order receive seq num */

    /*
     * Window management variables
     */
    uint32_t wait_ack;

} tcb_t;

#ifdef _TCP_DEBUG_
#define _tcp_setstate(tcb,state) (tcb)->tcb_state = (state); \
                                 printf("tcp state = " #state "\n");
#define DEBUGMSG(x) printf x
#else
#define _tcp_setstate(tcb,state) (tcb)->tcb_state = (state);
#define DEBUGMSG(x)
#endif	/* _TCP_DEBUG_ */

/*  *********************************************************************
    *  HTTP API
    ********************************************************************* */

typedef struct http_info_s http_info_t;

http_info_t *_tcphttp_init(ip_info_t *ipi,void *ref);
void _tcphttp_uninit(http_info_t *info);

#endif	/* _NET_HTTP_H */
