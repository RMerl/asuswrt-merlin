/* RIPngd Zebra
 * Copyright (C) 2002 6WIND <vincent.jardin@6wind.com>
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

/* This file is required in order to support properly the RIPng nexthop
 * feature.
 */

#include <zebra.h>

/* For struct udphdr. */
#include <netinet/udp.h>

#include "linklist.h"
#include "stream.h"
#include "log.h"
#include "memory.h"
#include "vty.h"
#include "if.h"
#include "prefix.h"

#include "ripngd/ripngd.h"
#include "ripngd/ripng_debug.h"
#include "ripngd/ripng_nexthop.h"

#define DEBUG 1

#define min(a, b) ((a) < (b) ? (a) : (b))

struct ripng_rte_data {
  struct prefix_ipv6 *p;
  struct ripng_info *rinfo;
  struct ripng_aggregate *aggregate;
};

void _ripng_rte_del(struct ripng_rte_data *A);
int _ripng_rte_cmp(struct ripng_rte_data *A, struct ripng_rte_data *B);

#define METRIC_OUT(a) \
    ((a)->rinfo ?  (a)->rinfo->metric_out : (a)->aggregate->metric_out)
#define NEXTHOP_OUT_PTR(a) \
    ((a)->rinfo ?  &((a)->rinfo->nexthop_out) : &((a)->aggregate->nexthop_out))
#define TAG_OUT(a) \
    ((a)->rinfo ?  (a)->rinfo->tag_out : (a)->aggregate->tag_out)

struct list *
ripng_rte_new(void) {
  struct list *rte;

  rte = list_new();
  rte->cmp = (int (*)(void *, void *)) _ripng_rte_cmp;
  rte->del = (void (*)(void *)) _ripng_rte_del;

  return rte;
}

void
ripng_rte_free(struct list *ripng_rte_list) {
  list_delete(ripng_rte_list);
}

/* Delete RTE */
void
_ripng_rte_del(struct ripng_rte_data *A) {
  XFREE(MTYPE_RIPNG_RTE_DATA, A);
}

/* Compare RTE:
 *  return +  if A > B
 *         0  if A = B
 *         -  if A < B
 */
int
_ripng_rte_cmp(struct ripng_rte_data *A, struct ripng_rte_data *B) {
  return addr6_cmp(NEXTHOP_OUT_PTR(A), NEXTHOP_OUT_PTR(B));
}

/* Add routing table entry */
void
ripng_rte_add(struct list *ripng_rte_list, struct prefix_ipv6 *p,
              struct ripng_info *rinfo, struct ripng_aggregate *aggregate) {

  struct ripng_rte_data *data;

  /* At least one should not be null */
  assert(!rinfo || !aggregate);

  data = XMALLOC(MTYPE_RIPNG_RTE_DATA, sizeof(*data));
  data->p     = p;
  data->rinfo = rinfo;
  data->aggregate = aggregate;

  listnode_add_sort(ripng_rte_list, data);
} 

/* Send the RTE with the nexthop support
 */
void
ripng_rte_send(struct list *ripng_rte_list, struct interface *ifp,
               struct sockaddr_in6 *to) {

  struct ripng_rte_data *data;
  struct listnode *node, *nnode;

  struct in6_addr last_nexthop;
  struct in6_addr myself_nexthop;

  struct stream *s;
  int num;
  int mtu;
  int rtemax;
  int ret;

  /* Most of the time, there is no nexthop */
  memset(&last_nexthop, 0, sizeof(last_nexthop));

  /* Use myself_nexthop if the nexthop is not a link-local address, because
   * we remain a right path without beeing the optimal one.
   */
  memset(&myself_nexthop, 0, sizeof(myself_nexthop));

  /* Output stream get from ripng structre.  XXX this should be
     interface structure. */
  s = ripng->obuf;

  /* Reset stream and RTE counter. */
  stream_reset (s);
  num = 0;

  mtu = ifp->mtu6;
  if (mtu < 0)
    mtu = IFMINMTU;

  rtemax = (min (mtu, RIPNG_MAX_PACKET_SIZE) -
	    IPV6_HDRLEN - 
	    sizeof (struct udphdr) -
	    sizeof (struct ripng_packet) +
	    sizeof (struct rte)) / sizeof (struct rte);

  for (ALL_LIST_ELEMENTS (ripng_rte_list, node, nnode, data)) {
    /* (2.1) Next hop support */
    if (!IPV6_ADDR_SAME(&last_nexthop, NEXTHOP_OUT_PTR(data))) {

      /* A nexthop entry should be at least followed by 1 RTE */
      if (num == (rtemax-1)) {
	ret = ripng_send_packet ((caddr_t) STREAM_DATA (s), stream_get_endp (s),
				 to, ifp);

        if (ret >= 0 && IS_RIPNG_DEBUG_SEND)
          ripng_packet_dump((struct ripng_packet *)STREAM_DATA (s),
			    stream_get_endp(s), "SEND");
        num = 0;
        stream_reset (s);
      }

      /* Add the nexthop (2.1) */

      /* If the received next hop address is not a link-local address,
       * it should be treated as 0:0:0:0:0:0:0:0.
       */
      if (!IN6_IS_ADDR_LINKLOCAL(NEXTHOP_OUT_PTR(data)))
        last_nexthop = myself_nexthop;
      else
	last_nexthop = *NEXTHOP_OUT_PTR(data);

      num = ripng_write_rte(num, s, NULL, &last_nexthop, 0, RIPNG_METRIC_NEXTHOP);
    } else {
      /* Rewrite the nexthop for each new packet */
      if ((num == 0) && !IPV6_ADDR_SAME(&last_nexthop, &myself_nexthop))
        num = ripng_write_rte(num, s, NULL, &last_nexthop, 0, RIPNG_METRIC_NEXTHOP);
    }
    num = ripng_write_rte(num, s, data->p, NULL,
			  TAG_OUT(data), METRIC_OUT(data));

    if (num == rtemax) {
      ret = ripng_send_packet ((caddr_t) STREAM_DATA (s), stream_get_endp (s),
			       to, ifp);

      if (ret >= 0 && IS_RIPNG_DEBUG_SEND)
        ripng_packet_dump((struct ripng_packet *)STREAM_DATA (s),
			  stream_get_endp(s), "SEND");
      num = 0;
      stream_reset (s);
    }
  }

  /* If unwritten RTE exist, flush it. */
  if (num != 0) {
    ret = ripng_send_packet ((caddr_t) STREAM_DATA (s), stream_get_endp (s),
			     to, ifp);

    if (ret >= 0 && IS_RIPNG_DEBUG_SEND)
      ripng_packet_dump ((struct ripng_packet *)STREAM_DATA (s),
			 stream_get_endp (s), "SEND");
    stream_reset (s);
  }
}
