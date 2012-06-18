/*
 * OSPF Sending and Receiving OSPF Packets.
 * Copyright (C) 1999, 2000 Toshiaki Takada
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

#include <zebra.h>

#include "thread.h"
#include "memory.h"
#include "linklist.h"
#include "prefix.h"
#include "if.h"
#include "table.h"
#include "sockunion.h"
#include "stream.h"
#include "log.h"
#include "checksum.h"
#include "md5-gnu.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_network.h"
#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_ism.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_lsdb.h"
#include "ospfd/ospf_neighbor.h"
#include "ospfd/ospf_nsm.h"
#include "ospfd/ospf_packet.h"
#include "ospfd/ospf_spf.h"
#include "ospfd/ospf_flood.h"
#include "ospfd/ospf_dump.h"

static void ospf_ls_ack_send_list (struct ospf_interface *, list,
				   struct in_addr);

/* Packet Type String. */
char *ospf_packet_type_str[] =
  {
    "unknown",
    "Hello",
    "Database Description",
    "Link State Request",
    "Link State Update",
    "Link State Acknowledgment",
  };

/* OSPF authentication checking function */
int
ospf_auth_type (struct ospf_interface *oi)
{
  int auth_type;

  if (OSPF_IF_PARAM (oi, auth_type) == OSPF_AUTH_NOTSET)
    auth_type = oi->area->auth_type;
  else
    auth_type = OSPF_IF_PARAM (oi, auth_type);

  /* Handle case where MD5 key list is not configured aka Cisco */
  if (auth_type == OSPF_AUTH_CRYPTOGRAPHIC &&
      list_isempty (OSPF_IF_PARAM (oi, auth_crypt)))
    return OSPF_AUTH_NULL;
  
  return auth_type;

}

/* forward output pointer. */
void
ospf_output_forward (struct stream *s, int size)
{
  s->putp += size;
}

struct ospf_packet *
ospf_packet_new (size_t size)
{
  struct ospf_packet *new;

  new = XCALLOC (MTYPE_OSPF_PACKET, sizeof (struct ospf_packet));
  new->s = stream_new (size);

  return new;
}

void
ospf_packet_free (struct ospf_packet *op)
{
  if (op->s)
    stream_free (op->s);

  XFREE (MTYPE_OSPF_PACKET, op);

  op = NULL;
}

struct ospf_fifo *
ospf_fifo_new ()
{
  struct ospf_fifo *new;

  new = XCALLOC (MTYPE_OSPF_FIFO, sizeof (struct ospf_fifo));
  return new;
}

/* Add new packet to fifo. */
void
ospf_fifo_push (struct ospf_fifo *fifo, struct ospf_packet *op)
{
  if (fifo->tail)
    fifo->tail->next = op;
  else
    fifo->head = op;

  fifo->tail = op;

  fifo->count++;
}

/* Delete first packet from fifo. */
struct ospf_packet *
ospf_fifo_pop (struct ospf_fifo *fifo)
{
  struct ospf_packet *op;

  op = fifo->head;

  if (op)
    {
      fifo->head = op->next;

      if (fifo->head == NULL)
	fifo->tail = NULL;

      fifo->count--;
    }

  return op;
}

/* Return first fifo entry. */
struct ospf_packet *
ospf_fifo_head (struct ospf_fifo *fifo)
{
  return fifo->head;
}

/* Flush ospf packet fifo. */
void
ospf_fifo_flush (struct ospf_fifo *fifo)
{
  struct ospf_packet *op;
  struct ospf_packet *next;

  for (op = fifo->head; op; op = next)
    {
      next = op->next;
      ospf_packet_free (op);
    }
  fifo->head = fifo->tail = NULL;
  fifo->count = 0;
}

/* Free ospf packet fifo. */
void
ospf_fifo_free (struct ospf_fifo *fifo)
{
  ospf_fifo_flush (fifo);

  XFREE (MTYPE_OSPF_FIFO, fifo);
}

void
ospf_packet_add (struct ospf_interface *oi, struct ospf_packet *op)
{
  /* Add packet to end of queue. */
  ospf_fifo_push (oi->obuf, op);

  /* Debug of packet fifo*/
  /* ospf_fifo_debug (oi->obuf); */
}

void
ospf_packet_delete (struct ospf_interface *oi)
{
  struct ospf_packet *op;
  
  op = ospf_fifo_pop (oi->obuf);

  if (op)
    ospf_packet_free (op);
}

struct stream *
ospf_stream_copy (struct stream *new, struct stream *s)
{
  new->endp = s->endp;
  new->putp = s->putp;
  new->getp = s->getp;

  memcpy (new->data, s->data, stream_get_endp (s));

  return new;
}

struct ospf_packet *
ospf_packet_dup (struct ospf_packet *op)
{
  struct ospf_packet *new;

  if (stream_get_endp(op->s) != op->length)
    zlog_warn ("ospf_packet_dup stream %ld ospf_packet %d size mismatch",
	       STREAM_SIZE(op->s), op->length);

  /* Reserve space for MD5 authentication that may be added later. */
  new = ospf_packet_new (stream_get_endp(op->s) + OSPF_AUTH_MD5_SIZE);
  ospf_stream_copy (new->s, op->s);

  new->dst = op->dst;
  new->length = op->length;

  return new;
}

int
ospf_packet_max (struct ospf_interface *oi)
{
  int max;

  if ( ospf_auth_type (oi) == OSPF_AUTH_CRYPTOGRAPHIC)
    max = oi->ifp->mtu - OSPF_AUTH_MD5_SIZE - 88;
  else
    max = oi->ifp->mtu - 88;

  return max;
}


int
ospf_check_md5_digest (struct ospf_interface *oi, struct stream *s,
                       u_int16_t length)
{
  void *ibuf;
  struct md5_ctx ctx;
  unsigned char digest[OSPF_AUTH_MD5_SIZE];
  unsigned char *pdigest;
  struct crypt_key *ck;
  struct ospf_header *ospfh;
  struct ospf_neighbor *nbr;
  

  ibuf = STREAM_PNT (s);
  ospfh = (struct ospf_header *) ibuf;

  /* Get pointer to the end of the packet. */
  pdigest = (unsigned char *) ibuf + length;

  /* Get secret key. */
  ck = ospf_crypt_key_lookup (OSPF_IF_PARAM (oi, auth_crypt),
			      ospfh->u.crypt.key_id);
  if (ck == NULL)
    {
      zlog_warn ("interface %s: ospf_check_md5 no key %d",
		 IF_NAME (oi), ospfh->u.crypt.key_id);
      return 0;
    }

  /* check crypto seqnum. */
  nbr = ospf_nbr_lookup_by_routerid (oi->nbrs, &ospfh->router_id);

  if (nbr && ntohl(nbr->crypt_seqnum) > ntohl(ospfh->u.crypt.crypt_seqnum))
    {
      zlog_warn ("interface %s: ospf_check_md5 bad sequence %d (expect %d)",
		 IF_NAME (oi),
		 ntohl(ospfh->u.crypt.crypt_seqnum),
		 ntohl(nbr->crypt_seqnum));
      return 0;
    }
      
  /* Generate a digest for the ospf packet - their digest + our digest. */
  md5_init_ctx (&ctx);
  md5_process_bytes (ibuf, length, &ctx);
  md5_process_bytes (ck->auth_key, OSPF_AUTH_MD5_SIZE, &ctx);
  md5_finish_ctx (&ctx, digest);

  /* compare the two */
  if (memcmp (pdigest, digest, OSPF_AUTH_MD5_SIZE))
    {
      zlog_warn ("interface %s: ospf_check_md5 checksum mismatch",
		 IF_NAME (oi));
      return 0;
    }

  /* save neighbor's crypt_seqnum */
  if (nbr)
    nbr->crypt_seqnum = ospfh->u.crypt.crypt_seqnum;
  return 1;
}

/* This function is called from ospf_write(), it will detect the
   authentication scheme and if it is MD5, it will change the sequence
   and update the MD5 digest. */
int
ospf_make_md5_digest (struct ospf_interface *oi, struct ospf_packet *op)
{
  struct ospf_header *ospfh;
  unsigned char digest[OSPF_AUTH_MD5_SIZE];
  struct md5_ctx ctx;
  void *ibuf;
  unsigned long oldputp;
  u_int32_t t;
  struct crypt_key *ck;
  char *auth_key;

  ibuf = STREAM_DATA (op->s);
  ospfh = (struct ospf_header *) ibuf;

  if (ntohs (ospfh->auth_type) != OSPF_AUTH_CRYPTOGRAPHIC)
    return 0;

  /* We do this here so when we dup a packet, we don't have to
     waste CPU rewriting other headers. */
  t = (time(NULL) & 0xFFFFFFFF);
  oi->crypt_seqnum = ( t > oi->crypt_seqnum ? t : oi->crypt_seqnum++);
  ospfh->u.crypt.crypt_seqnum = htonl (oi->crypt_seqnum); 

  /* Get MD5 Authentication key from auth_key list. */
  if (list_isempty (OSPF_IF_PARAM (oi, auth_crypt)))
    auth_key = "";
  else
    {
      ck = getdata (OSPF_IF_PARAM (oi, auth_crypt)->tail);
      auth_key = ck->auth_key;
    }

  /* Generate a digest for the entire packet + our secret key. */
  md5_init_ctx (&ctx);
  md5_process_bytes (ibuf, ntohs (ospfh->length), &ctx);
  md5_process_bytes (auth_key, OSPF_AUTH_MD5_SIZE, &ctx);
  md5_finish_ctx (&ctx, digest);

  /* Append md5 digest to the end of the stream. */
  oldputp = stream_get_putp (op->s);
  stream_set_putp (op->s, ntohs (ospfh->length));
  stream_put (op->s, digest, OSPF_AUTH_MD5_SIZE);
  stream_set_putp (op->s, oldputp);

  /* We do *NOT* increment the OSPF header length. */
  op->length = ntohs (ospfh->length) + OSPF_AUTH_MD5_SIZE;

  if (stream_get_endp(op->s) != op->length)
    zlog_warn("ospf_make_md5_digest: length mismatch stream %ld ospf_packet %d", stream_get_endp(op->s), op->length);

  return OSPF_AUTH_MD5_SIZE;
}


int
ospf_ls_req_timer (struct thread *thread)
{
  struct ospf_neighbor *nbr;

  nbr = THREAD_ARG (thread);
  nbr->t_ls_req = NULL;

  /* Send Link State Request. */
  if (ospf_ls_request_count (nbr))
    ospf_ls_req_send (nbr);

  /* Set Link State Request retransmission timer. */
  OSPF_NSM_TIMER_ON (nbr->t_ls_req, ospf_ls_req_timer, nbr->v_ls_req);

  return 0;
}

void
ospf_ls_req_event (struct ospf_neighbor *nbr)
{
  if (nbr->t_ls_req)
    {
      thread_cancel (nbr->t_ls_req);
      nbr->t_ls_req = NULL;
    }
  nbr->t_ls_req = thread_add_event (master, ospf_ls_req_timer, nbr, 0);
}

/* Cyclic timer function.  Fist registered in ospf_nbr_new () in
   ospf_neighbor.c  */
int
ospf_ls_upd_timer (struct thread *thread)
{
  struct ospf_neighbor *nbr;

  nbr = THREAD_ARG (thread);
  nbr->t_ls_upd = NULL;

  /* Send Link State Update. */
  if (ospf_ls_retransmit_count (nbr) > 0)
    {
      list update;
      struct ospf_lsdb *lsdb;
      int i;
      struct timeval now;
      int retransmit_interval;

      gettimeofday (&now, NULL);
      retransmit_interval = OSPF_IF_PARAM (nbr->oi, retransmit_interval);

      lsdb = &nbr->ls_rxmt;
      update = list_new ();

      for (i = OSPF_MIN_LSA; i < OSPF_MAX_LSA; i++)
	{
	  struct route_table *table = lsdb->type[i].db;
	  struct route_node *rn;
	  
	  for (rn = route_top (table); rn; rn = route_next (rn))
	    {
	      struct ospf_lsa *lsa;
	      
	      if ((lsa = rn->info) != NULL)
		/* Don't retransmit an LSA if we received it within
		   the last RxmtInterval seconds - this is to allow the
		   neighbour a chance to acknowledge the LSA as it may
		   have ben just received before the retransmit timer
		   fired.  This is a small tweak to what is in the RFC,
		   but it will cut out out a lot of retransmit traffic
		   - MAG */
		if (tv_cmp (tv_sub (now, lsa->tv_recv), 
			    int2tv (retransmit_interval)) >= 0)
		  listnode_add (update, rn->info);
	    }
	}

      if (listcount (update) > 0)
	ospf_ls_upd_send (nbr, update, OSPF_SEND_PACKET_DIRECT);
      list_delete (update);
    }

  /* Set LS Update retransmission timer. */
  OSPF_NSM_TIMER_ON (nbr->t_ls_upd, ospf_ls_upd_timer, nbr->v_ls_upd);

  return 0;
}

int
ospf_ls_ack_timer (struct thread *thread)
{
  struct ospf_interface *oi;

  oi = THREAD_ARG (thread);
  oi->t_ls_ack = NULL;

  /* Send Link State Acknowledgment. */
  if (listcount (oi->ls_ack) > 0)
    ospf_ls_ack_send_delayed (oi);

  /* Set LS Ack timer. */
  OSPF_ISM_TIMER_ON (oi->t_ls_ack, ospf_ls_ack_timer, oi->v_ls_ack);

  return 0;
}

int
ospf_write (struct thread *thread)
{
  struct ospf *ospf = THREAD_ARG (thread);
  struct ospf_interface *oi;
  struct ospf_packet *op;
  struct sockaddr_in sa_dst;
  struct ip iph;
  struct msghdr msg;
  struct iovec iov[2];
  u_char type;
  int ret;
  int flags = 0;
  listnode node;
  
  ospf->t_write = NULL;

  node = listhead (ospf->oi_write_q);
  assert (node);
  oi = getdata (node);
  assert (oi);
  
  /* Get one packet from queue. */
  op = ospf_fifo_head (oi->obuf);
  assert (op);
  assert (op->length >= OSPF_HEADER_SIZE);

  if (op->dst.s_addr == htonl (OSPF_ALLSPFROUTERS)
      || op->dst.s_addr == htonl (OSPF_ALLDROUTERS))
    ospf_if_ipmulticast (ospf, oi->address, oi->ifp->ifindex);

  /* Rewrite the md5 signature & update the seq */
  ospf_make_md5_digest (oi, op);

  memset (&sa_dst, 0, sizeof (sa_dst));
  sa_dst.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
  sa_dst.sin_len = sizeof(sa_dst);
#endif /* HAVE_SIN_LEN */
  sa_dst.sin_addr = op->dst;
  sa_dst.sin_port = htons (0);

  /* Set DONTROUTE flag if dst is unicast. */
  if (oi->type != OSPF_IFTYPE_VIRTUALLINK)
    if (!IN_MULTICAST (htonl (op->dst.s_addr)))
      flags = MSG_DONTROUTE;

  iph.ip_hl = sizeof (struct ip) >> 2;
  iph.ip_v = IPVERSION;
  iph.ip_tos = IPTOS_PREC_INTERNETCONTROL;
#if defined(__NetBSD__) || defined(__FreeBSD__)
  iph.ip_len = iph.ip_hl*4 + op->length;
#else
  iph.ip_len = htons (iph.ip_hl*4 + op->length);
#endif
  iph.ip_id = 0;
  iph.ip_off = 0;
  if (oi->type == OSPF_IFTYPE_VIRTUALLINK)
    iph.ip_ttl = OSPF_VL_IP_TTL;
  else
    iph.ip_ttl = OSPF_IP_TTL;
  iph.ip_p = IPPROTO_OSPFIGP;
  iph.ip_sum = 0;
  iph.ip_src.s_addr = oi->address->u.prefix4.s_addr;
  iph.ip_dst.s_addr = op->dst.s_addr;

  memset (&msg, 0, sizeof (msg));
  msg.msg_name = &sa_dst;
  msg.msg_namelen = sizeof (sa_dst); 
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;
  iov[0].iov_base = (char*)&iph;
  iov[0].iov_len = iph.ip_hl*4;
  iov[1].iov_base = STREAM_DATA (op->s);
  iov[1].iov_len = op->length;

  ret = sendmsg (ospf->fd, &msg, flags);
  
  if (ret < 0)
    zlog_warn ("*** sendmsg in ospf_write failed with %s", strerror (errno));

  /* Retrieve OSPF packet type. */
  stream_set_getp (op->s, 1);
  type = stream_getc (op->s);

  /* Show debug sending packet. */
  if (IS_DEBUG_OSPF_PACKET (type - 1, SEND))
    {
      if (IS_DEBUG_OSPF_PACKET (type - 1, DETAIL))
	{
	  zlog_info ("-----------------------------------------------------");
	  stream_set_getp (op->s, 0);
	  ospf_packet_dump (op->s);
	}

      zlog_info ("%s sent to [%s] via [%s].",
		 ospf_packet_type_str[type], inet_ntoa (op->dst),
		 IF_NAME (oi));

      if (IS_DEBUG_OSPF_PACKET (type - 1, DETAIL))
	zlog_info ("-----------------------------------------------------");
    }

  /* Now delete packet from queue. */
  ospf_packet_delete (oi);

  if (ospf_fifo_head (oi->obuf) == NULL)
    {
      oi->on_write_q = 0;
      list_delete_node (ospf->oi_write_q, node);
    }
  
  /* If packets still remain in queue, call write thread. */
  if (!list_isempty (ospf->oi_write_q))
    ospf->t_write =                                              
      thread_add_write (master, ospf_write, ospf, ospf->fd);

  return 0;
}

/* OSPF Hello message read -- RFC2328 Section 10.5. */
void
ospf_hello (struct ip *iph, struct ospf_header *ospfh,
	    struct stream * s, struct ospf_interface *oi, int size)
{
  struct ospf_hello *hello;
  struct ospf_neighbor *nbr;
  struct route_node *rn;
  struct prefix p, key;
  int old_state;

  /* increment statistics. */
  oi->hello_in++;

  hello = (struct ospf_hello *) STREAM_PNT (s);

  /* If Hello is myself, silently discard. */
  if (IPV4_ADDR_SAME (&ospfh->router_id, &oi->ospf->router_id))
    return;

  /* If incoming interface is passive one, ignore Hello. */
  if (OSPF_IF_PARAM (oi, passive_interface) == OSPF_IF_PASSIVE)
    return;

  /* get neighbor prefix. */
  p.family = AF_INET;
  p.prefixlen = ip_masklen (hello->network_mask);
  p.u.prefix4 = iph->ip_src;

  /* Compare network mask. */
  /* Checking is ignored for Point-to-Point and Virtual link. */
  if (oi->type != OSPF_IFTYPE_POINTOPOINT 
      && oi->type != OSPF_IFTYPE_VIRTUALLINK)
    if (oi->address->prefixlen != p.prefixlen)
      {
	zlog_warn ("Packet %s [Hello:RECV]: NetworkMask mismatch.",
		   inet_ntoa (ospfh->router_id));
	return;
      }

  /* Compare Hello Interval. */
  if (OSPF_IF_PARAM (oi, v_hello) != ntohs (hello->hello_interval))
    {
      zlog_warn ("Packet %s [Hello:RECV]: HelloInterval mismatch.",
		 inet_ntoa (ospfh->router_id));
      return;
    }

  /* Compare Router Dead Interval. */
  if (OSPF_IF_PARAM (oi, v_wait) != ntohl (hello->dead_interval))
    {
      zlog_warn ("Packet %s [Hello:RECV]: RouterDeadInterval mismatch.",
		 inet_ntoa (ospfh->router_id));
      return;
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Packet %s [Hello:RECV]: Options %s",
	       inet_ntoa (ospfh->router_id),
	       ospf_options_dump (hello->options));

  /* Compare options. */
#define REJECT_IF_TBIT_ON	1 /* XXX */
#ifdef REJECT_IF_TBIT_ON
  if (CHECK_FLAG (hello->options, OSPF_OPTION_T))
    {
      /*
       * This router does not support non-zero TOS.
       * Drop this Hello packet not to establish neighbor relationship.
       */
      zlog_warn ("Packet %s [Hello:RECV]: T-bit on, drop it.",
		 inet_ntoa (ospfh->router_id));
      return;
    }
#endif /* REJECT_IF_TBIT_ON */

#ifdef HAVE_OPAQUE_LSA
  if (CHECK_FLAG (oi->ospf->config, OSPF_OPAQUE_CAPABLE)
      && CHECK_FLAG (hello->options, OSPF_OPTION_O))
    {
      /*
       * This router does know the correct usage of O-bit
       * the bit should be set in DD packet only.
       */
      zlog_warn ("Packet %s [Hello:RECV]: O-bit abuse?",
		 inet_ntoa (ospfh->router_id));
#ifdef STRICT_OBIT_USAGE_CHECK
      return;                                     /* Reject this packet. */
#else /* STRICT_OBIT_USAGE_CHECK */
      UNSET_FLAG (hello->options, OSPF_OPTION_O); /* Ignore O-bit. */
#endif /* STRICT_OBIT_USAGE_CHECK */
    }
#endif /* HAVE_OPAQUE_LSA */

  /* new for NSSA is to ensure that NP is on and E is off */

#ifdef HAVE_NSSA
  if (oi->area->external_routing == OSPF_AREA_NSSA) 
    {
      if (! (CHECK_FLAG (OPTIONS (oi), OSPF_OPTION_NP)
	     && CHECK_FLAG (hello->options, OSPF_OPTION_NP)
	     && ! CHECK_FLAG (OPTIONS (oi), OSPF_OPTION_E)
	     && ! CHECK_FLAG (hello->options, OSPF_OPTION_E)))
	{
	  zlog_warn ("NSSA-Packet-%s[Hello:RECV]: my options: %x, his options %x", inet_ntoa (ospfh->router_id), OPTIONS (oi), hello->options);
	  return;
	}
      if (IS_DEBUG_OSPF_NSSA)
        zlog_info ("NSSA-Hello:RECV:Packet from %s:", inet_ntoa(ospfh->router_id));
    }
  else    
#endif /* HAVE_NSSA */
    /* The setting of the E-bit found in the Hello Packet's Options
       field must match this area's ExternalRoutingCapability A
       mismatch causes processing to stop and the packet to be
       dropped. The setting of the rest of the bits in the Hello
       Packet's Options field should be ignored. */
    if (CHECK_FLAG (OPTIONS (oi), OSPF_OPTION_E) !=
	CHECK_FLAG (hello->options, OSPF_OPTION_E))
      {
	zlog_warn ("Packet[Hello:RECV]: my options: %x, his options %x",
		   OPTIONS (oi), hello->options);
	return;
      }


  /* Get neighbor information from table. */
  key.family = AF_INET;
  key.prefixlen = IPV4_MAX_BITLEN;
  key.u.prefix4 = iph->ip_src;

  rn = route_node_get (oi->nbrs, &key);
  if (rn->info)
    {
      route_unlock_node (rn);
      nbr = rn->info;

      if (oi->type == OSPF_IFTYPE_NBMA && nbr->state == NSM_Attempt)
	{
	  nbr->src = iph->ip_src;
	  nbr->address = p;
	}
    }
  else
    {
      /* Create new OSPF Neighbor structure. */
      nbr = ospf_nbr_new (oi);
      nbr->state = NSM_Down;
      nbr->src = iph->ip_src;
      nbr->address = p;

      rn->info = nbr;

      nbr->nbr_nbma = NULL;

      if (oi->type == OSPF_IFTYPE_NBMA)
	{
	  struct ospf_nbr_nbma *nbr_nbma;
	  listnode node;

	  for (node = listhead (oi->nbr_nbma); node; nextnode (node))
	    {
	      nbr_nbma = getdata (node);
	      assert (nbr_nbma);
      
	      if (IPV4_ADDR_SAME(&nbr_nbma->addr, &iph->ip_src))
		{
		  nbr_nbma->nbr = nbr;
		  nbr->nbr_nbma = nbr_nbma;

		  if (nbr_nbma->t_poll)
		    OSPF_POLL_TIMER_OFF (nbr_nbma->t_poll);
		  
		  nbr->state_change = nbr_nbma->state_change + 1;
		}
	    }
	}
      
      /* New nbr, save the crypto sequence number if necessary */
      if (ntohs (ospfh->auth_type) == OSPF_AUTH_CRYPTOGRAPHIC)
	nbr->crypt_seqnum = ospfh->u.crypt.crypt_seqnum;

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("NSM[%s:%s]: start", IF_NAME (nbr->oi),
		   inet_ntoa (nbr->router_id));
    }
  
  nbr->router_id = ospfh->router_id;

  old_state = nbr->state;

  /* Add event to thread. */
  OSPF_NSM_EVENT_EXECUTE (nbr, NSM_HelloReceived);

  /*  RFC2328  Section 9.5.1
      If the router is not eligible to become Designated Router,
      (snip)   It	must also send an Hello	Packet in reply	to an
      Hello Packet received from any eligible neighbor (other than
      the	current	Designated Router and Backup Designated	Router).  */
  if (oi->type == OSPF_IFTYPE_NBMA)
    if (PRIORITY(oi) == 0 && hello->priority > 0
	&& IPV4_ADDR_CMP(&DR(oi),  &iph->ip_src)
	&& IPV4_ADDR_CMP(&BDR(oi), &iph->ip_src))
      OSPF_NSM_TIMER_ON (nbr->t_hello_reply, ospf_hello_reply_timer,
			 OSPF_HELLO_REPLY_DELAY);

  /* on NBMA network type, it happens to receive bidirectional Hello packet
     without advance 1-Way Received event.
     To avoid incorrect DR-seletion, raise 1-Way Received event.*/
  if (oi->type == OSPF_IFTYPE_NBMA &&
      (old_state == NSM_Down || old_state == NSM_Attempt))
    {
      OSPF_NSM_EVENT_EXECUTE (nbr, NSM_OneWayReceived);
      nbr->priority = hello->priority;
      nbr->d_router = hello->d_router;
      nbr->bd_router = hello->bd_router;
      return;
    }

  if (ospf_nbr_bidirectional (&oi->ospf->router_id, hello->neighbors,
			      size - OSPF_HELLO_MIN_SIZE))
    {
      OSPF_NSM_EVENT_EXECUTE (nbr, NSM_TwoWayReceived);
      nbr->options |= hello->options;
    }
  else
    {
      OSPF_NSM_EVENT_EXECUTE (nbr, NSM_OneWayReceived);
      /* Set neighbor information. */
      nbr->priority = hello->priority;
      nbr->d_router = hello->d_router;
      nbr->bd_router = hello->bd_router;
      return;
    }

  /* If neighbor itself declares DR and no BDR exists,
     cause event BackupSeen */
  if (IPV4_ADDR_SAME (&nbr->address.u.prefix4, &hello->d_router))
    if (hello->bd_router.s_addr == 0 && oi->state == ISM_Waiting)
      OSPF_ISM_EVENT_SCHEDULE (oi, ISM_BackupSeen);

  /* neighbor itself declares BDR. */
  if (oi->state == ISM_Waiting &&
      IPV4_ADDR_SAME (&nbr->address.u.prefix4, &hello->bd_router))
    OSPF_ISM_EVENT_SCHEDULE (oi, ISM_BackupSeen);

  /* had not previously. */
  if ((IPV4_ADDR_SAME (&nbr->address.u.prefix4, &hello->d_router) &&
       IPV4_ADDR_CMP (&nbr->address.u.prefix4, &nbr->d_router)) ||
      (IPV4_ADDR_CMP (&nbr->address.u.prefix4, &hello->d_router) &&
       IPV4_ADDR_SAME (&nbr->address.u.prefix4, &nbr->d_router)))
    OSPF_ISM_EVENT_SCHEDULE (oi, ISM_NeighborChange);

  /* had not previously. */
  if ((IPV4_ADDR_SAME (&nbr->address.u.prefix4, &hello->bd_router) &&
       IPV4_ADDR_CMP (&nbr->address.u.prefix4, &nbr->bd_router)) ||
      (IPV4_ADDR_CMP (&nbr->address.u.prefix4, &hello->bd_router) &&
       IPV4_ADDR_SAME (&nbr->address.u.prefix4, &nbr->bd_router)))
    OSPF_ISM_EVENT_SCHEDULE (oi, ISM_NeighborChange);

  /* Neighbor priority check. */
  if (nbr->priority >= 0 && nbr->priority != hello->priority)
    OSPF_ISM_EVENT_SCHEDULE (oi, ISM_NeighborChange);

  /* Set neighbor information. */
  nbr->priority = hello->priority;
  nbr->d_router = hello->d_router;
  nbr->bd_router = hello->bd_router;
}

/* Save DD flags/options/Seqnum received. */
void
ospf_db_desc_save_current (struct ospf_neighbor *nbr,
			   struct ospf_db_desc *dd)
{
  nbr->last_recv.flags = dd->flags;
  nbr->last_recv.options = dd->options;
  nbr->last_recv.dd_seqnum = ntohl (dd->dd_seqnum);
}

/* Process rest of DD packet. */
static void
ospf_db_desc_proc (struct stream *s, struct ospf_interface *oi,
		   struct ospf_neighbor *nbr, struct ospf_db_desc *dd,
		   u_int16_t size)
{
  struct ospf_lsa *new, *find;
  struct lsa_header *lsah;

  stream_forward (s, OSPF_DB_DESC_MIN_SIZE);
  for (size -= OSPF_DB_DESC_MIN_SIZE;
       size >= OSPF_LSA_HEADER_SIZE; size -= OSPF_LSA_HEADER_SIZE) 
    {
      lsah = (struct lsa_header *) STREAM_PNT (s);
      stream_forward (s, OSPF_LSA_HEADER_SIZE);

      /* Unknown LS type. */
      if (lsah->type < OSPF_MIN_LSA || lsah->type >= OSPF_MAX_LSA)
	{
	  zlog_warn ("Pakcet [DD:RECV]: Unknown LS type %d.", lsah->type);
	  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_SeqNumberMismatch);
	  return;
	}

#ifdef HAVE_OPAQUE_LSA
      if (IS_OPAQUE_LSA (lsah->type)
	  &&  ! CHECK_FLAG (nbr->options, OSPF_OPTION_O))
        {
          zlog_warn ("LSA[Type%d:%s]: Opaque capability mismatch?", lsah->type, inet_ntoa (lsah->id));
          OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_SeqNumberMismatch);
          return;
        }
#endif /* HAVE_OPAQUE_LSA */

      switch (lsah->type)
        {
        case OSPF_AS_EXTERNAL_LSA:
#ifdef HAVE_OPAQUE_LSA
	case OSPF_OPAQUE_AS_LSA:
#endif /* HAVE_OPAQUE_LSA */
#ifdef HAVE_NSSA
          /* Check for stub area.  Reject if AS-External from stub but
             allow if from NSSA. */
          if (oi->area->external_routing == OSPF_AREA_STUB)
#else /* ! HAVE_NSSA */
	    if (oi->area->external_routing != OSPF_AREA_DEFAULT)
#endif /* HAVE_NSSA */
	      {
		zlog_warn ("Packet [DD:RECV]: LSA[Type%d:%s] from %s area.",
			   lsah->type, inet_ntoa (lsah->id),
			   (oi->area->external_routing == OSPF_AREA_STUB) ?\
			   "STUB" : "NSSA");
		OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_SeqNumberMismatch);
		return;
	      }
          break;
	default:
	  break;
        }

      /* Create LS-request object. */
      new = ospf_ls_request_new (lsah);

      /* Lookup received LSA, then add LS request list. */
      find = ospf_lsa_lookup_by_header (oi->area, lsah);
      if (!find || ospf_lsa_more_recent (find, new) < 0)
	{
	  ospf_ls_request_add (nbr, new);
	  ospf_lsa_discard (new);
	}
      else
	{
	  /* Received LSA is not recent. */
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("Packet [DD:RECV]: LSA received Type %d, "
		       "ID %s is not recent.", lsah->type, inet_ntoa (lsah->id));
	  ospf_lsa_discard (new);
	  continue;
	}
    }

  /* Master */
  if (IS_SET_DD_MS (nbr->dd_flags))
    {
      nbr->dd_seqnum++;
      /* Entire DD packet sent. */
      if (!IS_SET_DD_M (dd->flags) && !IS_SET_DD_M (nbr->dd_flags))
	OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_ExchangeDone);
      else
	/* Send new DD packet. */
	ospf_db_desc_send (nbr);
    }
  /* Slave */
  else
    {
      nbr->dd_seqnum = ntohl (dd->dd_seqnum);

      /* When master's more flags is not set. */
      if (!IS_SET_DD_M (dd->flags) && ospf_db_summary_isempty (nbr))
	{
	  nbr->dd_flags &= ~(OSPF_DD_FLAG_M);
	  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_ExchangeDone);
	}

      /* Send DD pakcet in reply. */
      ospf_db_desc_send (nbr);
    }

  /* Save received neighbor values from DD. */
  ospf_db_desc_save_current (nbr, dd);
}

int
ospf_db_desc_is_dup (struct ospf_db_desc *dd, struct ospf_neighbor *nbr)
{
  /* Is DD duplicated? */
  if (dd->options == nbr->last_recv.options &&
      dd->flags == nbr->last_recv.flags &&
      dd->dd_seqnum == htonl (nbr->last_recv.dd_seqnum))
    return 1;

  return 0;
}

/* OSPF Database Description message read -- RFC2328 Section 10.6. */
void
ospf_db_desc (struct ip *iph, struct ospf_header *ospfh,
	      struct stream *s, struct ospf_interface *oi, u_int16_t size)
{
  struct ospf_db_desc *dd;
  struct ospf_neighbor *nbr;

  /* Increment statistics. */
  oi->db_desc_in++;

  dd = (struct ospf_db_desc *) STREAM_PNT (s);

  nbr = ospf_nbr_lookup_by_addr (oi->nbrs, &iph->ip_src);
  if (nbr == NULL)
    {
      zlog_warn ("Packet[DD]: Unknown Neighbor %s",
		 inet_ntoa (ospfh->router_id));
      return;
    }

  /* Check MTU. */
  if (ntohs (dd->mtu) > oi->ifp->mtu)
    {
      zlog_warn ("Packet[DD]: MTU is larger than [%s]'s MTU", IF_NAME (oi));
      return;
    }

#ifdef REJECT_IF_TBIT_ON
  if (CHECK_FLAG (dd->options, OSPF_OPTION_T))
    {
      /*
       * In Hello protocol, optional capability must have checked
       * to prevent this T-bit enabled router be my neighbor.
       */
      zlog_warn ("Packet[DD]: Neighbor %s: T-bit on?", inet_ntoa (nbr->router_id));
      return;
    }
#endif /* REJECT_IF_TBIT_ON */

#ifdef HAVE_OPAQUE_LSA
  if (CHECK_FLAG (dd->options, OSPF_OPTION_O)
      && !CHECK_FLAG (oi->ospf->config, OSPF_OPAQUE_CAPABLE))
    {
      /*
       * This node is not configured to handle O-bit, for now.
       * Clear it to ignore unsupported capability proposed by neighbor.
       */
      UNSET_FLAG (dd->options, OSPF_OPTION_O);
    }
#endif /* HAVE_OPAQUE_LSA */

  /* Process DD packet by neighbor status. */
  switch (nbr->state)
    {
    case NSM_Down:
    case NSM_Attempt:
    case NSM_TwoWay:
      zlog_warn ("Packet[DD]: Neighbor state is %s, packet discarded.",
		 LOOKUP (ospf_nsm_state_msg, nbr->state));
      break;
    case NSM_Init:
      OSPF_NSM_EVENT_EXECUTE (nbr, NSM_TwoWayReceived);
      /* If the new state is ExStart, the processing of the current
	 packet should then continue in this new state by falling
	 through to case ExStart below.  */
      if (nbr->state != NSM_ExStart)
	break;
    case NSM_ExStart:
      /* Initial DBD */
      if ((IS_SET_DD_ALL (dd->flags) == OSPF_DD_FLAG_ALL) &&
	  (size == OSPF_DB_DESC_MIN_SIZE))
	{
	  if (IPV4_ADDR_CMP (&nbr->router_id, &oi->ospf->router_id) > 0)
	    {
	      /* We're Slave---obey */
	      zlog_warn ("Packet[DD]: Negotiation done (Slave).");
	      nbr->dd_seqnum = ntohl (dd->dd_seqnum);
	      nbr->dd_flags &= ~(OSPF_DD_FLAG_MS|OSPF_DD_FLAG_I); /* Reset I/MS */
	    }
	  else
	    {
	      /* We're Master, ignore the initial DBD from Slave */
	      zlog_warn ("Packet[DD]: Initial DBD from Slave, ignoring.");
	      break;
	    }
	}
      /* Ack from the Slave */
      else if (!IS_SET_DD_MS (dd->flags) && !IS_SET_DD_I (dd->flags) &&
	       ntohl (dd->dd_seqnum) == nbr->dd_seqnum &&
	       IPV4_ADDR_CMP (&nbr->router_id, &oi->ospf->router_id) < 0)
	{
	  zlog_warn ("Packet[DD]: Negotiation done (Master).");
	  nbr->dd_flags &= ~OSPF_DD_FLAG_I;
	}
      else
	{
	  zlog_warn ("Packet[DD]: Negotiation fails.");
	  break;
	}
      
      /* This is where the real Options are saved */
      nbr->options = dd->options;

#ifdef HAVE_OPAQUE_LSA
      if (CHECK_FLAG (oi->ospf->config, OSPF_OPAQUE_CAPABLE))
        {
          if (IS_DEBUG_OSPF_EVENT)
            zlog_info ("Neighbor[%s] is %sOpaque-capable.",
		       inet_ntoa (nbr->router_id),
		       CHECK_FLAG (nbr->options, OSPF_OPTION_O) ? "" : "NOT ");

          if (! CHECK_FLAG (nbr->options, OSPF_OPTION_O)
	      &&  IPV4_ADDR_SAME (&DR (oi), &nbr->address.u.prefix4))
            {
              zlog_warn ("DR-neighbor[%s] is NOT opaque-capable; Opaque-LSAs cannot be reliably advertised in this network.", inet_ntoa (nbr->router_id));
              /* This situation is undesirable, but not a real error. */
            }
        }
#endif /* HAVE_OPAQUE_LSA */

      OSPF_NSM_EVENT_EXECUTE (nbr, NSM_NegotiationDone);

      /* continue processing rest of packet. */
      ospf_db_desc_proc (s, oi, nbr, dd, size);
      break;
    case NSM_Exchange:
      if (ospf_db_desc_is_dup (dd, nbr))
	{
	  if (IS_SET_DD_MS (nbr->dd_flags))
	    /* Master: discard duplicated DD packet. */
	    zlog_warn ("Packet[DD] (Master): packet duplicated.");
	  else
	    /* Slave: cause to retransmit the last Database Description. */
	    {
	      zlog_warn ("Packet[DD] [Slave]: packet duplicated.");
	      ospf_db_desc_resend (nbr);
	    }
	  break;
	}

      /* Otherwise DD packet should be checked. */
      /* Check Master/Slave bit mismatch */
      if (IS_SET_DD_MS (dd->flags) != IS_SET_DD_MS (nbr->last_recv.flags))
	{
	  zlog_warn ("Packet[DD]: MS-bit mismatch.");
	  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_SeqNumberMismatch);
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("Packet[DD]: dd->flags=%d, nbr->dd_flags=%d",
		       dd->flags, nbr->dd_flags);
	  break;
	}

      /* Check initialize bit is set. */
      if (IS_SET_DD_I (dd->flags))
	{
	  zlog_warn ("Packet[DD]: I-bit set.");
	  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_SeqNumberMismatch);
	  break;
	}

      /* Check DD Options. */
      if (dd->options != nbr->options)
	{
	  zlog_warn ("Packet[DD]: options mismatch.");
	  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_SeqNumberMismatch);
	  break;
	}

      /* Check DD sequence number. */
      if ((IS_SET_DD_MS (nbr->dd_flags) &&
	   ntohl (dd->dd_seqnum) != nbr->dd_seqnum) ||
	  (!IS_SET_DD_MS (nbr->dd_flags) &&
	   ntohl (dd->dd_seqnum) != nbr->dd_seqnum + 1))
	{
	  zlog_warn ("Pakcet[DD]: sequence number mismatch.");
	  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_SeqNumberMismatch);
	  break;
	}

      /* Continue processing rest of packet. */
      ospf_db_desc_proc (s, oi, nbr, dd, size);
      break;
    case NSM_Loading:
    case NSM_Full:
      if (ospf_db_desc_is_dup (dd, nbr))
	{
	  if (IS_SET_DD_MS (nbr->dd_flags))
	    {
	      /* Master should discard duplicate DD packet. */
	      zlog_warn ("Pakcet[DD]: duplicated, packet discarded.");
	      break;
	    }
	  else
	    {
	      struct timeval t, now;
	      gettimeofday (&now, NULL);
	      t = tv_sub (now, nbr->last_send_ts);
	      if (tv_cmp (t, int2tv (nbr->v_inactivity)) < 0)
		{
		  /* In states Loading and Full the slave must resend
		     its last Database Description packet in response to
		     duplicate Database Description packets received
		     from the master.  For this reason the slave must
		     wait RouterDeadInterval seconds before freeing the
		     last Database Description packet.  Reception of a
		     Database Description packet from the master after
		     this interval will generate a SeqNumberMismatch
		     neighbor event. RFC2328 Section 10.8 */
		  ospf_db_desc_resend (nbr);
		  break;
		}
	    }
	}

      OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_SeqNumberMismatch);
      break;
    default:
      zlog_warn ("Packet[DD]: NSM illegal status.");
      break;
    }
}

#define OSPF_LSA_KEY_SIZE       12 /* type(4) + id(4) + ar(4) */

/* OSPF Link State Request Read -- RFC2328 Section 10.7. */
void
ospf_ls_req (struct ip *iph, struct ospf_header *ospfh,
	     struct stream *s, struct ospf_interface *oi, u_int16_t size)
{
  struct ospf_neighbor *nbr;
  u_int32_t ls_type;
  struct in_addr ls_id;
  struct in_addr adv_router;
  struct ospf_lsa *find;
  list ls_upd;
  int length;

  /* Increment statistics. */
  oi->ls_req_in++;

  nbr = ospf_nbr_lookup_by_addr (oi->nbrs, &iph->ip_src);
  if (nbr == NULL)
    {
      zlog_warn ("Link State Request: Unknown Neighbor %s.",
		 inet_ntoa (ospfh->router_id));
      return;
    }

  /* Neighbor State should be Exchange or later. */
  if (nbr->state != NSM_Exchange &&
      nbr->state != NSM_Loading &&
      nbr->state != NSM_Full)
    {
      zlog_warn ("Link State Request: Neighbor state is %s, packet discarded.",
		 LOOKUP (ospf_nsm_state_msg, nbr->state));
      return;
    }

  /* Send Link State Update for ALL requested LSAs. */
  ls_upd = list_new ();
  length = OSPF_HEADER_SIZE + OSPF_LS_UPD_MIN_SIZE;

  while (size >= OSPF_LSA_KEY_SIZE)
    {
      /* Get one slice of Link State Request. */
      ls_type = stream_getl (s);
      ls_id.s_addr = stream_get_ipv4 (s);
      adv_router.s_addr = stream_get_ipv4 (s);

      /* Verify LSA type. */
      if (ls_type < OSPF_MIN_LSA || ls_type >= OSPF_MAX_LSA)
	{
	  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_BadLSReq);
	  list_delete (ls_upd);
	  return;
	}

      /* Search proper LSA in LSDB. */
      find = ospf_lsa_lookup (oi->area, ls_type, ls_id, adv_router);
      if (find == NULL)
	{
	  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_BadLSReq);
	  list_delete (ls_upd);
	  return;
	}

      /* Packet overflows MTU size, send immediatly. */
      if (length + ntohs (find->data->length) > OSPF_PACKET_MAX (oi))
	{
	  if (oi->type == OSPF_IFTYPE_NBMA)
	    ospf_ls_upd_send (nbr, ls_upd, OSPF_SEND_PACKET_DIRECT);
	  else
	    ospf_ls_upd_send (nbr, ls_upd, OSPF_SEND_PACKET_INDIRECT);

	  /* Only remove list contents.  Keep ls_upd. */
	  list_delete_all_node (ls_upd);

	  length = OSPF_HEADER_SIZE + OSPF_LS_UPD_MIN_SIZE;
	}

      /* Append LSA to update list. */
      listnode_add (ls_upd, find);
      length += ntohs (find->data->length);

      size -= OSPF_LSA_KEY_SIZE;
    }

  /* Send rest of Link State Update. */
  if (listcount (ls_upd) > 0)
    {
      if (oi->type == OSPF_IFTYPE_NBMA)
	ospf_ls_upd_send (nbr, ls_upd, OSPF_SEND_PACKET_DIRECT);
      else
	ospf_ls_upd_send (nbr, ls_upd, OSPF_SEND_PACKET_INDIRECT);

      list_delete (ls_upd);
    }
  else
    list_free (ls_upd);
}

/* Get the list of LSAs from Link State Update packet.
   And process some validation -- RFC2328 Section 13. (1)-(2). */
static list
ospf_ls_upd_list_lsa (struct ospf_neighbor *nbr, struct stream *s,
                      struct ospf_interface *oi, size_t size)
{
  u_int16_t count, sum;
  u_int32_t length;
  struct lsa_header *lsah;
  struct ospf_lsa *lsa;
  list lsas;

  lsas = list_new ();

  count = stream_getl (s);
  size -= OSPF_LS_UPD_MIN_SIZE; /* # LSAs */

  for (; size >= OSPF_LSA_HEADER_SIZE && count > 0;
       size -= length, stream_forward (s, length), count--)
    {
      lsah = (struct lsa_header *) STREAM_PNT (s);
      length = ntohs (lsah->length);

      if (length > size)
	{
	  zlog_warn ("Link State Update: LSA length exceeds packet size.");
	  break;
	}

      /* Validate the LSA's LS checksum. */
      sum = lsah->checksum;
      if (sum != ospf_lsa_checksum (lsah))
	{
	  zlog_warn ("Link State Update: LSA checksum error %x, %x.",
		     sum, lsah->checksum);
	  continue;
	}

      /* Examine the LSA's LS type. */
      if (lsah->type < OSPF_MIN_LSA || lsah->type >= OSPF_MAX_LSA)
	{
	  zlog_warn ("Link State Update: Unknown LS type %d", lsah->type);
	  continue;
	}

      /*
       * What if the received LSA's age is greater than MaxAge?
       * Treat it as a MaxAge case -- endo.
       */
      if (ntohs (lsah->ls_age) > OSPF_LSA_MAXAGE)
        lsah->ls_age = htons (OSPF_LSA_MAXAGE);

#ifdef HAVE_OPAQUE_LSA
      if (CHECK_FLAG (nbr->options, OSPF_OPTION_O))
        {
#ifdef STRICT_OBIT_USAGE_CHECK
	  if ((IS_OPAQUE_LSA(lsah->type) &&
               ! CHECK_FLAG (lsah->options, OSPF_OPTION_O))
	      ||  (! IS_OPAQUE_LSA(lsah->type) &&
		   CHECK_FLAG (lsah->options, OSPF_OPTION_O)))
            {
              /*
               * This neighbor must know the exact usage of O-bit;
               * the bit will be set in Type-9,10,11 LSAs only.
               */
              zlog_warn ("LSA[Type%d:%s]: O-bit abuse?", lsah->type, inet_ntoa (lsah->id));
              continue;
            }
#endif /* STRICT_OBIT_USAGE_CHECK */

          /* Do not take in AS External Opaque-LSAs if we are a stub. */
          if (lsah->type == OSPF_OPAQUE_AS_LSA
	      && nbr->oi->area->external_routing != OSPF_AREA_DEFAULT) 
            {
              if (IS_DEBUG_OSPF_EVENT)
                zlog_info ("LSA[Type%d:%s]: We are a stub, don't take this LSA.", lsah->type, inet_ntoa (lsah->id));
              continue;
            }
        }
      else if (IS_OPAQUE_LSA(lsah->type))
        {
          zlog_warn ("LSA[Type%d:%s]: Opaque capability mismatch?", lsah->type, inet_ntoa (lsah->id));
          continue;
        }
#endif /* HAVE_OPAQUE_LSA */

      /* Create OSPF LSA instance. */
      lsa = ospf_lsa_new ();

      /* We may wish to put some error checking if type NSSA comes in
         and area not in NSSA mode */
      switch (lsah->type)
        {
        case OSPF_AS_EXTERNAL_LSA:
#ifdef HAVE_OPAQUE_LSA
        case OSPF_OPAQUE_AS_LSA:
          lsa->area = NULL;
          break;
        case OSPF_OPAQUE_LINK_LSA:
          lsa->oi = oi; /* Remember incoming interface for flooding control. */
          /* Fallthrough */
#endif /* HAVE_OPAQUE_LSA */
        default:
          lsa->area = oi->area;
          break;
        }

      lsa->data = ospf_lsa_data_new (length);
      memcpy (lsa->data, lsah, length);

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info("LSA[Type%d:%s]: %p new LSA created with Link State Update",
		  lsa->data->type, inet_ntoa (lsa->data->id), lsa);
      listnode_add (lsas, lsa);
    }

  return lsas;
}

/* Cleanup Update list. */
void
ospf_upd_list_clean (list lsas)
{
  listnode node;
  struct ospf_lsa *lsa;

  for (node = listhead (lsas); node; nextnode (node))
    if ((lsa = getdata (node)) != NULL)
      ospf_lsa_discard (lsa);

  list_delete (lsas);
}

/* OSPF Link State Update message read -- RFC2328 Section 13. */
void
ospf_ls_upd (struct ip *iph, struct ospf_header *ospfh,
	     struct stream *s, struct ospf_interface *oi, u_int16_t size)
{
  struct ospf_neighbor *nbr;
  list lsas;
#ifdef HAVE_OPAQUE_LSA
  list mylsa_acks, mylsa_upds;
#endif /* HAVE_OPAQUE_LSA */
  listnode node, next;
  struct ospf_lsa *lsa = NULL;
  /* unsigned long ls_req_found = 0; */

  /* Dis-assemble the stream, update each entry, re-encapsulate for flooding */

  /* Increment statistics. */
  oi->ls_upd_in++;

  /* Check neighbor. */
  nbr = ospf_nbr_lookup_by_addr (oi->nbrs, &iph->ip_src);
  if (nbr == NULL)
    {
      zlog_warn ("Link State Update: Unknown Neighbor %s on int: %s",
		 inet_ntoa (ospfh->router_id), IF_NAME (oi));
      return;
    }

  /* Check neighbor state. */
  if (nbr->state < NSM_Exchange)
    {
      zlog_warn ("Link State Update: Neighbor[%s] state is less than Exchange",
		 inet_ntoa (ospfh->router_id));
      return;
    }

  /* Get list of LSAs from Link State Update packet. - Also perorms Stages 
   * 1 (validate LSA checksum) and 2 (check for LSA consistent type) 
   * of section 13. 
   */
  lsas = ospf_ls_upd_list_lsa (nbr, s, oi, size);

#ifdef HAVE_OPAQUE_LSA
  /*
   * Prepare two kinds of lists to clean up unwanted self-originated
   * Opaque-LSAs from the routing domain as soon as possible.
   */
  mylsa_acks = list_new (); /* Let the sender cease retransmission. */
  mylsa_upds = list_new (); /* Flush target LSAs if necessary. */

  /*
   * If self-originated Opaque-LSAs that have flooded before restart
   * are contained in the received LSUpd message, corresponding LSReq
   * messages to be sent may have to be modified.
   * To eliminate possible race conditions such that flushing and normal
   * updating for the same LSA would take place alternately, this trick
   * must be done before entering to the loop below.
   */
  ospf_opaque_adjust_lsreq (nbr, lsas);
#endif /* HAVE_OPAQUE_LSA */

#define DISCARD_LSA(L,N) {\
        if (IS_DEBUG_OSPF_EVENT) \
          zlog_info ("ospf_lsa_discard() in ospf_ls_upd() point %d: lsa %p Type-%d", N, lsa, (int) lsa->data->type); \
        ospf_lsa_discard (L); \
	continue; }

  /* Process each LSA received in the one packet. */
  for (node = listhead (lsas); node; node = next)
    {
      struct ospf_lsa *ls_ret, *current;
      int ret = 1;

      next = node->next;

      lsa = getdata (node);

#ifdef HAVE_NSSA
      if (IS_DEBUG_OSPF_NSSA)
	{
	  char buf1[INET_ADDRSTRLEN];
	  char buf2[INET_ADDRSTRLEN];
	  char buf3[INET_ADDRSTRLEN];

	  zlog_info("LSA Type-%d from %s, ID: %s, ADV: %s",
		    lsa->data->type,
		    inet_ntop (AF_INET, &ospfh->router_id,
			       buf1, INET_ADDRSTRLEN),
		    inet_ntop (AF_INET, &lsa->data->id,
			       buf2, INET_ADDRSTRLEN),
		    inet_ntop (AF_INET, &lsa->data->adv_router,
			       buf3, INET_ADDRSTRLEN));
	}
#endif /* HAVE_NSSA */

      listnode_delete (lsas, lsa); /* We don't need it in list anymore */

      /* Validate Checksum - Done above by ospf_ls_upd_list_lsa() */

      /* LSA Type  - Done above by ospf_ls_upd_list_lsa() */
   
      /* Do not take in AS External LSAs if we are a stub or NSSA. */

      /* Do not take in AS NSSA if this neighbor and we are not NSSA */

      /* Do take in Type-7's if we are an NSSA  */ 
 
      /* If we are also an ABR, later translate them to a Type-5 packet */
 
      /* Later, an NSSA Re-fresh can Re-fresh Type-7's and an ABR will
	 translate them to a separate Type-5 packet.  */

      if (lsa->data->type == OSPF_AS_EXTERNAL_LSA)
        /* Reject from STUB or NSSA */
        if (nbr->oi->area->external_routing != OSPF_AREA_DEFAULT) 
	  {
	    DISCARD_LSA (lsa, 1);
#ifdef HAVE_NSSA
	    if (IS_DEBUG_OSPF_NSSA)
	      zlog_info("Incoming External LSA Discarded: We are NSSA/STUB Area");
#endif /* HAVE_NSSA */
	  }

#ifdef  HAVE_NSSA 
      if (lsa->data->type == OSPF_AS_NSSA_LSA)
	if (nbr->oi->area->external_routing != OSPF_AREA_NSSA)
	  {
	    DISCARD_LSA (lsa,2);
	    if (IS_DEBUG_OSPF_NSSA)
	      zlog_info("Incoming NSSA LSA Discarded:  Not NSSA Area");
	  }
#endif /* HAVE_NSSA */

      /* Find the LSA in the current database. */

      current = ospf_lsa_lookup_by_header (oi->area, lsa->data);

      /* If the LSA's LS age is equal to MaxAge, and there is currently
	 no instance of the LSA in the router's link state database,
	 and none of router's neighbors are in states Exchange or Loading,
	 then take the following actions. */

      if (IS_LSA_MAXAGE (lsa) && !current &&
	  (ospf_nbr_count (oi, NSM_Exchange) +
	   ospf_nbr_count (oi, NSM_Loading)) == 0)
	{
	  /* Response Link State Acknowledgment. */
	  ospf_ls_ack_send (nbr, lsa);

	  /* Discard LSA. */	  
	  zlog_warn ("Link State Update: LS age is equal to MaxAge.");
          DISCARD_LSA (lsa, 3);
	}

#ifdef HAVE_OPAQUE_LSA
      if (IS_OPAQUE_LSA (lsa->data->type)
	  &&  IPV4_ADDR_SAME (&lsa->data->adv_router, &oi->ospf->router_id))
        {
          /*
           * Even if initial flushing seems to be completed, there might
           * be a case that self-originated LSA with MaxAge still remain
           * in the routing domain.
           * Just send an LSAck message to cease retransmission.
           */
          if (IS_LSA_MAXAGE (lsa))
            {
              zlog_warn ("LSA[%s]: Boomerang effect?", dump_lsa_key (lsa));
              ospf_ls_ack_send (nbr, lsa);
              ospf_lsa_discard (lsa);

              if (current != NULL && ! IS_LSA_MAXAGE (current))
                ospf_opaque_lsa_refresh_schedule (current);
              continue;
            }

          /*
           * If an instance of self-originated Opaque-LSA is not found
           * in the LSDB, there are some possible cases here.
           *
           * 1) This node lost opaque-capability after restart.
           * 2) Else, a part of opaque-type is no more supported.
           * 3) Else, a part of opaque-id is no more supported.
           *
           * Anyway, it is still this node's responsibility to flush it.
           * Otherwise, the LSA instance remains in the routing domain
           * until its age reaches to MaxAge.
           */
          if (current == NULL)
            {
              if (IS_DEBUG_OSPF_EVENT)
                zlog_info ("LSA[%s]: Previously originated Opaque-LSA, not found in the LSDB.", dump_lsa_key (lsa));

              SET_FLAG (lsa->flags, OSPF_LSA_SELF);
              listnode_add (mylsa_upds, ospf_lsa_dup  (lsa));
              listnode_add (mylsa_acks, ospf_lsa_lock (lsa));
              continue;
            }
        }
#endif /* HAVE_OPAQUE_LSA */

      /* (5) Find the instance of this LSA that is currently contained
	 in the router's link state database.  If there is no
	 database copy, or the received LSA is more recent than
	 the database copy the following steps must be performed. */

      if (current == NULL ||
	  (ret = ospf_lsa_more_recent (current, lsa)) < 0)
	{
	  /* Actual flooding procedure. */
	  if (ospf_flood (oi->ospf, nbr, current, lsa) < 0)  /* Trap NSSA later. */
	    DISCARD_LSA (lsa, 4);
	  continue;
	}

      /* (6) Else, If there is an instance of the LSA on the sending
	 neighbor's Link state request list, an error has occurred in
	 the Database Exchange process.  In this case, restart the
	 Database Exchange process by generating the neighbor event
	 BadLSReq for the sending neighbor and stop processing the
	 Link State Update packet. */

      if (ospf_ls_request_lookup (nbr, lsa))
	{
	  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_BadLSReq);
	  zlog_warn ("LSA instance exists on Link state request list");

	  /* Clean list of LSAs. */
          ospf_upd_list_clean (lsas);
	  /* this lsa is not on lsas list already. */
	  ospf_lsa_discard (lsa);
#ifdef HAVE_OPAQUE_LSA
          list_delete (mylsa_acks);
          list_delete (mylsa_upds);
#endif /* HAVE_OPAQUE_LSA */
	  return;
	}

      /* If the received LSA is the same instance as the database copy
	 (i.e., neither one is more recent) the following two steps
	 should be performed: */

      if (ret == 0)
	{
	  /* If the LSA is listed in the Link state retransmission list
	     for the receiving adjacency, the router itself is expecting
	     an acknowledgment for this LSA.  The router should treat the
	     received LSA as an acknowledgment by removing the LSA from
	     the Link state retransmission list.  This is termed an
	     "implied acknowledgment". */

	  ls_ret = ospf_ls_retransmit_lookup (nbr, lsa);

	  if (ls_ret != NULL)
	    {
	      ospf_ls_retransmit_delete (nbr, ls_ret);

	      /* Delayed acknowledgment sent if advertisement received
		 from Designated Router, otherwise do nothing. */
	      if (oi->state == ISM_Backup)
		if (NBR_IS_DR (nbr))
		  listnode_add (oi->ls_ack, ospf_lsa_lock (lsa));

              DISCARD_LSA (lsa, 5);
	    }
	  else
	    /* Acknowledge the receipt of the LSA by sending a
	       Link State Acknowledgment packet back out the receiving
	       interface. */
	    {
	      ospf_ls_ack_send (nbr, lsa);
	      DISCARD_LSA (lsa, 6);
	    }
	}

      /* The database copy is more recent.  If the database copy
	 has LS age equal to MaxAge and LS sequence number equal to
	 MaxSequenceNumber, simply discard the received LSA without
	 acknowledging it. (In this case, the LSA's LS sequence number is
	 wrapping, and the MaxSequenceNumber LSA must be completely
	 flushed before any new LSA instance can be introduced). */

      else if (ret > 0)  /* Database copy is more recent */
	{ 
	  if (IS_LSA_MAXAGE (current) &&
	      current->data->ls_seqnum == htonl (OSPF_MAX_SEQUENCE_NUMBER))
	    {
	      DISCARD_LSA (lsa, 7);
	    }
	  /* Otherwise, as long as the database copy has not been sent in a
	     Link State Update within the last MinLSArrival seconds, send the
	     database copy back to the sending neighbor, encapsulated within
	     a Link State Update Packet. The Link State Update Packet should
	     be sent directly to the neighbor. In so doing, do not put the
	     database copy of the LSA on the neighbor's link state
	     retransmission list, and do not acknowledge the received (less
	     recent) LSA instance. */
	  else
	    {
	      struct timeval now;
	      
	      gettimeofday (&now, NULL);
	      
	      if (tv_cmp (tv_sub (now, current->tv_orig), 
			  int2tv (OSPF_MIN_LS_ARRIVAL)) > 0)
		/* Trap NSSA type later.*/
		ospf_ls_upd_send_lsa (nbr, current, OSPF_SEND_PACKET_DIRECT);
	      DISCARD_LSA (lsa, 8);
	    }
	}
    }
  
#ifdef HAVE_OPAQUE_LSA
  /*
   * Now that previously originated Opaque-LSAs those which not yet
   * installed into LSDB are captured, take several steps to clear
   * them completely from the routing domain, before proceeding to
   * origination for the current target Opaque-LSAs.
   */
  while (listcount (mylsa_acks) > 0)
    ospf_ls_ack_send_list (oi, mylsa_acks, nbr->address.u.prefix4);

  if (listcount (mylsa_upds) > 0)
    ospf_opaque_self_originated_lsa_received (nbr, mylsa_upds);

  list_delete (mylsa_acks);
  list_delete (mylsa_upds);
#endif /* HAVE_OPAQUE_LSA */

  assert (listcount (lsas) == 0);
  list_delete (lsas);
}

/* OSPF Link State Acknowledgment message read -- RFC2328 Section 13.7. */
void
ospf_ls_ack (struct ip *iph, struct ospf_header *ospfh,
	     struct stream *s, struct ospf_interface *oi, u_int16_t size)
{
  struct ospf_neighbor *nbr;
#ifdef HAVE_OPAQUE_LSA
  list opaque_acks;
#endif /* HAVE_OPAQUE_LSA */

  /* increment statistics. */
  oi->ls_ack_in++;

  nbr = ospf_nbr_lookup_by_addr (oi->nbrs, &iph->ip_src);
  if (nbr == NULL)
    {
      zlog_warn ("Link State Acknowledgment: Unknown Neighbor %s.",
		 inet_ntoa (ospfh->router_id));
      return;
    }

  if (nbr->state < NSM_Exchange)
    {
      zlog_warn ("Link State Acknowledgment: State is less than Exchange.");
      return;
    }

#ifdef HAVE_OPAQUE_LSA
  opaque_acks = list_new ();
#endif /* HAVE_OPAQUE_LSA */

  while (size >= OSPF_LSA_HEADER_SIZE)
    {
      struct ospf_lsa *lsa, *lsr;

      lsa = ospf_lsa_new ();
      lsa->data = (struct lsa_header *) STREAM_PNT (s);

      /* lsah = (struct lsa_header *) STREAM_PNT (s); */
      size -= OSPF_LSA_HEADER_SIZE;
      stream_forward (s, OSPF_LSA_HEADER_SIZE);

      if (lsa->data->type < OSPF_MIN_LSA || lsa->data->type >= OSPF_MAX_LSA)
	{
	  lsa->data = NULL;
	  ospf_lsa_discard (lsa);
	  continue;
	}

      lsr = ospf_ls_retransmit_lookup (nbr, lsa);

      if (lsr != NULL && lsr->data->ls_seqnum == lsa->data->ls_seqnum)
        {
#ifdef HAVE_OPAQUE_LSA
          /* Keep this LSA entry for later reference. */
          if (IS_OPAQUE_LSA (lsr->data->type))
            listnode_add (opaque_acks, ospf_lsa_dup (lsr));
#endif /* HAVE_OPAQUE_LSA */

          ospf_ls_retransmit_delete (nbr, lsr);
        }

      lsa->data = NULL;
      ospf_lsa_discard (lsa);
    }

#ifdef HAVE_OPAQUE_LSA
  if (listcount (opaque_acks) > 0)
    ospf_opaque_ls_ack_received (nbr, opaque_acks);

  list_delete (opaque_acks);
  return;
#endif /* HAVE_OPAQUE_LSA */
}

struct stream *
ospf_recv_packet (int fd, struct interface **ifp)
{
  int ret;
  struct ip iph;
  u_int16_t ip_len;
  struct stream *ibuf;
  unsigned int ifindex = 0;
  struct iovec iov;
  struct cmsghdr *cmsg;
#if defined (IP_PKTINFO)
  struct in_pktinfo *pktinfo;
#elif defined (IP_RECVIF)
  struct sockaddr_dl *pktinfo;
#else
  char *pktinfo; /* dummy */
#endif
  char buff [sizeof (*cmsg) + sizeof (*pktinfo)];
  struct msghdr msgh = {NULL, 0, &iov, 1, buff,
			sizeof (*cmsg) + sizeof (*pktinfo), 0};
    
  ret = recvfrom (fd, (void *)&iph, sizeof (iph), MSG_PEEK, NULL, 0);
  
  if (ret != sizeof (iph))
    {
      zlog_warn ("ospf_recv_packet packet smaller than ip header");
      return NULL;
    }

#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(OpenBSD_IP_LEN)
  ip_len = iph.ip_len;
#else
  ip_len = ntohs (iph.ip_len);
#endif

#if !defined(GNU_LINUX) && !defined(OpenBSD_IP_LEN)
  /*
   * Kernel network code touches incoming IP header parameters,
   * before protocol specific processing.
   *
   *   1) Convert byteorder to host representation.
   *      --> ip_len, ip_id, ip_off
   *
   *   2) Adjust ip_len to strip IP header size!
   *      --> If user process receives entire IP packet via RAW
   *          socket, it must consider adding IP header size to
   *          the "ip_len" field of "ip" structure.
   *
   * For more details, see <netinet/ip_input.c>.
   */
  ip_len = ip_len + (iph.ip_hl << 2);
#endif
  
  ibuf = stream_new (ip_len);
  iov.iov_base = STREAM_DATA (ibuf);
  iov.iov_len = ip_len;
  ret = recvmsg (fd, &msgh, 0);
  
  cmsg = CMSG_FIRSTHDR (&msgh);
  
  if (cmsg != NULL && //cmsg->cmsg_len == sizeof (*pktinfo) &&
      cmsg->cmsg_level == IPPROTO_IP &&
#if defined (IP_PKTINFO)
      cmsg->cmsg_type == IP_PKTINFO
#elif defined (IP_RECVIF)
      cmsg->cmsg_type == IP_RECVIF
#else
      0
#endif
      )
    {
#if defined (IP_PKTINFO)
      pktinfo = (struct in_pktinfo *)CMSG_DATA(cmsg);
      ifindex = pktinfo->ipi_ifindex;
#elif defined (IP_RECVIF)
      pktinfo = (struct sockaddr_dl *)CMSG_DATA(cmsg);
      ifindex = pktinfo->sdl_index;
#else
      ifindex = 0;
#endif
    }
  
  *ifp = if_lookup_by_index (ifindex);

  if (ret != ip_len)
    {
      zlog_warn ("ospf_recv_packet short read. "
		 "ip_len %d bytes read %d", ip_len, ret);
      stream_free (ibuf);
      return NULL;
    }
  
  return ibuf;
}

struct ospf_interface *
ospf_associate_packet_vl (struct ospf *ospf,
			  struct interface *ifp, struct ospf_interface *oi,
			  struct ip *iph, struct ospf_header *ospfh)
{
  struct ospf_interface *rcv_oi;
  struct ospf_vl_data *vl_data;
  struct ospf_area *vl_area;
  listnode node;

  if (IN_MULTICAST (ntohl (iph->ip_dst.s_addr)) ||
      !OSPF_IS_AREA_BACKBONE (ospfh))
    return oi;

  if ((rcv_oi = oi) == NULL)
    {
      if ((rcv_oi = ospf_if_lookup_by_local_addr (ospf, ifp,
						  iph->ip_dst)) == NULL)
	return NULL;
    }

  for (node = listhead (ospf->vlinks); node; nextnode (node))
    {
      if ((vl_data = getdata (node)) == NULL)
	continue;
      
      vl_area = ospf_area_lookup_by_area_id (ospf, vl_data->vl_area_id);
      if (!vl_area)
	continue;
      
      if (OSPF_AREA_SAME (&vl_area, &rcv_oi->area) &&
	  IPV4_ADDR_SAME (&vl_data->vl_peer, &ospfh->router_id))
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("associating packet with %s",
		       IF_NAME (vl_data->vl_oi));
	  if (! CHECK_FLAG (vl_data->vl_oi->ifp->flags, IFF_UP))
	    {
	      if (IS_DEBUG_OSPF_EVENT)
		zlog_info ("This VL is not up yet, sorry");
	      return NULL;
	    }
	  
	  return vl_data->vl_oi;
	}
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("couldn't find any VL to associate the packet with");
  
  return oi;
}

int
ospf_check_area_id (struct ospf_interface *oi, struct ospf_header *ospfh)
{
  /* Check match the Area ID of the receiving interface. */
  if (OSPF_AREA_SAME (&oi->area, &ospfh))
    return 1;

  return 0;
}

/* Unbound socket will accept any Raw IP packets if proto is matched.
   To prevent it, compare src IP address and i/f address with masking
   i/f network mask. */
int
ospf_check_network_mask (struct ospf_interface *oi, struct in_addr ip_src)
{
  struct in_addr mask, me, him;

  if (oi->type == OSPF_IFTYPE_POINTOPOINT ||
      oi->type == OSPF_IFTYPE_VIRTUALLINK)
    return 1;

  masklen2ip (oi->address->prefixlen, &mask);

  me.s_addr = oi->address->u.prefix4.s_addr & mask.s_addr;
  him.s_addr = ip_src.s_addr & mask.s_addr;

  if (IPV4_ADDR_SAME (&me, &him))
    return 1;

  return 0;
}

int
ospf_check_auth (struct ospf_interface *oi, struct stream *ibuf,
		 struct ospf_header *ospfh)
{
  int ret = 0;
  struct crypt_key *ck;
  list list;
  listnode node;

  switch (ntohs (ospfh->auth_type))
    {
    case OSPF_AUTH_NULL:
      ret = 1;
      break;
    case OSPF_AUTH_SIMPLE:
      if (!memcmp (OSPF_IF_PARAM (oi, auth_simple), ospfh->u.auth_data, OSPF_AUTH_SIMPLE_SIZE))
	ret = 1;
      else
	ret = 0;
      break;
    case OSPF_AUTH_CRYPTOGRAPHIC:
      ret = 0;
      list = OSPF_IF_PARAM (oi, auth_crypt);
      for (node = listhead (list); node; nextnode (node))
        {
          ck = getdata (node);
          if (ck == NULL)
            continue;

          /* This is very basic, the digest processing is elsewhere */
          if (ospfh->u.crypt.auth_data_len == OSPF_AUTH_MD5_SIZE && 
              ospfh->u.crypt.key_id == ck->key_id &&
              ntohs (ospfh->length) + OSPF_AUTH_SIMPLE_SIZE <=
                stream_get_size (ibuf))
            {
              ret = 1;
              break;
            }
        }
      break;
    default:
      ret = 0;
      break;
    }

  return ret;
}

int
ospf_check_sum (struct ospf_header *ospfh)
{
  u_int32_t ret;
  u_int16_t sum;

  /* clear auth_data for checksum. */
  memset (ospfh->u.auth_data, 0, OSPF_AUTH_SIMPLE_SIZE);

  /* keep checksum and clear. */
  sum = ospfh->checksum;
  memset (&ospfh->checksum, 0, sizeof (u_int16_t));

  /* calculate checksum. */
  ret = in_cksum (ospfh, ntohs (ospfh->length));

  if (ret != sum)
    {
      zlog_info ("ospf_check_sum(): checksum mismatch, my %X, his %X",
		 ret, sum);
      return 0;
    }

  return 1;
}

/* OSPF Header verification. */
int
ospf_verify_header (struct stream *ibuf, struct ospf_interface *oi,
		    struct ip *iph, struct ospf_header *ospfh)
{
  /* check version. */
  if (ospfh->version != OSPF_VERSION)
    {
      zlog_warn ("interface %s: ospf_read version number mismatch.",
		 IF_NAME (oi));
      return -1;
    }

  /* Check Area ID. */
  if (!ospf_check_area_id (oi, ospfh))
    {
      zlog_warn ("interface %s: ospf_read invalid Area ID %s.",
		 IF_NAME (oi), inet_ntoa (ospfh->area_id));
      return -1;
    }

  /* Check network mask, Silently discarded. */
  if (! ospf_check_network_mask (oi, iph->ip_src))
    {
      zlog_warn ("interface %s: ospf_read network address is not same [%s]",
		 IF_NAME (oi), inet_ntoa (iph->ip_src));
      return -1;
    }

  /* Check authentication. */
  if (ospf_auth_type (oi) != ntohs (ospfh->auth_type))
    {
      zlog_warn ("interface %s: ospf_read authentication type mismatch.",
		 IF_NAME (oi));
      return -1;
    }

  if (! ospf_check_auth (oi, ibuf, ospfh))
    {
      zlog_warn ("interface %s: ospf_read authentication failed.",
		 IF_NAME (oi));
      return -1;
    }

  /* if check sum is invalid, packet is discarded. */
  if (ntohs (ospfh->auth_type) != OSPF_AUTH_CRYPTOGRAPHIC)
    {
      if (! ospf_check_sum (ospfh))
	{
	  zlog_warn ("interface %s: ospf_read packet checksum error %s",
		     IF_NAME (oi), inet_ntoa (ospfh->router_id));
	  return -1;
	}
    }
  else
    {
      if (ospfh->checksum != 0)
	return -1;
      if (ospf_check_md5_digest (oi, ibuf, ntohs (ospfh->length)) == 0)
	{
	  zlog_warn ("interface %s: ospf_read md5 authentication failed.",
		     IF_NAME (oi));
	  return -1;
	}
    }

  return 0;
}

/* Starting point of packet process function. */
int
ospf_read (struct thread *thread)
{
  int ret;
  struct stream *ibuf;
  struct ospf *ospf;
  struct ospf_interface *oi;
  struct ip *iph;
  struct ospf_header *ospfh;
  u_int16_t length;
  struct interface *ifp;

  /* first of all get interface pointer. */
  ospf = THREAD_ARG (thread);
  ospf->t_read = NULL;

  /* prepare for next packet. */
  ospf->t_read = thread_add_read (master, ospf_read, ospf, ospf->fd);

  /* read OSPF packet. */
  ibuf = ospf_recv_packet (ospf->fd, &ifp);
  if (ibuf == NULL)
    return -1;
  
  iph = (struct ip *) STREAM_DATA (ibuf);

  /* Self-originated packet should be discarded silently. */
  if (ospf_if_lookup_by_local_addr (ospf, NULL, iph->ip_src))
    {
      stream_free (ibuf);
      return 0;
    }

  /* Adjust size to message length. */
  stream_forward (ibuf, iph->ip_hl * 4);
  
  /* Get ospf packet header. */
  ospfh = (struct ospf_header *) STREAM_PNT (ibuf);

  /* associate packet with ospf interface */
  oi = ospf_if_lookup_recv_if (ospf, iph->ip_src);
  if (ifp && oi && oi->ifp != ifp)
    {
      zlog_warn ("Packet from [%s] received on wrong link %s",
		 inet_ntoa (iph->ip_src), ifp->name); 
      stream_free (ibuf);
      return 0;
    }
  
  if ((oi = ospf_associate_packet_vl (ospf, ifp, oi, iph, ospfh)) == NULL)
    {
      stream_free (ibuf);
      return 0;
    }

  /*
   * If the received packet is destined for AllDRouters, the packet
   * should be accepted only if the received ospf interface state is
   * either DR or Backup -- endo.
   */
  if (iph->ip_dst.s_addr == htonl (OSPF_ALLDROUTERS)
      && (oi->state != ISM_DR && oi->state != ISM_Backup))
    {
      zlog_info ("Packet for AllDRouters from [%s] via [%s] (ISM: %s)",
                 inet_ntoa (iph->ip_src), IF_NAME (oi),
                 LOOKUP (ospf_ism_state_msg, oi->state));
      stream_free (ibuf);
      return 0;
    }

  /* Show debug receiving packet. */
  if (IS_DEBUG_OSPF_PACKET (ospfh->type - 1, RECV))
    {
      if (IS_DEBUG_OSPF_PACKET (ospfh->type - 1, DETAIL))
	{
	  zlog_info ("-----------------------------------------------------");
	  ospf_packet_dump (ibuf);
	}

      zlog_info ("%s received from [%s] via [%s]",
		 ospf_packet_type_str[ospfh->type],
		 inet_ntoa (ospfh->router_id), IF_NAME (oi));
      zlog_info (" src [%s],", inet_ntoa (iph->ip_src));
      zlog_info (" dst [%s]", inet_ntoa (iph->ip_dst));

      if (IS_DEBUG_OSPF_PACKET (ospfh->type - 1, DETAIL))
	zlog_info ("-----------------------------------------------------");
    }

  /* Some header verification. */
  ret = ospf_verify_header (ibuf, oi, iph, ospfh);
  if (ret < 0)
    {
      stream_free (ibuf);
      return ret;
    }

  stream_forward (ibuf, OSPF_HEADER_SIZE);

  /* Adjust size to message length. */
  length = ntohs (ospfh->length) - OSPF_HEADER_SIZE;

  /* Read rest of the packet and call each sort of packet routine. */
  switch (ospfh->type)
    {
    case OSPF_MSG_HELLO:
      ospf_hello (iph, ospfh, ibuf, oi, length);
      break;
    case OSPF_MSG_DB_DESC:
      ospf_db_desc (iph, ospfh, ibuf, oi, length);
      break;
    case OSPF_MSG_LS_REQ:
      ospf_ls_req (iph, ospfh, ibuf, oi, length);
      break;
    case OSPF_MSG_LS_UPD:
      ospf_ls_upd (iph, ospfh, ibuf, oi, length);
      break;
    case OSPF_MSG_LS_ACK:
      ospf_ls_ack (iph, ospfh, ibuf, oi, length);
      break;
    default:
      zlog (NULL, LOG_WARNING,
	    "interface %s: OSPF packet header type %d is illegal",
	    IF_NAME (oi), ospfh->type);
      break;
    }

  stream_free (ibuf);
  return 0;
}

/* Make OSPF header. */
void
ospf_make_header (int type, struct ospf_interface *oi, struct stream *s)
{
  struct ospf_header *ospfh;

  ospfh = (struct ospf_header *) STREAM_DATA (s);

  ospfh->version = (u_char) OSPF_VERSION;
  ospfh->type = (u_char) type;

  ospfh->router_id = oi->ospf->router_id;

  ospfh->checksum = 0;
  ospfh->area_id = oi->area->area_id;
  ospfh->auth_type = htons (ospf_auth_type (oi));

  memset (ospfh->u.auth_data, 0, OSPF_AUTH_SIMPLE_SIZE);

  ospf_output_forward (s, OSPF_HEADER_SIZE);
}

/* Make Authentication Data. */
int
ospf_make_auth (struct ospf_interface *oi, struct ospf_header *ospfh)
{
  struct crypt_key *ck;

  switch (ospf_auth_type (oi))
    {
    case OSPF_AUTH_NULL:
      break;
    case OSPF_AUTH_SIMPLE:
      memcpy (ospfh->u.auth_data, OSPF_IF_PARAM (oi, auth_simple),
	      OSPF_AUTH_SIMPLE_SIZE);
      break;
    case OSPF_AUTH_CRYPTOGRAPHIC:
      /* If key is not set, then set 0. */
      if (list_isempty (OSPF_IF_PARAM (oi, auth_crypt)))
	{
	  ospfh->u.crypt.zero = 0;
	  ospfh->u.crypt.key_id = 0;
	  ospfh->u.crypt.auth_data_len = OSPF_AUTH_MD5_SIZE;
	}
      else
	{
	  ck = getdata (OSPF_IF_PARAM (oi, auth_crypt)->tail);
	  ospfh->u.crypt.zero = 0;
	  ospfh->u.crypt.key_id = ck->key_id;
	  ospfh->u.crypt.auth_data_len = OSPF_AUTH_MD5_SIZE;
	}
      /* note: the seq is done in ospf_make_md5_digest() */
      break;
    default:
      break;
    }

  return 0;
}

/* Fill rest of OSPF header. */
void
ospf_fill_header (struct ospf_interface *oi,
		  struct stream *s, u_int16_t length)
{
  struct ospf_header *ospfh;

  ospfh = (struct ospf_header *) STREAM_DATA (s);

  /* Fill length. */
  ospfh->length = htons (length);

  /* Calculate checksum. */
  if (ntohs (ospfh->auth_type) != OSPF_AUTH_CRYPTOGRAPHIC)
    ospfh->checksum = in_cksum (ospfh, length);
  else
    ospfh->checksum = 0;

  /* Add Authentication Data. */
  ospf_make_auth (oi, ospfh);
}

int
ospf_make_hello (struct ospf_interface *oi, struct stream *s)
{
  struct ospf_neighbor *nbr;
  struct route_node *rn;
  u_int16_t length = OSPF_HELLO_MIN_SIZE;
  struct in_addr mask;
  unsigned long p;
  int flag = 0;

  /* Set netmask of interface. */
  if (oi->type != OSPF_IFTYPE_POINTOPOINT &&
      oi->type != OSPF_IFTYPE_VIRTUALLINK)
    masklen2ip (oi->address->prefixlen, &mask);
  else
    memset ((char *) &mask, 0, sizeof (struct in_addr));
  stream_put_ipv4 (s, mask.s_addr);

  /* Set Hello Interval. */
  stream_putw (s, OSPF_IF_PARAM (oi, v_hello));

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("make_hello: options: %x, int: %s",
	       OPTIONS(oi), IF_NAME (oi));

  /* Set Options. */
  stream_putc (s, OPTIONS (oi));

  /* Set Router Priority. */
  stream_putc (s, PRIORITY (oi));

  /* Set Router Dead Interval. */
  stream_putl (s, OSPF_IF_PARAM (oi, v_wait));

  /* Set Designated Router. */
  stream_put_ipv4 (s, DR (oi).s_addr);

  p = s->putp;

  /* Set Backup Designated Router. */
  stream_put_ipv4 (s, BDR (oi).s_addr);

  /* Add neighbor seen. */
  for (rn = route_top (oi->nbrs); rn; rn = route_next (rn))
    if ((nbr = rn->info))
      if (nbr->router_id.s_addr != 0)	/* Ignore 0.0.0.0 node. */
	if (nbr->state != NSM_Attempt)  /* Ignore Down neighbor. */
	  if (nbr->state != NSM_Down)     /* This is myself for DR election. */
	    if (!IPV4_ADDR_SAME (&nbr->router_id, &oi->ospf->router_id))
	      {
		/* Check neighbor is sane? */
		if (nbr->d_router.s_addr != 0
		    && IPV4_ADDR_SAME (&nbr->d_router, &oi->address->u.prefix4)
		    && IPV4_ADDR_SAME (&nbr->bd_router, &oi->address->u.prefix4))
		  flag = 1;

		stream_put_ipv4 (s, nbr->router_id.s_addr);
		length += 4;
	      }

  /* Let neighbor generate BackupSeen. */
  if (flag == 1)
    {
      stream_set_putp (s, p);
      stream_put_ipv4 (s, 0);
    }

  return length;
}

int
ospf_make_db_desc (struct ospf_interface *oi, struct ospf_neighbor *nbr,
		   struct stream *s)
{
  struct ospf_lsa *lsa;
  u_int16_t length = OSPF_DB_DESC_MIN_SIZE;
  u_char options;
  unsigned long pp;
  int i;
  struct ospf_lsdb *lsdb;
  
  /* Set Interface MTU. */
  if (oi->type == OSPF_IFTYPE_VIRTUALLINK)
    stream_putw (s, 0);
  else
    stream_putw (s, oi->ifp->mtu);

  /* Set Options. */
  options = OPTIONS (oi);
#ifdef HAVE_OPAQUE_LSA
  if (CHECK_FLAG (oi->ospf->config, OSPF_OPAQUE_CAPABLE))
    {
      if (IS_SET_DD_I (nbr->dd_flags)
	  ||  CHECK_FLAG (nbr->options, OSPF_OPTION_O))
        /*
         * Set O-bit in the outgoing DD packet for capablity negotiation,
         * if one of following case is applicable. 
         *
         * 1) WaitTimer expiration event triggered the neighbor state to
         *    change to Exstart, but no (valid) DD packet has received
         *    from the neighbor yet.
         *
         * 2) At least one DD packet with O-bit on has received from the
         *    neighbor.
         */
        SET_FLAG (options, OSPF_OPTION_O);
    }
#endif /* HAVE_OPAQUE_LSA */
  stream_putc (s, options);

  /* Keep pointer to flags. */
  pp = stream_get_putp (s);
  stream_putc (s, nbr->dd_flags);

  /* Set DD Sequence Number. */
  stream_putl (s, nbr->dd_seqnum);

  if (ospf_db_summary_isempty (nbr))
    {
      if (nbr->state >= NSM_Exchange)
	{
	  nbr->dd_flags &= ~OSPF_DD_FLAG_M;
	  /* Set DD flags again */
	  stream_set_putp (s, pp);
	  stream_putc (s, nbr->dd_flags);
	}
      return length;
    }

  /* Describe LSA Header from Database Summary List. */
  lsdb = &nbr->db_sum;

  for (i = OSPF_MIN_LSA; i < OSPF_MAX_LSA; i++)
    {
      struct route_table *table = lsdb->type[i].db;
      struct route_node *rn;

      for (rn = route_top (table); rn; rn = route_next (rn))
	if ((lsa = rn->info) != NULL)
	  {
#ifdef HAVE_OPAQUE_LSA
            if (IS_OPAQUE_LSA (lsa->data->type)
		&& (! CHECK_FLAG (options, OSPF_OPTION_O)))
              {
                /* Suppress advertising opaque-informations. */
                /* Remove LSA from DB summary list. */
                ospf_lsdb_delete (lsdb, lsa);
                continue;
              }
#endif /* HAVE_OPAQUE_LSA */

	    if (!CHECK_FLAG (lsa->flags, OSPF_LSA_DISCARD))
	      {
		struct lsa_header *lsah;
		u_int16_t ls_age;
		
		/* DD packet overflows interface MTU. */
		if (length + OSPF_LSA_HEADER_SIZE > OSPF_PACKET_MAX (oi))
		  break;
		
		/* Keep pointer to LS age. */
		lsah = (struct lsa_header *) (STREAM_DATA (s) +
					      stream_get_putp (s));
		
		/* Proceed stream pointer. */
		stream_put (s, lsa->data, OSPF_LSA_HEADER_SIZE);
		length += OSPF_LSA_HEADER_SIZE;
		
		/* Set LS age. */
		ls_age = LS_AGE (lsa);
		lsah->ls_age = htons (ls_age);
		
	      }
	    
	    /* Remove LSA from DB summary list. */
	    ospf_lsdb_delete (lsdb, lsa);
	  }
    }

  return length;
}

int
ospf_make_ls_req_func (struct stream *s, u_int16_t *length,
		       unsigned long delta, struct ospf_neighbor *nbr,
		       struct ospf_lsa *lsa)
{
  struct ospf_interface *oi;

  oi = nbr->oi;

  /* LS Request packet overflows interface MTU. */
  if (*length + delta > OSPF_PACKET_MAX(oi))
    return 0;

  stream_putl (s, lsa->data->type);
  stream_put_ipv4 (s, lsa->data->id.s_addr);
  stream_put_ipv4 (s, lsa->data->adv_router.s_addr);
  
  ospf_lsa_unlock (nbr->ls_req_last);
  nbr->ls_req_last = ospf_lsa_lock (lsa);
  
  *length += 12;
  return 1;
}

int
ospf_make_ls_req (struct ospf_neighbor *nbr, struct stream *s)
{
  struct ospf_lsa *lsa;
  u_int16_t length = OSPF_LS_REQ_MIN_SIZE;
  unsigned long delta = stream_get_putp(s)+12;
  struct route_table *table;
  struct route_node *rn;
  int i;
  struct ospf_lsdb *lsdb;

  lsdb = &nbr->ls_req;

  for (i = OSPF_MIN_LSA; i < OSPF_MAX_LSA; i++)
    {
      table = lsdb->type[i].db;
      for (rn = route_top (table); rn; rn = route_next (rn))
	if ((lsa = (rn->info)) != NULL)
	  if (ospf_make_ls_req_func (s, &length, delta, nbr, lsa) == 0)
	    {
	      route_unlock_node (rn);
	      break;
	    }
    }
  return length;
}

int
ls_age_increment (struct ospf_lsa *lsa, int delay)
{
  int age;

  age = IS_LSA_MAXAGE (lsa) ? OSPF_LSA_MAXAGE : LS_AGE (lsa) + delay;

  return (age > OSPF_LSA_MAXAGE ? OSPF_LSA_MAXAGE : age);
}

int
ospf_make_ls_upd (struct ospf_interface *oi, list update, struct stream *s)
{
  struct ospf_lsa *lsa;
  listnode node;
  u_int16_t length = OSPF_LS_UPD_MIN_SIZE;
  unsigned long delta = stream_get_putp (s);
  unsigned long pp;
  int count = 0;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info("ospf_make_ls_upd: Start");
  
  pp = stream_get_putp (s);
  ospf_output_forward (s, 4);

  while ((node = listhead (update)) != NULL)
    {
      struct lsa_header *lsah;
      u_int16_t ls_age;

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info("ospf_make_ls_upd: List Iteration");

      lsa = getdata (node);
      assert (lsa);
      assert (lsa->data);

      /* Check packet size. */
      if (length + delta + ntohs (lsa->data->length) > OSPF_PACKET_MAX (oi))
	break;
      
      /* Keep pointer to LS age. */
      lsah = (struct lsa_header *) (STREAM_DATA (s) + stream_get_putp (s));

      /* Put LSA to Link State Request. */
      stream_put (s, lsa->data, ntohs (lsa->data->length));

      /* Set LS age. */
      /* each hop must increment an lsa_age by transmit_delay 
         of OSPF interface */
      ls_age = ls_age_increment (lsa, OSPF_IF_PARAM (oi, transmit_delay));
      lsah->ls_age = htons (ls_age);

      length += ntohs (lsa->data->length);
      count++;

      list_delete_node (update, node);
      ospf_lsa_unlock (lsa);
    }

  /* Now set #LSAs. */
  stream_set_putp (s, pp);
  stream_putl (s, count);

  stream_set_putp (s, s->endp);

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info("ospf_make_ls_upd: Stop");
  return length;
}

int
ospf_make_ls_ack (struct ospf_interface *oi, list ack, struct stream *s)
{
  list rm_list;
  listnode node;
  u_int16_t length = OSPF_LS_ACK_MIN_SIZE;
  unsigned long delta = stream_get_putp(s) + 24;
  struct ospf_lsa *lsa;

  rm_list = list_new ();
  
  for (node = listhead (ack); node; nextnode (node))
    {
      lsa = getdata (node);
      assert (lsa);
      
      if (length + delta > OSPF_PACKET_MAX (oi))
	break;
      
      stream_put (s, lsa->data, OSPF_LSA_HEADER_SIZE);
      length += OSPF_LSA_HEADER_SIZE;
      
      listnode_add (rm_list, lsa);
    }
  
  /* Remove LSA from LS-Ack list. */
  for (node = listhead (rm_list); node; nextnode (node))
    {
      lsa = (struct ospf_lsa *) getdata (node);
      
      listnode_delete (ack, lsa);
      ospf_lsa_unlock (lsa);
    }
  
  list_delete (rm_list);
  
  return length;
}

void
ospf_hello_send_sub (struct ospf_interface *oi, struct in_addr *addr)
{
  struct ospf_packet *op;
  u_int16_t length = OSPF_HEADER_SIZE;

  op = ospf_packet_new (oi->ifp->mtu);

  /* Prepare OSPF common header. */
  ospf_make_header (OSPF_MSG_HELLO, oi, op->s);

  /* Prepare OSPF Hello body. */
  length += ospf_make_hello (oi, op->s);

  /* Fill OSPF header. */
  ospf_fill_header (oi, op->s, length);

  /* Set packet length. */
  op->length = length;

  op->dst.s_addr = addr->s_addr;

  /* Add packet to the interface output queue. */
  ospf_packet_add (oi, op);

  /* Hook thread to write packet. */
  OSPF_ISM_WRITE_ON (oi->ospf);
}

void
ospf_poll_send (struct ospf_nbr_nbma *nbr_nbma)
{
  struct ospf_interface *oi;

  oi = nbr_nbma->oi;
  assert(oi);

  /* If this is passive interface, do not send OSPF Hello. */
  if (OSPF_IF_PARAM (oi, passive_interface) == OSPF_IF_PASSIVE)
    return;

  if (oi->type != OSPF_IFTYPE_NBMA)
    return;

  if (nbr_nbma->nbr != NULL && nbr_nbma->nbr->state != NSM_Down)
    return;

  if (PRIORITY(oi) == 0)
    return;

  if (nbr_nbma->priority == 0
      && oi->state != ISM_DR && oi->state != ISM_Backup)
    return;

  ospf_hello_send_sub (oi, &nbr_nbma->addr);
}

int
ospf_poll_timer (struct thread *thread)
{
  struct ospf_nbr_nbma *nbr_nbma;

  nbr_nbma = THREAD_ARG (thread);
  nbr_nbma->t_poll = NULL;

  if (IS_DEBUG_OSPF (nsm, NSM_TIMERS))
    zlog (NULL, LOG_INFO, "NSM[%s:%s]: Timer (Poll timer expire)",
	  IF_NAME (nbr_nbma->oi), inet_ntoa (nbr_nbma->addr));

  ospf_poll_send (nbr_nbma);

  if (nbr_nbma->v_poll > 0)
    OSPF_POLL_TIMER_ON (nbr_nbma->t_poll, ospf_poll_timer,
			nbr_nbma->v_poll);

  return 0;
}


int
ospf_hello_reply_timer (struct thread *thread)
{
  struct ospf_neighbor *nbr;

  nbr = THREAD_ARG (thread);
  nbr->t_hello_reply = NULL;

  assert (nbr->oi);

  if (IS_DEBUG_OSPF (nsm, NSM_TIMERS))
    zlog (NULL, LOG_INFO, "NSM[%s:%s]: Timer (hello-reply timer expire)",
	  IF_NAME (nbr->oi), inet_ntoa (nbr->router_id));

  ospf_hello_send_sub (nbr->oi, &nbr->address.u.prefix4);

  return 0;
}

/* Send OSPF Hello. */
void
ospf_hello_send (struct ospf_interface *oi)
{
  struct ospf_packet *op;
  u_int16_t length = OSPF_HEADER_SIZE;

  /* If this is passive interface, do not send OSPF Hello. */
  if (OSPF_IF_PARAM (oi, passive_interface) == OSPF_IF_PASSIVE)
    return;

  op = ospf_packet_new (oi->ifp->mtu);

  /* Prepare OSPF common header. */
  ospf_make_header (OSPF_MSG_HELLO, oi, op->s);

  /* Prepare OSPF Hello body. */
  length += ospf_make_hello (oi, op->s);

  /* Fill OSPF header. */
  ospf_fill_header (oi, op->s, length);

  /* Set packet length. */
  op->length = length;

  if (oi->type == OSPF_IFTYPE_NBMA)
    {
      struct ospf_neighbor *nbr;
      struct route_node *rn;

      for (rn = route_top (oi->nbrs); rn; rn = route_next (rn))
	if ((nbr = rn->info))
	  if (nbr != oi->nbr_self)
	    if (nbr->state != NSM_Down)
	      {
		/*  RFC 2328  Section 9.5.1
		    If the router is not eligible to become Designated Router,
		    it must periodically send Hello Packets to both the
		    Designated Router and the Backup Designated Router (if they
		    exist).  */
		if (PRIORITY(oi) == 0 &&
		    IPV4_ADDR_CMP(&DR(oi),  &nbr->address.u.prefix4) &&
		    IPV4_ADDR_CMP(&BDR(oi), &nbr->address.u.prefix4))
		  continue;

		/*  If the router is eligible to become Designated Router, it
		    must periodically send Hello Packets to all neighbors that
		    are also eligible. In addition, if the router is itself the
		    Designated Router or Backup Designated Router, it must also
		    send periodic Hello Packets to all other neighbors. */

		if (nbr->priority == 0 && oi->state == ISM_DROther)
		  continue;
		/* if oi->state == Waiting, send hello to all neighbors */
		{
		  struct ospf_packet *op_dup;

		  op_dup = ospf_packet_dup(op);
		  op_dup->dst = nbr->address.u.prefix4;

		  /* Add packet to the interface output queue. */
		  ospf_packet_add (oi, op_dup);

		  OSPF_ISM_WRITE_ON (oi->ospf);
		}

	      }
      ospf_packet_free (op);
    }
  else
    {
      /* Decide destination address. */
      if (oi->type == OSPF_IFTYPE_VIRTUALLINK)
	op->dst.s_addr = oi->vl_data->peer_addr.s_addr;
      else 
	op->dst.s_addr = htonl (OSPF_ALLSPFROUTERS);

      /* Add packet to the interface output queue. */
      ospf_packet_add (oi, op);

      /* Hook thread to write packet. */
      OSPF_ISM_WRITE_ON (oi->ospf);
    }
}

/* Send OSPF Database Description. */
void
ospf_db_desc_send (struct ospf_neighbor *nbr)
{
  struct ospf_interface *oi;
  struct ospf_packet *op;
  u_int16_t length = OSPF_HEADER_SIZE;

  oi = nbr->oi;
  op = ospf_packet_new (oi->ifp->mtu);

  /* Prepare OSPF common header. */
  ospf_make_header (OSPF_MSG_DB_DESC, oi, op->s);

  /* Prepare OSPF Database Description body. */
  length += ospf_make_db_desc (oi, nbr, op->s);

  /* Fill OSPF header. */
  ospf_fill_header (oi, op->s, length);

  /* Set packet length. */
  op->length = length;

  /* Decide destination address. */
  op->dst = nbr->address.u.prefix4;

  /* Add packet to the interface output queue. */
  ospf_packet_add (oi, op);

  /* Hook thread to write packet. */
  OSPF_ISM_WRITE_ON (oi->ospf);

  /* Remove old DD packet, then copy new one and keep in neighbor structure. */
  if (nbr->last_send)
    ospf_packet_free (nbr->last_send);
  nbr->last_send = ospf_packet_dup (op);
  gettimeofday (&nbr->last_send_ts, NULL);
}

/* Re-send Database Description. */
void
ospf_db_desc_resend (struct ospf_neighbor *nbr)
{
  struct ospf_interface *oi;

  oi = nbr->oi;

  /* Add packet to the interface output queue. */
  ospf_packet_add (oi, ospf_packet_dup (nbr->last_send));

  /* Hook thread to write packet. */
  OSPF_ISM_WRITE_ON (oi->ospf);
}

/* Send Link State Request. */
void
ospf_ls_req_send (struct ospf_neighbor *nbr)
{
  struct ospf_interface *oi;
  struct ospf_packet *op;
  u_int16_t length = OSPF_HEADER_SIZE;

  oi = nbr->oi;
  op = ospf_packet_new (oi->ifp->mtu);

  /* Prepare OSPF common header. */
  ospf_make_header (OSPF_MSG_LS_REQ, oi, op->s);

  /* Prepare OSPF Link State Request body. */
  length += ospf_make_ls_req (nbr, op->s);
  if (length == OSPF_HEADER_SIZE)
    {
      ospf_packet_free (op);
      return;
    }

  /* Fill OSPF header. */
  ospf_fill_header (oi, op->s, length);

  /* Set packet length. */
  op->length = length;

  /* Decide destination address. */
  op->dst = nbr->address.u.prefix4;

  /* Add packet to the interface output queue. */
  ospf_packet_add (oi, op);

  /* Hook thread to write packet. */
  OSPF_ISM_WRITE_ON (oi->ospf);

  /* Add Link State Request Retransmission Timer. */
  OSPF_NSM_TIMER_ON (nbr->t_ls_req, ospf_ls_req_timer, nbr->v_ls_req);
}

/* Send Link State Update with an LSA. */
void
ospf_ls_upd_send_lsa (struct ospf_neighbor *nbr, struct ospf_lsa *lsa,
		      int flag)
{
  list update;

  update = list_new ();

  listnode_add (update, lsa);
  ospf_ls_upd_send (nbr, update, flag);

  list_delete (update);
}

static void
ospf_ls_upd_queue_send (struct ospf_interface *oi, list update,
			struct in_addr addr)
{
  struct ospf_packet *op;
  u_int16_t length = OSPF_HEADER_SIZE;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("listcount = %d, dst %s", listcount (update), inet_ntoa(addr));

  op = ospf_packet_new (oi->ifp->mtu);

  /* Prepare OSPF common header. */
  ospf_make_header (OSPF_MSG_LS_UPD, oi, op->s);

  /* Prepare OSPF Link State Update body. */
  /* Includes Type-7 translation. */
  length += ospf_make_ls_upd (oi, update, op->s);

  /* Fill OSPF header. */
  ospf_fill_header (oi, op->s, length);

  /* Set packet length. */
  op->length = length;

  /* Decide destination address. */
  op->dst.s_addr = addr.s_addr;

  /* Add packet to the interface output queue. */
  ospf_packet_add (oi, op);

  /* Hook thread to write packet. */
  OSPF_ISM_WRITE_ON (oi->ospf);
}

static int
ospf_ls_upd_send_queue_event (struct thread *thread)
{
  struct ospf_interface *oi = THREAD_ARG(thread);
  struct route_node *rn;
  
  oi->t_ls_upd_event = NULL;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_ls_upd_send_queue start");

  for (rn = route_top (oi->ls_upd_queue); rn; rn = route_next (rn))
    {
      if (rn->info == NULL)
	continue;

      while (!list_isempty ((list)rn->info))
	ospf_ls_upd_queue_send (oi, rn->info, rn->p.u.prefix4);

      list_delete (rn->info);
      rn->info = NULL;
      
      route_unlock_node (rn);
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_ls_upd_send_queue stop");
  return 0;
}

void
ospf_ls_upd_send (struct ospf_neighbor *nbr, list update, int flag)
{
  struct ospf_interface *oi;
  struct prefix_ipv4 p;
  struct route_node *rn;
  listnode n;
  
  oi = nbr->oi;

  p.family = AF_INET;
  p.prefixlen = IPV4_MAX_BITLEN;
  
  /* Decide destination address. */
  if (oi->type == OSPF_IFTYPE_VIRTUALLINK)
    p.prefix = oi->vl_data->peer_addr;
  else if (flag == OSPF_SEND_PACKET_DIRECT)
    p.prefix = nbr->address.u.prefix4;
  else if (oi->state == ISM_DR || oi->state == ISM_Backup)
    p.prefix.s_addr = htonl (OSPF_ALLSPFROUTERS);
  else if ((oi->type == OSPF_IFTYPE_POINTOPOINT) 
	   && (flag == OSPF_SEND_PACKET_INDIRECT))
    p.prefix.s_addr = htonl (OSPF_ALLSPFROUTERS);
  else if (oi->type == OSPF_IFTYPE_POINTOMULTIPOINT)
    p.prefix.s_addr = htonl (OSPF_ALLSPFROUTERS);
  else
    p.prefix.s_addr = htonl (OSPF_ALLDROUTERS);

  if (oi->type == OSPF_IFTYPE_NBMA)
    {
      if (flag == OSPF_SEND_PACKET_INDIRECT)
	zlog_warn ("* LS-Update is directly sent on NBMA network.");
      if (IPV4_ADDR_SAME(&oi->address->u.prefix4, &p.prefix.s_addr))
	zlog_warn ("* LS-Update is sent to myself.");
    }

  rn = route_node_get (oi->ls_upd_queue, (struct prefix *) &p);

  if (rn->info == NULL)
    rn->info = list_new ();

  for (n = listhead (update); n; nextnode (n))
    listnode_add (rn->info, ospf_lsa_lock (getdata (n)));

  if (oi->t_ls_upd_event == NULL)
    oi->t_ls_upd_event =
      thread_add_event (master, ospf_ls_upd_send_queue_event, oi, 0);
}

static void
ospf_ls_ack_send_list (struct ospf_interface *oi, list ack, struct in_addr dst)
{
  struct ospf_packet *op;
  u_int16_t length = OSPF_HEADER_SIZE;

  op = ospf_packet_new (oi->ifp->mtu);

  /* Prepare OSPF common header. */
  ospf_make_header (OSPF_MSG_LS_ACK, oi, op->s);

  /* Prepare OSPF Link State Acknowledgment body. */
  length += ospf_make_ls_ack (oi, ack, op->s);

  /* Fill OSPF header. */
  ospf_fill_header (oi, op->s, length);

  /* Set packet length. */
  op->length = length;

  /* Set destination IP address. */
  op->dst = dst;
  
  /* Add packet to the interface output queue. */
  ospf_packet_add (oi, op);

  /* Hook thread to write packet. */
  OSPF_ISM_WRITE_ON (oi->ospf);
}

static int
ospf_ls_ack_send_event (struct thread *thread)
{
  struct ospf_interface *oi = THREAD_ARG (thread);

  oi->t_ls_ack_direct = NULL;
  
  while (listcount (oi->ls_ack_direct.ls_ack))
    ospf_ls_ack_send_list (oi, oi->ls_ack_direct.ls_ack,
			   oi->ls_ack_direct.dst);

  return 0;
}

void
ospf_ls_ack_send (struct ospf_neighbor *nbr, struct ospf_lsa *lsa)
{
  struct ospf_interface *oi = nbr->oi;

  if (listcount (oi->ls_ack_direct.ls_ack) == 0)
    oi->ls_ack_direct.dst = nbr->address.u.prefix4;
  
  listnode_add (oi->ls_ack_direct.ls_ack, ospf_lsa_lock (lsa));
  
  if (oi->t_ls_ack_direct == NULL)
    oi->t_ls_ack_direct =
      thread_add_event (master, ospf_ls_ack_send_event, oi, 0);
}

/* Send Link State Acknowledgment delayed. */
void
ospf_ls_ack_send_delayed (struct ospf_interface *oi)
{
  struct in_addr dst;
  
  /* Decide destination address. */
  /* RFC2328 Section 13.5                           On non-broadcast
     networks, delayed Link State Acknowledgment packets must be
     unicast	separately over	each adjacency (i.e., neighbor whose
     state is >= Exchange).  */
  if (oi->type == OSPF_IFTYPE_NBMA)
    {
      struct ospf_neighbor *nbr;
      struct route_node *rn;

      for (rn = route_top (oi->nbrs); rn; rn = route_next (rn))
	if ((nbr = rn->info) != NULL)
	  if (nbr != oi->nbr_self && nbr->state >= NSM_Exchange)
	    while (listcount (oi->ls_ack))
	      ospf_ls_ack_send_list (oi, oi->ls_ack, nbr->address.u.prefix4);
      return;
    }
  if (oi->type == OSPF_IFTYPE_VIRTUALLINK)
    dst.s_addr = oi->vl_data->peer_addr.s_addr;
  else if (oi->state == ISM_DR || oi->state == ISM_Backup)
    dst.s_addr = htonl (OSPF_ALLSPFROUTERS);
  else if (oi->type == OSPF_IFTYPE_POINTOPOINT)
    dst.s_addr = htonl (OSPF_ALLSPFROUTERS);
  else
    dst.s_addr = htonl (OSPF_ALLDROUTERS);

  while (listcount (oi->ls_ack))
    ospf_ls_ack_send_list (oi, oi->ls_ack, dst);
}
