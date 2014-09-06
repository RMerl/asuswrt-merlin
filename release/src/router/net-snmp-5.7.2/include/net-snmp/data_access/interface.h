/*
 * interface data access header
 *
 * $Id$
 */
#ifndef NETSNMP_ACCESS_INTERFACE_H
#define NETSNMP_ACCESS_INTERFACE_H

#ifdef __cplusplus
extern          "C" {
#endif

/*
 * define flags to indicate the availability of certain data
 */
#define NETSNMP_INTERFACE_FLAGS_ACTIVE			0x00000001
#define NETSNMP_INTERFACE_FLAGS_HAS_BYTES		0x00000002
#define NETSNMP_INTERFACE_FLAGS_HAS_DROPS		0x00000004
#define NETSNMP_INTERFACE_FLAGS_HAS_MCAST_PKTS		0x00000008
#define NETSNMP_INTERFACE_FLAGS_HAS_HIGH_BYTES		0x00000010
#define NETSNMP_INTERFACE_FLAGS_HAS_HIGH_PACKETS	0x00000020
#define NETSNMP_INTERFACE_FLAGS_HAS_HIGH_SPEED		0x00000040
#define NETSNMP_INTERFACE_FLAGS_DYNAMIC_SPEED		0x00000080
#define NETSNMP_INTERFACE_FLAGS_HAS_LASTCHANGE		0x00000100
#define NETSNMP_INTERFACE_FLAGS_HAS_DISCONTINUITY	0x00000200
#define NETSNMP_INTERFACE_FLAGS_HAS_IF_FLAGS      	0x00000400
#define NETSNMP_INTERFACE_FLAGS_CAN_DOWN_PROTOCOL       0x00000800
#define NETSNMP_INTERFACE_FLAGS_HAS_IPV4                0x00001000
#define NETSNMP_INTERFACE_FLAGS_HAS_IPV6                0x00002000
#define NETSNMP_INTERFACE_FLAGS_HAS_V4_RETRANSMIT       0x00004000
#define NETSNMP_INTERFACE_FLAGS_HAS_V4_REASMMAX         0x00008000
#define NETSNMP_INTERFACE_FLAGS_HAS_V6_RETRANSMIT       0x00010000
#define NETSNMP_INTERFACE_FLAGS_HAS_V6_REASMMAX         0x00020000
#define NETSNMP_INTERFACE_FLAGS_HAS_V6_REACHABLE        0x00040000
#define NETSNMP_INTERFACE_FLAGS_HAS_V6_IFID             0x00080000
#define NETSNMP_INTERFACE_FLAGS_HAS_V6_FORWARDING       0x00100000
/* Some platforms, e.g.Linux, do not provide standalone counter
 * for incoming unicast packets - they provide counter of all packets
 * + separate counter for the multicast ones.
 * That means the counter of all packets must watched and checked
 * for overflows to reconstruct its 64-bit value (i.e. as usual
 * for counter of unicast packets), and after its expansion to 64-bits,
 * nr.of multicast packets must be substracted to get nr. of unicast
 * packets.
 * This flag marks stats of such platforms. Nr. of all incoming packets,
 * provided by the platform, must be stored in
 * netsnmp_interface_stats.iall and netsnmp_interface_stats.iucast will
 * be automatically calculated later.
 */
#define NETSNMP_INTERFACE_FLAGS_CALCULATE_UCAST         0x00200000

/*************************************************************
 * constants for enums for the MIB node
 * ifAdminStatus (INTEGER / ASN_INTEGER)
 *
 * since a Textual Convention may be referenced more than once in a
 * MIB, protect againt redifinitions of the enum values.
 */
#ifndef ifAdminStatus_ENUMS
#define ifAdminStatus_ENUMS

#define IFADMINSTATUS_UP  1
#define IFADMINSTATUS_DOWN  2
#define IFADMINSTATUS_TESTING  3

#endif                          /* ifAdminStatus_ENUMS */

/*************************************************************
 * constants for enums for the MIB node
 * ifOperStatus (INTEGER / ASN_INTEGER)
 *
 * since a Textual Convention may be referenced more than once in a
 * MIB, protect againt redifinitions of the enum values.
 */
#ifndef ifOperStatus_ENUMS
#define ifOperStatus_ENUMS

#define IFOPERSTATUS_UP  1
#define IFOPERSTATUS_DOWN  2
#define IFOPERSTATUS_TESTING  3
#define IFOPERSTATUS_UNKNOWN  4
#define IFOPERSTATUS_DORMANT  5
#define IFOPERSTATUS_NOTPRESENT 6
#define IFOPERSTATUS_LOWERLAYERDOWN 7

#endif                          /* ifOperStatus_ENUMS */

/* nominal speed of network interface - used when the real speed is unknown */
#define NOMINAL_LINK_SPEED 10000000

/**---------------------------------------------------------------------*/
/*
 * structure definitions
 *
 * NOTE: if you add fields, update code dealing with
 *       stats in interface_common.c, particularly
 *       netsnmp_access_interface_entry_update_stats()
 *
 */
typedef struct netsnmp_interface_stats_s {
    /*
     *  "Dynamic" fields
     *  Cached versions of byte/packet counters, etc
     *  (saved as a 'struct counter64' even if the
     *   high order half isn't actually used)
     *
     */
   /** input */
    struct counter64 ibytes;
    /*
     * nr. of all packets (to calculate iucast, when underlying platform does
     * not provide it)
     */
    struct counter64 iall;
    struct counter64 iucast;
    struct counter64 imcast;
    struct counter64 ibcast;
    unsigned int     ierrors;
    unsigned int     idiscards;
    unsigned int     iunknown_protos;
    unsigned int     inucast;
   /** output */
    struct counter64 obytes;
    struct counter64 oucast;
    struct counter64 omcast;
    struct counter64 obcast;
    unsigned int     oerrors;
    unsigned int     odiscards;
    unsigned int     oqlen;
    unsigned int     collisions;
    unsigned int     onucast;
} netsnmp_interface_stats;

/*
 *
 * NOTE: if you add fields, update code dealing with
 *       them in interface_common.c, particularly
 *       netsnmp_access_interface_entry_copy().
 */
typedef struct netsnmp_interface_entry_s {
    netsnmp_index oid_index;

    u_int   ns_flags; /* net-snmp flags */
    oid     index;

    /*
     *  "Static" information
     *  Typically constant for a given interface
     */
    char   *name;
    char   *descr;
    int     type;
    u_int   speed;
    u_int   speed_high;
    char   *paddr;
    u_int   paddr_len;
    u_int   mtu;

    u_int   retransmit_v4; /* milliseconds */
    u_int   retransmit_v6; /* milliseconds */

    u_int   reachable_time; /* ipv6 / milliseconds */

    u_long  lastchange;
    time_t  discontinuity;

    uint16_t     reasm_max_v4; /* 0..65535 */
    uint16_t     reasm_max_v6; /* 1500..65535 */
    char  admin_status;
    char  oper_status;

    /** booleans (not TruthValues!) */
    char  promiscuous;
    char  connector_present;
    char  forwarding_v6;

    char    v6_if_id_len;
    u_char  v6_if_id[8];

    /*-----------------------------------------------
     * platform/arch/access specific data
     */
    u_int os_flags; /* iff NETSNMP_INTERFACE_FLAGS_HAS_IF_FLAGS */

    /*
     * statistics
     */
    netsnmp_interface_stats stats;

    /** old_stats is used in netsnmp_access_interface_entry_update_stats */
    netsnmp_interface_stats *old_stats;

} netsnmp_interface_entry;

/*
 * conf file overrides
 */
typedef struct _conf_if_list {
    const char     *name;
    int             type;
    uint64_t speed;
    struct _conf_if_list *next;
} netsnmp_conf_if_list;

    typedef netsnmp_conf_if_list conf_if_list; /* backwards compat */

/**---------------------------------------------------------------------*/
/*
 * ACCESS function prototypes
 */
void init_interface(void);
void netsnmp_access_interface_init(void);

/*
 * ifcontainer init
 */
netsnmp_container * netsnmp_access_interface_container_init(u_int init_flags);
#define NETSNMP_ACCESS_INTERFACE_INIT_NOFLAGS               0x0000
#define NETSNMP_ACCESS_INTERFACE_INIT_ADDL_IDX_BY_NAME      0x0001

/*
 * ifcontainer load and free
 */
netsnmp_container*
netsnmp_access_interface_container_load(netsnmp_container* container,
                                        u_int load_flags);
#define NETSNMP_ACCESS_INTERFACE_LOAD_NOFLAGS               0x0000
#define NETSNMP_ACCESS_INTERFACE_LOAD_NO_STATS              0x0001
#define NETSNMP_ACCESS_INTERFACE_LOAD_IP4_ONLY              0x0002
#define NETSNMP_ACCESS_INTERFACE_LOAD_IP6_ONLY              0x0004

void netsnmp_access_interface_container_free(netsnmp_container *container,
                                             u_int free_flags);
#define NETSNMP_ACCESS_INTERFACE_FREE_NOFLAGS               0x0000
#define NETSNMP_ACCESS_INTERFACE_FREE_DONT_CLEAR            0x0001


/*
 * create/free an ifentry
 */
netsnmp_interface_entry *
netsnmp_access_interface_entry_create(const char *name, oid if_index);

void netsnmp_access_interface_entry_free(netsnmp_interface_entry * entry);

int
netsnmp_access_interface_entry_set_admin_status(netsnmp_interface_entry * entry,
	                                                int ifAdminStatus);

/*
 * find entry in container
 */
netsnmp_interface_entry *
netsnmp_access_interface_entry_get_by_name(netsnmp_container *container,
                                           const char *name);
netsnmp_interface_entry *
netsnmp_access_interface_entry_get_by_index(netsnmp_container *container,
                                            oid index);

/*
 * find ifIndex for given interface. 0 == not found.
 */
oid netsnmp_access_interface_index_find(const char *name);

/*
 * find name for given index
 */
const char *netsnmp_access_interface_name_find(oid index);

/*
 * copy interface entry data
 */
int netsnmp_access_interface_entry_copy(netsnmp_interface_entry * lhs,
                                        netsnmp_interface_entry * rhs);

/*
 * utility routines
 */
void netsnmp_access_interface_entry_guess_speed(netsnmp_interface_entry *);
void netsnmp_access_interface_entry_overrides(netsnmp_interface_entry *);


netsnmp_conf_if_list *
netsnmp_access_interface_entry_overrides_get(const char * name);

/**---------------------------------------------------------------------*/

#if defined( USING_IF_MIB_IFTABLE_IFTABLE_DATA_ACCESS_MODULE ) && \
    ! defined( NETSNMP_NO_BACKWARDS_COMPATABILITY )
void
Interface_Scan_Init(void);
int
Interface_Scan_Next(short *index, char *name, netsnmp_interface_entry **entry,
                    void *dc);
int
Interface_Scan_NextInt(int *index, char *name, netsnmp_interface_entry **entry,
                    void *dc);
#endif

/**---------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* NETSNMP_ACCESS_INTERFACE_H */
