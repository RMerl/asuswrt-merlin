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

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

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
#include "ospf6_snmp.h"

/* OSPFv3-MIB */
#define OSPFv3MIB 1,3,6,1,2,1,191

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
#define OSPFv3EXITOVERFLOWINTERVAL      12
#define OSPFv3DEMANDEXTENSIONS          13
#define OSPFv3REFERENCEBANDWIDTH        14
#define OSPFv3RESTARTSUPPORT            15
#define OSPFv3RESTARTINTERVAL           16
#define OSPFv3RESTARTSTRICTLSACHECKING  17
#define OSPFv3RESTARTSTATUS             18
#define OSPFv3RESTARTAGE                19
#define OSPFv3RESTARTEXITREASON         20
#define OSPFv3NOTIFICATIONENABLE        21
#define OSPFv3STUBROUTERSUPPORT         22
#define OSPFv3STUBROUTERADVERTISEMENT   23
#define OSPFv3DISCONTINUITYTIME         24
#define OSPFv3RESTARTTIME               25

/* OSPFv3 MIB Area Table values: ospfv3AreaTable */
#define OSPFv3IMPORTASEXTERN             2
#define OSPFv3AREASPFRUNS                3
#define OSPFv3AREABDRRTRCOUNT            4
#define OSPFv3AREAASBDRRTRCOUNT          5
#define OSPFv3AREASCOPELSACOUNT          6
#define OSPFv3AREASCOPELSACKSUMSUM       7
#define OSPFv3AREASUMMARY                8
#define OSPFv3AREAROWSTATUS              9
#define OSPFv3AREASTUBMETRIC            10
#define OSPFv3AREANSSATRANSLATORROLE    11
#define OSPFv3AREANSSATRANSLATORSTATE   12
#define OSPFv3AREANSSATRANSLATORSTABINTERVAL    13
#define OSPFv3AREANSSATRANSLATOREVENTS  14
#define OSPFv3AREASTUBMETRICTYPE        15
#define OSPFv3AREATEENABLED             16

/* OSPFv3 MIB * Lsdb Table values: ospfv3*LsdbTable */
#define OSPFv3WWLSDBSEQUENCE             1
#define OSPFv3WWLSDBAGE                  2
#define OSPFv3WWLSDBCHECKSUM             3
#define OSPFv3WWLSDBADVERTISEMENT        4
#define OSPFv3WWLSDBTYPEKNOWN            5

/* Three first bits are to identify column */
#define OSPFv3WWCOLUMN 0x7
/* Then we use other bits to identify table */
#define OSPFv3WWASTABLE   (1 << 3)
#define OSPFv3WWAREATABLE (1 << 4)
#define OSPFv3WWLINKTABLE (1 << 5)
#define OSPFv3WWVIRTLINKTABLE (1 << 6)

/* OSPFv3 MIB Host Table values: ospfv3HostTable */
#define OSPFv3HOSTMETRIC                 3
#define OSPFv3HOSTROWSTATUS              4
#define OSPFv3HOSTAREAID                 5

/* OSPFv3 MIB Interface Table values: ospfv3IfTable */
#define OSPFv3IFAREAID                   3
#define OSPFv3IFTYPE                     4
#define OSPFv3IFADMINSTATUS              5
#define OSPFv3IFRTRPRIORITY              6
#define OSPFv3IFTRANSITDELAY             7
#define OSPFv3IFRETRANSINTERVAL          8
#define OSPFv3IFHELLOINTERVAL            9
#define OSPFv3IFRTRDEADINTERVAL         10
#define OSPFv3IFPOLLINTERVAL            11
#define OSPFv3IFSTATE                   12
#define OSPFv3IFDESIGNATEDROUTER        13
#define OSPFv3IFBACKUPDESIGNATEDROUTER  14
#define OSPFv3IFEVENTS                  15
#define OSPFv3IFROWSTATUS               16
#define OSPFv3IFDEMAND                  17
#define OSPFv3IFMETRICVALUE             18
#define OSPFv3IFLINKSCOPELSACOUNT       19
#define OSPFv3IFLINKLSACKSUMSUM         20
#define OSPFv3IFDEMANDNBRPROBE          21
#define OSPFv3IFDEMANDNBRPROBERETRANSLIMIT 22
#define OSPFv3IFDEMANDNBRPROBEINTERVAL  23
#define OSPFv3IFTEDISABLED              24
#define OSPFv3IFLINKLSASUPPRESSION      25

/* OSPFv3 MIB Virtual Interface Table values: ospfv3VirtIfTable */
#define OSPFv3VIRTIFINDEX           3
#define OSPFv3VIRTIFINSTID          4
#define OSPFv3VIRTIFTRANSITDELAY    5
#define OSPFv3VIRTIFRETRANSINTERVAL 6
#define OSPFv3VIRTIFHELLOINTERVAL   7
#define OSPFv3VIRTIFRTRDEADINTERVAL 8
#define OSPFv3VIRTIFSTATE           9
#define OSPFv3VIRTIFEVENTS         10
#define OSPFv3VIRTIFROWSTATUS      11
#define OSPFv3VIRTIFLINKSCOPELSACOUNT 12
#define OSPFv3VIRTIFLINKLSACKSUMSUM   13

/* OSPFv3 MIB Neighbors Table values: ospfv3NbrTable */
#define OSPFv3NBRADDRESSTYPE      4
#define OSPFv3NBRADDRESS          5
#define OSPFv3NBROPTIONS          6
#define OSPFv3NBRPRIORITY         7
#define OSPFv3NBRSTATE            8
#define OSPFv3NBREVENTS           9
#define OSPFv3NBRLSRETRANSQLEN   10
#define OSPFv3NBRHELLOSUPPRESSED 11
#define OSPFv3NBRIFID            12
#define OSPFv3NBRRESTARTHELPERSTATUS     13
#define OSPFv3NBRRESTARTHELPERAGE        14
#define OSPFv3NBRRESTARTHELPEREXITREASON 15

/* OSPFv3 MIB Configured Neighbors Table values: ospfv3CfgNbrTable */
#define OSPFv3CFGNBRPRIORITY  5
#define OSPFv3CFGNBRROWSTATUS 6

/* OSPFv3 MIB Virtual Neighbors Table values: ospfv3VirtNbrTable */
#define OSPFv3VIRTNBRIFINDEX          3
#define OSPFv3VIRTNBRIFINSTID         4
#define OSPFv3VIRTNBRADDRESSTYPE      5
#define OSPFv3VIRTNBRADDRESS          6
#define OSPFv3VIRTNBROPTIONS          7
#define OSPFv3VIRTNBRSTATE            8
#define OSPFv3VIRTNBREVENTS           9
#define OSPFv3VIRTNBRLSRETRANSQLEN   10
#define OSPFv3VIRTNBRHELLOSUPPRESSED 11
#define OSPFv3VIRTNBRIFID            12
#define OSPFv3VIRTNBRRESTARTHELPERSTATUS     13
#define OSPFv3VIRTNBRRESTARTHELPERAGE        14
#define OSPFv3VIRTNBRRESTARTHELPEREXITREASON 15

/* OSPFv3 MIB Area Aggregate Table values: ospfv3AreaAggregateTable */
#define OSPFv3AREAAGGREGATEROWSTATUS  6
#define OSPFv3AREAAGGREGATEEFFECT     7
#define OSPFv3AREAAGGREGATEROUTETAG   8

/* SYNTAX Status from OSPF-MIB. */
#define OSPF_STATUS_ENABLED  1
#define OSPF_STATUS_DISABLED 2

/* SNMP value hack. */
#define COUNTER     ASN_COUNTER
#define INTEGER     ASN_INTEGER
#define GAUGE       ASN_GAUGE
#define UNSIGNED    ASN_UNSIGNED
#define TIMETICKS   ASN_TIMETICKS
#define IPADDRESS   ASN_IPADDRESS
#define STRING      ASN_OCTET_STR

/* For return values e.g. SNMP_INTEGER macro */
SNMP_LOCAL_VARIABLES

/* OSPFv3-MIB instances. */
oid ospfv3_oid [] = { OSPFv3MIB };
oid ospfv3_trap_oid [] = { OSPFv3MIB, 0 };

/* Hook functions. */
static u_char *ospfv3GeneralGroup (struct variable *, oid *, size_t *,
				   int, size_t *, WriteMethod **);
static u_char *ospfv3AreaEntry (struct variable *, oid *, size_t *,
				int, size_t *, WriteMethod **);
static u_char *ospfv3WwLsdbEntry (struct variable *, oid *, size_t *,
				  int, size_t *, WriteMethod **);
static u_char *ospfv3NbrEntry (struct variable *, oid *, size_t *,
			       int, size_t *, WriteMethod **);
static u_char *ospfv3IfEntry (struct variable *, oid *, size_t *,
			      int, size_t *, WriteMethod **);

struct variable ospfv3_variables[] =
{
  /* OSPF general variables */
  {OSPFv3ROUTERID,             UNSIGNED,   RWRITE, ospfv3GeneralGroup,
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
  {OSPFv3ASSCOPELSACHECKSUMSUM,UNSIGNED,   RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 7}},
  {OSPFv3ORIGINATENEWLSAS,      COUNTER,   RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 8}},
  {OSPFv3RXNEWLSAS,             COUNTER,   RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 9}},
  {OSPFv3EXTLSACOUNT,           GAUGE,     RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 10}},
  {OSPFv3EXTAREALSDBLIMIT,      INTEGER,   RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 11}},
  {OSPFv3EXITOVERFLOWINTERVAL, UNSIGNED,   RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 12}},
  {OSPFv3DEMANDEXTENSIONS,      INTEGER,   RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 13}},
  {OSPFv3REFERENCEBANDWIDTH,   UNSIGNED, RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 14}},
  {OSPFv3RESTARTSUPPORT,        INTEGER, RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 15}},
  {OSPFv3RESTARTINTERVAL,      UNSIGNED, RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 16}},
  {OSPFv3RESTARTSTRICTLSACHECKING, INTEGER, RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 17}},
  {OSPFv3RESTARTSTATUS,         INTEGER, RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 18}},
  {OSPFv3RESTARTAGE,           UNSIGNED, RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 19}},
  {OSPFv3RESTARTEXITREASON,     INTEGER, RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 20}},
  {OSPFv3NOTIFICATIONENABLE,    INTEGER, RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 21}},
  {OSPFv3STUBROUTERSUPPORT,     INTEGER, RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 22}},
  {OSPFv3STUBROUTERADVERTISEMENT, INTEGER, RWRITE, ospfv3GeneralGroup,
   3, {1, 1, 23}},
  {OSPFv3DISCONTINUITYTIME,     TIMETICKS, RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 24}},
  {OSPFv3RESTARTTIME,           TIMETICKS, RONLY,  ospfv3GeneralGroup,
   3, {1, 1, 25}},

  /* OSPFv3 Area Data Structure */
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
  {OSPFv3AREASCOPELSACKSUMSUM, UNSIGNED,   RONLY,  ospfv3AreaEntry,
   4, {1, 2, 1, 7}},
  {OSPFv3AREASUMMARY,           INTEGER,   RWRITE, ospfv3AreaEntry,
   4, {1, 2, 1, 8}},
  {OSPFv3AREAROWSTATUS,         INTEGER,   RWRITE, ospfv3AreaEntry,
   4, {1, 2, 1, 9}},
  {OSPFv3AREASTUBMETRIC,        INTEGER,   RWRITE, ospfv3AreaEntry,
   4, {1, 2, 1, 10}},
  {OSPFv3AREANSSATRANSLATORROLE, INTEGER,  RWRITE, ospfv3AreaEntry,
   4, {1, 2, 1, 11}},
  {OSPFv3AREANSSATRANSLATORSTATE, INTEGER, RONLY,  ospfv3AreaEntry,
   4, {1, 2, 1, 12}},
  {OSPFv3AREANSSATRANSLATORSTABINTERVAL, UNSIGNED, RWRITE, ospfv3AreaEntry,
   4, {1, 2, 1, 13}},
  {OSPFv3AREANSSATRANSLATOREVENTS, COUNTER, RONLY, ospfv3AreaEntry,
   4, {1, 2, 1, 14}},
  {OSPFv3AREASTUBMETRICTYPE,    INTEGER, RWRITE, ospfv3AreaEntry,
   4, {1, 2, 1, 15}},
  {OSPFv3AREATEENABLED,         INTEGER, RWRITE, ospfv3AreaEntry,
   4, {1, 2, 1, 16}},

  /* OSPFv3 AS LSDB */
  {OSPFv3WWLSDBSEQUENCE | OSPFv3WWASTABLE,      INTEGER,   RONLY,  ospfv3WwLsdbEntry,
   4, {1, 3, 1, 4}},
  {OSPFv3WWLSDBAGE | OSPFv3WWASTABLE,           UNSIGNED,  RONLY,  ospfv3WwLsdbEntry,
   4, {1, 3, 1, 5}},
  {OSPFv3WWLSDBCHECKSUM | OSPFv3WWASTABLE,      INTEGER,   RONLY,  ospfv3WwLsdbEntry,
   4, {1, 3, 1, 6}},
  {OSPFv3WWLSDBADVERTISEMENT | OSPFv3WWASTABLE, STRING,    RONLY,  ospfv3WwLsdbEntry,
   4, {1, 3, 1, 7}},
  {OSPFv3WWLSDBTYPEKNOWN | OSPFv3WWASTABLE,     INTEGER,   RONLY,  ospfv3WwLsdbEntry,
   4, {1, 3, 1, 8}},

  /* OSPFv3 Area LSDB */
  {OSPFv3WWLSDBSEQUENCE | OSPFv3WWAREATABLE,      INTEGER,   RONLY,  ospfv3WwLsdbEntry,
   4, {1, 4, 1, 5}},
  {OSPFv3WWLSDBAGE | OSPFv3WWAREATABLE,           UNSIGNED,  RONLY,  ospfv3WwLsdbEntry,
   4, {1, 4, 1, 6}},
  {OSPFv3WWLSDBCHECKSUM | OSPFv3WWAREATABLE,      INTEGER,   RONLY,  ospfv3WwLsdbEntry,
   4, {1, 4, 1, 7}},
  {OSPFv3WWLSDBADVERTISEMENT | OSPFv3WWAREATABLE, STRING,    RONLY,  ospfv3WwLsdbEntry,
   4, {1, 4, 1, 8}},
  {OSPFv3WWLSDBTYPEKNOWN | OSPFv3WWAREATABLE,     INTEGER,   RONLY,  ospfv3WwLsdbEntry,
   4, {1, 4, 1, 9}},

  /* OSPFv3 Link LSDB */
  {OSPFv3WWLSDBSEQUENCE | OSPFv3WWLINKTABLE,      INTEGER,   RONLY,  ospfv3WwLsdbEntry,
   4, {1, 5, 1, 6}},
  {OSPFv3WWLSDBAGE | OSPFv3WWLINKTABLE,           UNSIGNED,  RONLY,  ospfv3WwLsdbEntry,
   4, {1, 5, 1, 7}},
  {OSPFv3WWLSDBCHECKSUM | OSPFv3WWLINKTABLE,      INTEGER,   RONLY,  ospfv3WwLsdbEntry,
   4, {1, 5, 1, 8}},
  {OSPFv3WWLSDBADVERTISEMENT | OSPFv3WWLINKTABLE, STRING,    RONLY,  ospfv3WwLsdbEntry,
   4, {1, 5, 1, 9}},
  {OSPFv3WWLSDBTYPEKNOWN | OSPFv3WWLINKTABLE,     INTEGER,   RONLY,  ospfv3WwLsdbEntry,
   4, {1, 5, 1, 10}},

  /* OSPFv3 interfaces */
  {OSPFv3IFAREAID,             UNSIGNED, RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 3}},
  {OSPFv3IFTYPE,               INTEGER,  RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 4}},
  {OSPFv3IFADMINSTATUS,        INTEGER,  RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 5}},
  {OSPFv3IFRTRPRIORITY,        INTEGER,  RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 6}},
  {OSPFv3IFTRANSITDELAY,       UNSIGNED, RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 7}},
  {OSPFv3IFRETRANSINTERVAL,    UNSIGNED, RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 8}},
  {OSPFv3IFHELLOINTERVAL,      INTEGER,  RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 9}},
  {OSPFv3IFRTRDEADINTERVAL,    UNSIGNED, RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 10}},
  {OSPFv3IFPOLLINTERVAL,       UNSIGNED, RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 11}},
  {OSPFv3IFSTATE,              INTEGER,  RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 12}},
  {OSPFv3IFDESIGNATEDROUTER,   UNSIGNED, RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 13}},
  {OSPFv3IFBACKUPDESIGNATEDROUTER, UNSIGNED, RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 14}},
  {OSPFv3IFEVENTS,             COUNTER,  RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 15}},
  {OSPFv3IFROWSTATUS,          INTEGER,  RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 16}},
  {OSPFv3IFDEMAND,             INTEGER,  RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 17}},
  {OSPFv3IFMETRICVALUE,        INTEGER,  RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 18}},
  {OSPFv3IFLINKSCOPELSACOUNT,  GAUGE,    RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 19}},
  {OSPFv3IFLINKLSACKSUMSUM,    UNSIGNED, RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 20}},
  {OSPFv3IFDEMANDNBRPROBE,     INTEGER,  RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 21}},
  {OSPFv3IFDEMANDNBRPROBERETRANSLIMIT, UNSIGNED, RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 22}},
  {OSPFv3IFDEMANDNBRPROBEINTERVAL, UNSIGNED, RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 23}},
  {OSPFv3IFTEDISABLED,         INTEGER,  RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 24}},
  {OSPFv3IFLINKLSASUPPRESSION, INTEGER,  RONLY, ospfv3IfEntry,
   4, {1, 7, 1, 25}},

  /* OSPFv3 neighbors */
  {OSPFv3NBRADDRESSTYPE,        INTEGER,   RONLY,  ospfv3NbrEntry,
   4, {1, 9, 1, 4}},
  {OSPFv3NBRADDRESS,            STRING,    RONLY,  ospfv3NbrEntry,
   4, {1, 9, 1, 5}},
  {OSPFv3NBROPTIONS,            INTEGER,   RONLY,  ospfv3NbrEntry,
   4, {1, 9, 1, 6}},
  {OSPFv3NBRPRIORITY,           INTEGER,   RONLY,  ospfv3NbrEntry,
   4, {1, 9, 1, 7}},
  {OSPFv3NBRSTATE,              INTEGER,   RONLY,  ospfv3NbrEntry,
   4, {1, 9, 1, 8}},
  {OSPFv3NBREVENTS,             COUNTER,   RONLY,  ospfv3NbrEntry,
   4, {1, 9, 1, 9}},
  {OSPFv3NBRLSRETRANSQLEN,        GAUGE,   RONLY,  ospfv3NbrEntry,
   4, {1, 9, 1, 10}},
  {OSPFv3NBRHELLOSUPPRESSED,    INTEGER,   RONLY,  ospfv3NbrEntry,
   4, {1, 9, 1, 11}},
  {OSPFv3NBRIFID,               INTEGER,   RONLY,  ospfv3NbrEntry,
   4, {1, 9, 1, 12}},
  {OSPFv3NBRRESTARTHELPERSTATUS, INTEGER,  RONLY,  ospfv3NbrEntry,
   4, {1, 9, 1, 13}},
  {OSPFv3NBRRESTARTHELPERAGE,  UNSIGNED,   RONLY,  ospfv3NbrEntry,
   4, {1, 9, 1, 14}},
  {OSPFv3NBRRESTARTHELPEREXITREASON, INTEGER, RONLY, ospfv3NbrEntry,
   4, {1, 9, 1, 15}},
};

static u_char *
ospfv3GeneralGroup (struct variable *v, oid *name, size_t *length,
                    int exact, size_t *var_len, WriteMethod **write_method)
{
  u_int16_t sum;
  u_int32_t count;
  struct ospf6_lsa *lsa = NULL;

  /* Check whether the instance identifier is valid */
  if (smux_header_generic (v, name, length, exact, var_len, write_method)
      == MATCH_FAILED)
    return NULL;

  /* Return the current value of the variable */
  switch (v->magic)
    {
    case OSPFv3ROUTERID:
      /* Router-ID of this OSPF instance. */
      if (ospf6)
	return SNMP_INTEGER (ntohl (ospf6->router_id));
      return SNMP_INTEGER (0);
    case OSPFv3ADMINSTAT:
      if (ospf6)
	return SNMP_INTEGER (CHECK_FLAG (ospf6->flag, OSPF6_DISABLED)?
			     OSPF_STATUS_DISABLED:OSPF_STATUS_ENABLED);
      return SNMP_INTEGER (OSPF_STATUS_DISABLED);
    case OSPFv3VERSIONNUMBER:
      return SNMP_INTEGER (3);
    case OSPFv3AREABDRRTRSTATUS:
      if (ospf6)
	return SNMP_INTEGER (ospf6_is_router_abr (ospf6)?SNMP_TRUE:SNMP_FALSE);
      return SNMP_INTEGER (SNMP_FALSE);
    case OSPFv3ASBDRRTRSTATUS:
      if (ospf6)
	return SNMP_INTEGER (ospf6_asbr_is_asbr (ospf6)?SNMP_TRUE:SNMP_FALSE);
      return SNMP_INTEGER (SNMP_FALSE);
    case OSPFv3ASSCOPELSACOUNT:
      if (ospf6)
	return SNMP_INTEGER (ospf6->lsdb->count);
      return SNMP_INTEGER (0);
    case OSPFv3ASSCOPELSACHECKSUMSUM:
      if (ospf6)
        {
          for (sum = 0, lsa = ospf6_lsdb_head (ospf6->lsdb);
               lsa;
               lsa = ospf6_lsdb_next (lsa))
            sum += ntohs (lsa->header->checksum);
          return SNMP_INTEGER (sum);
        }
      return SNMP_INTEGER (0);
    case OSPFv3ORIGINATENEWLSAS:
      return SNMP_INTEGER (0);	/* Don't know where to get this value... */
    case OSPFv3RXNEWLSAS:
      return SNMP_INTEGER (0);	/* Don't know where to get this value... */
    case OSPFv3EXTLSACOUNT:
      if (ospf6)
        {
          for (count = 0, lsa = ospf6_lsdb_type_head (htons (OSPF6_LSTYPE_AS_EXTERNAL),
                                                      ospf6->lsdb);
               lsa;
               lsa = ospf6_lsdb_type_next (htons (OSPF6_LSTYPE_AS_EXTERNAL),
                                           lsa))
            count += 1;
          return SNMP_INTEGER (count);
        }
      return SNMP_INTEGER (0);
    case OSPFv3EXTAREALSDBLIMIT:
      return SNMP_INTEGER (-1);
    case OSPFv3EXITOVERFLOWINTERVAL:
      return SNMP_INTEGER (0);	/* Not supported */
    case OSPFv3DEMANDEXTENSIONS:
      return SNMP_INTEGER (0);	/* Not supported */
    case OSPFv3REFERENCEBANDWIDTH:
      if (ospf6)
        return SNMP_INTEGER (ospf6->ref_bandwidth);
      /* Otherwise, like for "not implemented". */
    case OSPFv3RESTARTSUPPORT:
    case OSPFv3RESTARTINTERVAL:
    case OSPFv3RESTARTSTRICTLSACHECKING:
    case OSPFv3RESTARTSTATUS:
    case OSPFv3RESTARTAGE:
    case OSPFv3RESTARTEXITREASON:
    case OSPFv3NOTIFICATIONENABLE:
    case OSPFv3STUBROUTERSUPPORT:
    case OSPFv3STUBROUTERADVERTISEMENT:
    case OSPFv3DISCONTINUITYTIME:
    case OSPFv3RESTARTTIME:
      /* TODO: Not implemented */
      return NULL;
    }
  return NULL;
}

static u_char *
ospfv3AreaEntry (struct variable *v, oid *name, size_t *length,
                 int exact, size_t *var_len, WriteMethod **write_method)
{
  struct ospf6_area *oa, *area = NULL;
  struct ospf6_lsa *lsa = NULL;
  u_int32_t area_id = 0;
  u_int32_t count;
  u_int16_t sum;
  struct listnode *node;
  unsigned int len;
  char a[16];
  struct ospf6_route *ro;

  if (ospf6 == NULL)
    return NULL;

  if (smux_header_table(v, name, length, exact, var_len, write_method)
      == MATCH_FAILED)
    return NULL;

  len = *length - v->namelen;
  len = (len >= 1 ? 1 : 0);
  if (exact && len != 1)
    return NULL;
  if (len)
    area_id  = htonl (name[v->namelen]);

  inet_ntop (AF_INET, &area_id, a, sizeof (a));
  zlog_debug ("SNMP access by area: %s, exact=%d len=%d length=%lu",
	      a, exact, len, (u_long)*length);

  for (ALL_LIST_ELEMENTS_RO (ospf6->area_list, node, oa))
    {
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

  *length = v->namelen + 1;
  name[v->namelen] = ntohl (area->area_id);

  inet_ntop (AF_INET, &area->area_id, a, sizeof (a));
  zlog_debug ("SNMP found area: %s, exact=%d len=%d length=%lu",
	      a, exact, len, (u_long)*length);

  switch (v->magic)
    {
    case OSPFv3IMPORTASEXTERN:
      /* No NSSA support */
      return SNMP_INTEGER (IS_AREA_STUB(area)?2:1);
    case OSPFv3AREASPFRUNS:
      return SNMP_INTEGER (area->spf_calculation);
    case OSPFv3AREABDRRTRCOUNT:
    case OSPFv3AREAASBDRRTRCOUNT:
      count = 0;
      for (ro = ospf6_route_head (ospf6->brouter_table); ro;
	   ro = ospf6_route_next (ro))
        {
          if (ntohl (ro->path.area_id) != ntohl (area->area_id)) continue;
          if (v->magic == OSPFv3AREABDRRTRCOUNT &&
              CHECK_FLAG (ro->path.router_bits, OSPF6_ROUTER_BIT_B))
            count++;
          if (v->magic == OSPFv3AREAASBDRRTRCOUNT &&
              CHECK_FLAG (ro->path.router_bits, OSPF6_ROUTER_BIT_E))
            count++;
        }
      return SNMP_INTEGER (count);
    case OSPFv3AREASCOPELSACOUNT:
      return SNMP_INTEGER (area->lsdb->count);
    case OSPFv3AREASCOPELSACKSUMSUM:
      for (sum = 0, lsa = ospf6_lsdb_head (area->lsdb);
	   lsa;
	   lsa = ospf6_lsdb_next (lsa))
	sum += ntohs (lsa->header->checksum);
      return SNMP_INTEGER (sum);
    case OSPFv3AREASUMMARY:
      return SNMP_INTEGER (2); /* sendAreaSummary */
    case OSPFv3AREAROWSTATUS:
      return SNMP_INTEGER (1); /* Active */
    case OSPFv3AREASTUBMETRIC:
    case OSPFv3AREANSSATRANSLATORROLE:
    case OSPFv3AREANSSATRANSLATORSTATE:
    case OSPFv3AREANSSATRANSLATORSTABINTERVAL:
    case OSPFv3AREANSSATRANSLATOREVENTS:
    case OSPFv3AREASTUBMETRICTYPE:
    case OSPFv3AREATEENABLED:
      /* Not implemented. */
      return NULL;
    }
  return NULL;
}

static int
if_icmp_func (struct interface *ifp1, struct interface *ifp2)
{
  return (ifp1->ifindex - ifp2->ifindex);
}

static u_char *
ospfv3WwLsdbEntry (struct variable *v, oid *name, size_t *length,
                     int exact, size_t *var_len, WriteMethod **write_method)
{
  struct ospf6_lsa *lsa = NULL;
  u_int32_t ifindex, area_id, id, instid, adv_router;
  u_int16_t type;
  int len;
  oid *offset;
  int offsetlen;
  char a[16], b[16], c[16];
  struct ospf6_area *oa;
  struct listnode *node;
  struct interface *iif;
  struct ospf6_interface *oi = NULL;
  struct list *ifslist;

  if (smux_header_table(v, name, length, exact, var_len, write_method)
      == MATCH_FAILED)
    return NULL;

  instid = ifindex = area_id = type = id = adv_router = 0;

  /* Check OSPFv3 instance. */
  if (ospf6 == NULL)
    return NULL;

  /* Get variable length. */
  offset = name + v->namelen;
  offsetlen = *length - v->namelen;

  if (exact && (v->magic & OSPFv3WWASTABLE) && offsetlen != 3)
    return NULL;
  if (exact && (v->magic & OSPFv3WWAREATABLE) && offsetlen != 4)
    return NULL;
  if (exact && (v->magic & OSPFv3WWLINKTABLE) && offsetlen != 5)
    return NULL;

  if (v->magic & OSPFv3WWLINKTABLE)
    {
      /* Parse ifindex */
      len = (offsetlen < 1 ? 0 : 1);
      if (len)
        ifindex = *offset;
      offset += len;
      offsetlen -= len;

      /* Parse instance ID */
      len = (offsetlen < 1 ? 0 : 1);
      if (len)
        instid = *offset;
      offset += len;
      offsetlen -= len;
    }
  else if (v->magic & OSPFv3WWAREATABLE)
    {
      /* Parse area-id */
      len = (offsetlen < 1 ? 0 : 1);
      if (len)
        area_id = htonl (*offset);
      offset += len;
      offsetlen -= len;
    }

  /* Parse type */
  len = (offsetlen < 1 ? 0 : 1);
  if (len)
    type = htons (*offset);
  offset += len;
  offsetlen -= len;

  /* Parse Router-ID */
  len = (offsetlen < 1 ? 0 : 1);
  if (len)
    adv_router = htonl (*offset);
  offset += len;
  offsetlen -= len;

  /* Parse LS-ID */
  len = (offsetlen < 1 ? 0 : 1);
  if (len)
    id = htonl (*offset);
  offset += len;
  offsetlen -= len;

  if (exact)
    {
      if (v->magic & OSPFv3WWASTABLE)
        {
          lsa = ospf6_lsdb_lookup (type, id, adv_router, ospf6->lsdb);
        }
      else if (v->magic & OSPFv3WWAREATABLE)
        {
          oa = ospf6_area_lookup (area_id, ospf6);
          if (!oa) return NULL;
          lsa = ospf6_lsdb_lookup (type, id, adv_router, oa->lsdb);
        }
      else if (v->magic & OSPFv3WWLINKTABLE)
        {
          oi = ospf6_interface_lookup_by_ifindex (ifindex);
          if (!oi || oi->instance_id != instid) return NULL;
          lsa = ospf6_lsdb_lookup (type, id, adv_router, oi->lsdb);
        }
    }
  else
    {
      if (v->magic & OSPFv3WWASTABLE)
	{
	  if (ospf6->lsdb->count)
	    lsa = ospf6_lsdb_lookup_next (type, id, adv_router,
					  ospf6->lsdb);
	}
      else if (v->magic & OSPFv3WWAREATABLE)
	for (ALL_LIST_ELEMENTS_RO (ospf6->area_list, node, oa))
          {
            if (oa->area_id < area_id)
              continue;

            if (oa->lsdb->count)
              lsa = ospf6_lsdb_lookup_next (type, id, adv_router,
                                            oa->lsdb);
            if (lsa) break;
            type = 0;
            id = 0;
            adv_router = 0;
          }
      else if (v->magic & OSPFv3WWLINKTABLE)
        {
          /* We build a sorted list of interfaces */
          ifslist = list_new ();
          if (!ifslist) return NULL;
          ifslist->cmp = (int (*)(void *, void *))if_icmp_func;
          for (ALL_LIST_ELEMENTS_RO (iflist, node, iif))
            listnode_add_sort (ifslist, iif);
          
          for (ALL_LIST_ELEMENTS_RO (ifslist, node, iif))
            {
              if (!iif->ifindex) continue;
              oi = ospf6_interface_lookup_by_ifindex (iif->ifindex);
              if (!oi) continue;
              if (iif->ifindex < ifindex) continue;
              if (oi->instance_id < instid) continue;
              
              if (oi->lsdb->count)
                lsa = ospf6_lsdb_lookup_next (type, id, adv_router,
                                            oi->lsdb);
              if (lsa) break;
              type = 0;
              id = 0;
              adv_router = 0;
              oi = NULL;
            }

          list_delete_all_node (ifslist);
        }
    }

  if (! lsa)
      return NULL;

  /* Add indexes */
  if (v->magic & OSPFv3WWASTABLE)
    {
      *length = v->namelen + 3;
      offset = name + v->namelen;
    }
  else if (v->magic & OSPFv3WWAREATABLE)
    {
      *length = v->namelen + 4;
      offset = name + v->namelen;
      *offset = ntohl (oa->area_id);
      offset++;
    }
  else if (v->magic & OSPFv3WWLINKTABLE)
    {
      *length = v->namelen + 5;
      offset = name + v->namelen;
      *offset = oi->interface->ifindex;
      offset++;
      *offset = oi->instance_id;
      offset++;
    }
  *offset = ntohs (lsa->header->type);
  offset++;
  *offset = ntohl (lsa->header->adv_router);
  offset++;
  *offset = ntohl (lsa->header->id);
  offset++;

  /* Return the current value of the variable */
  switch (v->magic & OSPFv3WWCOLUMN)
    {
    case OSPFv3WWLSDBSEQUENCE:
      return SNMP_INTEGER (ntohl (lsa->header->seqnum));
      break;
    case OSPFv3WWLSDBAGE:
      ospf6_lsa_age_current (lsa);
      return SNMP_INTEGER (ntohs (lsa->header->age));
      break;
    case OSPFv3WWLSDBCHECKSUM:
      return SNMP_INTEGER (ntohs (lsa->header->checksum));
      break;
    case OSPFv3WWLSDBADVERTISEMENT:
      *var_len = ntohs (lsa->header->length);
      return (u_char *) lsa->header;
      break;
    case OSPFv3WWLSDBTYPEKNOWN:
      return SNMP_INTEGER (OSPF6_LSA_IS_KNOWN (lsa->header->type) ?
                           SNMP_TRUE : SNMP_FALSE);
      break;
    }
  return NULL;
}

static u_char *
ospfv3IfEntry (struct variable *v, oid *name, size_t *length,
		int exact, size_t *var_len, WriteMethod **write_method)
{
  unsigned int ifindex, instid;
  struct ospf6_interface *oi = NULL;
  struct ospf6_lsa *lsa = NULL;
  struct interface      *iif;
  struct listnode *i;
  struct list *ifslist;
  oid *offset;
  int offsetlen, len;
  u_int32_t sum;

  if (smux_header_table (v, name, length, exact, var_len, write_method)
      == MATCH_FAILED)
    return NULL;

  ifindex = instid = 0;

  /* Check OSPFv3 instance. */
  if (ospf6 == NULL)
    return NULL;

  /* Get variable length. */
  offset = name + v->namelen;
  offsetlen = *length - v->namelen;

  if (exact && offsetlen != 2)
    return NULL;

  /* Parse if index */
  len = (offsetlen < 1 ? 0 : 1);
  if (len)
    ifindex = *offset;
  offset += len;
  offsetlen -= len;

  /* Parse instance ID */
  len = (offsetlen < 1 ? 0 : 1);
  if (len)
    instid = *offset;
  offset += len;
  offsetlen -= len;

  if (exact)
    {
      oi = ospf6_interface_lookup_by_ifindex (ifindex);
      if (!oi || oi->instance_id != instid) return NULL;
    }
  else
    {
      /* We build a sorted list of interfaces */
      ifslist = list_new ();
      if (!ifslist) return NULL;
      ifslist->cmp = (int (*)(void *, void *))if_icmp_func;
      for (ALL_LIST_ELEMENTS_RO (iflist, i, iif))
	listnode_add_sort (ifslist, iif);

      for (ALL_LIST_ELEMENTS_RO (ifslist, i, iif))
        {
          if (!iif->ifindex) continue;
          oi = ospf6_interface_lookup_by_ifindex (iif->ifindex);
          if (!oi) continue;
          if (iif->ifindex > ifindex ||
              (iif->ifindex == ifindex &&
               (oi->instance_id > instid)))
            break;
          oi = NULL;
        }

      list_delete_all_node (ifslist);
    }

  if (!oi) return NULL;

  /* Add Index (IfIndex, IfInstId) */
  *length = v->namelen + 2;
  offset = name + v->namelen;
  *offset = oi->interface->ifindex;
  offset++;
  *offset = oi->instance_id;
  offset++;

  /* Return the current value of the variable */
  switch (v->magic)
    {
    case OSPFv3IFAREAID:
      if (oi->area)
	return SNMP_INTEGER (ntohl (oi->area->area_id));
      break;
    case OSPFv3IFTYPE:
      if (if_is_broadcast (oi->interface))
	return SNMP_INTEGER (1);
      else if (if_is_pointopoint (oi->interface))
	return SNMP_INTEGER (3);
      else break;		/* Unknown, don't put anything */
    case OSPFv3IFADMINSTATUS:
      if (oi->area)
	return SNMP_INTEGER (OSPF_STATUS_ENABLED);
      return SNMP_INTEGER (OSPF_STATUS_DISABLED);
    case OSPFv3IFRTRPRIORITY:
      return SNMP_INTEGER (oi->priority);
    case OSPFv3IFTRANSITDELAY:
      return SNMP_INTEGER (oi->transdelay);
    case OSPFv3IFRETRANSINTERVAL:
      return SNMP_INTEGER (oi->rxmt_interval);
    case OSPFv3IFHELLOINTERVAL:
      return SNMP_INTEGER (oi->hello_interval);
    case OSPFv3IFRTRDEADINTERVAL:
      return SNMP_INTEGER (oi->dead_interval);
    case OSPFv3IFPOLLINTERVAL:
      /* No support for NBMA */
      break;
    case OSPFv3IFSTATE:
      return SNMP_INTEGER (oi->state);
    case OSPFv3IFDESIGNATEDROUTER:
      return SNMP_INTEGER (ntohl (oi->drouter));
    case OSPFv3IFBACKUPDESIGNATEDROUTER:
      return SNMP_INTEGER (ntohl (oi->bdrouter));
    case OSPFv3IFEVENTS:
      return SNMP_INTEGER (oi->state_change);
    case OSPFv3IFROWSTATUS:
      return SNMP_INTEGER (1);
    case OSPFv3IFDEMAND:
      return SNMP_INTEGER (SNMP_FALSE);
    case OSPFv3IFMETRICVALUE:
      return SNMP_INTEGER (oi->cost);
    case OSPFv3IFLINKSCOPELSACOUNT:
      return SNMP_INTEGER (oi->lsdb->count);
    case OSPFv3IFLINKLSACKSUMSUM:
      for (sum = 0, lsa = ospf6_lsdb_head (oi->lsdb);
	   lsa;
	   lsa = ospf6_lsdb_next (lsa))
	sum += ntohs (lsa->header->checksum);
      return SNMP_INTEGER (sum);
    case OSPFv3IFDEMANDNBRPROBE:
    case OSPFv3IFDEMANDNBRPROBERETRANSLIMIT:
    case OSPFv3IFDEMANDNBRPROBEINTERVAL:
    case OSPFv3IFTEDISABLED:
    case OSPFv3IFLINKLSASUPPRESSION:
      /* Not implemented. Only works if all the last ones are not
	 implemented! */
      return NULL;
    }

  /* Try an internal getnext. Some columns are missing in this table. */
  if (!exact && (name[*length-1] < MAX_SUBID))
    return ospfv3IfEntry(v, name, length,
			 exact, var_len, write_method);
  return NULL;
}

static u_char *
ospfv3NbrEntry (struct variable *v, oid *name, size_t *length,
		int exact, size_t *var_len, WriteMethod **write_method)
{
  unsigned int ifindex, instid, rtrid;
  struct ospf6_interface *oi = NULL;
  struct ospf6_neighbor  *on = NULL;
  struct interface      *iif;
  struct listnode *i, *j;
  struct list *ifslist;
  oid *offset;
  int offsetlen, len;

  if (smux_header_table (v, name, length, exact, var_len, write_method)
      == MATCH_FAILED)
    return NULL;

  ifindex = instid = rtrid = 0;

  /* Check OSPFv3 instance. */
  if (ospf6 == NULL)
    return NULL;

  /* Get variable length. */
  offset = name + v->namelen;
  offsetlen = *length - v->namelen;

  if (exact && offsetlen != 3)
    return NULL;

  /* Parse if index */
  len = (offsetlen < 1 ? 0 : 1);
  if (len)
    ifindex = *offset;
  offset += len;
  offsetlen -= len;

  /* Parse instance ID */
  len = (offsetlen < 1 ? 0 : 1);
  if (len)
    instid = *offset;
  offset += len;
  offsetlen -= len;

  /* Parse router ID */
  len = (offsetlen < 1 ? 0 : 1);
  if (len)
    rtrid = htonl (*offset);
  offset += len;
  offsetlen -= len;

  if (exact)
    {
      oi = ospf6_interface_lookup_by_ifindex (ifindex);
      if (!oi || oi->instance_id != instid) return NULL;
      on = ospf6_neighbor_lookup (rtrid, oi);
    }
  else
    {
      /* We build a sorted list of interfaces */
      ifslist = list_new ();
      if (!ifslist) return NULL;
      ifslist->cmp = (int (*)(void *, void *))if_icmp_func;
      for (ALL_LIST_ELEMENTS_RO (iflist, i, iif))
	listnode_add_sort (ifslist, iif);

      for (ALL_LIST_ELEMENTS_RO (ifslist, i, iif))
        {
          if (!iif->ifindex) continue;
          oi = ospf6_interface_lookup_by_ifindex (iif->ifindex);
          if (!oi) continue;
          for (ALL_LIST_ELEMENTS_RO (oi->neighbor_list, j, on)) {
            if (iif->ifindex > ifindex ||
                (iif->ifindex == ifindex &&
                 (oi->instance_id > instid ||
                  (oi->instance_id == instid &&
                   ntohl (on->router_id) > ntohl (rtrid)))))
              break;
          }
          if (on) break;
          oi = NULL;
          on = NULL;
        }

      list_delete_all_node (ifslist);
    }

  if (!oi || !on) return NULL;

  /* Add Index (IfIndex, IfInstId, RtrId) */
  *length = v->namelen + 3;
  offset = name + v->namelen;
  *offset = oi->interface->ifindex;
  offset++;
  *offset = oi->instance_id;
  offset++;
  *offset = ntohl (on->router_id);
  offset++;

  /* Return the current value of the variable */
  switch (v->magic)
    {
    case OSPFv3NBRADDRESSTYPE:
      return SNMP_INTEGER (2);	/* IPv6 only */
    case OSPFv3NBRADDRESS:
      *var_len = sizeof (struct in6_addr);
      return (u_char *) &on->linklocal_addr;
    case OSPFv3NBROPTIONS:
      return SNMP_INTEGER (on->options[2]);
    case OSPFv3NBRPRIORITY:
      return SNMP_INTEGER (on->priority);
    case OSPFv3NBRSTATE:
      return SNMP_INTEGER (on->state);
    case OSPFv3NBREVENTS:
      return SNMP_INTEGER (on->state_change);
    case OSPFv3NBRLSRETRANSQLEN:
      return SNMP_INTEGER (on->retrans_list->count);
    case OSPFv3NBRHELLOSUPPRESSED:
      return SNMP_INTEGER (SNMP_FALSE);
    case OSPFv3NBRIFID:
      return SNMP_INTEGER (on->ifindex);
    case OSPFv3NBRRESTARTHELPERSTATUS:
    case OSPFv3NBRRESTARTHELPERAGE:
    case OSPFv3NBRRESTARTHELPEREXITREASON:
      /* Not implemented. Only works if all the last ones are not
	 implemented! */
      return NULL;
    }

  return NULL;
}

/* OSPF Traps. */
#define NBRSTATECHANGE      2
#define IFSTATECHANGE      10

static struct trap_object ospf6NbrTrapList[] =
{
  {-3, {1, 1, OSPFv3ROUTERID}},
  {4, {1, 9, 1, OSPFv3NBRADDRESSTYPE}},
  {4, {1, 9, 1, OSPFv3NBRADDRESS}},
  {4, {1, 9, 1, OSPFv3NBRSTATE}}
};

static struct trap_object ospf6IfTrapList[] =
{
  {-3, {1, 1, OSPFv3ROUTERID}},
  {4, {1, 7, 1, OSPFv3IFSTATE}},
  {4, {1, 7, 1, OSPFv3IFADMINSTATUS}},
  {4, {1, 7, 1, OSPFv3IFAREAID}}
};

void
ospf6TrapNbrStateChange (struct ospf6_neighbor *on)
{
  oid index[3];

  index[0] = on->ospf6_if->interface->ifindex;
  index[1] = on->ospf6_if->instance_id;
  index[2] = ntohl (on->router_id);

  smux_trap (ospfv3_variables, sizeof ospfv3_variables / sizeof (struct variable),
	     ospfv3_trap_oid, sizeof ospfv3_trap_oid / sizeof (oid),
	     ospfv3_oid, sizeof ospfv3_oid / sizeof (oid),
             index,  3,
             ospf6NbrTrapList, 
             sizeof ospf6NbrTrapList / sizeof (struct trap_object),
             NBRSTATECHANGE);
}

void
ospf6TrapIfStateChange (struct ospf6_interface *oi)
{
  oid index[2];

  index[0] = oi->interface->ifindex;
  index[1] = oi->instance_id;

  smux_trap (ospfv3_variables, sizeof ospfv3_variables / sizeof (struct variable),
	     ospfv3_trap_oid, sizeof ospfv3_trap_oid / sizeof (oid),
	     ospfv3_oid, sizeof ospfv3_oid / sizeof (oid),
             index,  2,
             ospf6IfTrapList, 
             sizeof ospf6IfTrapList / sizeof (struct trap_object),
             IFSTATECHANGE);
}

/* Register OSPFv3-MIB. */
void
ospf6_snmp_init (struct thread_master *master)
{
  smux_init (master);
  REGISTER_MIB ("OSPFv3MIB", ospfv3_variables, variable, ospfv3_oid);
}

#endif /* HAVE_SNMP */

