/*
 * pptpgre.h
 *
 * Functions to handle the GRE en/decapsulation
 *
 * $Id: pptpgre.h,v 1.3 2005/08/02 11:33:31 quozl Exp $
 */

#ifndef _PPTPD_PPTPGRE_H
#define _PPTPD_PPTPGRE_H

extern int decaps_hdlc(int fd, int (*cb) (int cl, void *pack, unsigned len), int cl);
extern int encaps_hdlc(int fd, void *pack, unsigned len);
extern int decaps_gre(int fd, int (*cb) (int cl, void *pack, unsigned len), int cl);
extern int encaps_gre(int fd, void *pack, unsigned len);

extern int pptp_gre_init(u_int32_t call_id_pair, int pty_fd, struct in_addr *inetaddrs);

struct gre_state {
	u_int32_t ack_sent, ack_recv;
	u_int32_t seq_sent, seq_recv;
	u_int32_t call_id_pair;
};

extern int disable_buffer;

typedef struct pack_track {
  uint32_t seq;       // seq no of this tracked packet
  uint64_t time;      // time when this tracked packet was sent (in usecs)
} pack_track_t;

typedef struct gre_stats {
  /* statistics for GRE receive */

  uint32_t rx_accepted;  // data packet was passed to pppd
  uint32_t rx_lost;      // data packet did not arrive before timeout
  uint32_t rx_underwin;  // data packet was under window (arrived too late
                         // or duplicate packet)
  uint32_t rx_overwin;   // data packet was over window
                         // (too many packets lost?)
  uint32_t rx_buffered;  // data packet arrived earlier than expected,
                         // packet(s) before it were lost or reordered
  uint32_t rx_errors;    // OS error on receive
  uint32_t rx_truncated; // truncated packet
  uint32_t rx_invalid;   // wrong protocol or invalid flags
  uint32_t rx_acks;      // acknowledgement only

  /* statistics for GRE transmit */

  uint32_t tx_sent;      // data packet write() to GRE socket succeeded
  uint32_t tx_failed;    // data packet write() to GRE socket returned error
  uint32_t tx_short;     // data packet write() to GRE socket underflowed
  uint32_t tx_acks;      // sent packet with just ACK
  uint32_t tx_oversize;  // data packet dropped because it was too large

  /* statistics for packet tracking, for RTT calculation */

  pack_track_t pt;       // last data packet seq/time
  int rtt;               // estimated round-trip time in us

} gre_stats_t;

extern gre_stats_t stats;

#endif	/* !_PPTPD_PPTPGRE_H */
