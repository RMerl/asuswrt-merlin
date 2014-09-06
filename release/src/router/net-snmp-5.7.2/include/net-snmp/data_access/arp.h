/*
 * arp data access header
 *
 * $Id$
 */
#ifndef NETSNMP_ACCESS_ARP_H
#define NETSNMP_ACCESS_ARP_H

#ifdef __cplusplus
extern          "C" {
#endif

/**---------------------------------------------------------------------*/
#if defined( NETSNMP_ENABLE_IPV6 )
#   define NETSNMP_ACCESS_ARP_IPADDR_BUF_SIZE 16
#else
#   define NETSNMP_ACCESS_ARP_IPADDR_BUF_SIZE 4
#endif

/** MAC address is 6, InfiniBand uses more, 32 must be enough for near future.*/
#define NETSNMP_ACCESS_ARP_PHYSADDR_BUF_SIZE 32

/*************************************************************
 * constants for enums for the MIB node
 * inetNetToMediaType (INTEGER / ASN_INTEGER)
 *
 * since a Textual Convention may be referenced more than once in a
 * MIB, protect againt redefinitions of the enum values.
 */
#ifndef inetNetToMediaType_ENUMS
#define inetNetToMediaType_ENUMS

#define INETNETTOMEDIATYPE_OTHER  1
#define INETNETTOMEDIATYPE_INVALID  2
#define INETNETTOMEDIATYPE_DYNAMIC  3
#define INETNETTOMEDIATYPE_STATIC  4
#define INETNETTOMEDIATYPE_LOCAL  5

#endif                          /* inetNetToMediaType_ENUMS */

/*************************************************************
 * constants for enums for the MIB node
 * inetNetToMediaState (INTEGER / ASN_INTEGER)
 *
 * since a Textual Convention may be referenced more than once in a
 * MIB, protect againt redifinitions of the enum values.
 */
#ifndef inetNetToMediaState_ENUMS
#define inetNetToMediaState_ENUMS

#define INETNETTOMEDIASTATE_REACHABLE  1
#define INETNETTOMEDIASTATE_STALE  2
#define INETNETTOMEDIASTATE_DELAY  3
#define INETNETTOMEDIASTATE_PROBE  4
#define INETNETTOMEDIASTATE_INVALID  5
#define INETNETTOMEDIASTATE_UNKNOWN  6
#define INETNETTOMEDIASTATE_INCOMPLETE  7

#endif                          /* inetNetToMediaState_ENUMS */

/**---------------------------------------------------------------------*/
/*
 * structure definitions
 */
/*
 * netsnmp_arp_entry
 *   - primary arp structure for both ipv4 & ipv6
 */
typedef struct netsnmp_arp_s {

   netsnmp_index oid_index;      /* MUST BE FIRST!! for container use */
   oid           ns_arp_index;  /* arbitrary index */

   int       flags; /* for net-snmp use */

   unsigned  generation;
   oid       if_index;

   u_char    arp_physaddress[NETSNMP_ACCESS_ARP_PHYSADDR_BUF_SIZE];
   u_char    arp_ipaddress[NETSNMP_ACCESS_ARP_IPADDR_BUF_SIZE];

   u_char    arp_physaddress_len;/* phys address len, max 32 */
   u_char    arp_ipaddress_len;  /* ip address len, 4 | 16 */
   u_char    arp_type;           /* inetNetToMediaType 1-5 */
   u_char    arp_state;          /* inetNetToMediaState 1-7 */

   u_long    arp_last_updated;   /* timeticks of last update */
} netsnmp_arp_entry;

#define NETSNMP_ACCESS_ARP_ENTRY_FLAG_DELETE      0x00000001

/**---------------------------------------------------------------------*/
/*
 * ACCESS function prototypes
 */
struct netsnmp_arp_access_s;
typedef struct netsnmp_arp_access_s netsnmp_arp_access;

typedef void (NetsnmpAccessArpUpdate)(netsnmp_arp_access *, netsnmp_arp_entry*);
typedef void (NetsnmpAccessArpGC)    (netsnmp_arp_access *);

struct netsnmp_arp_access_s {
    void *magic;
    void *arch_magic;
    int synchronized;
    unsigned generation;
    NetsnmpAccessArpUpdate *update_hook;
    NetsnmpAccessArpGC *gc_hook;
    char *cache_expired;
};

netsnmp_arp_access *
netsnmp_access_arp_create(u_int init_flags,
                          NetsnmpAccessArpUpdate *update_hook,
                          NetsnmpAccessArpGC *gc_hook,
                          int *cache_timeout, int *cache_flags,
                          char *cache_expired);
#define NETSNMP_ACCESS_ARP_CREATE_NOFLAGS             0x0000

int netsnmp_access_arp_delete(netsnmp_arp_access *access);
int netsnmp_access_arp_load(netsnmp_arp_access *access);
int netsnmp_access_arp_unload(netsnmp_arp_access *access);

/*
 * create/free a arp+entry
 */
netsnmp_arp_entry *
netsnmp_access_arp_entry_create(void);

void netsnmp_access_arp_entry_free(netsnmp_arp_entry * entry);

void netsnmp_access_arp_entry_update(netsnmp_arp_entry *entry,
        netsnmp_arp_entry *new_data);

/*
 * find entry in container
 */
/** not yet */

/**---------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* NETSNMP_ACCESS_ARP_H */
