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

/* Include other stuffs */
#include "log.h"
#include "linklist.h"
#include "vector.h"
#include "vty.h"
#include "command.h"
#include "memory.h"
#include "thread.h"

#include "ospf6_proto.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_message.h"

#include "ospf6_top.h"
#include "ospf6_area.h"
#include "ospf6_interface.h"
#include "ospf6_neighbor.h"

#include "ospf6_flood.h"
#include "ospf6d.h"

vector ospf6_lsa_handler_vector;

int
ospf6_unknown_lsa_show (struct vty *vty, struct ospf6_lsa *lsa)
{
  u_char *start, *end, *current;
  char byte[4];

  start = (u_char *) lsa->header + sizeof (struct ospf6_lsa_header);
  end = (u_char *) lsa->header + ntohs (lsa->header->length);

  vty_out (vty, "        Unknown contents:%s", VNL);
  for (current = start; current < end; current ++)
    {
      if ((current - start) % 16 == 0)
        vty_out (vty, "%s        ", VNL);
      else if ((current - start) % 4 == 0)
        vty_out (vty, " ");

      snprintf (byte, sizeof (byte), "%02x", *current);
      vty_out (vty, "%s", byte);
    }

  vty_out (vty, "%s%s", VNL, VNL);
  return 0;
}

struct ospf6_lsa_handler unknown_handler =
{
  OSPF6_LSTYPE_UNKNOWN,
  "Unknown",
  ospf6_unknown_lsa_show,
  OSPF6_LSA_DEBUG,
};

void
ospf6_install_lsa_handler (struct ospf6_lsa_handler *handler)
{
  /* type in handler is host byte order */
  int index = handler->type & OSPF6_LSTYPE_FCODE_MASK;
  vector_set_index (ospf6_lsa_handler_vector, index, handler);
}

struct ospf6_lsa_handler *
ospf6_get_lsa_handler (u_int16_t type)
{
  struct ospf6_lsa_handler *handler = NULL;
  int index = ntohs (type) & OSPF6_LSTYPE_FCODE_MASK;

  if (ospf6_lsa_handler_vector &&
      index < vector_max (ospf6_lsa_handler_vector))
    handler = vector_slot (ospf6_lsa_handler_vector, index);
  else
    handler = &unknown_handler;

  if (handler == NULL)
    handler = &unknown_handler;

  return handler;
}

char *
ospf6_lstype_name (u_int16_t type)
{
  static char buf[8];
  struct ospf6_lsa_handler *handler;

  handler = ospf6_get_lsa_handler (type);
  if (handler && handler != &unknown_handler)
    return handler->name;

  snprintf (buf, sizeof (buf), "0x%04hx", ntohs (type));
  return buf;
}

u_char
ospf6_lstype_debug (u_int16_t type)
{
  struct ospf6_lsa_handler *handler;
  handler = ospf6_get_lsa_handler (type);
  return handler->debug;
}

/* RFC2328: Section 13.2 */
int
ospf6_lsa_is_differ (struct ospf6_lsa *lsa1,
                     struct ospf6_lsa *lsa2)
{
  int len;

  assert (OSPF6_LSA_IS_SAME (lsa1, lsa2));

  /* XXX, Options ??? */

  ospf6_lsa_age_current (lsa1);
  ospf6_lsa_age_current (lsa2);
  if (ntohs (lsa1->header->age) == MAXAGE &&
      ntohs (lsa2->header->age) != MAXAGE)
    return 1;
  if (ntohs (lsa1->header->age) != MAXAGE &&
      ntohs (lsa2->header->age) == MAXAGE)
    return 1;

  /* compare body */
  if (ntohs (lsa1->header->length) != ntohs (lsa2->header->length))
    return 1;

  len = ntohs (lsa1->header->length) - sizeof (struct ospf6_lsa_header);
  return memcmp (lsa1->header + 1, lsa2->header + 1, len);
}

int
ospf6_lsa_is_changed (struct ospf6_lsa *lsa1,
                      struct ospf6_lsa *lsa2)
{
  int length;

  if (OSPF6_LSA_IS_MAXAGE (lsa1) ^ OSPF6_LSA_IS_MAXAGE (lsa2))
    return 1;
  if (ntohs (lsa1->header->length) != ntohs (lsa2->header->length))
    return 1;

  length = OSPF6_LSA_SIZE (lsa1->header) - sizeof (struct ospf6_lsa_header);
  assert (length > 0);

  return memcmp (OSPF6_LSA_HEADER_END (lsa1->header),
                 OSPF6_LSA_HEADER_END (lsa2->header), length);
}

/* ospf6 age functions */
/* calculate birth */
static void
ospf6_lsa_age_set (struct ospf6_lsa *lsa)
{
  struct timeval now;

  assert (lsa && lsa->header);

  if (gettimeofday (&now, (struct timezone *)NULL) < 0)
    zlog_warn ("LSA: gettimeofday failed, may fail LSA AGEs: %s",
               strerror (errno));

  lsa->birth.tv_sec = now.tv_sec - ntohs (lsa->header->age);
  lsa->birth.tv_usec = now.tv_usec;

  return;
}

/* this function calculates current age from its birth,
   then update age field of LSA header. return value is current age */
u_int16_t
ospf6_lsa_age_current (struct ospf6_lsa *lsa)
{
  struct timeval now;
  u_int32_t ulage;
  u_int16_t age;

  assert (lsa);
  assert (lsa->header);

  /* current time */
  if (gettimeofday (&now, (struct timezone *)NULL) < 0)
    zlog_warn ("LSA: gettimeofday failed, may fail LSA AGEs: %s",
               strerror (errno));

  /* calculate age */
  ulage = now.tv_sec - lsa->birth.tv_sec;

  /* if over MAXAGE, set to it */
  age = (ulage > MAXAGE ? MAXAGE : ulage);

  lsa->header->age = htons (age);
  return age;
}

/* update age field of LSA header with adding InfTransDelay */
void
ospf6_lsa_age_update_to_send (struct ospf6_lsa *lsa, u_int32_t transdelay)
{
  unsigned short age;

  age = ospf6_lsa_age_current (lsa) + transdelay;
  if (age > MAXAGE)
    age = MAXAGE;
  lsa->header->age = htons (age);
}

void
ospf6_lsa_premature_aging (struct ospf6_lsa *lsa)
{
  /* log */
  if (IS_OSPF6_DEBUG_LSA_TYPE (lsa->header->type))
    zlog_info ("LSA: Premature aging: %s", lsa->name);

  THREAD_OFF (lsa->expire);
  THREAD_OFF (lsa->refresh);

  memset (&lsa->birth, 0, sizeof (struct timeval));
  thread_execute (master, ospf6_lsa_expire, lsa, 0);
}

/* check which is more recent. if a is more recent, return -1;
   if the same, return 0; otherwise(b is more recent), return 1 */
int
ospf6_lsa_compare (struct ospf6_lsa *a, struct ospf6_lsa *b)
{
  signed long seqnuma, seqnumb;
  u_int16_t cksuma, cksumb;
  u_int16_t agea, ageb;

  assert (a && a->header);
  assert (b && b->header);
  assert (OSPF6_LSA_IS_SAME (a, b));

  seqnuma = ((signed long) ntohl (a->header->seqnum))
             - (signed long) INITIAL_SEQUENCE_NUMBER;
  seqnumb = ((signed long) ntohl (b->header->seqnum))
             - (signed long) INITIAL_SEQUENCE_NUMBER;

  /* compare by sequence number */
  /* XXX, LS sequence number wrapping */
  if (seqnuma > seqnumb)
    return -1;
  else if (seqnuma < seqnumb)
    return 1;

  /* Checksum */
  cksuma = ntohs (a->header->checksum);
  cksumb = ntohs (b->header->checksum);
  if (cksuma > cksumb)
    return -1;
  if (cksuma < cksumb)
    return 0;

  /* Update Age */
  agea = ospf6_lsa_age_current (a);
  ageb = ospf6_lsa_age_current (b);

  /* MaxAge check */
  if (agea == MAXAGE && ageb != MAXAGE)
    return -1;
  else if (agea != MAXAGE && ageb == MAXAGE)
    return 1;

  /* Age check */
  if (agea > ageb && agea - ageb >= MAX_AGE_DIFF)
    return 1;
  else if (agea < ageb && ageb - agea >= MAX_AGE_DIFF)
    return -1;

  /* neither recent */
  return 0;
}

char *
ospf6_lsa_printbuf (struct ospf6_lsa *lsa, char *buf, int size)
{
  char id[16], adv_router[16];
  inet_ntop (AF_INET, &lsa->header->id, id, sizeof (id));
  inet_ntop (AF_INET, &lsa->header->adv_router, adv_router,
             sizeof (adv_router));
  snprintf (buf, size, "[%s Id:%s Adv:%s]",
            ospf6_lstype_name (lsa->header->type), id, adv_router);
  return buf;
}

void
ospf6_lsa_header_print_raw (struct ospf6_lsa_header *header)
{
  char id[16], adv_router[16];
  inet_ntop (AF_INET, &header->id, id, sizeof (id));
  inet_ntop (AF_INET, &header->adv_router, adv_router,
             sizeof (adv_router));
  zlog_info ("    [%s Id:%s Adv:%s]",
             ospf6_lstype_name (header->type), id, adv_router);
  zlog_info ("    Age: %4hu SeqNum: %#08lx Cksum: %04hx Len: %d",
             ntohs (header->age), (u_long) ntohl (header->seqnum),
             ntohs (header->checksum), ntohs (header->length));
}

void
ospf6_lsa_header_print (struct ospf6_lsa *lsa)
{
  ospf6_lsa_age_current (lsa);
  ospf6_lsa_header_print_raw (lsa->header);
}

void
ospf6_lsa_show_summary_header (struct vty *vty)
{
  vty_out (vty, "%-12s %-15s %-15s %4s %8s %4s %4s %-8s%s",
           "Type", "LSId", "AdvRouter", "Age", "SeqNum",
           "Cksm", "Len", "Duration", VNL);
}

void
ospf6_lsa_show_summary (struct vty *vty, struct ospf6_lsa *lsa)
{
  char adv_router[16], id[16];
  struct timeval now, res;
  char duration[16];

  assert (lsa);
  assert (lsa->header);

  inet_ntop (AF_INET, &lsa->header->id, id, sizeof (id));
  inet_ntop (AF_INET, &lsa->header->adv_router, adv_router,
             sizeof (adv_router));

  gettimeofday (&now, NULL);
  timersub (&now, &lsa->installed, &res);
  timerstring (&res, duration, sizeof (duration));

  vty_out (vty, "%-12s %-15s %-15s %4hu %8lx %04hx %4hu %8s%s",
           ospf6_lstype_name (lsa->header->type),
           id, adv_router, ospf6_lsa_age_current (lsa),
           (u_long) ntohl (lsa->header->seqnum),
           ntohs (lsa->header->checksum), ntohs (lsa->header->length),
           duration, VNL);
}

void
ospf6_lsa_show_dump (struct vty *vty, struct ospf6_lsa *lsa)
{
  u_char *start, *end, *current;
  char byte[4];

  start = (u_char *) lsa->header;
  end = (u_char *) lsa->header + ntohs (lsa->header->length);

  vty_out (vty, "%s", VNL);
  vty_out (vty, "%s:%s", lsa->name, VNL);

  for (current = start; current < end; current ++)
    {
      if ((current - start) % 16 == 0)
        vty_out (vty, "%s        ", VNL);
      else if ((current - start) % 4 == 0)
        vty_out (vty, " ");

      snprintf (byte, sizeof (byte), "%02x", *current);
      vty_out (vty, "%s", byte);
    }

  vty_out (vty, "%s%s", VNL, VNL);
  return;
}

void
ospf6_lsa_show_internal (struct vty *vty, struct ospf6_lsa *lsa)
{
  char adv_router[64], id[64];

  assert (lsa && lsa->header);

  inet_ntop (AF_INET, &lsa->header->id, id, sizeof (id));
  inet_ntop (AF_INET, &lsa->header->adv_router,
             adv_router, sizeof (adv_router));

  vty_out (vty, "%s", VNL);
  vty_out (vty, "Age: %4hu Type: %s%s", ospf6_lsa_age_current (lsa),
           ospf6_lstype_name (lsa->header->type), VNL);
  vty_out (vty, "Link State ID: %s%s", id, VNL);
  vty_out (vty, "Advertising Router: %s%s", adv_router, VNL);
  vty_out (vty, "LS Sequence Number: %#010lx%s",
           (u_long) ntohl (lsa->header->seqnum), VNL);
  vty_out (vty, "CheckSum: %#06hx Length: %hu%s",
           ntohs (lsa->header->checksum),
           ntohs (lsa->header->length), VNL);
  vty_out (vty, "    Prev: %p This: %p Next: %p%s",
           lsa->prev, lsa, lsa->next, VNL);
  vty_out (vty, "%s", VNL);
  return;
}

void
ospf6_lsa_show (struct vty *vty, struct ospf6_lsa *lsa)
{
  char adv_router[64], id[64];
  struct ospf6_lsa_handler *handler;

  assert (lsa && lsa->header);

  inet_ntop (AF_INET, &lsa->header->id, id, sizeof (id));
  inet_ntop (AF_INET, &lsa->header->adv_router,
             adv_router, sizeof (adv_router));

  vty_out (vty, "Age: %4hu Type: %s%s", ospf6_lsa_age_current (lsa),
           ospf6_lstype_name (lsa->header->type), VNL);
  vty_out (vty, "Link State ID: %s%s", id, VNL);
  vty_out (vty, "Advertising Router: %s%s", adv_router, VNL);
  vty_out (vty, "LS Sequence Number: %#010lx%s",
           (u_long) ntohl (lsa->header->seqnum), VNL);
  vty_out (vty, "CheckSum: %#06hx Length: %hu%s",
           ntohs (lsa->header->checksum),
           ntohs (lsa->header->length), VNL);

  handler = ospf6_get_lsa_handler (lsa->header->type);
  if (handler->show == NULL)
    handler = &unknown_handler;
  (*handler->show) (vty, lsa);

  vty_out (vty, "%s", VNL);
}

/* OSPFv3 LSA creation/deletion function */
struct ospf6_lsa *
ospf6_lsa_create (struct ospf6_lsa_header *header)
{
  struct ospf6_lsa *lsa = NULL;
  struct ospf6_lsa_header *new_header = NULL;
  u_int16_t lsa_size = 0;

  /* size of the entire LSA */
  lsa_size = ntohs (header->length);   /* XXX vulnerable */

  /* allocate memory for this LSA */
  new_header = (struct ospf6_lsa_header *)
    XMALLOC (MTYPE_OSPF6_LSA, lsa_size);

  /* copy LSA from original header */
  memcpy (new_header, header, lsa_size);

  /* LSA information structure */
  /* allocate memory */
  lsa = (struct ospf6_lsa *)
    XMALLOC (MTYPE_OSPF6_LSA, sizeof (struct ospf6_lsa));
  memset (lsa, 0, sizeof (struct ospf6_lsa));

  lsa->header = (struct ospf6_lsa_header *) new_header;

  /* dump string */
  ospf6_lsa_printbuf (lsa, lsa->name, sizeof (lsa->name));

  /* calculate birth of this lsa */
  ospf6_lsa_age_set (lsa);

  return lsa;
}

struct ospf6_lsa *
ospf6_lsa_create_headeronly (struct ospf6_lsa_header *header)
{
  struct ospf6_lsa *lsa = NULL;
  struct ospf6_lsa_header *new_header = NULL;

  /* allocate memory for this LSA */
  new_header = (struct ospf6_lsa_header *)
    XMALLOC (MTYPE_OSPF6_LSA, sizeof (struct ospf6_lsa_header));

  /* copy LSA from original header */
  memcpy (new_header, header, sizeof (struct ospf6_lsa_header));

  /* LSA information structure */
  /* allocate memory */
  lsa = (struct ospf6_lsa *)
    XMALLOC (MTYPE_OSPF6_LSA, sizeof (struct ospf6_lsa));
  memset (lsa, 0, sizeof (struct ospf6_lsa));

  lsa->header = (struct ospf6_lsa_header *) new_header;
  SET_FLAG (lsa->flag, OSPF6_LSA_HEADERONLY);

  /* dump string */
  ospf6_lsa_printbuf (lsa, lsa->name, sizeof (lsa->name));

  /* calculate birth of this lsa */
  ospf6_lsa_age_set (lsa);

  return lsa;
}

void
ospf6_lsa_delete (struct ospf6_lsa *lsa)
{
  assert (lsa->lock == 0);

  /* cancel threads */
  THREAD_OFF (lsa->expire);
  THREAD_OFF (lsa->refresh);

  /* do free */
  XFREE (MTYPE_OSPF6_LSA, lsa->header);
  XFREE (MTYPE_OSPF6_LSA, lsa);
}

struct ospf6_lsa *
ospf6_lsa_copy (struct ospf6_lsa *lsa)
{
  struct ospf6_lsa *copy = NULL;

  ospf6_lsa_age_current (lsa);
  if (CHECK_FLAG (lsa->flag, OSPF6_LSA_HEADERONLY))
    copy = ospf6_lsa_create_headeronly (lsa->header);
  else
    copy = ospf6_lsa_create (lsa->header);
  assert (copy->lock == 0);

  copy->birth = lsa->birth;
  copy->originated = lsa->originated;
  copy->received = lsa->received;
  copy->installed = lsa->installed;
  copy->lsdb = lsa->lsdb;

  return copy;
}

/* increment reference counter of struct ospf6_lsa */
void
ospf6_lsa_lock (struct ospf6_lsa *lsa)
{
  lsa->lock++;
  return;
}

/* decrement reference counter of struct ospf6_lsa */
void
ospf6_lsa_unlock (struct ospf6_lsa *lsa)
{
  /* decrement reference counter */
  assert (lsa->lock > 0);
  lsa->lock--;

  if (lsa->lock != 0)
    return;

  ospf6_lsa_delete (lsa);
}


/* ospf6 lsa expiry */
int
ospf6_lsa_expire (struct thread *thread)
{
  struct ospf6_lsa *lsa;

  lsa = (struct ospf6_lsa *) THREAD_ARG (thread);

  assert (lsa && lsa->header);
  assert (OSPF6_LSA_IS_MAXAGE (lsa));
  assert (! lsa->refresh);

  lsa->expire = (struct thread *) NULL;

  if (IS_OSPF6_DEBUG_LSA_TYPE (lsa->header->type))
    {
      zlog_info ("LSA Expire:");
      ospf6_lsa_header_print (lsa);
    }

  if (CHECK_FLAG (lsa->flag, OSPF6_LSA_HEADERONLY))
    return 0;    /* dbexchange will do something ... */

  /* reflood lsa */
  ospf6_flood (NULL, lsa);

  /* reinstall lsa */
  ospf6_install_lsa (lsa);

  /* schedule maxage remover */
  ospf6_maxage_remove (ospf6);

  return 0;
}

int
ospf6_lsa_refresh (struct thread *thread)
{
  struct ospf6_lsa *old, *self, *new;
  struct ospf6_lsdb *lsdb_self;

  assert (thread);
  old = (struct ospf6_lsa *) THREAD_ARG (thread);
  assert (old && old->header);

  old->refresh = (struct thread *) NULL;

  lsdb_self = ospf6_get_scoped_lsdb_self (old);
  self = ospf6_lsdb_lookup (old->header->type, old->header->id,
                            old->header->adv_router, lsdb_self);
  if (self == NULL)
    {
      if (IS_OSPF6_DEBUG_LSA_TYPE (old->header->type))
        zlog_info ("Refresh: could not find self LSA, flush %s", old->name);
      ospf6_lsa_premature_aging (old);
      return 0;
    }

  /* Reset age, increment LS sequence number. */
  self->header->age = htons (0);
  self->header->seqnum =
    ospf6_new_ls_seqnum (self->header->type, self->header->id,
                         self->header->adv_router, old->lsdb);
  ospf6_lsa_checksum (self->header);

  new = ospf6_lsa_create (self->header);
  new->lsdb = old->lsdb;
  new->refresh = thread_add_timer (master, ospf6_lsa_refresh, new,
                                   LS_REFRESH_TIME);

  /* store it in the LSDB for self-originated LSAs */
  ospf6_lsdb_add (ospf6_lsa_copy (new), lsdb_self);

  if (IS_OSPF6_DEBUG_LSA_TYPE (new->header->type))
    {
      zlog_info ("LSA Refresh:");
      ospf6_lsa_header_print (new);
    }

  ospf6_flood_clear (old);
  ospf6_flood (NULL, new);
  ospf6_install_lsa (new);

  return 0;
}



/* enhanced Fletcher checksum algorithm, RFC1008 7.2 */
#define MODX                4102
#define LSA_CHECKSUM_OFFSET   15

unsigned short
ospf6_lsa_checksum (struct ospf6_lsa_header *lsa_header)
{
  u_char *sp, *ep, *p, *q;
  int c0 = 0, c1 = 0;
  int x, y;
  u_int16_t length;

  lsa_header->checksum = 0;
  length = ntohs (lsa_header->length) - 2;
  sp = (u_char *) &lsa_header->type;

  for (ep = sp + length; sp < ep; sp = q)
    {
      q = sp + MODX;
      if (q > ep)
        q = ep;
      for (p = sp; p < q; p++)
        {
          c0 += *p;
          c1 += c0;
        }
      c0 %= 255;
      c1 %= 255;
    }

  /* r = (c1 << 8) + c0; */
  x = ((length - LSA_CHECKSUM_OFFSET) * c0 - c1) % 255;
  if (x <= 0)
    x += 255;
  y = 510 - c0 - x;
  if (y > 255)
    y -= 255;

  lsa_header->checksum = htons ((x << 8) + y);

  return (lsa_header->checksum);
}

void
ospf6_lsa_init ()
{
  ospf6_lsa_handler_vector = vector_init (0);
  ospf6_install_lsa_handler (&unknown_handler);
}


char *
ospf6_lsa_handler_name (struct ospf6_lsa_handler *h)
{
  static char buf[64];
  int i, size = strlen (h->name);

  if (h->name == "Unknown" &&
      h->type != OSPF6_LSTYPE_UNKNOWN)
    {
      snprintf (buf, sizeof (buf), "%#04hx", h->type);
      return buf;
    }

  for (i = 0; i < MIN (size, sizeof (buf)); i++)
    {
      if (! islower (h->name[i]))
        buf[i] = tolower (h->name[i]);
      else
        buf[i] = h->name[i];
    }
  buf[size] = '\0';
  return buf;
}

DEFUN (debug_ospf6_lsa_type,
       debug_ospf6_lsa_hex_cmd,
       "debug ospf6 lsa XXXX/0xXXXX",
       DEBUG_STR
       OSPF6_STR
       "Debug Link State Advertisements (LSAs)\n"
       "Specify LS type as Hexadecimal\n"
      )
{
  int i;
  struct ospf6_lsa_handler *handler = NULL;
  unsigned long val;
  char *endptr = NULL;
  u_int16_t type = 0;

  assert (argc);

  if ((strlen (argv[0]) == 6 && ! strncmp (argv[0], "0x", 2)) ||
      (strlen (argv[0]) == 4))
    {
      val = strtoul (argv[0], &endptr, 16);
      if (*endptr == '\0')
        type = val;
    }

  for (i = 0; i < vector_max (ospf6_lsa_handler_vector); i++)
    {
      handler = vector_slot (ospf6_lsa_handler_vector, i);
      if (handler == NULL)
        continue;
      if (type && handler->type == type)
        break;
      if (! strcasecmp (argv[0], handler->name))
        break;
      handler = NULL;
    }

  if (type && handler == NULL)
    {
      handler = (struct ospf6_lsa_handler *)
        malloc (sizeof (struct ospf6_lsa_handler));
      memset (handler, 0, sizeof (struct ospf6_lsa_handler));
      handler->type = type;
      handler->name = "Unknown";
      handler->show = ospf6_unknown_lsa_show;
      vector_set_index (ospf6_lsa_handler_vector,
                        handler->type & OSPF6_LSTYPE_FCODE_MASK, handler);
    }

  if (handler == NULL)
    handler = &unknown_handler;

  if (argc >= 2)
    {
      if (! strcmp (argv[1], "originate"))
        SET_FLAG (handler->debug, OSPF6_LSA_DEBUG_ORIGINATE);
      if (! strcmp (argv[1], "examin"))
        SET_FLAG (handler->debug, OSPF6_LSA_DEBUG_EXAMIN);
      if (! strcmp (argv[1], "flooding"))
        SET_FLAG (handler->debug, OSPF6_LSA_DEBUG_FLOOD);
    }
  else
    SET_FLAG (handler->debug, OSPF6_LSA_DEBUG);

  return CMD_SUCCESS;
}

DEFUN (no_debug_ospf6_lsa_type,
       no_debug_ospf6_lsa_hex_cmd,
       "no debug ospf6 lsa XXXX/0xXXXX",
       NO_STR
       DEBUG_STR
       OSPF6_STR
       "Debug Link State Advertisements (LSAs)\n"
       "Specify LS type as Hexadecimal\n"
      )
{
  int i;
  struct ospf6_lsa_handler *handler = NULL;
  unsigned long val;
  char *endptr = NULL;
  u_int16_t type = 0;

  assert (argc);

  if ((strlen (argv[0]) == 6 && ! strncmp (argv[0], "0x", 2)) ||
      (strlen (argv[0]) == 4))
    {
      val = strtoul (argv[0], &endptr, 16);
      if (*endptr == '\0')
        type = val;
    }

  for (i = 0; i < vector_max (ospf6_lsa_handler_vector); i++)
    {
      handler = vector_slot (ospf6_lsa_handler_vector, i);
      if (handler == NULL)
        continue;
      if (type && handler->type == type)
        break;
      if (! strcasecmp (argv[0], handler->name))
        break;
    }

  if (handler == NULL)
    return CMD_SUCCESS;

  if (argc >= 2)
    {
      if (! strcmp (argv[1], "originate"))
        UNSET_FLAG (handler->debug, OSPF6_LSA_DEBUG_ORIGINATE);
      if (! strcmp (argv[1], "examin"))
        UNSET_FLAG (handler->debug, OSPF6_LSA_DEBUG_EXAMIN);
      if (! strcmp (argv[1], "flooding"))
        UNSET_FLAG (handler->debug, OSPF6_LSA_DEBUG_FLOOD);
    }
  else
    UNSET_FLAG (handler->debug, OSPF6_LSA_DEBUG);

  if (handler->debug == 0 &&
      handler->name == "Unknown" && type != OSPF6_LSTYPE_UNKNOWN)
    {
      free (handler);
      vector_slot (ospf6_lsa_handler_vector, i) = NULL;
    }

  return CMD_SUCCESS;
}

struct cmd_element debug_ospf6_lsa_type_cmd;
struct cmd_element debug_ospf6_lsa_type_detail_cmd;
struct cmd_element no_debug_ospf6_lsa_type_cmd;
struct cmd_element no_debug_ospf6_lsa_type_detail_cmd;

void
install_element_ospf6_debug_lsa ()
{
  int i;
  struct ospf6_lsa_handler *handler;
#define STRSIZE  256
#define DOCSIZE  1024
  static char strbuf[STRSIZE];
  static char docbuf[DOCSIZE];
  static char detail_strbuf[STRSIZE];
  static char detail_docbuf[DOCSIZE];
  char *str, *no_str;
  char *doc, *no_doc;

  strbuf[0] = '\0';
  no_str = &strbuf[strlen (strbuf)];
  strncat (strbuf, "no ", STRSIZE - strlen (strbuf));
  str = &strbuf[strlen (strbuf)];

  strncat (strbuf, "debug ospf6 lsa (", STRSIZE - strlen (strbuf));
  for (i = 0; i < vector_max (ospf6_lsa_handler_vector); i++)
    {
      handler = vector_slot (ospf6_lsa_handler_vector, i);
      if (handler == NULL)
        continue;
      strncat (strbuf, ospf6_lsa_handler_name (handler),
               STRSIZE - strlen (strbuf));
      strncat (strbuf, "|", STRSIZE - strlen (strbuf));
    }
  strbuf[strlen (strbuf) - 1] = ')';
  strbuf[strlen (strbuf)] = '\0';

  docbuf[0] = '\0';
  no_doc = &docbuf[strlen (docbuf)];
  strncat (docbuf, NO_STR, DOCSIZE - strlen (docbuf));
  doc = &docbuf[strlen (docbuf)];

  strncat (docbuf, DEBUG_STR, DOCSIZE - strlen (docbuf));
  strncat (docbuf, OSPF6_STR, DOCSIZE - strlen (docbuf));
  strncat (docbuf, "Debug Link State Advertisements (LSAs)\n",
           DOCSIZE - strlen (docbuf));

  for (i = 0; i < vector_max (ospf6_lsa_handler_vector); i++)
    {
      handler = vector_slot (ospf6_lsa_handler_vector, i);
      if (handler == NULL)
        continue;
      strncat (docbuf, "Debug ", DOCSIZE - strlen (docbuf));
      strncat (docbuf, handler->name, DOCSIZE - strlen (docbuf));
      strncat (docbuf, "-LSA\n", DOCSIZE - strlen (docbuf));
    }
  docbuf[strlen (docbuf)] = '\0';

  debug_ospf6_lsa_type_cmd.string = str;
  debug_ospf6_lsa_type_cmd.func = debug_ospf6_lsa_type;
  debug_ospf6_lsa_type_cmd.doc = doc;

  no_debug_ospf6_lsa_type_cmd.string = no_str;
  no_debug_ospf6_lsa_type_cmd.func = no_debug_ospf6_lsa_type;
  no_debug_ospf6_lsa_type_cmd.doc = no_doc;

  strncpy (detail_strbuf, strbuf, STRSIZE);
  strncat (detail_strbuf, " (originate|examin|flooding)",
           STRSIZE - strlen (detail_strbuf));
  detail_strbuf[strlen (detail_strbuf)] = '\0';
  no_str = &detail_strbuf[0];
  str = &detail_strbuf[strlen ("no ")];

  strncpy (detail_docbuf, docbuf, DOCSIZE);
  strncat (detail_docbuf, "Debug Originating LSA\n",
           DOCSIZE - strlen (detail_docbuf));
  strncat (detail_docbuf, "Debug Examining LSA\n",
           DOCSIZE - strlen (detail_docbuf));
  strncat (detail_docbuf, "Debug Flooding LSA\n",
           DOCSIZE - strlen (detail_docbuf));
  detail_docbuf[strlen (detail_docbuf)] = '\0';
  no_doc = &detail_docbuf[0];
  doc = &detail_docbuf[strlen (NO_STR)];

  debug_ospf6_lsa_type_detail_cmd.string = str;
  debug_ospf6_lsa_type_detail_cmd.func = debug_ospf6_lsa_type;
  debug_ospf6_lsa_type_detail_cmd.doc = doc;

  no_debug_ospf6_lsa_type_detail_cmd.string = no_str;
  no_debug_ospf6_lsa_type_detail_cmd.func = no_debug_ospf6_lsa_type;
  no_debug_ospf6_lsa_type_detail_cmd.doc = no_doc;

  install_element (ENABLE_NODE, &debug_ospf6_lsa_hex_cmd);
  install_element (ENABLE_NODE, &debug_ospf6_lsa_type_cmd);
  install_element (ENABLE_NODE, &debug_ospf6_lsa_type_detail_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf6_lsa_hex_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf6_lsa_type_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf6_lsa_type_detail_cmd);
  install_element (CONFIG_NODE, &debug_ospf6_lsa_hex_cmd);
  install_element (CONFIG_NODE, &debug_ospf6_lsa_type_cmd);
  install_element (CONFIG_NODE, &debug_ospf6_lsa_type_detail_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf6_lsa_hex_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf6_lsa_type_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf6_lsa_type_detail_cmd);
}

int
config_write_ospf6_debug_lsa (struct vty *vty)
{
  int i;
  struct ospf6_lsa_handler *handler;

  for (i = 0; i < vector_max (ospf6_lsa_handler_vector); i++)
    {
      handler = vector_slot (ospf6_lsa_handler_vector, i);
      if (handler == NULL)
        continue;
      if (CHECK_FLAG (handler->debug, OSPF6_LSA_DEBUG))
        vty_out (vty, "debug ospf6 lsa %s%s",
                 ospf6_lsa_handler_name (handler), VNL);
      if (CHECK_FLAG (handler->debug, OSPF6_LSA_DEBUG_ORIGINATE))
        vty_out (vty, "debug ospf6 lsa %s originate%s",
                 ospf6_lsa_handler_name (handler), VNL);
      if (CHECK_FLAG (handler->debug, OSPF6_LSA_DEBUG_EXAMIN))
        vty_out (vty, "debug ospf6 lsa %s examin%s",
                 ospf6_lsa_handler_name (handler), VNL);
      if (CHECK_FLAG (handler->debug, OSPF6_LSA_DEBUG_FLOOD))
        vty_out (vty, "debug ospf6 lsa %s flooding%s",
                 ospf6_lsa_handler_name (handler), VNL);
    }

  return 0;
}


