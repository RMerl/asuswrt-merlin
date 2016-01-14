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
#include "sockopt.h"
#include "checksum.h"
#include "md5.h"

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

/* Packet Type String. */
const struct message ospf_packet_type_str[] =
{
  { OSPF_MSG_HELLO,   "Hello"                     },
  { OSPF_MSG_DB_DESC, "Database Description"      },
  { OSPF_MSG_LS_REQ,  "Link State Request"        },
  { OSPF_MSG_LS_UPD,  "Link State Update"         },
  { OSPF_MSG_LS_ACK,  "Link State Acknowledgment" },
};
const size_t ospf_packet_type_str_max = sizeof (ospf_packet_type_str) /
  sizeof (ospf_packet_type_str[0]);

/* Minimum (besides OSPF_HEADER_SIZE) lengths for OSPF packets of
   particular types, offset is the "type" field of a packet. */
static const u_int16_t ospf_packet_minlen[] =
{
  0,
  OSPF_HELLO_MIN_SIZE,
  OSPF_DB_DESC_MIN_SIZE,
  OSPF_LS_REQ_MIN_SIZE,
  OSPF_LS_UPD_MIN_SIZE,
  OSPF_LS_ACK_MIN_SIZE,
};

/* Minimum (besides OSPF_LSA_HEADER_SIZE) lengths for LSAs of particular
   types, offset is the "LSA type" field. */
static const u_int16_t ospf_lsa_minlen[] =
{
  0,
  OSPF_ROUTER_LSA_MIN_SIZE,
  OSPF_NETWORK_LSA_MIN_SIZE,
  OSPF_SUMMARY_LSA_MIN_SIZE,
  OSPF_SUMMARY_LSA_MIN_SIZE,
  OSPF_AS_EXTERNAL_LSA_MIN_SIZE,
  0,
  OSPF_AS_EXTERNAL_LSA_MIN_SIZE,
  0,
  0,
  0,
  0,
};

/* for ospf_check_auth() */
static int ospf_check_sum (struct ospf_header *);

/* OSPF authentication checking function */
static int
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

/* Add new packet to head of fifo. */
static void
ospf_fifo_push_head (struct ospf_fifo *fifo, struct ospf_packet *op)
{
  op->next = fifo->head;
  
  if (fifo->tail == NULL)
    fifo->tail = op;
  
  fifo->head = op;
  
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
  if (!oi->obuf)
    {
      zlog_err("ospf_packet_add(interface %s in state %d [%s], packet type %s, "
	       "destination %s) called with NULL obuf, ignoring "
	       "(please report this bug)!\n",
	       IF_NAME(oi), oi->state, LOOKUP (ospf_ism_state_msg, oi->state),
	       LOOKUP (ospf_packet_type_str, stream_getc_from(op->s, 1)),
	       inet_ntoa (op->dst));
      return;
    }

  /* Add packet to end of queue. */
  ospf_fifo_push (oi->obuf, op);

  /* Debug of packet fifo*/
  /* ospf_fifo_debug (oi->obuf); */
}

static void
ospf_packet_add_top (struct ospf_interface *oi, struct ospf_packet *op)
{
  if (!oi->obuf)
    {
      zlog_err("ospf_packet_add(interface %s in state %d [%s], packet type %s, "
	       "destination %s) called with NULL obuf, ignoring "
	       "(please report this bug)!\n",
	       IF_NAME(oi), oi->state, LOOKUP (ospf_ism_state_msg, oi->state),
	       LOOKUP (ospf_packet_type_str, stream_getc_from(op->s, 1)),
	       inet_ntoa (op->dst));
      return;
    }

  /* Add packet to head of queue. */
  ospf_fifo_push_head (oi->obuf, op);

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

struct ospf_packet *
ospf_packet_dup (struct ospf_packet *op)
{
  struct ospf_packet *new;

  if (stream_get_endp(op->s) != op->length)
    /* XXX size_t */
    zlog_warn ("ospf_packet_dup stream %lu ospf_packet %u size mismatch",
	       (u_long)STREAM_SIZE(op->s), op->length);

  /* Reserve space for MD5 authentication that may be added later. */
  new = ospf_packet_new (stream_get_endp(op->s) + OSPF_AUTH_MD5_SIZE);
  stream_copy (new->s, op->s);

  new->dst = op->dst;
  new->length = op->length;

  return new;
}

/* XXX inline */
static unsigned int
ospf_packet_authspace (struct ospf_interface *oi)
{
  int auth = 0;

  if ( ospf_auth_type (oi) == OSPF_AUTH_CRYPTOGRAPHIC)
    auth = OSPF_AUTH_MD5_SIZE;

  return auth;
}

static unsigned int
ospf_packet_max (struct ospf_interface *oi)
{
  int max;

  max = oi->ifp->mtu - ospf_packet_authspace(oi);

  max -= (OSPF_HEADER_SIZE + sizeof (struct ip));

  return max;
}


static int
ospf_check_md5_digest (struct ospf_interface *oi, struct ospf_header *ospfh)
{
  MD5_CTX ctx;
  unsigned char digest[OSPF_AUTH_MD5_SIZE];
  struct crypt_key *ck;
  struct ospf_neighbor *nbr;
  u_int16_t length = ntohs (ospfh->length);
  
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
  memset(&ctx, 0, sizeof(ctx));
  MD5Init(&ctx);
  MD5Update(&ctx, ospfh, length);
  MD5Update(&ctx, ck->auth_key, OSPF_AUTH_MD5_SIZE);
  MD5Final(digest, &ctx);

  /* compare the two */
  if (memcmp ((caddr_t)ospfh + length, digest, OSPF_AUTH_MD5_SIZE))
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
static int
ospf_make_md5_digest (struct ospf_interface *oi, struct ospf_packet *op)
{
  struct ospf_header *ospfh;
  unsigned char digest[OSPF_AUTH_MD5_SIZE] = {0};
  MD5_CTX ctx;
  void *ibuf;
  u_int32_t t;
  struct crypt_key *ck;
  const u_int8_t *auth_key;

  ibuf = STREAM_DATA (op->s);
  ospfh = (struct ospf_header *) ibuf;

  if (ntohs (ospfh->auth_type) != OSPF_AUTH_CRYPTOGRAPHIC)
    return 0;

  /* We do this here so when we dup a packet, we don't have to
     waste CPU rewriting other headers.
     
     Note that quagga_time /deliberately/ is not used here */
  t = (time(NULL) & 0xFFFFFFFF);
  if (t > oi->crypt_seqnum)
    oi->crypt_seqnum = t;
  else
    oi->crypt_seqnum++;
  
  ospfh->u.crypt.crypt_seqnum = htonl (oi->crypt_seqnum); 

  /* Get MD5 Authentication key from auth_key list. */
  if (list_isempty (OSPF_IF_PARAM (oi, auth_crypt)))
    auth_key = (const u_int8_t *) digest;
  else
    {
      ck = listgetdata (listtail(OSPF_IF_PARAM (oi, auth_crypt)));
      auth_key = ck->auth_key;
    }

  /* Generate a digest for the entire packet + our secret key. */
  memset(&ctx, 0, sizeof(ctx));
  MD5Init(&ctx);
  MD5Update(&ctx, ibuf, ntohs (ospfh->length));
  MD5Update(&ctx, auth_key, OSPF_AUTH_MD5_SIZE);
  MD5Final(digest, &ctx);

  /* Append md5 digest to the end of the stream. */
  stream_put (op->s, digest, OSPF_AUTH_MD5_SIZE);

  /* We do *NOT* increment the OSPF header length. */
  op->length = ntohs (ospfh->length) + OSPF_AUTH_MD5_SIZE;

  if (stream_get_endp(op->s) != op->length)
    /* XXX size_t */
    zlog_warn("ospf_make_md5_digest: length mismatch stream %lu ospf_packet %u",
	      (u_long)stream_get_endp(op->s), op->length);

  return OSPF_AUTH_MD5_SIZE;
}


static int
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
      struct list *update;
      struct ospf_lsdb *lsdb;
      int i;
      int retransmit_interval;

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
		if (tv_cmp (tv_sub (recent_relative_time (), lsa->tv_recv), 
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

#ifdef WANT_OSPF_WRITE_FRAGMENT
static void
ospf_write_frags (int fd, struct ospf_packet *op, struct ip *iph, 
                  struct msghdr *msg, unsigned int maxdatasize, 
                  unsigned int mtu, int flags, u_char type)
{
#define OSPF_WRITE_FRAG_SHIFT 3
  u_int16_t offset;
  struct iovec *iovp;
  int ret;

  assert ( op->length == stream_get_endp(op->s) );
  assert (msg->msg_iovlen == 2);

  /* we can but try.
   *
   * SunOS, BSD and BSD derived kernels likely will clear ip_id, as
   * well as the IP_MF flag, making this all quite pointless.
   *
   * However, for a system on which IP_MF is left alone, and ip_id left
   * alone or else which sets same ip_id for each fragment this might
   * work, eg linux.
   *
   * XXX-TODO: It would be much nicer to have the kernel's use their
   * existing fragmentation support to do this for us. Bugs/RFEs need to
   * be raised against the various kernels.
   */
  
  /* set More Frag */
  iph->ip_off |= IP_MF;
  
  /* ip frag offset is expressed in units of 8byte words */
  offset = maxdatasize >> OSPF_WRITE_FRAG_SHIFT;
  
  iovp = &msg->msg_iov[1];
  
  while ( (stream_get_endp(op->s) - stream_get_getp (op->s)) 
         > maxdatasize )
    {
      /* data length of this frag is to next offset value */
      iovp->iov_len = offset << OSPF_WRITE_FRAG_SHIFT;
      iph->ip_len = iovp->iov_len + sizeof (struct ip);
      assert (iph->ip_len <= mtu);

      sockopt_iphdrincl_swab_htosys (iph);

      ret = sendmsg (fd, msg, flags);
      
      sockopt_iphdrincl_swab_systoh (iph);
      
      if (ret < 0)
        zlog_warn ("*** ospf_write_frags: sendmsg failed to %s,"
		   " id %d, off %d, len %d, mtu %u failed with %s",
		   inet_ntoa (iph->ip_dst),
		   iph->ip_id,
		   iph->ip_off,
		   iph->ip_len,
		   mtu,
		   safe_strerror (errno));
      
      if (IS_DEBUG_OSPF_PACKET (type - 1, SEND))
        {
          zlog_debug ("ospf_write_frags: sent id %d, off %d, len %d to %s\n",
                     iph->ip_id, iph->ip_off, iph->ip_len,
                     inet_ntoa (iph->ip_dst));
          if (IS_DEBUG_OSPF_PACKET (type - 1, DETAIL))
            {
              zlog_debug ("-----------------IP Header Dump----------------------");
              ospf_ip_header_dump (iph);
              zlog_debug ("-----------------------------------------------------");
            }
        }
      
      iph->ip_off += offset;
      stream_forward_getp (op->s, iovp->iov_len);
      iovp->iov_base = STREAM_PNT (op->s); 
    }
    
  /* setup for final fragment */
  iovp->iov_len = stream_get_endp(op->s) - stream_get_getp (op->s);
  iph->ip_len = iovp->iov_len + sizeof (struct ip);
  iph->ip_off &= (~IP_MF);
}
#endif /* WANT_OSPF_WRITE_FRAGMENT */

static int
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
  struct listnode *node;
#ifdef WANT_OSPF_WRITE_FRAGMENT
  static u_int16_t ipid = 0;
#endif /* WANT_OSPF_WRITE_FRAGMENT */
  u_int16_t maxdatasize;
#define OSPF_WRITE_IPHL_SHIFT 2
  
  ospf->t_write = NULL;

  node = listhead (ospf->oi_write_q);
  assert (node);
  oi = listgetdata (node);
  assert (oi);

#ifdef WANT_OSPF_WRITE_FRAGMENT
  /* seed ipid static with low order bits of time */
  if (ipid == 0)
    ipid = (time(NULL) & 0xffff);
#endif /* WANT_OSPF_WRITE_FRAGMENT */

  /* convenience - max OSPF data per packet,
   * and reliability - not more data, than our
   * socket can accept
   */
  maxdatasize = MIN (oi->ifp->mtu, ospf->maxsndbuflen) -
    sizeof (struct ip);
  
  /* Get one packet from queue. */
  op = ospf_fifo_head (oi->obuf);
  assert (op);
  assert (op->length >= OSPF_HEADER_SIZE);

  if (op->dst.s_addr == htonl (OSPF_ALLSPFROUTERS)
      || op->dst.s_addr == htonl (OSPF_ALLDROUTERS))
      ospf_if_ipmulticast (ospf, oi->address, oi->ifp->ifindex);
    
  /* Rewrite the md5 signature & update the seq */
  ospf_make_md5_digest (oi, op);

  /* Retrieve OSPF packet type. */
  stream_set_getp (op->s, 1);
  type = stream_getc (op->s);
  
  /* reset get pointer */
  stream_set_getp (op->s, 0);

  memset (&iph, 0, sizeof (struct ip));
  memset (&sa_dst, 0, sizeof (sa_dst));
  
  sa_dst.sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  sa_dst.sin_len = sizeof(sa_dst);
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */
  sa_dst.sin_addr = op->dst;
  sa_dst.sin_port = htons (0);

  /* Set DONTROUTE flag if dst is unicast. */
  if (oi->type != OSPF_IFTYPE_VIRTUALLINK)
    if (!IN_MULTICAST (htonl (op->dst.s_addr)))
      flags = MSG_DONTROUTE;

  iph.ip_hl = sizeof (struct ip) >> OSPF_WRITE_IPHL_SHIFT;
  /* it'd be very strange for header to not be 4byte-word aligned but.. */
  if ( sizeof (struct ip) 
        > (unsigned int)(iph.ip_hl << OSPF_WRITE_IPHL_SHIFT) )
    iph.ip_hl++; /* we presume sizeof struct ip cant overflow ip_hl.. */
  
  iph.ip_v = IPVERSION;
  iph.ip_tos = IPTOS_PREC_INTERNETCONTROL;
  iph.ip_len = (iph.ip_hl << OSPF_WRITE_IPHL_SHIFT) + op->length;

#if defined(__DragonFly__)
  /*
   * DragonFly's raw socket expects ip_len/ip_off in network byte order.
   */
  iph.ip_len = htons(iph.ip_len);
#endif

#ifdef WANT_OSPF_WRITE_FRAGMENT
  /* XXX-MT: not thread-safe at all..
   * XXX: this presumes this is only programme sending OSPF packets 
   * otherwise, no guarantee ipid will be unique
   */
  iph.ip_id = ++ipid;
#endif /* WANT_OSPF_WRITE_FRAGMENT */

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
  msg.msg_name = (caddr_t) &sa_dst;
  msg.msg_namelen = sizeof (sa_dst); 
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;
  iov[0].iov_base = (char*)&iph;
  iov[0].iov_len = iph.ip_hl << OSPF_WRITE_IPHL_SHIFT;
  iov[1].iov_base = STREAM_PNT (op->s);
  iov[1].iov_len = op->length;
  
  /* Sadly we can not rely on kernels to fragment packets because of either
   * IP_HDRINCL and/or multicast destination being set.
   */
#ifdef WANT_OSPF_WRITE_FRAGMENT
  if ( op->length > maxdatasize )
    ospf_write_frags (ospf->fd, op, &iph, &msg, maxdatasize, 
                      oi->ifp->mtu, flags, type);
#endif /* WANT_OSPF_WRITE_FRAGMENT */

  /* send final fragment (could be first) */
  sockopt_iphdrincl_swab_htosys (&iph);
  ret = sendmsg (ospf->fd, &msg, flags);
  sockopt_iphdrincl_swab_systoh (&iph);
  
  if (ret < 0)
    zlog_warn ("*** sendmsg in ospf_write failed to %s, "
	       "id %d, off %d, len %d, interface %s, mtu %u: %s",
	       inet_ntoa (iph.ip_dst), iph.ip_id, iph.ip_off, iph.ip_len,
	       oi->ifp->name, oi->ifp->mtu, safe_strerror (errno));

  /* Show debug sending packet. */
  if (IS_DEBUG_OSPF_PACKET (type - 1, SEND))
    {
      if (IS_DEBUG_OSPF_PACKET (type - 1, DETAIL))
	{
	  zlog_debug ("-----------------------------------------------------");
	  ospf_ip_header_dump (&iph);
	  stream_set_getp (op->s, 0);
	  ospf_packet_dump (op->s);
	}

      zlog_debug ("%s sent to [%s] via [%s].",
		 LOOKUP (ospf_packet_type_str, type), inet_ntoa (op->dst),
		 IF_NAME (oi));

      if (IS_DEBUG_OSPF_PACKET (type - 1, DETAIL))
	zlog_debug ("-----------------------------------------------------");
    }

  /* Now delete packet from queue. */
  ospf_packet_delete (oi);

  /* Move this interface to the tail of write_q to
	 serve everyone in a round robin fashion */
  listnode_move_to_tail (ospf->oi_write_q, node);
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
static void
ospf_hello (struct ip *iph, struct ospf_header *ospfh,
	    struct stream * s, struct ospf_interface *oi, int size)
{
  struct ospf_hello *hello;
  struct ospf_neighbor *nbr;
  int old_state;
  struct prefix p;

  /* increment statistics. */
  oi->hello_in++;

  hello = (struct ospf_hello *) STREAM_PNT (s);

  /* If Hello is myself, silently discard. */
  if (IPV4_ADDR_SAME (&ospfh->router_id, &oi->ospf->router_id))
    {
      if (IS_DEBUG_OSPF_PACKET (ospfh->type - 1, RECV))
        {
          zlog_debug ("ospf_header[%s/%s]: selforiginated, "
                     "dropping.",
                     LOOKUP (ospf_packet_type_str, ospfh->type),
                     inet_ntoa (iph->ip_src));
        }
      return;
    }

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
	zlog_warn ("Packet %s [Hello:RECV]: NetworkMask mismatch on %s (configured prefix length is %d, but hello packet indicates %d).",
		   inet_ntoa(ospfh->router_id), IF_NAME(oi),
		   (int)oi->address->prefixlen, (int)p.prefixlen);
	return;
      }

  /* Compare Router Dead Interval. */
  if (OSPF_IF_PARAM (oi, v_wait) != ntohl (hello->dead_interval))
    {
      zlog_warn ("Packet %s [Hello:RECV]: RouterDeadInterval mismatch "
      		 "(expected %u, but received %u).",
		 inet_ntoa(ospfh->router_id),
		 OSPF_IF_PARAM(oi, v_wait), ntohl(hello->dead_interval));
      return;
    }

  /* Compare Hello Interval - ignored if fast-hellos are set. */
  if (OSPF_IF_PARAM (oi, fast_hello) == 0)
    {
      if (OSPF_IF_PARAM (oi, v_hello) != ntohs (hello->hello_interval))
        {
          zlog_warn ("Packet %s [Hello:RECV]: HelloInterval mismatch "
		     "(expected %u, but received %u).",
		     inet_ntoa(ospfh->router_id),
		     OSPF_IF_PARAM(oi, v_hello), ntohs(hello->hello_interval));
          return;
        }
    }
  
  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("Packet %s [Hello:RECV]: Options %s",
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
        zlog_debug ("NSSA-Hello:RECV:Packet from %s:", inet_ntoa(ospfh->router_id));
    }
  else    
    /* The setting of the E-bit found in the Hello Packet's Options
       field must match this area's ExternalRoutingCapability A
       mismatch causes processing to stop and the packet to be
       dropped. The setting of the rest of the bits in the Hello
       Packet's Options field should be ignored. */
    if (CHECK_FLAG (OPTIONS (oi), OSPF_OPTION_E) !=
	CHECK_FLAG (hello->options, OSPF_OPTION_E))
      {
	zlog_warn ("Packet %s [Hello:RECV]: my options: %x, his options %x",
		   inet_ntoa(ospfh->router_id), OPTIONS (oi), hello->options);
	return;
      }
  
  /* get neighbour struct */
  nbr = ospf_nbr_get (oi, ospfh, iph, &p);

  /* neighbour must be valid, ospf_nbr_get creates if none existed */
  assert (nbr);

  old_state = nbr->state;

  /* Add event to thread. */
  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_PacketReceived);

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
      OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_OneWayReceived);
      nbr->priority = hello->priority;
      nbr->d_router = hello->d_router;
      nbr->bd_router = hello->bd_router;
      return;
    }

  if (ospf_nbr_bidirectional (&oi->ospf->router_id, hello->neighbors,
			      size - OSPF_HELLO_MIN_SIZE))
    {
      OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_TwoWayReceived);
      nbr->options |= hello->options;
    }
  else
    {
      OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_OneWayReceived);
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
static void
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

  stream_forward_getp (s, OSPF_DB_DESC_MIN_SIZE);
  for (size -= OSPF_DB_DESC_MIN_SIZE;
       size >= OSPF_LSA_HEADER_SIZE; size -= OSPF_LSA_HEADER_SIZE) 
    {
      lsah = (struct lsa_header *) STREAM_PNT (s);
      stream_forward_getp (s, OSPF_LSA_HEADER_SIZE);

      /* Unknown LS type. */
      if (lsah->type < OSPF_MIN_LSA || lsah->type >= OSPF_MAX_LSA)
	{
	  zlog_warn ("Packet [DD:RECV]: Unknown LS type %d.", lsah->type);
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
          /* Check for stub area.  Reject if AS-External from stub but
             allow if from NSSA. */
          if (oi->area->external_routing == OSPF_AREA_STUB)
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
      
      /* ospf_lsa_more_recent is fine with NULL pointers */
      switch (ospf_lsa_more_recent (find, new))
        {
          case -1:
            /* Neighbour has a more recent LSA, we must request it */
            ospf_ls_request_add (nbr, new);
          case 0:
            /* If we have a copy of this LSA, it's either less recent
             * and we're requesting it from neighbour (the case above), or
             * it's as recent and we both have same copy (this case).
             *
             * In neither of these two cases is there any point in
             * describing our copy of the LSA to the neighbour in a
             * DB-Summary packet, if we're still intending to do so.
             *
             * See: draft-ogier-ospf-dbex-opt-00.txt, describing the
             * backward compatible optimisation to OSPF DB Exchange /
             * DB Description process implemented here.
             */
            if (find)
              ospf_lsdb_delete (&nbr->db_sum, find);
            ospf_lsa_discard (new);
            break;
          default:
            /* We have the more recent copy, nothing specific to do:
             * - no need to request neighbours stale copy
             * - must leave DB summary list copy alone
             */
            if (IS_DEBUG_OSPF_EVENT)
              zlog_debug ("Packet [DD:RECV]: LSA received Type %d, "
                         "ID %s is not recent.", lsah->type, inet_ntoa (lsah->id));
            ospf_lsa_discard (new);
        }
    }

  /* Master */
  if (IS_SET_DD_MS (nbr->dd_flags))
    {
      nbr->dd_seqnum++;

      /* Both sides have no More, then we're done with Exchange */
      if (!IS_SET_DD_M (dd->flags) && !IS_SET_DD_M (nbr->dd_flags))
	OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_ExchangeDone);
      else
	ospf_db_desc_send (nbr);
    }
  /* Slave */
  else
    {
      nbr->dd_seqnum = ntohl (dd->dd_seqnum);

      /* Send DD packet in reply. 
       * 
       * Must be done to acknowledge the Master's DD, regardless of
       * whether we have more LSAs ourselves to describe.
       *
       * This function will clear the 'More' bit, if after this DD
       * we have no more LSAs to describe to the master..
       */
      ospf_db_desc_send (nbr);
      
      /* Slave can raise ExchangeDone now, if master is also done */
      if (!IS_SET_DD_M (dd->flags) && !IS_SET_DD_M (nbr->dd_flags))
	OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_ExchangeDone);
    }
  
  /* Save received neighbor values from DD. */
  ospf_db_desc_save_current (nbr, dd);
}

static int
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
static void
ospf_db_desc (struct ip *iph, struct ospf_header *ospfh,
	      struct stream *s, struct ospf_interface *oi, u_int16_t size)
{
  struct ospf_db_desc *dd;
  struct ospf_neighbor *nbr;

  /* Increment statistics. */
  oi->db_desc_in++;

  dd = (struct ospf_db_desc *) STREAM_PNT (s);

  nbr = ospf_nbr_lookup (oi, iph, ospfh);
  if (nbr == NULL)
    {
      zlog_warn ("Packet[DD]: Unknown Neighbor %s",
		 inet_ntoa (ospfh->router_id));
      return;
    }

  /* Check MTU. */
  if ((OSPF_IF_PARAM (oi, mtu_ignore) == 0) && 
      (ntohs (dd->mtu) > oi->ifp->mtu))
    {
      zlog_warn ("Packet[DD]: Neighbor %s MTU %u is larger than [%s]'s MTU %u",
		 inet_ntoa (nbr->router_id), ntohs (dd->mtu),
		 IF_NAME (oi), oi->ifp->mtu);
      return;
    }

  /* 
   * XXX HACK by Hasso Tepper. Setting N/P bit in NSSA area DD packets is not
   * required. In fact at least JunOS sends DD packets with P bit clear. 
   * Until proper solution is developped, this hack should help.
   *
   * Update: According to the RFCs, N bit is specified /only/ for Hello
   * options, unfortunately its use in DD options is not specified. Hence some
   * implementations follow E-bit semantics and set it in DD options, and some
   * treat it as unspecified and hence follow the directive "default for 
   * options is clear", ie unset.
   *
   * Reset the flag, as ospfd follows E-bit semantics.
   */
  if ( (oi->area->external_routing == OSPF_AREA_NSSA)
       && (CHECK_FLAG (nbr->options, OSPF_OPTION_NP))
       && (!CHECK_FLAG (dd->options, OSPF_OPTION_NP)) )
    {
      if (IS_DEBUG_OSPF_EVENT) 
        zlog_debug ("Packet[DD]: Neighbour %s: Has NSSA capability, sends with N bit clear in DD options",
                    inet_ntoa (nbr->router_id) );
      SET_FLAG (dd->options, OSPF_OPTION_NP);
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

  /* Add event to thread. */
  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_PacketReceived);

  /* Process DD packet by neighbor status. */
  switch (nbr->state)
    {
    case NSM_Down:
    case NSM_Attempt:
    case NSM_TwoWay:
      zlog_warn ("Packet[DD]: Neighbor %s state is %s, packet discarded.",
		 inet_ntoa(nbr->router_id),
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
	      zlog_info ("Packet[DD]: Neighbor %s Negotiation done (Slave).",
	      		 inet_ntoa(nbr->router_id));
	      nbr->dd_seqnum = ntohl (dd->dd_seqnum);
	      
	      /* Reset I/MS */
	      UNSET_FLAG (nbr->dd_flags, (OSPF_DD_FLAG_MS|OSPF_DD_FLAG_I));
	    }
	  else
	    {
	      /* We're Master, ignore the initial DBD from Slave */
	      zlog_info ("Packet[DD]: Neighbor %s: Initial DBD from Slave, "
	      		 "ignoring.", inet_ntoa(nbr->router_id));
	      break;
	    }
	}
      /* Ack from the Slave */
      else if (!IS_SET_DD_MS (dd->flags) && !IS_SET_DD_I (dd->flags) &&
	       ntohl (dd->dd_seqnum) == nbr->dd_seqnum &&
	       IPV4_ADDR_CMP (&nbr->router_id, &oi->ospf->router_id) < 0)
	{
	  zlog_info ("Packet[DD]: Neighbor %s Negotiation done (Master).",
		     inet_ntoa(nbr->router_id));
          /* Reset I, leaving MS */
          UNSET_FLAG (nbr->dd_flags, OSPF_DD_FLAG_I);
	}
      else
	{
	  zlog_warn ("Packet[DD]: Neighbor %s Negotiation fails.",
		     inet_ntoa(nbr->router_id));
	  break;
	}
      
      /* This is where the real Options are saved */
      nbr->options = dd->options;

#ifdef HAVE_OPAQUE_LSA
      if (CHECK_FLAG (oi->ospf->config, OSPF_OPAQUE_CAPABLE))
        {
          if (IS_DEBUG_OSPF_EVENT)
            zlog_debug ("Neighbor[%s] is %sOpaque-capable.",
		       inet_ntoa (nbr->router_id),
		       CHECK_FLAG (nbr->options, OSPF_OPTION_O) ? "" : "NOT ");

          if (! CHECK_FLAG (nbr->options, OSPF_OPTION_O)
          &&  IPV4_ADDR_SAME (&DR (oi), &nbr->address.u.prefix4))
            {
              zlog_warn ("DR-neighbor[%s] is NOT opaque-capable; "
                         "Opaque-LSAs cannot be reliably advertised "
                         "in this network.",
                         inet_ntoa (nbr->router_id));
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
	    zlog_info ("Packet[DD] (Master): Neighbor %s packet duplicated.",
		       inet_ntoa (nbr->router_id));
	  else
	    /* Slave: cause to retransmit the last Database Description. */
	    {
	      zlog_info ("Packet[DD] [Slave]: Neighbor %s packet duplicated.",
			 inet_ntoa (nbr->router_id));
	      ospf_db_desc_resend (nbr);
	    }
	  break;
	}

      /* Otherwise DD packet should be checked. */
      /* Check Master/Slave bit mismatch */
      if (IS_SET_DD_MS (dd->flags) != IS_SET_DD_MS (nbr->last_recv.flags))
	{
	  zlog_warn ("Packet[DD]: Neighbor %s MS-bit mismatch.",
		     inet_ntoa(nbr->router_id));
	  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_SeqNumberMismatch);
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_debug ("Packet[DD]: dd->flags=%d, nbr->dd_flags=%d",
		        dd->flags, nbr->dd_flags);
	  break;
	}

      /* Check initialize bit is set. */
      if (IS_SET_DD_I (dd->flags))
	{
	  zlog_info ("Packet[DD]: Neighbor %s I-bit set.",
		     inet_ntoa(nbr->router_id));
	  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_SeqNumberMismatch);
	  break;
	}

      /* Check DD Options. */
      if (dd->options != nbr->options)
	{
#ifdef ORIGINAL_CODING
	  /* Save the new options for debugging */
	  nbr->options = dd->options;
#endif /* ORIGINAL_CODING */
	  zlog_warn ("Packet[DD]: Neighbor %s options mismatch.",
		     inet_ntoa(nbr->router_id));
	  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_SeqNumberMismatch);
	  break;
	}

      /* Check DD sequence number. */
      if ((IS_SET_DD_MS (nbr->dd_flags) &&
	   ntohl (dd->dd_seqnum) != nbr->dd_seqnum) ||
	  (!IS_SET_DD_MS (nbr->dd_flags) &&
	   ntohl (dd->dd_seqnum) != nbr->dd_seqnum + 1))
	{
	  zlog_warn ("Packet[DD]: Neighbor %s sequence number mismatch.",
		     inet_ntoa(nbr->router_id));
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
	      zlog_info ("Packet[DD]: Neighbor %s duplicated, "
	                 "packet discarded.",
			inet_ntoa(nbr->router_id));
	      break;
	    }
	  else
	    {
	      struct timeval t, now;
	      quagga_gettime (QUAGGA_CLK_MONOTONIC, &now);
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
      zlog_warn ("Packet[DD]: Neighbor %s NSM illegal status %u.",
		 inet_ntoa(nbr->router_id), nbr->state);
      break;
    }
}

#define OSPF_LSA_KEY_SIZE       12 /* type(4) + id(4) + ar(4) */

/* OSPF Link State Request Read -- RFC2328 Section 10.7. */
static void
ospf_ls_req (struct ip *iph, struct ospf_header *ospfh,
	     struct stream *s, struct ospf_interface *oi, u_int16_t size)
{
  struct ospf_neighbor *nbr;
  u_int32_t ls_type;
  struct in_addr ls_id;
  struct in_addr adv_router;
  struct ospf_lsa *find;
  struct list *ls_upd;
  unsigned int length;

  /* Increment statistics. */
  oi->ls_req_in++;

  nbr = ospf_nbr_lookup (oi, iph, ospfh);
  if (nbr == NULL)
    {
      zlog_warn ("Link State Request: Unknown Neighbor %s.",
		 inet_ntoa (ospfh->router_id));
      return;
    }

  /* Add event to thread. */
  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_PacketReceived);

  /* Neighbor State should be Exchange or later. */
  if (nbr->state != NSM_Exchange &&
      nbr->state != NSM_Loading &&
      nbr->state != NSM_Full)
    {
      zlog_warn ("Link State Request received from %s: "
      		 "Neighbor state is %s, packet discarded.",
		 inet_ntoa (ospfh->router_id),
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

      /* Packet overflows MTU size, send immediately. */
      if (length + ntohs (find->data->length) > ospf_packet_max (oi))
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
static struct list *
ospf_ls_upd_list_lsa (struct ospf_neighbor *nbr, struct stream *s,
                      struct ospf_interface *oi, size_t size)
{
  u_int16_t count, sum;
  u_int32_t length;
  struct lsa_header *lsah;
  struct ospf_lsa *lsa;
  struct list *lsas;

  lsas = list_new ();

  count = stream_getl (s);
  size -= OSPF_LS_UPD_MIN_SIZE; /* # LSAs */

  for (; size >= OSPF_LSA_HEADER_SIZE && count > 0;
       size -= length, stream_forward_getp (s, length), count--)
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
      if (! ospf_lsa_checksum_valid (lsah))
	{
	  /* (bug #685) more details in a one-line message make it possible
	   * to identify problem source on the one hand and to have a better
	   * chance to compress repeated messages in syslog on the other */
	  zlog_warn ("Link State Update: LSA checksum error %x/%x, ID=%s from: nbr %s, router ID %s, adv router %s",
		     sum, lsah->checksum, inet_ntoa (lsah->id),
		     inet_ntoa (nbr->src), inet_ntoa (nbr->router_id),
		     inet_ntoa (lsah->adv_router));
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
                zlog_debug ("LSA[Type%d:%s]: We are a stub, don't take this LSA.", lsah->type, inet_ntoa (lsah->id));
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
#endif /* HAVE_OPAQUE_LSA */
          lsa->area = NULL;
          break;
#ifdef HAVE_OPAQUE_LSA
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
	zlog_debug("LSA[Type%d:%s]: %p new LSA created with Link State Update",
		  lsa->data->type, inet_ntoa (lsa->data->id), lsa);
      listnode_add (lsas, lsa);
    }

  return lsas;
}

/* Cleanup Update list. */
static void
ospf_upd_list_clean (struct list *lsas)
{
  struct listnode *node, *nnode;
  struct ospf_lsa *lsa;

  for (ALL_LIST_ELEMENTS (lsas, node, nnode, lsa))
    ospf_lsa_discard (lsa);

  list_delete (lsas);
}

/* OSPF Link State Update message read -- RFC2328 Section 13. */
static void
ospf_ls_upd (struct ip *iph, struct ospf_header *ospfh,
	     struct stream *s, struct ospf_interface *oi, u_int16_t size)
{
  struct ospf_neighbor *nbr;
  struct list *lsas;
  struct listnode *node, *nnode;
  struct ospf_lsa *lsa = NULL;
  /* unsigned long ls_req_found = 0; */

  /* Dis-assemble the stream, update each entry, re-encapsulate for flooding */

  /* Increment statistics. */
  oi->ls_upd_in++;

  /* Check neighbor. */
  nbr = ospf_nbr_lookup (oi, iph, ospfh);
  if (nbr == NULL)
    {
      zlog_warn ("Link State Update: Unknown Neighbor %s on int: %s",
		 inet_ntoa (ospfh->router_id), IF_NAME (oi));
      return;
    }

  /* Add event to thread. */
  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_PacketReceived);

  /* Check neighbor state. */
  if (nbr->state < NSM_Exchange)
    {
      zlog_warn ("Link State Update: "
      		 "Neighbor[%s] state %s is less than Exchange",
		 inet_ntoa (ospfh->router_id),
		 LOOKUP(ospf_nsm_state_msg, nbr->state));
      return;
    }

  /* Get list of LSAs from Link State Update packet. - Also perorms Stages 
   * 1 (validate LSA checksum) and 2 (check for LSA consistent type) 
   * of section 13. 
   */
  lsas = ospf_ls_upd_list_lsa (nbr, s, oi, size);

#ifdef HAVE_OPAQUE_LSA
  /*
   * If self-originated Opaque-LSAs that have flooded before restart
   * are contained in the received LSUpd message, corresponding LSReq
   * messages to be sent may have to be modified.
   * To eliminate possible race conditions such that flushing and normal
   * updating for the same LSA would take place alternately, this trick
   * must be done before entering to the loop below.
   */
   /* XXX: Why is this Opaque specific? Either our core code is deficient
    * and this should be fixed generally, or Opaque is inventing strawman
    * problems */
   ospf_opaque_adjust_lsreq (nbr, lsas);
#endif /* HAVE_OPAQUE_LSA */

#define DISCARD_LSA(L,N) {\
        if (IS_DEBUG_OSPF_EVENT) \
          zlog_debug ("ospf_lsa_discard() in ospf_ls_upd() point %d: lsa %p Type-%d", N, lsa, (int) lsa->data->type); \
        ospf_lsa_discard (L); \
	continue; }

  /* Process each LSA received in the one packet.
   *
   * Numbers in parentheses, e.g. (1), (2), etc., and the corresponding
   * text below are from the steps in RFC 2328, Section 13.
   */
  for (ALL_LIST_ELEMENTS (lsas, node, nnode, lsa))
    {
      struct ospf_lsa *ls_ret, *current;
      int ret = 1;

      if (IS_DEBUG_OSPF_NSSA)
	{
	  char buf1[INET_ADDRSTRLEN];
	  char buf2[INET_ADDRSTRLEN];
	  char buf3[INET_ADDRSTRLEN];

	  zlog_debug("LSA Type-%d from %s, ID: %s, ADV: %s",
		  lsa->data->type,
		  inet_ntop (AF_INET, &ospfh->router_id,
			     buf1, INET_ADDRSTRLEN),
		  inet_ntop (AF_INET, &lsa->data->id,
			     buf2, INET_ADDRSTRLEN),
		  inet_ntop (AF_INET, &lsa->data->adv_router,
			     buf3, INET_ADDRSTRLEN));
	}

      listnode_delete (lsas, lsa); /* We don't need it in list anymore */

      /* (1) Validate Checksum - Done above by ospf_ls_upd_list_lsa() */

      /* (2) LSA Type  - Done above by ospf_ls_upd_list_lsa() */
   
      /* (3) Do not take in AS External LSAs if we are a stub or NSSA. */

      /* Do not take in AS NSSA if this neighbor and we are not NSSA */

      /* Do take in Type-7's if we are an NSSA  */ 
 
      /* If we are also an ABR, later translate them to a Type-5 packet */
 
      /* Later, an NSSA Re-fresh can Re-fresh Type-7's and an ABR will
	 translate them to a separate Type-5 packet.  */

      if (lsa->data->type == OSPF_AS_EXTERNAL_LSA)
        /* Reject from STUB or NSSA */
        if (nbr->oi->area->external_routing != OSPF_AREA_DEFAULT) 
	  {
	    if (IS_DEBUG_OSPF_NSSA)
	      zlog_debug("Incoming External LSA Discarded: We are NSSA/STUB Area");
	    DISCARD_LSA (lsa, 1);
	  }

      if (lsa->data->type == OSPF_AS_NSSA_LSA)
	if (nbr->oi->area->external_routing != OSPF_AREA_NSSA)
	  {
	    if (IS_DEBUG_OSPF_NSSA)
	      zlog_debug("Incoming NSSA LSA Discarded:  Not NSSA Area");
	    DISCARD_LSA (lsa,2);
	  }

      /* VU229804: Router-LSA Adv-ID must be equal to LS-ID */
      if (lsa->data->type == OSPF_ROUTER_LSA)
	if (!IPV4_ADDR_SAME(&lsa->data->id, &lsa->data->adv_router))
	  {
	    char buf1[INET_ADDRSTRLEN];
	    char buf2[INET_ADDRSTRLEN];
	    char buf3[INET_ADDRSTRLEN];

	    zlog_err("Incoming Router-LSA from %s with "
		      "Adv-ID[%s] != LS-ID[%s]",
		      inet_ntop (AF_INET, &ospfh->router_id,
				 buf1, INET_ADDRSTRLEN),
		      inet_ntop (AF_INET, &lsa->data->id,
				 buf2, INET_ADDRSTRLEN),
		      inet_ntop (AF_INET, &lsa->data->adv_router,
				 buf3, INET_ADDRSTRLEN));
	    zlog_err("OSPF domain compromised by attack or corruption. "
		     "Verify correct operation of -ALL- OSPF routers.");
	    DISCARD_LSA (lsa, 0);
	  }

      /* Find the LSA in the current database. */

      current = ospf_lsa_lookup_by_header (oi->area, lsa->data);

      /* (4) If the LSA's LS age is equal to MaxAge, and there is currently
	 no instance of the LSA in the router's link state database,
	 and none of router's neighbors are in states Exchange or Loading,
	 then take the following actions: */

      if (IS_LSA_MAXAGE (lsa) && !current &&
	  ospf_check_nbr_status(oi->ospf))
	{
	  /* (4a) Response Link State Acknowledgment. */
	  ospf_ls_ack_send (nbr, lsa);

	  /* (4b) Discard LSA. */
          if (IS_DEBUG_OSPF (lsa, LSA))
            {
	       zlog_debug ("Link State Update[%s]: LS age is equal to MaxAge.",
                           dump_lsa_key(lsa));
            }
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
          /* XXX: We should deal with this for *ALL* LSAs, not just opaque */
          if (current == NULL)
            {
              if (IS_DEBUG_OSPF_EVENT)
                zlog_debug ("LSA[%s]: Previously originated Opaque-LSA,"
                            "not found in the LSDB.", dump_lsa_key (lsa));

              SET_FLAG (lsa->flags, OSPF_LSA_SELF);
              
              ospf_opaque_self_originated_lsa_received (nbr, lsa);
              ospf_ls_ack_send (nbr, lsa);
              
              continue;
            }
        }
#endif /* HAVE_OPAQUE_LSA */

      /* It might be happen that received LSA is self-originated network LSA, but
       * router ID is changed. So, we should check if LSA is a network-LSA whose
       * Link State ID is one of the router's own IP interface addresses but whose
       * Advertising Router is not equal to the router's own Router ID
       * According to RFC 2328 12.4.2 and 13.4 this LSA should be flushed.
       */

      if(lsa->data->type == OSPF_NETWORK_LSA)
      {
        struct listnode *oinode, *oinnode;
        struct ospf_interface *out_if;
        int Flag = 0;

        for (ALL_LIST_ELEMENTS (oi->ospf->oiflist, oinode, oinnode, out_if))
        {
          if(out_if == NULL)
            break;

          if((IPV4_ADDR_SAME(&out_if->address->u.prefix4, &lsa->data->id)) &&
              (!(IPV4_ADDR_SAME(&oi->ospf->router_id, &lsa->data->adv_router))))
          {
            if(out_if->network_lsa_self)
            {
              ospf_lsa_flush_area(lsa,out_if->area);
              if(IS_DEBUG_OSPF_EVENT)
                zlog_debug ("ospf_lsa_discard() in ospf_ls_upd() point 9: lsa %p Type-%d",
                            lsa, (int) lsa->data->type);
              ospf_lsa_discard (lsa);
              Flag = 1;
            }
            break;
          }
        }
        if(Flag)
          continue;
      }

      /* (5) Find the instance of this LSA that is currently contained
	 in the router's link state database.  If there is no
	 database copy, or the received LSA is more recent than
	 the database copy the following steps must be performed.
         (The sub steps from RFC 2328 section 13 step (5) will be performed in
         ospf_flood() ) */

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
	  zlog_warn("LSA[%s] instance exists on Link state request list",
	  	    dump_lsa_key(lsa));

	  /* Clean list of LSAs. */
          ospf_upd_list_clean (lsas);
	  /* this lsa is not on lsas list already. */
	  ospf_lsa_discard (lsa);
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
	      
	      quagga_gettime (QUAGGA_CLK_MONOTONIC, &now);
	      
	      if (tv_cmp (tv_sub (now, current->tv_orig), 
			  int2tv (OSPF_MIN_LS_ARRIVAL)) >= 0)
		/* Trap NSSA type later.*/
		ospf_ls_upd_send_lsa (nbr, current, OSPF_SEND_PACKET_DIRECT);
	      DISCARD_LSA (lsa, 8);
	    }
	}
    }
#undef DISCARD_LSA

  assert (listcount (lsas) == 0);
  list_delete (lsas);
}

/* OSPF Link State Acknowledgment message read -- RFC2328 Section 13.7. */
static void
ospf_ls_ack (struct ip *iph, struct ospf_header *ospfh,
	     struct stream *s, struct ospf_interface *oi, u_int16_t size)
{
  struct ospf_neighbor *nbr;
  
  /* increment statistics. */
  oi->ls_ack_in++;

  nbr = ospf_nbr_lookup (oi, iph, ospfh);
  if (nbr == NULL)
    {
      zlog_warn ("Link State Acknowledgment: Unknown Neighbor %s.",
		 inet_ntoa (ospfh->router_id));
      return;
    }

  /* Add event to thread. */
  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_PacketReceived);

  if (nbr->state < NSM_Exchange)
    {
      zlog_warn ("Link State Acknowledgment: "
      		 "Neighbor[%s] state %s is less than Exchange",
		 inet_ntoa (ospfh->router_id),
		 LOOKUP(ospf_nsm_state_msg, nbr->state));
      return;
    }
  
  while (size >= OSPF_LSA_HEADER_SIZE)
    {
      struct ospf_lsa *lsa, *lsr;

      lsa = ospf_lsa_new ();
      lsa->data = (struct lsa_header *) STREAM_PNT (s);

      /* lsah = (struct lsa_header *) STREAM_PNT (s); */
      size -= OSPF_LSA_HEADER_SIZE;
      stream_forward_getp (s, OSPF_LSA_HEADER_SIZE);

      if (lsa->data->type < OSPF_MIN_LSA || lsa->data->type >= OSPF_MAX_LSA)
	{
	  lsa->data = NULL;
	  ospf_lsa_discard (lsa);
	  continue;
	}

      lsr = ospf_ls_retransmit_lookup (nbr, lsa);

      if (lsr != NULL && ospf_lsa_more_recent (lsr, lsa) == 0)
        {
#ifdef HAVE_OPAQUE_LSA
          if (IS_OPAQUE_LSA (lsr->data->type))
            ospf_opaque_ls_ack_received (nbr, lsr);
#endif /* HAVE_OPAQUE_LSA */

          ospf_ls_retransmit_delete (nbr, lsr);
        }

      lsa->data = NULL;
      ospf_lsa_discard (lsa);
    }

  return;
}

static struct stream *
ospf_recv_packet (int fd, struct interface **ifp, struct stream *ibuf)
{
  int ret;
  struct ip *iph;
  u_int16_t ip_len;
  unsigned int ifindex = 0;
  struct iovec iov;
  /* Header and data both require alignment. */
  char buff [CMSG_SPACE(SOPT_SIZE_CMSG_IFINDEX_IPV4())];
  struct msghdr msgh;

  memset (&msgh, 0, sizeof (struct msghdr));
  msgh.msg_iov = &iov;
  msgh.msg_iovlen = 1;
  msgh.msg_control = (caddr_t) buff;
  msgh.msg_controllen = sizeof (buff);
  
  ret = stream_recvmsg (ibuf, fd, &msgh, 0, OSPF_MAX_PACKET_SIZE+1);
  if (ret < 0)
    {
      zlog_warn("stream_recvmsg failed: %s", safe_strerror(errno));
      return NULL;
    }
  if ((unsigned int)ret < sizeof(iph)) /* ret must be > 0 now */
    {
      zlog_warn("ospf_recv_packet: discarding runt packet of length %d "
		"(ip header size is %u)",
		ret, (u_int)sizeof(iph));
      return NULL;
    }
  
  /* Note that there should not be alignment problems with this assignment
     because this is at the beginning of the stream data buffer. */
  iph = (struct ip *) STREAM_DATA(ibuf);
  sockopt_iphdrincl_swab_systoh (iph);
  
  ip_len = iph->ip_len;
  
#if !defined(GNU_LINUX) && (OpenBSD < 200311) && (__FreeBSD_version < 1000000)
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
  ip_len = ip_len + (iph->ip_hl << 2);
#endif
  
#if defined(__DragonFly__)
  /*
   * in DragonFly's raw socket, ip_len/ip_off are read 
   * in network byte order.
   * As OpenBSD < 200311 adjust ip_len to strip IP header size!
   */
  ip_len = ntohs(iph->ip_len) + (iph->ip_hl << 2);
#endif

  ifindex = getsockopt_ifindex (AF_INET, &msgh);
  
  *ifp = if_lookup_by_index (ifindex);

  if (ret != ip_len)
    {
      zlog_warn ("ospf_recv_packet read length mismatch: ip_len is %d, "
       		 "but recvmsg returned %d", ip_len, ret);
      return NULL;
    }
  
  return ibuf;
}

static struct ospf_interface *
ospf_associate_packet_vl (struct ospf *ospf, struct interface *ifp, 
			  struct ip *iph, struct ospf_header *ospfh)
{
  struct ospf_interface *rcv_oi;
  struct ospf_vl_data *vl_data;
  struct ospf_area *vl_area;
  struct listnode *node;

  if (IN_MULTICAST (ntohl (iph->ip_dst.s_addr)) ||
      !OSPF_IS_AREA_BACKBONE (ospfh))
    return NULL;

  /* look for local OSPF interface matching the destination
   * to determine Area ID. We presume therefore the destination address
   * is unique, or at least (for "unnumbered" links), not used in other 
   * areas
   */
  if ((rcv_oi = ospf_if_lookup_by_local_addr (ospf, NULL, 
                                              iph->ip_dst)) == NULL)
    return NULL;

  for (ALL_LIST_ELEMENTS_RO (ospf->vlinks, node, vl_data))
    {
      vl_area = ospf_area_lookup_by_area_id (ospf, vl_data->vl_area_id);
      if (!vl_area)
	continue;
      
      if (OSPF_AREA_SAME (&vl_area, &rcv_oi->area) &&
	  IPV4_ADDR_SAME (&vl_data->vl_peer, &ospfh->router_id))
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_debug ("associating packet with %s",
		       IF_NAME (vl_data->vl_oi));
	  if (! CHECK_FLAG (vl_data->vl_oi->ifp->flags, IFF_UP))
	    {
	      if (IS_DEBUG_OSPF_EVENT)
		zlog_debug ("This VL is not up yet, sorry");
	      return NULL;
	    }
	  
	  return vl_data->vl_oi;
	}
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("couldn't find any VL to associate the packet with");
  
  return NULL;
}

static int
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
static int
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

/* Return 1, if the packet is properly authenticated and checksummed,
   0 otherwise. In particular, check that AuType header field is valid and
   matches the locally configured AuType, and that D.5 requirements are met. */
static int
ospf_check_auth (struct ospf_interface *oi, struct ospf_header *ospfh)
{
  struct crypt_key *ck;
  u_int16_t iface_auth_type;
  u_int16_t pkt_auth_type = ntohs (ospfh->auth_type);

  switch (pkt_auth_type)
  {
  case OSPF_AUTH_NULL: /* RFC2328 D.5.1 */
    if (OSPF_AUTH_NULL != (iface_auth_type = ospf_auth_type (oi)))
    {
      if (IS_DEBUG_OSPF_PACKET (ospfh->type - 1, RECV))
        zlog_warn ("interface %s: auth-type mismatch, local %s, rcvd Null",
                   IF_NAME (oi), LOOKUP (ospf_auth_type_str, iface_auth_type));
      return 0;
    }
    if (! ospf_check_sum (ospfh))
    {
      if (IS_DEBUG_OSPF_PACKET (ospfh->type - 1, RECV))
        zlog_warn ("interface %s: Null auth OK, but checksum error, Router-ID %s",
                   IF_NAME (oi), inet_ntoa (ospfh->router_id));
      return 0;
    }
    return 1;
  case OSPF_AUTH_SIMPLE: /* RFC2328 D.5.2 */
    if (OSPF_AUTH_SIMPLE != (iface_auth_type = ospf_auth_type (oi)))
    {
      if (IS_DEBUG_OSPF_PACKET (ospfh->type - 1, RECV))
        zlog_warn ("interface %s: auth-type mismatch, local %s, rcvd Simple",
                   IF_NAME (oi), LOOKUP (ospf_auth_type_str, iface_auth_type));
      return 0;
    }
    if (memcmp (OSPF_IF_PARAM (oi, auth_simple), ospfh->u.auth_data, OSPF_AUTH_SIMPLE_SIZE))
    {
      if (IS_DEBUG_OSPF_PACKET (ospfh->type - 1, RECV))
        zlog_warn ("interface %s: Simple auth failed", IF_NAME (oi));
      return 0;
    }
    if (! ospf_check_sum (ospfh))
    {
      if (IS_DEBUG_OSPF_PACKET (ospfh->type - 1, RECV))
        zlog_warn ("interface %s: Simple auth OK, checksum error, Router-ID %s",
                   IF_NAME (oi), inet_ntoa (ospfh->router_id));
      return 0;
    }
    return 1;
  case OSPF_AUTH_CRYPTOGRAPHIC: /* RFC2328 D.5.3 */
    if (OSPF_AUTH_CRYPTOGRAPHIC != (iface_auth_type = ospf_auth_type (oi)))
    {
      if (IS_DEBUG_OSPF_PACKET (ospfh->type - 1, RECV))
        zlog_warn ("interface %s: auth-type mismatch, local %s, rcvd Cryptographic",
                   IF_NAME (oi), LOOKUP (ospf_auth_type_str, iface_auth_type));
      return 0;
    }
    if (ospfh->checksum)
    {
      if (IS_DEBUG_OSPF_PACKET (ospfh->type - 1, RECV))
        zlog_warn ("interface %s: OSPF header checksum is not 0", IF_NAME (oi));
      return 0;
    }
    /* only MD5 crypto method can pass ospf_packet_examin() */
    if
    (
      NULL == (ck = listgetdata (listtail(OSPF_IF_PARAM (oi,auth_crypt)))) ||
      ospfh->u.crypt.key_id != ck->key_id ||
      /* Condition above uses the last key ID on the list, which is
         different from what ospf_crypt_key_lookup() does. A bug? */
      ! ospf_check_md5_digest (oi, ospfh)
    )
    {
      if (IS_DEBUG_OSPF_PACKET (ospfh->type - 1, RECV))
        zlog_warn ("interface %s: MD5 auth failed", IF_NAME (oi));
      return 0;
    }
    return 1;
  default:
    if (IS_DEBUG_OSPF_PACKET (ospfh->type - 1, RECV))
      zlog_warn ("interface %s: invalid packet auth-type (%02x)",
                 IF_NAME (oi), pkt_auth_type);
    return 0;
  }
}

static int
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

/* Verify, that given link/TOS records are properly sized/aligned and match
   Router-LSA "# links" and "# TOS" fields as specified in RFC2328 A.4.2. */
static unsigned
ospf_router_lsa_links_examin
(
  struct router_lsa_link * link,
  u_int16_t linkbytes,
  const u_int16_t num_links
)
{
  unsigned counted_links = 0, thislinklen;

  while (linkbytes)
  {
    thislinklen = OSPF_ROUTER_LSA_LINK_SIZE + 4 * link->m[0].tos_count;
    if (thislinklen > linkbytes)
    {
      if (IS_DEBUG_OSPF_PACKET (0, RECV))
        zlog_debug ("%s: length error in link block #%u", __func__, counted_links);
      return MSG_NG;
    }
    link = (struct router_lsa_link *)((caddr_t) link + thislinklen);
    linkbytes -= thislinklen;
    counted_links++;
  }
  if (counted_links != num_links)
  {
    if (IS_DEBUG_OSPF_PACKET (0, RECV))
      zlog_debug ("%s: %u link blocks declared, %u present",
                  __func__, num_links, counted_links);
    return MSG_NG;
  }
  return MSG_OK;
}

/* Verify, that the given LSA is properly sized/aligned (including type-specific
   minimum length constraint). */
static unsigned
ospf_lsa_examin (struct lsa_header * lsah, const u_int16_t lsalen, const u_char headeronly)
{
  unsigned ret;
  struct router_lsa * rlsa;
  if
  (
    lsah->type < OSPF_MAX_LSA &&
    ospf_lsa_minlen[lsah->type] &&
    lsalen < OSPF_LSA_HEADER_SIZE + ospf_lsa_minlen[lsah->type]
  )
  {
    if (IS_DEBUG_OSPF_PACKET (0, RECV))
      zlog_debug ("%s: undersized (%u B) %s",
                  __func__, lsalen, LOOKUP (ospf_lsa_type_msg, lsah->type));
    return MSG_NG;
  }
  switch (lsah->type)
  {
  case OSPF_ROUTER_LSA:
    /* RFC2328 A.4.2, LSA header + 4 bytes followed by N>=1 (12+)-byte link blocks */
    if (headeronly)
    {
      ret = (lsalen - OSPF_LSA_HEADER_SIZE - OSPF_ROUTER_LSA_MIN_SIZE) % 4 ? MSG_NG : MSG_OK;
      break;
    }
    rlsa = (struct router_lsa *) lsah;
    ret = ospf_router_lsa_links_examin
    (
      (struct router_lsa_link *) rlsa->link,
      lsalen - OSPF_LSA_HEADER_SIZE - 4, /* skip: basic header, "flags", 0, "# links" */
      ntohs (rlsa->links) /* 16 bits */
    );
    break;
  case OSPF_AS_EXTERNAL_LSA:
    /* RFC2328 A.4.5, LSA header + 4 bytes followed by N>=1 12-bytes long blocks */
  case OSPF_AS_NSSA_LSA:
    /* RFC3101 C, idem */
    ret = (lsalen - OSPF_LSA_HEADER_SIZE - OSPF_AS_EXTERNAL_LSA_MIN_SIZE) % 12 ? MSG_NG : MSG_OK;
    break;
  /* Following LSA types are considered OK length-wise as soon as their minimum
   * length constraint is met and length of the whole LSA is a multiple of 4
   * (basic LSA header size is already a multiple of 4). */
  case OSPF_NETWORK_LSA:
    /* RFC2328 A.4.3, LSA header + 4 bytes followed by N>=1 router-IDs */
  case OSPF_SUMMARY_LSA:
  case OSPF_ASBR_SUMMARY_LSA:
    /* RFC2328 A.4.4, LSA header + 4 bytes followed by N>=1 4-bytes TOS blocks */
#ifdef HAVE_OPAQUE_LSA
  case OSPF_OPAQUE_LINK_LSA:
  case OSPF_OPAQUE_AREA_LSA:
  case OSPF_OPAQUE_AS_LSA:
    /* RFC5250 A.2, "some number of octets (of application-specific
     * data) padded to 32-bit alignment." This is considered equivalent
     * to 4-byte alignment of all other LSA types, see OSPF-ALIGNMENT.txt
     * file for the detailed analysis of this passage. */
#endif
    ret = lsalen % 4 ? MSG_NG : MSG_OK;
    break;
  default:
    if (IS_DEBUG_OSPF_PACKET (0, RECV))
      zlog_debug ("%s: unsupported LSA type 0x%02x", __func__, lsah->type);
    return MSG_NG;
  }
  if (ret != MSG_OK && IS_DEBUG_OSPF_PACKET (0, RECV))
    zlog_debug ("%s: alignment error in %s",
                __func__, LOOKUP (ospf_lsa_type_msg, lsah->type));
  return ret;
}

/* Verify if the provided input buffer is a valid sequence of LSAs. This
   includes verification of LSA blocks length/alignment and dispatching
   of deeper-level checks. */
static unsigned
ospf_lsaseq_examin
(
  struct lsa_header *lsah, /* start of buffered data */
  size_t length,
  const u_char headeronly,
  /* When declared_num_lsas is not 0, compare it to the real number of LSAs
     and treat the difference as an error. */
  const u_int32_t declared_num_lsas
)
{
  u_int32_t counted_lsas = 0;

  while (length)
  {
    u_int16_t lsalen;
    if (length < OSPF_LSA_HEADER_SIZE)
    {
      if (IS_DEBUG_OSPF_PACKET (0, RECV))
        zlog_debug ("%s: undersized (%zu B) trailing (#%u) LSA header",
                    __func__, length, counted_lsas);
      return MSG_NG;
    }
    /* save on ntohs() calls here and in the LSA validator */
    lsalen = ntohs (lsah->length);
    if (lsalen < OSPF_LSA_HEADER_SIZE)
    {
      if (IS_DEBUG_OSPF_PACKET (0, RECV))
        zlog_debug ("%s: malformed LSA header #%u, declared length is %u B",
                    __func__, counted_lsas, lsalen);
      return MSG_NG;
    }
    if (headeronly)
    {
      /* less checks here and in ospf_lsa_examin() */
      if (MSG_OK != ospf_lsa_examin (lsah, lsalen, 1))
      {
        if (IS_DEBUG_OSPF_PACKET (0, RECV))
          zlog_debug ("%s: malformed header-only LSA #%u", __func__, counted_lsas);
        return MSG_NG;
      }
      lsah = (struct lsa_header *) ((caddr_t) lsah + OSPF_LSA_HEADER_SIZE);
      length -= OSPF_LSA_HEADER_SIZE;
    }
    else
    {
      /* make sure the input buffer is deep enough before further checks */
      if (lsalen > length)
      {
        if (IS_DEBUG_OSPF_PACKET (0, RECV))
          zlog_debug ("%s: anomaly in LSA #%u: declared length is %u B, buffered length is %zu B",
                      __func__, counted_lsas, lsalen, length);
        return MSG_NG;
      }
      if (MSG_OK != ospf_lsa_examin (lsah, lsalen, 0))
      {
        if (IS_DEBUG_OSPF_PACKET (0, RECV))
          zlog_debug ("%s: malformed LSA #%u", __func__, counted_lsas);
        return MSG_NG;
      }
      lsah = (struct lsa_header *) ((caddr_t) lsah + lsalen);
      length -= lsalen;
    }
    counted_lsas++;
  }

  if (declared_num_lsas && counted_lsas != declared_num_lsas)
  {
    if (IS_DEBUG_OSPF_PACKET (0, RECV))
      zlog_debug ("%s: #LSAs declared (%u) does not match actual (%u)",
                  __func__, declared_num_lsas, counted_lsas);
    return MSG_NG;
  }
  return MSG_OK;
}

/* Verify a complete OSPF packet for proper sizing/alignment. */
static unsigned
ospf_packet_examin (struct ospf_header * oh, const unsigned bytesonwire)
{
  u_int16_t bytesdeclared, bytesauth;
  unsigned ret;
  struct ospf_ls_update * lsupd;

  /* Length, 1st approximation. */
  if (bytesonwire < OSPF_HEADER_SIZE)
  {
    if (IS_DEBUG_OSPF_PACKET (0, RECV))
      zlog_debug ("%s: undersized (%u B) packet", __func__, bytesonwire);
    return MSG_NG;
  }
  /* Now it is safe to access header fields. Performing length check, allow
   * for possible extra bytes of crypto auth/padding, which are not counted
   * in the OSPF header "length" field. */
  if (oh->version != OSPF_VERSION)
  {
    if (IS_DEBUG_OSPF_PACKET (0, RECV))
      zlog_debug ("%s: invalid (%u) protocol version", __func__, oh->version);
    return MSG_NG;
  }
  bytesdeclared = ntohs (oh->length);
  if (ntohs (oh->auth_type) != OSPF_AUTH_CRYPTOGRAPHIC)
    bytesauth = 0;
  else
  {
    if (oh->u.crypt.auth_data_len != OSPF_AUTH_MD5_SIZE)
    {
      if (IS_DEBUG_OSPF_PACKET (0, RECV))
        zlog_debug ("%s: unsupported crypto auth length (%u B)",
                    __func__, oh->u.crypt.auth_data_len);
      return MSG_NG;
    }
    bytesauth = OSPF_AUTH_MD5_SIZE;
  }
  if (bytesdeclared + bytesauth > bytesonwire)
  {
    if (IS_DEBUG_OSPF_PACKET (0, RECV))
      zlog_debug ("%s: packet length error (%u real, %u+%u declared)",
                  __func__, bytesonwire, bytesdeclared, bytesauth);
    return MSG_NG;
  }
  /* Length, 2nd approximation. The type-specific constraint is checked
     against declared length, not amount of bytes on wire. */
  if
  (
    oh->type >= OSPF_MSG_HELLO &&
    oh->type <= OSPF_MSG_LS_ACK &&
    bytesdeclared < OSPF_HEADER_SIZE + ospf_packet_minlen[oh->type]
  )
  {
    if (IS_DEBUG_OSPF_PACKET (0, RECV))
      zlog_debug ("%s: undersized (%u B) %s packet", __func__,
                  bytesdeclared, LOOKUP (ospf_packet_type_str, oh->type));
    return MSG_NG;
  }
  switch (oh->type)
  {
  case OSPF_MSG_HELLO:
    /* RFC2328 A.3.2, packet header + OSPF_HELLO_MIN_SIZE bytes followed
       by N>=0 router-IDs. */
    ret = (bytesdeclared - OSPF_HEADER_SIZE - OSPF_HELLO_MIN_SIZE) % 4 ? MSG_NG : MSG_OK;
    break;
  case OSPF_MSG_DB_DESC:
    /* RFC2328 A.3.3, packet header + OSPF_DB_DESC_MIN_SIZE bytes followed
       by N>=0 header-only LSAs. */
    ret = ospf_lsaseq_examin
    (
      (struct lsa_header *) ((caddr_t) oh + OSPF_HEADER_SIZE + OSPF_DB_DESC_MIN_SIZE),
      bytesdeclared - OSPF_HEADER_SIZE - OSPF_DB_DESC_MIN_SIZE,
      1, /* header-only LSAs */
      0
    );
    break;
  case OSPF_MSG_LS_REQ:
    /* RFC2328 A.3.4, packet header followed by N>=0 12-bytes request blocks. */
    ret = (bytesdeclared - OSPF_HEADER_SIZE - OSPF_LS_REQ_MIN_SIZE) %
      OSPF_LSA_KEY_SIZE ? MSG_NG : MSG_OK;
    break;
  case OSPF_MSG_LS_UPD:
    /* RFC2328 A.3.5, packet header + OSPF_LS_UPD_MIN_SIZE bytes followed
       by N>=0 full LSAs (with N declared beforehand). */
    lsupd = (struct ospf_ls_update *) ((caddr_t) oh + OSPF_HEADER_SIZE);
    ret = ospf_lsaseq_examin
    (
      (struct lsa_header *) ((caddr_t) lsupd + OSPF_LS_UPD_MIN_SIZE),
      bytesdeclared - OSPF_HEADER_SIZE - OSPF_LS_UPD_MIN_SIZE,
      0, /* full LSAs */
      ntohl (lsupd->num_lsas) /* 32 bits */
    );
    break;
  case OSPF_MSG_LS_ACK:
    /* RFC2328 A.3.6, packet header followed by N>=0 header-only LSAs. */
    ret = ospf_lsaseq_examin
    (
      (struct lsa_header *) ((caddr_t) oh + OSPF_HEADER_SIZE + OSPF_LS_ACK_MIN_SIZE),
      bytesdeclared - OSPF_HEADER_SIZE - OSPF_LS_ACK_MIN_SIZE,
      1, /* header-only LSAs */
      0
    );
    break;
  default:
    if (IS_DEBUG_OSPF_PACKET (0, RECV))
      zlog_debug ("%s: invalid packet type 0x%02x", __func__, oh->type);
    return MSG_NG;
  }
  if (ret != MSG_OK && IS_DEBUG_OSPF_PACKET (0, RECV))
    zlog_debug ("%s: malformed %s packet", __func__, LOOKUP (ospf_packet_type_str, oh->type));
  return ret;
}

/* OSPF Header verification. */
static int
ospf_verify_header (struct stream *ibuf, struct ospf_interface *oi,
		    struct ip *iph, struct ospf_header *ospfh)
{
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

  /* Check authentication. The function handles logging actions, where required. */
  if (! ospf_check_auth (oi, ospfh))
    return -1;

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

  /* prepare for next packet. */
  ospf->t_read = thread_add_read (master, ospf_read, ospf, ospf->fd);

  stream_reset(ospf->ibuf);
  if (!(ibuf = ospf_recv_packet (ospf->fd, &ifp, ospf->ibuf)))
    return -1;
  /* This raw packet is known to be at least as big as its IP header. */
  
  /* Note that there should not be alignment problems with this assignment
     because this is at the beginning of the stream data buffer. */
  iph = (struct ip *) STREAM_DATA (ibuf);
  /* Note that sockopt_iphdrincl_swab_systoh was called in ospf_recv_packet. */

  if (ifp == NULL)
    /* Handle cases where the platform does not support retrieving the ifindex,
       and also platforms (such as Solaris 8) that claim to support ifindex
       retrieval but do not. */
    ifp = if_lookup_address (iph->ip_src);
  
  if (ifp == NULL)
    return 0;

  /* IP Header dump. */
    if (IS_DEBUG_OSPF_PACKET(0, RECV))
	    ospf_ip_header_dump (iph);

  /* Self-originated packet should be discarded silently. */
  if (ospf_if_lookup_by_local_addr (ospf, NULL, iph->ip_src))
    {
      if (IS_DEBUG_OSPF_PACKET (0, RECV))
        {
          zlog_debug ("ospf_read[%s]: Dropping self-originated packet",
                     inet_ntoa (iph->ip_src));
        }
      return 0;
    }

  /* Advance from IP header to OSPF header (iph->ip_hl has been verified
     by ospf_recv_packet() to be correct). */
  stream_forward_getp (ibuf, iph->ip_hl * 4);

  ospfh = (struct ospf_header *) STREAM_PNT (ibuf);
  if (MSG_OK != ospf_packet_examin (ospfh, stream_get_endp (ibuf) - stream_get_getp (ibuf)))
    return -1;
  /* Now it is safe to access all fields of OSPF packet header. */

  /* associate packet with ospf interface */
  oi = ospf_if_lookup_recv_if (ospf, iph->ip_src, ifp);

  /* ospf_verify_header() relies on a valid "oi" and thus can be called only
     after the passive/backbone/other checks below are passed. These checks
     in turn access the fields of unverified "ospfh" structure for their own
     purposes and must remain very accurate in doing this. */

  /* If incoming interface is passive one, ignore it. */
  if (oi && OSPF_IF_PASSIVE_STATUS (oi) == OSPF_IF_PASSIVE)
    {
      char buf[3][INET_ADDRSTRLEN];

      if (IS_DEBUG_OSPF_EVENT)
	zlog_debug ("ignoring packet from router %s sent to %s, "
		    "received on a passive interface, %s",
		    inet_ntop(AF_INET, &ospfh->router_id, buf[0], sizeof(buf[0])),
		    inet_ntop(AF_INET, &iph->ip_dst, buf[1], sizeof(buf[1])),
		    inet_ntop(AF_INET, &oi->address->u.prefix4,
			      buf[2], sizeof(buf[2])));

      if (iph->ip_dst.s_addr == htonl(OSPF_ALLSPFROUTERS))
	{
	  /* Try to fix multicast membership.
	   * Some OS:es may have problems in this area,
	   * make sure it is removed.
	   */
	  OI_MEMBER_JOINED(oi, MEMBER_ALLROUTERS);
	  ospf_if_set_multicast(oi);
	}
      return 0;
  }


  /* if no local ospf_interface, 
   * or header area is backbone but ospf_interface is not
   * check for VLINK interface
   */
  if ( (oi == NULL) ||
      (OSPF_IS_AREA_ID_BACKBONE(ospfh->area_id)
      && !OSPF_IS_AREA_ID_BACKBONE(oi->area->area_id))
     )
    {
      if ((oi = ospf_associate_packet_vl (ospf, ifp, iph, ospfh)) == NULL)
        {
          if (IS_DEBUG_OSPF_EVENT)
            zlog_debug ("Packet from [%s] received on link %s"
                        " but no ospf_interface",
                        inet_ntoa (iph->ip_src), ifp->name);
          return 0;
        }
    }

  /* else it must be a local ospf interface, check it was received on 
   * correct link 
   */
  else if (oi->ifp != ifp)
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_warn ("Packet from [%s] received on wrong link %s",
                   inet_ntoa (iph->ip_src), ifp->name); 
      return 0;
    }
  else if (oi->state == ISM_Down)
    {
      char buf[2][INET_ADDRSTRLEN];
      zlog_warn ("Ignoring packet from %s to %s received on interface that is "
      		 "down [%s]; interface flags are %s",
		 inet_ntop(AF_INET, &iph->ip_src, buf[0], sizeof(buf[0])),
		 inet_ntop(AF_INET, &iph->ip_dst, buf[1], sizeof(buf[1])),
	         ifp->name, if_flag_dump(ifp->flags));
      /* Fix multicast memberships? */
      if (iph->ip_dst.s_addr == htonl(OSPF_ALLSPFROUTERS))
        OI_MEMBER_JOINED(oi, MEMBER_ALLROUTERS);
      else if (iph->ip_dst.s_addr == htonl(OSPF_ALLDROUTERS))
	OI_MEMBER_JOINED(oi, MEMBER_DROUTERS);
      if (oi->multicast_memberships)
	ospf_if_set_multicast(oi);
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
      zlog_warn ("Dropping packet for AllDRouters from [%s] via [%s] (ISM: %s)",
                 inet_ntoa (iph->ip_src), IF_NAME (oi),
                 LOOKUP (ospf_ism_state_msg, oi->state));
      /* Try to fix multicast membership. */
      SET_FLAG(oi->multicast_memberships, MEMBER_DROUTERS);
      ospf_if_set_multicast(oi);
      return 0;
    }

  /* Verify more OSPF header fields. */
  ret = ospf_verify_header (ibuf, oi, iph, ospfh);
  if (ret < 0)
  {
    if (IS_DEBUG_OSPF_PACKET (0, RECV))
      zlog_debug ("ospf_read[%s]: Header check failed, "
                  "dropping.",
                  inet_ntoa (iph->ip_src));
    return ret;
  }

  /* Show debug receiving packet. */
  if (IS_DEBUG_OSPF_PACKET (ospfh->type - 1, RECV))
    {
      if (IS_DEBUG_OSPF_PACKET (ospfh->type - 1, DETAIL))
        {
          zlog_debug ("-----------------------------------------------------");
          ospf_packet_dump (ibuf);
        }

      zlog_debug ("%s received from [%s] via [%s]",
                 LOOKUP (ospf_packet_type_str, ospfh->type),
                 inet_ntoa (ospfh->router_id), IF_NAME (oi));
      zlog_debug (" src [%s],", inet_ntoa (iph->ip_src));
      zlog_debug (" dst [%s]", inet_ntoa (iph->ip_dst));

      if (IS_DEBUG_OSPF_PACKET (ospfh->type - 1, DETAIL))
	zlog_debug ("-----------------------------------------------------");
  }

  stream_forward_getp (ibuf, OSPF_HEADER_SIZE);

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

  return 0;
}

/* Make OSPF header. */
static void
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

  stream_forward_endp (s, OSPF_HEADER_SIZE);
}

/* Make Authentication Data. */
static int
ospf_make_auth (struct ospf_interface *oi, struct ospf_header *ospfh)
{
  struct crypt_key *ck;

  switch (ospf_auth_type (oi))
    {
    case OSPF_AUTH_NULL:
      /* memset (ospfh->u.auth_data, 0, sizeof (ospfh->u.auth_data)); */
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
	  ck = listgetdata (listtail(OSPF_IF_PARAM (oi, auth_crypt)));
	  ospfh->u.crypt.zero = 0;
	  ospfh->u.crypt.key_id = ck->key_id;
	  ospfh->u.crypt.auth_data_len = OSPF_AUTH_MD5_SIZE;
	}
      /* note: the seq is done in ospf_make_md5_digest() */
      break;
    default:
      /* memset (ospfh->u.auth_data, 0, sizeof (ospfh->u.auth_data)); */
      break;
    }

  return 0;
}

/* Fill rest of OSPF header. */
static void
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

static int
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
  if (OSPF_IF_PARAM (oi, fast_hello) == 0)
    stream_putw (s, OSPF_IF_PARAM (oi, v_hello));
  else
    stream_putw (s, 0); /* hello-interval of 0 for fast-hellos */

  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("make_hello: options: %x, int: %s",
	       OPTIONS(oi), IF_NAME (oi));

  /* Set Options. */
  stream_putc (s, OPTIONS (oi));

  /* Set Router Priority. */
  stream_putc (s, PRIORITY (oi));

  /* Set Router Dead Interval. */
  stream_putl (s, OSPF_IF_PARAM (oi, v_wait));

  /* Set Designated Router. */
  stream_put_ipv4 (s, DR (oi).s_addr);

  p = stream_get_endp (s);

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
    stream_putl_at (s, p, 0); /* ipv4 address, normally */

  return length;
}

static int
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
    SET_FLAG (options, OSPF_OPTION_O);
#endif /* HAVE_OPAQUE_LSA */
  stream_putc (s, options);

  /* DD flags */
  pp = stream_get_endp (s);
  stream_putc (s, nbr->dd_flags);

  /* Set DD Sequence Number. */
  stream_putl (s, nbr->dd_seqnum);

  /* shortcut unneeded walk of (empty) summary LSDBs */
  if (ospf_db_summary_isempty (nbr))
    goto empty;

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
		if (length + OSPF_LSA_HEADER_SIZE > ospf_packet_max (oi))
		  break;
		
		/* Keep pointer to LS age. */
		lsah = (struct lsa_header *) (STREAM_DATA (s) +
					      stream_get_endp (s));
		
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

  /* Update 'More' bit */
  if (ospf_db_summary_isempty (nbr))
    {
empty:
      if (nbr->state >= NSM_Exchange)
        {
          UNSET_FLAG (nbr->dd_flags, OSPF_DD_FLAG_M);
          /* Rewrite DD flags */
          stream_putc_at (s, pp, nbr->dd_flags);
        }
      else
        {
          assert (IS_SET_DD_M(nbr->dd_flags));
        }
    }
  return length;
}

static int
ospf_make_ls_req_func (struct stream *s, u_int16_t *length,
		       unsigned long delta, struct ospf_neighbor *nbr,
		       struct ospf_lsa *lsa)
{
  struct ospf_interface *oi;

  oi = nbr->oi;

  /* LS Request packet overflows interface MTU. */
  if (*length + delta > ospf_packet_max(oi))
    return 0;

  stream_putl (s, lsa->data->type);
  stream_put_ipv4 (s, lsa->data->id.s_addr);
  stream_put_ipv4 (s, lsa->data->adv_router.s_addr);
  
  ospf_lsa_unlock (&nbr->ls_req_last);
  nbr->ls_req_last = ospf_lsa_lock (lsa);
  
  *length += 12;
  return 1;
}

static int
ospf_make_ls_req (struct ospf_neighbor *nbr, struct stream *s)
{
  struct ospf_lsa *lsa;
  u_int16_t length = OSPF_LS_REQ_MIN_SIZE;
  unsigned long delta = stream_get_endp(s)+12;
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

static int
ls_age_increment (struct ospf_lsa *lsa, int delay)
{
  int age;

  age = IS_LSA_MAXAGE (lsa) ? OSPF_LSA_MAXAGE : LS_AGE (lsa) + delay;

  return (age > OSPF_LSA_MAXAGE ? OSPF_LSA_MAXAGE : age);
}

static int
ospf_make_ls_upd (struct ospf_interface *oi, struct list *update, struct stream *s)
{
  struct ospf_lsa *lsa;
  struct listnode *node;
  u_int16_t length = 0;
  unsigned int size_noauth;
  unsigned long delta = stream_get_endp (s);
  unsigned long pp;
  int count = 0;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("ospf_make_ls_upd: Start");

  pp = stream_get_endp (s);
  stream_forward_endp (s, OSPF_LS_UPD_MIN_SIZE);
  length += OSPF_LS_UPD_MIN_SIZE;

  /* Calculate amount of packet usable for data. */
  size_noauth = stream_get_size(s) - ospf_packet_authspace(oi);

  while ((node = listhead (update)) != NULL)
    {
      struct lsa_header *lsah;
      u_int16_t ls_age;

      if (IS_DEBUG_OSPF_EVENT)
        zlog_debug ("ospf_make_ls_upd: List Iteration %d", count);

      lsa = listgetdata (node);

      assert (lsa->data);

      /* Will it fit? */
      if (length + delta + ntohs (lsa->data->length) > size_noauth)
        break;

      /* Keep pointer to LS age. */
      lsah = (struct lsa_header *) (STREAM_DATA (s) + stream_get_endp (s));

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
      ospf_lsa_unlock (&lsa); /* oi->ls_upd_queue */
    }

  /* Now set #LSAs. */
  stream_putl_at (s, pp, count);

  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("ospf_make_ls_upd: Stop");
  return length;
}

static int
ospf_make_ls_ack (struct ospf_interface *oi, struct list *ack, struct stream *s)
{
  struct listnode *node, *nnode;
  u_int16_t length = OSPF_LS_ACK_MIN_SIZE;
  unsigned long delta = stream_get_endp(s) + 24;
  struct ospf_lsa *lsa;

  for (ALL_LIST_ELEMENTS (ack, node, nnode, lsa))
    {
      assert (lsa);
      
      if (length + delta > ospf_packet_max (oi))
	break;
      
      stream_put (s, lsa->data, OSPF_LSA_HEADER_SIZE);
      length += OSPF_LSA_HEADER_SIZE;
      
      listnode_delete (ack, lsa);
      ospf_lsa_unlock (&lsa); /* oi->ls_ack_direct.ls_ack */
    }
  
  return length;
}

static void
ospf_hello_send_sub (struct ospf_interface *oi, in_addr_t addr)
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

  op->dst.s_addr = addr;

  /* Add packet to the top of the interface output queue, so that they
   * can't get delayed by things like long queues of LS Update packets
   */
  ospf_packet_add_top (oi, op);

  /* Hook thread to write packet. */
  OSPF_ISM_WRITE_ON (oi->ospf);
}

static void
ospf_poll_send (struct ospf_nbr_nbma *nbr_nbma)
{
  struct ospf_interface *oi;

  oi = nbr_nbma->oi;
  assert(oi);

  /* If this is passive interface, do not send OSPF Hello. */
  if (OSPF_IF_PASSIVE_STATUS (oi) == OSPF_IF_PASSIVE)
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

  ospf_hello_send_sub (oi, nbr_nbma->addr.s_addr);
}

int
ospf_poll_timer (struct thread *thread)
{
  struct ospf_nbr_nbma *nbr_nbma;

  nbr_nbma = THREAD_ARG (thread);
  nbr_nbma->t_poll = NULL;

  if (IS_DEBUG_OSPF (nsm, NSM_TIMERS))
    zlog (NULL, LOG_DEBUG, "NSM[%s:%s]: Timer (Poll timer expire)",
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
    zlog (NULL, LOG_DEBUG, "NSM[%s:%s]: Timer (hello-reply timer expire)",
	  IF_NAME (nbr->oi), inet_ntoa (nbr->router_id));

  ospf_hello_send_sub (nbr->oi, nbr->address.u.prefix4.s_addr);

  return 0;
}

/* Send OSPF Hello. */
void
ospf_hello_send (struct ospf_interface *oi)
{
  /* If this is passive interface, do not send OSPF Hello. */
  if (OSPF_IF_PASSIVE_STATUS (oi) == OSPF_IF_PASSIVE)
    return;

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
		ospf_hello_send_sub (oi, nbr->address.u.prefix4.s_addr);
	      }
    }
  else
    {
      /* Decide destination address. */
      if (oi->type == OSPF_IFTYPE_VIRTUALLINK)
        ospf_hello_send_sub (oi, oi->vl_data->peer_addr.s_addr);
      else
        ospf_hello_send_sub (oi, htonl (OSPF_ALLSPFROUTERS));
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
  if (oi->type == OSPF_IFTYPE_POINTOPOINT) 
    op->dst.s_addr = htonl (OSPF_ALLSPFROUTERS);
  else
    op->dst = nbr->address.u.prefix4;

  /* Add packet to the interface output queue. */
  ospf_packet_add (oi, op);

  /* Hook thread to write packet. */
  OSPF_ISM_WRITE_ON (oi->ospf);

  /* Remove old DD packet, then copy new one and keep in neighbor structure. */
  if (nbr->last_send)
    ospf_packet_free (nbr->last_send);
  nbr->last_send = ospf_packet_dup (op);
  quagga_gettime (QUAGGA_CLK_MONOTONIC, &nbr->last_send_ts);
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
  if (oi->type == OSPF_IFTYPE_POINTOPOINT) 
    op->dst.s_addr = htonl (OSPF_ALLSPFROUTERS);
  else
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
  struct list *update;

  update = list_new ();

  listnode_add (update, lsa);
  ospf_ls_upd_send (nbr, update, flag);

  list_delete (update);
}

/* Determine size for packet. Must be at least big enough to accomodate next
 * LSA on list, which may be bigger than MTU size.
 *
 * Return pointer to new ospf_packet
 * NULL if we can not allocate, eg because LSA is bigger than imposed limit
 * on packet sizes (in which case offending LSA is deleted from update list)
 */
static struct ospf_packet *
ospf_ls_upd_packet_new (struct list *update, struct ospf_interface *oi)
{
  struct ospf_lsa *lsa;
  struct listnode *ln;
  size_t size;
  static char warned = 0;

  lsa = listgetdata((ln = listhead (update)));
  assert (lsa->data);

  if ((OSPF_LS_UPD_MIN_SIZE + ntohs (lsa->data->length))
      > ospf_packet_max (oi))
    {
      if (!warned)
        {
          zlog_warn ("ospf_ls_upd_packet_new: oversized LSA encountered!"
                     "will need to fragment. Not optimal. Try divide up"
                     " your network with areas. Use 'debug ospf packet send'"
                     " to see details, or look at 'show ip ospf database ..'");
          warned = 1;
        }

      if (IS_DEBUG_OSPF_PACKET (0, SEND))
        zlog_debug ("ospf_ls_upd_packet_new: oversized LSA id:%s,"
                   " %d bytes originated by %s, will be fragmented!",
                   inet_ntoa (lsa->data->id),
                   ntohs (lsa->data->length),
                   inet_ntoa (lsa->data->adv_router));

      /* 
       * Allocate just enough to fit this LSA only, to avoid including other
       * LSAs in fragmented LSA Updates.
       */
      size = ntohs (lsa->data->length) + (oi->ifp->mtu - ospf_packet_max (oi))
             + OSPF_LS_UPD_MIN_SIZE;
    }
  else
    size = oi->ifp->mtu;

  if (size > OSPF_MAX_PACKET_SIZE)
    {
      zlog_warn ("ospf_ls_upd_packet_new: oversized LSA id:%s too big,"
                 " %d bytes, packet size %ld, dropping it completely."
                 " OSPF routing is broken!",
                 inet_ntoa (lsa->data->id), ntohs (lsa->data->length),
                 (long int) size);
      list_delete_node (update, ln);
      return NULL;
    }

  /* IP header is built up separately by ospf_write(). This means, that we must
   * reduce the "affordable" size just calculated by length of an IP header.
   * This makes sure, that even if we manage to fill the payload with LSA data
   * completely, the final packet (our data plus IP header) still fits into
   * outgoing interface MTU. This correction isn't really meaningful for an
   * oversized LSA, but for consistency the correction is done for both cases.
   *
   * P.S. OSPF_MAX_PACKET_SIZE above already includes IP header size
   */
  return ospf_packet_new (size - sizeof (struct ip));
}

static void
ospf_ls_upd_queue_send (struct ospf_interface *oi, struct list *update,
			struct in_addr addr)
{
  struct ospf_packet *op;
  u_int16_t length = OSPF_HEADER_SIZE;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("listcount = %d, [%s]dst %s", listcount (update), IF_NAME(oi),
			    inet_ntoa(addr));
  
  op = ospf_ls_upd_packet_new (update, oi);

  /* Prepare OSPF common header. */
  ospf_make_header (OSPF_MSG_LS_UPD, oi, op->s);

  /* Prepare OSPF Link State Update body.
   * Includes Type-7 translation. 
   */
  length += ospf_make_ls_upd (oi, update, op->s);

  /* Fill OSPF header. */
  ospf_fill_header (oi, op->s, length);

  /* Set packet length. */
  op->length = length;

  /* Decide destination address. */
  if (oi->type == OSPF_IFTYPE_POINTOPOINT) 
    op->dst.s_addr = htonl (OSPF_ALLSPFROUTERS);
  else
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
  struct route_node *rnext;
  struct list *update;
  char again = 0;
  
  oi->t_ls_upd_event = NULL;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("ospf_ls_upd_send_queue start");

  for (rn = route_top (oi->ls_upd_queue); rn; rn = rnext)
    {
      rnext = route_next (rn);
      
      if (rn->info == NULL)
        continue;
      
      update = (struct list *)rn->info;

      ospf_ls_upd_queue_send (oi, update, rn->p.u.prefix4);
      
      /* list might not be empty. */
      if (listcount(update) == 0)
        {
          list_delete (rn->info);
          rn->info = NULL;
          route_unlock_node (rn);
        }
      else
        again = 1;
    }

  if (again != 0)
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_debug ("ospf_ls_upd_send_queue: update lists not cleared,"
                   " %d nodes to try again, raising new event", again);
      oi->t_ls_upd_event = 
        thread_add_event (master, ospf_ls_upd_send_queue_event, oi, 0);
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("ospf_ls_upd_send_queue stop");
  
  return 0;
}

void
ospf_ls_upd_send (struct ospf_neighbor *nbr, struct list *update, int flag)
{
  struct ospf_interface *oi;
  struct ospf_lsa *lsa;
  struct prefix_ipv4 p;
  struct route_node *rn;
  struct listnode *node;
  
  oi = nbr->oi;

  p.family = AF_INET;
  p.prefixlen = IPV4_MAX_BITLEN;
  
  /* Decide destination address. */
  if (oi->type == OSPF_IFTYPE_VIRTUALLINK)
    p.prefix = oi->vl_data->peer_addr;
  else if (oi->type == OSPF_IFTYPE_POINTOPOINT) 
     p.prefix.s_addr = htonl (OSPF_ALLSPFROUTERS);
  else if (flag == OSPF_SEND_PACKET_DIRECT)
     p.prefix = nbr->address.u.prefix4;
  else if (oi->state == ISM_DR || oi->state == ISM_Backup)
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

  for (ALL_LIST_ELEMENTS_RO (update, node, lsa))
    listnode_add (rn->info, ospf_lsa_lock (lsa)); /* oi->ls_upd_queue */

  if (oi->t_ls_upd_event == NULL)
    oi->t_ls_upd_event =
      thread_add_event (master, ospf_ls_upd_send_queue_event, oi, 0);
}

static void
ospf_ls_ack_send_list (struct ospf_interface *oi, struct list *ack,
		       struct in_addr dst)
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
  else if (oi->type == OSPF_IFTYPE_POINTOMULTIPOINT)
    dst.s_addr = htonl (OSPF_ALLSPFROUTERS);
  else
    dst.s_addr = htonl (OSPF_ALLDROUTERS);

  while (listcount (oi->ls_ack))
    ospf_ls_ack_send_list (oi, oi->ls_ack, dst);
}
