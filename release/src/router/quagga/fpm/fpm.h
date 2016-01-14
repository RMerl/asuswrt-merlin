/*
 * Public definitions pertaining to the Forwarding Plane Manager component.
 *
 * Permission is granted to use, copy, modify and/or distribute this
 * software under either one of the licenses below.
 *
 * Note that if you use other files from the Quagga tree directly or
 * indirectly, then the licenses in those files still apply.
 *
 * Please retain both licenses below when modifying this code in the
 * Quagga tree.
 *
 * Copyright (C) 2012 by Open Source Routing.
 * Copyright (C) 2012 by Internet Systems Consortium, Inc. ("ISC")
 */

/*
 * License Option 1: GPL
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
 * License Option 2: ISC License
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _FPM_H
#define _FPM_H

/*
 * The Forwarding Plane Manager (FPM) is an optional component that
 * may be used in scenarios where the router has a forwarding path
 * that is distinct from the kernel, commonly a hardware-based fast
 * path. It is responsible for programming forwarding information
 * (such as routes and nexthops) in the fast path.
 *
 * In Quagga, the Routing Information Base is maintained in the
 * 'zebra' infrastructure daemon. Routing protocols communicate their
 * best routes to zebra, and zebra computes the best route across
 * protocols for each prefix. This latter information comprises the
 * bulk of the Forwarding Information Base.
 *
 * This header file defines a point-to-point interface using which
 * zebra can update the FPM about changes in routes. The communication
 * takes place over a stream socket. The FPM listens on a well-known
 * TCP port, and zebra initiates the connection.
 *
 * All messages sent over the connection start with a short FPM
 * header, fpm_msg_hdr_t. In the case of route add/delete messages,
 * the header is followed by a netlink message. Zebra should send a
 * complete copy of the forwarding table(s) to the FPM, including
 * routes that it may have picked up from the kernel.
 *
 * The FPM interface uses replace semantics. That is, if a 'route add'
 * message for a prefix is followed by another 'route add' message, the
 * information in the second message is complete by itself, and replaces
 * the information sent in the first message.
 *
 * If the connection to the FPM goes down for some reason, the client
 * (zebra) should send the FPM a complete copy of the forwarding
 * table(s) when it reconnects.
 */

#define FPM_DEFAULT_PORT 2620

/*
 * Largest message that can be sent to or received from the FPM.
 */
#define FPM_MAX_MSG_LEN 4096

/*
 * Header that precedes each fpm message to/from the FPM.
 */
typedef struct fpm_msg_hdr_t_
{
  /*
   * Protocol version.
   */
  uint8_t version;

  /*
   * Type of message, see below.
   */
  uint8_t msg_type;

  /*
   * Length of entire message, including the header, in network byte
   * order.
   *
   * Note that msg_len is rounded up to make sure that message is at
   * the desired alignment. This means that some payloads may need
   * padding at the end.
   */
  uint16_t msg_len;
} fpm_msg_hdr_t;

/*
 * The current version of the FPM protocol is 1.
 */
#define FPM_PROTO_VERSION 1

typedef enum fpm_msg_type_e_ {
  FPM_MSG_TYPE_NONE = 0,

  /*
   * Indicates that the payload is a completely formed netlink
   * message.
   */
  FPM_MSG_TYPE_NETLINK = 1,
} fpm_msg_type_e;

/*
 * The FPM message header is aligned to the same boundary as netlink
 * messages (4). This means that a netlink message does not need
 * padding when encapsulated in an FPM message.
 */
#define FPM_MSG_ALIGNTO 4

/*
 * fpm_msg_align
 *
 * Round up the given length to the desired alignment.
 */
static inline size_t
fpm_msg_align (size_t len)
{
  return (len + FPM_MSG_ALIGNTO - 1) & ~(FPM_MSG_ALIGNTO - 1);
}

/*
 * The (rounded up) size of the FPM message header. This ensures that
 * the message payload always starts at an aligned address.
 */
#define FPM_MSG_HDR_LEN (fpm_msg_align (sizeof (fpm_msg_hdr_t)))

/*
 * fpm_data_len_to_msg_len
 *
 * The length value that should be placed in the msg_len field of the
 * header for a *payload* of size 'data_len'.
 */
static inline size_t
fpm_data_len_to_msg_len (size_t data_len)
{
  return fpm_msg_align (data_len) + FPM_MSG_HDR_LEN;
}

/*
 * fpm_msg_data
 *
 * Pointer to the payload of the given fpm header.
 */
static inline void *
fpm_msg_data (fpm_msg_hdr_t *hdr)
{
  return ((char*) hdr) + FPM_MSG_HDR_LEN;
}

/*
 * fpm_msg_len
 */
static inline size_t
fpm_msg_len (const fpm_msg_hdr_t *hdr)
{
  return ntohs (hdr->msg_len);
}

/*
 * fpm_msg_data_len
 */
static inline size_t
fpm_msg_data_len (const fpm_msg_hdr_t *hdr)
{
  return (fpm_msg_len (hdr) - FPM_MSG_HDR_LEN);
}

/*
 * fpm_msg_next
 *
 * Move to the next message in a buffer.
 */
static inline fpm_msg_hdr_t *
fpm_msg_next (fpm_msg_hdr_t *hdr, size_t *len)
{
  size_t msg_len;

  msg_len = fpm_msg_len (hdr);

  if (len) {
    if (*len < msg_len)
      {
	assert(0);
	return NULL;
      }
    *len -= msg_len;
  }

  return (fpm_msg_hdr_t *) (((char*) hdr) + msg_len);
}

/*
 * fpm_msg_hdr_ok
 *
 * Returns TRUE if a message header looks well-formed.
 */
static inline int
fpm_msg_hdr_ok (const fpm_msg_hdr_t *hdr)
{
  size_t msg_len;

  if (hdr->msg_type == FPM_MSG_TYPE_NONE)
    return 0;

  msg_len = fpm_msg_len (hdr);

  if (msg_len < FPM_MSG_HDR_LEN || msg_len > FPM_MAX_MSG_LEN)
    return 0;

  if (fpm_msg_align (msg_len) != msg_len)
    return 0;

  return 1;
}

/*
 * fpm_msg_ok
 *
 * Returns TRUE if a message looks well-formed.
 *
 * @param len The length in bytes from 'hdr' to the end of the buffer.
 */
static inline int
fpm_msg_ok (const fpm_msg_hdr_t *hdr, size_t len)
{
  if (len < FPM_MSG_HDR_LEN)
    return 0;

  if (!fpm_msg_hdr_ok (hdr))
    return 0;

  if (fpm_msg_len (hdr) > len)
    return 0;

  return 1;
}

#endif /* _FPM_H */
