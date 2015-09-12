/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  HTTP Protocol 				File: net_http.c
    *  
    *  This file contains a very simple TCP and HTTP.  The basic goals of this
    *  tcp are to be "good enough for firmware."  We try to be 
    *  correct in our protocol implementation, but not very fancy.
    *  In particular, we don't deal with out-of-order segments,
    *  we don't hesitate to copy data more then necessary, etc.
    *  We strive to implement important protocol features
    *  like slow start, nagle, etc., but even these things are
    *  subsetted and simplified as much as possible.
    *
    *  Author:
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
#include "lib_scanf.h"
#include <bcmendian.h>
#include "net_ebuf.h"
#include "net_ether.h"

#include "net_ip.h"
#include "net_ip_internal.h"

#include "cfe_iocb.h"
#include "cfe_devfuncs.h"
#include "cfe_ioctl.h"
#include "cfe_timer.h"
#include "cfe_error.h"
#include "cfe.h"
#include "cfe_flashimage.h"

#include "net_http.h"
#include <trxhdr.h>

#ifndef	NULL
#define NULL (void *)0
#endif

/* The HTTP server states: */
#define HTTP_NOT_READY    0
#define HTTP_NOGET        1
#define HTTP_FILE         2
#define HTTP_TEXT         3
#define HTTP_FUNC         4
#define HTTP_END          5
#define	HTTP_UPLOAD_BOOT  6
#define	HTTP_UPLOAD_EXE	  7

#ifdef DEBUG
#define PRINT(x)	printf("%s", x)
#define PRINTLN(x)	printf("%s\n", x)
#else
#define PRINT(x)
#define PRINTLN(x)
#endif /* DEBUG */


struct http_info_s {
	void *ti_ref;			/* ref data for IP layer */
	ip_info_t *ti_ipinfo;		/* IP layer handle */
	tcb_t tcb;			/* TCB */
	uint32_t ti_iss;		/* initial sequence number */
};

#define	UL_HEADER	0
#define	UL_MULTIPART	1
#define	UL_UPDATE	2
struct httpd_state {
	unsigned char state;
	unsigned char ul_state;
	struct http_info_s *ti;
	tcb_t *tcb;
	char eval[256];
	char *uip_appdata;
	int uip_len;
	char page_buf[1024];
	int page_len;
	char *load_addr;
	int  load_limit;
	int ul_len;
	int ul_cnt;
	int ul_offset;
};

static struct httpd_state httpd_state;


#define ISO_G        0x47
#define ISO_E        0x45
#define ISO_T        0x54
#define ISO_slash    0x2f
#define ISO_c        0x63
#define ISO_g        0x67
#define ISO_i        0x69
#define ISO_space    0x20
#define ISO_nl       0x0a
#define ISO_cr       0x0d
#define ISO_a        0x61
#define ISO_t        0x74
#define ISO_hash     0x23
#define ISO_period   0x2e

/* Forward Declarations */
static int _tcphttp_rx_callback(void *ref, ebuf_t *buf, uint8_t *destaddr, uint8_t *srcaddr);
static void _tcphttp_protosend(http_info_t *ti, tcb_t *tcb);

static int httpd_accept(struct httpd_state *hs, http_info_t *ti, tcb_t *tcb);
static int httpd_appcall(struct httpd_state *hs);
static void httpd_defercall(struct httpd_state *hs);
static void httpd_page_init(struct httpd_state *hs);
static void httpd_page_end(struct httpd_state *hs, int rc, int f);
static int httpd_do_cmd(struct httpd_state *hs);
static int httpd_load_program(struct httpd_state *hs);
static int httpd_do_ul(struct httpd_state *hs);
static void httpd_printf(struct httpd_state *hs, char *templat, ...);
static int trx_validate(uint8_t *ptr, int *insize);
static int httpd_write_flash(char *flashdev, uint8 *load_addr, int len);

extern int ui_docommands(char *buf);
extern int flash_validate(uint8_t *ptr, int bufsize, int insize, uint8_t **outptr, int *outsize);
extern void ui_get_flash_buf(uint8_t **bufptr, int *bufsize);
extern unsigned int flash_crc32(const unsigned char *databuf, unsigned int  datalen);

/*
 * _tcp_init(ipi,ref)
 *
 * Initialize the TCP module.  We set up our data structures
 * and register ourselves with the IP layer.
 *
 * Input parameters:
 *  	ipi - IP information
 *      ref - will be passed back to IP as needed
 *
 *  Return value:
 *  	http_info_t structure, or NULL if problems
 */
http_info_t *
_tcphttp_init(ip_info_t *ipi, void *ref)
{
	extern int32_t _getticks(void);		/* return value of CP0 COUNT */
	http_info_t *ti = NULL;
	tcb_t *tcb = NULL;

	ti = (http_info_t *)KMALLOC(sizeof(http_info_t), 0);
	if (!ti)
		return NULL;

	ti->ti_ref = ref;
	ti->ti_ipinfo = ipi;

	/*
	* Initialize the TCB
	*/
	tcb = &ti->tcb;
	memset(tcb, 0, sizeof(tcb_t));

	/*
	* Set up initial state.
	*/
	_tcp_setstate(tcb, TCPSTATE_CLOSED);

	/*
	* Set up the initial sequence number
	*/
	ti->ti_iss = (uint32_t)_getticks();

	/*
	* Register our protocol with IP
	*/

	_ip_register(ipi, IPPROTO_TCP, _tcphttp_rx_callback, ti);

	httpd_state.state = HTTP_NOGET;
	return ti;
}

/*
 * _tcp_uninit(info)
 *
 * De-initialize the TCP layer, unregistering from the IP layer.
 *
 * Input parameters:
 *     info - our http_info_t, from _tcp_init()
 *
 *  Return value:
 *     nothing
 */
void
_tcphttp_uninit(http_info_t *info)
{
	/*
	* Deregister with IP
	*/
	_ip_deregister(info->ti_ipinfo, IPPROTO_TCP);

	/*
	* Free up the info structure
	*/
	KFREE(info);

	/* Clear protocol */
	memset(&httpd_state, 0, sizeof(httpd_state));
}


/*
 * _tcp_protosend(ti,tcb)
 *
 * Transmit "protocol messages" on the tcb.  This is used for
 * sending SYN, FIN, ACK, and other control packets.
 *
 * Input parameters:
 *      ti - tcp infomration
 *      tcb - tcb we're interested in
 *
 * Return value:
 *      nothing
 */
static void
_tcphttp_protosend(http_info_t *ti, tcb_t *tcb)
{
	ebuf_t *b;
	uint16_t flags;
	uint8_t *cksumptr;
	uint8_t *tcphdr;
	uint16_t cksum;
	int hdrlen;
	uint8_t pseudoheader[12];
	int tcplen = 0;

	/*
	* Allocate a buffer and remember the pointer to where
	* the header starts.
	*/
	b = _ip_alloc(ti->ti_ipinfo);
	if (!b)
		return;

	tcphdr = ebuf_ptr(b) + ebuf_length(b);

	/*
	* Build the TCP header
	 */
	flags = tcb->tcb_txflags | (TCPHDRFLG(TCP_HDR_LENGTH));
	hdrlen = TCP_HDR_LENGTH;

	/*
	* Fill in the fields according to the RFC.
	*/
	ebuf_append_u16_be(b, tcb->tcb_lclport);	/* Local and remote ports */
	ebuf_append_u16_be(b, tcb->tcb_peerport);
	ebuf_append_u32_be(b, tcb->tcb_sendnext);	/* sequence and ack numbers */
	ebuf_append_u32_be(b, tcb->tcb_rcvnext);
	ebuf_append_u16_be(b, flags);			/* Flags */
	ebuf_append_u16_be(b, tcb->tcb_sendwindow);	/* Window size */
	cksumptr = ebuf_ptr(b) + ebuf_length(b);
	ebuf_append_u16_be(b, 0);			/* dummy checksum for calculation */
	ebuf_append_u16_be(b, 0);			/* Urgent Pointer (not used) */

	tcplen = tcb->txlen;
	if (tcb->txlen > 0) {
		memcpy(b->eb_ptr + b->eb_length, tcb->tcb_txbuf, tcb->txlen);
		b->eb_length += tcb->txlen;
		tcb->tcb_txbuf = NULL;
		tcb->txlen = 0;
	}

	/*
	 * Build the pseudoheader, which is part of the checksum calculation
	 */
	_ip_getaddr(ti->ti_ipinfo, &pseudoheader[0]);
	memcpy(&pseudoheader[4], tcb->tcb_peeraddr, IP_ADDR_LEN);
	pseudoheader[8] = 0;
	pseudoheader[9] = IPPROTO_TCP;
	pseudoheader[10] = ((tcplen+hdrlen) >> 8) & 0xFF;
	pseudoheader[11] = ((tcplen+hdrlen) & 0xFF);

	/*
	 * Checksum the packet and insert the checksum into the header
	 */
	cksum = ip_chksum(0, pseudoheader, sizeof(pseudoheader));
	cksum = ip_chksum(cksum, tcphdr, tcplen + hdrlen);
	cksum = ~cksum;
	cksumptr[0] = (cksum >> 8) & 0xFF;
	cksumptr[1] = (cksum & 0xFF);

	/*
	 * Transmit the packet.  The layer below us will free it.
	 */
	_ip_send(ti->ti_ipinfo, b, tcb->tcb_peeraddr, IPPROTO_TCP);
}

/*
 * _tcphttp_rx_callback(ref,buf,destaddr,srcaddr)
 *
 * The IP layer calls this routine when a TCP packet is received.
 * We dispatch the packet to appropriate handlers from here.
 *
 * Input parameters:
 *         ref - our tcp information (held by the ip stack for us)
 *         buf - the ebuf that we received
 *         destaddr,srcaddr - destination and source IP addresses
 *
 *  Return value:
 *         ETH_DROP or ETH_KEEP, depending if we keep the packet or not
 */
static int
_tcphttp_rx_callback(void *ref, ebuf_t *buf, uint8_t *destaddr, uint8_t *srcaddr)
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
	http_info_t *ti = (http_info_t *)ref;
	tcb_t *tcb = &ti->tcb;
	uint32_t tt;
	uint8_t *bp;
	int t = -1;
	int plen = 0;

	if (httpd_state.state == HTTP_NOT_READY)
		return ETH_DROP;
	/*
	 * get a pointer to the TCP header
	 */
	tcplen = ebuf_length(buf);
	tcphdr = ebuf_ptr(buf);

	/* 
	 * construct the pseudoheader for the cksum calculation
	 */
	memcpy(&pseudoheader[0], srcaddr, IP_ADDR_LEN);
	memcpy(&pseudoheader[4], destaddr, IP_ADDR_LEN);
	pseudoheader[8] = 0;
	pseudoheader[9] = IPPROTO_TCP;
	pseudoheader[10] = (tcplen >> 8) & 0xFF;
	pseudoheader[11] = (tcplen & 0xFF);

	origcksum  = ((uint16_t)tcphdr[16] << 8) | (uint16_t)tcphdr[17];
	tcphdr[16] = tcphdr[17] = 0;

	calccksum = ip_chksum(0, pseudoheader, sizeof(pseudoheader));
	calccksum = ip_chksum(calccksum, tcphdr, tcplen);
	calccksum = ~calccksum;
	if (calccksum != origcksum)
		return ETH_DROP;

	/* Read the other TCP header fields from the packet */
	ebuf_get_u16_be(buf, srcport);
	ebuf_get_u16_be(buf, dstport);
	if (dstport != 80) {
		DEBUGMSG(("TCP dport is not 80\n"));
		return ETH_DROP;
	}

	ebuf_get_u32_be(buf, seqnum);
	ebuf_get_u32_be(buf, acknum);
	ebuf_get_u16_be(buf, flags);
	ebuf_get_u16_be(buf, window);

	/* skip checksum and urgent pointer */
	ebuf_skip(buf, 4);

	/* Skip options in header */
	if (TCPHDRSIZE(flags) < TCP_HDR_LENGTH)
		return ETH_DROP;

	ebuf_skip(buf, (TCPHDRSIZE(flags) - TCP_HDR_LENGTH));

	switch (tcb->tcb_state) {
	case TCPSTATE_CLOSED:
		if (flags & TCPFLG_SYN) {
rec_syn:
			if (tcb->tcb_txbuf) {
				tcb->tcb_txbuf = NULL;
				tcb->txlen = 0;
			}
			if (tcb->tcb_rxbuf) {
				KFREE(tcb->tcb_rxbuf);
				tcb->tcb_rxbuf = NULL;
				tcb->rxlen = 0;
			}
			tcb->tcb_txflags = TCPFLG_SYN | TCPFLG_ACK;
			tcb->tcb_rcvnext = seqnum + 1;
			tcb->tcb_sendnext = ti->ti_iss;
			tcb->tcb_lclport = dstport;
			tcb->tcb_peerport = srcport;
			_tcp_setstate(tcb, TCPSTATE_SYN_SENT);
			tcb->wait_ack = 1;

			memcpy(tcb->tcb_peeraddr, srcaddr, IP_ADDR_LEN);

			tcb->tcb_sendwindow = 1500;
			_tcphttp_protosend(ti, tcb);
		}
		break;

	case TCPSTATE_SYN_SENT:
		if (flags & TCPFLG_ACK) {
			if (tcb->tcb_rcvnext != seqnum)
				goto send_ack_nodata;

			tt = tcb->tcb_sendnext;
			tt += tcb->wait_ack;
			if (acknum == tt) {
				tcb->tcb_sendnext = tt;
				tcb->wait_ack = 0;
				httpd_accept(&httpd_state, ti, tcb);
				_tcp_setstate(tcb, TCPSTATE_ESTABLISHED);
			}
		}
		else if (flags & TCPFLG_SYN)
			goto rec_syn;
		break;

	case TCPSTATE_ESTABLISHED:
		/* swap src,dst port */
		if (srcport != tcb->tcb_peerport) {
			break;
		}
		if (tcb->tcb_rcvnext != seqnum) {
			printf("tcb->tcb_rcvnext=%08x seqnum=%08x\n", tcb->tcb_rcvnext, seqnum);
			goto send_ack_nodata;
		}

		tt = tcb->tcb_sendnext;
		tt += tcb->wait_ack;
		if (acknum != tt) {
			printf("acknum=%x expect=%08x\n", acknum, tt);
			break;
		}
		else {
			tcb->tcb_sendnext = tt;
			tcb->rxflag = flags;

			/*
			 * Figure out how much we're going to take.  This should not
			 * exceed our buffer size because we advertised a window
			 * only big enough to fill our buffer.
			 */
			bp = ebuf_ptr(buf);	/* pointer to TCP data */
			tt = ebuf_remlen(buf);	/* Size of TCP data */
			tcb->rxlen = tt;

			tcb->tcb_rcvnext += tt;
			tcb->tcb_rxbuf = KMALLOC(tt, 0);
			memcpy(tcb->tcb_rxbuf, bp, tt);

			t = httpd_appcall(&httpd_state);
			if (t != 0) {
				tcb->tcb_txflags = TCPFLG_ACK|TCPFLG_FIN;
				tcb->wait_ack = 1;
				_tcp_setstate(tcb, TCPSTATE_FINWAIT_1);
				goto send_ack_nodata;
			}

			plen = tcb->txlen;
			tcb->wait_ack = plen;
			tcb->tcb_txflags = TCPFLG_ACK;

			if (tcb->tcb_state == TCPSTATE_FINWAIT_1) {
				tcb->tcb_txflags |= TCPFLG_FIN;
			}
			goto send_ack;
		}
		break;

	case TCPSTATE_FINWAIT_1:
	default:
		_tcp_setstate(tcb, TCPSTATE_CLOSED);
		tcb->tcb_sendnext = acknum;
		tcb->tcb_txflags = TCPFLG_RST;
		goto send_ack_nodata;
	}

	if (tcb->tcb_rxbuf)
	{
		KFREE(tcb->tcb_rxbuf);
		tcb->tcb_rxbuf = NULL;
		tcb->rxlen = 0;
	}

	return ETH_DROP;

send_ack_nodata:
	plen = 0;

send_ack:
	if (plen)
		tcb->tcb_sendwindow =  0;
	else
		tcb->tcb_sendwindow = 1500;

	_tcphttp_protosend(ti, tcb);

	if (tcb->tcb_rxbuf) {
		KFREE(tcb->tcb_rxbuf);
		tcb->tcb_rxbuf = NULL;
		tcb->rxlen = 0;
	}

	httpd_defercall(&httpd_state);
	return ETH_DROP;
}

static int
httpd_accept(struct httpd_state *hs, http_info_t *ti, tcb_t *tcb)
{
	memset(hs, 0, sizeof(*hs));

	hs->state = HTTP_NOGET;
	hs->ti = ti;
	hs->tcb = tcb;
	return 0;
}

static inline void
httpd_flush(struct httpd_state *hs)
{
	tcb_t *tcb = hs->tcb;

	tcb->tcb_txbuf = hs->page_buf;
	tcb->txlen = hs->page_len;
	_tcp_setstate(tcb, TCPSTATE_FINWAIT_1);
}

static int
httpd_appcall(struct httpd_state *hs)
{
	/* Save receive buffer and length */
	hs->uip_appdata = hs->tcb->tcb_rxbuf;
	hs->uip_len = hs->tcb->rxlen;
	if (hs->uip_len <= 0)
		return 0;

	switch (hs->state) {
	case HTTP_UPLOAD_EXE:
		return httpd_do_ul(hs);	

	case HTTP_NOGET:
		if (strncmp("POST ", hs->uip_appdata, 5) == 0) {
			/* 'POST */
			if (strncmp("f2", hs->uip_appdata+6, 2) == 0) {
				hs->state = HTTP_UPLOAD_EXE;
				httpd_page_init(hs);

				/* Do http upload */
				return httpd_do_ul(hs);
			}
		}
		else if (strncmp("GET ", hs->uip_appdata, 4) == 0) {
			if (!strncmp("do", hs->uip_appdata+5, 2)) {
				hs->state = HTTP_FUNC;
				httpd_page_init(hs);

				/* Do commands */
				return httpd_do_cmd(hs);
			}
		}
		else {
			/* If it isn't a GET, we abort the connection. */
			return -1;
		}
	
		/* Send mini-web index page out */
		httpd_page_init(hs);
		httpd_printf(hs,
			"<table border=0 cellpadding=0 cellspacing=0 bgcolor=#306498>\r\n"
			"<tr><td height=57 width=600>\r\n"
			"<font face=Arial size=6 color=#ffffff>ASUSTek - CFE miniWeb Server</font>\r\n"
			"</td></tr>\r\n"
			"</table><br>\r\n"
			"<form action=f2.htm method=post encType=multipart/form-data>\r\n"
			"Firmware File&nbsp\r\n"
			"<input type=file size=35 name=files>\r\n"
			"<input type=submit value=Upload><br>\r\n"
			"</form>\r\n"
			"<form action=do.htm method=get>\r\n"
			"<br>Command:<br><a href=do.htm?cmd=reboot>Reboot.</a>\r\n"
			"<br><a href=do.htm?cmd=nvram+erase>Restore default NVRAM values.</a>\r\n"
			"</form>\r\n");
		httpd_page_end(hs, 0, 0);
		return 0;

	default:
		break;
	}

	return 0;
}

static void
httpd_defercall(struct httpd_state *hs)
{
	http_info_t *ti = hs->ti;
	tcb_t *tcb = hs->tcb;
#ifdef CFG_NFLASH
	char flashdev[12];
#else
	char *flashdev = "flash1.trx";
#endif

	if (hs->state == HTTP_UPLOAD_EXE) {
		if (hs->ul_state == UL_UPDATE) {
			/* Send reset */
			tcb->tcb_sendwindow =  0;
			_tcp_setstate(tcb, TCPSTATE_CLOSED);
			tcb->tcb_txflags = TCPFLG_RST;
			_tcphttp_protosend(ti, tcb);

#ifdef CFG_NFLASH
			ui_get_trx_flashdev(flashdev);
#endif
			/* Write to flash */
			httpd_write_flash(flashdev, (uint8 *)hs->load_addr, hs->ul_offset);
			hs->ul_state = 0;
			ui_docommands("reboot");
		}
	}
	else if (hs->state == HTTP_FUNC) {
		if (hs->eval[0]) {
			xprintf("%s command executed\n", hs->eval);
			ui_docommands(hs->eval);

			/* End of doing command */
			hs->eval[0] = 0;
		}
	}
}

/*
 * ul_index[0] : http header end
 * ul_index[1] : multipart[0] header end
 */
static int  ul_index[2];

static int
read_ul_headers(struct httpd_state *hs)
{
	int i, j;
	char *ul_header = hs->load_addr;
	char *end;

	/*
	 * Gather packets together to overcome the problem
	 * when headers crosses packet boundary.
	 */
	if ((hs->ul_offset + hs->uip_len) >= 4096)
		return -1;

	memcpy(&ul_header[hs->ul_offset], hs->uip_appdata, hs->uip_len);
	hs->ul_offset += hs->uip_len;
	ul_header[hs->ul_offset] = 0;

	/* Search for two <CR><LF><CR><LF> */
	j = 0;
	ul_index[0] = ul_index[1] = 0;
	for (i = 0; i < hs->ul_offset; i++) {
		end = &ul_header[i];

		/* Search end of headers */
		if (*(end+0) == ISO_cr && *(end+1) == ISO_nl &&
		    *(end+2) == ISO_cr && *(end+3) == ISO_nl) {
			i += 4;
			ul_index[j] = i;
			if (++j == 2)
				break;
		}
	}
	if (ul_index[1] == 0)
		return 0;

	return 1;
}

static int
parse_ul_headers(struct httpd_state *hs)
{
	int i;
	char *ul_header = hs->load_addr;
	char *line;

	for (i = 0; i < ul_index[1]; i++) {
		line = &ul_header[i];

		if (*line == ISO_cr || *line == ISO_nl) {
			/* Null end this line */
			*line = 0;
		}
		else {
			/* to upper */
			*line = *line | 0x20;
		}
	}

	/* Get necessary name:value */
	for (i = 0; i < ul_index[0]; i++) {
		line = &ul_header[i];

		if (*line == 0)
			continue;

		if (!strncmp("content-length:", line, 15)) {
			sscanf(line+15, "%d", &hs->ul_len);
		}
	}

	return 0;
}

/*
 * Reads one line and decides what to do next.
 */
static int
httpd_load_program(struct httpd_state *hs)
{
	int rc;
	int i, j;

	/* Process headers for content-length, and content-type */
	if (hs->ul_state == 0) {
		/*
		 * Read http headers and
		 * multipart headers
		 */
		rc = read_ul_headers(hs);
		if (rc <= 0)
			return rc;

		/*
		 * Parse http headers to get
		 * content_length and content_type.
		 */
		parse_ul_headers(hs);


		/* Copy remaining data to load_addr */
		hs->ul_cnt = hs->ul_offset - ul_index[0];
		hs->ul_offset -= ul_index[1];

		/*
		 * Do memmove from load_addr[i],
		 * where i is the beginning of
		 * upload file.
		 */
		i = ul_index[1];
		for (j = 0; j < hs->ul_offset; j++)
			hs->load_addr[j] = hs->load_addr[i+j];		

		hs->ul_state++;
	}
	else if (hs->ul_state == UL_MULTIPART) {
		if ((hs->ul_offset + hs->uip_len) > hs->load_limit ||
		    hs->ul_cnt >= hs->ul_len) {
			return -1;
		}
		
		/* Copy data */
		memcpy(hs->load_addr + hs->ul_offset, hs->uip_appdata, hs->uip_len);
		hs->ul_offset += hs->uip_len;
		hs->load_addr[hs->ul_offset] = 0;

		hs->ul_cnt += hs->uip_len;
		if (hs->ul_cnt < hs->ul_len)
			return 0;

		/* Let trx_validate() handle image size */
		hs->ul_state++;
	}

	if (hs->ul_state == UL_UPDATE) {
		char *resp;

		httpd_printf(hs,
			"<pre>Receive file size=%d<br><font face=Arial size=5>",
			hs->ul_offset);
		/*
		 * Do trx header check.
		 */
		rc = trx_validate((uint8_t *)hs->load_addr, &hs->ul_offset);
		switch (rc) {
		case 0:
			resp = "Upload completed.  System is going to reboot.<br>Please wait a few moments.";
			break;

		case CFE_ERR_DEVNOTFOUND:
			resp = "Could not open flash device.";
			hs->ul_state = 0;
			break;

		default:
			resp = "The file transferred is not a valid firmware image.";
			hs->ul_state = 0;
			break;
		}

		httpd_printf(hs, "%s</font></pre>", resp);
		httpd_page_end(hs, 0, 1);

		return (rc == 0 ? 0 : -1);
	}

	return 0;
}

/*
 * Parse the command issued by the mini Web
 */
static char *
httpd_cmd_parse(struct httpd_state *hs)
{
	char buf[256];
	int len;
	char *cmd;
	char *p;
	int c;

	/* Work on temp buffer */
	len = hs->uip_len > sizeof(buf)-1 ? sizeof(buf)-1 : hs->uip_len;
	memcpy(buf, hs->uip_appdata, len);
	buf[len] = 0;

	/* process GET do.htm?cmd= */
	p = buf;
	while (*p != '=')
		p++;

	if (*p == '=')
		p++;

	/* Parse command */
	cmd = p;
	while ((c = *p & 0xff) != 0 ) {
		switch (c) {
		case ' ':
			*p = 0;
			return cmd;
		case '%':
			sscanf(p+1, "%02x", &c);
			*p++ = c;

			/*
			 * Replace with white space
			 */
			*p++ = ' ';
			*p++ = ' ';
			break;
		case '+':
			*p = ' ';
			/* fall through */
		default:
			p++;
			break;
		}
	}

	/* Return null command */
	return p;
}

static void
httpd_printf(struct httpd_state *hs, char *templat, ...)
{
	va_list marker;

	/* Copy to page */
	if (hs->page_len > sizeof(hs->page_buf) - 32)
		return;

	va_start(marker, templat);
	hs->page_len += xvsprintf(hs->page_buf + hs->page_len, templat, marker);
	va_end(marker);
}

static void
httpd_page_init(struct httpd_state *hs)
{
	hs->page_len = 0;
	httpd_printf(hs,
		"HTTP/1.1 200 OK\r\n"
		"Pragma: no-cache\r\nCache-Control: no-cache\r\n"
		"Connection: close\r\n\r\n"
		"<HTML><BODY>\r\n");
}

static void
httpd_page_end(struct httpd_state *hs, int rc, int f)
{
	httpd_printf(hs,
		"<!-- Recive file size=%d bytes-->%s</BODY></HTML>\r\n",
		rc, f ? "<a href=""/"">Continue</a>" : "");

	/* Flush out */
	httpd_flush(hs);
}

static int
httpd_do_cmd(struct httpd_state *hs)
{
	char *cmd;
	int rc = 0;

	cmd = httpd_cmd_parse(hs);
	if (strcmp(cmd, "reboot") == 0 ||
	    strcmp(cmd, "nvram erase") == 0) {
		/*
		 * Defer these two comands,
		 * until sending response out.
		 */
		strcpy(hs->eval, cmd);
	}
	else {
		rc = ui_docommands(cmd);
	}

	/* Send response out */
	httpd_printf(hs,
		"<pre><font face=Arial size=5>Command %s completed.</font></pre>",
		cmd);

	httpd_page_end(hs, rc, 1);
	return 0;
}

static int
httpd_do_ul(struct httpd_state *hs)
{
	uint8_t *load_addr = NULL;

	if (hs->load_addr == NULL) {
		hs->load_limit = 0;
		ui_get_flash_buf(&load_addr, &hs->load_limit);
		hs->load_addr = (char *)load_addr;

		hs->ul_cnt = 0;
		hs->ul_offset = 0;
	}

	if (httpd_load_program(hs) == -1)
		return -1;

	return 0;
}

static int
httpd_write_flash(char *flashdev, uint8 *load_addr, int len)
{
	int devtype;
	int copysize;
	flash_info_t flashinfo;
	int res;
	int fh;
	int retlen;
	int offset = 0;
	int noerase = 0;
	int amtcopy;

	/*
	 * Make sure it's a flash device.
	 */
	res = cfe_getdevinfo(flashdev);
	if (res < 0) {
		xprintf("Could not find flash device '%s'\n", flashdev);
		return -1;
	}

	devtype = res & CFE_DEV_MASK;

	copysize = len;
	if (copysize == 0)
		return 0;		/* 0 bytes, don't flash */

	/*
	 * Open the destination flash device.
	 */
	fh = cfe_open(flashdev);
	if (fh < 0) {
		xprintf("Could not open device '%s'\n", flashdev);
		return CFE_ERR_DEVNOTFOUND;
	}

	if (cfe_ioctl(fh, IOCTL_FLASH_GETINFO, (unsigned char *)&flashinfo,
		sizeof(flash_info_t), &res, 0) == 0) {
		/* Truncate write if source size is greater than flash size */
		if ((copysize + offset) > flashinfo.flash_size)
			copysize = flashinfo.flash_size - offset;
	}

	/*
	 * If overwriting the boot flash, we need to use the special IOCTL
	 * that will force a reboot after writing the flash.
	 */
	if (flashinfo.flash_flags & FLASH_FLAG_INUSE) {
#if CFG_EMBEDDED_PIC
		xprintf(
		"\n\n** DO NOT TURN OFF YOUR MACHINE UNTIL THE FLASH UPDATE COMPLETES!! **\n\n");
#else
#if CFG_NETWORK
		if (net_getparam(NET_DEVNAME)) {
			xprintf("Closing network.\n");
			net_uninit();
		}
#endif /* CFG_NETWORK */
		xprintf("Rewriting boot flash device '%s'\n", flashdev);
		xprintf("\n\n**DO NOT TURN OFF YOUR MACHINE UNTIL IT REBOOTS!**\n\n");
		cfe_ioctl(fh, IOCTL_FLASH_WRITE_ALL, load_addr, copysize, &retlen, 0);

		/* should not return */
		return CFE_ERR;
#endif	/* EMBEDDED_PIC */
	}

	/*
	 * Otherwise: it's not the flash we're using right
	 * now, so we can be more verbose about things, and
	 * more importantly, we can return to the command
	 * prompt without rebooting!
	 */

	/*
	 * Erase the flash, if the device requires it.  Our new flash
	 * driver does the copy/merge/erase for us.
	 */
	if (!noerase) {
		if ((devtype == CFE_DEV_FLASH) && !(flashinfo.flash_flags & FLASH_FLAG_NOERASE)) {
			flash_range_t range;
			range.range_base = offset;
			range.range_length = copysize;
			xprintf("Erasing flash...");
			if (cfe_ioctl(fh, IOCTL_FLASH_ERASE_RANGE, (uint8_t *)&range,
				sizeof(range), NULL, 0) != 0) {
				xprintf("Failed to erase the flash\n");
				cfe_close(fh);
				return CFE_ERR_IOERR;
			}
		}
	}

	/*
	 * Program the flash
	 */
	xprintf("Programming...");

	amtcopy = cfe_writeblk(fh, offset, load_addr, copysize);
	if (copysize == amtcopy) {
		xprintf("done. %d bytes written\n", amtcopy);
		res = 0;
	}
	else {
		xprintf("Failed. %d bytes written\n", amtcopy);
		res = CFE_ERR_IOERR;
	}

	/*
	 * done!
	 */
	cfe_close(fh);
	return res;
}

static int
trx_validate(uint8_t *ptr, int *insize)
{
	struct trx_header *hdr = (struct trx_header *)ptr;
	uint32_t calccrc;
	int32 len = ltoh32(hdr->len);
	flash_info_t flashinfo;
	int fh, res;
#ifdef CFG_NFLASH
	char flashdev[12];
	
	ui_get_trx_flashdev(flashdev);
#else
	char *flashdev = "flash1.trx";
#endif

	if (ltoh32(hdr->magic) != TRX_MAGIC) {
		xprintf("\nTRX magic number error!");
		return CFE_ERR;
	}

	/*
	 * Open the destination flash device.
	 */
	fh = cfe_open(flashdev);
	if (fh < 0) {
		xprintf("\nOpen device '%s' failed!", flashdev);
		return CFE_ERR_DEVNOTFOUND;
	}

	cfe_ioctl(fh, IOCTL_FLASH_GETINFO, (unsigned char *)&flashinfo,
		 sizeof(flash_info_t), &res, 0);

	cfe_close(fh);

	/* Do CRC check */
	if (len > *insize ||
	    len > flashinfo.flash_size ||
	    len < sizeof(struct trx_header)) {
		xprintf("\nTRX file size error!");
		return CFE_ERR;
	}

	calccrc = flash_crc32(ptr + 12, len-12);
	if (calccrc != ltoh32(hdr->crc32)) {
		xprintf("\nTRX CRC error!");
		return CFE_ERR;
	}

	*insize = len;
	printf("\nTRX file size = %d\n", len);
	return 0;
}
