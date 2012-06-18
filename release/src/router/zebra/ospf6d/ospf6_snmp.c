/* OSPFv3 SNMP support
 * Copyright (C) 2004 Yasuhiro Ohara
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

#ifdef HAVE_SNMP

#ifdef HAVE_NETSNMP
#include <net-snmp/net-snmp-config.h>
#endif /*HAVE_NETSNMP*/

#include <asn1.h>
#include <snmp.h>
#include <snmp_impl.h>

#include "log.h"
#include "vty.h"
#include "linklist.h"
#include "smux.h"

#include "ospf6_proto.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_route.h"
#include "ospf6_top.h"
#include "ospf6_area.h"
#include "ospf6_interface.h"
#include "ospf6_message.h"
#include "ospf6_neighbor.h"
#include "ospf6d.h"

/* OSPFv3-MIB */
#define OSPFv3MIB 1,3,6,1,3,102

/* Zebra enterprise ospf6d MIB */
#define OSPF6DOID 1,3,6,1,4,1,3317,1,2,6

/* OSPFv3 MIB General Group values. */
#define OSPFv3ROUTERID                   1
#define OSPFv3ADMINSTAT                  2
#define OSPFv3VERSIONNUMBER              3
#define OSPFv3AREABDRRTRSTATUS           4
#define OSPFv3ASBDRRTRSTATUS             5
#define OSPFv3ASSCOPELSACOUNT            6
#define OSPFv3ASSCOPELSACHECKSUMSUM      7
#define OSPFv3ORIGINATENEWLSAS           8
#define OSPFv3RXNEWLSAS                  9
#define OSPFv3EXTLSACOUNT               10
#define OSPFv3EXTAREALSDBLIMIT          11
#define OSPFv3MULTICASTEXTENSIONS       12
#define OSPFv3EXITOVERFLOWINTERVAL      13
#define OSPFv3DEMANDEXTENSIONS          14
#define OSPFv3TRAFFICENGINEERINGSUPPORT 15
#define OSPFv3REFERENCEBANDWIDTH        16
#define OSPFv3RESTARTSUPPORT            17
#define OSPFv3RESTARTINTERVAL           18
#define OSPFv3RESTARTSTATUS             19
#define OSPFv3RESTARTAGE                20
#define OSPFv3RESTARTEXITREASON         21

/* OSPFv3 MIB Area Table values. */
#define OSPFv3AREAID                     1
#define OSPFv3IMPORTASEXTERN             2
#define OSPFv3AREASPFRUNS                3
#define OSPFv3AREABDRRTRCOUNT            4
#define OSPFv3AREAASBDRRTRCOUNT          5
#define OSPFv3AREASCOPELSACOUNT          6
#define OSPFv3AREASCOPELSACKSUMSUM       7
#define OSPFv3AREASUMMARY                8
#define OSPFv3AREASTATUS                 9
#define OSPFv3STUBMETRIC                10
#define OSPFv3AREANSSATRANSLATORROLE    11
#define OSPFv3AREANSSATRANSLATORSTATE   12
#define OSPFv3AREANSSATRANSLATORSTABILITYINTERVAL    13
#define OSPFv3AREANSSATRANSLATOREVENTS  14
#define OSPFv3AREASTUBMETRICTYPE        15

/* OSPFv3 MIB Area Lsdb Table values. */
#define OSPFv3AREALSDBAREAID             1
#define OSPFv3AREALSDBTYPE               2
#define OSPFv3AREALSDBROUTERID           3
#define OSPFv3AREALSDBLSID               4
#define OSPFv3AREALSDBSEQUENCE           5
#define OSPFv3AREALSDBAGE                6
#define OSPFv3AREALSDBCHECKSUM           7
#define OSPFv3AREALSDBADVERTISEMENT      8
#define OSPFv3AREALSDBTYPEKNOWN          9

/* SYNTAX Status from OSPF-MIB. */
#define OSPF_STATUS_ENABLED  1
#define OSPF_STATUS_DISABLED 2

/* SNMP value hack. */
#define COUNTER     ASN_COUNTER
#define INTEGER     ASN_INTEGER
#define GAUGE       ASN_GAUGE
#define TIMETICKS   ASN_TIMETICKS
#define IPADDRESS   ASN_IPADDRESS
#define STRING      ASN_OCTET_STR

/* For return values e.g. SNMP_INTEGER macro */
SNMP_LOCAL_VARIABLES

static struct in_addr tmp;
#define INT32_INADDR(x) \
  (tmp.s_addr = (x), tmp)

/* OSPFv3-MIB instances. */
oid ospfv3_oid [] = { OSPFv3MIB };
oid ospf6d_oid [] = { OSPF6DOID };

/* empty ID 0.0.0.0 e.g. empty router-id */
static struct in_addr ospf6_empty_id = {0};

/* Hook functions. */
static u_char *ospfv3GeneralGroup ();
static u_char *ospfv3AreaEntry ();
static u_char *ospfv3AreaLsdbEntry ();

struct variable ospfv3_variables[] =
{
  /* OSPF general variables */
  {OSPFv3ROUTERID,              IPADDRESS, RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 1}},
  {OSPFv3ADMINSTAT,             INTEGER,   RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 2}},
  {OSPFv3VERSIONNUMBER,         INTEGER,   RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 3}},
  {OSPFv3AREABDRRTRSTATUS,      INTEGER,   RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 4}},
  {OSPFv3ASBDRRTRSTATUS,        INTEGER,   RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 5}},
  {OSPFv3ASSCOPELSACOUNT,       GAUGE,     RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 6}},
  {OSPFv3ASSCOPELSACHECKSUMSUM, INTEGER,   RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 7}},
  {OSPFv3ORIGINATENEWLSAS,      COUNTER,   RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 8}},
  {OSPFv3RXNEWLSAS,             COUNTER,   RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 9}},
  {OSPFv3EXTLSACOUNT,           GAUGE,     RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 10}},
  {OSPFv3EXTAREALSDBLIMIT,      INTEGER,   RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 11}},
  {OSPFv3MULTICASTEXTENSIONS,   INTEGER,   RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 12}},
  {OSPFv3EXITOVERFLOWINTERVAL,  INTEGER,   RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 13}},
  {OSPFv3DEMANDEXTENSIONS,      INTEGER,   RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 14}},
  {OSPFv3TRAFFICENGINEERINGSUPPORT, INTEGER, RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 15}},
  {OSPFv3REFERENCEBANDWIDTH,    INTEGER, RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 16}},
  {OSPFv3RESTARTSUPPORT,        INTEGER, RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 17}},
  {OSPFv3RESTARTINTERVAL,       INTEGER, RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 18}},
  {OSPFv3RESTARTSTATUS,         INTEGER, RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 19}},
  {OSPFv3RESTARTAGE,            INTEGER, RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 20}},
  {OSPFv3RESTARTEXITREASON,     INTEGER, RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 21}},

  /* OSPFv3 Area Data Structure */
  {OSPFv3AREAID,                IPADDRESS, RONLY,  ospfv3AreaEntry,
   4, {1, 2, 1, 1}},
  {OSPFv3IMPORTASEXTERN,        INTEGER,   RWRITE, ospfv3AreaEntry,
   4, {1, 2, 1, 2}},
  {OSPFv3AREASPFRUNS,           COUNTER,   RONLY,  ospfv3AreaEntry,
   4, {1, 2, 1, 3}},
  {OSPFv3AREABDRRTRCOUNT,       GAUGE,     RONLY,  ospfv3AreaEntry,
   4, {1, 2, 1, 4}},
  {OSPFv3AREAASBDRRTRCOUNT,     GAUGE,     RONLY,  ospfv3AreaEntry,
   4, {1, 2, 1, 5}},
  {OSPFv3AREASCOPELSACOUNT,     GAUGE,     RONLY,  ospfv3AreaEntry,
   4, {1, 2, 1, 6}},
  {OSPFv3AREASCOPELSACKSUMSUM,  INTEGER,   RONLY,  ospfv3AreaEntry,
   4, {1, 2, 1, 7}},
  {OSPFv3AREASUMMARY,           INTEGER,   RWRITE, ospfv3AreaEntry,
   4, {1, 2, 1, 8}},
  {OSPFv3AREASTATUS,            INTEGER,   RWRITE, ospfv3AreaEntry,
   4, {1, 2, 1, 9}},
  {OSPFv3STUBMETRIC,            INTEGER,   RWRITE, ospfv3AreaEntry,
   4, {1, 2, 1, 10}},
  {OSPFv3AREANSSATRANSLATORROLE, INTEGER,  RWRITE, ospfv3AreaEntry,
   4, {1, 2, 1, 11}},
  {OSPFv3AREANSSATRANSLATORSTATE, INTEGER, RONLY,  ospfv3AreaEntry,
   4, {1, 2, 1, 12}},
  {OSPFv3AREANSSATRANSLATORSTABILITYINTERVAL, INTEGER, RWRITE, ospfv3AreaEntry,
   4, {1, 2, 1, 13}},
  {OSPFv3AREANSSATRANSLATOREVENTS, COUNTER, RONLY, ospfv3AreaEntry,
   4, {1, 2, 1, 14}},
  {OSPFv3AREASTUBMETRICTYPE,    INTEGER, RWRITE, ospfv3AreaEntry,
   4, {1, 2, 1, 15}},

  {OSPFv3AREALSDBAREAID,        IPADDRESS, RONLY,  ospfv3AreaLsdbEntry,
   4, {1, 4, 1, 1}},
  {OSPFv3AREALSDBTYPE,          GAUGE,     RONLY,  ospfv3AreaLsdbEntry,
   4, {1, 4, 1, 2}},
  {OSPFv3AREALSDBROUTERID,      IPADDRESS, RONLY,  ospfv3AreaLsdbEntry,
   4, {1, 4, 1, 3}},
  {OSPFv3AREALSDBLSID,          IPADDRESS, RONLY,  ospfv3AreaLsdbEntry,
   4, {1, 4, 1, 4}},
  {OSPFv3AREALSDBSEQUENCE,      INTEGER,   RONLY,  ospfv3AreaLsdbEntry,
   4, {1, 4, 1, 5}},
  {OSPFv3AREALSDBAGE,           INTEGER,   RONLY,  ospfv3AreaLsdbEntry,
   4, {1, 4, 1, 6}},
  {OSPFv3AREALSDBCHECKSUM,      INTEGER,   RONLY,  ospfv3AreaLsdbEntry,
   4, {1, 4, 1, 7}},
  {OSPFv3AREALSDBADVERTISEMENT, STRING,    RONLY,  ospfv3AreaLsdbEntry,
   4, {1, 4, 1, 8}},
  {OSPFv3AREALSDBTYPEKNOWN,     INTEGER,   RONLY,  ospfv3AreaLsdbEntry,
   4, {1, 4, 1, 9}},

};

static u_char *
ospfv3GeneralGroup (struct variable *v, oid *name, size_t *length,
                    int exact, size_t *var_len, WriteMethod **write_method)
{
  /* Check whether the instance identifier is valid */
  if (smux_header_generic (v, name, length, exact, var_len, write_method)
      == MATCH_FAILED)
    return NULL;

  /* Return the current value of the variable */
  switch (v->magic)
    {
    case OSPFv3ROUTERID:                  /* 1*/
      /* Router-ID of this OSPF instance. */
      if (ospf6)
	return SNMP_IPADDRESS (INT32_INADDR (ospf6->router_id));
      else
	return SNMP_IPADDRESS (ospf6_empty_id);
      break;
    case OSPFv3ADMINSTAT:                 /* 2*/
      break;
    case OSPFv3VERSIONNUMBER:             /* 3*/
      break;
    case OSPFv3AREABDRRTRSTATUS:          /* 4*/
      break;
    case OSPFv3ASBDRRTRSTATUS:            /* 5*/
      break;
    case OSPFv3ASSCOPELSACOUNT:           /* 6*/
      break;
    case OSPFv3ASSCOPELSACHECKSUMSUM:     /* 7*/
      break;
    case OSPFv3ORIGINATENEWLSAS:          /* 8*/
      break;
    case OSPFv3RXNEWLSAS:                 /* 9*/
      break;
    case OSPFv3EXTLSACOUNT:               /*10*/
      break;
    case OSPFv3EXTAREALSDBLIMIT:          /*11*/
      break;
    case OSPFv3MULTICASTEXTENSIONS:       /*12*/
      break;
    case OSPFv3EXITOVERFLOWINTERVAL:      /*13*/
      break;
    case OSPFv3DEMANDEXTENSIONS:          /*14*/
      break;
    case OSPFv3TRAFFICENGINEERINGSUPPORT: /*15*/
      break;
    case OSPFv3REFERENCEBANDWIDTH:        /*16*/
      break;
    case OSPFv3RESTARTSUPPORT:            /*17*/
      break;
    case OSPFv3RESTARTINTERVAL:           /*18*/
      break;
    case OSPFv3RESTARTSTATUS:             /*19*/
      break;
    case OSPFv3RESTARTAGE:                /*20*/
      break;
    case OSPFv3RESTARTEXITREASON:         /*21*/
      break;
    default:
      return NULL;
    }
  return NULL;
}

static u_char *
ospfv3AreaEntry (struct variable *v, oid *name, size_t *length,
                 int exact, size_t *var_len, WriteMethod **write_method)
{
  struct ospf6_area *oa, *area = NULL;
  u_int32_t area_id = 0;
  listnode node;
  int len;

  if (ospf6 == NULL)
    return NULL;

  len = *length - v->namelen;
  len = (len >= sizeof (u_int32_t) ? sizeof (u_int32_t) : 0);
  if (exact && len != sizeof (u_int32_t))
    return NULL;
  if (len)
    oid2in_addr (name + v->namelen, len, (struct in_addr *) &area_id);

  zlog_info ("SNMP access by area: %s, exact=%d len=%d length=%d",
             inet_ntoa (* (struct in_addr *) &area_id),
             exact, len, *length);

  for (node = listhead (ospf6->area_list); node; nextnode (node))
    {
      oa = (struct ospf6_area *) getdata (node);
      if (area == NULL)
        {
          if (len == 0) /* return first area entry */
            area = oa;
          else if (exact && ntohl (oa->area_id) == ntohl (area_id))
            area = oa;
          else if (ntohl (oa->area_id) > ntohl (area_id))
            area = oa;
        }
    }

  if (area == NULL)
    return NULL;

  *length = v->namelen + sizeof (u_int32_t);
  oid_copy_addr (name + v->namelen, (struct in_addr *) &area->area_id,
                 sizeof (u_int32_t));

  zlog_info ("SNMP found area: %s, exact=%d len=%d length=%d",
             inet_ntoa (* (struct in_addr *) &area->area_id),
             exact, len, *length);

  switch (v->magic)
    {
    case OSPFv3AREAID:                   /* 1*/
      return SNMP_IPADDRESS (INT32_INADDR (area->area_id));
      break;
    case OSPFv3IMPORTASEXTERN:           /* 2*/
      return SNMP_INTEGER (ospf6->external_table->count);
      break;
    default:
      return NULL;
      break;
    }
  return NULL;
}

static u_char *
ospfv3AreaLsdbEntry (struct variable *v, oid *name, size_t *length,
                     int exact, size_t *var_len, WriteMethod **write_method)
{
  struct ospf6_lsa *lsa = NULL;
  struct in_addr area_id;
  u_int16_t type;
  struct in_addr id;
  struct in_addr adv_router;
  int len;
  oid *offset;
  int offsetlen;
  char a[16], b[16], c[16];
  struct ospf6_area *oa;
  listnode node;

  memset (&area_id, 0, sizeof (struct in_addr));
  type = 0;
  memset (&id, 0, sizeof (struct in_addr));
  memset (&adv_router, 0, sizeof (struct in_addr));

  /* Check OSPFv3 instance. */
  if (ospf6 == NULL)
    return NULL;

  /* Get variable length. */
  offset = name + v->namelen;
  offsetlen = *length - v->namelen;

#define OSPFV3_AREA_LSDB_ENTRY_EXACT_OFFSET \
  (IN_ADDR_SIZE + 1 + IN_ADDR_SIZE + IN_ADDR_SIZE)

  if (exact && offsetlen != OSPFV3_AREA_LSDB_ENTRY_EXACT_OFFSET)
    return NULL;

  /* Parse area-id */
  len = (offsetlen < IN_ADDR_SIZE ? offsetlen : IN_ADDR_SIZE);
  if (len)
    oid2in_addr (offset, len, &area_id);
  offset += len;
  offsetlen -= len;

  /* Parse type */
  len = (offsetlen < 1 ? offsetlen : 1);
  if (len)
    type = htons (*offset);
  offset += len;
  offsetlen -= len;

  /* Parse Router-ID */
  len = (offsetlen < IN_ADDR_SIZE ? offsetlen : IN_ADDR_SIZE);
  if (len)
    oid2in_addr (offset, len, &adv_router);
  offset += len;
  offsetlen -= len;

  /* Parse LS-ID */
  len = (offsetlen < IN_ADDR_SIZE ? offsetlen : IN_ADDR_SIZE);
  if (len)
    oid2in_addr (offset, len, &id);
  offset += len;
  offsetlen -= len;

  inet_ntop (AF_INET, &area_id, a, sizeof (a));
  inet_ntop (AF_INET, &adv_router, b, sizeof (b));
  inet_ntop (AF_INET, &id, c, sizeof (c));
  zlog_info ("SNMP access by lsdb: area=%s exact=%d length=%d magic=%d"
             " type=%#x adv_router=%s id=%s",
             a, exact, *length, v->magic, ntohs (type), b, c);

  if (exact)
    {
      oa = ospf6_area_lookup (area_id.s_addr, ospf6);
      lsa = ospf6_lsdb_lookup (type, id.s_addr, adv_router.s_addr, oa->lsdb);
    }
  else
    {
      for (node = listhead (ospf6->area_list); node; nextnode (node))
        {
          oa = (struct ospf6_area *) getdata (node);

          if (lsa)
            continue;
          if (ntohl (oa->area_id) < ntohl (area_id.s_addr))
            continue;

          lsa = ospf6_lsdb_lookup_next (type, id.s_addr, adv_router.s_addr,
                                        oa->lsdb);
          if (! lsa)
            {
              type = 0;
              memset (&id, 0, sizeof (struct in_addr));
              memset (&adv_router, 0, sizeof (struct in_addr));
            }
        }
    }

  if (! lsa)
    {
      zlog_info ("SNMP respond: No LSA to return");
      return NULL;
    }
  oa = OSPF6_AREA (lsa->lsdb->data);

  zlog_info ("SNMP respond: area: %s lsa: %s", oa->name, lsa->name);

  /* Add Index (AreaId, Type, RouterId, Lsid) */
  *length = v->namelen + OSPFV3_AREA_LSDB_ENTRY_EXACT_OFFSET;
  offset = name + v->namelen;
  oid_copy_addr (offset, (struct in_addr *) &oa->area_id, IN_ADDR_SIZE);
  offset += IN_ADDR_SIZE;
  *offset = ntohs (lsa->header->type);
  offset++;
  oid_copy_addr (offset, (struct in_addr *) &lsa->header->adv_router,
                 IN_ADDR_SIZE);
  offset += IN_ADDR_SIZE;
  oid_copy_addr (offset, (struct in_addr *) &lsa->header->id, IN_ADDR_SIZE);
  offset += IN_ADDR_SIZE;

  /* Return the current value of the variable */
  switch (v->magic)
    {
    case OSPFv3AREALSDBAREAID:        /* 1 */
      area_id.s_addr = OSPF6_AREA (lsa->lsdb->data)->area_id;
      return SNMP_IPADDRESS (area_id);
      break;
    case OSPFv3AREALSDBTYPE:          /* 2 */
      return SNMP_INTEGER (ntohs (lsa->header->type));
      break;
    case OSPFv3AREALSDBROUTERID:      /* 3 */
      adv_router.s_addr = lsa->header->adv_router;
      return SNMP_IPADDRESS (adv_router);
      break;
    case OSPFv3AREALSDBLSID:          /* 4 */
      id.s_addr = lsa->header->id;
      return SNMP_IPADDRESS (id);
      break;
    case OSPFv3AREALSDBSEQUENCE:      /* 5 */
      return SNMP_INTEGER (lsa->header->seqnum);
      break;
    case OSPFv3AREALSDBAGE:           /* 6 */
      ospf6_lsa_age_current (lsa);
      return SNMP_INTEGER (lsa->header->age);
      break;
    case OSPFv3AREALSDBCHECKSUM:      /* 7 */
      return SNMP_INTEGER (lsa->header->checksum);
      break;
    case OSPFv3AREALSDBADVERTISEMENT: /* 8 */
      *var_len = ntohs (lsa->header->length);
      return (u_char *) lsa->header;
      break;
    case OSPFv3AREALSDBTYPEKNOWN:     /* 9 */
      return SNMP_INTEGER (OSPF6_LSA_IS_KNOWN (lsa->header->type) ?
                           SNMP_TRUE : SNMP_FALSE);
      break;
    default:
      return NULL;
      break;
    }
  return NULL;
}


/* Register OSPFv3-MIB. */
void
ospf6_snmp_init ()
{
  smux_init (ospf6d_oid, sizeof (ospf6d_oid) / sizeof (oid));
  REGISTER_MIB ("OSPFv3MIB", ospfv3_variables, variable, ospfv3_oid);
  smux_start ();
}

#endif /* HAVE_SNMP */

