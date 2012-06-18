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

#ifndef OSPF6_LSA_H
#define OSPF6_LSA_H

/* Debug option */
#define OSPF6_LSA_DEBUG           0x01
#define OSPF6_LSA_DEBUG_ORIGINATE 0x02
#define OSPF6_LSA_DEBUG_EXAMIN    0x04
#define OSPF6_LSA_DEBUG_FLOOD     0x08

#define IS_OSPF6_DEBUG_LSA(name)                        \
  (ospf6_lstype_debug (htons (OSPF6_LSTYPE_ ## name)) & \
   OSPF6_LSA_DEBUG)
#define IS_OSPF6_DEBUG_ORIGINATE(name)                  \
  (ospf6_lstype_debug (htons (OSPF6_LSTYPE_ ## name)) & \
   OSPF6_LSA_DEBUG_ORIGINATE)
#define IS_OSPF6_DEBUG_EXAMIN(name)                     \
  (ospf6_lstype_debug (htons (OSPF6_LSTYPE_ ## name)) & \
   OSPF6_LSA_DEBUG_EXAMIN)
#define IS_OSPF6_DEBUG_LSA_TYPE(type) \
  (ospf6_lstype_debug (type) & OSPF6_LSA_DEBUG)
#define IS_OSPF6_DEBUG_ORIGINATE_TYPE(type) \
  (ospf6_lstype_debug (type) & OSPF6_LSA_DEBUG_ORIGINATE)
#define IS_OSPF6_DEBUG_EXAMIN_TYPE(type) \
  (ospf6_lstype_debug (type) & OSPF6_LSA_DEBUG_EXAMIN)
#define IS_OSPF6_DEBUG_FLOOD_TYPE(type) \
  (ospf6_lstype_debug (type) & OSPF6_LSA_DEBUG_FLOOD)

/* LSA definition */

#define OSPF6_MAX_LSASIZE      4096

/* Type */
#define OSPF6_LSTYPE_UNKNOWN          0x0000
#define OSPF6_LSTYPE_ROUTER           0x2001
#define OSPF6_LSTYPE_NETWORK          0x2002
#define OSPF6_LSTYPE_INTER_PREFIX     0x2003
#define OSPF6_LSTYPE_INTER_ROUTER     0x2004
#define OSPF6_LSTYPE_AS_EXTERNAL      0x4005
#define OSPF6_LSTYPE_GROUP_MEMBERSHIP 0x2006
#define OSPF6_LSTYPE_TYPE_7           0x2007
#define OSPF6_LSTYPE_LINK             0x0008
#define OSPF6_LSTYPE_INTRA_PREFIX     0x2009
#define OSPF6_LSTYPE_SIZE             0x000a

/* Masks for LS Type : RFC 2740 A.4.2.1 "LS type" */
#define OSPF6_LSTYPE_UBIT_MASK        0x8000
#define OSPF6_LSTYPE_SCOPE_MASK       0x6000
#define OSPF6_LSTYPE_FCODE_MASK       0x1fff

/* LSA scope */
#define OSPF6_SCOPE_LINKLOCAL  0x0000
#define OSPF6_SCOPE_AREA       0x2000
#define OSPF6_SCOPE_AS         0x4000
#define OSPF6_SCOPE_RESERVED   0x6000

/* XXX U-bit handling should be treated here */
#define OSPF6_LSA_SCOPE(type) \
  (ntohs (type) & OSPF6_LSTYPE_SCOPE_MASK)

/* LSA Header */
struct ospf6_lsa_header
{
  u_int16_t age;        /* LS age */
  u_int16_t type;       /* LS type */
  u_int32_t id;         /* Link State ID */
  u_int32_t adv_router; /* Advertising Router */
  u_int32_t seqnum;     /* LS sequence number */
  u_int16_t checksum;   /* LS checksum */
  u_int16_t length;     /* LSA length */
};

#define OSPF6_LSA_HEADER_END(h) \
  ((caddr_t)(h) + sizeof (struct ospf6_lsa_header))
#define OSPF6_LSA_SIZE(h) \
  (ntohs (((struct ospf6_lsa_header *) (h))->length))
#define OSPF6_LSA_END(h) \
  ((caddr_t)(h) + ntohs (((struct ospf6_lsa_header *) (h))->length))
#define OSPF6_LSA_IS_TYPE(t, L) \
  ((L)->header->type == htons (OSPF6_LSTYPE_ ## t) ? 1 : 0)
#define OSPF6_LSA_IS_SAME(L1, L2) \
  ((L1)->header->adv_router == (L2)->header->adv_router && \
   (L1)->header->id == (L2)->header->id && \
   (L1)->header->type == (L2)->header->type)
#define OSPF6_LSA_IS_MATCH(t, i, a, L) \
  ((L)->header->adv_router == (a) && (L)->header->id == (i) && \
   (L)->header->type == (t))
#define OSPF6_LSA_IS_DIFFER(L1, L2)  ospf6_lsa_is_differ (L1, L2)
#define OSPF6_LSA_IS_MAXAGE(L) (ospf6_lsa_age_current (L) == MAXAGE)
#define OSPF6_LSA_IS_CHANGED(L1, L2) ospf6_lsa_is_changed (L1, L2)

struct ospf6_lsa
{
  char              name[64];   /* dump string */

  struct ospf6_lsa *prev;
  struct ospf6_lsa *next;

  unsigned char     lock;           /* reference counter */
  unsigned char     flag;           /* special meaning (e.g. floodback) */

  struct timeval    birth;          /* tv_sec when LS age 0 */
  struct timeval    originated;     /* used by MinLSInterval check */
  struct timeval    received;       /* used by MinLSArrival check */
  struct timeval    installed;

  struct thread    *expire;
  struct thread    *refresh;        /* For self-originated LSA */

  int               retrans_count;

  struct ospf6_lsdb *lsdb;

  /* lsa instance */
  struct ospf6_lsa_header *header;
};

#define OSPF6_LSA_HEADERONLY 0x01
#define OSPF6_LSA_FLOODBACK  0x02
#define OSPF6_LSA_DUPLICATE  0x04
#define OSPF6_LSA_IMPLIEDACK 0x08

struct ospf6_lsa_handler
{
  u_int16_t type; /* host byte order */
  char *name;
  int (*show) (struct vty *, struct ospf6_lsa *);
  u_char debug;
};

extern struct ospf6_lsa_handler unknown_handler;
#define OSPF6_LSA_IS_KNOWN(type) \
  (ospf6_get_lsa_handler (type) != &unknown_handler ? 1 : 0)

/* Macro for LSA Origination */
/* addr is (struct prefix *) */
#define CONTINUE_IF_ADDRESS_LINKLOCAL(debug,addr)      \
  if (IN6_IS_ADDR_LINKLOCAL (&(addr)->u.prefix6))      \
    {                                                  \
      char buf[64];                                    \
      prefix2str (addr, buf, sizeof (buf));            \
      if (debug)                                       \
        zlog_info ("Filter out Linklocal: %s", buf);   \
      continue;                                        \
    }

#define CONTINUE_IF_ADDRESS_UNSPECIFIED(debug,addr)    \
  if (IN6_IS_ADDR_UNSPECIFIED (&(addr)->u.prefix6))    \
    {                                                  \
      char buf[64];                                    \
      prefix2str (addr, buf, sizeof (buf));            \
      if (debug)                                       \
        zlog_info ("Filter out Unspecified: %s", buf); \
      continue;                                        \
    }

#define CONTINUE_IF_ADDRESS_LOOPBACK(debug,addr)       \
  if (IN6_IS_ADDR_LOOPBACK (&(addr)->u.prefix6))       \
    {                                                  \
      char buf[64];                                    \
      prefix2str (addr, buf, sizeof (buf));            \
      if (debug)                                       \
        zlog_info ("Filter out Loopback: %s", buf);    \
      continue;                                        \
    }

#define CONTINUE_IF_ADDRESS_V4COMPAT(debug,addr)       \
  if (IN6_IS_ADDR_V4COMPAT (&(addr)->u.prefix6))       \
    {                                                  \
      char buf[64];                                    \
      prefix2str (addr, buf, sizeof (buf));            \
      if (debug)                                       \
        zlog_info ("Filter out V4Compat: %s", buf);    \
      continue;                                        \
    }

#define CONTINUE_IF_ADDRESS_V4MAPPED(debug,addr)       \
  if (IN6_IS_ADDR_V4MAPPED (&(addr)->u.prefix6))       \
    {                                                  \
      char buf[64];                                    \
      prefix2str (addr, buf, sizeof (buf));            \
      if (debug)                                       \
        zlog_info ("Filter out V4Mapped: %s", buf);    \
      continue;                                        \
    }


/* Function Prototypes */
char *ospf6_lstype_name (u_int16_t type);
u_char ospf6_lstype_debug (u_int16_t type);
int ospf6_lsa_is_differ (struct ospf6_lsa *lsa1, struct ospf6_lsa *lsa2);
int ospf6_lsa_is_changed (struct ospf6_lsa *lsa1, struct ospf6_lsa *lsa2);
u_int16_t ospf6_lsa_age_current (struct ospf6_lsa *);
void ospf6_lsa_age_update_to_send (struct ospf6_lsa *, u_int32_t);
void ospf6_lsa_premature_aging (struct ospf6_lsa *);
int ospf6_lsa_compare (struct ospf6_lsa *, struct ospf6_lsa *);

char *ospf6_lsa_printbuf (struct ospf6_lsa *lsa, char *buf, int size);
void ospf6_lsa_header_print_raw (struct ospf6_lsa_header *header);
void ospf6_lsa_header_print (struct ospf6_lsa *lsa);
void ospf6_lsa_show_summary_header (struct vty *vty);
void ospf6_lsa_show_summary (struct vty *vty, struct ospf6_lsa *lsa);
void ospf6_lsa_show_dump (struct vty *vty, struct ospf6_lsa *lsa);
void ospf6_lsa_show_internal (struct vty *vty, struct ospf6_lsa *lsa);
void ospf6_lsa_show (struct vty *vty, struct ospf6_lsa *lsa);

struct ospf6_lsa *ospf6_lsa_create (struct ospf6_lsa_header *header);
struct ospf6_lsa *ospf6_lsa_create_headeronly (struct ospf6_lsa_header *header);
void ospf6_lsa_delete (struct ospf6_lsa *lsa);
struct ospf6_lsa *ospf6_lsa_copy (struct ospf6_lsa *);

void ospf6_lsa_lock (struct ospf6_lsa *);
void ospf6_lsa_unlock (struct ospf6_lsa *);

int ospf6_lsa_expire (struct thread *);
int ospf6_lsa_refresh (struct thread *);

unsigned short ospf6_lsa_checksum (struct ospf6_lsa_header *);
int ospf6_lsa_prohibited_duration (u_int16_t type, u_int32_t id,
                                   u_int32_t adv_router, void *scope);

void ospf6_install_lsa_handler (struct ospf6_lsa_handler *handler);
struct ospf6_lsa_handler *ospf6_get_lsa_handler (u_int16_t type);

void ospf6_lsa_init ();
void ospf6_lsa_cmd_init ();

int config_write_ospf6_debug_lsa (struct vty *vty);
void install_element_ospf6_debug_lsa ();

#endif /* OSPF6_LSA_H */

