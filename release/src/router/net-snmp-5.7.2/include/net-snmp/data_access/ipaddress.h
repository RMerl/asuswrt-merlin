/*
 * ipaddress data access header
 *
 * $Id$
 */
#ifndef NETSNMP_ACCESS_IPADDRESS_H
#define NETSNMP_ACCESS_IPADDRESS_H

# ifdef __cplusplus
extern          "C" {
#endif

/**---------------------------------------------------------------------*/
/*
 * structure definitions
 */
#if defined( NETSNMP_ENABLE_IPV6 )
#   define NETSNMP_ACCESS_IPADDRESS_BUF_SIZE 16   /* xxx-rks: 20, for ip6z? */
#else
#   define NETSNMP_ACCESS_IPADDRESS_BUF_SIZE 4
#endif


/*
 * netsnmp_ipaddress_entry
 *   - primary ipaddress structure for both ipv4 & ipv6
 */
typedef struct netsnmp_ipaddress_s {

   netsnmp_index oid_index;   /* MUST BE FIRST!! for container use */
   oid           ns_ia_index; /* arbitrary index */

   int       flags; /* for net-snmp use */

   /*
    * mib related data (considered for
    *  netsnmp_access_ipaddress_entry_update)
    */

   u_char    ia_address[NETSNMP_ACCESS_IPADDRESS_BUF_SIZE];

   oid       if_index;

   u_char    ia_address_len;/* address len, 4 | 16 */
   u_char    ia_prefix_len; /* 1-128 bits */
   u_char    ia_type;       /* 1-3 */
   u_char    ia_status;     /* IpAddressStatus (1-7) */
   u_char    ia_origin;     /* IpAddressOrigin (1-6) */
   u_char    ia_storagetype; /* IpAddressStorageType (1-5) */
   u_char    ia_onlink_flag; /* IpOnlinkFlag */
   u_char    ia_autonomous_flag; /*IpAutonomousFlag */
   u_long    ia_prefered_lifetime;/*IpPreferedLifeTime*/
   u_long    ia_valid_lifetime;/*IpValidLifeTime*/
   netsnmp_data_list *arch_data;      /* arch specific data */

} netsnmp_ipaddress_entry;


/**---------------------------------------------------------------------*/
/*
 * ACCESS function prototypes
 */
/*
 * ifcontainer init
 */
netsnmp_container * netsnmp_access_ipaddress_container_init(u_int init_flags);
#define NETSNMP_ACCESS_IPADDRESS_INIT_NOFLAGS               0x0000
#define NETSNMP_ACCESS_IPADDRESS_INIT_ADDL_IDX_BY_ADDR      0x0001

/*
 * ifcontainer load and free
 */
netsnmp_container*
netsnmp_access_ipaddress_container_load(netsnmp_container* container,
                                    u_int load_flags);
#define NETSNMP_ACCESS_IPADDRESS_LOAD_NOFLAGS               0x0000
#define NETSNMP_ACCESS_IPADDRESS_LOAD_IPV4_ONLY             0x0001
#define NETSNMP_ACCESS_IPADDRESS_LOAD_IPV6_ONLY             0x0002
#define NETSNMP_ACCESS_IPADDRESS_LOAD_ADDL_IDX_BY_ADDR      0x0004

void netsnmp_access_ipaddress_container_free(netsnmp_container *container,
                                         u_int free_flags);
#define NETSNMP_ACCESS_IPADDRESS_FREE_NOFLAGS               0x0000
#define NETSNMP_ACCESS_IPADDRESS_FREE_DONT_CLEAR            0x0001
#define NETSNMP_ACCESS_IPADDRESS_FREE_KEEP_CONTAINER        0x0002


/*
 * create/free a ipaddress+entry
 */
netsnmp_ipaddress_entry *
netsnmp_access_ipaddress_entry_create(void);

void netsnmp_access_ipaddress_entry_free(netsnmp_ipaddress_entry * entry);

/*
 * copy
 */
int
netsnmp_access_ipaddress_entry_copy(netsnmp_ipaddress_entry *old, 
                                    netsnmp_ipaddress_entry *new_val);

/*
 * update/compare
 */
int
netsnmp_access_ipaddress_entry_update(netsnmp_ipaddress_entry *old, 
                                      netsnmp_ipaddress_entry *new_val);

/*
 * find entry in container
 */
/** not yet */

/*
 * create/change/delete
 */
int
netsnmp_access_ipaddress_entry_set(netsnmp_ipaddress_entry * entry);


/*
 * ipaddress flags
 *   upper bits for internal use
 *   lower bits indicate changed fields. see FLAG_IPADDRESS* definitions in
 *         ipAddressTable_constants.h
 */
#define NETSNMP_ACCESS_IPADDRESS_CREATE     0x80000000
#define NETSNMP_ACCESS_IPADDRESS_DELETE     0x40000000
#define NETSNMP_ACCESS_IPADDRESS_CHANGE     0x20000000

#define NETSNMP_ACCESS_IPADDRESS_ISALIAS    0x10000000

/* 
 * mask for change flag bits
 */
#define NETSNMP_ACCESS_IPADDRESS_RESERVED_BITS 0x0000001f


/*
 * utility routines
 */
int netsnmp_ipaddress_prefix_copy(u_char *dst, u_char *src, 
                                  int addr_len, int pfx_len);

int netsnmp_ipaddress_ipv4_prefix_len(in_addr_t mask);

int netsnmp_ipaddress_flags_copy(u_long *ipAddressPrefixAdvPreferredLifetime,
                                 u_long *ipAddressPrefixAdvValidLifetime,
                                 u_long *ipAddressPrefixOnLinkFlag,
                                 u_long *ipAddressPrefixAutonomousFlag,
                                 u_long *ia_prefered_lifetime,
                                 u_long *ia_valid_lifetime,
                                 u_char *ia_onlink_flag,
                                 u_char *ia_autonomous_flag);

int netsnmp_ipaddress_prefix_origin_copy(u_long *ipAddressPrefixOrigin,
                                         u_char ia_origin,
                                         int flags,
                                         u_long ipAddressAddrType);

/**---------------------------------------------------------------------*/

# ifdef __cplusplus
}
#endif

#endif /* NETSNMP_ACCESS_IPADDRESS_H */
