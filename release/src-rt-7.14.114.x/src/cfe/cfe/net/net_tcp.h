/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  TCP Protocol Definitions			File: net_tcp.h
    *  
    *  This file contains TCP protocol-specific definitions
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
    *  TCP Flags - keep in sync with net_api.h
    ********************************************************************* */

#define TCPFLG_NODELAY	1		/* disable nagle */
#define TCPFLG_NBIO	2		/* Non-blocking I/O */

#define TCPSTATUS_NOTCONN	0
#define TCPSTATUS_CONNECTING	1
#define TCPSTATUS_CONNECTED	2

/*  *********************************************************************
    *  TCP API
    ********************************************************************* */

typedef struct tcp_info_s tcp_info_t;

tcp_info_t *_tcp_init(ip_info_t *ipi,void *ref);
void _tcp_uninit(tcp_info_t *info);

int _tcp_socket(tcp_info_t *info);
int _tcp_connect(tcp_info_t *ti,int s,uint8_t *dest,uint16_t port);
int _tcp_close(tcp_info_t *ti,int s);
int _tcp_send(tcp_info_t *ti,int s,uint8_t *buf,int len);
int _tcp_recv(tcp_info_t *ti,int s,uint8_t *buf,int len);
int _tcp_bind(tcp_info_t *ti,int s,uint16_t port);
int _tcp_peeraddr(tcp_info_t *ti,int s,uint8_t *addr,uint16_t *port);
int _tcp_listen(tcp_info_t *ti,int s,uint16_t port);
int _tcp_status(tcp_info_t *ti,int s,int *connflag,int *rxready,int *rxeof);
int _tcp_debug(tcp_info_t *ti,int s,int arg);
int _tcp_setflags(tcp_info_t *ti,int s,unsigned int flags);
int _tcp_getflags(tcp_info_t *ti,int s,unsigned int *flags);

void _tcp_poll(void *arg);
