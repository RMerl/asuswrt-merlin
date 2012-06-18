/* pptp_gre.c  -- encapsulate PPP in PPTP-GRE.
 *                Handle the IP Protocol 47 portion of PPTP.
 *                C. Scott Ananian <cananian@alumni.princeton.edu>
 *
 * $Id: pptp_gre.c,v 1.39 2005/07/11 03:23:48 quozl Exp $
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "ppp_fcs.h"
#include "pptp_msg.h"
#include "pptp_gre.h"
#include "util.h"
#include "pqueue.h"

#define PACKET_MAX 8196
/* test for a 32 bit counter overflow */
#define WRAPPED( curseq, lastseq) \
    ((((curseq) & 0xffffff00) == 0) && \
     (((lastseq) & 0xffffff00 ) == 0xffffff00))

static u_int32_t ack_sent, ack_recv;
static u_int32_t seq_sent, seq_recv;
static u_int16_t pptp_gre_call_id, pptp_gre_peer_call_id;
gre_stats_t stats;

typedef int (*callback_t)(int cl, void *pack, unsigned int len);

/* decaps_hdlc gets all the packets possible with ONE blocking read */
/* returns <0 if read() call fails */
int decaps_hdlc(int fd, callback_t callback, int cl);
int encaps_hdlc(int fd, void *pack, unsigned int len);
int decaps_gre (int fd, callback_t callback, int cl);
int encaps_gre (int fd, void *pack, unsigned int len);
int dequeue_gre(callback_t callback, int cl);

#if 1
#include <stdio.h>
void print_packet(int fd, void *pack, unsigned int len)
{
    unsigned char *b = (unsigned char *)pack;
    unsigned int i,j;
    FILE *out = fdopen(fd, "w");
    fprintf(out,"-- begin packet (%u) --\n", len);
    for (i = 0; i < len; i += 16) {
        for (j = 0; j < 8; j++)
            if (i + 2 * j + 1 < len)
                fprintf(out, "%02x%02x ", 
                        (unsigned int) b[i + 2 * j],
                        (unsigned int) b[i + 2 * j + 1]);
            else if (i + 2 * j < len)
                fprintf(out, "%02x ", (unsigned int) b[i + 2 * j]);
        fprintf(out, "\n");
    }
    fprintf(out, "-- end packet --\n");
    fflush(out);
}
#endif

/*** time_now_usecs ***********************************************************/
uint64_t time_now_usecs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000000) + tv.tv_usec;
}

/*** Open IP protocol socket **************************************************/
int pptp_gre_bind(struct in_addr inetaddr)
{
    struct sockaddr_in src_addr, loc_addr;
    extern struct in_addr localbind;
    int s = socket(AF_INET, SOCK_RAW, PPTP_PROTO);
    if (s < 0) { warn("socket: %s", strerror(errno)); return -1; }
    if (localbind.s_addr != INADDR_NONE) {
        bzero(&loc_addr, sizeof(loc_addr));
        loc_addr.sin_family = AF_INET;
        loc_addr.sin_addr   = localbind;
        if (bind(s, (struct sockaddr *) &loc_addr, sizeof(loc_addr)) != 0) {
            warn("bind: %s", strerror(errno)); close(s); return -1;
        }
    }
    src_addr.sin_family = AF_INET;
    src_addr.sin_addr   = inetaddr;
    src_addr.sin_port   = 0;
    if (connect(s, (struct sockaddr *) &src_addr, sizeof(src_addr)) < 0) {
        warn("connect: %s", strerror(errno)); close(s); return -1;
    }
    return s;
}

/*** pptp_gre_copy ************************************************************/
void pptp_gre_copy(u_int16_t call_id, u_int16_t peer_call_id, 
		   int pty_fd, int gre_fd)
{
    int max_fd;
    pptp_gre_call_id = call_id;
    pptp_gre_peer_call_id = peer_call_id;
    /* Pseudo-terminal already open. */
    ack_sent = ack_recv = seq_sent = seq_recv = 0;
    /* weird select semantics */
    max_fd = gre_fd;
    if (pty_fd > max_fd) max_fd = pty_fd;
    /* Dispatch loop */
    for (;;) { /* until error happens on gre_fd or pty_fd */
        struct timeval tv = {0, 0};
        struct timeval *tvp;
        fd_set rfds;
        int retval;
        pqueue_t *head;
        int block_usecs = -1; /* wait forever */
        /* watch terminal and socket for input */
        FD_ZERO(&rfds);
        FD_SET(gre_fd, &rfds);
        FD_SET(pty_fd, &rfds);
        /*
         * if there are multiple pending ACKs, then do a minimal timeout;
         * else if there is a single pending ACK then timeout after 0,5 seconds;
         * else block until data is available.
         */
        if (ack_sent != seq_recv) {
            if (ack_sent + 1 == seq_recv)  /* u_int wrap-around safe */
                block_usecs = 500000;
            else
                /* don't use zero, this will force a resceduling */
                /* when calling select(), giving pppd a chance to */
                /* run. */
                block_usecs = 1;
        }
        /* otherwise block_usecs == -1, which means wait forever */
        /*
         * If there is a packet in the queue, then don't wait longer than
         * the time remaining until it expires.
         */
        head = pqueue_head();
        if (head != NULL) {
            int expiry_time = pqueue_expiry_time(head);
            if (block_usecs == -1 || expiry_time < block_usecs)
                block_usecs = expiry_time;
        }
        if (block_usecs == -1) {
            tvp = NULL;
        } else {
            tvp = &tv;
            tv.tv_usec = block_usecs;
            tv.tv_sec  = tv.tv_usec / 1000000;
            tv.tv_usec %= 1000000;
        }
        retval = select(max_fd + 1, &rfds, NULL, NULL, tvp);
        if (FD_ISSET(pty_fd, &rfds)) {
            if (decaps_hdlc(pty_fd, encaps_gre,  gre_fd) < 0)
                break;
        } else if (retval == 0 && ack_sent != seq_recv) {
            /* if outstanding ack */
            /* send ack with no payload */
            encaps_gre(gre_fd, NULL, 0);         
        }
        if (FD_ISSET(gre_fd, &rfds)) {
            if (decaps_gre (gre_fd, encaps_hdlc, pty_fd) < 0)
                break;
        }
        if (dequeue_gre (encaps_hdlc, pty_fd) < 0)
            break;
    }
    /* Close up when done. */
    close(gre_fd);
    close(pty_fd);
}

#define HDLC_FLAG         0x7E
#define HDLC_ESCAPE       0x7D
#define HDLC_TRANSPARENCY 0x20

/* ONE blocking read per call; dispatches all packets possible */
/* returns 0 on success, or <0 on read failure                 */
int decaps_hdlc(int fd, int (*cb)(int cl, void *pack, unsigned int len), int cl)
{
    unsigned char buffer[PACKET_MAX];
    unsigned int start = 0;
    int end;
    int status;
    static unsigned int len = 0, escape = 0;
    static unsigned char copy[PACKET_MAX];
    static int checkedsync = 0;
    /* start is start of packet.  end is end of buffer data */
    /*  this is the only blocking read we will allow */
    if ((end = read (fd, buffer, sizeof(buffer))) <= 0) {
        int saved_errno = errno;
        warn("short read (%d): %s", end, strerror(saved_errno));
	switch (saved_errno) {
	  case EMSGSIZE: {
	    int optval, optlen = sizeof(optval);
	    warn("transmitted GRE packet triggered an ICMP destination unreachable, fragmentation needed, or exceeds the MTU of the network interface");
#define IP_MTU 14
	    if(getsockopt(fd, IPPROTO_IP, IP_MTU, &optval, &optlen) < 0)
	      warn("getsockopt: %s", strerror(errno));
	    warn("getsockopt: IP_MTU: %d\n", optval);
	    return 0;
	  }
	case EIO:
    	    warn("pppd may have shutdown, see pppd log");
	    break;
	}
        return -1;
    }
    /* warn if the sync options of ppp and pptp don't match */
    if( !checkedsync) {
        checkedsync = 1;
        if( buffer[0] == HDLC_FLAG){
            if( syncppp )
                warn( "pptp --sync option is active, "
                        "yet the ppp mode is asynchronous!\n");
        }
	else if( !syncppp )
            warn( "The ppp mode is synchronous, "
                    "yet no pptp --sync option is specified!\n");
    }
    if ( syncppp ){
    	/* this handling is pretty simple thanks to N_HDLC */
        if ((status = cb (cl, buffer, end)) < 0)
            return status; /* error-check */
        return 0;
    }
    /* asynchronous mode */
    while (start < end) {
        /* Copy to 'copy' and un-escape as we go. */
        while (buffer[start] != HDLC_FLAG) {
            if ((escape == 0) && buffer[start] == HDLC_ESCAPE) {
                escape = HDLC_TRANSPARENCY;
            } else {
                if (len < PACKET_MAX)
                    copy [len++] = buffer[start] ^ escape;
                escape = 0;
            }
            start++;
            if (start >= end)
                return 0; /* No more data, but the frame is not complete yet. */
        }
        /* found flag.  skip past it */
        start++;
        /* check for over-short packets and silently discard, as per RFC1662 */
        if ((len < 4) || (escape != 0)) {
            len = 0; escape = 0;
            continue;
        }
        /* check, then remove the 16-bit FCS checksum field */
        if (pppfcs16 (PPPINITFCS16, copy, len) != PPPGOODFCS16)
            warn("Bad Frame Check Sequence during PPP to GRE decapsulation");
        len -= sizeof(u_int16_t);
        /* so now we have a packet of length 'len' in 'copy' */
        if ((status = cb (cl, copy, len)) < 0)
            return status; /* error-check */
        /* Great!  Let's do more! */
        len = 0; escape = 0;
    }
    return 0;
    /* No more data to process. */
}

/*** Make stripped packet into HDLC packet ************************************/
int encaps_hdlc(int fd, void *pack, unsigned int len)
{
    unsigned char *source = (unsigned char *)pack;
    unsigned char dest[2 * PACKET_MAX + 2]; /* largest expansion possible */
    unsigned int pos = 0, i;
    u_int16_t fcs;
    /* in synchronous mode there is little to do */
    if ( syncppp )
        return write(fd, source, len);
    /* asynchronous mode */
    /* Compute the FCS */
    fcs = pppfcs16(PPPINITFCS16, source, len) ^ 0xFFFF;
    /* start character */
    dest[pos++] = HDLC_FLAG;
    /* escape the payload */
    for (i = 0; i < len + 2; i++) {
        /* wacked out assignment to add FCS to end of source buffer */
        unsigned char c =
            (i < len)?source[i]:(i == len)?(fcs & 0xFF):((fcs >> 8) & 0xFF);
        if (pos >= sizeof(dest)) break; /* truncate on overflow */
        if ( (c < 0x20) || (c == HDLC_FLAG) || (c == HDLC_ESCAPE) ) {
            dest[pos++] = HDLC_ESCAPE;
            if (pos < sizeof(dest))
                dest[pos++] = c ^ 0x20;
        } else
            dest[pos++] = c;
    }
    /* tack on the end-flag */
    if (pos < sizeof(dest))
        dest[pos++] = HDLC_FLAG;
    /* now write this packet */
    return write(fd, dest, pos);
}

/*** decaps_gre ***************************************************************/
int decaps_gre (int fd, callback_t callback, int cl)
{
    unsigned char buffer[PACKET_MAX + 64 /*ip header*/];
    struct pptp_gre_header *header;
    int status, ip_len = 0;
    static int first = 1;
    unsigned int headersize;
    unsigned int payload_len;
    u_int32_t seq;

    if ((status = read (fd, buffer, sizeof(buffer))) <= 0) {
        warn("short read (%d): %s", status, strerror(errno));
        stats.rx_errors++;
        return -1;
    }
    /* strip off IP header, if present */
    if ((buffer[0] & 0xF0) == 0x40) 
        ip_len = (buffer[0] & 0xF) * 4;
    header = (struct pptp_gre_header *)(buffer + ip_len);
    /* verify packet (else discard) */
    if (    /* version should be 1 */
            ((ntoh8(header->ver) & 0x7F) != PPTP_GRE_VER) ||
            /* PPTP-GRE protocol for PPTP */
            (ntoh16(header->protocol) != PPTP_GRE_PROTO)||
            /* flag C should be clear   */
            PPTP_GRE_IS_C(ntoh8(header->flags)) ||
            /* flag R should be clear   */
            PPTP_GRE_IS_R(ntoh8(header->flags)) ||
            /* flag K should be set     */
            (!PPTP_GRE_IS_K(ntoh8(header->flags))) ||
            /* routing and recursion ctrl = 0  */
            ((ntoh8(header->flags)&0xF) != 0)) {
        /* if invalid, discard this packet */
        warn("Discarding GRE: %X %X %X %X %X %X", 
                ntoh8(header->ver)&0x7F, ntoh16(header->protocol), 
                PPTP_GRE_IS_C(ntoh8(header->flags)),
                PPTP_GRE_IS_R(ntoh8(header->flags)), 
                PPTP_GRE_IS_K(ntoh8(header->flags)),
                ntoh8(header->flags) & 0xF);
        stats.rx_invalid++;
        return 0;
    }
    /* silently discard packets not for this call */
    if (ntoh16(header->call_id) != pptp_gre_call_id) {
        if (log_level >= 2)
            log("Discarding for other call : %d %d",
                ntoh16(header->call_id), pptp_gre_call_id);
        if (pptp_gre_call_id == 0) {
            pptp_gre_call_id = ntoh16(header->call_id);
        } else {
            return 0;
        }
    }
    /* test if acknowledgement present */
    if (PPTP_GRE_IS_A(ntoh8(header->ver))) { 
        u_int32_t ack = (PPTP_GRE_IS_S(ntoh8(header->flags)))?
            header->ack:header->seq; /* ack in different place if S = 0 */
        ack = ntoh32( ack);
        if (ack > ack_recv) ack_recv = ack;
        /* also handle sequence number wrap-around  */
        if (WRAPPED(ack,ack_recv)) ack_recv = ack;
        if (ack_recv == stats.pt.seq) {
            int rtt = time_now_usecs() - stats.pt.time;
            stats.rtt = (stats.rtt + rtt) / 2;
        }
    }
    /* test if payload present */
    if (!PPTP_GRE_IS_S(ntoh8(header->flags)))
        return 0; /* ack, but no payload */
    headersize  = sizeof(*header);
    payload_len = ntoh16(header->payload_len);
    seq         = ntoh32(header->seq);
    /* no ack present? */
    if (!PPTP_GRE_IS_A(ntoh8(header->ver))) headersize -= sizeof(header->ack);
    /* check for incomplete packet (length smaller than expected) */
    if (status - headersize < payload_len) {
        warn("discarding truncated packet (expected %d, got %d bytes)",
                payload_len, status - headersize);
        stats.rx_truncated++;
        return 0; 
    }
    /* check for expected sequence number */
    if ( first || (seq == seq_recv + 1)) { /* wrap-around safe */
	if ( log_level >= 2 )
            log("accepting packet %d", seq);
        stats.rx_accepted++;
        first = 0;
        seq_recv = seq;
        return callback(cl, buffer + ip_len + headersize, payload_len);
    /* out of order, check if the number is too low and discard the packet. 
     * (handle sequence number wrap-around, and try to do it right) */
    } else if ( seq < seq_recv + 1 || WRAPPED(seq_recv, seq) ) {
	if ( log_level >= 1 )
            log("discarding duplicate or old packet %d (expecting %d)",
                seq, seq_recv + 1);
        stats.rx_underwin++;
    /* sequence number too high, is it reasonably close? */
    } else if ( seq < seq_recv + MISSING_WINDOW ||
                WRAPPED(seq, seq_recv + MISSING_WINDOW) ) {
	stats.rx_buffered++;
        if ( log_level >= 1 )
            log("%s packet %d (expecting %d, lost or reordered)",
                disable_buffer ? "accepting" : "buffering",
                seq, seq_recv+1);
        if ( disable_buffer ) {
            seq_recv = seq;
            stats.rx_lost += seq - seq_recv - 1;
            return callback(cl, buffer + ip_len + headersize, payload_len);
        } else {
            pqueue_add(seq, buffer + ip_len + headersize, payload_len);
	}
    /* no, packet must be discarded */
    } else {
	if ( log_level >= 1 )
            warn("discarding bogus packet %d (expecting %d)", 
		 seq, seq_recv + 1);
        stats.rx_overwin++;
    }
    return 0;
}

/*** dequeue_gre **************************************************************/
int dequeue_gre (callback_t callback, int cl)
{
    pqueue_t *head;
    int status;
    /* process packets in the queue that either are expected or have 
     * timed out. */
    head = pqueue_head();
    while ( head != NULL &&
            ( (head->seq == seq_recv + 1) || /* wrap-around safe */ 
              (pqueue_expiry_time(head) <= 0) 
            )
          ) {
        /* if it is timed out... */
        if (head->seq != seq_recv + 1 ) {  /* wrap-around safe */          
            stats.rx_lost += head->seq - seq_recv - 1;
	    if (log_level >= 2)
                log("timeout waiting for %d packets", head->seq - seq_recv - 1);
        }
	if (log_level >= 2)
            log("accepting %d from queue", head->seq);
        seq_recv = head->seq;
        status = callback(cl, head->packet, head->packlen);
        pqueue_del(head);
        if (status < 0)
            return status;
        head = pqueue_head();
    }
    return 0;
}

/*** encaps_gre ***************************************************************/
int encaps_gre (int fd, void *pack, unsigned int len)
{
    struct pptp_gre_header header;
    struct iovec iov[2] = { { &header, 0 }, { pack, len } };
    static u_int32_t seq = 1; /* first sequence number sent must be 1 */
    unsigned int header_len;
    int rc;
    /* package this up in a GRE shell. */
    header.flags       = hton8 (PPTP_GRE_FLAG_K);
    header.ver         = hton8 (PPTP_GRE_VER);
    header.protocol    = hton16(PPTP_GRE_PROTO);
    header.payload_len = hton16(len);
    header.call_id     = hton16(pptp_gre_peer_call_id);
    /* special case ACK with no payload */
    if (pack == NULL) {
        if (ack_sent != seq_recv) {
            header.ver |= hton8(PPTP_GRE_FLAG_A);
            header.payload_len = hton16(0);
            /* ack is in odd place because S == 0 */
            header.seq = hton32(seq_recv);
            ack_sent = seq_recv;
            rc = write(fd, &header, sizeof(header) - sizeof(header.seq));
            if (rc < 0) {
                stats.tx_failed++;
            } else if (rc < sizeof(header) - sizeof(header.seq)) {
                stats.tx_short++;
            } else {
                stats.tx_acks++;
            }
            return rc;
        } else return 0; /* we don't need to send ACK */
    } /* explicit brace to avoid ambiguous `else' warning */
    /* send packet with payload */
    header.flags |= hton8(PPTP_GRE_FLAG_S);
    header.seq    = hton32(seq);
    if (ack_sent != seq_recv) { /* send ack with this message */
        header.ver |= hton8(PPTP_GRE_FLAG_A);
        header.ack  = hton32(seq_recv);
        ack_sent = seq_recv;
        header_len = sizeof(header);
    } else { /* don't send ack */
        header_len = sizeof(header) - sizeof(header.ack);
    }
    /* save final header length */
    iov[0].iov_len = header_len;
    /* record and increment sequence numbers */
    seq_sent = seq; seq++;
    /* write this baby out to the net */
    rc = writev(fd, iov, 2);
    if (rc < 0) {
        stats.tx_failed++;
    } else if (rc < header_len + len) {
        stats.tx_short++;
    } else {
        stats.tx_sent++;
        stats.pt.seq  = seq_sent;
        stats.pt.time = time_now_usecs();
    }
    return rc;
}
