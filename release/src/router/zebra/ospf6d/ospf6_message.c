/*
 * Copyright (C) 2003 Yasuhiro Ohara
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA.  
 */

#include <zebra.h>

#include "memory.h"
#include "log.h"
#include "vty.h"
#include "command.h"
#include "thread.h"
#include "linklist.h"

#include "ospf6_proto.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_network.h"
#include "ospf6_message.h"

#include "ospf6_top.h"
#include "ospf6_area.h"
#include "ospf6_neighbor.h"
#include "ospf6_interface.h"

#include "ospf6_flood.h"
#include "ospf6d.h"

unsigned char conf_debug_ospf6_message[6] = {0x03, 0, 0, 0, 0, 0};
char *ospf6_message_type_str[] =
  { "Unknown", "Hello", "DbDesc", "LSReq", "LSUpdate", "LSAck" };

/* print functions */

static void
ospf6_header_print (struct ospf6_header *oh)
{
  char router_id[16], area_id[16];
  inet_ntop (AF_INET, &oh->router_id, router_id, sizeof (router_id));
  inet_ntop (AF_INET, &oh->area_id, area_id, sizeof (area_id));

  zlog_info ("    OSPFv%d Type:%d Len:%hu Router-ID:%s",
             oh->version, oh->type, ntohs (oh->length), router_id);
  zlog_info ("    Area-ID:%s Cksum:%hx Instance-ID:%d",
             area_id, ntohs (oh->checksum), oh->instance_id);
}

void
ospf6_hello_print (struct ospf6_header *oh)
{
  struct ospf6_hello *hello;
  char options[16];
  char drouter[16], bdrouter[16], neighbor[16];
  char *p;

  ospf6_header_print (oh);
  assert (oh->type == OSPF6_MESSAGE_TYPE_HELLO);

  hello = (struct ospf6_hello *)
    ((caddr_t) oh + sizeof (struct ospf6_header));

  inet_ntop (AF_INET, &hello->drouter, drouter, sizeof (drouter));
  inet_ntop (AF_INET, &hello->bdrouter, bdrouter, sizeof (bdrouter));
  ospf6_options_printbuf (hello->options, options, sizeof (options));

  zlog_info ("    I/F-Id:%ld Priority:%d Option:%s",
             (u_long) ntohl (hello->interface_id), hello->priority, options);
  zlog_info ("    HelloInterval:%hu DeadInterval:%hu",
             ntohs (hello->hello_interval), ntohs (hello->dead_interval));
  zlog_info ("    DR:%s BDR:%s", drouter, bdrouter);

  for (p = (char *) ((caddr_t) hello + sizeof (struct ospf6_hello));
       p + sizeof (u_int32_t) <= OSPF6_MESSAGE_END (oh);
       p += sizeof (u_int32_t))
    {
      inet_ntop (AF_INET, (void *) p, neighbor, sizeof (neighbor));
      zlog_info ("    Neighbor: %s", neighbor);
    }

  if (p != OSPF6_MESSAGE_END (oh))
    zlog_info ("Trailing garbage exists");
}

void
ospf6_dbdesc_print (struct ospf6_header *oh)
{
  struct ospf6_dbdesc *dbdesc;
  char options[16];
  char *p;

  ospf6_header_print (oh);
  assert (oh->type == OSPF6_MESSAGE_TYPE_DBDESC);

  dbdesc = (struct ospf6_dbdesc *)
    ((caddr_t) oh + sizeof (struct ospf6_header));

  ospf6_options_printbuf (dbdesc->options, options, sizeof (options));

  zlog_info ("    MBZ: %#x Option: %s IfMTU: %hu",
             dbdesc->reserved1, options, ntohs (dbdesc->ifmtu));
  zlog_info ("    MBZ: %#x Bits: %s%s%s SeqNum: %#lx",
             dbdesc->reserved2,
             (CHECK_FLAG (dbdesc->bits, OSPF6_DBDESC_IBIT) ? "I" : "-"),
             (CHECK_FLAG (dbdesc->bits, OSPF6_DBDESC_MBIT) ? "M" : "-"),
             (CHECK_FLAG (dbdesc->bits, OSPF6_DBDESC_MSBIT) ? "m" : "s"),
             (u_long) ntohl (dbdesc->seqnum));

  for (p = (char *) ((caddr_t) dbdesc + sizeof (struct ospf6_dbdesc));
       p + sizeof (struct ospf6_lsa_header) <= OSPF6_MESSAGE_END (oh);
       p += sizeof (struct ospf6_lsa_header))
    ospf6_lsa_header_print_raw ((struct ospf6_lsa_header *) p);

  if (p != OSPF6_MESSAGE_END (oh))
    zlog_info ("Trailing garbage exists");
}

void
ospf6_lsreq_print (struct ospf6_header *oh)
{
  char id[16], adv_router[16];
  char *p;

  ospf6_header_print (oh);
  assert (oh->type == OSPF6_MESSAGE_TYPE_LSREQ);

  for (p = (char *) ((caddr_t) oh + sizeof (struct ospf6_header));
       p + sizeof (struct ospf6_lsreq_entry) <= OSPF6_MESSAGE_END (oh);
       p += sizeof (struct ospf6_lsreq_entry))
    {
      struct ospf6_lsreq_entry *e = (struct ospf6_lsreq_entry *) p;
      inet_ntop (AF_INET, &e->adv_router, adv_router, sizeof (adv_router));
      inet_ntop (AF_INET, &e->id, id, sizeof (id));
      zlog_info ("    [%s Id:%s Adv:%s]",
                 ospf6_lstype_name (e->type), id, adv_router);
    }

  if (p != OSPF6_MESSAGE_END (oh))
    zlog_info ("Trailing garbage exists");
}

void
ospf6_lsupdate_print (struct ospf6_header *oh)
{
  struct ospf6_lsupdate *lsupdate;
  u_long num;
  char *p;

  ospf6_header_print (oh);
  assert (oh->type == OSPF6_MESSAGE_TYPE_LSUPDATE);

  lsupdate = (struct ospf6_lsupdate *)
    ((caddr_t) oh + sizeof (struct ospf6_header));

  num = ntohl (lsupdate->lsa_number);
  zlog_info ("    Number of LSA: %ld", num);

  for (p = (char *) ((caddr_t) lsupdate + sizeof (struct ospf6_lsupdate));
       p < OSPF6_MESSAGE_END (oh) &&
       p + OSPF6_LSA_SIZE (p) <= OSPF6_MESSAGE_END (oh);
       p += OSPF6_LSA_SIZE (p))
    {
      ospf6_lsa_header_print_raw ((struct ospf6_lsa_header *) p);
      if (OSPF6_LSA_SIZE (p) < sizeof (struct ospf6_lsa_header))
        {
          zlog_info ("    Malformed LSA length, quit printing");
          break;
        }
    }

  if (p != OSPF6_MESSAGE_END (oh))
    {
      char buf[32];

      int num = 0;
      memset (buf, 0, sizeof (buf));

      zlog_info ("    Trailing garbage exists");
      while (p < OSPF6_MESSAGE_END (oh))
        {
          snprintf (buf, sizeof (buf), "%s %2x", buf, *p++);
          num++;
          if (num == 8)
            {
              zlog_info ("    %s", buf);
              memset (buf, 0, sizeof (buf));
              num = 0;
            }
        }
      if (num)
        zlog_info ("    %s", buf);
    }
}

void
ospf6_lsack_print (struct ospf6_header *oh)
{
  char *p;

  ospf6_header_print (oh);
  assert (oh->type == OSPF6_MESSAGE_TYPE_LSACK);

  for (p = (char *) ((caddr_t) oh + sizeof (struct ospf6_header));
       p + sizeof (struct ospf6_lsa_header) <= OSPF6_MESSAGE_END (oh);
       p += sizeof (struct ospf6_lsa_header))
    ospf6_lsa_header_print_raw ((struct ospf6_lsa_header *) p);

  if (p != OSPF6_MESSAGE_END (oh))
    zlog_info ("Trailing garbage exists");
}

/* Receive function */
#define MSG_OK    0
#define MSG_NG    1
static int
ospf6_header_examin (struct in6_addr *src, struct in6_addr *dst,
                     struct ospf6_interface *oi, struct ospf6_header *oh)
{
  u_char type;
  type = OSPF6_MESSAGE_TYPE_CANONICAL (oh->type);

  /* version check */
  if (oh->version != OSPFV3_VERSION)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (type, RECV))
        zlog_info ("Message with unknown version");
      return MSG_NG;
    }

  /* Area-ID check */
  if (oh->area_id != oi->area->area_id)
    {
      if (oh->area_id == BACKBONE_AREA_ID)
        {
          if (IS_OSPF6_DEBUG_MESSAGE (type, RECV))
            zlog_info ("Message may be via Virtual Link: not supported");
          return MSG_NG;
        }

      if (IS_OSPF6_DEBUG_MESSAGE (type, RECV))
        zlog_info ("Area-ID mismatch");
      return MSG_NG;
    }

  /* Instance-ID check */
  if (oh->instance_id != oi->instance_id)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (type, RECV))
        zlog_info ("Instance-ID mismatch");
      return MSG_NG;
    }

  /* Router-ID check */
  if (oh->router_id == oi->area->ospf6->router_id)
    zlog_warn ("Detect duplicate Router-ID");

  return MSG_OK;
}

void
ospf6_hello_recv (struct in6_addr *src, struct in6_addr *dst,
                  struct ospf6_interface *oi, struct ospf6_header *oh)
{
  struct ospf6_hello *hello;
  struct ospf6_neighbor *on;
  char *p;
  int twoway = 0;
  int neighborchange = 0;
  int backupseen = 0;

  if (ospf6_header_examin (src, dst, oi, oh) != MSG_OK)
    return;

  hello = (struct ospf6_hello *)
    ((caddr_t) oh + sizeof (struct ospf6_header));

  /* HelloInterval check */
  if (ntohs (hello->hello_interval) != oi->hello_interval)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("HelloInterval mismatch");
      return;
    }

  /* RouterDeadInterval check */
  if (ntohs (hello->dead_interval) != oi->dead_interval)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("RouterDeadInterval mismatch");
      return;
    }

  /* E-bit check */
  if (OSPF6_OPT_ISSET (hello->options, OSPF6_OPT_E) !=
      OSPF6_OPT_ISSET (oi->area->options, OSPF6_OPT_E))
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("E-bit mismatch");
      return;
    }

  /* Find neighbor, create if not exist */
  on = ospf6_neighbor_lookup (oh->router_id, oi);
  if (on == NULL)
    {
      on = ospf6_neighbor_create (oh->router_id, oi);
      on->prev_drouter = on->drouter = hello->drouter;
      on->prev_bdrouter = on->bdrouter = hello->bdrouter;
      on->priority = hello->priority;
    }

  /* always override neighbor's source address and ifindex */
  on->ifindex = ntohl (hello->interface_id);
  memcpy (&on->linklocal_addr, src, sizeof (struct in6_addr));

  /* TwoWay check */
  for (p = (char *) ((caddr_t) hello + sizeof (struct ospf6_hello));
       p + sizeof (u_int32_t) <= OSPF6_MESSAGE_END (oh);
       p += sizeof (u_int32_t))
    {
      u_int32_t *router_id = (u_int32_t *) p;

      if (*router_id == oi->area->ospf6->router_id)
        twoway++;
    }

  if (p != OSPF6_MESSAGE_END (oh))
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Trailing garbage ignored");
    }

  /* RouterPriority check */
  if (on->priority != hello->priority)
    {
      on->priority = hello->priority;
      neighborchange++;
    }

  /* DR check */
  if (on->drouter != hello->drouter)
    {
      on->prev_drouter = on->drouter;
      on->drouter = hello->drouter;
      if (on->prev_drouter == on->router_id || on->drouter == on->router_id)
        neighborchange++;
    }

  /* BDR check */
  if (on->bdrouter != hello->bdrouter)
    {
      on->prev_bdrouter = on->bdrouter;
      on->bdrouter = hello->bdrouter;
      if (on->prev_bdrouter == on->router_id || on->bdrouter == on->router_id)
        neighborchange++;
    }

  /* BackupSeen check */
  if (oi->state == OSPF6_INTERFACE_WAITING)
    {
      if (hello->bdrouter == on->router_id)
        backupseen++;
      else if (hello->drouter == on->router_id && hello->bdrouter == htonl (0))
        backupseen++;
    }

  /* Execute neighbor events */
  thread_execute (master, hello_received, on, 0);
  if (twoway)
    thread_execute (master, twoway_received, on, 0);
  else
    thread_execute (master, oneway_received, on, 0);

  /* Schedule interface events */
  if (backupseen)
    thread_add_event (master, backup_seen, oi, 0);
  if (neighborchange)
    thread_add_event (master, neighbor_change, oi, 0);
}

static void
ospf6_dbdesc_recv_master (struct ospf6_header *oh,
                          struct ospf6_neighbor *on)
{
  struct ospf6_dbdesc *dbdesc;
  char *p;

  dbdesc = (struct ospf6_dbdesc *)
    ((caddr_t) oh + sizeof (struct ospf6_header));

  if (on->state < OSPF6_NEIGHBOR_INIT)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Neighbor state less than Init, ignore");
      return;
    }

  switch (on->state)
    {
    case OSPF6_NEIGHBOR_TWOWAY:
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Neighbor state is 2-Way, ignore");
      return;

    case OSPF6_NEIGHBOR_INIT:
      thread_execute (master, twoway_received, on, 0);
      if (on->state != OSPF6_NEIGHBOR_EXSTART)
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Neighbor state is not ExStart, ignore");
          return;
        }
      /* else fall through to ExStart */

    case OSPF6_NEIGHBOR_EXSTART:
      /* if neighbor obeys us as our slave, schedule negotiation_done
         and process LSA Headers. Otherwise, ignore this message */
      if (! CHECK_FLAG (dbdesc->bits, OSPF6_DBDESC_MSBIT) &&
          ! CHECK_FLAG (dbdesc->bits, OSPF6_DBDESC_IBIT) &&
          ntohl (dbdesc->seqnum) == on->dbdesc_seqnum)
        {
          /* execute NegotiationDone */
          thread_execute (master, negotiation_done, on, 0);

          /* Record neighbor options */
          memcpy (on->options, dbdesc->options, sizeof (on->options));
        }
      else
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Negotiation failed");
          return;
        }
      /* fall through to exchange */

    case OSPF6_NEIGHBOR_EXCHANGE:
      if (! memcmp (dbdesc, &on->dbdesc_last, sizeof (struct ospf6_dbdesc)))
        {
          /* Duplicated DatabaseDescription is dropped by master */
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Duplicated dbdesc discarded by Master, ignore");
          return;
        }

      if (CHECK_FLAG (dbdesc->bits, OSPF6_DBDESC_MSBIT))
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Master/Slave bit mismatch");
          thread_add_event (master, seqnumber_mismatch, on, 0);
          return;
        }

      if (CHECK_FLAG (dbdesc->bits, OSPF6_DBDESC_IBIT))
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Initialize bit mismatch");
          thread_add_event (master, seqnumber_mismatch, on, 0);
          return;
        }

      if (memcmp (on->options, dbdesc->options, sizeof (on->options)))
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Option field mismatch");
          thread_add_event (master, seqnumber_mismatch, on, 0);
          return;
        }

      if (ntohl (dbdesc->seqnum) != on->dbdesc_seqnum)
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Sequence number mismatch (%#lx expected)",
                       (u_long) on->dbdesc_seqnum);
          thread_add_event (master, seqnumber_mismatch, on, 0);
          return;
        }
      break;

    case OSPF6_NEIGHBOR_LOADING:
    case OSPF6_NEIGHBOR_FULL:
      if (! memcmp (dbdesc, &on->dbdesc_last, sizeof (struct ospf6_dbdesc)))
        {
          /* Duplicated DatabaseDescription is dropped by master */
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Duplicated dbdesc discarded by Master, ignore");
          return;
        }

      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Not duplicate dbdesc in state %s",
                   ospf6_neighbor_state_str[on->state]);
      thread_add_event (master, seqnumber_mismatch, on, 0);
      return;

    default:
      assert (0);
      break;
    }

  /* Process LSA headers */
  for (p = (char *) ((caddr_t) dbdesc + sizeof (struct ospf6_dbdesc));
       p + sizeof (struct ospf6_lsa_header) <= OSPF6_MESSAGE_END (oh);
       p += sizeof (struct ospf6_lsa_header))
    {
      struct ospf6_lsa *his, *mine;
      struct ospf6_lsdb *lsdb = NULL;

      his = ospf6_lsa_create_headeronly ((struct ospf6_lsa_header *) p);

      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("%s", his->name);

      switch (OSPF6_LSA_SCOPE (his->header->type))
        {
        case OSPF6_SCOPE_LINKLOCAL:
          lsdb = on->ospf6_if->lsdb;
          break;
        case OSPF6_SCOPE_AREA:
          lsdb = on->ospf6_if->area->lsdb;
          break;
        case OSPF6_SCOPE_AS:
          lsdb = on->ospf6_if->area->ospf6->lsdb;
          break;
        case OSPF6_SCOPE_RESERVED:
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Ignoring LSA of reserved scope");
          ospf6_lsa_delete (his);
          continue;
          break;
        }

      if (ntohs (his->header->type) == OSPF6_LSTYPE_AS_EXTERNAL &&
          IS_AREA_STUB (on->ospf6_if->area))
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("SeqNumMismatch (E-bit mismatch), discard");
          ospf6_lsa_delete (his);
          thread_add_event (master, seqnumber_mismatch, on, 0);
          return;
        }

      mine = ospf6_lsdb_lookup (his->header->type, his->header->id,
                                his->header->adv_router, lsdb);
      if (mine == NULL)
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Add request (No database copy)");
          ospf6_lsdb_add (his, on->request_list);
        }
      else if (ospf6_lsa_compare (his, mine) < 0)
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Add request (Received MoreRecent)");
          ospf6_lsdb_add (his, on->request_list);
        }
      else
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Discard (Existing MoreRecent)");
          ospf6_lsa_delete (his);
        }
    }

  if (p != OSPF6_MESSAGE_END (oh))
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Trailing garbage ignored");
    }

  /* Increment sequence number */
  on->dbdesc_seqnum ++;

  /* schedule send lsreq */
  if (on->thread_send_lsreq == NULL)
    on->thread_send_lsreq =
      thread_add_event (master, ospf6_lsreq_send, on, 0);

  THREAD_OFF (on->thread_send_dbdesc);

  /* More bit check */
  if (! CHECK_FLAG (dbdesc->bits, OSPF6_DBDESC_MBIT) &&
      ! CHECK_FLAG (on->dbdesc_bits, OSPF6_DBDESC_MBIT))
    thread_add_event (master, exchange_done, on, 0);
  else
    on->thread_send_dbdesc =
      thread_add_event (master, ospf6_dbdesc_send_newone, on, 0);

  /* save last received dbdesc */
  memcpy (&on->dbdesc_last, dbdesc, sizeof (struct ospf6_dbdesc));
}

static void
ospf6_dbdesc_recv_slave (struct ospf6_header *oh,
                         struct ospf6_neighbor *on)
{
  struct ospf6_dbdesc *dbdesc;
  char *p;

  dbdesc = (struct ospf6_dbdesc *)
    ((caddr_t) oh + sizeof (struct ospf6_header));

  if (on->state < OSPF6_NEIGHBOR_INIT)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Neighbor state less than Init, ignore");
      return;
    }

  switch (on->state)
    {
    case OSPF6_NEIGHBOR_TWOWAY:
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Neighbor state is 2-Way, ignore");
      return;

    case OSPF6_NEIGHBOR_INIT:
      thread_execute (master, twoway_received, on, 0);
      if (on->state != OSPF6_NEIGHBOR_EXSTART)
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Neighbor state is not ExStart, ignore");
          return;
        }
      /* else fall through to ExStart */

    case OSPF6_NEIGHBOR_EXSTART:
      /* If the neighbor is Master, act as Slave. Schedule negotiation_done
         and process LSA Headers. Otherwise, ignore this message */
      if (CHECK_FLAG (dbdesc->bits, OSPF6_DBDESC_IBIT) &&
          CHECK_FLAG (dbdesc->bits, OSPF6_DBDESC_MBIT) &&
          CHECK_FLAG (dbdesc->bits, OSPF6_DBDESC_MSBIT) &&
          ntohs (oh->length) == sizeof (struct ospf6_header) +
                                sizeof (struct ospf6_dbdesc))
        {
          /* set the master/slave bit to slave */
          UNSET_FLAG (on->dbdesc_bits, OSPF6_DBDESC_MSBIT);

          /* set the DD sequence number to one specified by master */
          on->dbdesc_seqnum = ntohl (dbdesc->seqnum);

          /* schedule NegotiationDone */
          thread_execute (master, negotiation_done, on, 0);

          /* Record neighbor options */
          memcpy (on->options, dbdesc->options, sizeof (on->options));
        }
      else
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Negotiation failed");
          return;
        }
      break;

    case OSPF6_NEIGHBOR_EXCHANGE:
      if (! memcmp (dbdesc, &on->dbdesc_last, sizeof (struct ospf6_dbdesc)))
        {
          /* Duplicated DatabaseDescription causes slave to retransmit */
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Duplicated dbdesc causes retransmit");
          THREAD_OFF (on->thread_send_dbdesc);
          on->thread_send_dbdesc =
            thread_add_event (master, ospf6_dbdesc_send, on, 0);
          return;
        }

      if (! CHECK_FLAG (dbdesc->bits, OSPF6_DBDESC_MSBIT))
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Master/Slave bit mismatch");
          thread_add_event (master, seqnumber_mismatch, on, 0);
          return;
        }

      if (CHECK_FLAG (dbdesc->bits, OSPF6_DBDESC_IBIT))
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Initialize bit mismatch");
          thread_add_event (master, seqnumber_mismatch, on, 0);
          return;
        }

      if (memcmp (on->options, dbdesc->options, sizeof (on->options)))
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Option field mismatch");
          thread_add_event (master, seqnumber_mismatch, on, 0);
          return;
        }

      if (ntohl (dbdesc->seqnum) != on->dbdesc_seqnum + 1)
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Sequence number mismatch (%#lx expected)",
                       (u_long) on->dbdesc_seqnum + 1);
          thread_add_event (master, seqnumber_mismatch, on, 0);
          return;
        }
      break;

    case OSPF6_NEIGHBOR_LOADING:
    case OSPF6_NEIGHBOR_FULL:
      if (! memcmp (dbdesc, &on->dbdesc_last, sizeof (struct ospf6_dbdesc)))
        {
          /* Duplicated DatabaseDescription causes slave to retransmit */
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Duplicated dbdesc causes retransmit");
          THREAD_OFF (on->thread_send_dbdesc);
          on->thread_send_dbdesc =
            thread_add_event (master, ospf6_dbdesc_send, on, 0);
          return;
        }

      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Not duplicate dbdesc in state %s",
                   ospf6_neighbor_state_str[on->state]);
      thread_add_event (master, seqnumber_mismatch, on, 0);
      return;

    default:
      assert (0);
      break;
    }

  /* Process LSA headers */
  for (p = (char *) ((caddr_t) dbdesc + sizeof (struct ospf6_dbdesc));
       p + sizeof (struct ospf6_lsa_header) <= OSPF6_MESSAGE_END (oh);
       p += sizeof (struct ospf6_lsa_header))
    {
      struct ospf6_lsa *his, *mine;
      struct ospf6_lsdb *lsdb = NULL;

      his = ospf6_lsa_create_headeronly ((struct ospf6_lsa_header *) p);

      switch (OSPF6_LSA_SCOPE (his->header->type))
        {
        case OSPF6_SCOPE_LINKLOCAL:
          lsdb = on->ospf6_if->lsdb;
          break;
        case OSPF6_SCOPE_AREA:
          lsdb = on->ospf6_if->area->lsdb;
          break;
        case OSPF6_SCOPE_AS:
          lsdb = on->ospf6_if->area->ospf6->lsdb;
          break;
        case OSPF6_SCOPE_RESERVED:
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Ignoring LSA of reserved scope");
          ospf6_lsa_delete (his);
          continue;
          break;
        }

      if (OSPF6_LSA_SCOPE (his->header->type) == OSPF6_SCOPE_AS &&
          IS_AREA_STUB (on->ospf6_if->area))
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("E-bit mismatch with LSA Headers");
          ospf6_lsa_delete (his);
          thread_add_event (master, seqnumber_mismatch, on, 0);
          return;
        }

      mine = ospf6_lsdb_lookup (his->header->type, his->header->id,
                                his->header->adv_router, lsdb);
      if (mine == NULL || ospf6_lsa_compare (his, mine) < 0)
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Add request-list: %s", his->name);
          ospf6_lsdb_add (his, on->request_list);
        }
      else
        ospf6_lsa_delete (his);
    }

  if (p != OSPF6_MESSAGE_END (oh))
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Trailing garbage ignored");
    }

  /* Set sequence number to Master's */
  on->dbdesc_seqnum = ntohl (dbdesc->seqnum);

  /* schedule send lsreq */
  if (on->thread_send_lsreq == NULL)
    on->thread_send_lsreq =
      thread_add_event (master, ospf6_lsreq_send, on, 0);

  THREAD_OFF (on->thread_send_dbdesc);
  on->thread_send_dbdesc =
    thread_add_event (master, ospf6_dbdesc_send_newone, on, 0);

  /* save last received dbdesc */
  memcpy (&on->dbdesc_last, dbdesc, sizeof (struct ospf6_dbdesc));
}

void
ospf6_dbdesc_recv (struct in6_addr *src, struct in6_addr *dst,
                   struct ospf6_interface *oi, struct ospf6_header *oh)
{
  struct ospf6_neighbor *on;
  struct ospf6_dbdesc *dbdesc;

  if (ospf6_header_examin (src, dst, oi, oh) != MSG_OK)
    return;

  on = ospf6_neighbor_lookup (oh->router_id, oi);
  if (on == NULL)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Neighbor not found, ignore");
      return;
    }

  dbdesc = (struct ospf6_dbdesc *)
    ((caddr_t) oh + sizeof (struct ospf6_header));

  /* Interface MTU check */
  if (ntohs (dbdesc->ifmtu) != oi->ifmtu)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("I/F MTU mismatch");
      return;
    }

  if (dbdesc->reserved1 || dbdesc->reserved2)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Non-0 reserved field in %s's DbDesc, correct",
                   on->name);
      dbdesc->reserved1 = 0;
      dbdesc->reserved2 = 0;
    }

  if (ntohl (oh->router_id) < ntohl (ospf6->router_id))
    ospf6_dbdesc_recv_master (oh, on);
  else if (ntohl (ospf6->router_id) < ntohl (oh->router_id))
    ospf6_dbdesc_recv_slave (oh, on);
  else
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Can't decide which is master, ignore");
    }
}

void
ospf6_lsreq_recv (struct in6_addr *src, struct in6_addr *dst,
                  struct ospf6_interface *oi, struct ospf6_header *oh)
{
  struct ospf6_neighbor *on;
  char *p;
  struct ospf6_lsreq_entry *e;
  struct ospf6_lsdb *lsdb = NULL;
  struct ospf6_lsa *lsa;

  if (ospf6_header_examin (src, dst, oi, oh) != MSG_OK)
    return;

  on = ospf6_neighbor_lookup (oh->router_id, oi);
  if (on == NULL)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Neighbor not found, ignore");
      return;
    }

  if (on->state != OSPF6_NEIGHBOR_EXCHANGE &&
      on->state != OSPF6_NEIGHBOR_LOADING &&
      on->state != OSPF6_NEIGHBOR_FULL)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Neighbor state less than Exchange, ignore");
      return;
    }

  /* Process each request */
  for (p = (char *) ((caddr_t) oh + sizeof (struct ospf6_header));
       p + sizeof (struct ospf6_lsreq_entry) <= OSPF6_MESSAGE_END (oh);
       p += sizeof (struct ospf6_lsreq_entry))
    {
      e = (struct ospf6_lsreq_entry *) p;

      switch (OSPF6_LSA_SCOPE (e->type))
        {
        case OSPF6_SCOPE_LINKLOCAL:
          lsdb = on->ospf6_if->lsdb;
          break;
        case OSPF6_SCOPE_AREA:
          lsdb = on->ospf6_if->area->lsdb;
          break;
        case OSPF6_SCOPE_AS:
          lsdb = on->ospf6_if->area->ospf6->lsdb;
          break;
        default:
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Ignoring LSA of reserved scope");
          continue;
          break;
        }

      /* Find database copy */
      lsa = ospf6_lsdb_lookup (e->type, e->id, e->adv_router, lsdb);
      if (lsa == NULL)
        {
          char id[16], adv_router[16];
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            {
              inet_ntop (AF_INET, &e->id, id, sizeof (id));
              inet_ntop (AF_INET, &e->adv_router, adv_router,
                     sizeof (adv_router));
              zlog_info ("Can't find requested [%s Id:%s Adv:%s]",
                         ospf6_lstype_name (e->type), id, adv_router);
            }
          thread_add_event (master, bad_lsreq, on, 0);
          return;
        }

      ospf6_lsdb_add (ospf6_lsa_copy (lsa), on->lsupdate_list);
    }

  if (p != OSPF6_MESSAGE_END (oh))
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Trailing garbage ignored");
    }

  /* schedule send lsupdate */
  THREAD_OFF (on->thread_send_lsupdate);
  on->thread_send_lsupdate =
    thread_add_event (master, ospf6_lsupdate_send_neighbor, on, 0);
}

void
ospf6_lsupdate_recv (struct in6_addr *src, struct in6_addr *dst,
                     struct ospf6_interface *oi, struct ospf6_header *oh)
{
  struct ospf6_neighbor *on;
  struct ospf6_lsupdate *lsupdate;
  unsigned long num;
  char *p;

  if (ospf6_header_examin (src, dst, oi, oh) != MSG_OK)
    return;

  on = ospf6_neighbor_lookup (oh->router_id, oi);
  if (on == NULL)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Neighbor not found, ignore");
      return;
    }

  if (on->state != OSPF6_NEIGHBOR_EXCHANGE &&
      on->state != OSPF6_NEIGHBOR_LOADING &&
      on->state != OSPF6_NEIGHBOR_FULL)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Neighbor state less than Exchange, ignore");
      return;
    }

  lsupdate = (struct ospf6_lsupdate *)
    ((caddr_t) oh + sizeof (struct ospf6_header));

  num = ntohl (lsupdate->lsa_number);

  /* Process LSAs */
  for (p = (char *) ((caddr_t) lsupdate + sizeof (struct ospf6_lsupdate));
       p < OSPF6_MESSAGE_END (oh) &&
       p + OSPF6_LSA_SIZE (p) <= OSPF6_MESSAGE_END (oh);
       p += OSPF6_LSA_SIZE (p))
    {
      if (num == 0)
        break;
      if (OSPF6_LSA_SIZE (p) < sizeof (struct ospf6_lsa_header))
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Malformed LSA length, quit processing");
          break;
        }

      ospf6_receive_lsa (on, (struct ospf6_lsa_header *) p);
      num--;
    }

  if (num != 0)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Malformed LSA number or LSA length");
    }
  if (p != OSPF6_MESSAGE_END (oh))
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Trailing garbage ignored");
    }

  /* RFC2328 Section 10.9: When the neighbor responds to these requests
     with the proper Link State Update packet(s), the Link state request
     list is truncated and a new Link State Request packet is sent. */
  /* send new Link State Request packet if this LS Update packet
     can be recognized as a response to our previous LS Request */
  if (! IN6_IS_ADDR_MULTICAST (dst) &&
      (on->state == OSPF6_NEIGHBOR_EXCHANGE ||
       on->state == OSPF6_NEIGHBOR_LOADING))
    {
      THREAD_OFF (on->thread_send_lsreq);
      on->thread_send_lsreq =
        thread_add_event (master, ospf6_lsreq_send, on, 0);
    }
}

void
ospf6_lsack_recv (struct in6_addr *src, struct in6_addr *dst,
                  struct ospf6_interface *oi, struct ospf6_header *oh)
{
  struct ospf6_neighbor *on;
  char *p;
  struct ospf6_lsa *his, *mine;
  struct ospf6_lsdb *lsdb = NULL;

  assert (oh->type == OSPF6_MESSAGE_TYPE_LSACK);
  if (ospf6_header_examin (src, dst, oi, oh) != MSG_OK)
    return;

  on = ospf6_neighbor_lookup (oh->router_id, oi);
  if (on == NULL)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Neighbor not found, ignore");
      return;
    }

  if (on->state != OSPF6_NEIGHBOR_EXCHANGE &&
      on->state != OSPF6_NEIGHBOR_LOADING &&
      on->state != OSPF6_NEIGHBOR_FULL)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Neighbor state less than Exchange, ignore");
      return;
    }

  for (p = (char *) ((caddr_t) oh + sizeof (struct ospf6_header));
       p + sizeof (struct ospf6_lsa_header) <= OSPF6_MESSAGE_END (oh);
       p += sizeof (struct ospf6_lsa_header))
    {
      his = ospf6_lsa_create_headeronly ((struct ospf6_lsa_header *) p);

      switch (OSPF6_LSA_SCOPE (his->header->type))
        {
        case OSPF6_SCOPE_LINKLOCAL:
          lsdb = on->ospf6_if->lsdb;
          break;
        case OSPF6_SCOPE_AREA:
          lsdb = on->ospf6_if->area->lsdb;
          break;
        case OSPF6_SCOPE_AS:
          lsdb = on->ospf6_if->area->ospf6->lsdb;
          break;
        case OSPF6_SCOPE_RESERVED:
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Ignoring LSA of reserved scope");
          ospf6_lsa_delete (his);
          continue;
          break;
        }

      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("%s acknowledged by %s", his->name, on->name);

      /* Find database copy */
      mine = ospf6_lsdb_lookup (his->header->type, his->header->id,
                                his->header->adv_router, lsdb);
      if (mine == NULL)
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("No database copy");
          ospf6_lsa_delete (his);
          continue;
        }

      /* Check if the LSA is on his retrans-list */
      mine = ospf6_lsdb_lookup (his->header->type, his->header->id,
                                his->header->adv_router, on->retrans_list);
      if (mine == NULL)
        {
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Not on %s's retrans-list", on->name);
          ospf6_lsa_delete (his);
          continue;
        }

      if (ospf6_lsa_compare (his, mine) != 0)
        {
          /* Log this questionable acknowledgement,
             and examine the next one. */
          if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
            zlog_info ("Questionable acknowledgement");
          ospf6_lsa_delete (his);
          continue;
        }

      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Acknowledged, remove from %s's retrans-list",
                   on->name);

      if (OSPF6_LSA_IS_MAXAGE (mine))
        ospf6_maxage_remove (on->ospf6_if->area->ospf6);

      ospf6_decrement_retrans_count (mine);
      ospf6_lsdb_remove (mine, on->retrans_list);
      ospf6_lsa_delete (his);
    }

  if (p != OSPF6_MESSAGE_END (oh))
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Trailing garbage ignored");
    }
}

char *recvbuf = NULL;
char *sendbuf = NULL;
int iobuflen = 0;

int
ospf6_iobuf_size (int size)
{
  char *recvnew, *sendnew;

  if (size <= iobuflen)
    return iobuflen;

  recvnew = XMALLOC (MTYPE_OSPF6_MESSAGE, size);
  sendnew = XMALLOC (MTYPE_OSPF6_MESSAGE, size);
  if (recvnew == NULL || sendnew == NULL)
    {
      if (recvnew)
        XFREE (MTYPE_OSPF6_MESSAGE, recvnew);
      if (sendnew)
        XFREE (MTYPE_OSPF6_MESSAGE, sendnew);
      zlog_info ("Could not allocate I/O buffer of size %d.", size);
      return iobuflen;
    }

  if (recvbuf)
    XFREE (MTYPE_OSPF6_MESSAGE, recvbuf);
  if (sendbuf)
    XFREE (MTYPE_OSPF6_MESSAGE, sendbuf);
  recvbuf = recvnew;
  sendbuf = sendnew;
  iobuflen = size;

  return iobuflen;
}

int
ospf6_receive (struct thread *thread)
{
  int sockfd, len;
  char srcname[64], dstname[64];
  struct in6_addr src, dst;
  unsigned int ifindex;
  struct iovec iovector[2];
  struct ospf6_interface *oi;
  struct ospf6_header *oh;

  /* add next read thread */
  sockfd = THREAD_FD (thread);
  thread_add_read (master, ospf6_receive, NULL, sockfd);

  /* initialize */
  memset (recvbuf, 0, iobuflen);
  iovector[0].iov_base = recvbuf;
  iovector[0].iov_len = iobuflen;
  iovector[1].iov_base = NULL;
  iovector[1].iov_len = 0;

  /* receive message */
  len = ospf6_recvmsg (&src, &dst, &ifindex, iovector);
  if (len > iobuflen)
    {
      zlog_err ("Excess message read");
      return 0;
    }
  else if (len < sizeof (struct ospf6_header))
    {
      zlog_err ("Deficient message read");
      return 0;
    }

  oi = ospf6_interface_lookup_by_ifindex (ifindex);
  if (oi == NULL || oi->area == NULL)
    {
      zlog_info ("Message received on disabled interface");
      return 0;
    }

  oh = (struct ospf6_header *) recvbuf;

  /* Log */
  if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
    {
      inet_ntop (AF_INET6, &src, srcname, sizeof (srcname));
      inet_ntop (AF_INET6, &dst, dstname, sizeof (dstname));
      zlog_info ("%s received on %s",
                 OSPF6_MESSAGE_TYPE_NAME (oh->type), oi->interface->name);
      zlog_info ("    src: %s", srcname);
      zlog_info ("    dst: %s", dstname);
      if (len != ntohs (oh->length))
        zlog_info ("Message length does not match actually received: %d", len);

      switch (oh->type)
        {
          case OSPF6_MESSAGE_TYPE_HELLO:
            ospf6_hello_print (oh);
            break;
          case OSPF6_MESSAGE_TYPE_DBDESC:
            ospf6_dbdesc_print (oh);
            break;
          case OSPF6_MESSAGE_TYPE_LSREQ:
            ospf6_lsreq_print (oh);
            break;
          case OSPF6_MESSAGE_TYPE_LSUPDATE:
            ospf6_lsupdate_print (oh);
            break;
          case OSPF6_MESSAGE_TYPE_LSACK:
            ospf6_lsack_print (oh);
            break;
          default:
            zlog_info ("Unknown message");
            break;
        }
    }

  if (CHECK_FLAG (oi->flag, OSPF6_INTERFACE_PASSIVE))
    {
      if (IS_OSPF6_DEBUG_MESSAGE (oh->type, RECV))
        zlog_info ("Ignore message on passive interface %s",
                   oi->interface->name);
      return 0;
    }

  switch (oh->type)
    {
      case OSPF6_MESSAGE_TYPE_HELLO:
        ospf6_hello_recv (&src, &dst, oi, oh);
        break;

      case OSPF6_MESSAGE_TYPE_DBDESC:
        ospf6_dbdesc_recv (&src, &dst, oi, oh);
        break;

      case OSPF6_MESSAGE_TYPE_LSREQ:
        ospf6_lsreq_recv (&src, &dst, oi, oh);
        break;

      case OSPF6_MESSAGE_TYPE_LSUPDATE:
        ospf6_lsupdate_recv (&src, &dst, oi, oh);
        break;

      case OSPF6_MESSAGE_TYPE_LSACK:
        ospf6_lsack_recv (&src, &dst, oi, oh);
        break;

      default:
        if (IS_OSPF6_DEBUG_MESSAGE (OSPF6_MESSAGE_TYPE_UNKNOWN, RECV))
          zlog_info ("Unknown message");
        break;
    }

  return 0;
}

void
ospf6_send (struct in6_addr *src, struct in6_addr *dst,
            struct ospf6_interface *oi, struct ospf6_header *oh)
{
  int len;
  char srcname[64], dstname[64];
  struct iovec iovector[2];

  /* initialize */
  iovector[0].iov_base = (caddr_t) oh;
  iovector[0].iov_len = ntohs (oh->length);
  iovector[1].iov_base = NULL;
  iovector[1].iov_len = 0;

  /* fill OSPF header */
  oh->version = OSPFV3_VERSION;
  /* message type must be set before */
  /* message length must be set before */
  oh->router_id = oi->area->ospf6->router_id;
  oh->area_id = oi->area->area_id;
  /* checksum is calculated by kernel */
  oh->instance_id = oi->instance_id;
  oh->reserved = 0;

  /* Log */
  if (IS_OSPF6_DEBUG_MESSAGE (oh->type, SEND))
    {
      inet_ntop (AF_INET6, dst, dstname, sizeof (dstname));
      if (src)
        inet_ntop (AF_INET6, src, srcname, sizeof (srcname));
      else
        memset (srcname, 0, sizeof (srcname));
      zlog_info ("%s send on %s",
                 OSPF6_MESSAGE_TYPE_NAME (oh->type), oi->interface->name);
      zlog_info ("    src: %s", srcname);
      zlog_info ("    dst: %s", dstname);

      switch (oh->type)
        {
          case OSPF6_MESSAGE_TYPE_HELLO:
            ospf6_hello_print (oh);
            break;
          case OSPF6_MESSAGE_TYPE_DBDESC:
            ospf6_dbdesc_print (oh);
            break;
          case OSPF6_MESSAGE_TYPE_LSREQ:
            ospf6_lsreq_print (oh);
            break;
          case OSPF6_MESSAGE_TYPE_LSUPDATE:
            ospf6_lsupdate_print (oh);
            break;
          case OSPF6_MESSAGE_TYPE_LSACK:
            ospf6_lsack_print (oh);
            break;
          default:
            zlog_info ("Unknown message");
            assert (0);
            break;
        }
    }

  /* send message */
  len = ospf6_sendmsg (src, dst, &oi->interface->ifindex, iovector);
  if (len != ntohs (oh->length))
    zlog_err ("Could not send entire message");
}

int
ospf6_hello_send (struct thread *thread)
{
  struct ospf6_interface *oi;
  struct ospf6_header *oh;
  struct ospf6_hello *hello;
  char *p;
  listnode node;
  struct ospf6_neighbor *on;

  oi = (struct ospf6_interface *) THREAD_ARG (thread);
  oi->thread_send_hello = (struct thread *) NULL;

  if (oi->state <= OSPF6_INTERFACE_DOWN)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (OSPF6_MESSAGE_TYPE_HELLO, SEND))
        zlog_info ("Unable to send Hello on down interface %s",
                   oi->interface->name);
      return 0;
    }

  /* set next thread */
  oi->thread_send_hello = thread_add_timer (master, ospf6_hello_send,
                                            oi, oi->hello_interval);

  memset (sendbuf, 0, iobuflen);
  oh = (struct ospf6_header *) sendbuf;
  hello = (struct ospf6_hello *)((caddr_t) oh + sizeof (struct ospf6_header));

  hello->interface_id = htonl (oi->interface->ifindex);
  hello->priority = oi->priority;
  hello->options[0] = oi->area->options[0];
  hello->options[1] = oi->area->options[1];
  hello->options[2] = oi->area->options[2];
  hello->hello_interval = htons (oi->hello_interval);
  hello->dead_interval = htons (oi->dead_interval);
  hello->drouter = oi->drouter;
  hello->bdrouter = oi->bdrouter;

  p = (char *)((caddr_t) hello + sizeof (struct ospf6_hello));

  for (node = listhead (oi->neighbor_list); node; nextnode (node))
    {
      on = (struct ospf6_neighbor *) getdata (node);

      if (on->state < OSPF6_NEIGHBOR_INIT)
        continue;

      if (p - sendbuf + sizeof (u_int32_t) > oi->ifmtu)
        {
          if (IS_OSPF6_DEBUG_MESSAGE (OSPF6_MESSAGE_TYPE_HELLO, SEND))
            zlog_info ("sending Hello message: exceeds I/F MTU");
          break;
        }

      memcpy (p, &on->router_id, sizeof (u_int32_t));
      p += sizeof (u_int32_t);
    }

  oh->type = OSPF6_MESSAGE_TYPE_HELLO;
  oh->length = htons (p - sendbuf);

  ospf6_send (oi->linklocal_addr, &allspfrouters6, oi, oh);
  return 0;
}

int
ospf6_dbdesc_send (struct thread *thread)
{
  struct ospf6_neighbor *on;
  struct ospf6_header *oh;
  struct ospf6_dbdesc *dbdesc;
  char *p;
  struct ospf6_lsa *lsa;

  on = (struct ospf6_neighbor *) THREAD_ARG (thread);
  on->thread_send_dbdesc = (struct thread *) NULL;

  if (on->state < OSPF6_NEIGHBOR_EXSTART)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (OSPF6_MESSAGE_TYPE_DBDESC, SEND))
        zlog_info ("Quit to send DbDesc to neighbor %s state %s",
                   on->name, ospf6_neighbor_state_str[on->state]);
      return 0;
    }

  /* set next thread if master */
  if (CHECK_FLAG (on->dbdesc_bits, OSPF6_DBDESC_MSBIT))
    on->thread_send_dbdesc =
      thread_add_timer (master, ospf6_dbdesc_send, on,
                        on->ospf6_if->rxmt_interval);

  memset (sendbuf, 0, iobuflen);
  oh = (struct ospf6_header *) sendbuf;
  dbdesc = (struct ospf6_dbdesc *)((caddr_t) oh +
                                   sizeof (struct ospf6_header));

  /* if this is initial one, initialize sequence number for DbDesc */
  if (CHECK_FLAG (on->dbdesc_bits, OSPF6_DBDESC_IBIT))
    {
      struct timeval tv;
      if (gettimeofday (&tv, (struct timezone *) NULL) < 0)
        tv.tv_sec = 1;
      on->dbdesc_seqnum = tv.tv_sec;
    }

  dbdesc->options[0] = on->ospf6_if->area->options[0];
  dbdesc->options[1] = on->ospf6_if->area->options[1];
  dbdesc->options[2] = on->ospf6_if->area->options[2];
  dbdesc->ifmtu = htons (on->ospf6_if->ifmtu);
  dbdesc->bits = on->dbdesc_bits;
  dbdesc->seqnum = htonl (on->dbdesc_seqnum);

  /* if this is not initial one, set LSA headers in dbdesc */
  p = (char *)((caddr_t) dbdesc + sizeof (struct ospf6_dbdesc));
  if (! CHECK_FLAG (on->dbdesc_bits, OSPF6_DBDESC_IBIT))
    {
      for (lsa = ospf6_lsdb_head (on->dbdesc_list); lsa;
           lsa = ospf6_lsdb_next (lsa))
        {
          ospf6_lsa_age_update_to_send (lsa, on->ospf6_if->transdelay);

          /* MTU check */
          if (p - sendbuf + sizeof (struct ospf6_lsa_header) >
              on->ospf6_if->ifmtu)
            {
              ospf6_lsa_unlock (lsa);
              break;
            }
          memcpy (p, lsa->header, sizeof (struct ospf6_lsa_header));
          p += sizeof (struct ospf6_lsa_header);
        }
    }

  oh->type = OSPF6_MESSAGE_TYPE_DBDESC;
  oh->length = htons (p - sendbuf);

  ospf6_send (on->ospf6_if->linklocal_addr, &on->linklocal_addr,
              on->ospf6_if, oh);
  return 0;
}

int
ospf6_dbdesc_send_newone (struct thread *thread)
{
  struct ospf6_neighbor *on;
  struct ospf6_lsa *lsa;
  unsigned int size = 0;

  on = (struct ospf6_neighbor *) THREAD_ARG (thread);
  ospf6_lsdb_remove_all (on->dbdesc_list);

  /* move LSAs from summary_list to dbdesc_list (within neighbor structure)
     so that ospf6_send_dbdesc () can send those LSAs */
  size = sizeof (struct ospf6_lsa_header) + sizeof (struct ospf6_dbdesc);
  for (lsa = ospf6_lsdb_head (on->summary_list); lsa;
       lsa = ospf6_lsdb_next (lsa))
    {
      if (size + sizeof (struct ospf6_lsa_header) > on->ospf6_if->ifmtu)
        {
          ospf6_lsa_unlock (lsa);
          break;
        }

      ospf6_lsdb_add (ospf6_lsa_copy (lsa), on->dbdesc_list);
      ospf6_lsdb_remove (lsa, on->summary_list);
      size += sizeof (struct ospf6_lsa_header);
    }

  if (on->summary_list->count == 0)
    UNSET_FLAG (on->dbdesc_bits, OSPF6_DBDESC_MBIT);

  /* If slave, More bit check must be done here */
  if (! CHECK_FLAG (on->dbdesc_bits, OSPF6_DBDESC_MSBIT) && /* Slave */
      ! CHECK_FLAG (on->dbdesc_last.bits, OSPF6_DBDESC_MBIT) &&
      ! CHECK_FLAG (on->dbdesc_bits, OSPF6_DBDESC_MBIT))
    thread_add_event (master, exchange_done, on, 0);

  thread_execute (master, ospf6_dbdesc_send, on, 0);
  return 0;
}

int
ospf6_lsreq_send (struct thread *thread)
{
  struct ospf6_neighbor *on;
  struct ospf6_header *oh;
  struct ospf6_lsreq_entry *e;
  char *p;
  struct ospf6_lsa *lsa;

  on = (struct ospf6_neighbor *) THREAD_ARG (thread);
  on->thread_send_lsreq = (struct thread *) NULL;

  /* LSReq will be sent only in ExStart or Loading */
  if (on->state != OSPF6_NEIGHBOR_EXCHANGE &&
      on->state != OSPF6_NEIGHBOR_LOADING)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (OSPF6_MESSAGE_TYPE_LSREQ, SEND))
        zlog_info ("Quit to send LSReq to neighbor %s state %s",
                   on->name, ospf6_neighbor_state_str[on->state]);
      return 0;
    }

  /* schedule loading_done if request list is empty */
  if (on->request_list->count == 0)
    {
      thread_add_event (master, loading_done, on, 0);
      return 0;
    }

  /* set next thread */
  on->thread_send_lsreq =
    thread_add_timer (master, ospf6_lsreq_send, on,
                      on->ospf6_if->rxmt_interval);

  memset (sendbuf, 0, iobuflen);
  oh = (struct ospf6_header *) sendbuf;

  /* set Request entries in lsreq */
  p = (char *)((caddr_t) oh + sizeof (struct ospf6_header));
  for (lsa = ospf6_lsdb_head (on->request_list); lsa;
       lsa = ospf6_lsdb_next (lsa))
    {
      /* MTU check */
      if (p - sendbuf + sizeof (struct ospf6_lsreq_entry) > on->ospf6_if->ifmtu)
        {
          ospf6_lsa_unlock (lsa);
          break;
        }

      e = (struct ospf6_lsreq_entry *) p;
      e->type = lsa->header->type;
      e->id = lsa->header->id;
      e->adv_router = lsa->header->adv_router;
      p += sizeof (struct ospf6_lsreq_entry);
    }

  oh->type = OSPF6_MESSAGE_TYPE_LSREQ;
  oh->length = htons (p - sendbuf);

  ospf6_send (on->ospf6_if->linklocal_addr, &on->linklocal_addr,
              on->ospf6_if, oh);
  return 0;
}

int
ospf6_lsupdate_send_neighbor (struct thread *thread)
{
  struct ospf6_neighbor *on;
  struct ospf6_header *oh;
  struct ospf6_lsupdate *lsupdate;
  char *p;
  int num;
  struct ospf6_lsa *lsa;

  on = (struct ospf6_neighbor *) THREAD_ARG (thread);
  on->thread_send_lsupdate = (struct thread *) NULL;

  if (IS_OSPF6_DEBUG_MESSAGE (OSPF6_MESSAGE_TYPE_LSUPDATE, SEND))
    zlog_info ("LSUpdate to neighbor %s", on->name);

  if (on->state < OSPF6_NEIGHBOR_EXCHANGE)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (OSPF6_MESSAGE_TYPE_LSUPDATE, SEND))
        zlog_info ("Quit to send (neighbor state %s)",
                   ospf6_neighbor_state_str[on->state]);
      return 0;
    }

  /* if we have nothing to send, return */
  if (on->lsupdate_list->count == 0 &&
      on->retrans_list->count == 0)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (OSPF6_MESSAGE_TYPE_LSUPDATE, SEND))
        zlog_info ("Quit to send (nothing to send)");
      return 0;
    }

  memset (sendbuf, 0, iobuflen);
  oh = (struct ospf6_header *) sendbuf;
  lsupdate = (struct ospf6_lsupdate *)
    ((caddr_t) oh + sizeof (struct ospf6_header));

  p = (char *)((caddr_t) lsupdate + sizeof (struct ospf6_lsupdate));
  num = 0;

  /* lsupdate_list lists those LSA which doesn't need to be
     retransmitted. remove those from the list */
  for (lsa = ospf6_lsdb_head (on->lsupdate_list); lsa;
       lsa = ospf6_lsdb_next (lsa))
    {
      /* MTU check */
      if (p - sendbuf + OSPF6_LSA_SIZE (lsa->header) > on->ospf6_if->ifmtu)
        {
          ospf6_lsa_unlock (lsa);
          break;
        }

      ospf6_lsa_age_update_to_send (lsa, on->ospf6_if->transdelay);
      memcpy (p, lsa->header, OSPF6_LSA_SIZE (lsa->header));
      p += OSPF6_LSA_SIZE (lsa->header);
      num++;

      assert (lsa->lock == 2);
      ospf6_lsdb_remove (lsa, on->lsupdate_list);
    }

  for (lsa = ospf6_lsdb_head (on->retrans_list); lsa;
       lsa = ospf6_lsdb_next (lsa))
    {
      /* MTU check */
      if (p - sendbuf + OSPF6_LSA_SIZE (lsa->header) > on->ospf6_if->ifmtu)
        {
          ospf6_lsa_unlock (lsa);
          break;
        }

      ospf6_lsa_age_update_to_send (lsa, on->ospf6_if->transdelay);
      memcpy (p, lsa->header, OSPF6_LSA_SIZE (lsa->header));
      p += OSPF6_LSA_SIZE (lsa->header);
      num++;
    }

  lsupdate->lsa_number = htonl (num);

  oh->type = OSPF6_MESSAGE_TYPE_LSUPDATE;
  oh->length = htons (p - sendbuf);

  ospf6_send (on->ospf6_if->linklocal_addr, &on->linklocal_addr,
              on->ospf6_if, oh);

  if (on->lsupdate_list->count != 0 ||
      on->retrans_list->count != 0)
    {
      if (on->lsupdate_list->count != 0)
        on->thread_send_lsupdate =
          thread_add_event (master, ospf6_lsupdate_send_neighbor, on, 0);
      else
        on->thread_send_lsupdate =
          thread_add_timer (master, ospf6_lsupdate_send_neighbor, on,
                            on->ospf6_if->rxmt_interval);
    }

  return 0;
}

int
ospf6_lsupdate_send_interface (struct thread *thread)
{
  struct ospf6_interface *oi;
  struct ospf6_header *oh;
  struct ospf6_lsupdate *lsupdate;
  char *p;
  int num;
  struct ospf6_lsa *lsa;

  oi = (struct ospf6_interface *) THREAD_ARG (thread);
  oi->thread_send_lsupdate = (struct thread *) NULL;

  if (oi->state <= OSPF6_INTERFACE_WAITING)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (OSPF6_MESSAGE_TYPE_LSUPDATE, SEND))
        zlog_info ("Quit to send LSUpdate to interface %s state %s",
                   oi->interface->name, ospf6_interface_state_str[oi->state]);
      return 0;
    }

  /* if we have nothing to send, return */
  if (oi->lsupdate_list->count == 0)
    return 0;

  memset (sendbuf, 0, iobuflen);
  oh = (struct ospf6_header *) sendbuf;
  lsupdate = (struct ospf6_lsupdate *)((caddr_t) oh +
                                       sizeof (struct ospf6_header));

  p = (char *)((caddr_t) lsupdate + sizeof (struct ospf6_lsupdate));
  num = 0;

  for (lsa = ospf6_lsdb_head (oi->lsupdate_list); lsa;
       lsa = ospf6_lsdb_next (lsa))
    {
      /* MTU check */
      if (p - sendbuf + OSPF6_LSA_SIZE (lsa->header) > oi->ifmtu)
        {
          ospf6_lsa_unlock (lsa);
          break;
        }

      ospf6_lsa_age_update_to_send (lsa, oi->transdelay);
      memcpy (p, lsa->header, OSPF6_LSA_SIZE (lsa->header));
      p += OSPF6_LSA_SIZE (lsa->header);
      num++;

      assert (lsa->lock == 2);
      ospf6_lsdb_remove (lsa, oi->lsupdate_list);
    }

  lsupdate->lsa_number = htonl (num);

  oh->type = OSPF6_MESSAGE_TYPE_LSUPDATE;
  oh->length = htons (p - sendbuf);

  if (oi->state == OSPF6_INTERFACE_DR ||
      oi->state == OSPF6_INTERFACE_BDR)
    ospf6_send (oi->linklocal_addr, &allspfrouters6, oi, oh);
  else
    ospf6_send (oi->linklocal_addr, &alldrouters6, oi, oh);

  if (oi->lsupdate_list->count > 0)
    {
      oi->thread_send_lsupdate =
        thread_add_event (master, ospf6_lsupdate_send_interface, oi, 0);
    }

  return 0;
}

int
ospf6_lsack_send_neighbor (struct thread *thread)
{
  struct ospf6_neighbor *on;
  struct ospf6_header *oh;
  char *p;
  struct ospf6_lsa *lsa;

  on = (struct ospf6_neighbor *) THREAD_ARG (thread);
  on->thread_send_lsack = (struct thread *) NULL;

  if (on->state < OSPF6_NEIGHBOR_EXCHANGE)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (OSPF6_MESSAGE_TYPE_LSACK, SEND))
        zlog_info ("Quit to send LSAck to neighbor %s state %s",
                   on->name, ospf6_neighbor_state_str[on->state]);
      return 0;
    }

  /* if we have nothing to send, return */
  if (on->lsack_list->count == 0)
    return 0;

  memset (sendbuf, 0, iobuflen);
  oh = (struct ospf6_header *) sendbuf;

  p = (char *)((caddr_t) oh + sizeof (struct ospf6_header));

  for (lsa = ospf6_lsdb_head (on->lsack_list); lsa;
       lsa = ospf6_lsdb_next (lsa))
    {
      /* MTU check */
      if (p - sendbuf + sizeof (struct ospf6_lsa_header) > on->ospf6_if->ifmtu)
        {
          /* if we run out of packet size/space here,
             better to try again soon. */
          THREAD_OFF (on->thread_send_lsack);
          on->thread_send_lsack =
            thread_add_event (master, ospf6_lsack_send_neighbor, on, 0);

          ospf6_lsa_unlock (lsa);
          break;
        }

      ospf6_lsa_age_update_to_send (lsa, on->ospf6_if->transdelay);
      memcpy (p, lsa->header, sizeof (struct ospf6_lsa_header));
      p += sizeof (struct ospf6_lsa_header);

      assert (lsa->lock == 2);
      ospf6_lsdb_remove (lsa, on->lsack_list);
    }

  oh->type = OSPF6_MESSAGE_TYPE_LSACK;
  oh->length = htons (p - sendbuf);

  ospf6_send (on->ospf6_if->linklocal_addr, &on->linklocal_addr,
              on->ospf6_if, oh);
  return 0;
}

int
ospf6_lsack_send_interface (struct thread *thread)
{
  struct ospf6_interface *oi;
  struct ospf6_header *oh;
  char *p;
  struct ospf6_lsa *lsa;

  oi = (struct ospf6_interface *) THREAD_ARG (thread);
  oi->thread_send_lsack = (struct thread *) NULL;

  if (oi->state <= OSPF6_INTERFACE_WAITING)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (OSPF6_MESSAGE_TYPE_LSACK, SEND))
        zlog_info ("Quit to send LSAck to interface %s state %s",
                   oi->interface->name, ospf6_interface_state_str[oi->state]);
      return 0;
    }

  /* if we have nothing to send, return */
  if (oi->lsack_list->count == 0)
    return 0;

  memset (sendbuf, 0, iobuflen);
  oh = (struct ospf6_header *) sendbuf;

  p = (char *)((caddr_t) oh + sizeof (struct ospf6_header));

  for (lsa = ospf6_lsdb_head (oi->lsack_list); lsa;
       lsa = ospf6_lsdb_next (lsa))
    {
      /* MTU check */
      if (p - sendbuf + sizeof (struct ospf6_lsa_header) > oi->ifmtu)
        {
          /* if we run out of packet size/space here,
             better to try again soon. */
          THREAD_OFF (oi->thread_send_lsack);
          oi->thread_send_lsack =
            thread_add_event (master, ospf6_lsack_send_interface, oi, 0);

          ospf6_lsa_unlock (lsa);
          break;
        }

      ospf6_lsa_age_update_to_send (lsa, oi->transdelay);
      memcpy (p, lsa->header, sizeof (struct ospf6_lsa_header));
      p += sizeof (struct ospf6_lsa_header);

      assert (lsa->lock == 2);
      ospf6_lsdb_remove (lsa, oi->lsack_list);
    }

  oh->type = OSPF6_MESSAGE_TYPE_LSACK;
  oh->length = htons (p - sendbuf);

  if (oi->state == OSPF6_INTERFACE_DR ||
      oi->state == OSPF6_INTERFACE_BDR)
    ospf6_send (oi->linklocal_addr, &allspfrouters6, oi, oh);
  else
    ospf6_send (oi->linklocal_addr, &alldrouters6, oi, oh);

  if (oi->thread_send_lsack == NULL && oi->lsack_list->count > 0)
    {
      oi->thread_send_lsack =
        thread_add_event (master, ospf6_lsack_send_interface, oi, 0);
    }

  return 0;
}


/* Commands */
DEFUN (debug_ospf6_message,
       debug_ospf6_message_cmd,
       "debug ospf6 message (unknown|hello|dbdesc|lsreq|lsupdate|lsack|all)",
       DEBUG_STR
       OSPF6_STR
       "Debug OSPFv3 message\n"
       "Debug Unknown message\n"
       "Debug Hello message\n"
       "Debug Database Description message\n"
       "Debug Link State Request message\n"
       "Debug Link State Update message\n"
       "Debug Link State Acknowledgement message\n"
       "Debug All message\n"
       )
{
  unsigned char level = 0;
  int type = 0;
  int i;

  assert (argc > 0);

  /* check type */
  if (! strncmp (argv[0], "u", 1))
    type = OSPF6_MESSAGE_TYPE_UNKNOWN;
  else if (! strncmp (argv[0], "h", 1))
    type = OSPF6_MESSAGE_TYPE_HELLO;
  else if (! strncmp (argv[0], "d", 1))
    type = OSPF6_MESSAGE_TYPE_DBDESC;
  else if (! strncmp (argv[0], "lsr", 3))
    type = OSPF6_MESSAGE_TYPE_LSREQ;
  else if (! strncmp (argv[0], "lsu", 3))
    type = OSPF6_MESSAGE_TYPE_LSUPDATE;
  else if (! strncmp (argv[0], "lsa", 3))
    type = OSPF6_MESSAGE_TYPE_LSACK;
  else if (! strncmp (argv[0], "a", 1))
    type = OSPF6_MESSAGE_TYPE_ALL;

  if (argc == 1)
    level = OSPF6_DEBUG_MESSAGE_SEND | OSPF6_DEBUG_MESSAGE_RECV;
  else if (! strncmp (argv[1], "s", 1))
    level = OSPF6_DEBUG_MESSAGE_SEND;
  else if (! strncmp (argv[1], "r", 1))
    level = OSPF6_DEBUG_MESSAGE_RECV;

  if (type == OSPF6_MESSAGE_TYPE_ALL)
    {
      for (i = 0; i < 6; i++)
        OSPF6_DEBUG_MESSAGE_ON (i, level);
    }
  else
    OSPF6_DEBUG_MESSAGE_ON (type, level);

  return CMD_SUCCESS;
}

ALIAS (debug_ospf6_message,
       debug_ospf6_message_sendrecv_cmd,
       "debug ospf6 message (unknown|hello|dbdesc|lsreq|lsupdate|lsack|all) (send|recv)",
       DEBUG_STR
       OSPF6_STR
       "Debug OSPFv3 message\n"
       "Debug Unknown message\n"
       "Debug Hello message\n"
       "Debug Database Description message\n"
       "Debug Link State Request message\n"
       "Debug Link State Update message\n"
       "Debug Link State Acknowledgement message\n"
       "Debug All message\n"
       "Debug only sending message\n"
       "Debug only receiving message\n"
       );


DEFUN (no_debug_ospf6_message,
       no_debug_ospf6_message_cmd,
       "no debug ospf6 message (unknown|hello|dbdesc|lsreq|lsupdate|lsack|all)",
       NO_STR
       DEBUG_STR
       OSPF6_STR
       "Debug OSPFv3 message\n"
       "Debug Unknown message\n"
       "Debug Hello message\n"
       "Debug Database Description message\n"
       "Debug Link State Request message\n"
       "Debug Link State Update message\n"
       "Debug Link State Acknowledgement message\n"
       "Debug All message\n"
       )
{
  unsigned char level = 0;
  int type = 0;
  int i;

  assert (argc > 0);

  /* check type */
  if (! strncmp (argv[0], "u", 1))
    type = OSPF6_MESSAGE_TYPE_UNKNOWN;
  else if (! strncmp (argv[0], "h", 1))
    type = OSPF6_MESSAGE_TYPE_HELLO;
  else if (! strncmp (argv[0], "d", 1))
    type = OSPF6_MESSAGE_TYPE_DBDESC;
  else if (! strncmp (argv[0], "lsr", 3))
    type = OSPF6_MESSAGE_TYPE_LSREQ;
  else if (! strncmp (argv[0], "lsu", 3))
    type = OSPF6_MESSAGE_TYPE_LSUPDATE;
  else if (! strncmp (argv[0], "lsa", 3))
    type = OSPF6_MESSAGE_TYPE_LSACK;
  else if (! strncmp (argv[0], "a", 1))
    type = OSPF6_MESSAGE_TYPE_ALL;

  if (argc == 1)
    level = OSPF6_DEBUG_MESSAGE_SEND | OSPF6_DEBUG_MESSAGE_RECV;
  else if (! strncmp (argv[1], "s", 1))
    level = OSPF6_DEBUG_MESSAGE_SEND;
  else if (! strncmp (argv[1], "r", 1))
    level = OSPF6_DEBUG_MESSAGE_RECV;

  if (type == OSPF6_MESSAGE_TYPE_ALL)
    {
      for (i = 0; i < 6; i++)
        OSPF6_DEBUG_MESSAGE_OFF (i, level);
    }
  else
    OSPF6_DEBUG_MESSAGE_OFF (type, level);

  return CMD_SUCCESS;
}

ALIAS (no_debug_ospf6_message,
       no_debug_ospf6_message_sendrecv_cmd,
       "no debug ospf6 message "
       "(unknown|hello|dbdesc|lsreq|lsupdate|lsack|all) (send|recv)",
       NO_STR
       DEBUG_STR
       OSPF6_STR
       "Debug OSPFv3 message\n"
       "Debug Unknown message\n"
       "Debug Hello message\n"
       "Debug Database Description message\n"
       "Debug Link State Request message\n"
       "Debug Link State Update message\n"
       "Debug Link State Acknowledgement message\n"
       "Debug All message\n"
       "Debug only sending message\n"
       "Debug only receiving message\n"
       );

int
config_write_ospf6_debug_message (struct vty *vty)
{
  char *type_str[] = {"unknown", "hello", "dbdesc",
                      "lsreq", "lsupdate", "lsack"};
  unsigned char s = 0, r = 0;
  int i;

  for (i = 0; i < 6; i++)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (i, SEND))
        s |= 1 << i;
      if (IS_OSPF6_DEBUG_MESSAGE (i, RECV))
        r |= 1 << i;
    }

  if (s == 0x3f && r == 0x3f)
    {
      vty_out (vty, "debug ospf6 message all%s", VNL);
      return 0;
    }

  if (s == 0x3f && r == 0)
    {
      vty_out (vty, "debug ospf6 message all send%s", VNL);
      return 0;
    }
  else if (s == 0 && r == 0x3f)
    {
      vty_out (vty, "debug ospf6 message all recv%s", VNL);
      return 0;
    }

  /* Unknown message is logged by default */
  if (! IS_OSPF6_DEBUG_MESSAGE (OSPF6_MESSAGE_TYPE_UNKNOWN, SEND) &&
      ! IS_OSPF6_DEBUG_MESSAGE (OSPF6_MESSAGE_TYPE_UNKNOWN, RECV))
    vty_out (vty, "no debug ospf6 message unknown%s", VNL);
  else if (! IS_OSPF6_DEBUG_MESSAGE (OSPF6_MESSAGE_TYPE_UNKNOWN, SEND))
    vty_out (vty, "no debug ospf6 message unknown send%s", VNL);
  else if (! IS_OSPF6_DEBUG_MESSAGE (OSPF6_MESSAGE_TYPE_UNKNOWN, RECV))
    vty_out (vty, "no debug ospf6 message unknown recv%s", VNL);

  for (i = 1; i < 6; i++)
    {
      if (IS_OSPF6_DEBUG_MESSAGE (i, SEND) &&
          IS_OSPF6_DEBUG_MESSAGE (i, RECV))
        vty_out (vty, "debug ospf6 message %s%s", type_str[i], VNL);
      else if (IS_OSPF6_DEBUG_MESSAGE (i, SEND))
        vty_out (vty, "debug ospf6 message %s send%s", type_str[i],
                 VNL);
      else if (IS_OSPF6_DEBUG_MESSAGE (i, RECV))
        vty_out (vty, "debug ospf6 message %s recv%s", type_str[i],
                 VNL);
    }

  return 0;
}

void
install_element_ospf6_debug_message ()
{
  install_element (ENABLE_NODE, &debug_ospf6_message_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf6_message_cmd);
  install_element (ENABLE_NODE, &debug_ospf6_message_sendrecv_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf6_message_sendrecv_cmd);
  install_element (CONFIG_NODE, &debug_ospf6_message_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf6_message_cmd);
  install_element (CONFIG_NODE, &debug_ospf6_message_sendrecv_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf6_message_sendrecv_cmd);
}


