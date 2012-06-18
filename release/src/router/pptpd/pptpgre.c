/*
 * pptpgre.c
 *
 * originally by C. S. Ananian
 * Modified for PoPToP
 *
 * $Id: pptpgre.c,v 1.9 2007/04/16 00:21:02 quozl Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __linux__
#define _GNU_SOURCE 1		/* broken arpa/inet.h */
#endif

#include "our_syslog.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#include "ppphdlc.h"
#include "pptpgre.h"
#include "pptpdefs.h"
#include "pptpctrl.h"
#include "defaults.h"
#include "pqueue.h"

#ifndef HAVE_STRERROR
#include "compat.h"
#endif

#define PACKET_MAX 8196

typedef int (*callback_t)(int cl, void *pack, unsigned int len);

/* test for a 32 bit counter overflow */
#define WRAPPED( curseq, lastseq) \
    ((((curseq) & 0xffffff00) == 0) && \
     (((lastseq) & 0xffffff00 ) == 0xffffff00))

static struct gre_state gre; 
gre_stats_t stats; 

static uint64_t time_now_usecs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000000) + tv.tv_usec;
}

int pptp_gre_init(u_int32_t call_id_pair, int pty_fd, struct in_addr *inetaddrs)
{
	struct sockaddr_in addr;
	int gre_fd;

	/* Open IP protocol socket */
	gre_fd = socket(AF_INET, SOCK_RAW, PPTP_PROTO);
	if (gre_fd < 0) {
		syslog(LOG_ERR, "GRE: socket() failed");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_addr = inetaddrs[0];
	addr.sin_port = 0;
	if (bind(gre_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		syslog(LOG_ERR, "GRE: bind() failed: %s", strerror(errno));
		syslog(LOG_ERR, "GRE: continuing, but may not work if multi-homed");
	}

	addr.sin_family = AF_INET;
	addr.sin_addr = inetaddrs[1];
	addr.sin_port = 0;
	if (connect(gre_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		syslog(LOG_ERR, "GRE: connect() failed: %s", strerror(errno));
		return -1;
	}

	gre.seq_sent = 0;
	gre.ack_sent = gre.ack_recv = gre.seq_recv = 0xFFFFFFFF;
	/* seq_recv is -1, therefore next packet expected is seq 0,
	   to comply with RFC 2637: 'The sequence number for each
	   user session is set to zero at session startup.' */
				      
	gre.call_id_pair = call_id_pair;	       /* network byte order */
	return gre_fd;
}

/* ONE blocking read per call; dispatches all packets possible */
/* returns 0 on success, or <0 on read failure                 */
int decaps_hdlc(int fd, int (*cb) (int cl, void *pack, unsigned len), int cl)
{
	static unsigned char buffer[PACKET_MAX], copy[PACKET_MAX];
	static unsigned start = 0, end = 0;
	static unsigned len = 0, escape = 0;
	static u_int16_t fcs = PPPINITFCS16;
	static unsigned char err = 0;
	unsigned char c;
	int status;

	/* we do one read only, since it may block.  and only if the
	 * buffer is empty (start == end)
	 */

	if (fd == -1) {
		if(cb == NULL) {
			/* peek mode */
			return err ? -1 : 0;
		} else if (!err) {
			/* re-xmit and nothing queued */
			syslog(LOG_ERR, "GRE: Re-xmit called with nothing queued");
			return -1;
		}
	}

	if (!err) {
		/* All known data is processed.  This true unless the last
		 * network write failed.
		 */
		if ((status = read(fd, buffer, sizeof(buffer))) <= 0) {
			syslog(LOG_ERR, "GRE: read(fd=%d,buffer=%lx,len=%d) from PTY failed: status = %d error = %s%s",
			       fd, (unsigned long) buffer, sizeof(buffer), 
			       status, status ? strerror(errno) : "No error", 
			       errno != EIO ? "" : ", usually caused by unexpected termination of pppd, check option syntax and pppd logs");
			/* FAQ: mistakes in pppd option spelling in
			 * /etc/ppp/options.pptpd often cause EIO,
			 * with pppd not reporting the problem to any
			 * logs.  Termination of pppd by signal can
			 * *also* cause this situation. -- James Cameron
			 */
			return -1;
		}
		end = status;
		start = 0;
	} else {
		/* We're here because of a network write failure.  Try again.
		 * Then do what we would do normally and enter the loop as if
		 * just continuing the while(1).  Not sure that this ever
		 * really happens, but since we error-check status then we
		 * should have the code to handle an error :-)
		 */
		err = 0;
		if ((status = cb(cl, copy, len)) < 0) {
			syslog(LOG_ERR, "GRE: re-xmit failed from decaps_hdlc: %s", strerror(errno));
			err = 1;
			return status;	/* return error */
		}
		/* Great!  Let's do more! */
		fcs = PPPINITFCS16;
		len = 0;
		escape = 0;
	}

	while (1) {
		/* Infinite loop, we return when we're out of data */

		/* Check if out of data */
		if (start == end)
			return 0;

		/* Add to the packet up till the next HDLC_FLAG (start/end of
		 * packet marker).  Copy to 'copy', un-escape and checksum as we go.
		 */
		while (buffer[start] != HDLC_FLAG) {

			/* Dispose of 'too long' packets */
			if (len >= PACKET_MAX) {
				syslog(LOG_ERR, "GRE: Received too long packet from pppd.");
				while (buffer[start] != HDLC_FLAG && start < end)
					start++;
				if (start < end) {
					goto newpacket;
				} else
					return 0;
			}
			/* Read a character, un-escaping if needed */
			if (buffer[start] == HDLC_ESCAPE && !escape)
				escape = 1;
			else {
				if (escape) {
					copy[len] = c = buffer[start] ^ 0x20;
					escape = 0;
				} else
					copy[len] = c = buffer[start];
				fcs = (fcs >> 8) ^ fcstab[(fcs ^ c) & 0xff];
				len++;
			}
			start++;

			/* Check if out of data */
			if (start == end)
				return 0;
		}

		/* Found flag.  Skip past it */
		start++;

		/* Check for over-short packets and silently discard, as per RFC1662 */
		if ((len < 4) || (escape == 1)) {
			/* len == 0 is possible, we generate it :-) [using HDLC_ESCAPE at
			 * start and end of packet].  Others are worth recording.
			 */
			if (len && len < 4)
				syslog(LOG_ERR, "GRE: Received too short packet from pppd.");
			if (escape)
				syslog(LOG_ERR, "GRE: Received bad packet from pppd.");
			goto newpacket;
		}
		/* Check, then remove the 16-bit FCS checksum field */
		if (fcs != PPPGOODFCS16) {
			syslog(LOG_ERR, "GRE: Bad checksum from pppd.");
			goto newpacket;
		}
		len -= sizeof(u_int16_t);

		/* So now we have a packet of length 'len' in 'copy' */
		if ((status = cb(cl, copy, len)) < 0) {
			syslog(LOG_ERR, "GRE: xmit failed from decaps_hdlc: %s", strerror(errno));
			err = 1;
			return status;	/* return error */
		}
	      newpacket:
		/* Great!  Let's do more! */
		fcs = PPPINITFCS16;
		len = 0;
		escape = 0;
	}
}

#define seq_greater(A,B) ((A)>(B) || \
                         (((u_int32_t)(A)<0xff) && ((~((u_int32_t)(B)))<0xff)))

/* Macro used in encaps_hdlc().  add "val" to "dest" at position "pos",
 * incrementing "pos" to point after the added value.  set "tmp" to "val"
 * as a side-effect.
 */
#define ADD_CHAR(dest, pos, val, tmp) \
	tmp = (val); \
	if ((tmp<0x20) || (tmp==HDLC_FLAG) || (tmp==HDLC_ESCAPE)) { \
		dest[pos++]=HDLC_ESCAPE; \
		dest[pos++]=tmp^0x20; \
	} else \
		dest[pos++]=tmp

/* Make stripped packet into HDLC packet */
int encaps_hdlc(int fd, void *pack, unsigned len)
{
	unsigned char *source = (unsigned char *) pack;
	/* largest expansion possible - double all + double fcs + 2 flags */
	static unsigned char dest[2 * PACKET_MAX + 6];
	unsigned pos = 1, i;
	u_int16_t fcs;
	unsigned char c;

	fcs = PPPINITFCS16;

	/* make sure overflow is impossible so we don't have to bounds check
	 * in loop.  drop large packets.
	 */
	if (len > PACKET_MAX) {
		syslog(LOG_ERR, "GRE: Asked to encapsulate too large packet (len = %d)", len);
		return -1;
	}
	/* start character */
	dest[0] = HDLC_FLAG;

	/* escape the payload */
	for (i = 0; i < len; i++) {
		ADD_CHAR(dest, pos, source[i], c);
		fcs = (fcs >> 8) ^ fcstab[(fcs ^ c) & 0xff];
	}

	fcs ^= 0xFFFF;

	ADD_CHAR(dest, pos, fcs & 0xFF, c);
	ADD_CHAR(dest, pos, fcs >> 8, c);

	/* tack on the end-flag */
	dest[pos++] = HDLC_FLAG;

	/* now write this packet */
	return write(fd, dest, pos);
}

#undef ADD_CHAR


static int dequeue_gre (callback_t callback, int cl)
{
	pqueue_t *head;
	int status;
	/* process packets in the queue that either are expected or
	   have timed out. */
	head = pqueue_head();
	while ( head != NULL &&
		( (head->seq == gre.seq_recv + 1) || /* wrap-around safe */
		  (pqueue_expiry_time(head) <= 0)
		  )
		) {
		/* if it is timed out... */
		if (head->seq != gre.seq_recv + 1 ) {  /* wrap-around safe */
			stats.rx_lost += head->seq - gre.seq_recv - 1;
			if (pptpctrl_debug)
				syslog(LOG_DEBUG, 
				       "GRE: timeout waiting for %d packets", 
				       head->seq - gre.seq_recv - 1);        
		}
		if (pptpctrl_debug)
			syslog(LOG_DEBUG, "GRE: accepting #%d from queue", 
			       head->seq);
		gre.seq_recv = head->seq;
		status = callback(cl, head->packet, head->packlen);
		pqueue_del(head);
		if (status < 0) return status;
		head = pqueue_head();
	}
	return 0;
}


int decaps_gre(int fd, int (*cb) (int cl, void *pack, unsigned len), int cl)
{
	static unsigned char buffer[PACKET_MAX + 64 /*ip header */ ];
	struct pptp_gre_header *header;
	int status, ip_len = 0;

	dequeue_gre(cb, cl);
	if ((status = read(fd, buffer, sizeof(buffer))) <= 0) {
		syslog(LOG_ERR, "GRE: read(fd=%d,buffer=%lx,len=%d) from network failed: status = %d error = %s",
		       fd, (unsigned long) buffer, sizeof(buffer), status, status ? strerror(errno) : "No error");
		stats.rx_errors++;
		return -1;
	}
	/* strip off IP header, if present */
	if ((buffer[0] & 0xF0) == 0x40)
		ip_len = (buffer[0] & 0xF) * 4;
	header = (struct pptp_gre_header *) (buffer + ip_len);

	/* verify packet (else discard) */
	if (((ntoh8(header->ver) & 0x7F) != PPTP_GRE_VER) ||	/* version should be 1   */
	    (ntoh16(header->protocol) != PPTP_GRE_PROTO) ||	/* GRE protocol for PPTP */
	    PPTP_GRE_IS_C(ntoh8(header->flags)) ||	/* flag C should be clear   */
	    PPTP_GRE_IS_R(ntoh8(header->flags)) ||	/* flag R should be clear   */
	    (!PPTP_GRE_IS_K(ntoh8(header->flags))) ||	/* flag K should be set     */
	    ((ntoh8(header->flags) & 0xF) != 0)) {	/* routing and recursion ctrl = 0  */
		/* if invalid, discard this packet */
		syslog(LOG_ERR, "GRE: Discarding packet by header check");
		stats.rx_invalid++;
		return 0;
	}
	if (header->call_id != GET_VALUE(PAC, gre.call_id_pair)) {
                /*
                 * Discard silently to allow more than one GRE tunnel from
                 * the same IP address in case clients are behind the
                 * firewall.
                 *
                 * syslog(LOG_ERR, "GRE: Discarding for incorrect call");
                 */
		return 0;
	}
	if (PPTP_GRE_IS_A(ntoh8(header->ver))) {	/* acknowledgement present */
		u_int32_t ack = (PPTP_GRE_IS_S(ntoh8(header->flags))) ?
			ntoh32(header->ack) : ntoh32(header->seq);
			/* ack in different place if S=0 */

		if (seq_greater(ack, gre.ack_recv))
			gre.ack_recv = ack;

		/* also handle sequence number wrap-around  */
		if (WRAPPED(ack,gre.ack_recv)) gre.ack_recv = ack;
		if (gre.ack_recv == stats.pt.seq) {
			int rtt = time_now_usecs() - stats.pt.time;
			stats.rtt = (stats.rtt + rtt) / 2;
		}
	}
	if (PPTP_GRE_IS_S(ntoh8(header->flags))) {	/* payload present */
		unsigned headersize = sizeof(*header);
		unsigned payload_len = ntoh16(header->payload_len);
		u_int32_t seq = ntoh32(header->seq);

		if (!PPTP_GRE_IS_A(ntoh8(header->ver)))
			headersize -= sizeof(header->ack);
		/* check for incomplete packet (length smaller than expected) */
		if (status - headersize < payload_len) {
			stats.rx_truncated++;
			return 0;
		}
		/* check for out-of-order sequence number */
		if (seq == gre.seq_recv + 1) {
			if (pptpctrl_debug)
				syslog(LOG_DEBUG, "GRE: accepting packet #%d", 
				       seq);
			stats.rx_accepted++;
			gre.seq_recv = seq;
			return cb(cl, buffer + ip_len + headersize, payload_len);
		} else if (seq == gre.seq_recv) {
			if (pptpctrl_debug)
				syslog(LOG_DEBUG,
				       "GRE: discarding duplicate or old packet #%d (expecting #%d)", 
				       seq, gre.seq_recv + 1);
			return 0;	/* discard duplicate packets */
		} else {
			stats.rx_buffered++;
			if (pptpctrl_debug)
				syslog(LOG_DEBUG,
				       "GRE: buffering packet #%d (expecting #%d, lost or reordered)",
				       seq, gre.seq_recv + 1);
			pqueue_add(seq, buffer + ip_len + headersize, payload_len);
			return 0;	/* discard out-of-order packets */
		}
	}
	return 0;		/* ack, but no payload */
}


int encaps_gre(int fd, void *pack, unsigned len)
{
	static union {
		struct pptp_gre_header header;
		unsigned char buffer[PACKET_MAX + sizeof(struct pptp_gre_header)];
	} u;
	unsigned header_len;
#ifdef HAVE_WRITEV
	struct iovec iovec[2];
#endif

	if(fd == -1)
		/* peek mode */
		return (gre.ack_sent == gre.seq_recv) ? 0 : -1;

	/* package this up in a GRE shell. */
	u.header.flags = hton8(PPTP_GRE_FLAG_K);
	u.header.ver = hton8(PPTP_GRE_VER);
	u.header.protocol = hton16(PPTP_GRE_PROTO);
	u.header.payload_len = hton16(len);
	u.header.call_id = GET_VALUE(PNS, gre.call_id_pair);

	/* special case ACK with no payload */
	if (pack == NULL) {
		if (gre.ack_sent != gre.seq_recv) {
			u.header.ver |= hton8(PPTP_GRE_FLAG_A);
			u.header.payload_len = hton16(0);
			u.header.seq = hton32(gre.seq_recv);	/* ack is in odd place because S=0 */
			gre.ack_sent = gre.seq_recv;
			/* don't sent ACK field, ACK is in SYN field */
			return write(fd, u.buffer, sizeof(u.header) - sizeof(u.header.ack));
		} else
			return 0;	/* we don't need to send ACK */
	}
	/* send packet with payload */
	u.header.flags |= hton8(PPTP_GRE_FLAG_S);
	u.header.seq = hton32(gre.seq_sent);
	gre.seq_sent++;
	if (gre.ack_sent != gre.seq_recv) {	/* send ack with this message */
		u.header.ver |= hton8(PPTP_GRE_FLAG_A);
		u.header.ack = hton32(gre.seq_recv);
		gre.ack_sent = gre.seq_recv;
		header_len = sizeof(u.header);
	} else {	/* don't send ack */
		header_len = sizeof(u.header) - sizeof(u.header.ack);
	}
	if (len > PACKET_MAX) {
		syslog(LOG_ERR, "GRE: packet is too large %d", len);
		stats.tx_oversize++;
		return 0;	/* drop this, it's too big */
	}
#ifdef HAVE_WRITEV
	/* write header and buffer without copying. */
	iovec[0].iov_base = u.buffer;
	iovec[0].iov_len = header_len;
	iovec[1].iov_base = pack;
	iovec[1].iov_len = len;
	return writev(fd, iovec, 2);
#else
	/* copy payload into buffer */
	memcpy(u.buffer + header_len, pack, len);
	/* record and increment sequence numbers */
	/* write this baby out to the net */
	return write(fd, u.buffer, header_len + len);
#endif
}
