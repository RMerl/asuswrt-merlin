/*
 *
 * Copyright (C) 2000  Robert Olsson.
 * Swedish University of Agricultural Sciences
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

/* 
 * This work includes work with the following copywrite:
 *
 * Copyright (C) 1997, 2000 Kunihiro Ishiguro
 *
 */

/* 
 * Thanks to Jens Låås at Swedish University of Agricultural Sciences
 * for reviewing and tests.
 */


#include <zebra.h>

#ifdef HAVE_IRDP

#include "if.h"
#include "vty.h"
#include "sockunion.h"
#include "prefix.h"
#include "command.h"
#include "memory.h"
#include "stream.h"
#include "ioctl.h"
#include "connected.h"
#include "log.h"
#include "zclient.h"
#include "thread.h"
#include "zebra/interface.h"
#include "zebra/rtadv.h"
#include "zebra/rib.h"
#include "zebra/zserv.h"
#include "zebra/redistribute.h"
#include "zebra/irdp.h"
#include <netinet/ip_icmp.h>

#include "if.h"
#include "sockunion.h"
#include "log.h"

/* GLOBAL VARS */

/* Master of threads. */
extern struct thread_master *master;
struct thread *t_irdp_raw;

/* Timer interval of irdp. */
int irdp_timer_interval = IRDP_DEFAULT_INTERVAL;

int irdp_read_raw(struct thread *r);
int in_cksum (void *ptr, int nbytes);
extern int irdp_sock;
void send_packet(struct interface *ifp, 
		 struct stream *s,
		 u_int32_t dst,
		 struct prefix *p,
		 u_int32_t ttl);

void irdp_if_init ();

char *
inet_2a(u_int32_t a, char *b)
{
  sprintf(b, "%u.%u.%u.%u", 
          (a    ) & 0xFF,
          (a>> 8) & 0xFF, 
          (a>>16) & 0xFF, 
          (a>>24) & 0xFF);
  return  b;
}

int
irdp_sock_init (void)
{
  int ret, i;

  irdp_sock = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP);
  if (irdp_sock < 0) {
    zlog_warn ("IRDP: can't create irdp socket %s", strerror(errno));
    return irdp_sock;
  };
  
  i = 1;
  ret = setsockopt (irdp_sock, IPPROTO_IP, IP_TTL, 
                        (void *) &i, sizeof (i));
  if (ret < 0) {
    zlog_warn ("IRDP: can't do irdp sockopt %s", strerror(errno));
    return ret;
  };
  
  i = 1;
  ret = setsockopt (irdp_sock, IPPROTO_IP, IP_PKTINFO, 
                        (void *) &i, sizeof (i));
  if (ret < 0) {
    zlog_warn ("IRDP: can't do irdp sockopt %s", strerror(errno));
    return ret;
  };

  t_irdp_raw = thread_add_read (master, irdp_read_raw, NULL, irdp_sock); 

  return irdp_sock;
}


int get_pref(struct irdp_interface *irdp, struct prefix *p)
{
  listnode node;
  struct Adv *adv;

  /* Use default preference or use the override pref */
  
  if( irdp->AdvPrefList == NULL ) return irdp->Preference;
  
  for (node = listhead (irdp->AdvPrefList); node; nextnode (node)) {
    adv = getdata (node);
    if( p->u.prefix4.s_addr == adv->ip.s_addr )
      return adv->pref;
  }
  return irdp->Preference;
}

/* Make ICMP Router Advertisement Message. */
int make_advertisement_packet (struct interface *ifp, 
			       struct prefix *p,
			       struct stream *s)
{
  struct zebra_if *zi=ifp->info;
  struct irdp_interface *irdp=&zi->irdp;
  int size;
  int pref;
  u_int16_t checksum;

  pref =  get_pref(irdp, p);

  stream_putc (s, ICMP_ROUTERADVERT); /* Type. */
  stream_putc (s, 0);		/* Code. */
  stream_putw (s, 0);		/* Checksum. */
  stream_putc (s, 1);		/* Num address. */
  stream_putc (s, 2);		/* Address Entry Size. */

  if(irdp->flags & IF_SHUTDOWN)  
    stream_putw (s, 0);
  else 
    stream_putw (s, irdp->Lifetime);

  stream_putl (s, htonl(p->u.prefix4.s_addr)); /* Router address. */
  stream_putl (s, pref);

  /* in_cksum return network byte order value */
  size = 16;
  checksum = in_cksum (s->data, size);
  stream_putw_at (s, 2, htons(checksum));

  return size;
}

void irdp_send(struct interface *ifp, 
	       struct prefix *p, 
	       struct stream *s)
{
  struct zebra_if *zi=ifp->info;
  struct irdp_interface *irdp=&zi->irdp;
  u_int32_t dst;
  u_int32_t ttl=1;

  if (! (ifp->flags & IFF_UP)) return; 

  if (irdp->flags & IF_BROADCAST) 
    dst =INADDR_BROADCAST ;
  else 
    dst = htonl(INADDR_ALLHOSTS_GROUP);

  if(irdp->flags & IF_DEBUG_MESSAGES) 
    zlog_warn("IRDP: TX Advert on %s %s/%d Holdtime=%d Preference=%d", 
	      ifp->name,
	      inet_ntoa(p->u.prefix4), 
	      p->prefixlen,
	      irdp->flags & IF_SHUTDOWN? 0 : irdp->Lifetime,
	      get_pref(irdp, p));

  send_packet (ifp, s, dst, p, ttl);
}

void irdp_advertisement (struct interface *ifp, 
		   struct prefix *p)
{
  struct stream *s;
  s = stream_new (128);
  make_advertisement_packet (ifp, p, s);
  irdp_send(ifp, p, s); 
}

int irdp_send_thread(struct thread *t_advert)
{
  u_int32_t timer, tmp;
  struct interface *ifp = THREAD_ARG (t_advert);
  struct zebra_if *zi=ifp->info;
  struct irdp_interface *irdp=&zi->irdp;
  struct prefix *p;
  listnode node;
  struct connected *ifc;

  irdp->flags &= ~IF_SOLICIT;

  if(ifp->connected) 
    for (node = listhead (ifp->connected); node; nextnode (node)) {
      ifc = getdata (node);

      p = ifc->address;

      irdp_advertisement(ifp, p);
      irdp->irdp_sent++;

    }

  tmp = irdp->MaxAdvertInterval-irdp->MinAdvertInterval;
  timer =  (random () % tmp ) + 1;
  timer = irdp->MinAdvertInterval + timer;

  if(irdp->irdp_sent <  MAX_INITIAL_ADVERTISEMENTS &&
     timer > MAX_INITIAL_ADVERT_INTERVAL ) 
	  timer= MAX_INITIAL_ADVERT_INTERVAL;

  if(irdp->flags & IF_DEBUG_MISC)
    zlog_warn("IRDP: New timer for %s set to %u\n", ifp->name, timer);

  irdp->t_advertise = thread_add_timer(master, irdp_send_thread, ifp, timer);
  return 0;
}

void irdp_advert_off(struct interface *ifp)
{
  struct zebra_if *zi=ifp->info;
  struct irdp_interface *irdp=&zi->irdp;
  listnode node;
  int i;
  struct connected *ifc;
  struct prefix *p;

  if(irdp->t_advertise)  thread_cancel(irdp->t_advertise);
  irdp->t_advertise = NULL;
  
  if(ifp->connected) 
    for (node = listhead (ifp->connected); node; nextnode (node)) {
      ifc = getdata (node);

      p = ifc->address;

      /* Output some packets with Lifetime 0 
	 we should add a wait...
      */

      for(i=0; i< IRDP_LAST_ADVERT_MESSAGES; i++) {

	irdp->irdp_sent++;
	irdp_advertisement(ifp, p);
      }
    }
}


void process_solicit (struct interface *ifp)
{
  struct zebra_if *zi=ifp->info;
  struct irdp_interface *irdp=&zi->irdp;
  u_int32_t timer;

  /* When SOLICIT is active we reject further incoming solicits 
     this keeps down the answering rate so we don't have think
     about DoS attacks here. */

  if( irdp->flags & IF_SOLICIT) return;

  irdp->flags |= IF_SOLICIT;
  if(irdp->t_advertise)  thread_cancel(irdp->t_advertise);
  irdp->t_advertise = NULL;

  timer =  (random () % MAX_RESPONSE_DELAY) + 1;

  irdp->t_advertise = thread_add_timer(master, 
				       irdp_send_thread, 
				       ifp, 
				       timer);
}

void irdp_finish()
{

  listnode node;
  struct interface *ifp;
  struct zebra_if *zi;
  struct irdp_interface *irdp;

  zlog_warn("IRDP: Received shutdown notification.");
  
  for (node = listhead (iflist); node; node = nextnode (node))
    {
      ifp = getdata(node);
      zi= ifp->info;
      if(! zi) continue;
      irdp = &zi->irdp;
      if(!irdp) continue;

      if(irdp->flags & IF_ACTIVE ) {
	irdp->flags |= IF_SHUTDOWN;
	irdp_advert_off(ifp);
      }
    }
}

void irdp_init()
{
  irdp_sock_init();
  irdp_if_init ();
}



#endif /* HAVE_IRDP */




