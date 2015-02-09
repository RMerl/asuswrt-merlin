/*
 * Copyright (C) ST-Ericsson AB 2010
 * Author:	Sjur Brendeland/sjur.brandeland@stericsson.com
 * License terms: GNU General Public License (GPL) version 2
 */

#ifndef CFPKT_H_
#define CFPKT_H_
#include <net/caif/caif_layer.h>
#include <linux/types.h>
struct cfpkt;

/* Create a CAIF packet.
 * len: Length of packet to be created
 * @return New packet.
 */
struct cfpkt *cfpkt_create(u16 len);

/* Create a CAIF packet.
 * data Data to copy.
 * len Length of packet to be created
 * @return New packet.
 */
struct cfpkt *cfpkt_create_uplink(const unsigned char *data, unsigned int len);
/*
 * Destroy a CAIF Packet.
 * pkt Packet to be destoyed.
 */
void cfpkt_destroy(struct cfpkt *pkt);

/*
 * Extract header from packet.
 *
 * pkt Packet to extract header data from.
 * data Pointer to copy the header data into.
 * len Length of head data to copy.
 * @return zero on success and error code upon failure
 */
int cfpkt_extr_head(struct cfpkt *pkt, void *data, u16 len);

/*
 * Peek header from packet.
 * Reads data from packet without changing packet.
 *
 * pkt Packet to extract header data from.
 * data Pointer to copy the header data into.
 * len Length of head data to copy.
 * @return zero on success and error code upon failure
 */
int cfpkt_peek_head(struct cfpkt *pkt, void *data, u16 len);

/*
 * Extract header from trailer (end of packet).
 *
 * pkt Packet to extract header data from.
 * data Pointer to copy the trailer data into.
 * len Length of header data to copy.
 * @return zero on success and error code upon failure
 */
int cfpkt_extr_trail(struct cfpkt *pkt, void *data, u16 len);

/*
 * Add header to packet.
 *
 *
 * pkt Packet to add header data to.
 * data Pointer to data to copy into the header.
 * len Length of header data to copy.
 * @return zero on success and error code upon failure
 */
int cfpkt_add_head(struct cfpkt *pkt, const void *data, u16 len);

/*
 * Add trailer to packet.
 *
 *
 * pkt Packet to add trailer data to.
 * data Pointer to data to copy into the trailer.
 * len Length of trailer data to copy.
 * @return zero on success and error code upon failure
 */
int cfpkt_add_trail(struct cfpkt *pkt, const void *data, u16 len);

/*
 * Pad trailer on packet.
 * Moves data pointer in packet, no content copied.
 *
 * pkt Packet in which to pad trailer.
 * len Length of padding to add.
 * @return zero on success and error code upon failure
 */
int cfpkt_pad_trail(struct cfpkt *pkt, u16 len);

/*
 * Add a single byte to packet body (tail).
 *
 * pkt Packet in which to add byte.
 * data Byte to add.
 * @return zero on success and error code upon failure
 */
int cfpkt_addbdy(struct cfpkt *pkt, const u8 data);

/*
 * Add a data to packet body (tail).
 *
 * pkt Packet in which to add data.
 * data Pointer to data to copy into the packet body.
 * len Length of data to add.
 * @return zero on success and error code upon failure
 */
int cfpkt_add_body(struct cfpkt *pkt, const void *data, u16 len);

/*
 * Checks whether there are more data to process in packet.
 * pkt Packet to check.
 * @return true if more data are available in packet false otherwise
 */
bool cfpkt_more(struct cfpkt *pkt);

/*
 * Checks whether the packet is erroneous,
 * i.e. if it has been attempted to extract more data than available in packet
 * or writing more data than has been allocated in cfpkt_create().
 * pkt Packet to check.
 * @return true on error false otherwise
 */
bool cfpkt_erroneous(struct cfpkt *pkt);

/*
 * Get the packet length.
 * pkt Packet to get length from.
 * @return Number of bytes in packet.
 */
u16 cfpkt_getlen(struct cfpkt *pkt);

/*
 * Set the packet length, by adjusting the trailer pointer according to length.
 * pkt Packet to set length.
 * len Packet length.
 * @return Number of bytes in packet.
 */
int cfpkt_setlen(struct cfpkt *pkt, u16 len);

/*
 * cfpkt_append - Appends a packet's data to another packet.
 * dstpkt:    Packet to append data into, WILL BE FREED BY THIS FUNCTION
 * addpkt:    Packet to be appended and automatically released,
 *            WILL BE FREED BY THIS FUNCTION.
 * expectlen: Packet's expected total length. This should be considered
 *            as a hint.
 * NB: Input packets will be destroyed after appending and cannot be used
 * after calling this function.
 * @return    The new appended packet.
 */
struct cfpkt *cfpkt_append(struct cfpkt *dstpkt, struct cfpkt *addpkt,
		      u16 expectlen);

/*
 * cfpkt_split - Split a packet into two packets at the specified split point.
 * pkt: Packet to be split (will contain the first part of the data on exit)
 * pos: Position to split packet in two parts.
 * @return The new packet, containing the second part of the data.
 */
struct cfpkt *cfpkt_split(struct cfpkt *pkt, u16 pos);

/*
 * Iteration function, iterates the packet buffers from start to end.
 *
 * Checksum iteration function used to iterate buffers
 * (we may have packets consisting of a chain of buffers)
 * pkt:       Packet to calculate checksum for
 * iter_func: Function pointer to iteration function
 * chks:      Checksum calculated so far.
 * buf:       Pointer to the buffer to checksum
 * len:       Length of buf.
 * data:      Initial checksum value.
 * @return    Checksum of buffer.
 */

u16 cfpkt_iterate(struct cfpkt *pkt,
		u16 (*iter_func)(u16 chks, void *buf, u16 len),
		u16 data);

/* Append by giving user access to packet buffer
 * cfpkt Packet to append to
 * buf Buffer inside pkt that user shall copy data into
 * buflen Length of buffer and number of bytes added to packet
 * @return 0 on error, 1 on success
 */
int cfpkt_raw_append(struct cfpkt *cfpkt, void **buf, unsigned int buflen);

/* Extract by giving user access to packet buffer
 * cfpkt Packet to extract from
 * buf Buffer inside pkt that user shall copy data from
 * buflen Length of buffer and number of bytes removed from packet
 * @return 0 on error, 1 on success
 */
int cfpkt_raw_extract(struct cfpkt *cfpkt, void **buf, unsigned int buflen);

/* Map from a "native" packet (e.g. Linux Socket Buffer) to a CAIF packet.
 *  dir - Direction indicating whether this packet is to be sent or received.
 *  nativepkt  - The native packet to be transformed to a CAIF packet
 *  @return The mapped CAIF Packet CFPKT.
 */
struct cfpkt *cfpkt_fromnative(enum caif_direction dir, void *nativepkt);

/* Map from a CAIF packet to a "native" packet (e.g. Linux Socket Buffer).
 *  pkt  - The CAIF packet to be transformed into a "native" packet.
 *  @return The native packet transformed from a CAIF packet.
 */
void *cfpkt_tonative(struct cfpkt *pkt);

/*
 * Insert a packet in the packet queue.
 * pktq Packet queue to insert into
 * pkt Packet to be inserted in queue
 * prio Priority of packet
 */
void cfpkt_queue(struct cfpktq *pktq, struct cfpkt *pkt,
		 unsigned short prio);

/*
 * Remove a packet from the packet queue.
 * pktq Packet queue to fetch packets from.
 * @return Dequeued packet.
 */
struct cfpkt *cfpkt_dequeue(struct cfpktq *pktq);

/*
 * Peek into a packet from the packet queue.
 * pktq Packet queue to fetch packets from.
 * @return Peeked packet.
 */
struct cfpkt *cfpkt_qpeek(struct cfpktq *pktq);

/*
 * Initiates the packet queue.
 * @return Pointer to new packet queue.
 */
struct cfpktq *cfpktq_create(void);

/*
 * Get the number of packets in the queue.
 * pktq Packet queue to fetch count from.
 * @return Number of packets in queue.
 */
int cfpkt_qcount(struct cfpktq *pktq);

/*
 * Put content of packet into buffer for debuging purposes.
 * pkt Packet to copy data from
 * buf Buffer to copy data into
 * buflen Length of data to copy
 * @return Pointer to copied data
 */
char *cfpkt_log_pkt(struct cfpkt *pkt, char *buf, int buflen);

/*
 * Clones a packet and releases the original packet.
 * This is used for taking ownership of a packet e.g queueing.
 * pkt Packet to clone and release.
 * @return Cloned packet.
 */
struct cfpkt *cfpkt_clone_release(struct cfpkt *pkt);


/*
 * Returns packet information for a packet.
 * pkt Packet to get info from;
 * @return Packet information
 */
struct caif_payload_info *cfpkt_info(struct cfpkt *pkt);
/*! @} */
#endif				/* CFPKT_H_ */
