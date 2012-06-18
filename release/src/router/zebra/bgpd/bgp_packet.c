/* BGP packet management routine.
   Copyright (C) 1999 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include <zebra.h>

#include "thread.h"
#include "stream.h"
#include "network.h"
#include "prefix.h"
#include "command.h"
#include "log.h"
#include "memory.h"
#include "sockunion.h"		/* for inet_ntop () */
#include "linklist.h"
#include "plist.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_table.h"
#include "bgpd/bgp_dump.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_debug.h"
#include "bgpd/bgp_fsm.h"
#include "bgpd/bgp_route.h"
#include "bgpd/bgp_packet.h"
#include "bgpd/bgp_open.h"
#include "bgpd/bgp_aspath.h"
#include "bgpd/bgp_community.h"
#include "bgpd/bgp_ecommunity.h"
#include "bgpd/bgp_network.h"
#include "bgpd/bgp_mplsvpn.h"
#include "bgpd/bgp_advertise.h"
#include "bgpd/bgp_vty.h"

int stream_put_prefix (struct stream *, struct prefix *);

/* Set up BGP packet marker and packet type. */
static int
bgp_packet_set_marker (struct stream *s, u_char type)
{
  int i;

  /* Fill in marker. */
  for (i = 0; i < BGP_MARKER_SIZE; i++)
    stream_putc (s, 0xff);

  /* Dummy total length. This field is should be filled in later on. */
  stream_putw (s, 0);

  /* BGP packet type. */
  stream_putc (s, type);

  /* Return current stream size. */
  return stream_get_putp (s);
}

/* Set BGP packet header size entry.  If size is zero then use current
   stream size. */
static int
bgp_packet_set_size (struct stream *s)
{
  int cp;

  /* Preserve current pointer. */
  cp = stream_get_putp (s);
  stream_set_putp (s, BGP_MARKER_SIZE);
  stream_putw (s, cp);

  /* Write back current pointer. */
  stream_set_putp (s, cp);

  return cp;
}

/* Add new packet to the peer. */
void
bgp_packet_add (struct peer *peer, struct stream *s)
{
  /* Add packet to the end of list. */
  stream_fifo_push (peer->obuf, s);
}

/* Free first packet. */
void
bgp_packet_delete (struct peer *peer)
{
  stream_free (stream_fifo_pop (peer->obuf));
}

/* Duplicate packet. */
struct stream *
bgp_packet_dup (struct stream *s)
{
  struct stream *new;

  new = stream_new (stream_get_endp (s));

  new->endp = s->endp;
  new->putp = s->putp;
  new->getp = s->getp;

  memcpy (new->data, s->data, stream_get_endp (s));

  return new;
}

/* Check file descriptor whether connect is established. */
static void
bgp_connect_check (struct peer *peer)
{
  int status;
  int slen;
  int ret;

  /* Anyway I have to reset read and write thread. */
  BGP_READ_OFF (peer->t_read);
  BGP_WRITE_OFF (peer->t_write);

  /* Check file descriptor. */
  slen = sizeof (status);
  ret = getsockopt(peer->fd, SOL_SOCKET, SO_ERROR, (void *) &status, &slen);

  /* If getsockopt is fail, this is fatal error. */
  if (ret < 0)
    {
      zlog (peer->log, LOG_INFO, "can't get sockopt for nonblocking connect");
      BGP_EVENT_ADD (peer, TCP_fatal_error);
      return;
    }      

  /* When status is 0 then TCP connection is established. */
  if (status == 0)
    {
      BGP_EVENT_ADD (peer, TCP_connection_open);
    }
  else
    {
      if (BGP_DEBUG (events, EVENTS))
	  plog_info (peer->log, "%s [Event] Connect failed (%s)",
		     peer->host, strerror (errno));
      BGP_EVENT_ADD (peer, TCP_connection_open_failed);
    }
}

/* Make BGP update packet.  */
struct stream *
bgp_update_packet (struct peer *peer, afi_t afi, safi_t safi)
{
  struct stream *s;
  struct bgp_adj_out *adj;
  struct bgp_advertise *adv;
  struct stream *packet;
  struct bgp_node *rn = NULL;
  struct bgp_info *binfo = NULL;
  bgp_size_t total_attr_len = 0;
  unsigned long pos;
  char buf[BUFSIZ];
  struct prefix_rd *prd = NULL;
  char *tag = NULL;

  s = peer->work;
  stream_reset (s);

  adv = FIFO_HEAD (&peer->sync[afi][safi]->update);

  while (adv)
    {
      if (adv->rn)
        rn = adv->rn;
      adj = adv->adj;
      if (adv->binfo)
        binfo = adv->binfo;
#ifdef MPLS_VPN
      if (rn)
        prd = (struct prefix_rd *) &rn->prn->p;
      if (binfo)
        tag = binfo->tag;
#endif /* MPLS_VPN */

      /* When remaining space can't include NLRI and it's length.  */
      if (rn && STREAM_REMAIN (s) <= BGP_NLRI_LENGTH + PSIZE (rn->p.prefixlen))
	break;

      /* If packet is empty, set attribute. */
      if (stream_empty (s))
	{
	  bgp_packet_set_marker (s, BGP_MSG_UPDATE);
	  stream_putw (s, 0);		
	  pos = stream_get_putp (s);
	  stream_putw (s, 0);
	  total_attr_len = bgp_packet_attribute (NULL, peer, s,
	                			 adv->baa->attr,
						 &rn->p, afi, safi,
						 binfo->peer, prd, tag);
	  stream_putw_at (s, pos, total_attr_len);
	}

      if (afi == AFI_IP && safi == SAFI_UNICAST)
	stream_put_prefix (s, &rn->p);
      
      if (BGP_DEBUG (update, UPDATE_OUT))
	zlog (peer->log, LOG_INFO, "%s send UPDATE %s/%d",
	      peer->host,
	      inet_ntop (rn->p.family, &(rn->p.u.prefix), buf, BUFSIZ),
	      rn->p.prefixlen);

      /* Synchnorize attribute.  */
      if (adj->attr)
	bgp_attr_unintern (adj->attr);
      else
	peer->scount[afi][safi]++;

      adj->attr = bgp_attr_intern (adv->baa->attr);

      adv = bgp_advertise_clean (peer, adj, afi, safi);

      if (! (afi == AFI_IP && safi == SAFI_UNICAST))
	break;
    }

  if (! stream_empty (s))
    {
      bgp_packet_set_size (s);
      packet = bgp_packet_dup (s);
      bgp_packet_add (peer, packet);
      stream_reset (s);
      return packet;
    }
  return NULL;
}

struct stream *
bgp_update_packet_eor (struct peer *peer, afi_t afi, safi_t safi)
{
  struct stream *s;
  struct stream *packet;

  if (BGP_DEBUG (normal, NORMAL))
    zlog_info ("Send End-of-RIB for AF %s to neighbor %s",
	       afi_safi_print(afi, safi), peer->host);

  s = stream_new (BGP_MAX_PACKET_SIZE);

  /* Make BGP update packet. */
  bgp_packet_set_marker (s, BGP_MSG_UPDATE);

  /* Unfeasible Routes Length */
  stream_putw (s, 0);

  if (afi == AFI_IP && safi == SAFI_UNICAST)
    {
      /* Total Path Attribute Length */
      stream_putw (s, 0);
    }
  else
    {
      /* Total Path Attribute Length */
      stream_putw (s, 6);
      stream_putc (s, BGP_ATTR_FLAG_OPTIONAL);
      stream_putc (s, BGP_ATTR_MP_UNREACH_NLRI);
      stream_putc (s, 3);
      stream_putw (s, afi);
      stream_putc (s, safi);
    }

  bgp_packet_set_size (s);
  packet = bgp_packet_dup (s);
  bgp_packet_add (peer, packet);
  stream_free (s);
  return packet;
}

/* Make BGP withdraw packet.  */
struct stream *
bgp_withdraw_packet (struct peer *peer, afi_t afi, safi_t safi)
{
  struct stream *s;
  struct stream *packet;
  struct bgp_adj_out *adj;
  struct bgp_advertise *adv;
  struct bgp_node *rn;
  unsigned long pos;
  bgp_size_t unfeasible_len;
  bgp_size_t total_attr_len;
  char buf[BUFSIZ];
  struct prefix_rd *prd = NULL;

  s = peer->work;
  stream_reset (s);

  while ((adv = FIFO_HEAD (&peer->sync[afi][safi]->withdraw)) != NULL)
    {
      adj = adv->adj;
      rn = adv->rn;
#ifdef MPLS_VPN
      prd = (struct prefix_rd *) &rn->prn->p;
#endif /* MPLS_VPN */

      if (STREAM_REMAIN (s) 
	  < (BGP_NLRI_LENGTH + BGP_TOTAL_ATTR_LEN + PSIZE (rn->p.prefixlen)))
	break;

      if (stream_empty (s))
	{
	  bgp_packet_set_marker (s, BGP_MSG_UPDATE);
	  stream_putw (s, 0);
	}

      if (afi == AFI_IP && safi == SAFI_UNICAST)
	stream_put_prefix (s, &rn->p);
      else
	{
	  pos = stream_get_putp (s);
	  stream_putw (s, 0);
	  total_attr_len
	    = bgp_packet_withdraw (peer, s, &rn->p, afi, safi, prd, NULL);
      
	  /* Set total path attribute length. */
	  stream_putw_at (s, pos, total_attr_len);
	}

      if (BGP_DEBUG (update, UPDATE_OUT))
	zlog (peer->log, LOG_INFO, "%s send UPDATE %s/%d -- unreachable",
	      peer->host,
	      inet_ntop (rn->p.family, &(rn->p.u.prefix), buf, BUFSIZ),
	      rn->p.prefixlen);

      peer->scount[afi][safi]--;

      bgp_adj_out_remove (rn, adj, peer, afi, safi);
      bgp_unlock_node (rn);

      if (! (afi == AFI_IP && safi == SAFI_UNICAST))
	break;
    }

  if (! stream_empty (s))
    {
      if (afi == AFI_IP && safi == SAFI_UNICAST)
	{
	  unfeasible_len 
	    = stream_get_putp (s) - BGP_HEADER_SIZE - BGP_UNFEASIBLE_LEN;
	  stream_putw_at (s, BGP_HEADER_SIZE, unfeasible_len);
	  stream_putw (s, 0);
	}
      bgp_packet_set_size (s);
      packet = bgp_packet_dup (s);
      bgp_packet_add (peer, packet);
      stream_reset (s);
      return packet;
    }

  return NULL;
}

void
bgp_default_update_send (struct peer *peer, struct attr *attr,
			 afi_t afi, safi_t safi, struct peer *from)
{
  struct stream *s;
  struct stream *packet;
  struct prefix p;
  unsigned long pos;
  bgp_size_t total_attr_len;
  char attrstr[BUFSIZ];
  char buf[BUFSIZ];

#ifdef DISABLE_BGP_ANNOUNCE
  return;
#endif /* DISABLE_BGP_ANNOUNCE */

  if (afi == AFI_IP)
    str2prefix ("0.0.0.0/0", &p);
#ifdef HAVE_IPV6
  else 
    str2prefix ("::/0", &p);
#endif /* HAVE_IPV6 */

  /* Logging the attribute. */
  if (BGP_DEBUG (update, UPDATE_OUT))
    {
      bgp_dump_attr (peer, attr, attrstr, BUFSIZ);
      zlog (peer->log, LOG_INFO, "%s send UPDATE %s/%d %s",
	    peer->host, inet_ntop(p.family, &(p.u.prefix), buf, BUFSIZ),
	    p.prefixlen, attrstr);
    }

  s = stream_new (BGP_MAX_PACKET_SIZE);

  /* Make BGP update packet. */
  bgp_packet_set_marker (s, BGP_MSG_UPDATE);

  /* Unfeasible Routes Length. */
  stream_putw (s, 0);

  /* Make place for total attribute length.  */
  pos = stream_get_putp (s);
  stream_putw (s, 0);
  total_attr_len = bgp_packet_attribute (NULL, peer, s, attr, &p, afi, safi, from, NULL, NULL);

  /* Set Total Path Attribute Length. */
  stream_putw_at (s, pos, total_attr_len);

  /* NLRI set. */
  if (p.family == AF_INET && safi == SAFI_UNICAST)
    stream_put_prefix (s, &p);

  /* Set size. */
  bgp_packet_set_size (s);

  packet = bgp_packet_dup (s);
  stream_free (s);

  /* Dump packet if debug option is set. */
#ifdef DEBUG
  bgp_packet_dump (packet);
#endif /* DEBUG */

  /* Add packet to the peer. */
  bgp_packet_add (peer, packet);

  BGP_WRITE_ON (peer->t_write, bgp_write, peer->fd);
}

void
bgp_default_withdraw_send (struct peer *peer, afi_t afi, safi_t safi)
{
  struct stream *s;
  struct stream *packet;
  struct prefix p;
  unsigned long pos;
  unsigned long cp;
  bgp_size_t unfeasible_len;
  bgp_size_t total_attr_len;
  char buf[BUFSIZ];

#ifdef DISABLE_BGP_ANNOUNCE
  return;
#endif /* DISABLE_BGP_ANNOUNCE */

  if (afi == AFI_IP)
    str2prefix ("0.0.0.0/0", &p);
#ifdef HAVE_IPV6
  else 
    str2prefix ("::/0", &p);
#endif /* HAVE_IPV6 */

  total_attr_len = 0;
  pos = 0;

  if (BGP_DEBUG (update, UPDATE_OUT))
    zlog (peer->log, LOG_INFO, "%s send UPDATE %s/%d -- unreachable",
          peer->host, inet_ntop(p.family, &(p.u.prefix), buf, BUFSIZ),
          p.prefixlen);

  s = stream_new (BGP_MAX_PACKET_SIZE);

  /* Make BGP update packet. */
  bgp_packet_set_marker (s, BGP_MSG_UPDATE);

  /* Unfeasible Routes Length. */;
  cp = stream_get_putp (s);
  stream_putw (s, 0);

  /* Withdrawn Routes. */
  if (p.family == AF_INET && safi == SAFI_UNICAST)
    {
      stream_put_prefix (s, &p);

      unfeasible_len = stream_get_putp (s) - cp - 2;

      /* Set unfeasible len.  */
      stream_putw_at (s, cp, unfeasible_len);

      /* Set total path attribute length. */
      stream_putw (s, 0);
    }
  else
    {
      pos = stream_get_putp (s);
      stream_putw (s, 0);
      total_attr_len = bgp_packet_withdraw (peer, s, &p, afi, safi, NULL, NULL);

      /* Set total path attribute length. */
      stream_putw_at (s, pos, total_attr_len);
    }

  bgp_packet_set_size (s);

  packet = bgp_packet_dup (s);
  stream_free (s);

  /* Add packet to the peer. */
  bgp_packet_add (peer, packet);

  BGP_WRITE_ON (peer->t_write, bgp_write, peer->fd);
}

/* Get next packet to be written.  */
struct stream *
bgp_write_packet (struct peer *peer)
{
  afi_t afi;
  safi_t safi;
  struct stream *s = NULL;
  struct bgp_advertise *adv;

  s = stream_fifo_head (peer->obuf);
  if (s)
    return s;

  for (afi = AFI_IP; afi < AFI_MAX; afi++)
    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
      {
	adv = FIFO_HEAD (&peer->sync[afi][safi]->withdraw);
	if (adv)
	  {
	    s = bgp_withdraw_packet (peer, afi, safi);
	    if (s)
	      return s;
	  }
      }
    
  for (afi = AFI_IP; afi < AFI_MAX; afi++)
    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
      {
	adv = FIFO_HEAD (&peer->sync[afi][safi]->update);
	if (adv)
	  {
            if (adv->binfo && adv->binfo->uptime < peer->synctime[afi][safi])
	      {
		if (CHECK_FLAG (adv->binfo->peer->cap, PEER_CAP_RESTART_RCV)
		    && CHECK_FLAG (adv->binfo->peer->cap, PEER_CAP_RESTART_ADV)
		    && ! CHECK_FLAG (adv->binfo->flags, BGP_INFO_STALE)
		    && safi != SAFI_MPLS_VPN)
		  {
		    if (CHECK_FLAG (adv->binfo->peer->af_sflags[afi][safi],
			PEER_STATUS_EOR_RECEIVED))
		      s = bgp_update_packet (peer, afi, safi);
		  }
		else
		  s = bgp_update_packet (peer, afi, safi);
	      }

	    if (s)
	      return s;
	  }

	if (CHECK_FLAG (peer->cap, PEER_CAP_RESTART_RCV))
	  {
	    if (peer->afc_nego[afi][safi] && peer->synctime[afi][safi]
		&& ! CHECK_FLAG (peer->af_sflags[afi][safi], PEER_STATUS_EOR_SEND)
		&& safi != SAFI_MPLS_VPN)
	      {
		SET_FLAG (peer->af_sflags[afi][safi], PEER_STATUS_EOR_SEND);
		return bgp_update_packet_eor (peer, afi, safi);
	      }
	  }
      }

  return NULL;
}

/* Is there partially written packet or updates we can send right
   now.  */
int
bgp_write_proceed (struct peer *peer)
{
  afi_t afi;
  safi_t safi;
  struct bgp_advertise *adv;

  if (stream_fifo_head (peer->obuf))
    return 1;

  for (afi = AFI_IP; afi < AFI_MAX; afi++)
    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
      if (FIFO_HEAD (&peer->sync[afi][safi]->withdraw))
	return 1;

  for (afi = AFI_IP; afi < AFI_MAX; afi++)
    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
      if ((adv = FIFO_HEAD (&peer->sync[afi][safi]->update)) != NULL)
	if (adv->binfo->uptime < peer->synctime[afi][safi])
	  return 1;

  return 0;
}

/* Write packet to the peer. */
int
bgp_write (struct thread *thread)
{
  struct peer *peer;
  u_char type;
  struct stream *s; 
  int num;
  int count = 0;
  int write_errno;

  /* Yes first of all get peer pointer. */
  peer = THREAD_ARG (thread);
  peer->t_write = NULL;

  /* For non-blocking IO check. */
  if (peer->status == Connect)
    {
      bgp_connect_check (peer);
      return 0;
    }

    /* Nonblocking write until TCP output buffer is full.  */
  while (1)
    {
      int writenum;

      s = bgp_write_packet (peer);
      if (! s)
	return 0;

      /* Number of bytes to be sent.  */
      writenum = stream_get_endp (s) - stream_get_getp (s);

      /* Call write() system call.  */
      num = write (peer->fd, STREAM_PNT (s), writenum);
      write_errno = errno;
      if (num <= 0)
	{
	  /* Partial write. */
	  if (write_errno == EWOULDBLOCK || write_errno == EAGAIN)
	      break;

	  bgp_stop (peer);
	  peer->status = Idle;
	  bgp_timer_set (peer);
	  return 0;
	}
      if (num != writenum)
	{
	  stream_forward (s, num);

	  if (write_errno == EAGAIN)
	    break;

	  continue;
	}

      /* Retrieve BGP packet type. */
      stream_set_getp (s, BGP_MARKER_SIZE + 2);
      type = stream_getc (s);

      switch (type)
	{
	case BGP_MSG_OPEN:
	  peer->open_out++;
	  break;
	case BGP_MSG_UPDATE:
	  peer->update_out++;
	  break;
	case BGP_MSG_NOTIFY:
	  peer->notify_out++;

	  /* BGP_EVENT_ADD (peer, BGP_Stop); */
	  bgp_stop_with_error (peer);
	  bgp_fsm_change_status (peer, Idle);
	  bgp_timer_set (peer);
	  return 0;
	  break;
	case BGP_MSG_KEEPALIVE:
	  peer->keepalive_out++;
	  break;
	case BGP_MSG_ROUTE_REFRESH_NEW:
	case BGP_MSG_ROUTE_REFRESH_OLD:
	  peer->refresh_out++;
	  break;
	case BGP_MSG_CAPABILITY:
	  peer->dynamic_cap_out++;
	  break;
	}

      /* OK we send packet so delete it. */
      bgp_packet_delete (peer);

      if (++count >= BGP_WRITE_PACKET_MAX)
	break;
    }
  
  if (bgp_write_proceed (peer))
    BGP_WRITE_ON (peer->t_write, bgp_write, peer->fd);
  
  return 0;
}

/* This is only for sending NOTIFICATION message to neighbor. */
int
bgp_write_notify (struct peer *peer)
{
  int ret;
  u_char type;
  struct stream *s; 

  /* There should be at least one packet. */
  s = stream_fifo_head (peer->obuf);
  if (!s)
    return 0;
  assert (stream_get_endp (s) >= BGP_HEADER_SIZE);

  /* I'm not sure fd is writable. */
  ret = writen (peer->fd, STREAM_DATA (s), stream_get_endp (s));
  if (ret <= 0)
    {
      bgp_stop (peer);
      peer->status = Idle;
      bgp_timer_set (peer);
      return 0;
    }

  /* Retrieve BGP packet type. */
  stream_set_getp (s, BGP_MARKER_SIZE + 2);
  type = stream_getc (s);

  assert (type == BGP_MSG_NOTIFY);

  /* Type should be notify. */
  peer->notify_out++;

  /* We don't call event manager at here for avoiding other events. */
  if (peer->status == Established)
    bgp_stop (peer);
  else
    {
      bgp_stop_with_error (peer);
      bgp_fsm_change_status (peer, Idle);
    }
  bgp_timer_set (peer);

  return 0;
}

/* Make keepalive packet and send it to the peer. */
void
bgp_keepalive_send (struct peer *peer)
{
  struct stream *s;
  int length;

  s = stream_new (BGP_MAX_PACKET_SIZE);

  /* Make keepalive packet. */
  bgp_packet_set_marker (s, BGP_MSG_KEEPALIVE);

  /* Set packet size. */
  length = bgp_packet_set_size (s);

  /* Dump packet if debug option is set. */
  /* bgp_packet_dump (s); */
 
  if (BGP_DEBUG (keepalive, KEEPALIVE))  
    zlog_info ("%s sending KEEPALIVE", peer->host); 

  /* Add packet to the peer. */
  bgp_packet_add (peer, s);

  BGP_WRITE_ON (peer->t_write, bgp_write, peer->fd);
}

/* Make open packet and send it to the peer. */
void
bgp_open_send (struct peer *peer)
{
  struct stream *s;
  int length;
  u_int16_t send_holdtime;
  as_t local_as;

  if (CHECK_FLAG (peer->config, PEER_CONFIG_TIMER))
    send_holdtime = peer->holdtime;
  else
    send_holdtime = peer->bgp->default_holdtime;

  /* local-as Change */
  if (peer->change_local_as)
    local_as = peer->change_local_as; 
  else
    local_as = peer->local_as; 

  s = stream_new (BGP_MAX_PACKET_SIZE);

  /* Make open packet. */
  bgp_packet_set_marker (s, BGP_MSG_OPEN);

  /* Set open packet values. */
  stream_putc (s, BGP_VERSION_4);        /* BGP version */
  stream_putw (s, local_as);		 /* My Autonomous System*/
  stream_putw (s, send_holdtime);     	 /* Hold Time */
  stream_put_in_addr (s, &peer->local_id); /* BGP Identifier */

  /* Set capability code. */
  bgp_open_capability (s, peer);

  /* Set BGP packet length. */
  length = bgp_packet_set_size (s);

  if (BGP_DEBUG (normal, NORMAL))
    zlog_info ("%s sending OPEN, version %d, my as %d, holdtime %d, id %s", 
	       peer->host, BGP_VERSION_4, local_as,
	       send_holdtime, inet_ntoa (peer->local_id));

  if (BGP_DEBUG (normal, NORMAL))
    zlog_info ("%s send message type %d, length (incl. header) %d",
	       peer->host, BGP_MSG_OPEN, length);

  /* Dump packet if debug option is set. */
  /* bgp_packet_dump (s); */

  /* Add packet to the peer. */
  bgp_packet_add (peer, s);

  BGP_WRITE_ON (peer->t_write, bgp_write, peer->fd);
}

/* Send BGP notify packet with data potion. */
void
bgp_notify_send_with_data (struct peer *peer, u_char code, u_char sub_code,
			   u_char *data, size_t datalen)
{
  struct stream *s;
  int length;

  /* Allocate new stream. */
  s = stream_new (BGP_MAX_PACKET_SIZE);

  /* Make nitify packet. */
  bgp_packet_set_marker (s, BGP_MSG_NOTIFY);

  /* Set notify packet values. */
  stream_putc (s, code);        /* BGP notify code */
  stream_putc (s, sub_code);	/* BGP notify sub_code */

  /* If notify data is present. */
  if (data)
    stream_write (s, data, datalen);
  
  /* Set BGP packet length. */
  length = bgp_packet_set_size (s);
  
  /* Add packet to the peer. */
  stream_fifo_clean (peer->obuf);
  bgp_packet_add (peer, s);

  /* For debug */
  {
    struct bgp_notify bgp_notify;
    int first = 0;
    int i;
    char c[4];

    bgp_notify.code = code;
    bgp_notify.subcode = sub_code;
    bgp_notify.data = NULL;
    bgp_notify.length = length - BGP_MSG_NOTIFY_MIN_SIZE;
    
    if (bgp_notify.length)
      {
	bgp_notify.data = XMALLOC (MTYPE_TMP, bgp_notify.length * 3);
	for (i = 0; i < bgp_notify.length; i++)
	  if (first)
	    {
	      sprintf (c, " %02x", data[i]);
	      strcat (bgp_notify.data, c);
	    }
	  else
	    {
	      first = 1;
	      sprintf (c, "%02x", data[i]);
	      strcpy (bgp_notify.data, c);
	    }
      }
    bgp_notify_print (peer, &bgp_notify, "sending");
    if (bgp_notify.data)
      XFREE (MTYPE_TMP, bgp_notify.data);
  }

  if (BGP_DEBUG (normal, NORMAL))
    zlog_info ("%s send message type %d, length (incl. header) %d",
	       peer->host, BGP_MSG_NOTIFY, length);

  /* peer reset cause */
  if (sub_code != BGP_NOTIFY_CEASE_CONFIG_CHANGE)
    {
      if (sub_code == BGP_NOTIFY_CEASE_ADMIN_RESET)
	peer->last_reset = PEER_DOWN_USER_RESET;
      else if (sub_code == BGP_NOTIFY_CEASE_ADMIN_SHUTDOWN)
	peer->last_reset = PEER_DOWN_USER_SHUTDOWN;
      else if (sub_code == BGP_NOTIFY_CEASE_PEER_UNCONFIG)
	peer->last_reset = PEER_DOWN_NEIGHBOR_DELETE;
      else
	peer->last_reset = PEER_DOWN_NOTIFY_SEND;
    }
  /* Call imidiately. */
  BGP_WRITE_OFF (peer->t_write);

  bgp_write_notify (peer);
}

/* Send BGP notify packet. */
void
bgp_notify_send (struct peer *peer, u_char code, u_char sub_code)
{
  bgp_notify_send_with_data (peer, code, sub_code, NULL, 0);
}

char *
afi2str (afi_t afi)
{
  if (afi == AFI_IP)
    return "AFI_IP";
  else if (afi == AFI_IP6)
    return "AFI_IP6";
  else
    return "Unknown AFI";
}

char *
safi2str (safi_t safi)
{
  if (safi == SAFI_UNICAST)
    return "SAFI_UNICAST";
  else if (safi == SAFI_MULTICAST)
    return "SAFI_MULTICAST";
  else if (safi == SAFI_MPLS_VPN || safi == BGP_SAFI_VPNV4)
    return "SAFI_MPLS_VPN";
  else
    return "Unknown SAFI";
}

/* Send route refresh message to the peer. */
void
bgp_route_refresh_send (struct peer *peer, afi_t afi, safi_t safi,
			u_char orf_type, u_char when_to_refresh, int remove)
{
  struct stream *s;
  struct stream *packet;
  int length;
  struct bgp_filter *filter;
  int orf_refresh = 0;
  int msg_type;

#ifdef DISABLE_BGP_ANNOUNCE
  return;
#endif /* DISABLE_BGP_ANNOUNCE */

  if (CHECK_FLAG (peer->cap, PEER_CAP_REFRESH_NEW_RCV))
    msg_type = BGP_MSG_ROUTE_REFRESH_NEW;
  else
    msg_type = BGP_MSG_ROUTE_REFRESH_OLD;
  filter = &peer->filter[afi][safi];

  /* Adjust safi code. */
  if (safi == SAFI_MPLS_VPN)
    safi = BGP_SAFI_VPNV4;
  
  s = stream_new (BGP_MAX_PACKET_SIZE);

  /* Make BGP update packet. */
  bgp_packet_set_marker (s, msg_type);

  /* Encode Route Refresh message. */
  stream_putw (s, afi);
  stream_putc (s, 0);
  stream_putc (s, safi);
 
  if (orf_type == ORF_TYPE_PREFIX
      || orf_type == ORF_TYPE_PREFIX_OLD)
    if (remove || filter->plist[FILTER_IN].plist)
      {
	u_int16_t orf_len;
	unsigned long orfp;

	orf_refresh = 1; 
	stream_putc (s, when_to_refresh);
	stream_putc (s, orf_type);
	orfp = stream_get_putp (s);
	stream_putw (s, 0);

	if (remove)
	  {
	    UNSET_FLAG (peer->af_sflags[afi][safi], PEER_STATUS_ORF_PREFIX_SEND);
	    stream_putc (s, ORF_COMMON_PART_REMOVE_ALL);
	    if (BGP_DEBUG (normal, NORMAL))
	      zlog_info ("%s sending REFRESH_REQ to remove ORF(%d) (%s) for afi/safi: %d/%d", 
			 peer->host, orf_type,
			 (when_to_refresh == REFRESH_DEFER ? "defer" : "immediate"),
			 afi, safi);
	  }
	else
	  {
	    SET_FLAG (peer->af_sflags[afi][safi], PEER_STATUS_ORF_PREFIX_SEND);
	    prefix_bgp_orf_entry (s, filter->plist[FILTER_IN].plist,
				  ORF_COMMON_PART_ADD, ORF_COMMON_PART_PERMIT,
				  ORF_COMMON_PART_DENY);
	    if (BGP_DEBUG (normal, NORMAL))
	      zlog_info ("%s sending REFRESH_REQ with pfxlist ORF(%d) (%s) for afi/safi: %d/%d", 
			 peer->host, orf_type,
			 (when_to_refresh == REFRESH_DEFER ? "defer" : "immediate"),
			 afi, safi);
	  }

	/* Total ORF Entry Len. */
	orf_len = stream_get_putp (s) - orfp - 2;
	stream_putw_at (s, orfp, orf_len);
      }

  /* Set packet size. */
  length = bgp_packet_set_size (s);

  if (BGP_DEBUG (normal, NORMAL))
    {
      if (! orf_refresh)
	zlog_info ("%s sending REFRESH_REQ(%d) for afi/safi: %d/%d", 
		   peer->host, msg_type, afi, safi);
      zlog_info ("%s send message type %d, length (incl. header) %d",
		 peer->host, msg_type, length);
    }

  /* Make real packet. */
  packet = bgp_packet_dup (s);
  stream_free (s);

  /* Add packet to the peer. */
  bgp_packet_add (peer, packet);

  BGP_WRITE_ON (peer->t_write, bgp_write, peer->fd);
}

/* Send capability message to the peer. */
void
bgp_capability_send (struct peer *peer, afi_t afi, safi_t safi,
		     int capability_code, int action)
{
  struct stream *s;
  struct stream *packet;
  int length;

  /* Adjust safi code. */
  if (safi == SAFI_MPLS_VPN)
    safi = BGP_SAFI_VPNV4;

  s = stream_new (BGP_MAX_PACKET_SIZE);

  /* Make BGP update packet. */
  bgp_packet_set_marker (s, BGP_MSG_CAPABILITY);

  /* Encode MP_EXT capability. */
  if (capability_code == CAPABILITY_CODE_MP)
    {
      stream_putc (s, action);
      stream_putc (s, CAPABILITY_CODE_MP);
      stream_putc (s, CAPABILITY_CODE_MP_LEN);
      stream_putw (s, afi);
      stream_putc (s, 0);
      stream_putc (s, safi);

      if (BGP_DEBUG (normal, NORMAL))
        zlog_info ("%s sending CAPABILITY has %s MP_EXT CAP for afi/safi: %d/%d",
		   peer->host, action == CAPABILITY_ACTION_SET ?
		   "Advertising" : "Removing", afi, safi);
    }

  /* Set packet size. */
  length = bgp_packet_set_size (s);

  /* Make real packet. */
  packet = bgp_packet_dup (s);
  stream_free (s);

  /* Add packet to the peer. */
  bgp_packet_add (peer, packet);

  if (BGP_DEBUG (normal, NORMAL))
    zlog_info ("%s send message type %d, length (incl. header) %d",
	       peer->host, BGP_MSG_CAPABILITY, length);

  BGP_WRITE_ON (peer->t_write, bgp_write, peer->fd);
}

/* RFC1771 6.8 Connection collision detection. */
int
bgp_collision_detect (struct peer *new, struct in_addr remote_id)
{
  struct peer *peer;
  struct listnode *nn;
  struct bgp *bgp;

  bgp = bgp_get_default ();
  if (! bgp)
    return 0;
  
  /* Upon receipt of an OPEN message, the local system must examine
     all of its connections that are in the OpenConfirm state.  A BGP
     speaker may also examine connections in an OpenSent state if it
     knows the BGP Identifier of the peer by means outside of the
     protocol.  If among these connections there is a connection to a
     remote BGP speaker whose BGP Identifier equals the one in the
     OPEN message, then the local system performs the following
     collision resolution procedure: */

  LIST_LOOP (bgp->peer, peer, nn)
    {
      /* Under OpenConfirm status, local peer structure already hold
         remote router ID. */

      if (peer != new
	  && (peer->status == OpenConfirm || peer->status == OpenSent)
	  && sockunion_same (&peer->su, &new->su))
	{
	  /* 1. The BGP Identifier of the local system is compared to
	     the BGP Identifier of the remote system (as specified in
	     the OPEN message). */

	  if (ntohl (peer->local_id.s_addr) < ntohl (remote_id.s_addr))
	    {
	      /* 2. If the value of the local BGP Identifier is less
		 than the remote one, the local system closes BGP
		 connection that already exists (the one that is
		 already in the OpenConfirm state), and accepts BGP
		 connection initiated by the remote system. */

	      if (peer->fd >= 0)
		bgp_notify_send (peer, BGP_NOTIFY_CEASE, BGP_NOTIFY_CEASE_CONNECT_COLLISION);
	      return 1;
	    }
	  else
	    {
	      /* 3. Otherwise, the local system closes newly created
		 BGP connection (the one associated with the newly
		 received OPEN message), and continues to use the
		 existing one (the one that is already in the
		 OpenConfirm state). */

	      if (new->fd >= 0)
		bgp_notify_send (peer, BGP_NOTIFY_CEASE, BGP_NOTIFY_CEASE_CONNECT_COLLISION);
	      return -1;
	    }
	}
    }
  return 0;
}

int
bgp_open_receive (struct peer *peer, bgp_size_t size)
{
  int ret;
  u_char version;
  u_char optlen;
  u_int16_t holdtime;
  u_int16_t send_holdtime;
  as_t remote_as;
  struct peer *realpeer;
  struct in_addr remote_id;
  int capability;
  char notify_data_remote_as[2];
  char notify_data_remote_id[4];

  realpeer = NULL;
  
  /* Parse open packet. */
  version = stream_getc (peer->ibuf);
  memcpy (notify_data_remote_as, stream_pnt (peer->ibuf), 2);
  remote_as  = stream_getw (peer->ibuf);
  holdtime = stream_getw (peer->ibuf);
  memcpy (notify_data_remote_id, stream_pnt (peer->ibuf), 4);
  remote_id.s_addr = stream_get_ipv4 (peer->ibuf);

  /* Receive OPEN message log  */
  if (BGP_DEBUG (normal, NORMAL))
    zlog_info ("%s rcv OPEN, version %d, remote-as %d, holdtime %d, id %s",
	       peer->host, version, remote_as, holdtime,
	       inet_ntoa (remote_id));
	  
  /* Lookup peer from Open packet. */
  if (CHECK_FLAG (peer->sflags, PEER_STATUS_ACCEPT_PEER))
    {
      int as = 0;

      realpeer = peer_lookup_with_open (&peer->su, remote_as, &remote_id, &as);

      if (! realpeer)
	{
	  /* Peer's source IP address is check in bgp_accept(), so this
	     must be AS number mismatch or remote-id configuration
	     mismatch. */
	  if (as)
	    {
	      if (BGP_DEBUG (normal, NORMAL))
		zlog_info ("%s bad OPEN, wrong router identifier %s",
			   peer->host, inet_ntoa (remote_id));
	      bgp_notify_send_with_data (peer, 
					 BGP_NOTIFY_OPEN_ERR, 
					 BGP_NOTIFY_OPEN_BAD_BGP_IDENT,
					 notify_data_remote_id, 4);
	    }
	  else
	    {
	      if (BGP_DEBUG (normal, NORMAL))
		zlog_info ("%s bad OPEN, remote AS is %d, expected %d",
			   peer->host, remote_as, peer->as);
	      bgp_notify_send_with_data (peer, 
					 BGP_NOTIFY_OPEN_ERR, 
					 BGP_NOTIFY_OPEN_BAD_PEER_AS,
					 notify_data_remote_as, 2);
	    }
	  return -1;
	}
    }

  /* When collision is detected and this peer is closed.  Retrun
     immidiately. */
  ret = bgp_collision_detect (peer, remote_id);
  if (ret < 0)
    return ret;

  /* Hack part. */
  if (CHECK_FLAG (peer->sflags, PEER_STATUS_ACCEPT_PEER))
    {
      if (CHECK_FLAG (realpeer->flags, PEER_FLAG_CONNECT_MODE_ACTIVE))
	{
 	  if (BGP_DEBUG (normal, NORMAL))
 	    zlog_info ("%s passive open failed - TCP session must be opened actively",
		       realpeer->host);
	  bgp_notify_send (peer, BGP_NOTIFY_CEASE, BGP_NOTIFY_CEASE_CONNECT_REJECT);
 	  return -1;
	}

      if (realpeer->status == Established
	  && CHECK_FLAG (realpeer->sflags, PEER_STATUS_NSF_MODE))
	{
	  realpeer->last_reset = PEER_DOWN_NSF_CLOSE_SESSION;
	  SET_FLAG (realpeer->sflags, PEER_STATUS_NSF_WAIT);
	}
      else if (ret == 0 && realpeer->status != Active
	       && realpeer->status != OpenSent
	       && realpeer->status != OpenConfirm)
 	{
 	  if (BGP_DEBUG (events, EVENTS))
 	    zlog_info ("%s peer status is %s close connection",
		       realpeer->host, LOOKUP (bgp_status_msg, realpeer->status));
	  bgp_notify_send (peer, BGP_NOTIFY_CEASE, BGP_NOTIFY_CEASE_CONNECT_REJECT);
 	  return -1;
 	}

      if (BGP_DEBUG (events, EVENTS))
	zlog_info ("%s [Event] Transfer temporary BGP peer to existing one",
		   peer->host);

      bgp_stop (realpeer);
      
      /* Transfer file descriptor. */
      realpeer->fd = peer->fd;
      peer->fd = -1;

      /* Transfer input buffer. */
      stream_free (realpeer->ibuf);
      realpeer->ibuf = peer->ibuf;
      realpeer->packet_size = peer->packet_size;
      peer->ibuf = NULL;

      /* Transfer status. */
      bgp_fsm_change_status (realpeer, peer->status);
      bgp_stop (peer);

      /* peer pointer change. Open packet send to neighbor. */
      peer = realpeer;
      bgp_open_send (peer);
      if (peer->fd < 0)
	{
	  zlog_err ("bgp_open_receive peer's fd is negative value %d",
		    peer->fd);
	  return -1;
	}
      BGP_READ_ON (peer->t_read, bgp_read, peer->fd);
    }

  /* remote router-id check. */
  if (remote_id.s_addr == 0
      || ntohl (remote_id.s_addr) >= 0xe0000000
      || ntohl (peer->local_id.s_addr) == ntohl (remote_id.s_addr))
    {
      if (BGP_DEBUG (normal, NORMAL))
	zlog_info ("%s bad OPEN, wrong router identifier %s",
		   peer->host, inet_ntoa (remote_id));
      bgp_notify_send_with_data (peer, 
				 BGP_NOTIFY_OPEN_ERR, 
				 BGP_NOTIFY_OPEN_BAD_BGP_IDENT,
				 notify_data_remote_id, 4);
      return -1;
    }

  /* Set remote router-id */
  peer->remote_id = remote_id;

  /* Peer BGP version check. */
  if (version != BGP_VERSION_4)
    {
      if (BGP_DEBUG (normal, NORMAL))
	zlog_info ("%s bad protocol version, remote requested %d, local request %d",
		   peer->host, version, BGP_VERSION_4);
      bgp_notify_send_with_data (peer, 
				 BGP_NOTIFY_OPEN_ERR, 
				 BGP_NOTIFY_OPEN_UNSUP_VERSION,
				 "\x04", 1);
      return -1;
    }

  /* Check neighbor as number. */
  if (remote_as != peer->as)
    {
      if (BGP_DEBUG (normal, NORMAL))
	zlog_info ("%s bad OPEN, remote AS is %d, expected %d",
		   peer->host, remote_as, peer->as);
      bgp_notify_send_with_data (peer, 
				 BGP_NOTIFY_OPEN_ERR, 
				 BGP_NOTIFY_OPEN_BAD_PEER_AS,
				 notify_data_remote_as, 2);
      return -1;
    }

  /* From the rfc: Upon receipt of an OPEN message, a BGP speaker MUST
     calculate the value of the Hold Timer by using the smaller of its
     configured Hold Time and the Hold Time received in the OPEN message.
     The Hold Time MUST be either zero or at least three seconds.  An
     implementation may reject connections on the basis of the Hold Time. */

  if (holdtime < 3 && holdtime != 0)
    {
      bgp_notify_send (peer,
		       BGP_NOTIFY_OPEN_ERR, 
		       BGP_NOTIFY_OPEN_UNACEP_HOLDTIME);
      return -1;
    }
    
  /* From the rfc: A reasonable maximum time between KEEPALIVE messages
     would be one third of the Hold Time interval.  KEEPALIVE messages
     MUST NOT be sent more frequently than one per second.  An
     implementation MAY adjust the rate at which it sends KEEPALIVE
     messages as a function of the Hold Time interval. */

  if (CHECK_FLAG (peer->config, PEER_CONFIG_TIMER))
    send_holdtime = peer->holdtime;
  else
    send_holdtime = peer->bgp->default_holdtime;

  if (holdtime < send_holdtime)
    peer->v_holdtime = holdtime;
  else
    peer->v_holdtime = send_holdtime;

  peer->v_keepalive = peer->v_holdtime / 3;

  /* Open option part parse. */
  capability = 0;
  optlen = stream_getc (peer->ibuf);
  if (optlen != 0) 
    {
      ret = bgp_open_option_parse (peer, optlen, &capability);
      if (ret < 0)
	return ret;

      stream_forward (peer->ibuf, optlen);
    }
  else
    {
      if (BGP_DEBUG (normal, NORMAL))
	zlog_info ("%s rcvd OPEN w/ OPTION parameter len: 0",
		   peer->host);
    }

  /* Override capability. */
  if (! capability || CHECK_FLAG (peer->flags, PEER_FLAG_OVERRIDE_CAPABILITY))
    {
      peer->afc_nego[AFI_IP][SAFI_UNICAST] = peer->afc[AFI_IP][SAFI_UNICAST];
      peer->afc_nego[AFI_IP][SAFI_MULTICAST] = peer->afc[AFI_IP][SAFI_MULTICAST];
      peer->afc_nego[AFI_IP6][SAFI_UNICAST] = peer->afc[AFI_IP6][SAFI_UNICAST];
      peer->afc_nego[AFI_IP6][SAFI_MULTICAST] = peer->afc[AFI_IP6][SAFI_MULTICAST];
    }

  /* Get sockname. */
  bgp_getsockname (peer);

  BGP_EVENT_ADD (peer, Receive_OPEN_message);

  peer->packet_size = 0;
  if (peer->ibuf)
    stream_reset (peer->ibuf);

  return 0;
}

/* Parse BGP Update packet and make attribute object. */
int
bgp_update_receive (struct peer *peer, bgp_size_t size)
{
  int ret;
  u_char *end;
  struct stream *s;
  struct attr attr;
  bgp_size_t attribute_len;
  bgp_size_t update_len;
  bgp_size_t withdraw_len;
  struct bgp_nlri update;
  struct bgp_nlri withdraw;
  struct bgp_nlri mp_update;
  struct bgp_nlri mp_withdraw;
  char attrstr[BUFSIZ] = "";

  /* Status must be Established. */
  if (peer->status != Established) 
    {
      zlog_err ("%s [FSM] Update packet received under status %s",
		peer->host, LOOKUP (bgp_status_msg, peer->status));
      bgp_notify_send (peer, BGP_NOTIFY_FSM_ERR, 0);
      return -1;
    }

  /* Set initial values. */
  memset (&attr, 0, sizeof (struct attr));
  memset (&update, 0, sizeof (struct bgp_nlri));
  memset (&withdraw, 0, sizeof (struct bgp_nlri));
  memset (&mp_update, 0, sizeof (struct bgp_nlri));
  memset (&mp_withdraw, 0, sizeof (struct bgp_nlri));

  s = peer->ibuf;
  end = stream_pnt (s) + size;

  /* RFC1771 6.3 If the Unfeasible Routes Length or Total Attribute
     Length is too large (i.e., if Unfeasible Routes Length + Total
     Attribute Length + 23 exceeds the message Length), then the Error
     Subcode is set to Malformed Attribute List.  */
  if (stream_pnt (s) + 2 > end)
    {
      zlog_err ("%s [Error] Update packet error"
		" (packet length is short for unfeasible length)",
		peer->host);
      bgp_notify_send (peer, BGP_NOTIFY_UPDATE_ERR, 
		       BGP_NOTIFY_UPDATE_MAL_ATTR);
      return -1;
    }

  /* Unfeasible Route Length. */
  withdraw_len = stream_getw (s);

  /* Unfeasible Route Length check. */
  if (stream_pnt (s) + withdraw_len > end)
    {
      zlog_err ("%s [Error] Update packet error"
		" (packet unfeasible length overflow %d)",
		peer->host, withdraw_len);
      bgp_notify_send (peer, BGP_NOTIFY_UPDATE_ERR, 
		       BGP_NOTIFY_UPDATE_MAL_ATTR);
      return -1;
    }

  /* Unfeasible Route packet format check. */
  if (withdraw_len > 0)
    {
      ret = bgp_nlri_sanity_check (peer, AFI_IP, stream_pnt (s), withdraw_len);
      if (ret < 0)
	return -1;

      if (BGP_DEBUG (packet, PACKET_RECV))
	  zlog_info ("%s [Update:RECV] Unfeasible NLRI received", peer->host);

      withdraw.afi = AFI_IP;
      withdraw.safi = SAFI_UNICAST;
      withdraw.nlri = stream_pnt (s);
      withdraw.length = withdraw_len;
      stream_forward (s, withdraw_len);
    }
  
  /* Attribute total length check. */
  if (stream_pnt (s) + 2 > end)
    {
      zlog_warn ("%s [Error] Packet Error"
		 " (update packet is short for attribute length)",
		 peer->host);
      bgp_notify_send (peer, BGP_NOTIFY_UPDATE_ERR, 
		       BGP_NOTIFY_UPDATE_MAL_ATTR);
      return -1;
    }

  /* Fetch attribute total length. */
  attribute_len = stream_getw (s);

  /* Attribute length check. */
  if (stream_pnt (s) + attribute_len > end)
    {
      zlog_warn ("%s [Error] Packet Error"
		 " (update packet attribute length overflow %d)",
		 peer->host, attribute_len);
      bgp_notify_send (peer, BGP_NOTIFY_UPDATE_ERR, 
		       BGP_NOTIFY_UPDATE_MAL_ATTR);
      return -1;
    }

  /* Parse attribute when it exists. */
  if (attribute_len)
    {
      ret = bgp_attr_parse (peer, &attr, attribute_len, 
			    &mp_update, &mp_withdraw);
      if (ret < 0)
	return -1;
    }

  /* Logging the attribute. */
  if (BGP_DEBUG (update, UPDATE_IN))
    {
      ret= bgp_dump_attr (peer, &attr, attrstr, BUFSIZ);

      if (ret)
	zlog (peer->log, LOG_INFO, "%s rcvd UPDATE w/ attr: %s",
	      peer->host, attrstr);
    }

  /* Network Layer Reachability Information. */
  update_len = end - stream_pnt (s);

  if (update_len)
    {
      /* Check NLRI packet format and prefix length. */
      ret = bgp_nlri_sanity_check (peer, AFI_IP, stream_pnt (s), update_len);
      if (ret < 0)
	return -1;

      /* Set NLRI portion to structure. */
      update.afi = AFI_IP;
      update.safi = SAFI_UNICAST;
      update.nlri = stream_pnt (s);
      update.length = update_len;
      stream_forward (s, update_len);
    }

  /* NLRI is processed only when the peer is configured specific
     Address Family and Subsequent Address Family. */
  if (peer->afc[AFI_IP][SAFI_UNICAST])
    {
      if (withdraw.length)
	bgp_nlri_parse (peer, NULL, &withdraw);

      if (update.length)
	{
	  /* We check well-known attribute only for IPv4 unicast
	     update. */
	  ret = bgp_attr_check (peer, &attr);
	  if (ret < 0)
	    return -1;

	  bgp_nlri_parse (peer, &attr, &update);
	}

      if (mp_update.length
	  && mp_update.afi == AFI_IP 
	  && mp_update.safi == SAFI_UNICAST)
	bgp_nlri_parse (peer, &attr, &mp_update);

      if (mp_withdraw.length
	  && mp_withdraw.afi == AFI_IP 
	  && mp_withdraw.safi == SAFI_UNICAST)
	bgp_nlri_parse (peer, NULL, &mp_withdraw);

      if (! attribute_len && ! withdraw_len)
	{
	  /* End-of-RIB received */
	  SET_FLAG (peer->af_sflags[AFI_IP][SAFI_UNICAST], PEER_STATUS_EOR_RECEIVED);

	  if (BGP_DEBUG (normal, NORMAL))
	    zlog_info ("neighbor %s sent End-of-RIB marker for IPv4 Unicast",
		       peer->host);

	  /* NSF delete stale route */
	  if (peer->nsf[AFI_IP][SAFI_UNICAST])
	    bgp_clear_stale_route (peer, AFI_IP, SAFI_UNICAST);
	}
    }
  if (peer->afc[AFI_IP][SAFI_MULTICAST])
    {
      if (mp_update.length
	  && mp_update.afi == AFI_IP 
	  && mp_update.safi == SAFI_MULTICAST)
	bgp_nlri_parse (peer, &attr, &mp_update);

      if (mp_withdraw.length
	  && mp_withdraw.afi == AFI_IP 
	  && mp_withdraw.safi == SAFI_MULTICAST)
	bgp_nlri_parse (peer, NULL, &mp_withdraw);

      if (! withdraw_len
	  && mp_withdraw.afi == AFI_IP
	  && mp_withdraw.safi == SAFI_MULTICAST
	  && mp_withdraw.length == 0)
	{
	  /* End-of-RIB received */
	  SET_FLAG (peer->af_sflags[AFI_IP][SAFI_MULTICAST], PEER_STATUS_EOR_RECEIVED);

	  if (BGP_DEBUG (normal, NORMAL))
	    zlog_info ("neighbor %s sent End-of-RIB marker for IPv4 Multicast",
		       peer->host);

	  /* NSF delete stale route */
	  if (peer->nsf[AFI_IP][SAFI_MULTICAST])
	    bgp_clear_stale_route (peer, AFI_IP, SAFI_MULTICAST);
	}
    }
  if (peer->afc[AFI_IP6][SAFI_UNICAST])
    {
      if (mp_update.length 
	  && mp_update.afi == AFI_IP6 
	  && mp_update.safi == SAFI_UNICAST)
	bgp_nlri_parse (peer, &attr, &mp_update);

      if (mp_withdraw.length 
	  && mp_withdraw.afi == AFI_IP6 
	  && mp_withdraw.safi == SAFI_UNICAST)
	bgp_nlri_parse (peer, NULL, &mp_withdraw);

      if (! withdraw_len
	  && mp_withdraw.afi == AFI_IP6
	  && mp_withdraw.safi == SAFI_UNICAST
	  && mp_withdraw.length == 0)
	{
	  /* End-of-RIB received */
	  SET_FLAG (peer->af_sflags[AFI_IP6][SAFI_UNICAST], PEER_STATUS_EOR_RECEIVED);

	  if (BGP_DEBUG (normal, NORMAL))
	    zlog_info ("neighbor %s sent End-of-RIB marker for IPv6 Unicast",
		       peer->host);

	  /* NSF delete stale route */
	  if (peer->nsf[AFI_IP6][SAFI_UNICAST])
	    bgp_clear_stale_route (peer, AFI_IP6, SAFI_UNICAST);
	}
    }
  if (peer->afc[AFI_IP6][SAFI_MULTICAST])
    {
      if (mp_update.length 
	  && mp_update.afi == AFI_IP6 
	  && mp_update.safi == SAFI_MULTICAST)
	bgp_nlri_parse (peer, &attr, &mp_update);

      if (mp_withdraw.length 
	  && mp_withdraw.afi == AFI_IP6 
	  && mp_withdraw.safi == SAFI_MULTICAST)
	bgp_nlri_parse (peer, NULL, &mp_withdraw);

      if (! withdraw_len
	  && mp_withdraw.afi == AFI_IP6
	  && mp_withdraw.safi == SAFI_MULTICAST
	  && mp_withdraw.length == 0)
	{
	  /* End-of-RIB received */
	  SET_FLAG (peer->af_sflags[AFI_IP6][SAFI_MULTICAST], PEER_STATUS_EOR_RECEIVED);

	  if (BGP_DEBUG (normal, NORMAL))
	    zlog_info ("neighbor %s sent End-of-RIB marker for IPv6 Multicast",
		       peer->host);

	  /* NSF delete stale route */
	  if (peer->nsf[AFI_IP6][SAFI_MULTICAST])
	    bgp_clear_stale_route (peer, AFI_IP6, SAFI_MULTICAST);
	}
    }
  if (peer->afc[AFI_IP][SAFI_MPLS_VPN])
    {
      if (mp_update.length 
	  && mp_update.afi == AFI_IP 
	  && mp_update.safi == BGP_SAFI_VPNV4)
	bgp_nlri_parse_vpnv4 (peer, &attr, &mp_update);

      if (mp_withdraw.length 
	  && mp_withdraw.afi == AFI_IP 
	  && mp_withdraw.safi == BGP_SAFI_VPNV4)
	bgp_nlri_parse_vpnv4 (peer, NULL, &mp_withdraw);

      if (! withdraw_len
	  && mp_withdraw.afi == AFI_IP
	  && mp_withdraw.safi == BGP_SAFI_VPNV4
	  && mp_withdraw.length == 0)
	{
	  /* End-of-RIB received */

	  if (BGP_DEBUG (normal, NORMAL))
	    zlog_info ("neighbor %s sent End-of-RIB marker for VPNv4 Unicast",
		       peer->host);
	}
    }

  /* Everything is done.  We unintern temporary structures which
     interned in bgp_attr_parse(). */
  if (attr.aspath)
    aspath_unintern (attr.aspath);
  if (attr.community)
    community_unintern (attr.community);
  if (attr.ecommunity)
    ecommunity_unintern (attr.ecommunity);
  if (attr.cluster)
    cluster_unintern (attr.cluster);
  if (attr.transit)
    transit_unintern (attr.transit);

  /* If peering is stopped due to some reason, do not generate BGP
     event.  */
  if (peer->status != Established)
    return 0;

  /* Increment packet counter. */
  peer->update_in++;
  peer->update_time = time (NULL);

  /* Generate BGP event. */
  BGP_EVENT_ADD (peer, Receive_UPDATE_message);

  return 0;
}

/* Notify message treatment function. */
void
bgp_notify_receive (struct peer *peer, bgp_size_t size)
{
  struct bgp_notify bgp_notify;

  if (peer->notify.data)
    {
      XFREE (MTYPE_TMP, peer->notify.data);
      peer->notify.data = NULL;
      peer->notify.length = 0;
    }

  bgp_notify.code = stream_getc (peer->ibuf);
  bgp_notify.subcode = stream_getc (peer->ibuf);
  bgp_notify.length = size - 2;
  bgp_notify.data = NULL;

  /* Preserv notify code and sub code. */
  peer->notify.code = bgp_notify.code;
  peer->notify.subcode = bgp_notify.subcode;
  /* For further diagnostic record returned Data. */
  if (bgp_notify.length)
    {
      peer->notify.length = size - 2;
      peer->notify.data = XMALLOC (MTYPE_TMP, size - 2);
      memcpy (peer->notify.data, stream_pnt (peer->ibuf), size - 2);
    }

  /* For debug */
  {
    int i;
    int first = 0;
    char c[4];

    if (bgp_notify.length)
      {
	bgp_notify.data = XMALLOC (MTYPE_TMP, bgp_notify.length * 3);
	for (i = 0; i < bgp_notify.length; i++)
	  if (first)
	    {
	      sprintf (c, " %02x", stream_getc (peer->ibuf));
	      strcat (bgp_notify.data, c);
	    }
	  else
	    {
	      first = 1;
	      sprintf (c, "%02x", stream_getc (peer->ibuf));
	      strcpy (bgp_notify.data, c);
	    }
      }

    bgp_notify_print(peer, &bgp_notify, "received");
    if (bgp_notify.data)
      XFREE (MTYPE_TMP, bgp_notify.data);
  }

  /* peer count update */
  peer->notify_in++;

  if (peer->status == Established)
    peer->last_reset = PEER_DOWN_NOTIFY_RECEIVED;
  /* We have to check for Notify with Unsupported Optional Parameter.
     in that case we fallback to open without the capability option.
     But this done in bgp_stop. We just mark it here to avoid changing
     the fsm tables.  */
  if (bgp_notify.code == BGP_NOTIFY_OPEN_ERR &&
      bgp_notify.subcode == BGP_NOTIFY_OPEN_UNSUP_PARAM )
    UNSET_FLAG (peer->sflags, PEER_STATUS_CAPABILITY_OPEN);

  /* Also apply to Unsupported Capability until remote router support
     capability. */
  if (bgp_notify.code == BGP_NOTIFY_OPEN_ERR &&
      bgp_notify.subcode == BGP_NOTIFY_OPEN_UNSUP_CAPBL)
    UNSET_FLAG (peer->sflags, PEER_STATUS_CAPABILITY_OPEN);

  BGP_EVENT_ADD (peer, Receive_NOTIFICATION_message);
}

/* Keepalive treatment function -- get keepalive send keepalive */
void
bgp_keepalive_receive (struct peer *peer, bgp_size_t size)
{
  if (BGP_DEBUG (keepalive, KEEPALIVE))  
    zlog_info ("%s received KEEPALIVE, length (excl. header) %d", peer->host, size); 
  
  BGP_EVENT_ADD (peer, Receive_KEEPALIVE_message);
}

/* Route refresh message is received. */
void
bgp_route_refresh_receive (struct peer *peer, bgp_size_t size)
{
  afi_t afi;
  safi_t safi;
  u_char reserved;
  struct stream *s;

  /* If peer does not have the capability, send notification. */
  if (! CHECK_FLAG (peer->cap, PEER_CAP_REFRESH_ADV))
    {
      plog_err (peer->log, "%s [Error] BGP route refresh is not enabled",
		peer->host);
      bgp_notify_send (peer,
		       BGP_NOTIFY_HEADER_ERR,
		       BGP_NOTIFY_HEADER_BAD_MESTYPE);
      return;
    }

  /* Status must be Established. */
  if (peer->status != Established) 
    {
      plog_err (peer->log,
		"%s [Error] Route refresh packet received under status %s",
		peer->host, LOOKUP (bgp_status_msg, peer->status));
      bgp_notify_send (peer, BGP_NOTIFY_FSM_ERR, 0);
      return;
    }

  s = peer->ibuf;
  
  /* Parse packet. */
  afi = stream_getw (s);
  reserved = stream_getc (s);
  safi = stream_getc (s);

  if (BGP_DEBUG (normal, NORMAL))
    zlog_info ("%s rcvd REFRESH_REQ for afi/safi: %d/%d",
	       peer->host, afi, safi);

  /* Check AFI and SAFI. */
  if ((afi != AFI_IP && afi != AFI_IP6)
      || (safi != SAFI_UNICAST && safi != SAFI_MULTICAST
	  && safi != BGP_SAFI_VPNV4))
    {
      if (BGP_DEBUG (normal, NORMAL))
	{
	  zlog_info ("%s REFRESH_REQ for unrecognized afi/safi: %d/%d - ignored",
		     peer->host, afi, safi);
	}
      return;
    }

  /* Adjust safi code. */
  if (safi == BGP_SAFI_VPNV4)
    safi = SAFI_MPLS_VPN;

  if (size != BGP_MSG_ROUTE_REFRESH_MIN_SIZE - BGP_HEADER_SIZE)
    {
      u_char *end;
      u_char when_to_refresh;
      u_char orf_type;
      u_int16_t orf_len;

      if (size - (BGP_MSG_ROUTE_REFRESH_MIN_SIZE - BGP_HEADER_SIZE) < 5)
        {
          zlog_info ("%s ORF route refresh length error", peer->host);
          bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
          return;
        }

      when_to_refresh = stream_getc (s);
      end = stream_pnt (s) + (size - 5);

      while (stream_pnt (s) < end)
	{
	  orf_type = stream_getc (s); 
	  orf_len = stream_getw (s);

	  if (orf_type == ORF_TYPE_PREFIX
	      || orf_type == ORF_TYPE_PREFIX_OLD)
	    {
	      u_char *p_pnt = stream_pnt (s);
	      u_char *p_end = stream_pnt (s) + orf_len;
	      struct orf_prefix orfp;
	      u_char common = 0;
	      u_int32_t seq;
	      int psize;
	      char name[BUFSIZ];
	      char buf[BUFSIZ];
	      int ret;

	      if (BGP_DEBUG (normal, NORMAL))
		{
		  zlog_info ("%s rcvd Prefixlist ORF(%d) length %d",
			     peer->host, orf_type, orf_len);
		}

	      /* ORF prefix-list name */
	      sprintf (name, "%s.%d.%d", peer->host, afi, safi);

	      while (p_pnt < p_end)
		{
		  memset (&orfp, 0, sizeof (struct orf_prefix));
		  common = *p_pnt++;
		  if (common & ORF_COMMON_PART_REMOVE_ALL)
		    {
		      if (BGP_DEBUG (normal, NORMAL))
			zlog_info ("%s rcvd Remove-All pfxlist ORF request", peer->host);
		      prefix_bgp_orf_remove_all (name);
		      break;
		    }
		  memcpy (&seq, p_pnt, sizeof (u_int32_t));
		  p_pnt += sizeof (u_int32_t);
		  orfp.seq = ntohl (seq);
		  orfp.ge = *p_pnt++;
		  orfp.le = *p_pnt++;
		  orfp.p.prefixlen = *p_pnt++;
		  orfp.p.family = afi2family (afi);
		  psize = PSIZE (orfp.p.prefixlen);
		  memcpy (&orfp.p.u.prefix, p_pnt, psize);
		  p_pnt += psize;

		  if (BGP_DEBUG (normal, NORMAL))
		    zlog_info ("%s rcvd %s %s seq %u %s/%d ge %d le %d",
			       peer->host,
			       (common & ORF_COMMON_PART_REMOVE ? "Remove" : "Add"), 
			       (common & ORF_COMMON_PART_DENY ? "deny" : "permit"),
			       orfp.seq, 
			       inet_ntop (orfp.p.family, &orfp.p.u.prefix, buf, BUFSIZ),
			       orfp.p.prefixlen, orfp.ge, orfp.le);

		  ret = prefix_bgp_orf_set (name, afi, &orfp,
				 (common & ORF_COMMON_PART_DENY ? 0 : 1 ),
				 (common & ORF_COMMON_PART_REMOVE ? 0 : 1));

		  if (ret != CMD_SUCCESS)
		    {
		      if (BGP_DEBUG (normal, NORMAL))
			zlog_info ("%s Received misformatted prefixlist ORF. Remove All pfxlist", peer->host);
		      prefix_bgp_orf_remove_all (name);
		      break;
		    }
		}
	      peer->orf_plist[afi][safi] =
			 prefix_list_lookup (AFI_ORF_PREFIX, name);
	    }
	  stream_forward (s, orf_len);
	}
      if (BGP_DEBUG (normal, NORMAL))
	zlog_info ("%s rcvd Refresh %s ORF request", peer->host,
		   when_to_refresh == REFRESH_DEFER ? "Defer" : "Immediate");
      if (when_to_refresh == REFRESH_DEFER)
	return;
    }

  /* First update is deferred until ORF or ROUTE-REFRESH is received */
  if (CHECK_FLAG (peer->af_sflags[afi][safi], PEER_STATUS_ORF_WAIT_REFRESH))
    UNSET_FLAG (peer->af_sflags[afi][safi], PEER_STATUS_ORF_WAIT_REFRESH);

  /* Perform route refreshment to the peer */
  bgp_announce_route (peer, afi, safi);
}

int
bgp_capability_msg_parse (struct peer *peer, u_char *pnt, bgp_size_t length)
{
  u_char *end;
  struct capability cap;
  u_char action;
  struct bgp *bgp;
  afi_t afi;
  safi_t safi;

  bgp = peer->bgp;
  end = pnt + length;

  while (pnt < end)
    {
      /* We need at least action, capability code and capability length. */
      if (pnt + 3 > end)
        {
          zlog_info ("%s Capability length error", peer->host);
          bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
          return -1;
        }

      action = *pnt;

      /* Fetch structure to the byte stream. */
      memcpy (&cap, pnt + 1, sizeof (struct capability));

      /* Action value check.  */
      if (action != CAPABILITY_ACTION_SET
	  && action != CAPABILITY_ACTION_UNSET)
        {
          zlog_info ("%s Capability Action Value error %d",
		     peer->host, action);
          bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
          return -1;
        }

      if (BGP_DEBUG (normal, NORMAL))
	zlog_info ("%s CAPABILITY has action: %d, code: %u, length %u",
		   peer->host, action, cap.code, cap.length);

      /* Capability length check. */
      if (pnt + (cap.length + 3) > end)
        {
          zlog_info ("%s Capability length error", peer->host);
          bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
          return -1;
        }

      /* We know MP Capability Code. */
      if (cap.code == CAPABILITY_CODE_MP)
        {
	  afi = ntohs (cap.mpc.afi);
	  safi = cap.mpc.safi;

          /* Ignore capability when override-capability is set. */
          if (CHECK_FLAG (peer->flags, PEER_FLAG_OVERRIDE_CAPABILITY))
	    continue;

	  /* Address family check.  */
	  if ((afi == AFI_IP 
	       || afi == AFI_IP6)
	      && (safi == SAFI_UNICAST 
		  || safi == SAFI_MULTICAST 
		  || safi == BGP_SAFI_VPNV4))
	    {
	      if (BGP_DEBUG (normal, NORMAL))
		zlog_info ("%s CAPABILITY has %s MP_EXT CAP for afi/safi: %u/%u",
			   peer->host,
			   action == CAPABILITY_ACTION_SET 
			   ? "Advertising" : "Removing",
			   ntohs(cap.mpc.afi) , cap.mpc.safi);
		  
	      /* Adjust safi code. */
	      if (safi == BGP_SAFI_VPNV4)
		safi = SAFI_MPLS_VPN;
	      
	      if (action == CAPABILITY_ACTION_SET)
		{
		  peer->afc_recv[afi][safi] = 1;
		  if (peer->afc[afi][safi])
		    {
		      peer->afc_nego[afi][safi] = 1;
		      bgp_announce_route (peer, afi, safi);
		    }
		}
	      else
		{
		  peer->afc_recv[afi][safi] = 0;
		  peer->afc_nego[afi][safi] = 0;

		  if (peer_active_nego (peer))
		    {
		      bgp_clear_route (peer, afi, safi);
		      peer->synctime[afi][safi] = 0;
		      BGP_TIMER_OFF (peer->t_routeadv[afi][safi]);
		      peer->af_sflags[afi][safi] = 0;
		    }
		  else
		    BGP_EVENT_ADD (peer, BGP_Stop);
		} 
	    }
        }
      else
        {
          zlog_warn ("%s unrecognized capability code: %d - ignored",
                     peer->host, cap.code);
        }
      pnt += cap.length + 3;
    }
  return 0;
}

/* Dynamic Capability is received. */
void
bgp_capability_receive (struct peer *peer, bgp_size_t size)
{
  u_char *pnt;
  int ret;

  /* Fetch pointer. */
  pnt = stream_pnt (peer->ibuf);

  if (BGP_DEBUG (normal, NORMAL))
    zlog_info ("%s rcv CAPABILITY", peer->host);

  /* If peer does not have the capability, send notification. */
  if (! CHECK_FLAG (peer->cap, PEER_CAP_DYNAMIC_ADV))
    {
      plog_err (peer->log, "%s [Error] BGP dynamic capability is not enabled",
		peer->host);
      bgp_notify_send (peer,
		       BGP_NOTIFY_HEADER_ERR,
		       BGP_NOTIFY_HEADER_BAD_MESTYPE);
      return;
    }

  /* Status must be Established. */
  if (peer->status != Established)
    {
      plog_err (peer->log,
		"%s [Error] Dynamic capability packet received under status %s", peer->host, LOOKUP (bgp_status_msg, peer->status));
      bgp_notify_send (peer, BGP_NOTIFY_FSM_ERR, 0);
      return;
    }

  /* Parse packet. */
  ret = bgp_capability_msg_parse (peer, pnt, size);
}

/* BGP read utility function. */
int
bgp_read_packet (struct peer *peer)
{
  int nbytes;
  int readsize;

  readsize = peer->packet_size - peer->ibuf->putp;

  /* If size is zero then return. */
  if (! readsize)
    return 0;

  /* Read packet from fd. */
  nbytes = stream_read_unblock (peer->ibuf, peer->fd, readsize);

  /* If read byte is smaller than zero then error occured. */
  if (nbytes < 0) 
    {
      if (errno == EAGAIN)
	return -1;

      plog_err (peer->log, "%s [Error] bgp_read_packet error: %s",
		 peer->host, strerror (errno));

      if (peer->status == Established) 
	{
	  if (CHECK_FLAG (peer->sflags, PEER_STATUS_NSF_MODE))
	    {
	      peer->last_reset = PEER_DOWN_NSF_CLOSE_SESSION;
	      SET_FLAG (peer->sflags, PEER_STATUS_NSF_WAIT);
	    }
	  else
	    peer->last_reset = PEER_DOWN_CLOSE_SESSION;
	}

      BGP_EVENT_ADD (peer, TCP_fatal_error);
      return -1;
    }  

  /* When read byte is zero : clear bgp peer and return */
  if (nbytes == 0) 
    {
      if (BGP_DEBUG (events, EVENTS))
	plog_info (peer->log, "%s [Event] BGP connection closed fd %d",
		   peer->host, peer->fd);

      if (peer->status == Established) 
	{
	  if (CHECK_FLAG (peer->sflags, PEER_STATUS_NSF_MODE))
	    {
	      peer->last_reset = PEER_DOWN_NSF_CLOSE_SESSION;
	      SET_FLAG (peer->sflags, PEER_STATUS_NSF_WAIT);
	    }
	  else
	    peer->last_reset = PEER_DOWN_CLOSE_SESSION;
	}

      BGP_EVENT_ADD (peer, TCP_connection_closed);
      return -1;
    }

  /* We read partial packet. */
  if (peer->ibuf->putp != peer->packet_size)
    return -1;

  return 0;
}

/* Marker check. */
int
bgp_marker_all_one (struct stream *s, int length)
{
  int i;

  for (i = 0; i < length; i++)
    if (s->data[i] != 0xff)
      return 0;

  return 1;
}

/* Starting point of packet process function. */
int
bgp_read (struct thread *thread)
{
  int ret;
  u_char type = 0;
  struct peer *peer;
  bgp_size_t size;
  char notify_data_length[2];

  /* Yes first of all get peer pointer. */
  peer = THREAD_ARG (thread);
  peer->t_read = NULL;

  /* For non-blocking IO check. */
  if (peer->status == Connect)
    {
      bgp_connect_check (peer);
      goto done;
    }
  else
    {
      if (peer->fd < 0)
	{
	  zlog_err ("bgp_read peer's fd is negative value %d", peer->fd);
	  return -1;
	}
      BGP_READ_ON (peer->t_read, bgp_read, peer->fd);
    }

  /* Read packet header to determine type of the packet */
  if (peer->packet_size == 0)
    peer->packet_size = BGP_HEADER_SIZE;

  if (peer->ibuf->putp < BGP_HEADER_SIZE)
    {
      ret = bgp_read_packet (peer);

      /* Header read error or partial read packet. */
      if (ret < 0) 
	goto done;

      /* Get size and type. */
      stream_forward (peer->ibuf, BGP_MARKER_SIZE);
      memcpy (notify_data_length, stream_pnt (peer->ibuf), 2);
      size = stream_getw (peer->ibuf);
      type = stream_getc (peer->ibuf);

      if (BGP_DEBUG (normal, NORMAL))
	if (type && type != BGP_MSG_UPDATE && type != BGP_MSG_KEEPALIVE)
	zlog_info ("%s rcv message type %d, length (excl. header) %d",
		   peer->host, type, size - BGP_HEADER_SIZE);

      /* Marker check */
      if (type == BGP_MSG_OPEN
	  && ! bgp_marker_all_one (peer->ibuf, BGP_MARKER_SIZE))
	{
	  bgp_notify_send (peer,
			   BGP_NOTIFY_HEADER_ERR, 
			   BGP_NOTIFY_HEADER_NOT_SYNC);
	  goto done;
	}

      /* BGP type check. */
      if (type != BGP_MSG_OPEN && type != BGP_MSG_UPDATE 
	  && type != BGP_MSG_NOTIFY && type != BGP_MSG_KEEPALIVE 
	  && type != BGP_MSG_ROUTE_REFRESH_NEW
	  && type != BGP_MSG_ROUTE_REFRESH_OLD
	  && type != BGP_MSG_CAPABILITY)
	{
	  if (BGP_DEBUG (normal, NORMAL))
	    plog_err (peer->log,
		      "%s unknown message type 0x%02x",
		      peer->host, type);
	  bgp_notify_send_with_data (peer,
				     BGP_NOTIFY_HEADER_ERR,
			 	     BGP_NOTIFY_HEADER_BAD_MESTYPE,
				     &type, 1);
	  goto done;
	}
      /* Mimimum packet length check. */
      if ((size < BGP_HEADER_SIZE)
	  || (size > BGP_MAX_PACKET_SIZE)
	  || (type == BGP_MSG_OPEN && size < BGP_MSG_OPEN_MIN_SIZE)
	  || (type == BGP_MSG_UPDATE && size < BGP_MSG_UPDATE_MIN_SIZE)
	  || (type == BGP_MSG_NOTIFY && size < BGP_MSG_NOTIFY_MIN_SIZE)
	  || (type == BGP_MSG_KEEPALIVE && size != BGP_MSG_KEEPALIVE_MIN_SIZE)
	  || (type == BGP_MSG_ROUTE_REFRESH_NEW && size < BGP_MSG_ROUTE_REFRESH_MIN_SIZE)
	  || (type == BGP_MSG_ROUTE_REFRESH_OLD && size < BGP_MSG_ROUTE_REFRESH_MIN_SIZE)
	  || (type == BGP_MSG_CAPABILITY && size < BGP_MSG_CAPABILITY_MIN_SIZE))
	{
	  if (BGP_DEBUG (normal, NORMAL))
	    plog_err (peer->log,
		      "%s bad message length - %d for %s",
		      peer->host, size, 
		      type == 128 ? "ROUTE-REFRESH" :
		      bgp_type_str[(int) type]);
	  bgp_notify_send_with_data (peer,
				     BGP_NOTIFY_HEADER_ERR,
			  	     BGP_NOTIFY_HEADER_BAD_MESLEN,
				     notify_data_length, 2);
	  goto done;
	}

      /* Adjust size to message length. */
      peer->packet_size = size;
    }

  ret = bgp_read_packet (peer);
  if (ret < 0) 
    goto done;

  /* Get size and type again. */
  size = stream_getw_from (peer->ibuf, BGP_MARKER_SIZE);
  type = stream_getc_from (peer->ibuf, BGP_MARKER_SIZE + 2);

  /* BGP packet dump function. */
  bgp_dump_packet (peer, type, peer->ibuf);
  
  size = (peer->packet_size - BGP_HEADER_SIZE);

  /* Read rest of the packet and call each sort of packet routine */
  switch (type) 
    {
    case BGP_MSG_OPEN:
      peer->open_in++;
      bgp_open_receive (peer, size);
      break;
    case BGP_MSG_UPDATE:
      peer->readtime = time(NULL);    /* Last read timer reset */
      bgp_update_receive (peer, size);
      break;
    case BGP_MSG_NOTIFY:
      bgp_notify_receive (peer, size);
      break;
    case BGP_MSG_KEEPALIVE:
      peer->readtime = time(NULL);    /* Last read timer reset */
      bgp_keepalive_receive (peer, size);
      break;
    case BGP_MSG_ROUTE_REFRESH_NEW:
    case BGP_MSG_ROUTE_REFRESH_OLD:
      peer->refresh_in++;
      bgp_route_refresh_receive (peer, size);
      break;
    case BGP_MSG_CAPABILITY:
      peer->dynamic_cap_in++;
      bgp_capability_receive (peer, size);
      break;
    }

  /* Clear input buffer. */
  peer->packet_size = 0;
  if (peer->ibuf)
    stream_reset (peer->ibuf);

 done:
  if (CHECK_FLAG (peer->sflags, PEER_STATUS_ACCEPT_PEER))
    {
      if (BGP_DEBUG (events, EVENTS))
	zlog_info ("%s [Event] Accepting BGP peer delete", peer->host);
      peer_delete (peer);
    }
  return 0;
}
